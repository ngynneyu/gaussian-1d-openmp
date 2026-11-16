#include "gaussian.h"

#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

struct GaussianState {
    Image input;
    Kernel kernel;
    double* scratch;
};

Kernel make_kernel(int size, double sigma) {
    if (size < 1 || size % 2 == 0) {
        fail("Kernel size must be a positive odd integer");
    }
    if (!isfinite(sigma) || sigma <= 0.0) {
        fail("Sigma must be finite and positive");
    }

    Kernel kernel = {size, allocate_array((size_t)size, sizeof(double))};
    const int radius = size / 2;
    double total = 0.0;
    for (int i = 0; i < size; ++i) {
        const double distance = (double)(i - radius) / sigma;
        kernel.weights[i] = exp(-0.5 * distance * distance);
        total += kernel.weights[i];
    }
    for (int i = 0; i < size; ++i) {
        kernel.weights[i] /= total;
    }
    return kernel;
}

static int clamp_index(int64_t index, int length) {
    if (index < 0) {
        return 0;
    }
    if (index >= length) {
        return length - 1;
    }
    return (int)index;
}

void blur_horizontal_row(const Image *input, const Kernel *kernel, double *intermediate, int y) {
    const size_t offset = (size_t)y * input->width;
    const float *source = input->pixels + offset;
    double *destination = intermediate + offset;
    const int radius = kernel->size / 2;
    for (int x = 0; x < input->width; ++x) {
        const double center = source[x];
        double difference = 0.0;
        for (int i = 0; i < kernel->size; ++i) {
            const int sx = clamp_index((int64_t)x + (i - radius), input->width);
            difference += kernel->weights[i] * (source[sx] - center);
        }
        // Normalized weights sum to one. Accumulating differences keeps flat areas exact.
        destination[x] = center + difference;
    }
}

void blur_vertical_row(const Image *input, const Kernel *kernel, const double *intermediate,
                       uint8_t *output, int y) {
    const size_t width = (size_t)input->width;
    const size_t offset = (size_t)y * width;
    const int radius = kernel->size / 2;
    for (int x = 0; x < input->width; ++x) {
        const double center = intermediate[offset + x];
        double difference = 0.0;
        for (int i = 0; i < kernel->size; ++i) {
            const int sy = clamp_index((int64_t)y + (i - radius), input->height);
            difference += kernel->weights[i] * (intermediate[(size_t)sy * width + x] - center);
        }
        double value = center + difference;
        value = value < 0.0 ? 0.0 : value;
        value = value > 255.0 ? 255.0 : value;
        output[offset + x] = (uint8_t)(value + 0.5);
    }
}

void blur_sequential(const Image *input, const Kernel *kernel, double *intermediate, uint8_t *output) {
    for (int y = 0; y < input->height; ++y) {
        blur_horizontal_row(input, kernel, intermediate, y);
    }
    for (int y = 0; y < input->height; ++y) {
        blur_vertical_row(input, kernel, intermediate, output, y);
    }
}

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

/* Allocation and image I/O helpers. */
_Static_assert(CHAR_BIT == 8 && sizeof(float) == 4 && FLT_RADIX == 2 &&
               FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128, "Dataset requires IEEE 754 float32");

_Noreturn void fail(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fputs("Error: ", stderr);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
    exit(EXIT_FAILURE);
}

void *allocate_array(size_t count, size_t element_size) {
    if (count > SIZE_MAX / element_size) {
        fail("Allocation is too large");
    }
    void *buffer = malloc(count * element_size);
    if (buffer == NULL) {
        fail("Cannot allocate %zu bytes", count * element_size);
    }
    return buffer;
}

int parse_integer(const char *text) {
    char *end;
    errno = 0;
    const long number = strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE || number < INT_MIN || number > INT_MAX) {
        fail("Invalid integer: %s", text);
    }
    return (int)number;
}

double parse_number(const char *text) {
    char *end;
    errno = 0;
    const double number = strtod(text, &end);
    if (end == text || *end != '\0' || errno == ERANGE) {
        fail("Invalid number: %s", text);
    }
    return number;
}

Image read_image(const char *path, int width, int height) {
    const uint32_t endian_test = 1;
    if (*(const unsigned char *)&endian_test != 1) {
        fail("Dataset reader requires a little-endian host");
    }
    if (width <= 0 || height <= 0) {
        fail("Image dimensions must be positive");
    }
    if ((size_t)width > SIZE_MAX / (size_t)height / sizeof(float)) {
        fail("Image is too large");
    }
    const size_t count = (size_t)width * (size_t)height;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fail("Cannot open %s: %s", path, strerror(errno));
    }
    #ifdef _WIN32
    struct _stat64 info;
    const int stat_status = _fstat64(_fileno(file), &info);
    #else
    struct stat info;
    const int stat_status = fstat(fileno(file), &info);
    #endif
    if (stat_status != 0 || !S_ISREG(info.st_mode)) {
        fail("Cannot read regular image file: %s", path);
    }
    if ((uintmax_t)info.st_size != count * sizeof(float)) {
        fail("File size does not match width * height * 4: %s", path);
    }
    Image image = {width, height, count, allocate_array(count, sizeof(float))};
    if (fread(image.pixels, sizeof(float), count, file) != count || fgetc(file) != EOF ||
        ferror(file)) {
        fail("Cannot read image: %s", path);
    }
    fclose(file);
    for (size_t i = 0; i < count; ++i) {
        const float pixel = image.pixels[i];
        if (!isfinite(pixel) || pixel < 0.0f || pixel > 255.0f) {
            fail("Expected finite grayscale pixels in [0, 255]: %s", path);
        }
    }
    return image;
}

void close_output(FILE *file, const char *path) {
    const int write_error = ferror(file);
    if (fclose(file) != 0 || write_error) {
        fail("Cannot write %s", path);
    }
}

void write_pgm(const char *path, const Image *image, const uint8_t *pixels) {
    #ifdef _WIN32
    const int fd = _open(path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
    #else
    const int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    #endif
    if (fd < 0) {
        fail("Cannot create PGM %s: %s", path, strerror(errno));
    }
    #ifdef _WIN32
    FILE *file = _fdopen(fd, "wb");
    #else
    FILE *file = fdopen(fd, "wb");
    #endif
    if (file == NULL) {
        #ifdef _WIN32
        _close(fd);
        #else
        close(fd);
        #endif
        remove(path);
        fail("Cannot open PGM stream: %s", path);
    }
    if (fprintf(file, "P5\n%d %d\n255\n", image->width, image->height) < 0 ||
        fwrite(pixels, 1, image->count, file) != image->count) {
        fail("Cannot write PGM: %s", path);
    }
    close_output(file, path);
}
