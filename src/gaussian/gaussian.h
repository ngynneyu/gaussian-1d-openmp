#ifndef GAUSSIAN_H
#define GAUSSIAN_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum { DEFAULT_KERNEL_SIZE = 13, OPENMP_CONFIGS = 6 };
#define DEFAULT_SIGMA 2.0

extern const int openmp_thread_counts[OPENMP_CONFIGS];

typedef struct {
    int width;
    int height;
    size_t count;
    float *pixels;
} Image;

typedef struct {
    int size;
    double *weights;
} Kernel;

Kernel make_kernel(int size, double sigma);
void blur_horizontal_row(const Image *input, const Kernel *kernel, double *intermediate, int y);
void blur_vertical_row(const Image *input, const Kernel *kernel, const double *intermediate,
                       uint8_t *output, int y);
void blur_sequential(const Image *input, const Kernel *kernel, double *intermediate, uint8_t *output);
int blur_openmp(const Image *input, const Kernel *kernel, double *intermediate,
                uint8_t *output, int threads);

typedef struct GaussianState GaussianState;
GaussianState* gaussian_create(float* pixels, int size);
void gaussian_destroy(GaussianState* state);
void gaussian_seq(GaussianState* state, uint8_t* output);
int gaussian_omp(GaussianState* state, uint8_t* output, int threads);

_Noreturn void fail(const char *format, ...);
void *allocate_array(size_t count, size_t element_size);
int parse_integer(const char *text);
double parse_number(const char *text);
Image read_image(const char *path, int width, int height);
void write_pgm(const char *path, const Image *image, const uint8_t *pixels);
void close_output(FILE *file, const char *path);

#endif
