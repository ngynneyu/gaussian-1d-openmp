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
