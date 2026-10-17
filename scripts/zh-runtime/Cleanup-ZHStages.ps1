<#
.SYNOPSIS
  Conservative retention/cleanup of staged Zero Hour (Win32) development runtimes
  (build\dev-runtimes\zh\<stage>), including the transient workflow-check-* test stages.

.DESCRIPTION
  Why: every stage carries a full copy of the retail Zero Hour assets (~2.9 GB). 23 stages accumulated ~66 GB without any
  retention mechanism. This tool is the Zero Hour counterpart of scripts\native-runtime\Cleanup-NativeCandidates.ps1.

  DRY RUN BY DEFAULT: nothing is copied or deleted unless -Execute is given AND -ApprovedPlan matches the plan hash that the
  dry run printed (so what runs is exactly what was reviewed).

  Decisions per directory under the zh root:
    KEEP       protected (policy file / -Protect), -Active, or among the newest N stages (default policy); AMBIGUOUS (fails closed)
    REMOVE     a superseded stage: its small metadata files (manifest.json, launch receipts, ...) are archived and verified
               first, then the directory is removed
    REMOVE     a workflow-check-<12 hex> directory (leftover of Test-ZHWorkflow.ps1, recreated on every test run; nothing archived)
    AMBIGUOUS  unexpected name, unexpected top-level entry, reparse point (junction/symlink) anywhere inside, Steam-looking path

  Never touched: source, Git, build trees, evidence-archive, performance, userdata-backups, native candidates, the Steam/EA
  installation, any directory outside <ZHRoot>\<name>. Never uses git clean. The retail assets inside a stage are copies and are
  re-staged from the installation with scripts\zh-runtime\Stage-ZHRuntime.ps1; the stage executables are rebuilt from the commit
  recorded in the stage's manifest.json (Revision).

.EXAMPLE
  pwsh scripts\zh-runtime\Cleanup-ZHStages.ps1
  (dry run; prints the plan and PLAN_HASH)
  pwsh scripts\zh-runtime\Cleanup-ZHStages.ps1 -Execute -ApprovedPlan <hash>
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string]$ZHRoot,
    [string]$ArchiveRoot,
    [string[]]$Protect = @(),
    [string]$Active,
    [int]$KeepNewest = -1,
    [string]$PolicyFile,
    [switch]$Execute,
    [string]$ApprovedPlan
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $ZHRoot) { $ZHRoot = Join-Path $repoRoot 'build\dev-runtimes\zh' }
if (-not $ArchiveRoot) { $ArchiveRoot = Join-Path $repoRoot 'build\evidence-archive\zh-stages' }
if (-not $PolicyFile) { $PolicyFile = Join-Path $PSScriptRoot 'zh-retention.json' }

$stageNamePattern = '^(baseline|stage[0-9]+[A-Za-z0-9._-]*)$'
$workflowNamePattern = '^workflow-check-[0-9a-f]{12}$'
$metadataPatterns = @('manifest.json', 'runtime-files.json', 'source-files.json', 'launch-*.json', 'launch-*.json.exit.json')
$knownEntries = @('game')          # everything else must match a metadata pattern or the directory is AMBIGUOUS

function Fail([string]$message, [int]$code = 1) {
    Write-Error $message -ErrorAction Continue
    exit $code
}
function Test-SteamPath([string]$path) {
    return ($path -match '(?i)[\\/]steamapps[\\/]') -or ($path -match '(?i)[\\/]Steam[\\/]') -or ($path -match '(?i)Program Files \(x86\)[\\/]Steam')
}
function Get-DirectorySize([string]$path) {
    [int64]$sum = 0
    foreach ($f in (Get-ChildItem -LiteralPath $path -Recurse -Force -File -ErrorAction SilentlyContinue)) { $sum += $f.Length }
    return $sum
}
function Test-MetadataName([string]$name) {
    foreach ($p in $metadataPatterns) { if ($name -like $p) { return $true } }
    return $false
}

