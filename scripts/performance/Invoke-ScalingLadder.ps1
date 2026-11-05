#Requires -Version 7.0
# X64.sc scalability ladder: runs the same seeded AI-vs-AI scenario family at increasing workload (more AI players, more starting cash = more production, bigger armies) with
# Invoke-PathStress.ps1 and prints one table: peak object count, tick distribution, the share of the main phases. Headless and deterministic: the digest of each rung is
# printed so a rung can be re-run and compared. Never run two game runs at once: the rungs run strictly one after another.
#
#   pwsh scripts/performance/Invoke-ScalingLadder.ps1 -Title ZeroHour -GameDir <abs stage>\game -Template <abs>\zh-template.rep -Exe <abs>\generalszh.exe -Out <abs>\ladder-zh
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals', 'ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$GameDir,
      [Parameter(Mandatory)][string]$Template,
      [Parameter(Mandatory)][string]$Exe,
      [Parameter(Mandatory)][string]$Out,
      [string]$Label = 'lad',
      [ValidateSet('O','E')][string]$AiMode = 'E',
      [string[]]$Rungs = @('1x','2x','3x','4x'))
$ErrorActionPreference = 'Stop'
# Disk hygiene: fail early instead of filling the disk (see docs/modernization/DISK_RETENTION.md).
. (Join-Path $PSScriptRoot '..\native-runtime\AssetStore.ps1')
Assert-FreeDiskSpace (Join-Path $PSScriptRoot '..\..\build') 2 'the scaling ladder'
$Rungs = @($Rungs | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$stress = Join-Path $PSScriptRoot 'Invoke-PathStress.ps1'
# rung -> scenario (AI count is part of the scenario), starting cash
$plan = [ordered]@{
    '1x' = @{ S = 'twilight4H'; Cash = '50000' }
    '2x' = @{ S = 'twilight7H'; Cash = '50000' }
    '3x' = @{ S = 'twilight7H'; Cash = '150000' }
    '4x' = @{ S = 'twilight7H'; Cash = '400000' }
}
$rows = @()
foreach ($r in $Rungs) {
    $p = $plan[$r]
    $dir = Join-Path $Out $r
    & pwsh -NoProfile -File $stress -Title $Title -GameDir $GameDir -Template $Template -Exe $Exe -Out $dir -Label "$Label$r" -AiMode $AiMode -Cash $p.Cash -Only $p.S | Out-Host
    $fp = Get-ChildItem $dir -Filter '*_fingerprint.txt' | Select-Object -First 1
    $peak = 0; $sum = 0; $n = 0
    if ($fp) { foreach ($l in Get-Content $fp.FullName) { if ($l -match 'objects=(\d+)') { $o = [int]$Matches[1]; $n++; $sum += $o; if ($o -gt $peak) { $peak = $o } } } }
    $sm = Get-ChildItem $dir -Filter '*_summary.txt' | Select-Object -First 1
    $rows += [pscustomobject]@{ Rung = $r; Scenario = $p.S; Cash = $p.Cash; PeakObjects = $peak; MeanObjects = $(if ($n) { [int]($sum / $n) } else { 0 }); Summary = $sm.FullName }
}
$rows | Format-Table Rung, Scenario, Cash, PeakObjects, MeanObjects -AutoSize | Out-Host
$rows | ConvertTo-Json | Set-Content (Join-Path $Out 'ladder.json')
