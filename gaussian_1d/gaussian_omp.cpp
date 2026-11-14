#include "gaussian_1d.hpp"

#include <omp.h>

namespace gaussian::separable {

int blur_openmp(const Image& input, const Kernel& kernel,
                std::vector<double>& intermediate, Pixels& output, int threads) {
    const int height = input.height;
    int actual_threads = 0;
    #pragma omp parallel num_threads(threads) default(none) \
        shared(input, kernel, intermediate, output, height, actual_threads, threads)
    {
        #pragma omp for schedule(static)
        for (int y = 0; y < height; ++y) {
            if (y == 0) {
                actual_threads = omp_get_num_threads();
            }
            blur_horizontal_row(input, kernel, intermediate, y);
        }
        // The implicit barrier above makes the complete intermediate image visible.
        #pragma omp for schedule(static)
        for (int y = 0; y < height; ++y) {
            blur_vertical_row(input, kernel, intermediate, output, y);
        }
    }
    return actual_threads;
}

}  // namespace gaussian::separable
