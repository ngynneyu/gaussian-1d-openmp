#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p build

compiler="${CXX:-g++}"
flags=(-std=c++17 -O3 -DNDEBUG -ffp-contract=off -Wall -Wextra -Wpedantic)
shared=../gaussian

"$compiler" "${flags[@]}" -c gaussian_1d.cpp -o build/gaussian_1d.o
"$compiler" "${flags[@]}" -c gaussian_seq.cpp -o build/gaussian_seq.o
"$compiler" "${flags[@]}" -DGAUSSIAN_SEPARABLE "$shared/filter_main.cpp" "$shared/image_io.cpp" \
    build/gaussian_1d.o build/gaussian_seq.o -o build/gaussian_seq
"$compiler" "${flags[@]}" -DGAUSSIAN_SEPARABLE -fopenmp -DGAUSSIAN_USE_OPENMP \
    "$shared/filter_main.cpp" "$shared/image_io.cpp" "$shared/openmp_runtime.cpp" \
    gaussian_omp.cpp build/gaussian_1d.o -o build/gaussian_omp
"$compiler" "${flags[@]}" -DGAUSSIAN_SEPARABLE -fopenmp \
    "$shared/benchmark.cpp" "$shared/image_io.cpp" "$shared/openmp_runtime.cpp" \
    gaussian_omp.cpp build/gaussian_1d.o build/gaussian_seq.o -o build/gaussian_bench
