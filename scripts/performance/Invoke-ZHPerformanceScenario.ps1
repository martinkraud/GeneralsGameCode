#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$Name = 'stage4a3-performance-harness',
    [Parameter(Mandatory)][string]$OutputRoot,
    [ValidateRange(1, 10)][int]$Trials = 3,
    [switch]$Build,
    [string]$StageSource,
    [switch]$ValidateOnly
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. "$repo/scripts/zh-runtime/Common.ps1"
Assert-ZHNoGameProcess
$output = Get-ZHPath $OutputRoot
$performanceRoot = Join-Path $repo 'build/performance'
if (-not (Test-ZHWithin $output $performanceRoot) -or $output -eq $performanceRoot) { throw 'OutputRoot must be a fresh child of repository build/performance.' }
Assert-ZHNoLinks $output
if (Test-Path -LiteralPath $output) { throw 'OutputRoot exists; choose a fresh name to preserve earlier trials.' }
if ($Build -and -not $ValidateOnly) {
    # Run this script from a Visual Studio x86 developer shell when requesting a build.
    & cmake --build "$repo/build/win32" --config Release
    if ($LASTEXITCODE) { throw 'Release build failed.' }
}
if ($StageSource) {
    & pwsh -NoProfile -File "$repo/scripts/zh-runtime/Stage-ZHRuntime.ps1" -SourcePath $StageSource -Name $Name -ValidateOnly:$ValidateOnly
    if ($LASTEXITCODE) { throw 'Runtime staging failed.' }
}
if (-not $ValidateOnly) { New-Item -ItemType Directory -Path $output | Out-Null }
for ($trial = 1; $trial -le $Trials; $trial++) {
    $directory = Join-Path $output ('trial-' + $trial)
    if (-not $ValidateOnly) { New-Item -ItemType Directory -Path $directory | Out-Null }
    Write-Host "Trial $trial/$Trials -> $directory"
    & pwsh -NoProfile -File "$PSScriptRoot/Invoke-ZHPerformanceTrial.ps1" -Name $Name -Directory $directory -ValidateOnly:$ValidateOnly
    if ($LASTEXITCODE) { throw "Guarded trial $trial failed." }
    if (-not $ValidateOnly) {
        & python -B "$PSScriptRoot/compare.py" --validate $directory
        if ($LASTEXITCODE) { throw "Trial $trial reports failed validation; preserve them for review." }
        & python -B "$PSScriptRoot/compare.py" --qualify $directory
        if ($LASTEXITCODE) { throw "Trial $trial failed workload qualification; preserve reports and stop for review." }
    }
}
if ($ValidateOnly) { Write-Host 'Validation only: no build, stage copy, output directory, backup or game launch.' }
