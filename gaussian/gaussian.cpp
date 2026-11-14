#include "gaussian.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace gaussian {

Kernel make_kernel(int size, double sigma) {
    if (size < 1 || size % 2 == 0) {
        throw std::invalid_argument("Kernel size must be a positive odd integer");
    }
    if (!std::isfinite(sigma) || sigma <= 0.0) {
        throw std::invalid_argument("Sigma must be finite and positive");
    }

    Kernel kernel{size, {}};
    const auto side = static_cast<std::size_t>(size);
    if (side > kernel.weights.max_size() / side) {
        throw std::length_error("Kernel is too large");
    }
    kernel.weights.reserve(side * side);

    const int radius = size / 2;
    double total = 0.0;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            const double squared_distance = double(x) * x + double(y) * y;
            const double scaled_distance = std::sqrt(squared_distance) / sigma;
            // The Gaussian prefactor cancels during discrete normalization.
            const double weight = std::exp(-0.5 * scaled_distance * scaled_distance);
            kernel.weights.push_back(weight);
            total += weight;
        }
    }
    for (double& weight : kernel.weights) {
        weight /= total;
    }
    return kernel;
}

void blur_row(const Image& input, const Kernel& kernel, Pixels& output, int y) {
    const int radius = kernel.size / 2;
    const auto width = static_cast<std::size_t>(input.width);
    auto* output_row = output.data() + static_cast<std::size_t>(y) * width;

    for (int x = 0; x < input.width; ++x) {
        double sum = 0.0;
        const double* weight = kernel.weights.data();
        // The kernel is symmetric. Using positive offsets preserves convolution.
        for (int dy = -radius; dy <= radius; ++dy) {
            const auto sy = std::clamp<std::int64_t>(std::int64_t(y) + dy, 0, input.height - 1);
            const auto* row = input.pixels.data() + static_cast<std::size_t>(sy) * width;
            for (int dx = -radius; dx <= radius; ++dx) {
                const auto sx = std::clamp<std::int64_t>(std::int64_t(x) + dx, 0, input.width - 1);
                sum += *weight++ * row[sx];
            }
        }
        output_row[x] = static_cast<std::uint8_t>(std::clamp(sum, 0.0, 255.0) + 0.5);
    }
}

}  // namespace gaussian
