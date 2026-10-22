#Requires -Version 7.0
# X64.51: the golden AI-behaviour gate. Runs deterministic, self-playing AI-versus-AI headless simulations (New-SimulationBenchmarkReplay.py: an idle human slot plus
# computer slots; playback IS the simulation) of both titles with one or more executables and both AI Behaviour modes, and records per run the authoritative
# determinism fingerprint (GameLogic CRC + object count every 30 frames, FNV-1a digest) and the tick / phase timing.
#   Original gate : the pre-X64.51 executable and the X64.51 executable must produce IDENTICAL digests for every scenario in Original mode.
#   Repeatability : every Enhanced scenario is run twice; both digests must be identical.
#   Effect        : Enhanced digests are reported next to Original ones (they may differ only where a repaired EA system acts).
# Nothing here touches a Steam directory or user data: -GameDir is a disposable stage (build\dev-runtimes\native\<Title>\<stage>\game); an extra executable is
# copied next to its files under a distinct name.
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals', 'ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$GameDir,
      [Parameter(Mandatory)][string]$Template,
      [Parameter(Mandatory)][string]$Out,
      [Parameter(Mandatory)][string[]]$Exe,            # label=path (the first is the reference), e.g. base=build\baseline-x64-50\generalsv.exe new=build\win64\Generals\Release\generalsv.exe
      [string[]]$Modes = @('O', 'E'),                  # AI Behaviour modes to run for the LAST executable; earlier executables only run Original (they do not know the rule)
      [int]$Runs = 2,
      [switch]$Quick,
      [string[]]$Only)
$ErrorActionPreference = 'Stop'
# `pwsh -File` passes one string for an array parameter: accept comma separated values (paths must not contain commas)
$Exe = @($Exe | ForEach-Object { $_ -split ',' } | Where-Object { $_ }); $Modes = @($Modes | ForEach-Object { $_ -split ',' } | Where-Object { $_ }); $Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gen = Join-Path $repo 'scripts\performance\New-SimulationBenchmarkReplay.py'
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$minutesScale = if ($Quick) { 0.25 } else { 1.0 }

# name, map, map size (bytes of the .map in the installation), AI slots, level, minutes, seed, factions (PlayerTemplate indices)
$scenarios = if ($Title -eq 'Generals') { @(
    @{ N = 'G-desert3H'; Map = 'Tournament Desert'; Size = 143638; Ai = 3; Level = 'H'; Min = 15; Seed = 51001; F = '' },
    @{ N = 'G-twilight7H'; Map = 'Twilight Flame'; Size = 402471; Ai = 7; Level = 'H'; Min = 20; Seed = 51002; F = '' },
    @{ N = 'G-wasteland5M'; Map = 'Wasteland Warlords'; Size = 305428; Ai = 5; Level = 'M'; Min = 15; Seed = 51003; F = '' },
    @{ N = 'G-desert3E'; Map = 'Tournament Desert'; Size = 143638; Ai = 3; Level = 'E'; Min = 12; Seed = 51004; F = '' }) }
else { @(
    @{ N = 'Z-desert3H-gla'; Map = 'Tournament Desert'; Size = 144534; Ai = 3; Level = 'H'; Min = 15; Seed = 51101; F = '4,11,13' },
    @{ N = 'Z-city5H-mix'; Map = 'Tournament City'; Size = 386149; Ai = 5; Level = 'H'; Min = 20; Seed = 51102; F = '2,3,4,5,8' },
    @{ N = 'Z-alpine3M-generals'; Map = 'Alpine Assault'; Size = 275491; Ai = 3; Level = 'M'; Min = 15; Seed = 51103; F = '6,9,12' },
    @{ N = 'Z-desert3H-china'; Map = 'Tournament Desert'; Size = 144534; Ai = 3; Level = 'H'; Min = 15; Seed = 51104; F = '3,8,10' }) }
if ($Only) { $scenarios = @($scenarios | Where-Object { $Only -contains $_.N }) }

$exes = [ordered]@{}
foreach ($e in $Exe) {
    $label, $path = $e -split '=', 2
    $name = ([IO.Path]::GetFileNameWithoutExtension((Join-Path $GameDir ($Title -eq 'Generals' ? 'generalsv.exe' : 'generalszh.exe')))) + "_gate_$label.exe"
    Copy-Item -LiteralPath (Resolve-Path $path) -Destination (Join-Path $GameDir $name) -Force
    $exes[$label] = $name
}
$labels = @($exes.Keys)

