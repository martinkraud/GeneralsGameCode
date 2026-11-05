#Requires -Version 7.0
# Read-only: verifies that every asset of every staged native runtime still hashes to the SHA-256 recorded in the stage's manifest.json (run after Optimize-StageStorage.ps1 to prove that
# sharing assets by hard links changed no content). Exit code 0 = all match.
[CmdletBinding()]
param([string]$NativeRoot)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $NativeRoot) { $NativeRoot = Join-Path $repoRoot 'build\dev-runtimes\native' }
$bad = 0; $checked = 0; $stages = 0
foreach ($title in Get-ChildItem -LiteralPath $NativeRoot -Directory) {
    foreach ($stage in Get-ChildItem -LiteralPath $title.FullName -Directory) {
        $manifestPath = Join-Path $stage.FullName 'manifest.json'
        if (-not (Test-Path -LiteralPath $manifestPath)) { continue }
        $m = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        ++$stages
        foreach ($a in @($m.Assets)) {
            $path = Join-Path (Join-Path $stage.FullName 'game') $a.Path
            ++$checked
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { ++$bad; Write-Host "MISSING $($title.Name)\$($stage.Name): $($a.Path)"; continue }
            if ((Get-Item -LiteralPath $path).Length -ne $a.Length -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $a.SHA256) { ++$bad; Write-Host "MISMATCH $($title.Name)\$($stage.Name): $($a.Path)" }
        }
    }
}
Write-Host "stages with a manifest: $stages | assets checked: $checked | problems: $bad"
if ($bad) { exit 1 }
