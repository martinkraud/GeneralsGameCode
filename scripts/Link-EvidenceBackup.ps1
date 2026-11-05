#Requires -Version 7.0
<#
.SYNOPSIS
  Removes the DUPLICATE disk space between the evidence backup (GeneralsGameCode-evidence-backup\<stamp>\build\...) and the identical files in the repository's build\ tree,
  without deleting any evidence: each identical in-repo file is replaced by an NTFS hard link to the backup file (same bytes, both paths keep working).

.DESCRIPTION
  DRY RUN BY DEFAULT (-Execute to change anything). For every file below <BackupRoot>\<stamp>\build the counterpart <repo>\build\<same relative path> is considered only when
  it exists, is a normal file (no reparse point), has the same length and the same SHA-256, and is not already the same physical file. The link is created under a temporary name
  next to the original and moved over it only after the hashes matched, so an interrupted run never loses a file. Nothing outside <repo>\build and the backup is touched;
  build\dev-runtimes, build\win32 and build\win64 are skipped (they are not evidence). Both volumes must be the same NTFS volume (hard links cannot cross volumes).

  Protection against a mistaken deletion is kept (either path can still be deleted without losing the content); like the original backup it is no protection against disk failure.
#>
[CmdletBinding()]
param([string]$BackupRoot,
      [string]$RepoBuild,
      [switch]$Execute)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
. (Join-Path $PSScriptRoot 'native-runtime\AssetStore.ps1')
function Fail([string]$message, [int]$code = 1) { Write-Error $message -ErrorAction Continue; exit $code }

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $RepoBuild) { $RepoBuild = Join-Path $repoRoot 'build' }
if (-not $BackupRoot) { $BackupRoot = Join-Path (Split-Path $repoRoot -Parent) 'GeneralsGameCode-evidence-backup' }
foreach ($p in @($RepoBuild, $BackupRoot)) {
    if (-not (Test-Path -LiteralPath $p -PathType Container)) { Fail "Missing directory: $p" }
    if (Test-AssetStoreSteamPath $p) { Fail "Refusing a Steam-looking location: $p" }
}
$RepoBuild = (Resolve-Path -LiteralPath $RepoBuild).Path.TrimEnd('\')
$BackupRoot = (Resolve-Path -LiteralPath $BackupRoot).Path.TrimEnd('\')
if ((Split-Path $RepoBuild -Leaf) -ne 'build') { Fail "RepoBuild must be a directory named build (got $RepoBuild)" }
if ([IO.Path]::GetPathRoot($RepoBuild) -ne [IO.Path]::GetPathRoot($BackupRoot)) { Fail 'The backup and the build directory are on different volumes; hard links are impossible.' }

Write-Host ('Evidence backup deduplication - ' + $(if ($Execute) { 'EXECUTE' } else { 'DRY RUN (nothing will be changed)' }))
$skipTop = @('dev-runtimes', 'win32', 'win64')
$linked = 0; $same = 0; $differ = 0; $missing = 0; $failed = 0; [int64]$reclaim = 0
foreach ($stamp in Get-ChildItem -LiteralPath $BackupRoot -Directory -Force) {
    $backupBuild = Join-Path $stamp.FullName 'build'
    if (-not (Test-Path -LiteralPath $backupBuild -PathType Container)) { continue }
    foreach ($b in Get-ChildItem -LiteralPath $backupBuild -Recurse -File -Force -ErrorAction SilentlyContinue) {
        $rel = [IO.Path]::GetRelativePath($backupBuild, $b.FullName)
        if ($skipTop -contains ($rel -split '\\')[0]) { continue }
        $target = Join-Path $RepoBuild $rel
        if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { ++$missing; continue }
        $t = Get-Item -LiteralPath $target -Force
        if ($t.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
        if ($t.Length -ne $b.Length) { ++$differ; continue }
        if ((Get-StageFileIdentity $t.FullName).Id -eq (Get-StageFileIdentity $b.FullName).Id) { ++$same; continue }
        if ((Get-FileHash -LiteralPath $t.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $b.FullName -Algorithm SHA256).Hash) { ++$differ; continue }
        $reclaim += $t.Length
        if (-not $Execute) { ++$linked; continue }
        $tmp = $t.FullName + '.dedupe-' + [guid]::NewGuid().ToString('N')
        try {
            New-Item -ItemType HardLink -Path $tmp -Target $b.FullName -ErrorAction Stop | Out-Null
            if ((Get-Item -LiteralPath $tmp -Force).Length -ne $t.Length) { throw 'length mismatch after linking' }
            Move-Item -LiteralPath $tmp -Destination $t.FullName -Force
            ++$linked
        } catch {
            ++$failed; Write-Host "  could not link $($t.FullName): $($_.Exception.Message)"
            if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue }
        }
    }
}
Write-Host ('files {0}: {1} | already the same file: {2} | content differs (kept separate): {3} | no counterpart in the repo: {4} | failed: {5} | {6}: {7:N2} GB' -f
    ($(if ($Execute) { 'linked' } else { 'that would be linked' })), $linked, $same, $differ, $missing, $failed, ($(if ($Execute) { 'reclaimed' } else { 'reclaimable' })), ($reclaim / 1GB))
if ($failed) { exit 2 }
