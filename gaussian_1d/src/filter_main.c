#include "gaussian.h"

#include <stdlib.h>

#ifdef GAUSSIAN_USE_OPENMP
#include <omp.h>
#endif

int main(int argc, char *argv[]) {
    #ifdef GAUSSIAN_USE_OPENMP
    const int max_args = 8;
    const char *optional_threads = " [threads]";
    #else
    const int max_args = 7;
    const char *optional_threads = "";
    #endif
    if (argc < 5 || argc > max_args) {
        fprintf(stderr, "Usage: %s input.bin width height output.pgm [kernel_size] [sigma]%s\n"
                        "Default kernel size: %d, sigma: %g\n",
                argv[0], optional_threads, DEFAULT_KERNEL_SIZE, DEFAULT_SIGMA);
        return EXIT_FAILURE;
    }

    const int width = parse_integer(argv[2]);
    const int height = parse_integer(argv[3]);
    const int size = argc >= 6 ? parse_integer(argv[5]) : DEFAULT_KERNEL_SIZE;
    const double sigma = argc >= 7 ? parse_number(argv[6]) : DEFAULT_SIGMA;
    #ifdef GAUSSIAN_USE_OPENMP
    const int threads = argc == 8 ? parse_integer(argv[7]) : 4;
    int index = 0;
    while (index < OPENMP_CONFIGS && openmp_thread_counts[index] != threads) {
        ++index;
    }
    if (index == OPENMP_CONFIGS) {
        fail("Supported thread counts: 2,4,8,16,20");
    }
    omp_set_dynamic(0);
    #endif

    Kernel kernel = make_kernel(size, sigma);
    Image input = read_image(argv[1], width, height);
    double *intermediate = allocate_array(input.count, sizeof(double));
    uint8_t *output = allocate_array(input.count, sizeof(uint8_t));
    #ifdef GAUSSIAN_USE_OPENMP
    const int actual = blur_openmp(&input, &kernel, intermediate, output, threads);
    if (actual != threads) {
        fail("OpenMP supplied %d threads, requested %d", actual, threads);
    }
    #else
    blur_sequential(&input, &kernel, intermediate, output);
    #endif
    write_pgm(argv[4], &input, output);
    printf("Saved %s\n", argv[4]);
    free(output);
    free(intermediate);
    free(input.pixels);
    free(kernel.weights);
    return EXIT_SUCCESS;
}
