#pragma once

#include "gaussian.hpp"

#include <array>
#include <string>

#ifdef GAUSSIAN_SEPARABLE
#include "../gaussian_1d/gaussian_1d.hpp"
#endif

namespace gaussian {

inline constexpr int default_kernel_size = 19;
inline constexpr double default_sigma = 3.0;

#ifdef GAUSSIAN_SEPARABLE
inline constexpr std::array openmp_thread_counts{2, 4, 8, 16, 20};
#else
inline constexpr std::array openmp_thread_counts{1, 2, 4, 8};
#endif

inline std::string openmp_threads_csv() {
    std::string counts = std::to_string(openmp_thread_counts.front());
    for (std::size_t i = 1; i < openmp_thread_counts.size(); ++i) {
        counts += ',' + std::to_string(openmp_thread_counts[i]);
    }
    return counts;
}

// The command-line filter and benchmark share this adapter.
class Filter {
public:
    #ifdef GAUSSIAN_SEPARABLE
    static constexpr const char* algorithm = "separable 1D";

    Filter(int size, double sigma) : kernel_(separable::make_kernel(size, sigma)) {}

    void prepare(const Image& input) { intermediate_.resize(input.pixels.size()); }
    void sequential(const Image& input, Pixels& output) {
        separable::blur_sequential(input, kernel_, intermediate_, output);
    }
    int openmp(const Image& input, Pixels& output, int threads) {
        return separable::blur_openmp(input, kernel_, intermediate_, output, threads);
    }

private:
    separable::Kernel kernel_;
    std::vector<double> intermediate_;
    #else
    static constexpr const char* algorithm = "direct 2D";

    Filter(int size, double sigma) : kernel_(make_kernel(size, sigma)) {}

    void prepare(const Image&) {}
    void sequential(const Image& input, Pixels& output) {
        blur_sequential(input, kernel_, output);
    }
    int openmp(const Image& input, Pixels& output, int threads) {
        return blur_openmp(input, kernel_, output, threads);
    }

private:
    Kernel kernel_;
    #endif
};

}  // namespace gaussian
