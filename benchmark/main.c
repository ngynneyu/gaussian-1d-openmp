#include "gaussian/gaussian_adapter.h"
#include "gaussian/gaussian.h"
#include "laplacian/laplacian_adapter.h"
#include "sobel/sobel.h"

#include <errno.h>
#include <float.h>
#include <math.h>
#include <omp.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

enum { IMAGE_COUNT = 20, REPEATS = 10, PATH_CAPACITY = 4096 };
static const int sizes[] = {512, 1024, 2048};
enum { SIZE_COUNT = sizeof sizes / sizeof sizes[0],
       THREAD_CONFIGS = OPENMP_CONFIGS };

typedef enum { SOBEL, GAUSSIAN, LAPLACIAN, FILTER_COUNT } Filter;
static const char* const filter_names[] = {"Sobel", "Gaussian", "Laplacian"};

typedef struct {
    Filter filter;
    int size;
    float* pixels;
    GaussianState* gaussian;
} ImageJob;

typedef struct {
    double max_error;
    double rmse;
} ErrorMetrics;

static _Noreturn void die(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
    exit(2);
}

#ifdef _WIN32
typedef LONGLONG BenchmarkTick;
static double milliseconds_per_tick;

static void initialize_clock(void) {
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
        die("Cannot initialize the Windows performance counter");
    }
    milliseconds_per_tick = 1000.0 / frequency.QuadPart;
}

static BenchmarkTick timestamp(void) {
    LARGE_INTEGER now;
    if (!QueryPerformanceCounter(&now)) {
        die("Cannot read the Windows performance counter");
    }
    return now.QuadPart;
}
#else
typedef double BenchmarkTick;
static const double milliseconds_per_tick = 1000.0;

static BenchmarkTick timestamp(void) {
    return omp_get_wtime();
}
#endif

static void format_path(char path[PATH_CAPACITY], const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int length = vsnprintf(path, PATH_CAPACITY, format, arguments);
    va_end(arguments);
    if (length < 0 || length >= PATH_CAPACITY) {
        die("Benchmark path is too long");
    }
}

static void make_directory(const char* path) {
    #ifdef _WIN32
    const int status = _mkdir(path);
    #else
    const int status = mkdir(path, 0777);
    #endif
    if (status != 0 && errno != EEXIST) {
        die("Cannot create %s: %s", path, strerror(errno));
    }
}

static float* load_image(const char* file, size_t count) {
    FILE* input = fopen(file, "rb");
    if (!input) {
        die("Cannot open %s: %s", file, strerror(errno));
    }
    float* pixels = malloc(count * sizeof(*pixels));
    if (!pixels) {
        die("Cannot allocate memory for %s", file);
    }
    const size_t read_count = fread(pixels, sizeof(*pixels), count, input);
    const int extra = fgetc(input);
    const bool valid_size = read_count == count && extra == EOF && !ferror(input);
    fclose(input);
    if (!valid_size) {
        die("Expected exactly %zu float32 pixels in %s", count, file);
    }
    for (size_t i = 0; i < count; ++i) {
        if (!isfinite(pixels[i]) || pixels[i] < 0.0f || pixels[i] > 255.0f) {
            die("Expected finite grayscale pixels in %s", file);
        }
    }
    return pixels;
}

static void save_pixels(const char* path, const void* pixels, size_t pixel_bytes, size_t count) {
    FILE* file = fopen(path, "wb");
    if (!file) {
        die("Cannot open %s: %s", path, strerror(errno));
    }
    const size_t written = fwrite(pixels, pixel_bytes, count, file);
    const int status = fclose(file);
    if (written != count || status != 0) {
        die("Cannot write %s", path);
    }
}

static void save_sample(const char* directory, const char* filter, const char* version,
                        const void* pixels, size_t pixel_bytes, size_t count) {
    char path[PATH_CAPACITY];
    format_path(path, "%s/%s_%s.bin", directory, filter, version);
    save_pixels(path, pixels, pixel_bytes, count);
}

