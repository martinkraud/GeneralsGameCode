#Requires -Version 7.0
# Multiplayer smoothness audit: unattended frame-delivery measurement of a staged normal-launch candidate (developer aid, not a validation gate).
# Drives the real shell screens (GENERALS_AUTOFLOW=skirmish | lan) into a live match, records the frame-delivery telemetry (GENERALS_FRAMEDELIVERY) for a fixed number of
# presented frames and quits. 'lan' hosts a LAN game alone with AI opponents (m_netMinPlayers = 1), so the real TheNetwork lock-step path runs on one PC (loopback: no
# network latency, but the full scheduler / stall / presentation path). Use a disposable stage.
#
#   pwsh scripts/native-runtime/Invoke-FrameDeliveryRun.ps1 -Title ZeroHour -Name mp-audit-zh -Mode Lan -Seconds 90 -RenderFps 120 -Ai 3
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Name,
      [ValidateSet('Skirmish','Lan')][string]$Mode = 'Skirmish',
      [ValidateRange(20,1800)][int]$Seconds = 90,
      [ValidateRange(30,1000)][int]$RenderFps = 120,     # engine render cap (-fps); the match lasts Seconds * RenderFps presented frames
      [ValidateRange(1,7)][int]$Ai = 1,                  # LAN mode: AI opponents
      [ValidateSet('','0','1')][string]$Vsync = '',       # session-only VSync override (GENERALS_FORCE_VSYNC); empty = the stored setting
      [string]$ClientSize = '',                         # session-only client size WxH (GENERALS_FORCE_CLIENT_SIZE), e.g. 1280x800 to make the laptop render faster
      [switch]$Refresh,                                  # copy the freshly built executable into the (disposable) stage first
      [switch]$PerFrame,                                 # also write every render iteration (GENERALS_FRAMEDELIVERY_FRAMES)
      [string]$Tag = '',
      [string]$OutRoot = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
if (@(Get-Process -Name generals,generalsv,generalszh -ErrorAction SilentlyContinue).Count) { throw 'Exit all existing games first' }
$stage = Get-NativeStage $Title $Name
$manifest = Get-Content -LiteralPath (Join-Path $stage 'manifest.json') -Raw | ConvertFrom-Json
$exe = Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Staged executable missing: $exe" }
if ($Refresh) {
    $built = Join-Path $script:ZHRepository $(if ($Title -eq 'ZeroHour') { 'build/win64/GeneralsMD/Release/generalszh.exe' } else { 'build/win64/Generals/Release/generalsv.exe' })
    Copy-Item -LiteralPath $built -Destination $exe -Force
}
$root = if ($OutRoot) { $OutRoot } else { Join-Path $script:ZHRepository 'build/diag' }
$out = Join-Path $root ('framedelivery-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + "-$Title-$Mode$Tag")
New-Item -ItemType Directory -Force -Path $out | Out-Null
$env:GENERALS_FRAMEDELIVERY = $out
if ($PerFrame) { $env:GENERALS_FRAMEDELIVERY_FRAMES = '1' }
$env:GENERALS_AUTOSHOT = $out
$env:GENERALS_AUTOSHOT_FRAMES = "$($Seconds * $RenderFps)"
$env:GENERALS_AUTOFLOW = $Mode.ToLowerInvariant()
if ($Vsync) { $env:GENERALS_FORCE_VSYNC = $Vsync }
if ($ClientSize) { $env:GENERALS_FORCE_CLIENT_SIZE = $ClientSize }
if ($Mode -eq 'Lan') { $env:GENERALS_AUTOFLOW_AI = "$Ai" }
$arguments = @('-noaudio', '-fps', "$RenderFps")
$timeout = 300 + $Seconds * 3
$process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $manifest.GamePath -PassThru
try {
    if (-not $process.WaitForExit($timeout * 1000)) { Write-Warning 'Timed out; the game is terminated.'; $process.Kill() }
} finally {
    foreach ($v in 'GENERALS_FRAMEDELIVERY','GENERALS_FRAMEDELIVERY_FRAMES','GENERALS_AUTOSHOT','GENERALS_AUTOSHOT_FRAMES','GENERALS_AUTOFLOW','GENERALS_AUTOFLOW_AI','GENERALS_FORCE_VSYNC','GENERALS_FORCE_CLIENT_SIZE') { Remove-Item "Env:$v" -ErrorAction SilentlyContinue }
}
$log = Join-Path $manifest.GamePath 'generals64.log'
if (Test-Path -LiteralPath $log) { Copy-Item -LiteralPath $log -Destination (Join-Path $out 'generals64.log') }
$csv = Get-ChildItem -LiteralPath $out -Filter 'frame-delivery-*-windows.csv' | Select-Object -First 1
if (-not $csv) { Write-Warning "No frame-delivery output in $out (match did not start?)"; return }
$rows = Import-Csv -LiteralPath $csv.FullName
Write-Host ("{0} / {1}{2}: {3} windows in {4}" -f $Title, $Mode, $Tag, $rows.Count, $out)
$rows | Select-Object window, mode, frames, render_fps, int_p50_ms, int_p95_ms, int_p99_ms, int_max_ms, hitches, tick_rate, spacing_p95_ms, spacing_max_ms, tickdur_p99_ms, alpha_valid_pct, epoch_changes, stalled_frames, net_framerate, net_selfslugs, net_resets | Format-Table -AutoSize
