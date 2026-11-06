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
      [ValidateSet('Skirmish','Lan','Quick')][string]$Mode = 'Skirmish',   # Quick = -devMode -quickGame (a fixed, repeatable offline match; -DevFps sets its render cap)
      [string]$DevFps = '',                              # Quick mode: render cap 15..1000 or 'uncapped' (-devFps)
      [ValidateRange(20,1800)][int]$Seconds = 90,
      [ValidateRange(30,1000)][int]$RenderFps = 120,     # engine render cap (-fps); the match lasts Seconds * RenderFps presented frames
      [ValidateRange(1,7)][int]$Ai = 1,                  # LAN mode: AI opponents
      [ValidateSet('','0','1')][string]$Vsync = '',       # session-only VSync override (GENERALS_FORCE_VSYNC); empty = the stored setting
      [string]$ClientSize = '',                         # session-only client size WxH (GENERALS_FORCE_CLIENT_SIZE), e.g. 1280x800 to make the laptop render faster
      [switch]$Refresh,                                  # copy the freshly built executable into the (disposable) stage first
      [switch]$Endurance,                                # also record the X64.44 endurance telemetry (per-tick CSV, phase costs, world scale) next to it
      [switch]$PerFrame,                                 # also write every render iteration (GENERALS_FRAMEDELIVERY_FRAMES)
      [string]$Replay = '',                              # SEEDED GRAPHICAL BENCHMARK: absolute path of a generated benchmark replay (New-SimulationBenchmarkReplay.py); the normal client plays it (-loadreplay)
                                                         # (-loadreplay) and writes the authoritative fingerprint (GENERALS_SIM_FINGERPRINT) next to the telemetry; -Seconds must cover the replay plus ~30 s of start-up
      [string]$Tag = '',
      [string]$OutRoot = '')
$ErrorActionPreference = 'Stop'
# Disk hygiene: fail early instead of filling the disk (see docs/modernization/DISK_RETENTION.md).
. (Join-Path $PSScriptRoot 'AssetStore.ps1')
Assert-FreeDiskSpace (Join-Path $PSScriptRoot '..\..\build') 2 'a frame-delivery run'
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
if ($Replay) {
    if (-not [IO.Path]::IsPathRooted($Replay) -or -not (Test-Path -LiteralPath $Replay -PathType Leaf)) { throw "-Replay must be an existing absolute path: $Replay" }
    $env:GENERALS_SIM_IGNORE_CRC = '1'
    $env:GENERALS_SIM_FINGERPRINT = (Join-Path $out 'fingerprint.txt')
}
elseif ($Mode -ne 'Quick') { $env:GENERALS_AUTOFLOW = $Mode.ToLowerInvariant() }
if ($Endurance) { $env:GENERALS_ENDURANCE = $out }
if ($Vsync) { $env:GENERALS_FORCE_VSYNC = $Vsync }
if ($ClientSize) { $env:GENERALS_FORCE_CLIENT_SIZE = $ClientSize }
if ($Mode -eq 'Lan') { $env:GENERALS_AUTOFLOW_AI = "$Ai" }
$arguments = @('-noaudio', '-fps', "$RenderFps")
if ($Replay) { $arguments = @('-noaudio', '-fps', "$RenderFps", '-loadreplay', $Replay) }   # the normal client plays the replay from start-up with the shell map off (no shell-game objects in the simulation)
if ($Mode -eq 'Quick') { $arguments = @('-noaudio', '-devMode', '-quickGame'); if ($DevFps) { $arguments += @('-devFps', $DevFps) } }
$timeout = 300 + $Seconds * 3
$process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $manifest.GamePath -PassThru
try {
    if ($Replay) {
        # A replayed match ends at the end of the replay; the game then idles on its end screen at a very low frame rate and would never reach the presented-frame limit. The endurance telemetry writes
        # its session summary when the match ends: wait for that (or the normal exit / the time limit), give the files a moment to flush, then end the process.
        $deadline = [DateTime]::UtcNow.AddSeconds($timeout)
        while (-not $process.HasExited -and [DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Seconds 3
            if (@(Get-ChildItem -LiteralPath $out -Filter 'endurance-*-summary.txt' -ErrorAction SilentlyContinue).Count) { Start-Sleep -Seconds 4; break }
        }
        if (-not $process.HasExited) { $process.Kill() }
    }
    elseif (-not $process.WaitForExit($timeout * 1000)) { Write-Warning 'Timed out; the game is terminated.'; $process.Kill() }
} finally {
    foreach ($v in 'GENERALS_AUTOPLAY_REPLAY','GENERALS_SIM_IGNORE_CRC','GENERALS_SIM_FINGERPRINT','GENERALS_FRAMEDELIVERY','GENERALS_FRAMEDELIVERY_FRAMES','GENERALS_AUTOSHOT','GENERALS_AUTOSHOT_FRAMES','GENERALS_AUTOFLOW','GENERALS_AUTOFLOW_AI','GENERALS_ENDURANCE','GENERALS_FORCE_VSYNC','GENERALS_FORCE_CLIENT_SIZE') { Remove-Item "Env:$v" -ErrorAction SilentlyContinue }
}
$log = Join-Path $manifest.GamePath 'generals64.log'
if (Test-Path -LiteralPath $log) { Copy-Item -LiteralPath $log -Destination (Join-Path $out 'generals64.log') }
$csv = Get-ChildItem -LiteralPath $out -Filter 'frame-delivery-*-windows.csv' | Select-Object -First 1
if (-not $csv) { Write-Warning "No frame-delivery output in $out (match did not start?)"; return }
$rows = Import-Csv -LiteralPath $csv.FullName
Write-Host ("{0} / {1}{2}: {3} windows in {4}" -f $Title, $Mode, $Tag, $rows.Count, $out)
$rows | Select-Object window, mode, frames, render_fps, int_p50_ms, int_p95_ms, int_p99_ms, int_max_ms, hitches, tick_rate, spacing_p95_ms, spacing_max_ms, tickdur_p99_ms, alpha_valid_pct, epoch_changes, stalled_frames, net_framerate, net_selfslugs, net_resets | Format-Table -AutoSize
