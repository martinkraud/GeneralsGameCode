<#
.SYNOPSIS
  Conservative retention/cleanup of staged native candidates (build\dev-runtimes\native\<Title>\<name>).

.DESCRIPTION
  DRY RUN BY DEFAULT: nothing is deleted or copied unless -Execute is given AND -ApprovedPlan matches the plan hash that
  the dry run printed (so what runs is exactly what was reviewed).

  Decisions per candidate:
    KEEP       protected / active / latest accepted / among the newest N per title / ambiguous (fails closed)
    ARCHIVE    superseded, has run evidence (runs\, normal-launch\): evidence is copied to the archive root, verified
               (file count + bytes), and only then is the candidate removed
    REMOVE     superseded, no run evidence: manifest.json/ordinaryplay.json are archived, then the candidate is removed
    AMBIGUOUS  unexpected entries, reparse points, unexpected name, outside the root: treated as KEEP, reported

  Never touched: source, Git, build trees, Steam, any directory outside <NativeRoot>\<Title>\<candidate>. Never uses
  git clean. Candidate payloads (retail .big assets, executables) are regenerable and are not archived.

.EXAMPLE
  pwsh scripts\native-runtime\Cleanup-NativeCandidates.ps1 -Active x64-36-attribution -LatestAccepted x64-34-smooth-presentation
  (dry run; prints the plan and PLAN_HASH)
  pwsh scripts\native-runtime\Cleanup-NativeCandidates.ps1 -Active ... -LatestAccepted ... -Execute -ApprovedPlan <hash>
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string]$NativeRoot,
    [string]$ArchiveRoot,
    [string[]]$Protect = @(),
    [string]$Active,
    [string]$LatestAccepted,
    [int]$KeepNewest = -1,
    [string]$PolicyFile,
    [string[]]$AllowedEntry = @(),
    [switch]$Execute,
    [string]$ApprovedPlan
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $NativeRoot) { $NativeRoot = Join-Path $repoRoot 'build\dev-runtimes\native' }
if (-not $ArchiveRoot) { $ArchiveRoot = Join-Path $repoRoot 'build\evidence-archive\native-candidates' }
if (-not $PolicyFile) { $PolicyFile = Join-Path $PSScriptRoot 'native-retention.json' }

$evidenceDirectories = @('runs', 'normal-launch')
$evidenceFiles = @('manifest.json', 'ordinaryplay.json', 'graphics.json')   # small stage metadata, archived with the evidence
$knownEntries = @('game') + $evidenceDirectories + $evidenceFiles + $AllowedEntry
$namePattern = '^x64-[0-9]{1,3}[A-Za-z0-9._-]*$'

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
function Get-TreeTotals([string]$path) {
    [int64]$bytes = 0; [int64]$count = 0
    foreach ($f in (Get-ChildItem -LiteralPath $path -Recurse -Force -File -ErrorAction SilentlyContinue)) { $bytes += $f.Length; ++$count }
    return [pscustomobject]@{ Bytes = $bytes; Count = $count }
}

