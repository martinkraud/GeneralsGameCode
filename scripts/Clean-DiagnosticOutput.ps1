#Requires -Version 7.0
<#
.SYNOPSIS
  Retention for the TEMPORARY diagnostic output in build\diag and build\audit-tmp (raw screenshots, frame-delivery and benchmark run directories, scratch zips).

.DESCRIPTION
  DRY RUN BY DEFAULT (-Execute removes). Only DIRECT children of <repo>\build\diag and <repo>\build\audit-tmp are ever candidates; nothing else is touched (no source, docs, Git, Steam,
  dev-runtimes, win32/win64, evidence-archive, performance).

  Policy (scripts\diag-retention.json):
    rawCapturePatterns / rawCaptureDays   bulky raw captures (screenshots) are removed after this many days
    runOutputDays                         everything else is removed after this many days
    protect                               names that are never removed
  Never removed regardless of age: a protected name; a directory that contains a file named KEEP; a directory that contains a crash dump (*.dmp, *.mdmp) - crash evidence is only ever
  removed by hand after investigation; reparse points (reported as skipped).
  Permanent evidence belongs in docs\ (summaries, tables, digests), not in these directories: summarise first, then let the retention remove the raw output.
#>
[CmdletBinding()]
param([string]$BuildRoot,
      [string]$PolicyFile,
      [switch]$Execute)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
function Fail([string]$message, [int]$code = 1) { Write-Error $message -ErrorAction Continue; exit $code }
function Test-SteamPath([string]$p) { return ($p -match '(?i)[\\/]steamapps[\\/]') -or ($p -match '(?i)[\\/]Steam[\\/]') -or ($p -match '(?i)Program Files \(x86\)[\\/]Steam') }

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $BuildRoot) { $BuildRoot = Join-Path $repoRoot 'build' }
if (-not $PolicyFile) { $PolicyFile = Join-Path $PSScriptRoot 'diag-retention.json' }
if (-not (Test-Path -LiteralPath $BuildRoot -PathType Container)) { Fail "Missing build directory: $BuildRoot" }
$BuildRoot = (Resolve-Path -LiteralPath $BuildRoot).Path.TrimEnd('\')
if ((Split-Path $BuildRoot -Leaf) -ne 'build') { Fail "BuildRoot must be a directory named build (got $BuildRoot)" }
if (Test-SteamPath $BuildRoot) { Fail "Refusing a Steam-looking location: $BuildRoot" }
$policy = Get-Content -LiteralPath $PolicyFile -Raw | ConvertFrom-Json
$rawDays = [double]$policy.rawCaptureDays; $runDays = [double]$policy.runOutputDays
if ($rawDays -lt 1 -or $runDays -lt 1) { Fail 'Retention days must be at least 1.' }
$protect = @($policy.protect); $rawPatterns = @($policy.rawCapturePatterns)

function Get-LatestWrite([System.IO.FileSystemInfo]$item) {
    if (-not $item.PSIsContainer) { return $item.LastWriteTimeUtc }
    $latest = $item.LastWriteTimeUtc
    foreach ($f in Get-ChildItem -LiteralPath $item.FullName -Recurse -File -Force -ErrorAction SilentlyContinue) { if ($f.LastWriteTimeUtc -gt $latest) { $latest = $f.LastWriteTimeUtc } }
    return $latest
}
function Get-TreeBytes([System.IO.FileSystemInfo]$item) {
    if (-not $item.PSIsContainer) { return $item.Length }
    [int64]$sum = 0
    foreach ($f in Get-ChildItem -LiteralPath $item.FullName -Recurse -File -Force -ErrorAction SilentlyContinue) { $sum += $f.Length }
    return $sum
}

Write-Host ('Diagnostic output retention - ' + $(if ($Execute) { 'EXECUTE' } else { 'DRY RUN (nothing will be changed)' }))
Write-Host "BuildRoot: $BuildRoot | raw captures after $rawDays d, other output after $runDays d"
$now = [DateTime]::UtcNow
[int64]$total = 0; $removed = 0; $kept = 0
foreach ($rootName in @('diag', 'audit-tmp')) {
    $root = Join-Path $BuildRoot $rootName
    if (-not (Test-Path -LiteralPath $root -PathType Container)) { continue }
    foreach ($c in Get-ChildItem -LiteralPath $root -Force) {
        $key = "$rootName\$($c.Name)"
        $reason = $null
        if ($c.Attributes -band [IO.FileAttributes]::ReparsePoint) { $reason = 'skipped: reparse point' }
        elseif (($protect -contains $c.Name) -or ($protect -contains $key)) { $reason = 'kept: protected' }
        elseif ($c.PSIsContainer -and (Test-Path -LiteralPath (Join-Path $c.FullName 'KEEP'))) { $reason = 'kept: KEEP marker' }
        elseif ($c.PSIsContainer -and @(Get-ChildItem -LiteralPath $c.FullName -Recurse -File -Force -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in '.dmp', '.mdmp' }).Count) { $reason = 'kept: contains a crash dump' }
        if ($reason) { ++$kept; if ($reason -notlike 'kept: protected') { Write-Host ("  {0,-70} {1}" -f $key, $reason) }; continue }
        $isRaw = $false; foreach ($p in $rawPatterns) { if ($c.Name -like $p) { $isRaw = $true } }
        $limit = if ($isRaw) { $rawDays } else { $runDays }
        $ageDays = ($now - (Get-LatestWrite $c)).TotalDays
        if ($ageDays -lt $limit) { ++$kept; continue }
        $bytes = Get-TreeBytes $c
        $total += $bytes; ++$removed
        Write-Host ("  {0,-70} {1,8:N0} MB  {2:N0} d old  {3}" -f $key, ($bytes / 1MB), $ageDays, $(if ($Execute) { 'removing' } else { 'would remove' }))
        if ($Execute) { Remove-Item -LiteralPath $c.FullName -Recurse -Force }
    }
}
Write-Host ('{0} {1} item(s), {2:N2} GB; kept {3}' -f $(if ($Execute) { 'removed' } else { 'would remove' }), $removed, ($total / 1GB), $kept)
