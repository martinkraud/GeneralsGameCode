#Requires -Version 7.0
# X64.47A: builds the PREVIEW Zero Hour player overlay ZIP from known build outputs. The ZIP is extracted over a legitimate Zero Hour
# installation (Steam: its Play button starts Generals.exe, which this package replaces with the AMD64 build). It contains only
# project-built files and the Microsoft Visual C++ runtime DLLs the executable imports (app-local deployment); never EA assets.
# Deterministic: the same inputs give the same ZIP bytes (sorted entries, fixed timestamps, no build-tree paths).
# Nothing is installed anywhere; the Steam installation is never touched.
[CmdletBinding()]
param([string]$ExePath,                   # default: build\win64\GeneralsMD\Release\generalszh.exe
      [string]$SymbolsPath,               # default: the .pdb next to the executable
      [string]$RedistDirectory,           # directory with the x64 Microsoft.VC14x.CRT DLLs; default: newest Visual Studio redistributable
      [string]$OutputDirectory,           # default: build\dist
      [string]$Version,                   # default: the short commit id
      [switch]$AllowDirtyTree)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$repo = $script:ZHRepository
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

if (-not $ExePath) { $ExePath = Join-Path $repo 'build\win64\GeneralsMD\Release\generalszh.exe' }
if (-not $SymbolsPath) { $SymbolsPath = [IO.Path]::ChangeExtension($ExePath, '.pdb') }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo 'build\dist' }
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) { throw "Executable not found: $ExePath (build target z_generals, preset win64)" }

