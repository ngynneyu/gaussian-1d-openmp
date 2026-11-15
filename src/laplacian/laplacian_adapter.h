#pragma once

void laplacian_seq(const float* input, float* output, int size);
void laplacian_omp(const float* input, float* output, int size, int threads);
