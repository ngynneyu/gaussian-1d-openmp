#include "gaussian/gaussian.h"
#include "laplacian/laplacian_adapter.h"
#include "sobel/sobel.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif

/* Sequential baseline and row-parallel filtering within each image. */
typedef enum { SEQ, INTRA } Strategy;
typedef struct { Strategy strategy; int threads; } Mode;
static const Mode modes[] = {{SEQ,0},{INTRA,16}};
static const char *const strategies[] = {"seq", "intra"};
enum { MODE_COUNT = sizeof modes / sizeof modes[0], WORKERS = 1,
       MIN_TICKS = 10, MAX_BATCHES = 65536 };
static const double MIN_SECONDS = 0.020;

typedef enum { GAUSSIAN, SOBEL, LAPLACIAN, FILTER_COUNT } Filter;
static const char *const names[] = {"Gaussian", "Sobel", "Laplacian"};
typedef struct {
    int width;
    size_t points;
    float *pixels;
    Kernel kernel;
} Workload;
typedef struct { double *scratch; void *output; void *reference; } Worker;
typedef struct { double max_error, rmse; } Error;
typedef struct { int batches; double elapsed, seconds; } Sample;

#ifdef _WIN32
static LARGE_INTEGER frequency;
#endif
static double timer_init(void) {
#ifdef _WIN32
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
        fail("QueryPerformanceFrequency failed.");
    return 1.0 / (double)frequency.QuadPart;
#else
    return omp_get_wtick();
#endif
}
static double timer_now(void) {
#ifdef _WIN32
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter)) fail("QueryPerformanceCounter failed.");
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    return omp_get_wtime();
#endif
}

static int positive_int(const char *value) {
    char *end;
    const long number = strtol(value, &end, 10);
    if (end == value || *end || number < 1 || number >= INT_MAX)
        fail("Expected a positive integer in benchmark arguments.");
    return (int)number;
}
static FILE *open_file(const char *path, const char *mode) {
    FILE *file = fopen(path, mode);
    if (!file) { perror(path); fail("Could not open benchmark file."); }
    return file;
}
static void flush_file(FILE *file) {
    if (fflush(file) || ferror(file)) fail("Could not write benchmark results.");
}
static void close_file(FILE *file) {
    flush_file(file);
    if (fclose(file)) fail("Could not close benchmark results.");
}
static size_t pixel_bytes(Filter filter) {
    return filter == GAUSSIAN ? sizeof(uint8_t) : sizeof(float);
}
static void init_workers(Worker *workers, int n, const Workload *work, Filter filter) {
    for (int i=0; i<n; ++i) {
        workers[i].scratch = filter == GAUSSIAN ? allocate_array(work->points, sizeof(double)) : NULL;
        workers[i].output = allocate_array(work->points, pixel_bytes(filter));
        workers[i].reference = NULL;
    }
}
static void free_workers(Worker *workers, int n) {
    for (int i=0; i<n; ++i) {
        free(workers[i].scratch); free(workers[i].output); free(workers[i].reference);
    }
}

