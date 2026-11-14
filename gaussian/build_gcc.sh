#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p build

compiler="${CXX:-g++}"
flags=(-std=c++17 -O3 -DNDEBUG -ffp-contract=off -Wall -Wextra -Wpedantic)

"$compiler" "${flags[@]}" filter_main.cpp image_io.cpp gaussian.cpp gaussian_seq.cpp -o build/gaussian_seq
"$compiler" "${flags[@]}" -fopenmp -DGAUSSIAN_USE_OPENMP filter_main.cpp image_io.cpp gaussian.cpp gaussian_omp.cpp openmp_runtime.cpp -o build/gaussian_omp

# Compile the sequential baseline and shared convolution without OpenMP.
"$compiler" "${flags[@]}" -c gaussian_seq.cpp -o build/gaussian_seq.o
"$compiler" "${flags[@]}" -c gaussian.cpp -o build/gaussian.o
"$compiler" "${flags[@]}" -fopenmp benchmark.cpp image_io.cpp gaussian_omp.cpp openmp_runtime.cpp build/gaussian_seq.o build/gaussian.o -o build/gaussian_bench