Push-Location $repo
try {
    $commit = (git rev-parse --short=9 HEAD).Trim()
    $commitFull = (git rev-parse HEAD).Trim()
    $dirty = @(git status --porcelain --untracked-files=no)
} finally { Pop-Location }
if ($dirty.Count -and -not $AllowDirtyTree) { throw "Tracked files have uncommitted changes; the package records the commit it was built from. Commit first (or -AllowDirtyTree for a throw-away test build).`n$($dirty -join "`n")" }
if (-not $Version) { $Version = $commit }

function Get-PeMachine([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    $pe = [BitConverter]::ToInt32($bytes, 0x3c)
    return [BitConverter]::ToUInt16($bytes, $pe + 4)
}
if ((Get-PeMachine $ExePath) -ne 0x8664) { throw "$ExePath is not an AMD64 (x64) executable." }

# The runtime DLLs the executable imports, derived from its import table (never guessed): everything that is not a Windows system DLL.
function Get-PeImports([string]$Path) {
    $d = [IO.File]::ReadAllBytes($Path)
    $pe = [BitConverter]::ToInt32($d, 0x3c)
    $sections = [BitConverter]::ToUInt16($d, $pe + 6)
    $optSize = [BitConverter]::ToUInt16($d, $pe + 20)
    $dir = $pe + 24 + 112                       # PE32+ data directories
    $importRva = [BitConverter]::ToUInt32($d, $dir + 8)
    $table = $pe + 24 + $optSize
    $map = @(for ($i = 0; $i -lt $sections; $i++) {
        $o = $table + $i * 40
        [pscustomobject]@{ Va = [BitConverter]::ToUInt32($d, $o + 12); Size = [BitConverter]::ToUInt32($d, $o + 8); Raw = [BitConverter]::ToUInt32($d, $o + 20) } })
    function ToOffset([uint32]$Rva) { foreach ($s in $map) { if ($Rva -ge $s.Va -and $Rva -lt $s.Va + [Math]::Max($s.Size, 1)) { return [int]($Rva - $s.Va + $s.Raw) } } }
    $names = @()
    $o = ToOffset $importRva
    while ($true) {
        $nameRva = [BitConverter]::ToUInt32($d, $o + 12)
        if (-not $nameRva) { break }
        $s = ToOffset $nameRva
        $e = [Array]::IndexOf($d, [byte]0, $s)
        $names += [Text.Encoding]::ASCII.GetString($d, $s, $e - $s)
        $o += 20
    }
    return $names
}
$imports = Get-PeImports $ExePath
$runtimeNames = @($imports | Where-Object { $_ -match '^(vcruntime|msvcp|concrt|vccorlib)' } | Sort-Object -Unique)
if (-not $RedistDirectory) {
    $RedistDirectory = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Redist\MSVC\*\x64\Microsoft.VC14*.CRT' -Directory -ErrorAction SilentlyContinue |
        Sort-Object { [version]($_.FullName -replace '^.*\\MSVC\\([^\\]+)\\.*$', '$1') } -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $RedistDirectory -or -not (Test-Path -LiteralPath $RedistDirectory)) { throw 'No x64 Microsoft Visual C++ redistributable directory found; pass -RedistDirectory.' }
foreach ($n in $runtimeNames) { if (-not (Test-Path -LiteralPath (Join-Path $RedistDirectory $n))) { throw "Redistributable DLL missing: $n in $RedistDirectory" } }

# ---- the overlay contract: every file, why, who owns it ----------------------------------------------------------------------------
$files = [System.Collections.Generic.List[object]]::new()
function Add-File([string]$Destination, [string]$Source, [string]$BuildTarget, [string]$Reason, [string]$Ownership, [string]$Requirement, [string]$Replaces, [byte[]]$Content) {
    $files.Add([pscustomobject]@{ Destination = $Destination; Source = $Source; BuildTarget = $BuildTarget; Reason = $Reason; Ownership = $Ownership
                                  Requirement = $Requirement; Replaces = $Replaces; Content = $Content })
}
Add-File 'Generals.exe' $ExePath 'z_generals (preset win64, Release) generalszh.exe' `
    'The AMD64 / Direct3D 11 Zero Hour runtime. Steam''s Play button starts Generals.exe, so the runtime is shipped under that name.' `
    'project-owned (GPL-3.0-or-later, see MKRaudstein\LICENSE.md)' 'required' `
    'REPLACES the original Electronic Arts launcher Generals.exe of the Steam installation (restore with Steam: Verify integrity of game files)' $null
foreach ($imported in $runtimeNames) {
    $n = $imported.ToLowerInvariant()     # the redistributable's own (lower-case) spelling
    Add-File $n (Join-Path $RedistDirectory $imported) 'Microsoft Visual C++ x64 redistributable (Visual Studio)' `
        "Imported by Generals.exe ($n); shipped app-local so the player need not install the Visual C++ x64 redistributable." `
        'Microsoft redistributable (app-local deployment of the Visual C++ runtime)' 'required' 'new file (not part of the Steam installation)' $null
}
Add-File 'MKRaudstein\LICENSE.md' (Join-Path $repo 'LICENSE.md') 'repository LICENSE.md' 'GPL-3.0-or-later with the Electronic Arts additional terms: required for redistributing the executable.' `
    'project-owned' 'required' 'new file' $null

# README (generated; same text for the same inputs)
$readme = @"
MKRaudstein Zero Hour x64 - PREVIEW $Version
Modern Experience by MKRaudstein | 64-bit * DirectX 11
================================================================

STATUS: PREVIEW / TESTING PACKAGE. NOT a public release. It is meant for testing on a second PC.

WHAT THIS IS
An overlay for an EXISTING, legitimate installation of Command & Conquer Generals: Zero Hour. It contains NO game data, no
music, no movies and no other Electronic Arts assets: you must own the game. It adds a 64-bit (AMD64) Direct3D 11 version of
the Zero Hour engine, built from the open GeneralsGameCode sources (GPL-3.0, see MKRaudstein\LICENSE.md).
Targeted at the current Steam release of Command & Conquer Generals Zero Hour (the folder usually is
C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour).

HOW TO INSTALL
 1. Install the game in Steam and start it once normally (to prove the unmodified game works). Exit it.
 2. OPTIONAL BACKUP: copy Generals.exe in the game folder to Generals.exe.original (the next step overwrites it).
 3. Extract this ZIP directly into the Zero Hour game folder. Choose "Replace the files in the destination" when asked.
    The files are listed below and in MKRaudstein\manifest.json. Only Generals.exe replaces an existing file.
 4. Start Zero Hour with the normal PLAY button in Steam. Steam starts Generals.exe, which is now the 64-bit runtime.
 No installer, no registry changes, no administrator rights beyond what Windows needs to write to the game folder.

WHAT IS IN THE ZIP
$(($files | Sort-Object Destination | ForEach-Object { '  ' + $_.Destination + '  - ' + $_.Replaces }) -join "`r`n")

HOW TO CHECK THAT THE 64-BIT RUNTIME IS RUNNING
 Task Manager > Details: the process "Generals.exe" shows Architecture "x64" (no "(32 bit)"). After a run the game folder
 contains generals64.log, whose first lines say normal_launch.

KNOWN LIMITATIONS OF THIS PREVIEW
 - Movie (Bink) playback does not exist in the 64-bit build: movies are skipped. Campaign briefing movies, the General's
   Challenge video screens and the intro movies are therefore absent; the mission or the static screens follow immediately.
 - The Enhanced Zero Hour HUD is not implemented yet (the classic Zero Hour control bar is used).
 - Three Zero Hour-only options are hidden in the Modern options pages; animated texture effects (rotating/offset
   textures) are static. These and more are planned for later milestones (X64.47 and later).
 - Mission victory / progression has not been validated by a human yet; LAN / online play is not supported.
 - Only the Steam release of Zero Hour (English) has been tested.

HOW TO GO BACK
 In Steam: right-click Zero Hour > Properties > Installed Files > "Verify integrity of game files". This restores the original
 Generals.exe. The other files of this package are extra files that Steam leaves alone; they are harmless and can be deleted
 by hand (the list above): vcruntime140.dll, vcruntime140_1.dll, msvcp140.dll, msvcp140_atomic_wait.dll, the MKRaudstein folder
 and this README.

SAVES AND SETTINGS
 The game uses the same user-data folder as the original (Documents\Command and Conquer Generals Zero Hour Data). Back it up
 before testing if you care about your saves and settings.

PROJECT
 Sources: https://github.com/martinkraud/GeneralsGameCode (branch dev/modern-engine), built from commit $commitFull.
 Based on https://github.com/TheSuperHackers/GeneralsGameCode and the source code released by Electronic Arts under the GPL.
 "Command & Conquer", "Generals" and "Zero Hour" are trademarks of Electronic Arts Inc. This project is not affiliated with EA.
"@ -replace "`r?`n", "`r`n"
Add-File 'MKRaudstein-ZeroHour-README.txt' $null 'generated by New-ZeroHourOverlayPackage.ps1' 'Explains the preview, installation, limitations and restoration.' 'project-owned' 'required (documentation)' 'new file' ([Text.Encoding]::UTF8.GetBytes($readme))

# hashes + manifest (the manifest inside the ZIP lists every other file with its SHA-256)
foreach ($f in $files) {
    $bytes = if ($f.Content) { $f.Content } else { [IO.File]::ReadAllBytes($f.Source) }
    $f | Add-Member -NotePropertyName Bytes -NotePropertyValue $bytes
    $f | Add-Member -NotePropertyName Sha256 -NotePropertyValue ([BitConverter]::ToString([Security.Cryptography.SHA256]::HashData($bytes)).Replace('-', ''))
}
$manifest = [ordered]@{
    package = "MKRaudstein-ZeroHour-x64-Preview-$Version"; status = 'PREVIEW - not a public release'; sourceCommit = $commitFull
    targetGame = 'Command & Conquer Generals Zero Hour (Steam, English); extract into the game folder; overwrite when asked'
    imports = @($imports | Sort-Object -Unique)
    files = @($files | Sort-Object Destination | ForEach-Object {
        [ordered]@{ file = $_.Destination; sourceOrBuildTarget = $_.BuildTarget; reason = $_.Reason; ownership = $_.Ownership; requirement = $_.Requirement
                    replaces = $_.Replaces; size = $_.Bytes.Length; sha256 = $_.Sha256 } })
}
$manifestBytes = [Text.Encoding]::UTF8.GetBytes((($manifest | ConvertTo-Json -Depth 6) -replace "`r?`n", "`r`n"))
Add-File 'MKRaudstein\manifest.json' $null 'generated by New-ZeroHourOverlayPackage.ps1' 'Machine-readable overlay contract (what each file is, why, and its SHA-256).' 'project-owned' 'required (documentation)' 'new file' $manifestBytes
$files[-1] | Add-Member -NotePropertyName Bytes -NotePropertyValue $manifestBytes
$files[-1] | Add-Member -NotePropertyName Sha256 -NotePropertyValue ([BitConverter]::ToString([Security.Cryptography.SHA256]::HashData($manifestBytes)).Replace('-', ''))

# ---- deterministic ZIP -----------------------------------------------------------------------------------------------------------------
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$zipName = "MKRaudstein-ZeroHour-x64-Preview-$Version.zip"
$zipPath = Join-Path $OutputDirectory $zipName
function Write-Zip([string]$Path, $Entries) {
    $temp = "$Path.tmp"
    if (Test-Path -LiteralPath $temp) { [IO.File]::Delete($temp) }
    $stream = [IO.File]::Open($temp, 'CreateNew')
    try {
        $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $false)
        try {
            foreach ($e in ($Entries | Sort-Object { $_.Destination } -CaseSensitive)) {
                $entry = $zip.CreateEntry(($e.Destination -replace '\\', '/'), [IO.Compression.CompressionLevel]::Optimal)
                $entry.LastWriteTime = [DateTimeOffset]::new(2026, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
                $out = $entry.Open()
                try { $out.Write($e.Bytes, 0, $e.Bytes.Length) } finally { $out.Dispose() }
            }
        } finally { $zip.Dispose() }
    } finally { $stream.Dispose() }
    if (Test-Path -LiteralPath $Path) { [IO.File]::Delete($Path) }
    [IO.File]::Move($temp, $Path)
}
Write-Zip $zipPath $files
$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash
"$zipHash  $zipName" | Set-Content -LiteralPath "$zipPath.sha256" -Encoding ascii
([ordered]@{ zip = $zipName; zipSha256 = $zipHash; zipSize = (Get-Item -LiteralPath $zipPath).Length; manifest = $manifest } | ConvertTo-Json -Depth 8) |
    Set-Content -LiteralPath (Join-Path $OutputDirectory "MKRaudstein-ZeroHour-x64-Preview-$Version.manifest.json") -Encoding utf8

# optional symbols archive (separate, never part of the player ZIP)
$symbolsZip = $null
if (Test-Path -LiteralPath $SymbolsPath -PathType Leaf) {
    $symbolsZip = Join-Path $OutputDirectory "MKRaudstein-ZeroHour-x64-Preview-$Version-Symbols.zip"
    $sym = [pscustomobject]@{ Destination = [IO.Path]::GetFileName($SymbolsPath); Bytes = [IO.File]::ReadAllBytes($SymbolsPath) }
    Write-Zip $symbolsZip @($sym)
}
Write-Host "Package   : $zipPath"
Write-Host "Size      : $((Get-Item -LiteralPath $zipPath).Length) bytes"
Write-Host "SHA-256   : $zipHash"
Write-Host "Commit    : $commitFull"
if ($symbolsZip) { Write-Host "Symbols   : $symbolsZip (optional, developers only)" }
[pscustomobject]@{ Zip = $zipPath; Sha256 = $zipHash; Version = $Version; Commit = $commitFull; Symbols = $symbolsZip }
