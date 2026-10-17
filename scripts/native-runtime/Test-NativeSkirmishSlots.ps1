#Requires -Version 7.0
# X64.48A: automated reachability test of the Skirmish setup screen. For a title and several client sizes it starts the staged native game with the
# developer flow GENERALS_AUTOFLOW=slots, which fills the eight slots with AI through the real combo boxes, then for every AI slot opens the drop-down
# list and asks the real mouse path (the window that GameWindowManager::winProcessMouseEvent would give a click to) who owns the centre of the combo and of
# every list row. A row owned by anything else is a defect. Needs a disposable stage (the game is run in place); a window is shown for about half a minute per run.
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Name,
      [string[]]$Sizes = @('1920x1080', '2560x1440', '5120x1440'))
$ErrorActionPreference = 'Stop'
$capture = Join-Path $PSScriptRoot 'Invoke-NativeAutoCapture.ps1'
$failures = @()
foreach ($size in $Sizes) {
    & pwsh -NoProfile -File $capture -Title $Title -Name $Name -Flow Slots -TimeoutSeconds 200 -ClientSize $size | Out-Null
    $dir = Get-ChildItem (Join-Path (Split-Path $PSScriptRoot -Parent | Split-Path -Parent) 'build\diag') -Directory | Sort-Object LastWriteTime | Select-Object -Last 1
    $rows = @(Get-Content -LiteralPath (Join-Path $dir.FullName 'generals64.log') | Where-Object { $_ -match "`tslotprobe`t(ok|defect)`t" })
    $ok = @($rows | Where-Object { $_ -match "`tslotprobe`tok`t" }).Count
    $bad = @($rows | Where-Object { $_ -match "`tslotprobe`tdefect`t" })
    Write-Host ("{0} {1}: {2} slots reachable, {3} with unreachable controls" -f $Title, $size, $ok, $bad.Count)
    if ($rows.Count -lt 7 -or $bad.Count) { $failures += "$Title $size ($($rows.Count) probed, $($bad.Count) defects): $dir" }
}
if ($failures.Count) { throw "Skirmish slot reachability FAILED:`n$($failures -join "`n")" }
Write-Host 'PASS: every AI slot (1-7) of the Skirmish setup screen is reachable in every row at every tested size.'
