param([string]$Compiler = 'g++')

$ErrorActionPreference = 'Stop'
$compilerPath = (Get-Command $Compiler -CommandType Application -ErrorAction Stop).Source
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force build | Out-Null
    $common = @('-std=c++17', '-O3', '-DNDEBUG', '-ffp-contract=off', '-Wall', '-Wextra', '-Wpedantic')
    $shared = '../gaussian'
    $ioSources = @("$shared/image_io.cpp")
    $ompSources = @("$shared/openmp_runtime.cpp", 'gaussian_omp.cpp')

    & $compilerPath @common -c gaussian_1d.cpp -o build/gaussian_1d_win.o
    if ($LASTEXITCODE -ne 0) { throw 'Convolution object build failed' }

    & $compilerPath @common -c gaussian_seq.cpp -o build/gaussian_seq_win.o
    if ($LASTEXITCODE -ne 0) { throw 'Sequential object build failed' }

    & $compilerPath @common -DGAUSSIAN_SEPARABLE "$shared/filter_main.cpp" @ioSources build/gaussian_1d_win.o build/gaussian_seq_win.o -o build/gaussian_seq.exe
    if ($LASTEXITCODE -ne 0) { throw 'Sequential build failed' }

    & $compilerPath @common -DGAUSSIAN_SEPARABLE -DGAUSSIAN_USE_OPENMP -fopenmp "$shared/filter_main.cpp" @ioSources @ompSources build/gaussian_1d_win.o -o build/gaussian_omp.exe
    if ($LASTEXITCODE -ne 0) { throw 'OpenMP build failed' }

    & $compilerPath @common -DGAUSSIAN_SEPARABLE -fopenmp "$shared/benchmark.cpp" @ioSources @ompSources build/gaussian_1d_win.o build/gaussian_seq_win.o -o build/gaussian_bench.exe
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark build failed' }
} finally {
    Pop-Location
}
