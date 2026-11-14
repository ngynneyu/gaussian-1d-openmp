#include "filter_backend.hpp"
#include "image_io.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using gaussian::Pixels;

namespace {

constexpr std::array<int, 3> supported_sizes{512, 1024, 2048};
constexpr auto team_sizes = gaussian::openmp_thread_counts;
constexpr int repetitions = 10;
using Samples = std::array<double, repetitions>;

std::vector<fs::path> find_images(const fs::path& data, const fs::path& only_image,
                                 const std::vector<int>& sizes) {
    std::vector<fs::path> names;
    for (int size : sizes) {
        const auto folder = data / std::to_string(size);
        if (!only_image.empty()) {
            if (!fs::is_regular_file(folder / only_image)) {
                throw std::runtime_error("Missing image: " + (folder / only_image).string());
            }
            continue;
        }
        std::vector<fs::path> current_names;
        for (const auto& entry : fs::directory_iterator(folder)) {
            if (entry.is_regular_file() && entry.path().extension() == ".bin") {
                current_names.push_back(entry.path().filename());
            }
        }
        std::sort(current_names.begin(), current_names.end());
        if (current_names.size() != 20) {
            throw std::runtime_error("Expected 20 .bin files in " + folder.string());
        }
        if (!names.empty() && names != current_names) {
            throw std::runtime_error("Image names must match across sizes: " + folder.string());
        }
        names = std::move(current_names);
    }
    if (!only_image.empty()) {
        names.push_back(only_image);
    }
    return names;
}

template <typename Filter>
Samples measure(int threads, Filter filter) {
    const int warmup_threads = filter();
    if (warmup_threads != threads) {
        throw std::runtime_error("OpenMP supplied " + std::to_string(warmup_threads) +
                                 " threads during warm-up, requested " + std::to_string(threads));
    }
    Samples samples{};
    for (double& elapsed : samples) {
        const auto start = std::chrono::steady_clock::now();
        const int actual = filter();
        const auto finish = std::chrono::steady_clock::now();
        if (actual != threads) {
            throw std::runtime_error("OpenMP supplied " + std::to_string(actual) +
                                     " threads during a timed run, requested " + std::to_string(threads));
        }
        elapsed = std::chrono::duration<double, std::milli>(finish - start).count();
    }
    return samples;
}

double median(Samples samples) {
    std::sort(samples.begin(), samples.end());
    return (samples[repetitions / 2 - 1] + samples[repetitions / 2]) / 2.0;
}

struct Errors {
    int max_absolute = 0;
    double rmse = 0.0;
    std::size_t different = 0;
    std::size_t first_difference = 0;
};

Errors compare(const Pixels& sequential, const Pixels& parallel) {
    Errors errors;
    std::uint64_t squared_sum = 0;
    for (std::size_t i = 0; i < sequential.size(); ++i) {
        const int difference = std::abs(int(sequential[i]) - int(parallel[i]));
        if (difference != 0) {
            if (errors.different == 0) {
                errors.first_difference = i;
            }
            ++errors.different;
        }
        errors.max_absolute = std::max(errors.max_absolute, difference);
        squared_sum += difference * difference;
    }
    errors.rmse = std::sqrt(static_cast<double>(squared_sum) / sequential.size());
    return errors;
}

std::string csv_cell(const std::string& value) {
    std::string result = "\"";
    for (char ch : value) {
        if (ch == '"') {
            result += '"';
        }
        result += ch;
    }
    return result + '"';
}

std::ofstream open_log(const fs::path& path) {
    std::ofstream stream(path);
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream << std::setprecision(std::numeric_limits<double>::max_digits10);
    return stream;
}

void write_metadata(const fs::path& path, const fs::path& data, int kernel_size, double sigma,
                    const fs::path& only_image, const std::vector<int>& sizes) {
    auto stream = open_log(path);
    stream << "data=" << fs::absolute(data).lexically_normal().string()
           << "\nimage=" << (only_image.empty() ? "all" : only_image.string()) << "\nsizes=";
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        if (i != 0) {
            stream << ',';
        }
        stream << sizes[i];
    }
    stream << "\nthreads=" << gaussian::openmp_threads_csv()
           << "\nlogical_cpus_available=" << gaussian::openmp_available_processors()
           << "\nkernel_size=" << kernel_size
           << "\nsigma=" << sigma << "\nborder=replicate\nalgorithm=" << gaussian::Filter::algorithm
           << "\nwarmup=1\nrepetitions=" << repetitions << "\nclock=steady_clock"
           << "\nmeasurement_order=cyclic_by_image"
           << "\nopenmp_macro=" << gaussian::openmp_version() << "\nopenmp_dynamic=disabled";
    #ifdef __VERSION__
    stream << "\ncompiler=" << __VERSION__;
    #endif
    for (const auto* key : {"OMP_THREAD_LIMIT", "OMP_PROC_BIND", "OMP_PLACES",
                            "OMP_WAIT_POLICY", "GOMP_CPU_AFFINITY"}) {
        const char* value = std::getenv(key);
        stream << '\n' << key << '=' << (value ? value : "(unset)");
    }
    stream << '\n';
    stream.close();
}

