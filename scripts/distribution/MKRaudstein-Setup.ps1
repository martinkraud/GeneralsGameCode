# MKRaudstein Modern Engine - shared helpers of the Developer Preview installer / uninstaller.
# Player-facing script: it must run on the Windows PowerShell 5.1 that ships with Windows 10 / 11 (no PowerShell 7 syntax, no Python, no Visual Studio).
# Nothing here touches anything but the game folder the player selects: no registry, no user data, no other Steam files.
Set-StrictMode -Version 2

$script:InstallDirName = 'MKRaudstein'
$script:RecordName = 'install-record.json'

function Write-Step([string]$Text) { Write-Host ''; Write-Host ('== ' + $Text) -ForegroundColor Cyan }
function Write-Ok([string]$Text) { Write-Host ('   ' + $Text) -ForegroundColor Green }
function Write-Warn([string]$Text) { Write-Host ('   WARNING: ' + $Text) -ForegroundColor Yellow }
function Write-Problem([string]$Text) { Write-Host ('   PROBLEM: ' + $Text) -ForegroundColor Red }

function Get-FileSha256([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant() }

function Read-JsonFile([string]$Path) {
    $text = [IO.File]::ReadAllText($Path)
    if ($text.Length -gt 0 -and $text[0] -eq [char]0xFEFF) { $text = $text.Substring(1) }
    return ($text | ConvertFrom-Json)
}

function Write-JsonFile([string]$Path, $Object) {
    $json = $Object | ConvertTo-Json -Depth 8
    [IO.File]::WriteAllText($Path, $json, (New-Object System.Text.UTF8Encoding($false)))
}

# The machine type of a PE image: 0x14C = 32-bit x86 (the retail launcher), 0x8664 = AMD64 (the project runtime). 0 when the file is not a PE image.
function Get-PeMachine([string]$Path) {
    try {
        $stream = [IO.File]::Open($Path, 'Open', 'Read', 'ReadWrite')
        try {
            $head = New-Object byte[] 4096
            $read = $stream.Read($head, 0, $head.Length)
            if ($read -lt 0x40 -or $head[0] -ne 0x4D -or $head[1] -ne 0x5A) { return 0 }
            $pe = [BitConverter]::ToInt32($head, 0x3C)
            if ($pe -lt 0 -or ($pe + 6) -gt $read) { return 0 }
            return [int][BitConverter]::ToUInt16($head, $pe + 4)
        } finally { $stream.Dispose() }
    } catch { return 0 }
}

function Read-PackageManifest([string]$PackageRoot) {
    $path = Join-Path $PackageRoot 'package-manifest.json'
    if (-not (Test-Path -LiteralPath $path)) { throw "package-manifest.json is missing next to the installer ($path). Extract the whole ZIP before running the installer." }
    return (Read-JsonFile $path)
}

# Steam library folders (the default one plus the ones listed in libraryfolders.vdf). Read-only.
# Windows path identity: case-insensitive, either slash, no trailing separator, "." / ".." resolved. Two spellings of one folder must be ONE entry (the registry reports
# "c:/program files (x86)/steam", the environment "C:\Program Files (x86)\Steam"); two genuinely different folders stay two. Unusable text yields $null.
function Get-FolderKey([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { return $null }
    try {
        $full = [IO.Path]::GetFullPath(($Path.Trim().Trim('"') -replace '/', '\'))
        $root = [IO.Path]::GetPathRoot($full)
        if ($full.Length -gt $root.Length) { $full = $full.TrimEnd('\') }
        return $full.ToUpperInvariant()
    } catch { return $null }
}
# Adds $Path to $List unless an equivalent folder is already there; the first spelling is kept (normalised for display: backslashes, no trailing separator).
function Add-UniqueFolder($List, [string]$Path) {
    $key = Get-FolderKey $Path
    if (-not $key) { return }
    foreach ($existing in $List) { if ((Get-FolderKey $existing) -eq $key) { return } }
    $List.Add((Get-ExactCasePath ([IO.Path]::GetFullPath(($Path.Trim().Trim('"') -replace '/', '\'))).TrimEnd('\')))
}
# The folder names exactly as they are on disk (the registry reports lower case); display only. Falls back to the given text when a segment cannot be listed.
function Get-ExactCasePath([string]$Path) {
    try {
        $root = [IO.Path]::GetPathRoot($Path)
        $current = $root.ToUpperInvariant()
        foreach ($segment in ($Path.Substring($root.Length) -split '\\' | Where-Object { $_ })) {
            $hit = @([IO.Directory]::EnumerateFileSystemEntries($current, $segment) | Select-Object -First 1)
            if ($hit.Count -eq 0) { return $Path }
            $current = $hit[0]
        }
        return $current
    } catch { return $Path }
}

function Get-SteamLibraries {
    $libraries = New-Object System.Collections.Generic.List[string]
    $roots = @()
    foreach ($key in @('HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam')) {
        try {
            $item = Get-ItemProperty -Path $key -ErrorAction Stop
            foreach ($name in @('SteamPath', 'InstallPath')) { if ($item.PSObject.Properties.Name -contains $name) { $roots += [string]$item.$name } }
        } catch { }
    }
    $roots += (Join-Path ${env:ProgramFiles(x86)} 'Steam')
    $roots += (Join-Path $env:ProgramFiles 'Steam')
    foreach ($root in @($roots | Where-Object { $_ })) {
        $root = $root -replace '/', '\'
        if (-not (Test-Path -LiteralPath $root)) { continue }
        Add-UniqueFolder $libraries $root
        $vdf = Join-Path $root 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $vdf) {
            foreach ($line in (Get-Content -LiteralPath $vdf)) {
                if ($line -match '"path"\s+"([^"]+)"') {
                    $p = ($Matches[1] -replace '\\\\', '\') -replace '/', '\'
                    if (Test-Path -LiteralPath $p) { Add-UniqueFolder $libraries $p }
                }
            }
        }
    }
    return $libraries.ToArray()
}

function Find-GameFolders($Manifest) {
    $found = New-Object System.Collections.Generic.List[string]
    foreach ($library in (Get-SteamLibraries)) {
        foreach ($folderName in @($Manifest.target.steamFolderNames)) {
            $candidate = Join-Path $library ('steamapps\common\' + $folderName)
            if (Test-Path -LiteralPath $candidate) { Add-UniqueFolder $found $candidate }
        }
    }
    return $found.ToArray()
}

function Get-InstallRecord([string]$GameFolder) {
    $path = Join-Path (Join-Path $GameFolder $script:InstallDirName) $script:RecordName
    if (Test-Path -LiteralPath $path) { return (Read-JsonFile $path) }
    return $null
}

# Is a game process running from this folder? (Replacing a running executable would fail or corrupt the install.)
function Test-GameRunning([string]$GameFolder) {
    $full = [IO.Path]::GetFullPath($GameFolder).TrimEnd('\') + '\'
    foreach ($p in (Get-Process -ErrorAction SilentlyContinue)) {
        try {
            $path = $p.Path
            if ($path -and $path.StartsWith($full, [StringComparison]::OrdinalIgnoreCase)) { return $true }
        } catch { }
    }
    return $false
}

# Safe relative path: no drive, no "..", no leading slash. Everything the installer writes goes under the game folder.
function Resolve-UnderGameFolder([string]$GameFolder, [string]$Relative) {
    if ([string]::IsNullOrWhiteSpace($Relative) -or $Relative -match '(^|[\\/])\.\.([\\/]|$)' -or [IO.Path]::IsPathRooted($Relative)) { throw "Refusing an unsafe path: '$Relative'" }
    $full = [IO.Path]::GetFullPath((Join-Path $GameFolder $Relative))
    $root = [IO.Path]::GetFullPath($GameFolder).TrimEnd('\') + '\'
    if (-not $full.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw "Refusing a path outside the game folder: '$Relative'" }
    return $full
}

# What state is the game folder in, and is it a supported game?
function Test-GameFolder([string]$GameFolder, $Manifest) {
    $problems = New-Object System.Collections.Generic.List[string]
    $warnings = New-Object System.Collections.Generic.List[string]
    $state = 'Unknown'
    if (-not (Test-Path -LiteralPath $GameFolder -PathType Container)) {
        $problems.Add("The folder does not exist: $GameFolder")
        return [pscustomobject]@{ Ok = $false; State = $state; Problems = @($problems); Warnings = @($warnings) }
    }
    foreach ($f in @($Manifest.target.requiredFiles)) {
        $path = Join-Path $GameFolder $f.path
        if (-not (Test-Path -LiteralPath $path)) { $problems.Add("Not a supported $($Manifest.target.displayName) installation: $($f.path) is missing.") }
        elseif ($f.size -and (Get-Item -LiteralPath $path).Length -ne [int64]$f.size) { $warnings.Add("$($f.path) has a different size than the supported Steam release (another game version?).") }
    }
    $exe = Join-Path $GameFolder 'Generals.exe'
    $record = Get-InstallRecord $GameFolder
    if (Test-Path -LiteralPath $exe) {
        $machine = Get-PeMachine $exe
        if ($record) { $state = 'Installed' }
        elseif ($machine -eq 0x14C) { $state = 'Retail' }
        elseif ($machine -eq 0x8664) { $state = 'UnknownX64'; $problems.Add('Generals.exe is already a 64-bit executable that this installer did not install (another modification?). Restore the original with Steam > Properties > Installed Files > Verify integrity of game files first.') }
        else { $problems.Add('Generals.exe is not a recognisable game executable. Verify the game files in Steam first.') }
    } else { $problems.Add('Generals.exe is missing from the game folder. Verify the game files in Steam first.') }
    if (Test-GameRunning $GameFolder) { $problems.Add('The game (or another program from this folder) is running. Close it first.') }
    return [pscustomobject]@{ Ok = ($problems.Count -eq 0); State = $state; Problems = @($problems); Warnings = @($warnings) }
}

function Remove-EmptyDirectories([string]$Folder) {
    if (-not (Test-Path -LiteralPath $Folder)) { return }
    foreach ($d in (Get-ChildItem -LiteralPath $Folder -Directory -Recurse -ErrorAction SilentlyContinue | Sort-Object { $_.FullName.Length } -Descending)) {
        if (-not (Get-ChildItem -LiteralPath $d.FullName -Force -ErrorAction SilentlyContinue)) { Remove-Item -LiteralPath $d.FullName -Force -ErrorAction SilentlyContinue }
    }
    if (-not (Get-ChildItem -LiteralPath $Folder -Force -ErrorAction SilentlyContinue)) { Remove-Item -LiteralPath $Folder -Force -ErrorAction SilentlyContinue }
}
