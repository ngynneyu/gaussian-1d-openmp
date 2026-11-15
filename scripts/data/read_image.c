#include <stdio.h>
#include <stdlib.h>

static float* load_image(const char* filename, int size) {
    const size_t count = (size_t)size * size;
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror(filename);
        return NULL;
    }

    float* image = malloc(count * sizeof(*image));
    if (!image) {
        fputs("Memory allocation failed\n", stderr);
        fclose(file);
        return NULL;
    }

    const size_t read_count = fread(image, sizeof(*image), count, file);
    const int extra = fgetc(file);
    const int read_error = ferror(file);
    fclose(file);
    if (read_count != count || extra != EOF || read_error) {
        fprintf(stderr, "Expected exactly %zu float32 pixels in %s\n", count, filename);
        free(image);
        return NULL;
    }
    return image;
}

int main(int argc, char** argv) {
    const char* filename = "data/processed/512/image_01.bin";
    int size = 512;
    if (argc == 3) {
        char* end;
        const long parsed = strtol(argv[2], &end, 10);
        if (end == argv[2] || *end != '\0' || parsed < 1 || parsed > 16384) {
            fputs("SIZE must be an integer between 1 and 16384\n", stderr);
            return EXIT_FAILURE;
        }
        filename = argv[1];
        size = (int)parsed;
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [INPUT.bin SIZE]\n", argv[0]);
        return EXIT_FAILURE;
    }

    float* image = load_image(filename, size);
    if (!image) {
        return EXIT_FAILURE;
    }

    const int x = size > 200 ? 100 : size / 2;
    const int y = size > 200 ? 200 : size / 2;
    printf("Dimensions: %d x %d\n", size, size);
    printf("Pixel (%d, %d): %.2f\n", x, y, image[(size_t)y * size + x]);
    free(image);
    return EXIT_SUCCESS;
}