# Pure classification; no filesystem writes. Returns one object per candidate.
function Get-CandidatePlan {
    param([string]$Root, [string[]]$ProtectNames, [string]$ActiveName, [string]$AcceptedName, [int]$Newest,
          [string[]]$Known, [string]$Pattern)
    $plan = @()
    foreach ($titleDir in (Get-ChildItem -LiteralPath $Root -Directory -Force)) {
        $candidates = @(Get-ChildItem -LiteralPath $titleDir.FullName -Directory -Force | Sort-Object LastWriteTime -Descending)
        $newestNames = @($candidates | Select-Object -First $Newest | ForEach-Object { $_.Name })
        foreach ($c in $candidates) {
            $key = '{0}\{1}' -f $titleDir.Name, $c.Name
            $reason = $null
            $decision = $null
            $ambiguous = @()
            if ($c.Name -notmatch $Pattern) { $ambiguous += 'unexpected candidate name' }
            if (Test-SteamPath $c.FullName) { $ambiguous += 'path looks like a Steam location' }
            if ($c.Attributes -band [IO.FileAttributes]::ReparsePoint) { $ambiguous += 'candidate directory is a reparse point' }
            foreach ($entry in (Get-ChildItem -LiteralPath $c.FullName -Force)) {
                if ($Known -notcontains $entry.Name) { $ambiguous += "unexpected entry '$($entry.Name)'" }
                if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { $ambiguous += "reparse point '$($entry.Name)'" }
            }
            $links = @(Get-ChildItem -LiteralPath $c.FullName -Recurse -Force -Attributes ReparsePoint -ErrorAction SilentlyContinue)
            if ($links.Count -gt 0) { $ambiguous += "$($links.Count) reparse point(s) inside" }
            $hasEvidence = $false
            foreach ($d in $evidenceDirectories) {
                if (Test-Path -LiteralPath (Join-Path $c.FullName $d)) { $hasEvidence = $true }
            }
            $protectedByName = ($ProtectNames -contains $c.Name) -or ($ProtectNames -contains $key)
            if ($ambiguous.Count -gt 0) { $decision = 'AMBIGUOUS'; $reason = ($ambiguous | Select-Object -Unique) -join '; ' }
            elseif ($protectedByName) { $decision = 'KEEP'; $reason = 'protected' }
            elseif ($ActiveName -and $c.Name -eq $ActiveName) { $decision = 'KEEP'; $reason = 'active candidate' }
            elseif ($AcceptedName -and $c.Name -eq $AcceptedName) { $decision = 'KEEP'; $reason = 'latest accepted' }
            elseif ($newestNames -contains $c.Name) { $decision = 'KEEP'; $reason = "among newest $Newest of $($titleDir.Name)" }
            elseif ($hasEvidence) { $decision = 'ARCHIVE'; $reason = 'superseded; run evidence archived first' }
            else { $decision = 'REMOVE'; $reason = 'superseded; no run evidence (manifest archived)' }
            $plan += [pscustomobject]@{
                Key = $key; Title = $titleDir.Name; Name = $c.Name; Path = $c.FullName
                Decision = $decision; Reason = $reason; HasEvidence = $hasEvidence
                Bytes = [int64]0
            }
        }
    }
    return $plan
}

