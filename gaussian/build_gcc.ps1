param([string]$Compiler = 'g++')

$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force build | Out-Null
    $common = @('-std=c++17', '-O3', '-DNDEBUG', '-ffp-contract=off', '-Wall', '-Wextra', '-Wpedantic')
    $sharedSources = @('image_io.cpp', 'gaussian.cpp')

    & $Compiler @common filter_main.cpp @sharedSources gaussian_seq.cpp -o build/gaussian_seq.exe
    if ($LASTEXITCODE -ne 0) { throw 'Sequential build failed' }

    & $Compiler @common -fopenmp -DGAUSSIAN_USE_OPENMP filter_main.cpp @sharedSources gaussian_omp.cpp openmp_runtime.cpp -o build/gaussian_omp.exe
    if ($LASTEXITCODE -ne 0) { throw 'OpenMP build failed' }

    # Compile the sequential baseline and shared convolution without OpenMP.
    & $Compiler @common -c gaussian_seq.cpp -o build/gaussian_seq_win.o
    if ($LASTEXITCODE -ne 0) { throw 'Sequential object build failed' }
    & $Compiler @common -c gaussian.cpp -o build/gaussian_win.o
    if ($LASTEXITCODE -ne 0) { throw 'Convolution object build failed' }
    & $Compiler @common -fopenmp benchmark.cpp image_io.cpp gaussian_omp.cpp openmp_runtime.cpp build/gaussian_seq_win.o build/gaussian_win.o -o build/gaussian_bench.exe
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark build failed' }
} finally {
    Pop-Location
}
