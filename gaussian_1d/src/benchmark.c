#include "gaussian.h"

#include <dirent.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <omp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

enum { DATASET_IMAGES = 20, REPETITIONS = 10, CONFIGURATIONS = OPENMP_CONFIGS + 1 };
static const int supported_sizes[] = {512, 1024, 2048};
static const char columns[] = "image,width,height,kernel_size,sigma,implementation,threads";

#ifdef _WIN32
typedef LONGLONG BenchmarkTime;
static double milliseconds_per_tick;
static const char benchmark_clock_name[] = "QueryPerformanceCounter";

static void initialize_benchmark_clock(void) {
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
        fail("Cannot initialize the Windows performance counter");
    }
    milliseconds_per_tick = 1000.0 / (double)frequency.QuadPart;
}

static BenchmarkTime benchmark_now(void) {
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter)) {
        fail("Cannot read the Windows performance counter");
    }
    return counter.QuadPart;
}

static double elapsed_ms(BenchmarkTime start, BenchmarkTime end) {
    return (double)(end - start) * milliseconds_per_tick;
}
#else
typedef double BenchmarkTime;
static const char benchmark_clock_name[] = "omp_get_wtime";

static BenchmarkTime benchmark_now(void) {
    return omp_get_wtime();
}

static double elapsed_ms(BenchmarkTime start, BenchmarkTime end) {
    return (end - start) * 1000.0;
}
#endif

typedef struct {
    const char *data;
    const char *output;
    const char *only_image;
    const int *sizes;
    size_t size_count;
    int kernel_size;
    double sigma;
    bool save_images;
} Options;

typedef struct {
    FILE *raw;
    FILE *metrics;
    FILE *mismatches;
} Logs;

typedef struct {
    uint8_t *pixels;
    double samples[REPETITIONS];
    double median;
} Measurement;

typedef struct {
    int max_absolute;
    double rmse;
    size_t different;
    size_t first_difference;
} Errors;

static char *format_text(const char *format, ...) {
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    const int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0) {
        fail("Cannot format output path");
    }
    char *text = allocate_array((size_t)length + 1, sizeof(char));
    vsnprintf(text, (size_t)length + 1, format, args);
    va_end(args);
    return text;
}

static void make_directory(const char *path) {
    #ifdef _WIN32
    const int status = _mkdir(path);
    #else
    const int status = mkdir(path, 0777);
    #endif
    if (status != 0) {
        fail("Cannot create directory %s: %s", path, strerror(errno));
    }
}

static bool regular_file(const char *path) {
    #ifdef _WIN32
    struct _stat64 info;
    return _stat64(path, &info) == 0 && S_ISREG(info.st_mode);
    #else
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
    #endif
}

static bool bin_name(const char *name) {
    const size_t length = strlen(name);
    return length > 4 && strcmp(name + length - 4, ".bin") == 0;
}

