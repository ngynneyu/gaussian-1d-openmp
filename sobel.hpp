#pragma once

#include <cmath>
#include <cstddef>

// Input and output are separate preallocated row-major float buffers.
// The one-pixel output border is zero. Interior values are raw L2 magnitudes.
void sobel_seq(const float* input, float* output, int width, int height);
void sobel_omp(const float* input, float* output, int width, int height, int threads);

namespace sobel_detail {
inline void filter_row(const float* input, float* output, int width, int height, int y) {
    float* dst = output + static_cast<std::size_t>(y) * width;
    if (y == 0 || y == height - 1) {
        for (int x = 0; x < width; ++x) {
            dst[x] = 0.0f;
        }
        return;
    }

    const float* center = input + static_cast<std::size_t>(y) * width;
    const float* top = center - width;
    const float* bottom = center + width;
    dst[0] = 0.0f;
    for (int x = 1; x < width - 1; ++x) {
        const float gx = (top[x + 1] - top[x - 1])
                       + 2.0f * (center[x + 1] - center[x - 1])
                       + (bottom[x + 1] - bottom[x - 1]);
        const float gy = (bottom[x - 1] - top[x - 1])
                       + 2.0f * (bottom[x] - top[x])
                       + (bottom[x + 1] - top[x + 1]);
        dst[x] = std::sqrt(gx * gx + gy * gy);
    }
    dst[width - 1] = 0.0f;
}
}  // namespace sobel_detail
