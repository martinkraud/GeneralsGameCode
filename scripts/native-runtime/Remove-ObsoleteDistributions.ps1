#Requires -Version 7.0
# DISTRIBUTION cleanup: removes the generated artifacts of OBSOLETE MKRaudstein distributions, keeping the current one and the immediately previous
# successfully validated one. Called by New-MKRaudsteinDistribution.ps1 only after both titles were built, packaged and validated.
#
# Provenance rules (anything that does not satisfy ALL of them is left untouched):
#   * a distribution exists only if its record MKRaudstein-x64-Preview-<version>.distribution.json is in the output directory, parses, has validated=true for every title
#     and names the same version (the record is written by the pipeline only after validation);
#   * a file is removed only when its name is exactly one of the names the pipeline generates for that version (the set below), directly inside the output directory;
#   * a staged candidate is removed only when it is named by an obsolete record or matches dist-<hex>-(gen|zh), lives directly in <stage root>\<Generals|ZeroHour>,
#     is a real directory (no reparse point) and carries the stager's ordinaryplay.json marker; staged candidates of retained distributions are kept.
# Nothing outside the output directory and the two stage folders is ever touched. -DryRun lists and deletes nothing.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory,
      [Parameter(Mandatory)][string]$StageRoot,         # ...\build\dev-runtimes\native
      [Parameter(Mandatory)][string]$CurrentVersion,
      [int]$Keep = 2,
      [switch]$DryRun)
$ErrorActionPreference = 'Stop'
if ($Keep -lt 2) { throw 'The retention policy keeps at least the current and the previous distribution.' }
if (-not (Test-Path -LiteralPath $OutputDirectory -PathType Container)) { Write-Host "cleanup: no output directory, nothing to do."; return }
$out = (Resolve-Path -LiteralPath $OutputDirectory).Path
$titles = @('Generals', 'ZeroHour')

# 1. Known, validated distributions (version -> record).
$records = @{}
foreach ($f in Get-ChildItem -LiteralPath $out -File -Filter 'MKRaudstein-x64-Preview-*.distribution.json') {
    if ($f.Name -notmatch '^MKRaudstein-x64-Preview-([0-9a-f]{7,40})\.distribution\.json$') { continue }
    $ver = $Matches[1]
    try { $j = Get-Content -LiteralPath $f.FullName -Raw | ConvertFrom-Json } catch { Write-Host "cleanup: unreadable record $($f.Name) left alone"; continue }
    $ts = @($j.titles)
    if ($j.version -ne $ver -or $ts.Count -ne 2 -or @($ts | Where-Object { $_.validated -ne $true }).Count) { Write-Host "cleanup: record $($f.Name) is not a fully validated record, left alone"; continue }
    $records[$ver] = [pscustomobject]@{ Version = $ver; Time = $f.LastWriteTimeUtc; Titles = $ts }
}
if (-not $records.ContainsKey($CurrentVersion)) { Write-Host "cleanup: the current distribution $CurrentVersion has no validated record; nothing is deleted."; return }

# 2. Retention: the current one plus the most recently written others.
$ordered = @($records.Values | Where-Object { $_.Version -ne $CurrentVersion } | Sort-Object Time -Descending)
$keepVersions = @($CurrentVersion) + @($ordered | Select-Object -First ($Keep - 1) | ForEach-Object { $_.Version })
$obsolete = @($ordered | Where-Object { $keepVersions -notcontains $_.Version })
# Older distributions that never got a record (pre-record pipeline versions, interrupted runs) are identified by the exact generated file names only.
$legacy = @(Get-ChildItem -LiteralPath $out -File | ForEach-Object {
        if ($_.Name -match '^MKRaudstein-(?:Generals|ZeroHour)-x64-Preview-([0-9a-f]{7,40})(?:\.zip|\.zip\.sha256|-Symbols\.zip|\.manifest\.json|\.overlay-test\.json|\.stage-parity\.json)$' -or
            $_.Name -match '^MKRaudstein-x64-Preview-([0-9a-f]{7,40})\.(?:SHA256SUMS|distribution\.json)$') { $Matches[1] } } |
    Where-Object { $keepVersions -notcontains $_ -and -not $records.ContainsKey($_) } | Sort-Object -Unique |
    ForEach-Object { [pscustomobject]@{ Version = $_; Time = [datetime]::MinValue; Titles = @() } })
$obsolete = @($obsolete) + $legacy
Write-Host "cleanup: keeping $($keepVersions -join ', '); obsolete: $(if ($obsolete) { (@($obsolete | ForEach-Object { $_.Version }) -join ', ') } else { 'none' })"

$removed = [System.Collections.Generic.List[string]]::new()
function Remove-Generated([string]$path, [bool]$directory) {
    if ($DryRun) { Write-Host "  would remove $path"; $removed.Add($path); return }
    if ($directory) { Remove-Item -LiteralPath $path -Recurse -Force } else { Remove-Item -LiteralPath $path -Force }
    Write-Host "  removed $path"; $removed.Add($path)
}

# 3. Final artifacts of obsolete versions: exact generated names only.
foreach ($r in $obsolete) {
    $names = @("MKRaudstein-x64-Preview-$($r.Version).distribution.json", "MKRaudstein-x64-Preview-$($r.Version).SHA256SUMS")
    foreach ($t in $titles) {
        $p = "MKRaudstein-$t-x64-Preview-$($r.Version)"
        $names += @("$p.zip", "$p.zip.sha256", "$p-Symbols.zip", "$p.manifest.json", "$p.overlay-test.json", "$p.stage-parity.json")
    }
    foreach ($n in $names) {
        $full = Join-Path $out $n
        $item = Get-Item -LiteralPath $full -Force -ErrorAction SilentlyContinue
        if ($item -and -not $item.PSIsContainer -and -not $item.LinkType) { Remove-Generated $full $false }
    }
}

# 4. Staged candidates: those named by obsolete records, plus orphaned dist-<hex>-(gen|zh) stages that are not retained.
$keepCandidates = @($keepVersions | ForEach-Object { $records[$_].Titles } | ForEach-Object { $_.candidate })
if (Test-Path -LiteralPath $StageRoot -PathType Container) {
    foreach ($t in $titles) {
        $dir = Join-Path $StageRoot $t
        if (-not (Test-Path -LiteralPath $dir -PathType Container)) { continue }
        foreach ($c in Get-ChildItem -LiteralPath $dir -Directory -Force -Filter 'dist-*') {
            if ($c.Name -notmatch '^dist-[0-9a-f]{7,40}-(gen|zh)$') { continue }
            if ($c.LinkType -or ($c.Attributes -band [IO.FileAttributes]::ReparsePoint)) { continue }
            if ($keepCandidates -contains $c.Name) { continue }
            if (-not (Test-Path -LiteralPath (Join-Path $c.FullName 'ordinaryplay.json') -PathType Leaf)) { Write-Host "cleanup: $($c.FullName) has no stager marker, left alone"; continue }
            Remove-Generated $c.FullName $true
        }
    }
}
Write-Host "cleanup: $(if ($DryRun) { 'would remove' } else { 'removed' }) $($removed.Count) item(s)."
