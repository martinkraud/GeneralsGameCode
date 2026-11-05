#Requires -Version 7.0
<#
.SYNOPSIS
  Shares the identical immutable assets (.big, .bik) of all staged runtimes through one content-addressed store of NTFS hard links, and prunes unused store entries.

.DESCRIPTION
  DRY RUN BY DEFAULT. Nothing is changed without -Execute. No file content is ever deleted: a duplicate stage file is replaced by a hard link to a store entry whose SHA-256 equals the
  file's SHA-256 (verified before the replacement; the link is created under a temporary name and moved over the original). A stage therefore looks and hashes exactly as before.

  Scope (anything else is never touched): files with extension .big / .bik of at least 1 MB inside  <DevRuntimes>\native  and  <DevRuntimes>\zh  (below a "game" directory),
  and the store  <DevRuntimes>\asset-store.  DevRuntimes must be a directory named "dev-runtimes", not a drive root and not a Steam location. Reparse points are skipped.
  Executables, PDBs, INI/WND overrides and everything else stay ordinary files (they are rewritten in place by -Refresh style workflows).

  -PruneStore removes store entries that no stage links to (link count 1), which is what returns the space after stages were deleted.

.EXAMPLE
  pwsh scripts\native-runtime\Optimize-StageStorage.ps1                       # report what would be shared
  pwsh scripts\native-runtime\Optimize-StageStorage.ps1 -Execute              # share it
  pwsh scripts\native-runtime\Optimize-StageStorage.ps1 -PruneStore -Execute  # drop store entries no stage uses
#>
[CmdletBinding()]
param([string]$DevRuntimes,
      [switch]$PruneStore,
      [switch]$Execute)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
. (Join-Path $PSScriptRoot 'AssetStore.ps1')

