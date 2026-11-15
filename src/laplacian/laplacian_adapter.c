#include "laplacian_adapter.h"

/* The upstream kernels are static. Include them in this translation unit. */
#define main upstream_laplacian_main
#include "laplacian_benchmark.c"
#undef main

void laplacian_seq(const float* input, float* output, int size) {
    laplacian_sequential(input, output, size);
}

void laplacian_omp(const float* input, float* output, int size, int threads) {
    laplacian_openmp(input, output, size, threads);
}
