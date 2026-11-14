#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p build

compiler="${CXX:-g++}"
flags=(-std=c++17 -O3 -DNDEBUG -ffp-contract=off -Wall -Wextra -Wpedantic)

"$compiler" "${flags[@]}" -c src/gaussian_1d.cpp -o build/gaussian_1d.o
"$compiler" "${flags[@]}" -c src/gaussian_seq.cpp -o build/gaussian_seq.o
"$compiler" "${flags[@]}" src/filter_main.cpp src/image_io.cpp \
    build/gaussian_1d.o build/gaussian_seq.o -o build/gaussian_seq
"$compiler" "${flags[@]}" -fopenmp -DGAUSSIAN_USE_OPENMP \
    src/filter_main.cpp src/image_io.cpp src/openmp_runtime.cpp \
    src/gaussian_omp.cpp build/gaussian_1d.o -o build/gaussian_omp
"$compiler" "${flags[@]}" -fopenmp \
    src/benchmark.cpp src/image_io.cpp src/openmp_runtime.cpp \
    src/gaussian_omp.cpp build/gaussian_1d.o build/gaussian_seq.o -o build/gaussian_bench