function Fail([string]$message, [int]$code = 1) { Write-Error $message -ErrorAction Continue; exit $code }

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $DevRuntimes) { $DevRuntimes = Join-Path $repoRoot 'build\dev-runtimes' }
if (-not (Test-Path -LiteralPath $DevRuntimes -PathType Container)) { Fail "DevRuntimes does not exist: $DevRuntimes" }
$DevRuntimes = (Resolve-Path -LiteralPath $DevRuntimes).Path.TrimEnd('\')
if ((Split-Path $DevRuntimes -Leaf) -ne 'dev-runtimes') { Fail "DevRuntimes must be a directory named dev-runtimes (got $DevRuntimes)" }
if ($DevRuntimes.Length -le 3 -or $null -eq (Split-Path $DevRuntimes -Parent)) { Fail 'Refusing a root directory.' }
if (Test-AssetStoreSteamPath $DevRuntimes) { Fail "Refusing a Steam-looking location: $DevRuntimes" }
if ((Get-Item -LiteralPath $DevRuntimes -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { Fail 'Refusing a reparse point.' }

$store = Get-AssetStoreRoot $DevRuntimes
$mode = if ($Execute) { 'EXECUTE' } else { 'DRY RUN (nothing will be changed)' }
Write-Host "Stage storage optimisation - $mode"
Write-Host "DevRuntimes: $DevRuntimes"

if ($PruneStore) {
    $removed = 0; [int64]$bytes = 0
    if (Test-Path -LiteralPath $store -PathType Container) {
        foreach ($e in Get-ChildItem -LiteralPath $store -File -Force) {
            if ($e.Name -notmatch '^[0-9A-F]{64}\.asset$') { Write-Host "  skipped (not a store entry): $($e.Name)"; continue }
            $id = Get-StageFileIdentity $e.FullName
            if ($id.Links -gt 1) { continue }
            $bytes += $e.Length; ++$removed
            if ($Execute) { Remove-Item -LiteralPath $e.FullName -Force; Write-Host "  pruned $($e.Name)" } else { Write-Host "  would prune $($e.Name) ($([math]::Round($e.Length/1MB)) MB)" }
        }
    }
    Write-Host ('store entries {0}: {1}, {2:N2} GB' -f ($(if ($Execute) { 'pruned' } else { 'prunable' })), $removed, ($bytes / 1GB))
    exit 0
}

# ---- collect candidates --------------------------------------------------------------------------------------------------
$files = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
foreach ($root in @((Join-Path $DevRuntimes 'native'), (Join-Path $DevRuntimes 'zh'))) {
    if (-not (Test-Path -LiteralPath $root -PathType Container)) { continue }
    foreach ($f in Get-ChildItem -LiteralPath $root -Recurse -File -Force -ErrorAction SilentlyContinue) {
        if ($f.FullName -notmatch '\\game\\') { continue }
        if (Test-AssetStoreEligible $f) { $files.Add($f) }
    }
}
Write-Host "eligible files: $($files.Count)"

$groups = $files | Group-Object { '{0}|{1}' -f $_.Name.ToLowerInvariant(), $_.Length }
[int64]$reclaim = 0; $linked = 0; $already = 0; $failed = 0; $hashed = 0
foreach ($g in $groups) {
    $items = @($g.Group | ForEach-Object { [pscustomobject]@{ File = $_; Identity = (Get-StageFileIdentity $_.FullName) } })
    $distinct = @($items | Group-Object { $_.Identity.Id })
    # a single physical file already: nothing to do (it may still lack a store entry; that is harmless)
    if ($distinct.Count -eq 1 -and (Test-Path -LiteralPath (Get-AssetStoreRoot $DevRuntimes))) {
        # shared already; make sure every member is the same file and move on
        $already += $items.Count; continue
    }
    $byHash = @{}
    foreach ($it in $items) {
        $key = $it.Identity.Id
        $hashes = if ($byHash.ContainsKey($key)) { $byHash[$key] } else { $null }
        if (-not $hashes) { $h = (Get-FileHash -LiteralPath $it.File.FullName -Algorithm SHA256).Hash; ++$hashed; $byHash[$key] = $h }
    }
    $byContent = $items | Group-Object { $byHash[$_.Identity.Id] }
    foreach ($content in $byContent) {
        $hash = $content.Name
        $members = @($content.Group | Sort-Object { $_.File.LastWriteTimeUtc }, { $_.File.FullName })
        $physical = @($members | Group-Object { $_.Identity.Id })
        $entry = Join-Path $store ($hash + '.asset')
        $entryExists = Test-Path -LiteralPath $entry -PathType Leaf
        $entryId = if ($entryExists) { (Get-StageFileIdentity $entry).Id } else { $null }
        $needLink = @($members | Where-Object { $_.Identity.Id -ne $entryId })
        if ($physical.Count -eq 1 -and $entryExists -and $entryId -eq $physical[0].Name) { $already += $members.Count; continue }
        # bytes saved = every physical copy except the one that becomes the store entry
        $copies = if ($entryExists) { $physical.Count - [int]($physical.Name -contains $entryId) } else { $physical.Count - 1 }
        $reclaim += [int64]$copies * $members[0].File.Length
        if (-not $Execute) { $linked += $needLink.Count; continue }
        try {
            New-Item -ItemType Directory -Force -Path $store | Out-Null
            if (-not $entryExists) {
                New-Item -ItemType HardLink -Path $entry -Target $members[0].File.FullName -ErrorAction Stop | Out-Null   # the store entry IS the oldest stage file (no copy)
                $entryId = (Get-StageFileIdentity $entry).Id
            }
            if ((Get-FileHash -LiteralPath $entry -Algorithm SHA256).Hash -ne $hash) { throw "store entry hash mismatch for $hash" }
            foreach ($m in $members) {
                if ($m.Identity.Id -eq $entryId) { continue }
                $tmp = $m.File.FullName + '.dedupe-' + [guid]::NewGuid().ToString('N')
                try {
                    New-Item -ItemType HardLink -Path $tmp -Target $entry -ErrorAction Stop | Out-Null
                    if ((Get-Item -LiteralPath $tmp -Force).Length -ne $m.File.Length) { throw 'length mismatch after linking' }
                    Move-Item -LiteralPath $tmp -Destination $m.File.FullName -Force
                    if ((Get-StageFileIdentity $m.File.FullName).Id -ne $entryId) { throw 'replacement is not the store entry' }
                    ++$linked
                } catch {
                    ++$failed; Write-Host "  could not share $($m.File.FullName): $($_.Exception.Message)"
                    if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue }
                }
            }
        } catch { ++$failed; Write-Host "  store entry problem for ${hash}: $($_.Exception.Message)" }
    }
}
Write-Host ('files {0}: {1} | already shared: {2} | failed: {3} | hashed: {4} | reclaim {5}: {6:N2} GB' -f ($(if ($Execute) { 'shared' } else { 'that would be shared' })), $linked, $already, $failed, $hashed, ($(if ($Execute) { 'achieved' } else { 'possible' })), ($reclaim / 1GB))
if ($failed) { exit 2 }
