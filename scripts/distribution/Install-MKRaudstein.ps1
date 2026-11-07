# MKRaudstein Modern Engine - Developer Preview installer.
# Installs the 64-bit Modern runtime into an EXISTING, legitimate Steam installation of the game. Safe by construction:
#   * only the selected game folder is written; no registry, no administrator rights beyond what that folder needs, no user data touched;
#   * every file it replaces is backed up first (MKRaudstein\backup), and a failure part-way rolls everything back;
#   * everything it did is recorded in MKRaudstein\install-record.json so the uninstaller can undo exactly that.
# Runs on the Windows PowerShell 5.1 that ships with Windows. Start it with Install.cmd.
[CmdletBinding()]
param([string]$GameFolder,        # the game folder; found automatically in your Steam libraries when omitted
      [switch]$Yes,               # do not ask for confirmation (scripted use)
      [switch]$Pause,             # wait for Enter at the end (used by Install.cmd so the window stays open)
      [switch]$CheckOnly)         # report what would happen, change nothing
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'MKRaudstein-Setup.ps1')
$packageRoot = Split-Path -Parent $PSScriptRoot
$exitCode = 0

function Finish([int]$Code) {
    if ($Pause) { Write-Host ''; try { [void](Read-Host 'Press Enter to close this window') } catch { } }
    exit $Code
}