static int compare_names(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

static size_t find_images(const Options *options, char *names[DATASET_IMAGES]) {
    if (options->only_image != NULL) {
        for (size_t i = 0; i < options->size_count; ++i) {
            char *path = format_text("%s/%d/%s", options->data, options->sizes[i],
                                     options->only_image);
            if (!regular_file(path)) {
                fail("Missing image: %s", path);
            }
            free(path);
        }
        names[0] = format_text("%s", options->only_image);
        return 1;
    }

    for (size_t i = 0; i < options->size_count; ++i) {
        char *folder = format_text("%s/%d", options->data, options->sizes[i]);
        DIR *directory = opendir(folder);
        if (directory == NULL) {
            fail("Cannot open dataset directory %s: %s", folder, strerror(errno));
        }
        char *current[DATASET_IMAGES];
        size_t count = 0;
        for (;;) {
            errno = 0;
            const struct dirent *entry = readdir(directory);
            if (entry == NULL) {
                if (errno != 0) {
                    fail("Cannot list dataset directory %s: %s", folder, strerror(errno));
                }
                break;
            }
            if (bin_name(entry->d_name)) {
                char *path = format_text("%s/%s", folder, entry->d_name);
                if (regular_file(path)) {
                    if (count == DATASET_IMAGES) {
                        fail("Expected %d .bin files in %s", DATASET_IMAGES, folder);
                    }
                    current[count++] = format_text("%s", entry->d_name);
                }
                free(path);
            }
        }
        closedir(directory);
        if (count != DATASET_IMAGES) {
            fail("Expected %d .bin files in %s", DATASET_IMAGES, folder);
        }
        qsort(current, count, sizeof(current[0]), compare_names);
        for (size_t j = 0; j < count; ++j) {
            if (i == 0) {
                names[j] = current[j];
            } else {
                if (strcmp(names[j], current[j]) != 0) {
                    fail("Image names must match across sizes: %s", folder);
                }
                free(current[j]);
            }
        }
        free(folder);
    }
    return DATASET_IMAGES;
}

static int compare_times(const void *left, const void *right) {
    const double a = *(const double *)left;
    const double b = *(const double *)right;
    return (a > b) - (a < b);
}

static void measure(const Image *input, const Kernel *kernel, double *intermediate,
                    Measurement *result, int threads) {
    for (int run = -1; run < REPETITIONS; ++run) {
        int actual = 1;
        const BenchmarkTime start = run >= 0 ? benchmark_now() : 0;
        if (threads == 1) {
            blur_sequential(input, kernel, intermediate, result->pixels);
        } else {
            actual = blur_openmp(input, kernel, intermediate, result->pixels, threads);
        }
        if (run >= 0) {
            result->samples[run] = elapsed_ms(start, benchmark_now());
        }
        if (actual != threads) {
            fail("OpenMP supplied %d threads during %s, requested %d", actual,
                 run < 0 ? "warm-up" : "a timed run", threads);
        }
    }
    double sorted[REPETITIONS];
    memcpy(sorted, result->samples, sizeof(sorted));
    qsort(sorted, REPETITIONS, sizeof(sorted[0]), compare_times);
    result->median = (sorted[REPETITIONS / 2 - 1] + sorted[REPETITIONS / 2]) / 2.0;
    if (result->median <= 0.0) {
        fail("Median for %d threads is below the clock resolution", threads);
    }
}

static Errors compare_pixels(const uint8_t *sequential, const uint8_t *parallel, size_t count) {
    Errors errors = {0};
    uint64_t squared_sum = 0;
    for (size_t i = 0; i < count; ++i) {
        const int difference = abs((int)sequential[i] - (int)parallel[i]);
        if (difference != 0) {
            if (errors.different == 0) {
                errors.first_difference = i;
            }
            ++errors.different;
        }
        if (difference > errors.max_absolute) {
            errors.max_absolute = difference;
        }
        squared_sum += (uint64_t)(difference * difference);
    }
    errors.rmse = sqrt((double)squared_sum / (double)count);
    return errors;
}

static FILE *open_log(const char *folder, const char *name) {
    char *path = format_text("%s/%s", folder, name);
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        fail("Cannot create %s: %s", path, strerror(errno));
    }
    free(path);
    return file;
}

static void write_metadata(const Options *options) {
    FILE *file = open_log(options->output, "run_config.txt");
    #ifdef _WIN32
    char *absolute = _fullpath(NULL, options->data, 0);
    #else
    char *absolute = realpath(options->data, NULL);
    #endif
    fprintf(file, "data=%s\nimage=%s\nsizes=", absolute != NULL ? absolute : options->data,
            options->only_image != NULL ? options->only_image : "all");
    free(absolute);
    for (size_t i = 0; i < options->size_count; ++i) {
        fprintf(file, "%s%d", i == 0 ? "" : ",", options->sizes[i]);
    }
    fputs("\nthreads=", file);
    for (int i = 0; i < OPENMP_CONFIGS; ++i) {
        fprintf(file, "%s%d", i == 0 ? "" : ",", openmp_thread_counts[i]);
    }
    fprintf(file, "\nlogical_cpus_available=%d\nkernel_size=%d\nsigma=%.*g"
                  "\nborder=replicate\nalgorithm=separable 1D\nwarmup=1\nrepetitions=%d"
                  "\nclock=%s\nmeasurement_order=cyclic_by_image"
                  "\nopenmp_macro=%d\nopenmp_dynamic=disabled\ncompiler=%s\n",
            omp_get_num_procs(), options->kernel_size, DBL_DECIMAL_DIG, options->sigma,
            REPETITIONS, benchmark_clock_name, _OPENMP, __VERSION__);
    const char *keys[] = {"OMP_THREAD_LIMIT", "OMP_PROC_BIND", "OMP_PLACES", "OMP_WAIT_POLICY",
                          "GOMP_CPU_AFFINITY"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const char *value = getenv(keys[i]);
        fprintf(file, "%s=%s\n", keys[i], value != NULL ? value : "(unset)");
    }
    close_output(file, "run_config.txt");
}

static void write_csv_config(FILE *file, const Options *options, const char *name,
                             int size, int threads) {
    fputc('"', file);
    for (const char *ch = name; *ch != '\0'; ++ch) {
        if (*ch == '"') {
            fputc('"', file);
        }
        fputc(*ch, file);
    }
    fprintf(file, "\",%d,%d,%d,%.*g,%s,%d", size, size, options->kernel_size,
            DBL_DECIMAL_DIG, options->sigma, threads == 1 ? "seq" : "omp", threads);
}

