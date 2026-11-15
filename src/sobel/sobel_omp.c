#include "sobel.h"

void sobel_omp(const float* input, float* output, int width, int height, int threads) {
    if (width <= 0 || height <= 0) {
        return;
    }
    #pragma omp parallel for schedule(static) num_threads(threads) default(none) shared(input, output, width, height)
    for (int y = 0; y < height; ++y) {
        sobel_filter_row(input, output, width, height, y);
    }
}
