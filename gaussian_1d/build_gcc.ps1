param([string]$Compiler = 'gcc')

$ErrorActionPreference = 'Stop'
$compilerPath = (Get-Command $Compiler -CommandType Application -ErrorAction Stop).Source
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force build | Out-Null
    $common = @('-std=c11', '-D_POSIX_C_SOURCE=200809L', '-O3', '-DNDEBUG', '-ffp-contract=off', '-Wall', '-Wextra', '-Wpedantic')

    & $compilerPath @common -c src/gaussian.c -o build/gaussian_win.o
    if ($LASTEXITCODE -ne 0) { throw 'Convolution object build failed' }

    & $compilerPath @common src/filter_main.c src/image_io.c build/gaussian_win.o -o build/gaussian_seq.exe -lm
    if ($LASTEXITCODE -ne 0) { throw 'Sequential build failed' }

    & $compilerPath @common -DGAUSSIAN_USE_OPENMP -fopenmp src/filter_main.c src/image_io.c src/gaussian_omp.c build/gaussian_win.o -o build/gaussian_omp.exe -lm
    if ($LASTEXITCODE -ne 0) { throw 'OpenMP build failed' }

    & $compilerPath @common -fopenmp src/benchmark.c src/image_io.c src/gaussian_omp.c build/gaussian_win.o -o build/gaussian_bench.exe -lm
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark build failed' }
} finally {
    Pop-Location
}