static int apply_filter(const ImageJob* job, void* output, int threads) {
    const int n = job->size;
    switch (job->filter) {
        case SOBEL:
            if (threads == 0) {
                sobel_seq(job->pixels, output, n, n);
            } else {
                sobel_omp(job->pixels, output, n, n, threads);
            }
            return threads;
        case GAUSSIAN:
            if (threads == 0) {
                gaussian_seq(job->gaussian, output);
                return 0;
            }
            return gaussian_omp(job->gaussian, output, threads);
        case LAPLACIAN:
            if (threads == 0) {
                laplacian_seq(job->pixels, output, n);
            } else {
                laplacian_omp(job->pixels, output, n, threads);
            }
            return threads;
        default:
            die("Unknown filter");
    }
}

static void measure(const ImageJob* job, void* output, int threads, double times[REPEATS]) {
    if (apply_filter(job, output, threads) != threads) {
        die("OpenMP created an unexpected team size during warm-up");
    }
    for (int run = 0; run < REPEATS; ++run) {
        const BenchmarkTick start = timestamp();
        const int actual = apply_filter(job, output, threads);
        times[run] = (timestamp() - start) * milliseconds_per_tick;
        if (actual != threads) {
            die("OpenMP created an unexpected team size during measurement");
        }
    }
}

static ErrorMetrics compare_outputs(const void* reference, const void* actual,
                                    size_t count, bool byte_output) {
    const uint8_t* ref_bytes = reference;
    const uint8_t* actual_bytes = actual;
    const float* ref_floats = reference;
    const float* actual_floats = actual;
    double max_error = 0.0;
    double squared_error = 0.0;
    for (size_t i = 0; i < count; ++i) {
        const double a = byte_output ? (double)ref_bytes[i] : (double)ref_floats[i];
        const double b = byte_output ? (double)actual_bytes[i] : (double)actual_floats[i];
        const double error = fabs(a - b);
        if (!isfinite(error)) {
            die("Non-finite filter output");
        }
        if (error > max_error) {
            max_error = error;
        }
        squared_error += error * error;
    }
    return (ErrorMetrics){max_error, sqrt(squared_error / count)};
}

static void write_times(FILE* output, const double times[REPEATS]) {
    for (int run = 0; run < REPEATS; ++run) {
        fprintf(output, ",%.12g", times[run]);
    }
}

static bool benchmark_image(FILE* output, const char* samples_dir, const char* png_source_dir,
                            const ImageJob* job, int image_index, size_t count) {
    const char* name = filter_names[job->filter];
    const bool byte_output = job->filter == GAUSSIAN;
    const size_t pixel_bytes = byte_output ? sizeof(uint8_t) : sizeof(float);
    void* reference = calloc(count, pixel_bytes);
    void* actual = calloc(count, pixel_bytes);
    if (!reference || !actual) {
        die("Cannot allocate output buffers for %s", name);
    }

    double baseline[REPEATS];
    measure(job, reference, 0, baseline);
    const bool example = job->size == sizes[0] && image_index == 1;
    if (example) {
        save_sample(samples_dir, name, "seq", reference, pixel_bytes, count);
    }

    bool identical = true;
    for (int offset = 0; offset < THREAD_CONFIGS; ++offset) {
        const int threads = openmp_thread_counts[(image_index - 1 + offset) % THREAD_CONFIGS];
        double parallel[REPEATS];
        measure(job, actual, threads, parallel);
        const ErrorMetrics errors = compare_outputs(reference, actual, count, byte_output);
        identical = identical && errors.max_error == 0.0;
        fprintf(output, "%s,%d,image_%02d,%d,%.12g,%.12g", name, job->size, image_index,
                threads, errors.max_error, errors.rmse);
        write_times(output, baseline);
        write_times(output, parallel);
        fputc('\n', output);
        if (example && threads == 4) {
            save_sample(samples_dir, name, "omp4", actual, pixel_bytes, count);
        }
    }

    if (png_source_dir && identical) {
        char path[PATH_CAPACITY];
        format_path(path, "%s/%s_%d_image_%02d.bin", png_source_dir, name, job->size, image_index);
        save_pixels(path, reference, pixel_bytes, count);
    }

    free(actual);
    free(reference);
    return identical;
}

