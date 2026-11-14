#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p build

compiler="${CC:-gcc}"
flags=(-std=c11 -D_XOPEN_SOURCE=700 -D_FILE_OFFSET_BITS=64 -O3 -DNDEBUG -ffp-contract=off -Wall -Wextra -Wpedantic)

"$compiler" "${flags[@]}" -c src/gaussian.c -o build/gaussian.o
"$compiler" "${flags[@]}" src/filter_main.c src/image_io.c \
    build/gaussian.o -o build/gaussian_seq -lm
"$compiler" "${flags[@]}" -fopenmp -DGAUSSIAN_USE_OPENMP \
    src/filter_main.c src/image_io.c src/gaussian_omp.c build/gaussian.o -o build/gaussian_omp -lm
"$compiler" "${flags[@]}" -fopenmp \
    src/benchmark.c src/image_io.c src/gaussian_omp.c build/gaussian.o -o build/gaussian_bench -lm
