#include "gaussian.hpp"

#include <omp.h>

namespace gaussian {

int blur_openmp(const Image& input, const Kernel& kernel, Pixels& output, int threads) {
    const int height = input.height;
    int actual_threads = 0;
    #pragma omp parallel for schedule(static) num_threads(threads) default(none) \
        shared(input, kernel, output, actual_threads, height, threads)
    for (int y = 0; y < height; ++y) {
        // Exactly one iteration observes the team size. The caller reads it after the join.
        if (y == 0) {
            actual_threads = omp_get_num_threads();
        }
        blur_row(input, kernel, output, y);
    }
    return actual_threads;
}

}  // namespace gaussian
