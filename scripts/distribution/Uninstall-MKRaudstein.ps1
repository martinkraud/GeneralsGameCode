# MKRaudstein Modern Engine - Developer Preview uninstaller.
# Undoes exactly what the installer recorded in MKRaudstein\install-record.json: restores the retail files it replaced from MKRaudstein\backup,
# deletes the files it added, and removes the MKRaudstein folder. Saves and settings (Documents\...Data) are never touched.
# It also copes with an interrupted install (record status "installing"). It cannot do more than the record says: if the record or a backup is
# missing it says so and points to Steam > Properties > Installed Files > Verify integrity of game files, which is always the fallback.
[CmdletBinding()]
param([string]$GameFolder,        # default: the folder this uninstaller was installed in (MKRaudstein\..) or the one found in your Steam libraries
      [string]$PackageRoot,       # only needed to find the title when started from the package (optional)
      [switch]$Yes,
      [switch]$Pause,
      [switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'MKRaudstein-Setup.ps1')

function Finish([int]$Code) {
    if ($Pause) { Write-Host ''; try { [void](Read-Host 'Press Enter to close this window') } catch { } }
    exit $Code
}

try {
    Write-Host ''
    Write-Host 'MKRaudstein Developer Preview - uninstall' -ForegroundColor White
    Write-Step '1/3  Locating the installation'
    if (-not $GameFolder) {
        $candidates = @()
        if ($PackageRoot -and (Test-Path -LiteralPath (Join-Path $PackageRoot 'package-manifest.json'))) { $candidates = @(Find-GameFolders (Read-PackageManifest $PackageRoot)) }
        else {
            foreach ($lib in (Get-SteamLibraries)) {
                foreach ($n in @('Command and Conquer Generals', 'Command & Conquer Generals - Zero Hour')) {
                    $c = Join-Path $lib ('steamapps\common\' + $n)
                    if (Test-Path -LiteralPath (Join-Path (Join-Path $c $script:InstallDirName) $script:RecordName)) { $candidates += $c }
                }
            }
        }
        $unique = New-Object System.Collections.Generic.List[string]
        foreach ($c in @($candidates | Where-Object { Get-InstallRecord $_ })) { Add-UniqueFolder $unique $c }   # one entry per real folder (case / slash / trailing-separator variants are the same folder)
        $candidates = $unique.ToArray()
        if ($candidates.Count -eq 1) { $GameFolder = $candidates[0] }
        elseif ($candidates.Count -gt 1) {
            for ($i = 0; $i -lt $candidates.Count; $i++) { Write-Host ("   [{0}] {1}" -f ($i + 1), $candidates[$i]) }
            $GameFolder = $candidates[[int](Read-Host 'Which one') - 1]
        } else { $GameFolder = (Read-Host 'Type the full path of the game folder').Trim('"') }
    }
    $GameFolder = [IO.Path]::GetFullPath($GameFolder)
    $recordPath = Join-Path (Join-Path $GameFolder $script:InstallDirName) $script:RecordName
    if (-not (Test-Path -LiteralPath $recordPath)) {
        Write-Problem "No MKRaudstein install record in $GameFolder."
        Write-Host '   Nothing was changed. If the game still starts the 64-bit runtime, use Steam > Properties > Installed Files > Verify integrity of game files.' -ForegroundColor Yellow
        Finish 2
    }
    if (Test-GameRunning $GameFolder) { Write-Problem 'The game is running. Close it first.'; Finish 2 }
    $record = Read-JsonFile $recordPath
    Write-Ok ("{0} ({1}), install status: {2}" -f $record.target.displayName, $record.identity.designation, $record.status)
    if ($record.status -eq 'installing') { Write-Warn 'The installation was interrupted. This run restores what the installer had backed up.' }

    Write-Step '2/3  Plan'
    $restore = @(); $delete = @(); $problems = @()
    foreach ($f in $record.files) {
        $dest = Resolve-UnderGameFolder $GameFolder $f.path
        if ($f.role -eq 'tool') { continue }
        if ($f.replaced) {
            $bk = Resolve-UnderGameFolder $GameFolder ('MKRaudstein\backup\' + $f.path)
            if ((Test-Path -LiteralPath $bk) -and ((Get-FileSha256 $bk) -eq $f.backupSha256)) { $restore += [pscustomobject]@{ Path = $f.path; Dest = $dest; Backup = $bk } }
            else { $problems += "The backup of $($f.path) is missing or damaged; it cannot be restored (Steam > Verify integrity of game files restores it)." }
        } else { $delete += [pscustomobject]@{ Path = $f.path; Dest = $dest } }
    }
    Write-Host ("   Restore {0} original files from the backup; delete {1} files the installer added." -f $restore.Count, $delete.Count)
    foreach ($a in @($record.generatedArtifacts)) { Write-Host ("   delete the file the game generated: {0}" -f $a) }
    Write-Host '   Your saves and settings (Documents\...Data) are not touched.'
    foreach ($p in $problems) { Write-Warn $p }
    if ($CheckOnly) { Write-Host 'Check only - nothing was changed.' -ForegroundColor Yellow; Finish 0 }
    if (-not $Yes) {
        if ((Read-Host ('Uninstall from "{0}"? [y/N]' -f $GameFolder)) -notmatch '^(y|yes)$') { Write-Host 'Cancelled. Nothing was changed.'; Finish 1 }
    }

    Write-Step '3/3  Restoring'
    $failed = @()
    foreach ($r in $restore) {
        try { Copy-Item -LiteralPath $r.Backup -Destination $r.Dest -Force; if ((Get-FileSha256 $r.Dest) -ne (Get-FileSha256 $r.Backup)) { throw 'verification failed' } }
        catch { $failed += "restore $($r.Path): $($_.Exception.Message)" }
    }
    foreach ($d in $delete) {
        try { if (Test-Path -LiteralPath $d.Dest) { Remove-Item -LiteralPath $d.Dest -Force } }
        catch { $failed += "delete $($d.Path): $($_.Exception.Message)" }
    }
    foreach ($a in @($record.generatedArtifacts)) {
        try { $ap = Resolve-UnderGameFolder $GameFolder $a; if (Test-Path -LiteralPath $ap) { Remove-Item -LiteralPath $ap -Force } } catch { }
    }
    # the folders of the files the installer generated (Data\INI, Window): remove them only if they are now empty (a retail folder that holds anything else stays)
    foreach ($a in @($record.generatedArtifacts)) {
        try { $dir = Split-Path -Parent (Resolve-UnderGameFolder $GameFolder $a); if ($dir -and $dir.Length -gt $GameFolder.Length) { Remove-EmptyDirectories $dir } } catch { }
    }
    # the empty crash-dump folder the 64-bit runtime creates next to the game (only when empty: a real dump is kept)
    try { Remove-EmptyDirectories (Join-Path $GameFolder 'CrashDumps') } catch { }
    # leftover temp files of an interrupted copy
    foreach ($f in $record.files) { try { $t = (Resolve-UnderGameFolder $GameFolder $f.path) + '.mkr-new'; if (Test-Path -LiteralPath $t) { Remove-Item -LiteralPath $t -Force } } catch { } }
    # empty directories the install created (only if now empty)
    foreach ($f in $record.files) {
        try { $dir = Split-Path -Parent (Resolve-UnderGameFolder $GameFolder $f.path); if ($dir -and $dir.Length -gt $GameFolder.Length -and -not $dir.StartsWith((Join-Path $GameFolder $script:InstallDirName), [StringComparison]::OrdinalIgnoreCase)) { Remove-EmptyDirectories $dir } } catch { }
    }
    if ($failed.Count -eq 0 -and $problems.Count -eq 0) {
        Set-Location -LiteralPath $GameFolder
        Remove-Item -LiteralPath (Join-Path $GameFolder $script:InstallDirName) -Recurse -Force -ErrorAction SilentlyContinue
        Write-Ok 'Original files restored, MKRaudstein files and the MKRaudstein folder removed.'
        Write-Host ''
        Write-Host 'DONE. The game is back to the original files this installer replaced.' -ForegroundColor Green
        Write-Host 'Your saves and settings were not touched. If anything still looks wrong: Steam > Properties > Installed Files > Verify integrity of game files.'
        Finish 0
    }
    # something could not be undone: keep the record and the backup so a second attempt (or Steam verify) is possible
    foreach ($m in ($failed + $problems)) { Write-Problem $m }
    $record.status = 'uninstall-incomplete'
    Write-JsonFile $recordPath $record
    Write-Host ''
    Write-Host 'The uninstall is INCOMPLETE. The record and the backup were kept. Fix the problem above and run it again, or use Steam > Properties > Installed Files > Verify integrity of game files.' -ForegroundColor Yellow
    Finish 4
} catch {
    Write-Problem $_.Exception.Message
    Write-Host 'Fallback: Steam > Properties > Installed Files > Verify integrity of game files.' -ForegroundColor Yellow
    Finish 1
}
