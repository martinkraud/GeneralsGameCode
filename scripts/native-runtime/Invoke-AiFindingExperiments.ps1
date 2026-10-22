#Requires -Version 7.0
# X64.51: bounded dynamic experiments for the X64.50 findings (E1 / AI-01, E2 / AI-02, E3 Command Center rebuild, E4 / AI-04, AI-08, AI-11).
# Each experiment is a deterministic self-playing AI-versus-AI headless simulation (the idle human + computer slots replay of Invoke-AiBehaviorGate.ps1) run twice with the
# SAME scenario aids - once in Original and once in Enhanced mode - with GENERALS_AI_DIAG counters (Common/AIDiagnostics.h). The aids (scheduled kills, forced
# defence orders, build list dumps) are identical in both runs; only the AI Behaviour rule differs. Disposable stage only; no Steam directory, no user data.
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals', 'ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$GameDir,
      [Parameter(Mandatory)][string]$Template,
      [Parameter(Mandatory)][string]$Exe,
      [Parameter(Mandatory)][string]$Out,
      [string]$Only = '')
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gen = Join-Path $repo 'scripts\performance\New-SimulationBenchmarkReplay.py'
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$exeName = ($Title -eq 'Generals' ? 'generalsv' : 'generalszh') + '_exp.exe'
Copy-Item -LiteralPath $Exe -Destination (Join-Path $GameDir $exeName) -Force

function Kills([int]$Count, [int]$First, [int]$Step, [string]$TemplateName) { (0..($Count - 1) | ForEach-Object { "$($First + $_ * $Step):$TemplateName@-1" }) -join ';' }

$gDesert = @{ Map = 'Tournament Desert'; Size = 143638; Ai = 3; Level = 'H'; Seed = 51001; F = '' }
$zDesert = @{ Map = 'Tournament Desert'; Size = 144534; Ai = 3; Level = 'H'; Seed = 51101 }
$experiments = if ($Title -eq 'Generals') { @(
    @{ N = 'G-CC-rebuild'; S = $gDesert; Min = 25; Kill = (Kills 4 3000 7000 'ChinaCommandCenter'); Dump = '2900,3200,24900'; Note = 'E3 + E1: China Command Center (Rebuilds=3, AutomaticallyBuild=Yes) destroyed four times' },
    @{ N = 'G-E1-automatic-entry'; S = @{ Map = 'Twilight Flame'; Size = 402471; Ai = 7; Level = 'H'; Seed = 51002; F = '' }; Min = 14; Kill = ''; Dump = '3000,6000,9000,17000'; Ini = 'AIData-E1-america-automatic-barracks.ini'; Note = 'E1: a synthetic America build list with an unscripted AutomaticallyBuild=Yes Barracks slot' },
    @{ N = 'G-GLA-holes'; S = @{ Map = 'Twilight Flame'; Size = 402471; Ai = 7; Level = 'H'; Seed = 51002; F = '' }; Min = 22; Kill = '6000:GLABarracks@-1;8000:GLABarracks@-1;10000:GLAArmsDealer@-1;12000:GLAPalace@-1;14000:GLATunnelNetwork@-1;16000:GLAStingerSite@-1;18000:GLASupplyStash@-1'; Dump = ''; Note = 'E2: GLA structures destroyed (they leave rebuild holes)' },
    @{ N = 'G-defense-stress'; S = $gDesert; Min = 12; Kill = ''; Defense = '4000:40@-1'; Note = 'AI-11: 40 front + 40 flank defences ordered at once' },
    @{ N = 'G-twilight-retaliation'; S = @{ Map = 'Twilight Flame'; Size = 402471; Ai = 7; Level = 'H'; Seed = 51002; F = '' }; Min = 20; Kill = ''; Note = 'AI-08: retaliation preference with 7 AI' }) }