static bool log_measurements(const Options *options, const Logs *logs, const char *name,
                             const Image *input, const Measurement results[CONFIGURATIONS]) {
    bool all_equal = true;
    const Measurement *seq = &results[0];
    for (int i = 0; i < CONFIGURATIONS; ++i) {
        const int threads = i == 0 ? 1 : openmp_thread_counts[i - 1];
        const Measurement *result = &results[i];
        for (int run = 0; run < REPETITIONS; ++run) {
            write_csv_config(logs->raw, options, name, input->width, threads);
            fprintf(logs->raw, ",%d,%.*g\n", run + 1, DBL_DECIMAL_DIG, result->samples[run]);
        }
        write_csv_config(logs->metrics, options, name, input->width, threads);
        fprintf(logs->metrics, ",%.*g", DBL_DECIMAL_DIG, result->median);
        if (i == 0) {
            fputs(",,,,\n", logs->metrics);
            continue;
        }

        const double speedup = seq->median / result->median;
        const Errors errors = compare_pixels(seq->pixels, result->pixels, input->count);
        fprintf(logs->metrics, ",%.*g,%.*g,%d,%.*g\n", DBL_DECIMAL_DIG, speedup,
                DBL_DECIMAL_DIG, speedup * 100.0 / threads, errors.max_absolute,
                DBL_DECIMAL_DIG, errors.rmse);
        if (errors.different != 0) {
            all_equal = false;
            const size_t pixel = errors.first_difference;
            write_csv_config(logs->mismatches, options, name, input->width, threads);
            fprintf(logs->mismatches, ",%zu,%zu,%zu,%d,%d\n", errors.different,
                    pixel % (size_t)input->width, pixel / (size_t)input->width,
                    seq->pixels[pixel], result->pixels[pixel]);
            fprintf(stderr, "Mismatch: %s %d threads=%d MaxAE=%d RMSE=%g\n",
                    name, input->width, threads, errors.max_absolute, errors.rmse);
        }
    }
    if (fflush(logs->raw) != 0 || fflush(logs->metrics) != 0 || fflush(logs->mismatches) != 0) {
        fail("Cannot flush benchmark CSV files");
    }
    return all_equal;
}

static void save_results(const Options *options, const char *folder, const char *name,
                         const Image *input, const Measurement results[CONFIGURATIONS], bool sample) {
    const int stem_length = (int)(strlen(name) - 4);
    for (int i = 0; i < CONFIGURATIONS; ++i) {
        char *path = i == 0 ? format_text("%s/seq/%.*s.pgm", folder, stem_length, name)
                            : format_text("%s/omp%d/%.*s.pgm", folder, openmp_thread_counts[i - 1],
                                          stem_length, name);
        write_pgm(path, input, results[i].pixels);
        free(path);
    }
    if (sample) {
        uint8_t *original = allocate_array(input->count, sizeof(uint8_t));
        for (size_t i = 0; i < input->count; ++i) {
            original[i] = (uint8_t)((double)input->pixels[i] + 0.5);
        }
        const int threads = openmp_thread_counts[OPENMP_CONFIGS - 1];
        char *stem = format_text("%s/samples/%d_%.*s", options->output, input->width,
                                 stem_length, name);
        char *source = format_text("%s_input.pgm", stem);
        char *seq = format_text("%s_seq.pgm", stem);
        char *omp = format_text("%s_omp%d.pgm", stem, threads);
        write_pgm(source, input, original);
        write_pgm(seq, input, results[0].pixels);
        write_pgm(omp, input, results[OPENMP_CONFIGS].pixels);
        free(source);
        free(seq);
        free(omp);
        free(stem);
        free(original);
    }
}