function Parse-Summary([string]$File) {
    $t = Get-Content -LiteralPath $File -Raw
    $r = [ordered]@{}
    if ($t -match 'digest ([0-9A-F]{16})') { $r.digest = $Matches[1] }
    if ($t -match 'mismatch/errors: (\d+)') { $r.mismatches = [int]$Matches[1] }
    if ($t -match 'ticks measured: (\d+)') { $r.ticks = [int]$Matches[1] }
    if ($t -match 'tick ms: mean ([\d.]+) \| median ([\d.]+) \| p90 ([\d.]+) \| p95 ([\d.]+) \| p99 ([\d.]+) \| p99\.9 ([\d.]+) \| worst ([\d.]+)') {
        $r.mean = [double]$Matches[1]; $r.p99 = [double]$Matches[5]; $r.p999 = [double]$Matches[6]; $r.worst = [double]$Matches[7] }
    if ($t -match 'ticks above 16\.67 ms: (\d+) \| 25 ms: (\d+) \| 33\.333 ms: (\d+)') { $r.over33 = [int]$Matches[3] }
    if ($t -match 'missed deadlines: (\d+) \| catch-up ticks: (\d+) \| longest catch-up run: (\d+) \| max backlog: ([\d.]+) ms') { $r.missed = [int]$Matches[1]; $r.catchup = [int]$Matches[2]; $r.backlogMs = [double]$Matches[4] }
    if ($t -match 'queued paths (\d+)') { $r.queuedPaths = [int]$Matches[1] }
    foreach ($phase in 'ai_player_strategy', 'ai_object_base_update', 'logic_scripts', 'ai_global', 'path_search', 'path_queue', 'object_update_loops') {
        if ($t -match "(?m)^\s+$phase\s+([\d.]+)\s+[\d.]+\s+[\d.]+\s+[\d.]+\s+([\d.]+)\s+([\d.]+)\s+[\d.]+\s+(\d+)") { $r["$phase`_total_ms"] = [double]$Matches[1]; $r["$phase`_p99"] = [double]$Matches[2]; $r["$phase`_calls"] = [int]$Matches[4] }
    }
    $r
}

$results = [System.Collections.Generic.List[object]]::new()
Push-Location $GameDir
try {
    foreach ($s in $scenarios) {
        $minutes = [Math]::Max(2, $s.Min * $minutesScale)
        foreach ($label in $labels) {
            $isLast = ($label -eq $labels[-1])
            foreach ($mode in $(if ($isLast) { $Modes } else { @('O') })) {
                $rep = Join-Path $Out "$($s.N)-$mode.rep"
                $genArgs = @('-I', $gen, '--template', $Template, '--out', $rep, '--map', $s.Map, '--map-size', $s.Size, '--ai', $s.Ai, '--level', $s.Level, '--minutes', $minutes, '--seed', $s.Seed, '--ai-mode', $mode)
                if ($s.F) { $genArgs += @('--factions', $s.F) }
                & python @genArgs | Out-Null
                if ($LASTEXITCODE) { throw "replay generation failed for $($s.N)" }
                $digests = @(); $last = $null
                for ($i = 1; $i -le $Runs; $i++) {
                    $base = Join-Path $Out "$($s.N)-$mode-$label-run$i"
                    $env:GENERALS_SIM_IGNORE_CRC = '1'; $env:GENERALS_SIM_FP_INTERVAL = '30'; $env:GENERALS_SIM_BENCH = $base
                    cmd /c "`"$(Join-Path $GameDir $exes[$label])`" -headless -replay `"$rep`" > `"$base.stdout.txt`" 2>&1"
                    $env:GENERALS_SIM_BENCH = $null
                    $sum = "${base}_summary.txt"
                    if (-not (Test-Path -LiteralPath $sum)) { throw "no summary for $($s.N) $label $mode run $i (see $base.stdout.txt)" }
                    $last = Parse-Summary $sum
                    $digests += $last.digest
                }
                $row = [ordered]@{ scenario = $s.N; exe = $label; mode = $mode; minutes = $minutes; runs = $Runs; repeatable = (@($digests | Sort-Object -Unique).Count -eq 1); digest = $digests[0] }
                foreach ($k in $last.Keys) { if ($k -ne 'digest') { $row[$k] = $last[$k] } }
                $results.Add([pscustomobject]$row)
                Write-Host ("{0,-20} {1,-5} {2} digest {3} repeatable={4} mean {5} ms p99 {6} p99.9 {7} over33 {8}" -f $s.N, $label, $mode, $row.digest, $row.repeatable, $row.mean, $row.p99, $row.p999, $row.over33)
            }
        }
    }
} finally {
    Pop-Location
    Remove-Item Env:GENERALS_SIM_BENCH, Env:GENERALS_SIM_IGNORE_CRC, Env:GENERALS_SIM_FP_INTERVAL -ErrorAction SilentlyContinue
}
$json = Join-Path $Out "gate-$Title.json"
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $json -Encoding utf8

# verdicts
$fail = @()
foreach ($s in $scenarios) {
    $ref = $results | Where-Object { $_.scenario -eq $s.N -and $_.exe -eq $labels[0] -and $_.mode -eq 'O' }
    foreach ($r in ($results | Where-Object { $_.scenario -eq $s.N })) {
        if (-not $r.repeatable) { $fail += "$($s.N) $($r.exe) $($r.mode): digest differs between runs" }
        if ($r.mismatches) { $fail += "$($s.N) $($r.exe) $($r.mode): $($r.mismatches) replay mismatches" }
        if ($r.mode -eq 'O' -and $ref -and $r.digest -ne $ref.digest) { $fail += "$($s.N): Original digest of '$($r.exe)' ($($r.digest)) differs from the reference '$($labels[0])' ($($ref.digest))" }
    }
}
if ($fail.Count) { Write-Host "GATE FAIL:`n$($fail -join "`n")" -ForegroundColor Red; exit 2 }
Write-Host "GATE PASS ($Title): Original digests equal the reference for every scenario; every run is repeatable. Results: $json"
