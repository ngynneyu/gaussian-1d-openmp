#include "teammate_filters.h"
#include "gaussian.h"

#include <stdlib.h>

struct GaussianState {
    Image input;
    Kernel kernel;
    double* scratch;
};

GaussianState* gaussian_create(float* pixels, int size) {
    GaussianState* state = allocate_array(1, sizeof(*state));
    const size_t count = (size_t)size * size;
    state->input = (Image){size, size, count, pixels};
    state->kernel = make_kernel(DEFAULT_KERNEL_SIZE, DEFAULT_SIGMA);
    state->scratch = allocate_array(count, sizeof(*state->scratch));
    return state;
}

void gaussian_destroy(GaussianState* state) {
    free(state->scratch);
    free(state->kernel.weights);
    free(state);
}

void gaussian_seq(GaussianState* state, uint8_t* output) {
    blur_sequential(&state->input, &state->kernel, state->scratch, output);
}

int gaussian_omp(GaussianState* state, uint8_t* output, int threads) {
    return blur_openmp(&state->input, &state->kernel, state->scratch, output, threads);
}
