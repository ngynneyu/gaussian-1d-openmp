#include "gaussian_1d.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace gaussian::separable {

Kernel make_kernel(int size, double sigma) {
    if (size < 1 || size % 2 == 0) {
        throw std::invalid_argument("Kernel size must be a positive odd integer");
    }
    if (!std::isfinite(sigma) || sigma <= 0.0) {
        throw std::invalid_argument("Sigma must be finite and positive");
    }

    Kernel kernel(size);
    const int radius = size / 2;
    double total = 0.0;
    for (int i = 0; i < size; ++i) {
        const double distance = double(i - radius) / sigma;
        kernel[i] = std::exp(-0.5 * distance * distance);
        total += kernel[i];
    }
    for (double& weight : kernel) {
        weight /= total;
    }
    return kernel;
}

void blur_horizontal_row(const Image& input, const Kernel& kernel,
                         std::vector<double>& intermediate, int y) {
    const auto offset = static_cast<std::size_t>(y) * input.width;
    const float* source = input.pixels.data() + offset;
    double* destination = intermediate.data() + offset;
    const int size = static_cast<int>(kernel.size());
    const int radius = size / 2;
    for (int x = 0; x < input.width; ++x) {
        const double center = source[x];
        double difference = 0.0;
        for (int i = 0; i < size; ++i) {
            const auto sx = std::clamp<std::int64_t>(
                std::int64_t(x) + (i - radius), 0, input.width - 1);
            difference += kernel[i] * (source[sx] - center);
        }
        // Normalized weights sum to one. Accumulating differences keeps flat areas exact.
        destination[x] = center + difference;
    }
}

void blur_vertical_row(const Image& input, const Kernel& kernel,
                       const std::vector<double>& intermediate, Pixels& output, int y) {
    const auto width = static_cast<std::size_t>(input.width);
    const auto offset = static_cast<std::size_t>(y) * width;
    auto* destination = output.data() + offset;
    const int size = static_cast<int>(kernel.size());
    const int radius = size / 2;
    for (int x = 0; x < input.width; ++x) {
        const double center = intermediate[offset + x];
        double difference = 0.0;
        for (int i = 0; i < size; ++i) {
            const auto sy = std::clamp<std::int64_t>(
                std::int64_t(y) + (i - radius), 0, input.height - 1);
            difference += kernel[i] * (intermediate[static_cast<std::size_t>(sy) * width + x] - center);
        }
        destination[x] = static_cast<std::uint8_t>(std::clamp(center + difference, 0.0, 255.0) + 0.5);
    }
}

}  // namespace gaussian::separable
