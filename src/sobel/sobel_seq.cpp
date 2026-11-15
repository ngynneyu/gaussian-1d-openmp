#include "sobel.hpp"

void sobel_seq(const float* input, float* output, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    for (int y = 0; y < height; ++y) {
        sobel_detail::filter_row(input, output, width, height, y);
    }
}
