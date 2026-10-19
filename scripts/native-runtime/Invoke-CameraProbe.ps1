#Requires -Version 7.0
# X64.49: developer measurement of the camera scrolling cadence of a STAGED native candidate (never a Steam directory) at several render-rate caps.
# For each cap it normal-launches the candidate through the skirmish flow with GENERALS_CAMERA_PROBE (see Core/GameEngine/Include/Common/CameraProbe.h), which
# holds the real scroll state (arrow-key path, then screen-edge path) for a fixed time and records per-frame / per-step camera data, then collects
# camera-probe-summary.txt and camera-probe.csv into build\diag\camera-probe-<Title>-<Label>\fps<N>\ . This is a measurement; it changes nothing in the candidate.
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals', 'ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Candidate,
      [string]$Label = 'run',
      [int[]]$Fps = @(30, 60, 120, 144, 240),
      [double]$HoldSeconds = 3,
      [switch]$Legacy,                 # measure the former post-draw scroll step (before/after comparison with the same executable)
      [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
$repo = $script:ZHRepository
$manifest = Read-NativeOrdinaryPlayStage $Title $Candidate $script:NativeNormalLaunchGenerated -AllowCrashDumps
$game = $manifest.GamePath
$exe = Join-Path $game ([IO.Path]::GetFileName($manifest.Build.Executable))
$root = Join-Path $repo "build\diag\camera-probe-$Title-$Label"
New-Item -ItemType Directory -Force -Path $root | Out-Null
foreach ($cap in $Fps) {
    if (@(Get-Process -Name generals, generalsv, generalszh -ErrorAction SilentlyContinue).Count) { throw 'Exit all existing games first' }
    $out = Join-Path $root "fps$cap"
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    $names = 'GENERALS_AUTOSHOT', 'GENERALS_AUTOSHOT_FRAMES', 'GENERALS_AUTOFLOW', 'GENERALS_CAMERA_PROBE', 'GENERALS_CAMERA_PROBE_FPS', 'GENERALS_CAMERA_PROBE_HOLD', 'GENERALS_CAMERA_PROBE_LEGACY'
    $saved = @{}; foreach ($n in $names) { $saved[$n] = [Environment]::GetEnvironmentVariable($n) }
    $env:GENERALS_AUTOSHOT = $out; $env:GENERALS_AUTOSHOT_FRAMES = '1000000'; $env:GENERALS_AUTOFLOW = 'skirmish'
    $env:GENERALS_CAMERA_PROBE = $out; $env:GENERALS_CAMERA_PROBE_FPS = "$cap"; $env:GENERALS_CAMERA_PROBE_HOLD = "$HoldSeconds"; $env:GENERALS_CAMERA_PROBE_LEGACY = $(if ($Legacy) { '1' } else { '' })
    try {
        $p = Start-Process -FilePath $exe -WorkingDirectory $game -PassThru
        if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { $p.Kill(); Write-Host "cap $cap : TIMED OUT" -ForegroundColor Red }
    } finally { foreach ($n in $names) { [Environment]::SetEnvironmentVariable($n, $saved[$n]) } }
    $log = Join-Path $game 'generals64.log'
    if (Test-Path -LiteralPath $log) { Copy-Item -LiteralPath $log -Destination (Join-Path $out 'generals64.log') -Force }
    $summary = Join-Path $out 'camera-probe-summary.txt'
    Write-Host "==== $Title cap $cap ===="
    if (Test-Path -LiteralPath $summary) { Get-Content -LiteralPath $summary } else { Write-Host 'no summary written' -ForegroundColor Red }
}