static bool benchmark(const Options *options) {
    Kernel kernel = make_kernel(options->kernel_size, options->sigma);
    char *names[DATASET_IMAGES];
    const size_t image_count = find_images(options, names);
    make_directory(options->output);
    if (options->save_images) {
        char *images = format_text("%s/images", options->output);
        char *samples = format_text("%s/samples", options->output);
        make_directory(images);
        make_directory(samples);
        free(images);
        free(samples);
    }
    write_metadata(options);
    const Logs logs = {open_log(options->output, "timings_raw.csv"),
                       open_log(options->output, "metrics.csv"),
                       open_log(options->output, "mismatches.csv")};
    fprintf(logs.raw, "%s,run,elapsed_ms\n", columns);
    fprintf(logs.metrics, "%s,median_ms,speedup,efficiency_pct,max_abs_error,rmse\n", columns);
    fprintf(logs.mismatches, "%s,different_pixels,first_x,first_y,seq_value,omp_value\n", columns);

    bool all_equal = true;
    int first_configuration = 0;
    for (size_t s = 0; s < options->size_count; ++s) {
        const int size = options->sizes[s];
        char *folder = format_text("%s/images/%d", options->output, size);
        if (options->save_images) {
            make_directory(folder);
            char *seq = format_text("%s/seq", folder);
            make_directory(seq);
            free(seq);
            for (int i = 0; i < OPENMP_CONFIGS; ++i) {
                char *omp = format_text("%s/omp%d", folder, openmp_thread_counts[i]);
                make_directory(omp);
                free(omp);
            }
        }

        const size_t count = (size_t)size * size;
        double *intermediate = allocate_array(count, sizeof(double));
        Measurement results[CONFIGURATIONS] = {0};
        for (int i = 0; i < CONFIGURATIONS; ++i) {
            results[i].pixels = allocate_array(count, sizeof(uint8_t));
        }
        for (size_t i = 0; i < image_count; ++i) {
            char *path = format_text("%s/%d/%s", options->data, size, names[i]);
            const Image input = read_image(path, size, size);
            free(path);
            for (int offset = 0; offset < CONFIGURATIONS; ++offset) {
                const int config = (first_configuration + offset) % CONFIGURATIONS;
                const int threads = config == 0 ? 1 : openmp_thread_counts[config - 1];
                measure(&input, &kernel, intermediate, &results[config], threads);
            }
            first_configuration = (first_configuration + 1) % CONFIGURATIONS;
            if (!log_measurements(options, &logs, names[i], &input, results)) {
                all_equal = false;
            }
            if (options->save_images) {
                save_results(options, folder, names[i], &input, results, i == 0);
            }
            free(input.pixels);
            printf("%dx%d %s done\n", size, size, names[i]);
            fflush(stdout);
        }
        for (int i = 0; i < CONFIGURATIONS; ++i) {
            free(results[i].pixels);
        }
        free(intermediate);
        free(folder);
    }
    close_output(logs.raw, "timings_raw.csv");
    close_output(logs.metrics, "metrics.csv");
    close_output(logs.mismatches, "mismatches.csv");
    for (size_t i = 0; i < image_count; ++i) {
        free(names[i]);
    }
    free(kernel.weights);
    return all_equal;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s processed_dir new_output_dir [kernel_size] [sigma] [image.bin]\n"
                        "       [--size 512|1024|2048] [--image image.bin] [--no-images]\n"
                        "Default kernel size: %d, sigma: %g\n",
                argv[0], DEFAULT_KERNEL_SIZE, DEFAULT_SIGMA);
        return EXIT_FAILURE;
    }
    Options options = {
        .data = argv[1],
        .output = argv[2],
        .sizes = supported_sizes,
        .size_count = sizeof(supported_sizes) / sizeof(supported_sizes[0]),
        .kernel_size = DEFAULT_KERNEL_SIZE,
        .sigma = DEFAULT_SIGMA,
        .save_images = true,
    };
    const char *positional[3];
    size_t positional_count = 0;
    int selected_size = 0;
    for (int i = 3; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--no-images") == 0) {
            options.save_images = false;
        } else if (strcmp(arg, "--size") == 0 && i + 1 < argc) {
            selected_size = parse_integer(argv[++i]);
            if (selected_size != 512 && selected_size != 1024 && selected_size != 2048) {
                fail("Size must be 512, 1024 or 2048");
            }
            options.sizes = &selected_size;
            options.size_count = 1;
        } else if (strcmp(arg, "--image") == 0 && i + 1 < argc) {
            options.only_image = argv[++i];
        } else if (strncmp(arg, "--", 2) == 0) {
            fail("Unknown or incomplete option: %s", arg);
        } else {
            if (positional_count == sizeof(positional) / sizeof(positional[0])) {
                fail("Too many positional arguments");
            }
            positional[positional_count++] = arg;
        }
    }
    if (positional_count >= 1) {
        options.kernel_size = parse_integer(positional[0]);
    }
    if (positional_count >= 2) {
        options.sigma = parse_number(positional[1]);
    }
    if (positional_count == 3) {
        if (options.only_image != NULL) {
            fail("Image specified twice");
        }
        options.only_image = positional[2];
    }
    if (options.only_image != NULL &&
        (!bin_name(options.only_image) || strpbrk(options.only_image, "/\\") != NULL)) {
        fail("Specify an image filename ending in .bin");
    }
    omp_set_dynamic(0);
    #ifdef _WIN32
    initialize_benchmark_clock();
    #endif
    if (!benchmark(&options)) {
        fputs("Outputs differ. Inspect mismatches.csv and metrics.csv.\n", stderr);
        return 2;
    }
    puts("All OpenMP outputs match Sequential.");
    return EXIT_SUCCESS;
}
