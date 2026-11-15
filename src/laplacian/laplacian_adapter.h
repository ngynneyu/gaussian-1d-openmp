#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void laplacian_seq(const float* input, float* output, int size);
void laplacian_omp(const float* input, float* output, int size, int threads);

#ifdef __cplusplus
}
#endif