# Pure classification; no filesystem writes.
function Get-StagePlan {
    param([string]$Root, [string[]]$ProtectNames, [string]$ActiveName, [int]$Newest)
    $plan = @()
    $dirs = @(Get-ChildItem -LiteralPath $Root -Directory -Force | Sort-Object LastWriteTime -Descending)
    $stageDirs = @($dirs | Where-Object { $_.Name -match $stageNamePattern })
    $newestNames = @($stageDirs | Select-Object -First $Newest | ForEach-Object { $_.Name })
    foreach ($d in $dirs) {
        $isWorkflow = $d.Name -match $workflowNamePattern
        $isStage = $d.Name -match $stageNamePattern
        $ambiguous = @()
        if (-not ($isWorkflow -or $isStage)) { $ambiguous += 'unexpected directory name' }
        if (Test-SteamPath $d.FullName) { $ambiguous += 'path looks like a Steam location' }
        if ($d.Attributes -band [IO.FileAttributes]::ReparsePoint) { $ambiguous += 'directory is a reparse point' }
        foreach ($entry in (Get-ChildItem -LiteralPath $d.FullName -Force)) {
            if (($knownEntries -notcontains $entry.Name) -and -not (Test-MetadataName $entry.Name)) { $ambiguous += "unexpected entry '$($entry.Name)'" }
        }
        $links = @(Get-ChildItem -LiteralPath $d.FullName -Recurse -Force -Attributes ReparsePoint -ErrorAction SilentlyContinue)
        if ($links.Count -gt 0) { $ambiguous += "$($links.Count) reparse point(s) inside" }
        if ($ambiguous.Count -gt 0) { $decision = 'AMBIGUOUS'; $reason = ($ambiguous | Select-Object -Unique) -join '; ' }
        elseif ($isWorkflow) { $decision = 'REMOVE'; $reason = 'workflow-check test leftover (recreated by Test-ZHWorkflow.ps1)' }
        elseif ($ProtectNames -contains $d.Name) { $decision = 'KEEP'; $reason = 'protected' }
        elseif ($ActiveName -and $d.Name -eq $ActiveName) { $decision = 'KEEP'; $reason = 'active stage' }
        elseif ($newestNames -contains $d.Name) { $decision = 'KEEP'; $reason = "among newest $Newest stages" }
        else { $decision = 'REMOVE'; $reason = 'superseded stage; metadata archived first' }
        $plan += [pscustomobject]@{ Key = $d.Name; Path = $d.FullName; Decision = $decision; Reason = $reason; Workflow = $isWorkflow; Bytes = [int64]0 }
    }
    return $plan
}

