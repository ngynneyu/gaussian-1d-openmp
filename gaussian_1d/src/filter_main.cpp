#include "filter_backend.hpp"
#include "image_io.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) {
    #ifdef GAUSSIAN_USE_OPENMP
    constexpr int max_args = 8;
    constexpr auto optional_threads = " [threads]";
    #else
    constexpr int max_args = 7;
    constexpr auto optional_threads = "";
    #endif
    if (argc < 5 || argc > max_args) {
        std::cerr << "Usage: " << argv[0]
                  << " input.bin width height output.pgm [kernel_size] [sigma]"
                  << optional_threads << "\nDefault kernel size: " << gaussian::default_kernel_size
                  << ", sigma: " << gaussian::default_sigma << '\n';
        return 1;
    }

    try {
        const int width = gaussian::parse_integer(argv[2]);
        const int height = gaussian::parse_integer(argv[3]);
        const int size = argc >= 6 ? gaussian::parse_integer(argv[5]) : gaussian::default_kernel_size;
        const double sigma = argc >= 7 ? gaussian::parse_number(argv[6]) : gaussian::default_sigma;
        #ifdef GAUSSIAN_USE_OPENMP
        const int threads = argc == 8 ? gaussian::parse_integer(argv[7]) : 4;
        const auto& counts = gaussian::openmp_thread_counts;
        if (std::find(counts.begin(), counts.end(), threads) == counts.end()) {
            throw std::invalid_argument("Supported thread counts: " + gaussian::openmp_threads_csv());
        }
        gaussian::configure_openmp();
        #endif
        gaussian::Filter filter(size, sigma);
        const auto input = gaussian::read_image(argv[1], width, height);
        gaussian::Pixels output(input.pixels.size());
        filter.prepare(input);
        #ifdef GAUSSIAN_USE_OPENMP
        const int actual = filter.openmp(input, output, threads);
        if (actual != threads) {
            throw std::runtime_error("OpenMP supplied " + std::to_string(actual) +
                                     " threads, requested " + std::to_string(threads));
        }
        #else
        filter.sequential(input, output);
        #endif
        gaussian::write_pgm(argv[4], input, output);
        std::cout << "Saved " << argv[4] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
