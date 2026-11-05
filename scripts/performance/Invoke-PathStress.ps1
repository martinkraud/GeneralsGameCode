#Requires -Version 7.0
# Pathfinding stress suite: deterministic, headless, seeded AI-vs-AI replays (an idle human slot plus computer slots) of named scenarios, run with an executable and
# optionally the read-only pathfinding diagnostics. Same replay + same executable = same simulation (the fingerprint digest proves it), so before / after runs compare
# like with like. Nothing is written to a Steam folder: -GameDir is a disposable stage (build\dev-runtimes\native\<Title>\<stage>\game).
#
#   pwsh scripts/performance/Invoke-PathStress.ps1 -Title ZeroHour -GameDir <stage>\game -Template build/x64-51/templates/zh-template.rep -Exe build/win64/GeneralsMD/Release/generalszh.exe -Out build/diag/pathstress-zh -Label after
#   add -Diag for GENERALS_SIM_PATHFAIL=1 (failing-search forensics; slower, kept apart from timing runs) and -Paths N for GENERALS_SIM_PATHS (line-phase profile)
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals', 'ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$GameDir,
      [Parameter(Mandatory)][string]$Template,
      [Parameter(Mandatory)][string]$Exe,
      [Parameter(Mandatory)][string]$Out,
      [string]$Label = 'run',
      [string[]]$Only,
      [double]$MinutesScale = 1.0,
      [ValidateSet('O','E')][string]$AiMode = 'O',        # AI Behaviour: O = Original (retail), E = Enhanced (what a default Skirmish uses)
      [string]$Cash = '',                                # starting cash for every slot (the interactive Skirmish default of this project is 50000)
      [switch]$Diag,
      [ValidateRange(0, 3)][int]$Paths = 0,
      [hashtable]$ExtraEnv = @{})
$ErrorActionPreference = 'Stop'
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gen = Join-Path $repo 'scripts\performance\New-SimulationBenchmarkReplay.py'
New-Item -ItemType Directory -Force -Path $Out | Out-Null
# name, map, size, AI count, level, minutes, seed, factions (PlayerTemplate indices; empty = cycled)
$all = @(
    @{ N = 'twilight4M';  Map = 'Twilight Flame';     Size = 402471; Ai = 4; Level = 'M'; Min = 13; Seed = 6101; F = '' },
    @{ N = 'twilight4H';  Map = 'Twilight Flame';     Size = 402471; Ai = 4; Level = 'H'; Min = 13; Seed = 6106; F = '' },
    @{ N = 'twilight7H';  Map = 'Twilight Flame';     Size = 402471; Ai = 7; Level = 'H'; Min = 20; Seed = 6102; F = '' },
    @{ N = 'desert3H';    Map = 'Tournament Desert';  Size = $(if ($Title -eq 'Generals') { 143638 } else { 144534 }); Ai = 3; Level = 'H'; Min = 15; Seed = 6103; F = '' },
    @{ N = 'wasteland5M'; Map = 'Wasteland Warlords'; Size = 305428; Ai = 5; Level = 'M'; Min = 15; Seed = 6104; F = '' },
    @{ N = 'city5H';      Map = 'Tournament City';    Size = 386149; Ai = 5; Level = 'H'; Min = 20; Seed = 6105; F = '' })
$scenarios = if ($Only.Count) { @($all | Where-Object { $Only -contains $_.N }) } else { $all }
$exeName = ([IO.Path]::GetFileNameWithoutExtension((Join-Path $GameDir ($Title -eq 'Generals' ? 'generalsv.exe' : 'generalszh.exe')))) + "_ps_$Label.exe"
Copy-Item -LiteralPath (Resolve-Path $Exe) -Destination (Join-Path $GameDir $exeName) -Force
Push-Location $GameDir
try {
    foreach ($s in $scenarios) {
        $minutes = [Math]::Max(2, $s.Min * $MinutesScale)
        $rep = Join-Path $Out "$($s.N).rep"
        $genArgs = @('-I', $gen, '--template', $Template, '--out', $rep, '--map', $s.Map, '--map-size', $s.Size, '--ai', $s.Ai, '--level', $s.Level, '--minutes', $minutes, '--seed', $s.Seed, '--ai-mode', $AiMode)
        if ($Cash) { $genArgs += @('--cash', $Cash) }
        if ($s.F) { $genArgs += @('--factions', $s.F) }
        & python @genArgs | Out-Null
        if ($LASTEXITCODE) { Write-Warning "replay generation failed for $($s.N)"; continue }
        $base = Join-Path $Out "$($s.N)-$Label"
        $env:GENERALS_SIM_IGNORE_CRC = '1'; $env:GENERALS_SIM_FP_INTERVAL = '30'; $env:GENERALS_SIM_BENCH = $base
        if ($Diag) { $env:GENERALS_SIM_PATHFAIL = '1' }
        if ($Paths) { $env:GENERALS_SIM_PATHS = "$Paths" }
        foreach ($k in $ExtraEnv.Keys) { Set-Item "Env:$k" $ExtraEnv[$k] }
        $t = Get-Date
        cmd /c "`"$(Join-Path $GameDir $exeName)`" -headless -replay `"$rep`" > `"$base.stdout.txt`" 2>&1"
        $sum = "${base}_summary.txt"
        if (-not (Test-Path -LiteralPath $sum)) { Write-Warning "no summary for $($s.N) (see $base.stdout.txt)"; continue }
        $text = Get-Content -LiteralPath $sum -Raw
        $digest = if ($text -match 'digest ([0-9A-F]{16})') { $Matches[1] } else { '?' }
        $tick = if ($text -match 'tick ms: mean ([\d.]+) \| median ([\d.]+) \| p90 ([\d.]+) \| p95 ([\d.]+) \| p99 ([\d.]+) \| p99\.9 ([\d.]+) \| worst ([\d.]+)') { "mean $($Matches[1]) p50 $($Matches[2]) p95 $($Matches[4]) p99 $($Matches[5]) p99.9 $($Matches[6]) worst $($Matches[7])" } else { '?' }
        $over = if ($text -match 'ticks above 16\.67 ms: (\d+) \| 25 ms: (\d+) \| 33\.333 ms: (\d+)') { ">33.3ms $($Matches[3])" } else { '' }
        Write-Host ("{0,-12} {1} digest {2} {3} {4} ({5:N0} s)" -f $s.N, $Label, $digest, $tick, $over, ((Get-Date) - $t).TotalSeconds)
    }
} finally {
    Pop-Location
    foreach ($v in 'GENERALS_SIM_BENCH', 'GENERALS_SIM_IGNORE_CRC', 'GENERALS_SIM_FP_INTERVAL', 'GENERALS_SIM_PATHFAIL', 'GENERALS_SIM_PATHS') { Remove-Item "Env:$v" -ErrorAction SilentlyContinue }
    foreach ($k in $ExtraEnv.Keys) { Remove-Item "Env:$k" -ErrorAction SilentlyContinue }
}
