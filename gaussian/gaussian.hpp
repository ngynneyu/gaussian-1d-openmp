#pragma once

#include <cstdint>
#include <vector>

namespace gaussian {

using Pixels = std::vector<std::uint8_t>;

struct Image {
    int width;
    int height;
    std::vector<float> pixels;
};

struct Kernel {
    int size;
    std::vector<double> weights;
};

Kernel make_kernel(int size, double sigma);

// The caller supplies a valid image, a kernel from make_kernel, and a
// separate output buffer with one byte per pixel.
void blur_sequential(const Image& input, const Kernel& kernel, Pixels& output);
int blur_openmp(const Image& input, const Kernel& kernel, Pixels& output, int threads);

// Shared row implementation. Each call writes only its assigned row.
void blur_row(const Image& input, const Kernel& kernel, Pixels& output, int y);

void configure_openmp();
int openmp_version();
int openmp_available_processors();

}  // namespace gaussian