/* threads==0 means a genuinely sequential filter (not OpenMP with one thread). */
static void filter_image(const Workload *work, Filter filter, int index,
                         double *scratch, void *output, int threads) {
    const int width = work->width;
    float *pixels = work->pixels + (size_t)index * work->points;
    switch (filter) {
        case GAUSSIAN: {
            const Image image = {width, width, work->points, pixels};
            if (!threads) blur_sequential(&image, &work->kernel, scratch, output);
            else {
                const int actual = blur_openmp(&image, &work->kernel, scratch, output, threads);
                if (actual != threads) fail("Requested %d Gaussian threads, received %d.", threads, actual);
            }
            return;
        }
        case SOBEL:
            if (!threads) sobel_seq(pixels, output, width, width);
            else sobel_omp(pixels, output, width, width, threads);
            return;
        case LAPLACIAN:
            if (!threads) laplacian_seq(pixels, output, width);
            else laplacian_omp(pixels, output, width, threads);
            return;
        default: fail("Unknown image filter.");
    }
}
static double output_value(const void *output, Filter filter, size_t index) {
    return filter == GAUSSIAN ? ((const uint8_t *)output)[index] : ((const float *)output)[index];
}
static Error compare_image(const Workload *work, Filter filter, int index, Worker *worker,
                           FILE *reference) {
    /* Independent serial references reside in a temporary file. Only the
       verification pass reads it, never the timed path. */
    const size_t frame = work->points * pixel_bytes(filter);
    {
        if ((size_t)index > (size_t)LONG_MAX/frame ||
            fseek(reference, (long)((size_t)index*frame), SEEK_SET) ||
            fread(worker->reference, 1, frame, reference) != frame)
            fail("Could not read independent sequential reference.");
    }
    Error error = {0,0};
    double sum = 0;
    for (size_t j=0; j<work->points; ++j) {
        const double d = fabs(output_value(worker->reference, filter, j) -
                              output_value(worker->output, filter, j));
        if (!isfinite(d)) fail("Non-finite output in %s, image %d.", names[filter], index+1);
        if (d > error.max_error) error.max_error = d;
        sum += d*d;
    }
    error.rmse = sqrt(sum / (double)work->points);
    return error;
}
static void process_one(const Workload *work, Filter filter, int index,
                        Worker *worker, int threads, Error *errors, FILE *reference) {
    filter_image(work, filter, index, worker->scratch, worker->output, threads);
    if (errors) errors[index] = compare_image(work, filter, index, worker, reference);
}
static void process_batch(const Workload *work, Filter filter, int count,
                          Worker *workers, Mode mode, Error *errors, FILE *reference) {
    for (int i=0; i<count; ++i)
        process_one(work, filter, i, &workers[0], mode.threads, errors, reference);
}
static double run_batch(const Workload *work, Filter filter, int count,
                        Worker *workers, Mode mode, int batches) {
    const double start = timer_now();
    for (int i=0; i<batches; ++i) process_batch(work, filter, count, workers, mode, NULL, NULL);
    return timer_now() - start;
}
static Sample measure_batch(const Workload *work, Filter filter, int count,
                            Worker *workers, Mode mode, double minimum) {
    for (int batches=1; batches<=MAX_BATCHES; batches*=2) {
        const double elapsed = run_batch(work, filter, count, workers, mode, batches);
        if (!isfinite(elapsed) || elapsed < 0) fail("Invalid elapsed time.");
        if (elapsed >= minimum) return (Sample){batches, elapsed, elapsed/batches};
    }
    fail("Could not reach the minimum sample duration.");
}
static void check_team_sizes(void) {
    omp_set_dynamic(0);
    omp_set_max_active_levels(1);
    for (int m=1; m<MODE_COUNT; ++m) {
        const int threads = modes[m].threads;
        int actual = 0;
        #pragma omp parallel num_threads(threads) default(none) shared(actual, threads)
        {
            if (omp_get_thread_num() == 0) actual = omp_get_num_threads();
        }
        if (actual != threads) fail("Requested %d OpenMP threads, received %d.", threads, actual);
    }
}
static void check_outputs(const Workload *work, Filter filter, int count,
                          Worker *workers, FILE *file) {
    Error *errors = allocate_array((size_t)count, sizeof *errors);
    for (int i=0; i<WORKERS; ++i)
        workers[i].reference = allocate_array(work->points, pixel_bytes(filter));
    FILE *reference = open_file("reference.tmp", "w+b");
    const size_t frame = work->points * pixel_bytes(filter);
    if ((size_t)count > (size_t)LONG_MAX/frame) fail("Reference file too large for fseek.");
    for (int i=0; i<count; ++i) {
        filter_image(work, filter, i, workers[0].scratch, workers[0].output, 0);
        if (fwrite(workers[0].output, 1, frame, reference) != frame)
            fail("Could not write sequential reference.");
    }
    flush_file(reference);
    for (int m=1; m<MODE_COUNT; ++m) {
        const Mode mode = modes[m];
        /* Same execution strategy as the benchmark; compare each result
           before its buffer is reused. CSV writes are serial. */
        process_batch(work, filter, count, workers, mode, errors, reference);
        for (int i=0; i<count; ++i)
            fprintf(file, "%s,%d,%s,%d,%.12g,%.12g\n", names[filter], i+1,
                    strategies[mode.strategy], mode.threads, errors[i].max_error, errors[i].rmse);
    }
    if (fclose(reference) || remove("reference.tmp")) fail("Could not remove sequential reference.");
    free(errors);
    flush_file(file);
}
static void export_outputs(const Workload *work, int count, int selected) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) fail("Could not open binary stream.");
#endif
    for (Filter f=GAUSSIAN; f<FILTER_COUNT; ++f) {
        if (selected >= 0 && (int)f != selected) continue;
        Worker worker;
        init_workers(&worker, 1, work, f);
        for (int i=0; i<count; ++i) {
            filter_image(work, f, i, worker.scratch, worker.output, 0);
            if (fwrite(worker.output, pixel_bytes(f), work->points, stdout) != work->points)
                fail("Could not stream filter output.");
        }
        free_workers(&worker, 1);
    }
    flush_file(stdout);
}

