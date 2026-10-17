#Requires -Version 7.0
# X64.44: starts the staged candidate exactly like a player would (Invoke-NativeNormalLaunch.ps1) with the endurance telemetry switched on.
# Telemetry goes to build\endurance-telemetry\<name>-<stamp> (outside the staged candidate, so the stage integrity check is unaffected).
# The game process is never touched by this script; ending the run is done in the game (Exit Game) or by closing it.
[CmdletBinding()]
param([string]$Name = 'x64-44-endurance-v1',
      [ValidateRange(1,4)][int]$MaxHours = 4)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
$out = Join-Path $repo "build\endurance-telemetry\$Name-$stamp"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$env:GENERALS_ENDURANCE = $out
Write-Host "Telemetry directory: $out"
Write-Host "Candidate          : $Name (Generals, normal launch). The run is capped at $MaxHours h; end it with Exit Game."
try {
    & (Join-Path $PSScriptRoot 'Invoke-NativeNormalLaunch.ps1') -Title Generals -Name $Name -RunNormalLaunch -TimeoutSeconds ($MaxHours * 3600)
} finally {
    $env:GENERALS_ENDURANCE = $null
    Write-Host ''
    Write-Host "Telemetry files in $out :"
    Get-ChildItem -LiteralPath $out -File | ForEach-Object { '  {0}  {1:N0} bytes' -f $_.Name, $_.Length }
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python -and (Test-Path -LiteralPath (Join-Path $out '*-meta.txt'))) {
        Write-Host ''
        & python -I (Join-Path $repo 'scripts\diagnostics\Analyze-EnduranceTelemetry.py') $out | Select-Object -First 60
    }
}
