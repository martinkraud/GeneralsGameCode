#Requires -Version 7.0
# X64.52: deterministic dynamic experiments for Enhanced AI Dynamic Base & Defence (layout variation, completeness, expansion, rebuilding, threat-responsive defence).
# Same machinery as Invoke-AiFindingExperiments.ps1 (self-playing AI-versus-AI headless simulation, GENERALS_AI_DIAG counters / census), parameterised by scenario, game seed and AI Behaviour mode.
# Every run records: the fingerprint digest, the counters, the structure census (<run>-diag_census.txt) and the event log (<run>-diag_ai.txt). Disposable stage only.
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals', 'ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$GameDir,
      [Parameter(Mandatory)][string]$Template,
      [Parameter(Mandatory)][string]$Exe,
      [Parameter(Mandatory)][string]$Out,
      [Parameter(Mandatory)][string]$Scenario,                 # name from the table below
      [string[]]$Seeds = @('51001'),
      [string[]]$Modes = @('O', 'E'),
      [string]$Census = '9000,18000,36000',
      [string]$Kill = '',
      [string]$Dump = '',
      [string]$Defense = '',
      [string]$Attack = '',
      [string]$Off = '',
      [string]$Sw = '0',                                       # X64.52A: Superweapons match rule 0|1|3|5|D (written into the replay setup)
      [string]$Cash = '0',                                     # X64.52A: Starting Cash match option (SC=N), 0 = default
      [string]$SwAid = '',                                     # X64.52A: GENERALS_AI_DIAG_SW aids (give / capture / probe / build)
      [string]$Tag = '',                                       # ablation: layout,jitter,threat,threatdef,fill,expand
      [double]$Minutes = 0)
$ErrorActionPreference = 'Stop'
$Seeds = @($Seeds | ForEach-Object { $_ -split ',' } | Where-Object { $_ }); $Modes = @($Modes | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gen = Join-Path $repo 'scripts\performance\New-SimulationBenchmarkReplay.py'
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$exeName = ($Title -eq 'Generals' ? 'generalsv' : 'generalszh') + '_exp.exe'
Copy-Item -LiteralPath $Exe -Destination (Join-Path $GameDir $exeName) -Force

# name -> map, map size, AI slots, level, minutes, factions (PlayerTemplate indices; empty = the generator default)
$scenarios = @{
    'G-twilight7H'  = @{ Map = 'Twilight Flame'; Size = 402471; Ai = 7; Level = 'H'; Min = 20; F = '' }
    'G-desert3H'    = @{ Map = 'Tournament Desert'; Size = 143638; Ai = 3; Level = 'H'; Min = 15; F = '' }
    'G-wasteland5M' = @{ Map = 'Wasteland Warlords'; Size = 305428; Ai = 5; Level = 'M'; Min = 15; F = '' }
    'Z-city5H'      = @{ Map = 'Tournament City'; Size = 386149; Ai = 5; Level = 'H'; Min = 20; F = '2,3,4,5,8' }
    'Z-desert3H-gla' = @{ Map = 'Tournament Desert'; Size = 144534; Ai = 3; Level = 'H'; Min = 15; F = '4,11,13' }
    'Z-desert3H-china' = @{ Map = 'Tournament Desert'; Size = 144534; Ai = 3; Level = 'H'; Min = 15; F = '3,8,10' }
    'Z-alpine3M'    = @{ Map = 'Alpine Assault'; Size = 275491; Ai = 3; Level = 'M'; Min = 15; F = '6,9,12' }
}
$x = $scenarios[$Scenario]
if (-not $x) { throw "unknown scenario $Scenario ($($scenarios.Keys -join ', '))" }
if ($Minutes -gt 0) { $x.Min = $Minutes }

$summary = [System.Collections.Generic.List[object]]::new()
Push-Location $GameDir
try {
    foreach ($seed in $Seeds) {
        foreach ($mode in $Modes) {
            $tag = $(if ($Off) { '-off-' + ($Off -replace ',', '+') } else { '' }) + $(if ($Sw -ne '0') { "-sw$Sw" } else { '' }) + $(if ($Cash -ne '0') { "-cash$Cash" } else { '' }) + $(if ($Tag) { "-$Tag" } else { '' })
            $base = Join-Path $Out "$Scenario-s$seed-$mode$tag"
            $rep = "$base.rep"
            $genArgs = @('-I', $gen, '--template', $Template, '--out', $rep, '--map', $x.Map, '--map-size', $x.Size, '--ai', $x.Ai, '--level', $x.Level, '--minutes', $x.Min, '--seed', $seed, '--ai-mode', $mode, '--superweapons', $Sw, '--cash', $Cash)
            if ($x.F) { $genArgs += @('--factions', $x.F) }
            & python @genArgs | Out-Null
            $env:GENERALS_SIM_IGNORE_CRC = '1'; $env:GENERALS_SIM_FP_INTERVAL = '30'; $env:GENERALS_SIM_BENCH = $base; $env:GENERALS_AI_DIAG = "$base-diag"
            $env:GENERALS_AI_DIAG_KILL = $Kill; $env:GENERALS_AI_DIAG_DUMP = $Dump; $env:GENERALS_AI_DIAG_DEFENSE = $Defense; $env:GENERALS_AI_DIAG_CENSUS = $Census; $env:GENERALS_AI_DIAG_ATTACK = $Attack; $env:GENERALS_AI_DIAG_X52_OFF = $Off; $env:GENERALS_AI_DIAG_SW = $SwAid
            cmd /c "`"$(Join-Path $GameDir $exeName)`" -headless -replay `"$rep`" > `"$base.stdout.txt`" 2>&1"
            Remove-Item Env:GENERALS_SIM_BENCH, Env:GENERALS_AI_DIAG, Env:GENERALS_AI_DIAG_KILL, Env:GENERALS_AI_DIAG_DUMP, Env:GENERALS_AI_DIAG_DEFENSE, Env:GENERALS_AI_DIAG_CENSUS, Env:GENERALS_AI_DIAG_ATTACK, Env:GENERALS_AI_DIAG_X52_OFF, Env:GENERALS_AI_DIAG_SW -ErrorAction SilentlyContinue
            $diag = Get-Content -LiteralPath "$base-diag_ai.txt" -ErrorAction SilentlyContinue
            $counters = [ordered]@{}
            foreach ($l in $diag) { if ($l -match '^counter (\S+) = (-?\d+)') { $counters[$Matches[1]] = [long]$Matches[2] } }
            $digest = ((Select-String -Path "${base}_summary.txt" -Pattern 'digest' | Select-Object -First 1).Line -replace '.*digest ', '')
            $tick = (Select-String -Path "${base}_summary.txt" -Pattern 'tick' | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | '
            $summary.Add([pscustomobject]@{ scenario = $Scenario; seed = $seed; mode = $mode; digest = $digest; counters = $counters; timing = $tick })
            Write-Host ("{0,-18} seed {1,-7} {2}  digest {3}" -f $Scenario, $seed, $mode, $digest)
        }
    }
} finally { Pop-Location }
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Out "base-$Scenario.json") -Encoding utf8
Write-Host "wrote $(Join-Path $Out "base-$Scenario.json")"