else { @(
    @{ N = 'Z-China-CC-unlimited'; S = ($zDesert + @{ F = '3,8,10' }); Min = 30; Kill = ((Kills 5 3000 6000 'ChinaCommandCenter') + ';3600:Tank_ChinaCommandCenter@-1'); Dump = '2900,3200,32900'; Note = 'E3: China (Rebuilds=-1) vs Tank General (Rebuilds=0) Command Centers destroyed' },
    @{ N = 'Z-GLA-holes'; S = ($zDesert + @{ F = '4,11,13' }); Min = 25; Kill = '5000:GLABarracks@-1;8000:GLABarracks@-1;11000:GLAArmsDealer@-1;14000:GLABlackMarket@-1;17000:GLAStingerSite@-1;20000:GLABarracks@-1'; Dump = ''; Note = 'E2: GLA structures destroyed' },
    @{ N = 'Z-superweapons'; S = @{ Map = 'Tournament City'; Size = 386149; Ai = 5; Level = 'H'; Seed = 51102; F = '5,10,12,6,3' }; Min = 35; Kill = ''; Note = 'E4: AI superweapon targeting (fine search)' },
    @{ N = 'Z-defense-stress'; S = ($zDesert + @{ F = '2,3,4' }); Min = 12; Kill = ''; Defense = '4000:40@-1'; Note = 'AI-11 stress' },
    @{ N = 'Z-retaliation'; S = @{ Map = 'Tournament City'; Size = 386149; Ai = 5; Level = 'H'; Seed = 51102; F = '2,3,4,5,8' }; Min = 20; Kill = ''; Note = 'AI-08' }) }
if ($Only) { $experiments = @($experiments | Where-Object { $_.N -eq $Only }) }

$summary = [System.Collections.Generic.List[object]]::new()
Push-Location $GameDir
try {
    foreach ($x in $experiments) {
        foreach ($mode in 'O', 'E') {
            $base = Join-Path $Out "$($x.N)-$mode"
            $rep = "$base.rep"
            $genArgs = @('-I', $gen, '--template', $Template, '--out', $rep, '--map', $x.S.Map, '--map-size', $x.S.Size, '--ai', $x.S.Ai, '--level', $x.S.Level, '--minutes', $x.Min, '--seed', $x.S.Seed, '--ai-mode', $mode)
            if ($x.S.F) { $genArgs += @('--factions', $x.S.F) }
            & python @genArgs | Out-Null
            $override = $null
            if ($x.Ini) { $override = Join-Path $GameDir 'Data\INI\AIData.ini'; if (Test-Path -LiteralPath $override) { throw "refusing to overwrite $override" }; New-Item -ItemType Directory -Force -Path (Split-Path $override) | Out-Null; Copy-Item -LiteralPath (Join-Path $PSScriptRoot "ai-experiments\$($x.Ini)") -Destination $override }
            $env:GENERALS_SIM_IGNORE_CRC = '1'; $env:GENERALS_SIM_FP_INTERVAL = '30'; $env:GENERALS_SIM_BENCH = $base; $env:GENERALS_AI_DIAG = "$base-diag"
            $env:GENERALS_AI_DIAG_KILL = $x.Kill; $env:GENERALS_AI_DIAG_DUMP = $x.Dump; $env:GENERALS_AI_DIAG_DEFENSE = $x.Defense
            cmd /c "`"$(Join-Path $GameDir $exeName)`" -headless -replay `"$rep`" > `"$base.stdout.txt`" 2>&1"
            Remove-Item Env:GENERALS_SIM_BENCH, Env:GENERALS_AI_DIAG, Env:GENERALS_AI_DIAG_KILL, Env:GENERALS_AI_DIAG_DUMP, Env:GENERALS_AI_DIAG_DEFENSE -ErrorAction SilentlyContinue
            if ($override -and (Test-Path -LiteralPath $override)) { [IO.File]::Delete($override) }
            $diag = Get-Content -LiteralPath "$base-diag_ai.txt" -ErrorAction SilentlyContinue
            $counters = [ordered]@{}
            foreach ($l in $diag) { if ($l -match '^counter (\S+) = (-?\d+)') { $counters[$Matches[1]] = [long]$Matches[2] } }
            $digest = ((Select-String -Path "${base}_summary.txt" -Pattern 'digest' | Select-Object -First 1).Line -replace '.*digest ', '')
            $events = @($diag | Where-Object { $_ -match '^event' -and $_ -notmatch ' buildlist ' })
            $summary.Add([pscustomobject]@{ experiment = $x.N; mode = $mode; note = $x.Note; digest = $digest; counters = $counters; events = $events.Count })
            Write-Host ("{0,-24} {1}  digest {2}" -f $x.N, $mode, $digest)
            foreach ($k in $counters.Keys) { if ($k -notlike 'build.started.*' -and $k -notlike 'events.buildlist') { Write-Host ("      {0} = {1}" -f $k, $counters[$k]) } }
        }
    }
} finally { Pop-Location }
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Out "experiments-$Title.json") -Encoding utf8
Write-Host "wrote $(Join-Path $Out "experiments-$Title.json")"
