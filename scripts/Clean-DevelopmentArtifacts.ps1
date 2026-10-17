<#
.SYNOPSIS
  One entry point for reclaiming disk space from staged development runtimes, without a wildcard delete anywhere.

.DESCRIPTION
  DRY RUN BY DEFAULT. The tool only ever delegates to two allowlisted, individually tested tools:
    * scripts\native-runtime\Cleanup-NativeCandidates.ps1   (build\dev-runtimes\native\<Title>\<candidate>)
    * scripts\zh-runtime\Cleanup-ZHStages.ps1               (build\dev-runtimes\zh\<stage>, workflow-check-*)
  Each prints its own plan and PLAN_HASH. -Execute needs the reviewed hash of every selected tool
  (-ApprovedNativePlan / -ApprovedZHPlan); a changed tree changes the hash and the execution is refused.

  It additionally REPORTS (never deletes) the categories that stay a human decision:
    build\win32, build\win64          regenerable (cmake --preset win32|win64 + cmake --build --preset ...), kept for the next build
    build\dev-runtimes\userdata-backups   snapshots of the user's own game data; only removed by hand after a hash comparison
    build\evidence-archive, build\performance, docs\modernization\evidence   canonical evidence - refused
  Refused by construction: tracked paths, source, docs, tests, scripts, Git, evidence-archive, performance, any Steam/EA
  installation path. Never calls git clean. Validates that it runs inside the expected repository (this script's parent).

.EXAMPLE
  pwsh scripts\Clean-DevelopmentArtifacts.ps1 -Active x64-37-modern-settings-v4 -LatestAccepted x64-37-modern-settings-v4
  pwsh scripts\Clean-DevelopmentArtifacts.ps1 -Active ... -LatestAccepted ... -Execute -ApprovedNativePlan <hash> -ApprovedZHPlan <hash>
#>
[CmdletBinding()]
param(
    [ValidateSet('Both', 'Native', 'ZH')][string]$Only = 'Both',
    [string]$Active,
    [string]$LatestAccepted,
    [string]$ZHActive,
    [switch]$Execute,
    [string]$ApprovedNativePlan,
    [string]$ApprovedZHPlan
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

function Fail([string]$message, [int]$code = 1) { Write-Error $message -ErrorAction Continue; exit $code }

# ---- repository validation ---------------------------------------------------------------------------------------------
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
foreach ($marker in @('.git', 'CMakeLists.txt', 'scripts\native-runtime\Cleanup-NativeCandidates.ps1', 'scripts\zh-runtime\Cleanup-ZHStages.ps1', 'build')) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $marker))) { Fail "Not the expected repository (missing $marker under $repoRoot)." }
}
if ($repoRoot -match '(?i)[\\/]steamapps[\\/]|[\\/]Steam[\\/]|Program Files \(x86\)[\\/]Steam') { Fail "Refusing a Steam-looking location: $repoRoot" }
$top = (& git -C $repoRoot rev-parse --show-toplevel 2>$null)
if (-not $top -or ((Resolve-Path $top).Path.TrimEnd('\') -ne $repoRoot.TrimEnd('\'))) { Fail 'The script is not inside the root of its own Git repository.' }

$nativeTool = Join-Path $repoRoot 'scripts\native-runtime\Cleanup-NativeCandidates.ps1'
$zhTool = Join-Path $repoRoot 'scripts\zh-runtime\Cleanup-ZHStages.ps1'
$runNative = $Only -in 'Both', 'Native'
$runZH = $Only -in 'Both', 'ZH'

function Invoke-Tool([string]$tool, [string[]]$arguments) {
    $text = & pwsh -NoProfile -File $tool @arguments 2>&1 | Out-String
    return [pscustomobject]@{ Code = $LASTEXITCODE; Text = $text }
}
function Get-PlanHash([string]$text) { if ($text -match 'PLAN_HASH: ([0-9A-F]{64})') { return $Matches[1] } return $null }
function Get-TreeGB([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    [int64]$sum = 0
    foreach ($f in (Get-ChildItem -LiteralPath $path -Recurse -Force -File -ErrorAction SilentlyContinue)) { $sum += $f.Length }
    return [math]::Round($sum / 1GB, 2)
}

$mode = if ($Execute) { 'EXECUTE' } else { 'DRY RUN (nothing will be changed)' }
Write-Host "Development artifact cleanup - $mode"
Write-Host "Repository: $repoRoot"
$drive = Get-PSDrive -Name ([IO.Path]::GetPathRoot($repoRoot).Substring(0, 1)) -ErrorAction SilentlyContinue
if ($drive) { Write-Host ('Free space on {0}: {1:N1} GB' -f $drive.Name, ($drive.Free / 1GB)) }
Write-Host ''

$nativeArgs = @()
if ($Active) { $nativeArgs += @('-Active', $Active) }
if ($LatestAccepted) { $nativeArgs += @('-LatestAccepted', $LatestAccepted) }
$zhArgs = @()
if ($ZHActive) { $zhArgs += @('-Active', $ZHActive) }

$plans = @{}
if ($runNative) {
    $r = Invoke-Tool $nativeTool $nativeArgs
    if ($r.Code -ne 0) { Write-Host $r.Text; Fail 'The native candidate plan failed.' }
    Write-Host '=== native candidates ==='; Write-Host $r.Text
    $plans['Native'] = Get-PlanHash $r.Text
}
if ($runZH) {
    $r = Invoke-Tool $zhTool $zhArgs
    if ($r.Code -ne 0) { Write-Host $r.Text; Fail 'The Zero Hour stage plan failed.' }
    Write-Host '=== Zero Hour stages ==='; Write-Host $r.Text
    $plans['ZH'] = Get-PlanHash $r.Text
}

Write-Host '=== report only (this tool never deletes these) ==='
foreach ($item in @(
        @('build\win32', 'regenerable: cmake --preset win32; cmake --build --preset win32'),
        @('build\win64', 'regenerable: cmake --preset win64; cmake --build --preset win64'),
        @('build\dev-runtimes\userdata-backups', 'snapshots of the user''s own game data - remove by hand after a hash comparison'),
        @('build\evidence-archive', 'canonical evidence - refused'),
        @('build\performance', 'canonical evidence - refused'))) {
    $gb = Get-TreeGB (Join-Path $repoRoot $item[0])
    Write-Host ('{0,-45} {1,8} GB  {2}' -f $item[0], $(if ($null -eq $gb) { 'absent' } else { '{0:N2}' -f $gb }), $item[1])
}
Write-Host ''
if ($plans.Count -gt 0) { foreach ($k in $plans.Keys) { Write-Host ("PLAN_HASH {0}: {1}" -f $k, $plans[$k]) } }

if (-not $Execute) {
    Write-Host 'Dry run only. To execute exactly these plans add -Execute with -ApprovedNativePlan / -ApprovedZHPlan (the hashes above) for every selected tool.'
    exit 0
}

# ---- execute: every selected tool needs its own reviewed hash, checked here AND again by the tool itself -------------------
if ($runNative -and (-not $ApprovedNativePlan -or $ApprovedNativePlan -ne $plans['Native'])) { Fail 'Refusing to execute: -ApprovedNativePlan missing or not equal to the current native plan hash.' }
if ($runZH -and (-not $ApprovedZHPlan -or $ApprovedZHPlan -ne $plans['ZH'])) { Fail 'Refusing to execute: -ApprovedZHPlan missing or not equal to the current Zero Hour plan hash.' }
if ($runNative) {
    $r = Invoke-Tool $nativeTool ($nativeArgs + @('-Execute', '-ApprovedPlan', $ApprovedNativePlan)); Write-Host $r.Text
    if ($r.Code -ne 0) { Fail 'Native cleanup reported a problem; stopping.' $r.Code }
}
if ($runZH) {
    $r = Invoke-Tool $zhTool ($zhArgs + @('-Execute', '-ApprovedPlan', $ApprovedZHPlan)); Write-Host $r.Text
    if ($r.Code -ne 0) { Fail 'Zero Hour cleanup reported a problem; stopping.' $r.Code }
}
Write-Host 'Cleanup complete.'
exit 0
