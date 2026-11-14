#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p build

compiler="${CXX:-g++}"
flags=(-std=c++17 -O3 -DNDEBUG -ffp-contract=off -Wall -Wextra -Wpedantic)

"$compiler" "${flags[@]}" -c gaussian_1d.cpp -o build/gaussian_1d.o
"$compiler" "${flags[@]}" -c gaussian_seq.cpp -o build/gaussian_seq.o
"$compiler" "${flags[@]}" filter_main.cpp image_io.cpp \
    build/gaussian_1d.o build/gaussian_seq.o -o build/gaussian_seq
"$compiler" "${flags[@]}" -fopenmp -DGAUSSIAN_USE_OPENMP \
    filter_main.cpp image_io.cpp openmp_runtime.cpp \
    gaussian_omp.cpp build/gaussian_1d.o -o build/gaussian_omp
"$compiler" "${flags[@]}" -fopenmp \
    benchmark.cpp image_io.cpp openmp_runtime.cpp \
    gaussian_omp.cpp build/gaussian_1d.o build/gaussian_seq.o -o build/gaussian_bench