int main(int argc, char **argv) {
    _Static_assert(sizeof(float)==4 && FLT_RADIX==2 && FLT_MANT_DIG==24, "Requires binary32");
    const uint16_t endian = 1;
    if (*(const uint8_t *)&endian != 1) fail("Requires little-endian input.");
    if (argc<6) fail("Usage: benchmark DATASET SIDE REPEATS|--export all|Gaussian|Sobel|Laplacian COUNT [COUNT ...]");
    const int width = positive_int(argv[2]);
    const int exporting = strcmp(argv[3], "--export")==0;
    if (exporting && argc!=6) fail("Export accepts one count.");
    const int repeats = exporting ? 0 : positive_int(argv[3]);
    int selected = -1;
    if (strcmp(argv[4], "all")) {
        for (int f=0; f<FILTER_COUNT; ++f) if (!strcmp(argv[4], names[f])) selected=f;
        if (selected<0) fail("Unknown filter selection.");
    }
    const int ncounts = argc-5;
    int *counts = allocate_array((size_t)ncounts, sizeof *counts);
    for (int i=0; i<ncounts; ++i) {
        counts[i] = positive_int(argv[i+5]);
        if (i && counts[i]<=counts[i-1]) fail("Counts must be strictly increasing.");
    }
    const int max_images = counts[ncounts-1];
    if ((size_t)width > SIZE_MAX/(size_t)width) fail("Image too large.");
    const size_t points=(size_t)width*(size_t)width;
    if (points>SIZE_MAX/(size_t)max_images) fail("Dataset too large.");
    const size_t values=points*(size_t)max_images;
    float *pixels=allocate_array(values,sizeof *pixels);
    FILE *file=open_file(argv[1],"rb");
    if (fread(pixels,sizeof *pixels,values,file)!=values || fgetc(file)!=EOF || ferror(file))
        fail("Unexpected byte count in prepared dataset.");
    if (fclose(file)) fail("Could not close dataset.");
    for (size_t i=0; i<values; ++i)
        if (!isfinite(pixels[i]) || pixels[i]<0 || pixels[i]>255) fail("Invalid grayscale input.");
    const Workload work={width,points,pixels,make_kernel(DEFAULT_KERNEL_SIZE,DEFAULT_SIGMA)};
    if (exporting) export_outputs(&work,max_images,selected);
    else {
        const double tick=timer_init();
        if (!isfinite(tick) || tick<=0) fail("Invalid timer resolution.");
        const double minimum=fmax(MIN_SECONDS,MIN_TICKS*tick);
        (void)timer_now();
        check_team_sizes();
        FILE *metadata=open_file("timer_info.csv","w");
        fprintf(metadata,"timer_tick_seconds,minimum_sample_seconds\n%.17g,%.17g\n",tick,minimum);
        close_file(metadata);
        FILE *timings=open_file("timings.csv","w");
        FILE *errors=open_file("errors_per_image.csv","w");
        fputs("algorithm,image_count,repeat,strategy,threads,batches_per_sample,elapsed_seconds,total_seconds\n",timings);
        fputs("algorithm,image_index,strategy,threads,max_abs_error,rmse\n",errors);
        for (Filter f=GAUSSIAN; f<FILTER_COUNT; ++f) {
            if (selected>=0 && (int)f!=selected) continue;
            Worker workers[WORKERS];
            init_workers(workers,WORKERS,&work,f);
            /* Warm all configurations once for each nested prefix. */
            for (int c=0;c<ncounts;++c)
                for (int m=0;m<MODE_COUNT;++m) process_batch(&work,f,counts[c],workers,modes[m],NULL,NULL);
            /* Rotate dataset sizes as well as strategy order each round. */
            for (int r=0;r<repeats;++r) {
                for (int offset=0;offset<ncounts;++offset) {
                    const int c=(r+offset)%ncounts;
                    for (int j=0;j<MODE_COUNT;++j) {
                        const Mode mode=modes[(r+c+j)%MODE_COUNT];
                        const Sample s=measure_batch(&work,f,counts[c],workers,mode,minimum);
                        fprintf(timings,"%s,%d,%d,%s,%d,%d,%.17g,%.17g\n",names[f],counts[c],r+1,
                                strategies[mode.strategy],mode.threads,s.batches,s.elapsed,s.seconds);
                        flush_file(timings);
                    }
                }
                printf("%s: measured round %d/%d\n",names[f],r+1,repeats); fflush(stdout);
            }
            check_outputs(&work,f,max_images,workers,errors);
            free_workers(workers,WORKERS);
        }
        close_file(timings);close_file(errors);
    }
    free(work.kernel.weights);free(pixels);free(counts);
    return EXIT_SUCCESS;
}
