param(
    [string]$Compiler,
    [string]$Python = 'python',
    [switch]$Images
)

$ErrorActionPreference = 'Stop'

if (-not $Compiler) {
    $gcc = Get-Command gcc.exe -CommandType Application -ErrorAction SilentlyContinue
    if ($gcc) {
        $Compiler = $gcc.Source
    } else {
        $localGcc = Join-Path $PSScriptRoot '..\cki\gaussian\build\toolchain\w64devkit\bin\gcc.exe'
        if (-not (Test-Path -LiteralPath $localGcc -PathType Leaf)) {
            throw 'GCC was not found. Install GCC with OpenMP or pass -Compiler with the path to gcc.exe.'
        }
        $Compiler = (Resolve-Path -LiteralPath $localGcc).Path
    }
}

Push-Location -LiteralPath $PSScriptRoot
try {
    $out = "results/windows_$(Get-Date -Format 'yyyyMMdd_HHmmss_fffffff')"
    $benchmarkArgs = @('benchmark/run.py', '--cc', $Compiler, '--out', $out)
    if ($Images) {
        $benchmarkArgs += '--images'
    }
    & $Python @benchmarkArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Benchmark failed (exit code $LASTEXITCODE)."
    }

    & $Python benchmark/summarize.py $out
    if ($LASTEXITCODE -ne 0) {
        throw "Summary failed (exit code $LASTEXITCODE)."
    }

    Write-Output "Results: $((Resolve-Path -LiteralPath $out).Path)"
} finally {
    Pop-Location
}
