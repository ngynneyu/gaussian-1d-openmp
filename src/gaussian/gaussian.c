#include "gaussian.h"

#include <math.h>

Kernel make_kernel(int size, double sigma) {
    if (size < 1 || size % 2 == 0) {
        fail("Kernel size must be a positive odd integer");
    }
    if (!isfinite(sigma) || sigma <= 0.0) {
        fail("Sigma must be finite and positive");
    }

    Kernel kernel = {size, allocate_array((size_t)size, sizeof(double))};
    const int radius = size / 2;
    double total = 0.0;
    for (int i = 0; i < size; ++i) {
        const double distance = (double)(i - radius) / sigma;
        kernel.weights[i] = exp(-0.5 * distance * distance);
        total += kernel.weights[i];
    }
    for (int i = 0; i < size; ++i) {
        kernel.weights[i] /= total;
    }
    return kernel;
}

static int clamp_index(int64_t index, int length) {
    if (index < 0) {
        return 0;
    }
    if (index >= length) {
        return length - 1;
    }
    return (int)index;
}

void blur_horizontal_row(const Image *input, const Kernel *kernel, double *intermediate, int y) {
    const size_t offset = (size_t)y * input->width;
    const float *source = input->pixels + offset;
    double *destination = intermediate + offset;
    const int radius = kernel->size / 2;
    for (int x = 0; x < input->width; ++x) {
        const double center = source[x];
        double difference = 0.0;
        for (int i = 0; i < kernel->size; ++i) {
            const int sx = clamp_index((int64_t)x + (i - radius), input->width);
            difference += kernel->weights[i] * (source[sx] - center);
        }
        // Normalized weights sum to one. Accumulating differences keeps flat areas exact.
        destination[x] = center + difference;
    }
}

void blur_vertical_row(const Image *input, const Kernel *kernel, const double *intermediate,
                       uint8_t *output, int y) {
    const size_t width = (size_t)input->width;
    const size_t offset = (size_t)y * width;
    const int radius = kernel->size / 2;
    for (int x = 0; x < input->width; ++x) {
        const double center = intermediate[offset + x];
        double difference = 0.0;
        for (int i = 0; i < kernel->size; ++i) {
            const int sy = clamp_index((int64_t)y + (i - radius), input->height);
            difference += kernel->weights[i] * (intermediate[(size_t)sy * width + x] - center);
        }
        double value = center + difference;
        value = value < 0.0 ? 0.0 : value;
        value = value > 255.0 ? 255.0 : value;
        output[offset + x] = (uint8_t)(value + 0.5);
    }
}

void blur_sequential(const Image *input, const Kernel *kernel, double *intermediate, uint8_t *output) {
    for (int y = 0; y < input->height; ++y) {
        blur_horizontal_row(input, kernel, intermediate, y);
    }
    for (int y = 0; y < input->height; ++y) {
        blur_vertical_row(input, kernel, intermediate, output, y);
    }
}