# ---- guards ---------------------------------------------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $NativeRoot -PathType Container)) { Fail "NativeRoot does not exist: $NativeRoot" }
$NativeRoot = (Resolve-Path -LiteralPath $NativeRoot).Path.TrimEnd('\')
if (Test-SteamPath $NativeRoot) { Fail "Refusing a Steam-looking root: $NativeRoot" }
if ((Split-Path $NativeRoot -Leaf) -ne 'native' -or (Split-Path (Split-Path $NativeRoot -Parent) -Leaf) -ne 'dev-runtimes') {
    Fail "NativeRoot must end with dev-runtimes\native (got $NativeRoot)"
}
$ArchiveFull = [IO.Path]::GetFullPath($ArchiveRoot).TrimEnd('\')
if (Test-SteamPath $ArchiveFull) { Fail "Refusing a Steam-looking archive root: $ArchiveFull" }
if ($ArchiveFull.StartsWith($NativeRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or $ArchiveFull -eq $NativeRoot) {
    Fail 'The archive root must not be inside the native root.'
}

$policyProtect = @()
$policyNewest = 2
if (Test-Path -LiteralPath $PolicyFile) {
    $policy = Get-Content -LiteralPath $PolicyFile -Raw | ConvertFrom-Json
    if ($policy.PSObject.Properties.Name -contains 'protect') { $policyProtect = @($policy.protect) }
    if ($policy.PSObject.Properties.Name -contains 'keepNewestPerTitle') { $policyNewest = [int]$policy.keepNewestPerTitle }
}
if ($KeepNewest -lt 0) { $KeepNewest = $policyNewest }
if ($KeepNewest -lt 1) { Fail 'KeepNewest must be at least 1.' }
$allProtect = @($policyProtect + $Protect | Where-Object { $_ })

# ---- plan -----------------------------------------------------------------------------------------------------------
$plan = @(Get-CandidatePlan -Root $NativeRoot -ProtectNames $allProtect -ActiveName $Active -AcceptedName $LatestAccepted `
    -Newest $KeepNewest -Known $knownEntries -Pattern $namePattern)
foreach ($item in $plan) { $item.Bytes = Get-DirectorySize $item.Path }

$mode = if ($Execute) { 'EXECUTE' } else { 'DRY RUN (nothing will be changed)' }
Write-Host "Native candidate cleanup - $mode"
Write-Host "NativeRoot : $NativeRoot"
Write-Host "ArchiveRoot: $ArchiveFull"
Write-Host ("KeepNewest : {0}; protected: {1}; active: {2}; latest accepted: {3}" -f $KeepNewest, ($allProtect -join ', '), $Active, $LatestAccepted)
Write-Host ''
foreach ($item in ($plan | Sort-Object Title, Name)) {
    Write-Host ("{0,-9} {1,-52} {2,9:N2} GB  {3}" -f $item.Decision, $item.Key, ($item.Bytes / 1GB), $item.Reason)
}
$remove = @($plan | Where-Object { $_.Decision -eq 'ARCHIVE' -or $_.Decision -eq 'REMOVE' })
$keep = @($plan | Where-Object { $_.Decision -eq 'KEEP' -or $_.Decision -eq 'AMBIGUOUS' })
[int64]$reclaim = 0
foreach ($r in $remove) { $reclaim += $r.Bytes }
Write-Host ''
Write-Host ("Candidates: {0}; to remove: {1}; kept: {2}; estimated reclaim: {3:N2} GB" -f $plan.Count, $remove.Count, $keep.Count, ($reclaim / 1GB))

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

# ---- execute: archive, verify, then remove ----------------------------------------------------------------------------
$failures = 0
foreach ($item in $remove) {
    $candidatePath = $item.Path
    if (-not $candidatePath.StartsWith($NativeRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { Fail "Path escapes the root: $candidatePath" 2 }
    $destination = Join-Path $ArchiveFull $item.Key
    $verified = $true
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    foreach ($file in $evidenceFiles) {
        $src = Join-Path $candidatePath $file
        if (Test-Path -LiteralPath $src) { Copy-Item -LiteralPath $src -Destination $destination -Force }
    }
    foreach ($dir in $evidenceDirectories) {
        $src = Join-Path $candidatePath $dir
        if (-not (Test-Path -LiteralPath $src)) { continue }
        $dst = Join-Path $destination $dir
        Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force
        $a = Get-TreeTotals $src
        $b = Get-TreeTotals $dst
        if ($a.Count -ne $b.Count -or $a.Bytes -ne $b.Bytes) { $verified = $false }
    }
    foreach ($file in $evidenceFiles) {
        $src = Join-Path $candidatePath $file
        if ((Test-Path -LiteralPath $src) -and -not (Test-Path -LiteralPath (Join-Path $destination $file))) { $verified = $false }
    }
    if (-not $verified) {
        Write-Host "ARCHIVE VERIFICATION FAILED, candidate kept: $($item.Key)"
        $failures++
        continue
    }
    if ($PSCmdlet.ShouldProcess($candidatePath, 'Remove superseded native candidate')) {
        Remove-Item -LiteralPath $candidatePath -Recurse -Force
        Write-Host ("REMOVED   {0} ({1:N2} GB)" -f $item.Key, ($item.Bytes / 1GB))
    }
}
if ($failures) { exit 2 }
Write-Host 'Cleanup complete.'
exit 0
