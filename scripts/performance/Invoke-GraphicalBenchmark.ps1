#Requires -Version 7.0
<#
.SYNOPSIS
  SEEDED GRAPHICAL BENCHMARK: plays a generated, seeded AI-vs-AI benchmark replay in the NORMAL (rendering) client and records simulation, render and hitch telemetry plus the authoritative fingerprint.

.DESCRIPTION
  The same executable, scenario and settings always simulate the same game (map, players, AI mode, starting cash and RNG seed are written into the replay; there is no human input and the camera is the
  replay's start camera), so before/after comparisons no longer depend on a random skirmish. Repeatability is proven by the fingerprint: the run writes one "frame crc objects" line every 30 logic frames
  and the summary compares it with the headless run of the same replay (identical lines = the graphical client simulated exactly what the headless bench simulates).

  Steps: (1) generate build\bench\<Title>-<Scenario>-<AiMode><Cash>-<Minutes>m.rep once (small file, derived from your own template replay), (2) Invoke-FrameDeliveryRun.ps1 -Replay ... -Endurance on a disposable
  stage, (3) Summarize-GraphicalBenchmark.py writes summary.json, (4) the bulky raw output (screenshots, per-tick and per-frame CSVs) is deleted unless -KeepRaw; summary.json and fingerprint.txt stay.
  Optional diagnostic unit multiplier: set GENERALS_SIM_CLONE=<k>@<frame> in the calling shell (see ENGINE_SCALABILITY_AUDIT.md).

.EXAMPLE
  pwsh scripts\performance\Invoke-GraphicalBenchmark.ps1 -Title ZeroHour -Stage mp-audit-zh -Label base
#>
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Stage,                 # disposable stage name under build\dev-runtimes\native\<Title>
      [ValidateSet('twilight4H')][string]$Scenario = 'twilight4H',
      [ValidateRange(2,20)][double]$Minutes = 9,
      [ValidateSet('O','E')][string]$AiMode = 'E',
      [int]$Cash = 50000,
      [ValidateSet(75,100,125,150)][int]$GameSpeed = 100,  # Game Speed rule written into the replay (GS=N); 100 = Normal = nothing written
      [int]$HeaderFps = 0,                                  # render cap written into the replay header (0 = the template's 120)
      [string]$Label = 'run',
      [int]$RenderFps = 120,                                # replay playback renders at the engine's 120 Hz cap whatever -fps says, and -Seconds is counted in presented frames
      [ValidateSet('0','1')][string]$Vsync = '0',          # VSync off by default: no dependence on the display's refresh state (an idle / locked display stalls Present to ~4 Hz with VSync on)
      [int]$FromFrame = 0,                                  # statistics window start (logic frame)
      [switch]$Refresh,
      [switch]$KeepRaw)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $repo 'scripts\native-runtime\AssetStore.ps1')
Assert-FreeDiskSpace (Join-Path $repo 'build') 3 'a graphical benchmark'
$scenarios = @{ twilight4H = @{ Map = 'Twilight Flame'; Size = 402471; Ai = 4; Level = 'H'; Seed = 6106 } }
$s = $scenarios[$Scenario]
$template = Join-Path $repo ('build\x64-51\templates\' + $(if ($Title -eq 'Generals') { 'gen-template.rep' } else { 'zh-template.rep' }))
if (-not (Test-Path -LiteralPath $template)) { throw "Missing replay template: $template" }
$benchDir = Join-Path $repo 'build\bench'
New-Item -ItemType Directory -Force -Path $benchDir | Out-Null
$suffix = $(if ($GameSpeed -ne 100) { "-gs$GameSpeed" } else { '' }) + $(if ($HeaderFps) { "-fps$HeaderFps" } else { '' })
$replay = Join-Path $benchDir ('{0}-{1}-{2}{3}-{4}m{5}.rep' -f $Title, $Scenario, $AiMode, $Cash, $Minutes, $suffix)
if (-not (Test-Path -LiteralPath $replay)) {
    & python (Join-Path $repo 'scripts\performance\New-SimulationBenchmarkReplay.py') --template $template --out $replay --map $s.Map --map-size $s.Size --ai $s.Ai --level $s.Level --minutes $Minutes --seed $s.Seed --cash $Cash --ai-mode $AiMode --game-speed $GameSpeed --fps $HeaderFps
    if ($LASTEXITCODE) { throw 'replay generation failed' }
}
$seconds = [int][math]::Ceiling($Minutes * 60 + 40)
$tag = "-gb-$Label"
$args2 = @('-Title', $Title, '-Name', $Stage, '-Mode', 'Skirmish', '-Seconds', "$seconds", '-RenderFps', "$RenderFps", '-Endurance', '-Replay', $replay, '-Tag', $tag)
if ($Refresh) { $args2 += '-Refresh' }
$args2 += @('-Vsync', $Vsync)
& pwsh -NoProfile -File (Join-Path $repo 'scripts\native-runtime\Invoke-FrameDeliveryRun.ps1') @args2 | Out-Host
$run = Get-ChildItem (Join-Path $repo 'build\diag') -Directory -Filter "framedelivery-*$tag" | Sort-Object LastWriteTime | Select-Object -Last 1
if (-not $run) { throw 'no run directory was produced' }
Get-Process -Name generalszh, generalsv -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "*$Stage*" } | Stop-Process -Force -ErrorAction SilentlyContinue
# The same replay, headless, with the same executable: its fingerprint must equal the graphical one (and gives the headless tick distribution for the headless-versus-graphical comparison).
$manifest = Get-Content -LiteralPath (Join-Path $repo "build\dev-runtimes\native\$Title\$Stage\manifest.json") -Raw | ConvertFrom-Json
$exe = Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))
$hb = Join-Path $run.FullName 'headless'
$env:GENERALS_SIM_IGNORE_CRC = '1'; $env:GENERALS_SIM_FP_INTERVAL = '30'; $env:GENERALS_SIM_BENCH = $hb
Push-Location $manifest.GamePath
try { cmd /c "`"$exe`" -headless -replay `"$replay`" > `"$hb.stdout.txt`" 2>&1" } finally { Pop-Location; foreach ($v in 'GENERALS_SIM_IGNORE_CRC','GENERALS_SIM_FP_INTERVAL','GENERALS_SIM_BENCH') { Remove-Item "Env:$v" -ErrorAction SilentlyContinue } }
$headlessFp = "${hb}_fingerprint.txt"
$summary = Join-Path $run.FullName 'summary.json'
$sumArgs = @($run.FullName, '--from-frame', "$FromFrame", '--out', $summary)
if (Test-Path -LiteralPath $headlessFp) { $sumArgs += @('--headless-fingerprint', $headlessFp) }
& python (Join-Path $repo 'scripts\performance\Summarize-GraphicalBenchmark.py') @sumArgs | Out-Host
if (-not $KeepRaw) {
    Get-ChildItem $run.FullName -File | Where-Object { $_.Name -notin 'summary.json', 'fingerprint.txt', 'generals64.log', 'headless_summary.txt', 'headless_fingerprint.txt', 'headless_ticks.csv' } | Remove-Item -Force
    Write-Host "raw output removed; kept summary.json, fingerprint.txt and generals64.log in $($run.FullName)"
}
Write-Host "RUN_DIR=$($run.FullName)"