void log_times(std::ostream& raw, const std::string& config, const Samples& times) {
    for (int run = 0; run < repetitions; ++run) {
        raw << config << ',' << run + 1 << ',' << times[run] << '\n';
    }
}

bool benchmark(const fs::path& data, const fs::path& output, int kernel_size, double sigma,
               const fs::path& only_image, const std::vector<int>& sizes, bool save_images) {
    gaussian::Filter filter(kernel_size, sigma);
    const auto names = find_images(data, only_image, sizes);
    if (!fs::create_directory(output)) {
        throw std::runtime_error("Choose a new output directory: " + output.string());
    }
    if (save_images) {
        fs::create_directory(output / "samples");
    }
    write_metadata(output / "run_config.txt", data, kernel_size, sigma, only_image, sizes);
    auto raw = open_log(output / "timings_raw.csv");
    auto summary = open_log(output / "metrics.csv");
    auto mismatches = open_log(output / "mismatches.csv");
    constexpr auto config_columns = "image,width,height,kernel_size,sigma,implementation,threads";
    raw << config_columns << ",run,elapsed_ms\n";
    summary << config_columns << ",median_ms,speedup,efficiency_pct,max_abs_error,rmse\n";
    mismatches << config_columns << ",different_pixels,first_x,first_y,seq_value,omp_value\n";

    bool all_equal = true;
    std::size_t first_configuration = 0;
    constexpr auto configuration_count = team_sizes.size() + 1;
    for (int size : sizes) {
        const auto images = output / "images" / std::to_string(size);
        if (save_images) {
            fs::create_directories(images / "seq");
            for (int threads : team_sizes) {
                fs::create_directory(images / ("omp" + std::to_string(threads)));
            }
        }

        for (const auto& name : names) {
            const auto input = gaussian::read_image(data / std::to_string(size) / name, size, size);
            filter.prepare(input);
            auto output_name = name;
            output_name.replace_extension(".pgm");
            Pixels sequential(input.pixels.size());
            std::array<Pixels, team_sizes.size()> parallel_outputs;
            for (auto& pixels : parallel_outputs) {
                pixels.resize(input.pixels.size());
            }
            Samples seq_times{};
            std::array<Samples, team_sizes.size()> parallel_times{};
            // Rotate between images. Keep a team's warm-up and timed calls together.
            for (std::size_t offset = 0; offset < configuration_count; ++offset) {
                const auto configuration = (first_configuration + offset) % configuration_count;
                if (configuration == 0) {
                    seq_times = measure(1, [&] {
                        filter.sequential(input, sequential);
                        return 1;
                    });
                } else {
                    const auto index = configuration - 1;
                    const int threads = team_sizes[index];
                    parallel_times[index] = measure(threads, [&] {
                        return filter.openmp(input, parallel_outputs[index], threads);
                    });
                }
            }
            first_configuration = (first_configuration + 1) % configuration_count;
            const double seq_ms = median(seq_times);
            if (seq_ms <= 0.0) {
                throw std::runtime_error("Sequential median is below the clock resolution");
            }
            const std::string prefix = csv_cell(name.string()) + ',' + std::to_string(size) + ',' +
                std::to_string(size) + ',' + std::to_string(kernel_size) + ',';
            std::ostringstream configuration;
            configuration << prefix << std::setprecision(std::numeric_limits<double>::max_digits10)
                          << sigma;
            const std::string image_config = configuration.str();
            log_times(raw, image_config + ",seq,1", seq_times);
            summary << image_config << ",seq,1," << seq_ms << ",,,,\n";

            for (std::size_t index = 0; index < team_sizes.size(); ++index) {
                const int threads = team_sizes[index];
                auto& parallel = parallel_outputs[index];
                const auto& omp_times = parallel_times[index];
                const double omp_ms = median(omp_times);
                if (omp_ms <= 0.0) {
                    throw std::runtime_error("OpenMP median is below the clock resolution");
                }
                const double speedup = seq_ms / omp_ms;
                const auto errors = compare(sequential, parallel);
                const auto config = image_config + ",omp," + std::to_string(threads);
                log_times(raw, config, omp_times);
                summary << config << ',' << omp_ms << ',' << speedup << ','
                        << speedup * 100.0 / threads << ',' << errors.max_absolute
                        << ',' << errors.rmse << '\n';
                if (errors.different != 0) {
                    all_equal = false;
                    const auto i = errors.first_difference;
                    mismatches << config << ',' << errors.different << ',' << i % size << ','
                               << i / size << ',' << int(sequential[i]) << ',' << int(parallel[i])
                               << '\n';
                    std::cerr << "Mismatch: " << name.string() << ' ' << size << " threads="
                              << threads << " MaxAE=" << errors.max_absolute
                              << " RMSE=" << errors.rmse << '\n';
                }
            }

            // Write each result once, after all timings for this image.
            if (save_images) {
                gaussian::write_pgm(images / "seq" / output_name, input, sequential);
                for (std::size_t index = 0; index < team_sizes.size(); ++index) {
                    const auto mode = "omp" + std::to_string(team_sizes[index]);
                    gaussian::write_pgm(images / mode / output_name, input, parallel_outputs[index]);
                }
            }

            if (save_images && name == names.front()) {
                const auto stem = output / "samples" / (std::to_string(size) + "_" + name.stem().string());
                Pixels source(input.pixels.size());
                std::transform(input.pixels.begin(), input.pixels.end(), source.begin(), [](float pixel) {
                    return static_cast<std::uint8_t>(double(pixel) + 0.5);
                });
                gaussian::write_pgm(stem.string() + "_input.pgm", input, source);
                gaussian::write_pgm(stem.string() + "_seq.pgm", input, sequential);
                const auto suffix = "_omp" + std::to_string(team_sizes.back()) + ".pgm";
                gaussian::write_pgm(stem.string() + suffix, input, parallel_outputs.back());
            }
            raw.flush();
            summary.flush();
            mismatches.flush();
            std::cout << size << 'x' << size << ' ' << name.string() << " done" << std::endl;
        }
    }
    raw.close();
    summary.close();
    mismatches.close();
    return all_equal;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " processed_dir new_output_dir [kernel_size] [sigma] [image.bin]\n"
                  << "       [--size 512|1024|2048] [--image image.bin] [--no-images]\n"
                  << "Default kernel size: " << gaussian::default_kernel_size
                  << ", sigma: " << gaussian::default_sigma << '\n';
        return 1;
    }
    try {
        std::vector<const char*> positional;
        std::vector<int> sizes(supported_sizes.begin(), supported_sizes.end());
        fs::path only_image;
        bool save_images = true;
        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--no-images") {
                save_images = false;
            } else if (arg == "--size" && i + 1 < argc) {
                const int size = gaussian::parse_integer(argv[++i]);
                if (std::find(supported_sizes.begin(), supported_sizes.end(), size) == supported_sizes.end()) {
                    throw std::invalid_argument("Size must be 512, 1024 or 2048");
                }
                sizes = {size};
            } else if (arg == "--image" && i + 1 < argc) {
                only_image = argv[++i];
                if (only_image.empty()) {
                    throw std::invalid_argument("--image requires a filename");
                }
            } else if (arg.rfind("--", 0) == 0) {
                throw std::invalid_argument("Unknown or incomplete option: " + arg);
            } else {
                positional.push_back(argv[i]);
            }
        }
        if (positional.size() > 3 || (positional.size() == 3 && !only_image.empty())) {
            throw std::invalid_argument("Too many positional arguments or image specified twice");
        }
        const int kernel_size = positional.empty() ? gaussian::default_kernel_size :
            gaussian::parse_integer(positional[0]);
        const double sigma = positional.size() < 2 ? gaussian::default_sigma :
            gaussian::parse_number(positional[1]);
        if (positional.size() == 3) {
            only_image = positional[2];
        }
        if (!only_image.empty() && (only_image.filename() != only_image ||
                                    only_image.extension() != ".bin")) {
            throw std::invalid_argument("Specify an image filename ending in .bin");
        }
        gaussian::configure_openmp();
        if (!benchmark(argv[1], argv[2], kernel_size, sigma, only_image, sizes, save_images)) {
            std::cerr << "Outputs differ. Inspect mismatches.csv and metrics.csv.\n";
            return 2;
        }
        std::cout << "All OpenMP outputs match Sequential.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