# ---- guards ---------------------------------------------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $ZHRoot -PathType Container)) { Fail "ZHRoot does not exist: $ZHRoot" }
$ZHRoot = (Resolve-Path -LiteralPath $ZHRoot).Path.TrimEnd('\')
if (Test-SteamPath $ZHRoot) { Fail "Refusing a Steam-looking root: $ZHRoot" }
if ((Split-Path $ZHRoot -Leaf) -ne 'zh' -or (Split-Path (Split-Path $ZHRoot -Parent) -Leaf) -ne 'dev-runtimes') {
    Fail "ZHRoot must end with dev-runtimes\zh (got $ZHRoot)"
}
$ArchiveFull = [IO.Path]::GetFullPath($ArchiveRoot).TrimEnd('\')
if (Test-SteamPath $ArchiveFull) { Fail "Refusing a Steam-looking archive root: $ArchiveFull" }
if ($ArchiveFull.StartsWith($ZHRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or $ArchiveFull -eq $ZHRoot) { Fail 'The archive root must not be inside the zh root.' }
foreach ($refused in @('\build\performance', '\userdata-backups', '\docs\', '\Core\', '\Generals\', '\GeneralsMD\', '\scripts\', '\Tests\')) {
    if (($ArchiveFull + '\') -like ('*' + $refused + '*')) { Fail "Refusing archive root $ArchiveFull" }
}

$policyProtect = @()
$policyNewest = 2
if (Test-Path -LiteralPath $PolicyFile) {
    $policy = Get-Content -LiteralPath $PolicyFile -Raw | ConvertFrom-Json
    if ($policy.PSObject.Properties.Name -contains 'protect') { $policyProtect = @($policy.protect) }
    if ($policy.PSObject.Properties.Name -contains 'keepNewestStages') { $policyNewest = [int]$policy.keepNewestStages }
}
if ($KeepNewest -lt 0) { $KeepNewest = $policyNewest }
if ($KeepNewest -lt 1) { Fail 'KeepNewest must be at least 1.' }
$allProtect = @($policyProtect + $Protect | Where-Object { $_ })

# ---- plan -----------------------------------------------------------------------------------------------------------
$plan = @(Get-StagePlan -Root $ZHRoot -ProtectNames $allProtect -ActiveName $Active -Newest $KeepNewest)
foreach ($item in $plan) { $item.Bytes = Get-DirectorySize $item.Path }

$mode = if ($Execute) { 'EXECUTE' } else { 'DRY RUN (nothing will be changed)' }
Write-Host "Zero Hour stage cleanup - $mode"
Write-Host "ZHRoot     : $ZHRoot"
Write-Host "ArchiveRoot: $ArchiveFull"
Write-Host ("KeepNewest : {0}; protected: {1}; active: {2}" -f $KeepNewest, ($allProtect -join ', '), $Active)
Write-Host ''
foreach ($item in ($plan | Sort-Object Key)) {
    Write-Host ("{0,-9} {1,-52} {2,9:N2} GB  {3}" -f $item.Decision, $item.Key, ($item.Bytes / 1GB), $item.Reason)
}
$remove = @($plan | Where-Object { $_.Decision -eq 'REMOVE' })
$keep = @($plan | Where-Object { $_.Decision -ne 'REMOVE' })
[int64]$reclaim = 0
foreach ($r in $remove) { $reclaim += $r.Bytes }
Write-Host ''
Write-Host ("Directories: {0}; to remove: {1}; kept: {2}; estimated reclaim: {3:N2} GB" -f $plan.Count, $remove.Count, $keep.Count, ($reclaim / 1GB))

$planText = ($plan | Sort-Object Key | ForEach-Object { '{0}|{1}|{2}' -f $_.Decision, $_.Key, $_.Bytes }) -join "`n"
$sha = [Security.Cryptography.SHA256]::Create()
$planHash = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($planText)))).Replace('-', '')
Write-Host "PLAN_HASH: $planHash"

if (-not $Execute) {
    Write-Host 'Dry run only. To execute exactly this plan: add -Execute -ApprovedPlan <PLAN_HASH>.'
    exit 0
}
if (-not $ApprovedPlan -or $ApprovedPlan -ne $planHash) {
    Fail 'Refusing to execute: -ApprovedPlan does not match the current plan hash (re-run the dry run and review it).'
}

# ---- execute: archive metadata, verify, then remove -----------------------------------------------------------------------
$failures = 0
foreach ($item in $remove) {
    $path = $item.Path
    if (-not $path.StartsWith($ZHRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { Fail "Path escapes the root: $path" 2 }
    if (-not $item.Workflow) {
        $destination = Join-Path $ArchiveFull $item.Key
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        $sources = @(Get-ChildItem -LiteralPath $path -File -Force | Where-Object { Test-MetadataName $_.Name })
        foreach ($s in $sources) { Copy-Item -LiteralPath $s.FullName -Destination $destination -Force }
        $copied = @(Get-ChildItem -LiteralPath $destination -File -Force)
        [int64]$a = 0; foreach ($s in $sources) { $a += $s.Length }
        [int64]$b = 0; foreach ($c in $copied) { $b += $c.Length }
        if ($copied.Count -lt $sources.Count -or $b -lt $a) {
            Write-Host "METADATA ARCHIVE VERIFICATION FAILED, stage kept: $($item.Key)"
            $failures++
            continue
        }
    }
    if ($PSCmdlet.ShouldProcess($path, 'Remove superseded Zero Hour stage')) {
        Remove-Item -LiteralPath $path -Recurse -Force
        Write-Host ("REMOVED   {0} ({1:N2} GB)" -f $item.Key, ($item.Bytes / 1GB))
    }
}
if ($failures) { exit 2 }
Write-Host 'Cleanup complete.'
exit 0
