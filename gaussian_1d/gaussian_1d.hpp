#pragma once

#include "gaussian.hpp"

namespace gaussian::separable {

using Kernel = std::vector<double>;

Kernel make_kernel(int size, double sigma);

// Each call writes only the requested row. Finish all horizontal rows before
// calling any vertical rows, which can read neighboring rows of the buffer.
void blur_horizontal_row(const Image& input, const Kernel& kernel,
                         std::vector<double>& intermediate, int y);
void blur_vertical_row(const Image& input, const Kernel& kernel,
                       const std::vector<double>& intermediate, Pixels& output, int y);

void blur_sequential(const Image& input, const Kernel& kernel,
                     std::vector<double>& intermediate, Pixels& output);
int blur_openmp(const Image& input, const Kernel& kernel,
                std::vector<double>& intermediate, Pixels& output, int threads);

}  // namespace gaussian::separable
