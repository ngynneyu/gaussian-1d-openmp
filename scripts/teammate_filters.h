#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GaussianState GaussianState;
GaussianState* gaussian_create(float* pixels, int size);
void gaussian_destroy(GaussianState* state);
void gaussian_seq(GaussianState* state, uint8_t* output);
int gaussian_omp(GaussianState* state, uint8_t* output, int threads);

void laplacian_seq(const float* input, float* output, int size);
void laplacian_omp(const float* input, float* output, int size, int threads);

#ifdef __cplusplus
}
#endif
