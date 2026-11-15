#include "gaussian/gaussian_adapter.h"
#include "laplacian/laplacian_adapter.h"
#include "sobel/sobel.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <omp.h>

namespace fs = std::filesystem;
namespace {
constexpr std::array sizes{512, 1024, 2048};
constexpr std::array thread_counts{4, 8, 12, 16, 20};
constexpr int repeats = 10;
using Samples = std::array<double, repeats>;
using Clock = std::chrono::steady_clock;

template<class Filter>
Samples measure(Filter&& filter) {
    filter();
    Samples times;
    for (double& milliseconds : times) {
        const auto start = Clock::now();
        filter();
        milliseconds = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
    return times;
}

std::vector<float> load(const fs::path& file, int size) {
    std::vector<float> pixels(static_cast<std::size_t>(size) * size);
    const auto bytes = pixels.size() * sizeof(float);
    std::ifstream input(file, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() != static_cast<std::streamoff>(bytes)) {
        throw std::runtime_error("Missing input or invalid byte count: " + file.string());
    }
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(pixels.data()), bytes)) {
        throw std::runtime_error("Cannot read " + file.string());
    }
    for (float value : pixels) {
        if (!std::isfinite(value) || value < 0.0f || value > 255.0f) {
            throw std::runtime_error("Expected finite grayscale pixels: " + file.string());
        }
    }
    return pixels;
}

template<class Pixel>
void save(const fs::path& file, const std::vector<Pixel>& pixels) {
    std::ofstream output(file, std::ios::binary);
    if (!output.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(Pixel))) {
        throw std::runtime_error("Cannot write " + file.string());
    }
}

void write_times(std::ostream& output, const Samples& times) {
    for (double time : times) {
        output << ',' << time;
    }
    output << '\n';
}

template<class Pixel, class Sequential, class Parallel>
bool benchmark_image(std::ostream& output, const fs::path& out_dir, std::string_view algorithm,
                     const std::string& image, int size, int image_index,
                     Sequential sequential, Parallel parallel) {
    const auto count = static_cast<std::size_t>(size) * size;
    std::vector<Pixel> reference(count), actual(count);
    const Samples baseline = measure([&] { sequential(reference.data()); });
    const bool example = size == sizes.front() && image_index == 1;
    if (example) {
        save(out_dir / (std::string(algorithm) + "_seq.bin"), reference);
    }

    bool identical = true;
    for (std::size_t offset = 0; offset < thread_counts.size(); ++offset) {
        const int threads = thread_counts[(image_index - 1 + offset) % thread_counts.size()];
        int team_size = threads;
        const Samples parallel_times = measure([&] { team_size = parallel(actual.data(), threads); });
        if (team_size != threads) {
            throw std::runtime_error("OpenMP created an unexpected team size");
        }
        double max_error = 0.0, squared_error = 0.0;
        for (std::size_t i = 0; i < count; ++i) {
            const double error = std::abs(static_cast<double>(reference[i]) - actual[i]);
            if (!std::isfinite(error)) {
                throw std::runtime_error("Non-finite filter output");
            }
            max_error = std::max(max_error, error);
            squared_error += error * error;
        }
        identical = identical && max_error == 0.0;
        output << algorithm << ',' << size << ',' << image << ',' << team_size << ','
               << max_error << ',' << std::sqrt(squared_error / count);
        for (double time : baseline) {
            output << ',' << time;
        }
        write_times(output, parallel_times);
        if (example && threads == thread_counts.front()) {
            save(out_dir / (std::string(algorithm) + "_omp4.bin"), actual);
        }
    }
    return identical;
}

void check_teams() {
    omp_set_dynamic(0);
    for (int threads : thread_counts) {
        int actual = 0;
        #pragma omp parallel num_threads(threads)
        {
            #pragma omp single
            actual = omp_get_num_threads();
        }
        if (actual != threads) {
            throw std::runtime_error("OpenMP runtime could not start " + std::to_string(threads) + " threads");
        }
    }
}
}  // namespace

int main(int argc, char** argv) try {
    static_assert(sizeof(float) == 4, "The dataset uses float32");
    if (argc != 3) {
        std::cerr << "Usage: image_benchmark PROCESSED_DIR OUTPUT_DIR\n";
        return 2;
    }
    const unsigned int endian_probe = 1;
    if (*reinterpret_cast<const unsigned char*>(&endian_probe) != 1) {
        throw std::runtime_error("This benchmark requires little-endian float32");
    }
    const fs::path dataset = argv[1], out_dir = argv[2];
    const fs::path samples_dir = out_dir / "samples";
    fs::create_directories(samples_dir);
    std::ofstream output(out_dir / "timings.csv");
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << std::setprecision(12) << "algorithm,size,image,threads,max_abs_error,rmse";
    for (const char* version : {"seq", "omp"}) {
        for (int run = 1; run <= repeats; ++run) {
            output << ',' << version << '_' << run << "_ms";
        }
    }
    output << '\n';
    check_teams();
    std::cout << "OpenMP logical CPUs: " << omp_get_num_procs() << std::endl;

    bool all_identical = true;
    for (std::string_view filter : {"Sobel", "Gaussian", "Laplacian"}) {
        for (int size : sizes) {
            for (int index = 1; index <= 20; ++index) {
                const std::string image = (index < 10 ? "image_0" : "image_") + std::to_string(index);
                auto pixels = load(dataset / std::to_string(size) / (image + ".bin"), size);
                bool identical;
                if (filter == "Sobel") {
                    identical = benchmark_image<float>(output, samples_dir, filter, image, size, index,
                        [&](float* dst) { sobel_seq(pixels.data(), dst, size, size); },
                        [&](float* dst, int threads) {
                            sobel_omp(pixels.data(), dst, size, size, threads);
                            return threads;
                        });
                } else if (filter == "Gaussian") {
                    std::unique_ptr<GaussianState, decltype(&gaussian_destroy)> state(
                        gaussian_create(pixels.data(), size), gaussian_destroy);
                    identical = benchmark_image<uint8_t>(output, samples_dir, filter, image, size, index,
                        [&](uint8_t* dst) { gaussian_seq(state.get(), dst); },
                        [&](uint8_t* dst, int threads) { return gaussian_omp(state.get(), dst, threads); });
                } else {
                    identical = benchmark_image<float>(output, samples_dir, filter, image, size, index,
                        [&](float* dst) { laplacian_seq(pixels.data(), dst, size); },
                        [&](float* dst, int threads) {
                            laplacian_omp(pixels.data(), dst, size, threads);
                            return threads;
                        });
                }
                all_identical = all_identical && identical;
            }
            output.flush();
            std::cout << filter << ' ' << size << "x" << size << ": 20/20" << std::endl;
        }
    }
    return all_identical ? 0 : 1;
} catch (const std::exception& error) {
    std::cerr << "Benchmark error: " << error.what() << '\n';
    return 2;
}