try {
    $manifest = Read-PackageManifest $packageRoot
    Write-Host ''
    Write-Host ("MKRaudstein {0} - {1}" -f $manifest.target.displayName, $manifest.identity.designation) -ForegroundColor White
    Write-Host ("Build {0}, commit {1}, {2}" -f $manifest.identity.buildDate, $manifest.identity.commit, $manifest.identity.arch)
    Write-Host $manifest.identity.branding

    # ---- 1. which game folder ------------------------------------------------------------------------------------------------------------------
    Write-Step '1/6  Locating the game'
    if (-not $GameFolder) {
        $found = @(Find-GameFolders $manifest)
        if ($found.Count -eq 1) { $GameFolder = $found[0]; Write-Ok "Found: $GameFolder" }
        elseif ($found.Count -gt 1) {
            for ($i = 0; $i -lt $found.Count; $i++) { Write-Host ("   [{0}] {1}" -f ($i + 1), $found[$i]) }
            $pick = Read-Host 'Which one'
            $GameFolder = $found[[int]$pick - 1]
        } else {
            Write-Warn "No Steam installation of $($manifest.target.displayName) was found automatically."
            $GameFolder = (Read-Host 'Type the full path of the game folder (the one that contains Generals.exe)').Trim('"')
        }
    }
    $GameFolder = [IO.Path]::GetFullPath($GameFolder)

    # ---- 2. is it a supported game, in what state ----------------------------------------------------------------------------------------------
    Write-Step '2/6  Checking the installation'
    $check = Test-GameFolder $GameFolder $manifest
    foreach ($w in $check.Warnings) { Write-Warn $w }
    if (-not $check.Ok) {
        foreach ($p in $check.Problems) { Write-Problem $p }
        Write-Host ''
        Write-Host 'Nothing was changed.' -ForegroundColor Yellow
        Finish 2
    }
    Write-Ok "$($manifest.target.displayName) found: $GameFolder"
    $oldRecord = $null
    switch ($check.State) {
        'Retail' { Write-Ok 'State: original 32-bit game, no earlier MKRaudstein install. Fresh install.' }
        'Installed' {
            $oldRecord = Get-InstallRecord $GameFolder
            Write-Ok ("State: MKRaudstein already installed ({0}, commit {1}). This run upgrades / repairs it." -f $oldRecord.identity.designation, $oldRecord.identity.commit)
            if ($oldRecord.target -and $oldRecord.target.title -and $oldRecord.target.title -ne $manifest.target.title) { throw "The existing install record belongs to $($oldRecord.target.title), not $($manifest.target.title)." }
        }
    }

    # ---- 3. verify the package itself ----------------------------------------------------------------------------------------------------------
    Write-Step '3/6  Verifying the package'
    $payloadRoot = Join-Path $packageRoot 'payload'
    $payloadBytes = 0L
    foreach ($f in $manifest.files) {
        $src = Join-Path $payloadRoot $f.path
        if (-not (Test-Path -LiteralPath $src)) { throw "Package file missing: payload\$($f.path). Extract the whole ZIP and run the installer again." }
        if ((Get-Item -LiteralPath $src).Length -ne [int64]$f.size) { throw "Package file has the wrong size: payload\$($f.path). The ZIP is damaged; download it again." }
        if ((Get-FileSha256 $src) -ne $f.sha256.ToUpperInvariant()) { throw "Package file is damaged (hash mismatch): payload\$($f.path). Download the ZIP again." }
        $payloadBytes += [int64]$f.size
    }
    Write-Ok ("{0} files, {1:N1} MB, all hashes match." -f @($manifest.files).Count, ($payloadBytes / 1MB))
    # Files that are modified copies of the game's own data are never shipped: they are generated here from the player's archives.
    $generated = @()
    $genTemp = $null
    if (($manifest.PSObject.Properties.Name -contains 'generators') -and (@($manifest.generators) -contains 'generals-modern-extensions')) {
        try {
            . (Join-Path $PSScriptRoot 'GeneralsModernExtensions.ps1')
            $genTemp = Join-Path ([IO.Path]::GetTempPath()) ('mkraudstein-gen-' + [Guid]::NewGuid().ToString('N'))
            $items = @()
            foreach ($g in (Build-GeneralsModernExtensions $GameFolder)) {
                $dst = Join-Path $genTemp $g.Path
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
                [IO.File]::WriteAllBytes($dst, $g.Bytes)
                $items += [pscustomobject]@{ Path = $g.Path; Source = $dst; Sha256 = (Get-FileSha256 $dst); Size = [int64]$g.Bytes.Length }
            }
            $generated = $items
            $expected = @{}
            foreach ($e in @($manifest.generated)) { $expected[$e.path.ToLowerInvariant()] = $e.sha256.ToUpperInvariant() }
            $differs = @($generated | Where-Object { -not $expected.ContainsKey($_.Path.ToLowerInvariant()) -or $expected[$_.Path.ToLowerInvariant()] -ne $_.Sha256 })
            if ($differs.Count -eq 0) { Write-Ok ("{0} sidebar-extension files generated from your game archives (identical to the tested release)." -f $generated.Count) }
            else { Write-Warn 'Your game archives differ from the tested Steam release: the sidebar-extension files were generated anyway.' }
        } catch {
            Write-Warn ("The Generals special-power sidebar extension could not be generated from your game files ({0}). Installing without it; the game works with the original sidebar." -f $_.Exception.Message)
            $generated = @()
        }
    }
    $drive = New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($GameFolder))
    if ($drive.AvailableFreeSpace -lt (2 * $payloadBytes + 50MB)) { throw ("Not enough free disk space on {0} ({1:N0} MB free, {2:N0} MB needed)." -f $drive.Name, ($drive.AvailableFreeSpace / 1MB), ((2 * $payloadBytes + 50MB) / 1MB)) }

    # ---- 4. plan -------------------------------------------------------------------------------------------------------------------------------
    Write-Step '4/6  Plan'
    $mkrDir = Join-Path $GameFolder $script:InstallDirName
    $backupDir = Join-Path $mkrDir 'backup'
    $plan = New-Object System.Collections.Generic.List[object]
    foreach ($f in $manifest.files) {
        $dest = Resolve-UnderGameFolder $GameFolder $f.path
        $exists = Test-Path -LiteralPath $dest
        $oldEntry = $null
        if ($oldRecord) { $oldEntry = @($oldRecord.files | Where-Object { $_.path -ieq $f.path })[0] }
        # A file the earlier install already replaced keeps its ORIGINAL backup; a file we have never touched is backed up now.
        # (a file the earlier install listed is ours or already backed up: only a file the record has never seen can be a retail file)
        $needsBackup = $exists -and -not $oldEntry
        $plan.Add([pscustomobject]@{ Path = $f.path; Source = (Join-Path $payloadRoot $f.path); Dest = $dest; Sha256 = $f.sha256.ToUpperInvariant(); Size = [int64]$f.size
                                     Exists = $exists; NeedsBackup = $needsBackup; Old = $oldEntry; Role = 'payload' })
    }
    foreach ($g in $generated) {
        $dest = Resolve-UnderGameFolder $GameFolder $g.Path
        $exists = Test-Path -LiteralPath $dest
        $oldEntry = $null
        if ($oldRecord) { $oldEntry = @($oldRecord.files | Where-Object { $_.path -ieq $g.Path })[0] }
        $plan.Add([pscustomobject]@{ Path = $g.Path; Source = $g.Source; Dest = $dest; Sha256 = $g.Sha256; Size = $g.Size
                                     Exists = $exists; NeedsBackup = ($exists -and -not $oldEntry); Old = $oldEntry; Role = 'generated' })
    }
    $toolFiles = @('Uninstall.cmd', 'tools\MKRaudstein-Setup.ps1', 'tools\Uninstall-MKRaudstein.ps1')
    $stale = @()
    if ($oldRecord) {
        $newPaths = @($plan | ForEach-Object { $_.Path.ToLowerInvariant() })
        $stale = @($oldRecord.files | Where-Object { $_.role -ne 'tool' -and $newPaths -notcontains $_.path.ToLowerInvariant() })
    }
    Write-Host ("   Add or replace {0} files; back up {1} existing files; remove {2} files of the earlier version." -f $plan.Count, @($plan | Where-Object NeedsBackup).Count, $stale.Count)
    foreach ($p in ($plan | Where-Object NeedsBackup)) { Write-Host ("   backup: {0}" -f $p.Path) }
    Write-Host '   Your saves and settings (Documents\...Data) are not touched.'
    if ($CheckOnly) { Write-Host ''; Write-Host 'Check only - nothing was changed.' -ForegroundColor Yellow; Finish 0 }
    if (-not $Yes) {
        $answer = Read-Host ('Install into "{0}"? [y/N]' -f $GameFolder)
        if ($answer -notmatch '^(y|yes)$') { Write-Host 'Cancelled. Nothing was changed.'; Finish 1 }
    }

    # ---- 5. back up, write the record (status "installing" so an interrupted run can be undone), copy ---------------------------------------------
    Write-Step '5/6  Installing'
    New-Item -ItemType Directory -Force -Path $mkrDir | Out-Null
    $recordFiles = New-Object System.Collections.Generic.List[object]
    foreach ($p in $plan) {
        $replaced = $false; $backupSha = $null
        if ($p.Old -and $p.Old.replaced) { $replaced = $true; $backupSha = $p.Old.backupSha256 }
        if ($p.NeedsBackup) {
            $bk = Resolve-UnderGameFolder $GameFolder ('MKRaudstein\backup\' + $p.Path)
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $bk) | Out-Null
            if (-not (Test-Path -LiteralPath $bk)) { Copy-Item -LiteralPath $p.Dest -Destination $bk }
            $backupSha = Get-FileSha256 $bk
            if ($backupSha -ne (Get-FileSha256 $p.Dest)) { throw "Backup of $($p.Path) did not verify." }
            $replaced = $true
        }
        $recordFiles.Add([ordered]@{ path = $p.Path; role = $p.Role; size = $p.Size; sha256 = $p.Sha256; replaced = $replaced; backupSha256 = $backupSha })
    }
    foreach ($t in $toolFiles) {
        $recordFiles.Add([ordered]@{ path = ('MKRaudstein\' + $t); role = 'tool'; size = 0; sha256 = $null; replaced = $false; backupSha256 = $null })
    }
    $record = [ordered]@{
        schema = 'mkraudstein-install-record/1'; status = 'installing'; installedUtc = (Get-Date).ToUniversalTime().ToString('o')
        identity = $manifest.identity; target = $manifest.target
        generatedArtifacts = @($manifest.generatedArtifacts); backupFolder = 'MKRaudstein\backup'; files = $recordFiles.ToArray()
    }
    $recordPath = Join-Path $mkrDir $script:RecordName
    Write-JsonFile $recordPath $record

    $written = New-Object System.Collections.Generic.List[object]
    try {
        foreach ($p in $plan) {
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $p.Dest) | Out-Null
            $tmp = $p.Dest + '.mkr-new'
            try {
                Copy-Item -LiteralPath $p.Source -Destination $tmp -Force
                if ((Get-FileSha256 $tmp) -ne $p.Sha256) { throw "Copy of $($p.Path) did not verify." }
                if (Test-Path -LiteralPath $p.Dest) { Remove-Item -LiteralPath $p.Dest -Force }
                Move-Item -LiteralPath $tmp -Destination $p.Dest
            } catch {
                if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue }
                throw
            }
            $written.Add($p)
        }
        foreach ($s in $stale) {   # files only the earlier version had
            $sd = Resolve-UnderGameFolder $GameFolder $s.path
            if (Test-Path -LiteralPath $sd) {
                if ($s.replaced) { $bk = Resolve-UnderGameFolder $GameFolder ('MKRaudstein\backup\' + $s.path); if (Test-Path -LiteralPath $bk) { Copy-Item -LiteralPath $bk -Destination $sd -Force } }
                else { Remove-Item -LiteralPath $sd -Force }
            }
        }
        # the uninstaller travels with the install, so removal never needs the ZIP
        New-Item -ItemType Directory -Force -Path (Join-Path $mkrDir 'tools') | Out-Null
        foreach ($t in $toolFiles) { Copy-Item -LiteralPath (Join-Path $packageRoot $t) -Destination (Join-Path $mkrDir ($t)) -Force }
    } catch {
        Write-Problem ("Installation failed: {0}" -f $_.Exception.Message)
        Write-Host '   Rolling back ...' -ForegroundColor Yellow
        foreach ($p in $written) {
            try {
                $bk = Join-Path $backupDir $p.Path
                if ($p.NeedsBackup -and (Test-Path -LiteralPath $bk)) { Copy-Item -LiteralPath $bk -Destination $p.Dest -Force }
                elseif (-not $p.Exists) { Remove-Item -LiteralPath $p.Dest -Force -ErrorAction SilentlyContinue }
            } catch { Write-Warn ("Could not roll back {0}: {1}" -f $p.Path, $_.Exception.Message) }
        }
        Write-Host '   Rolled back. The record was kept as "installing"; run Uninstall.cmd (or Steam > Verify integrity of game files) if anything looks wrong.' -ForegroundColor Yellow
        Finish 3
    }

    # ---- 6. verify, finalise the record -----------------------------------------------------------------------------------------------------------
    Write-Step '6/6  Verifying the installation'
    foreach ($p in $plan) {
        if ((Get-FileSha256 $p.Dest) -ne $p.Sha256) { throw "Installed file does not match the package: $($p.Path)" }
    }
    $exe = Join-Path $GameFolder 'Generals.exe'
    if ((Get-PeMachine $exe) -ne 0x8664) { throw 'Generals.exe is not the 64-bit runtime after installing.' }
    $record.status = 'installed'
    foreach ($entry in $record.files) {
        if ($entry.role -eq 'tool') {
            $tp = Join-Path $GameFolder $entry.path
            $entry.sha256 = Get-FileSha256 $tp; $entry.size = (Get-Item -LiteralPath $tp).Length
        }
    }
    Write-JsonFile $recordPath $record
    Write-Ok ("{0} files installed and verified." -f $plan.Count)

    if ($genTemp -and (Test-Path -LiteralPath $genTemp)) { Remove-Item -LiteralPath $genTemp -Recurse -Force -ErrorAction SilentlyContinue }
    Write-Host ''
    Write-Host 'DONE.' -ForegroundColor Green
    Write-Host ''
    Write-Host 'How to start:'
    Write-Host "  * Steam: press PLAY on $($manifest.target.displayName) as usual. Steam starts the 64-bit Modern runtime."
    Write-Host '  * Classic look at any time: Steam > Properties > Launch Options > -classic'
    Write-Host '  * Saves and settings keep using your normal Documents\...Data folder.'
    Write-Host '  * To remove it: MKRaudstein\Uninstall.cmd in the game folder (restores the original files). Steam "Verify integrity of game files" is the fallback.'
} catch {
    Write-Problem $_.Exception.Message
    Write-Host 'If files were already changed, run Uninstall.cmd, or use Steam > Properties > Installed Files > Verify integrity of game files.' -ForegroundColor Yellow
    $exitCode = 1
}
Finish $exitCode
