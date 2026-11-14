[CmdletBinding()]
param(
    [string]$DatasetDir = (Join-Path $PSScriptRoot '../data/processed_raw_names'),
    [string]$ResultsDir = (Join-Path $PSScriptRoot 'results/windows_full'),
    [string]$Python,
    [string]$Compiler,
    [switch]$NoImages
)

$ErrorActionPreference = 'Stop'
$dataset = (Resolve-Path -LiteralPath $DatasetDir).ProviderPath
$benchmark = Join-Path $PSScriptRoot 'build/gaussian_bench.exe'
$windowsVenv = Join-Path $PSScriptRoot '../.venv-windows/Scripts/python.exe'
if (-not $Python) {
    $Python = if (Test-Path -LiteralPath $windowsVenv -PathType Leaf) { $windowsVenv } else { 'python' }
}

if (-not $NoImages) {
    & $Python -X utf8 -c 'import PIL'
    if ($LASTEXITCODE -ne 0) { throw 'PNG export requires Pillow. Install it with python -m pip install pillow.' }
}

if ($Compiler -or -not (Test-Path -LiteralPath $benchmark -PathType Leaf)) {
    $build = Join-Path $PSScriptRoot 'build_gcc.ps1'
    if ($Compiler) {
        & $build -Compiler $Compiler
    } else {
        & $build
    }
}

$results = (New-Item -ItemType Directory -Force -Path $ResultsDir).FullName
$output = Join-Path $results ('run_' + (Get-Date -Format yyyyMMdd_HHmmss_fff))
$benchmarkArgs = @($dataset, $output)
if ($NoImages) { $benchmarkArgs += '--no-images' }

Write-Host "Windows results: $output"
& $benchmark @benchmarkArgs
if ($LASTEXITCODE -ne 0) { throw "Windows benchmark failed with exit code $LASTEXITCODE." }

& $Python -X utf8 (Join-Path $PSScriptRoot 'scripts/summarize_results.py') $output
if ($LASTEXITCODE -ne 0) { throw 'CSV summarization failed.' }

if (-not $NoImages) {
    & $Python -X utf8 (Join-Path $PSScriptRoot 'scripts/export_png.py') $output --dataset $dataset
    if ($LASTEXITCODE -ne 0) { throw 'PNG export failed.' }
}

Write-Host "Summary: $(Join-Path $output 'summary.csv')"
Invoke-Item -LiteralPath $output