static void check_teams(void) {
    omp_set_dynamic(0);
    for (int index = 0; index < THREAD_CONFIGS; ++index) {
        const int threads = openmp_thread_counts[index];
        int actual = 0;
        #pragma omp parallel num_threads(threads)
        {
            #pragma omp single
            actual = omp_get_num_threads();
        }
        if (actual != threads) {
            die("OpenMP runtime could not start %d threads", threads);
        }
    }
}

int main(int argc, char** argv) {
    _Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
                   "The dataset uses IEEE 754 float32");
    if (argc != 3 && argc != 4) {
        die("Usage: image_benchmark PROCESSED_DIR OUTPUT_DIR [PNG_SOURCE_DIR]");
    }
    const char* png_source_dir = argc == 4 ? argv[3] : NULL;
    const unsigned int endian_probe = 1;
    if (*(const unsigned char*)&endian_probe != 1) {
        die("This benchmark requires little-endian float32");
    }

    char samples_dir[PATH_CAPACITY];
    char path[PATH_CAPACITY];
    format_path(samples_dir, "%s/samples", argv[2]);
    make_directory(argv[2]);
    make_directory(samples_dir);
    format_path(path, "%s/timings.csv", argv[2]);
    FILE* output = fopen(path, "w");
    if (!output) {
        die("Cannot open %s: %s", path, strerror(errno));
    }
    fputs("algorithm,size,image,threads,max_abs_error,rmse", output);
    for (int run = 1; run <= REPEATS; ++run) {
        fprintf(output, ",seq_%d_ms", run);
    }
    for (int run = 1; run <= REPEATS; ++run) {
        fprintf(output, ",omp_%d_ms", run);
    }
    fputc('\n', output);

    #ifdef _WIN32
    initialize_clock();
    #endif
    check_teams();
    printf("OpenMP logical CPUs: %d\n", omp_get_num_procs());
    printf("Gaussian kernel: %dx%d, sigma: %g\n",
           DEFAULT_KERNEL_SIZE, DEFAULT_KERNEL_SIZE, DEFAULT_SIGMA);
    fflush(stdout);
    bool all_identical = true;
    for (int filter = 0; filter < FILTER_COUNT; ++filter) {
        for (int index = 0; index < SIZE_COUNT; ++index) {
            const int size = sizes[index];
            const size_t count = (size_t)size * size;
            for (int image = 1; image <= IMAGE_COUNT; ++image) {
                format_path(path, "%s/%d/image_%02d.bin", argv[1], size, image);
                ImageJob job = {filter, size, load_image(path, count), NULL};
                if (filter == GAUSSIAN) {
                    job.gaussian = gaussian_create(job.pixels, size);
                }
                const bool identical = benchmark_image(output, samples_dir, png_source_dir,
                                                       &job, image, count);
                all_identical = all_identical && identical;
                if (job.gaussian) {
                    gaussian_destroy(job.gaussian);
                }
                free(job.pixels);
            }
            if (fflush(output) != 0 || ferror(output)) {
                die("Cannot write timings.csv in %s", argv[2]);
            }
            printf("%s %dx%d: %d/%d\n", filter_names[filter], size, size, IMAGE_COUNT, IMAGE_COUNT);
            fflush(stdout);
        }
    }
    if (fclose(output) != 0) {
        die("Cannot close timings.csv in %s", argv[2]);
    }
    return all_identical ? 0 : 1;
}
