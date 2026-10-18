#Requires -Version 7.0
# X64.47A / X64.47B / X64.48B: builds the PREVIEW player overlay ZIP of a title (Zero Hour by default, -Title Generals) from its CANONICAL STAGED CANDIDATE.
#
# DISTRIBUTION RULE (non-negotiable): for each title   validated staged MKRaudstein runtime  ==  clean legitimate Steam installation + this ZIP.
# The contents are therefore DERIVED, never chosen by a hand-written list:
#   every file of the staged candidate's game directory is classified against the stage manifest (the legitimate game files that were copied in, with
#   their SHA-256):
#     * byte-identical to a legitimate game file  -> supplied by the installation, NOT redistributed (recorded in the parity file only);
#     * the staged executable                      -> shipped as Generals.exe (the file Steam's Play button starts; see X64_47A);
#     * the .pdb                                   -> the optional Symbols archive (not player-facing functionality);
#     * ANYTHING else (project-generated or project-modified runtime data, e.g. the Generals "modern extensions" INI/WND overrides) -> shipped at
#       the same relative path.
#   Plus the app-local Microsoft Visual C++ runtime DLLs the executable imports (the stage relies on the machine's), the README, the licence and the manifest.
# The validator (Test-ZeroHourOverlayPackage.ps1) fails if the ZIP and the stage ever disagree. Nothing is installed anywhere.
# Deterministic: the same inputs give the same ZIP bytes (sorted entries, fixed timestamps, no build paths).
[CmdletBinding()]
param([ValidateSet('ZeroHour', 'Generals')][string]$Title = 'ZeroHour',
      [Parameter(Mandatory)][string]$Candidate,   # name of the staged, preflighted native candidate (build\dev-runtimes\native\<Title>\<Candidate>)
      [string]$RedistDirectory,                   # directory with the x64 Microsoft.VC14x.CRT DLLs; default: newest Visual Studio redistributable
      [string]$OutputDirectory,                   # default: build\dist
      [string]$Version,                           # default: the short commit id
      [switch]$AllowDirtyTree)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
. (Join-Path $PSScriptRoot 'OverlayPackageCommon.ps1')
$repo = $script:ZHRepository
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

# Title-specific facts (everything else is shared). Steam launches Generals.exe for both titles (appinfo launch configuration).
$T = if ($Title -eq 'Generals') {
    @{ Display = 'Generals'; Full = 'Command & Conquer Generals'; Release = 'Command & Conquer Generals'; File = 'Generals'
       Folder = 'C:\Program Files (x86)\Steam\steamapps\common\Command and Conquer Generals'; Data = 'Documents\Command and Conquer Generals Data'
       Limits = @(' - Movie (Bink) playback does not exist in the 64-bit build: movies are skipped (intro, campaign briefing and cutscene movies).',
                  ' - Mission victory / progression has not been validated by a human on the preview; LAN / online play is not supported.',
                  ' - Only the Steam release of Generals (English) is targeted.') }
} else {
    @{ Display = 'Zero Hour'; Full = 'Command & Conquer Generals: Zero Hour'; Release = 'Command & Conquer Generals Zero Hour'; File = 'ZeroHour'
       Folder = 'C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour'; Data = 'Documents\Command and Conquer Generals Zero Hour Data'
       Limits = @(' - Movie (Bink) playback does not exist in the 64-bit build: movies are skipped. Campaign briefing movies, the General''s',
                  '   Challenge video screens and the intro movies are therefore absent; the mission or the static screens follow immediately.',
                  ' - The Enhanced Zero Hour HUD is not implemented yet (the classic Zero Hour control bar is used).',
                  ' - Three Zero Hour-only options are hidden in the Modern options pages; animated texture effects (rotating/offset',
                  '   textures) are static. These and more are planned for later milestones (X64.47 and later).',
                  ' - Mission victory / progression has not been validated by a human yet; LAN / online play is not supported.',
                  ' - Only the Steam release of Zero Hour (English) has been tested.') }
}
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo 'build\dist' }

Push-Location $repo
try {
    $commit = (git rev-parse --short=9 HEAD).Trim()
    $commitFull = (git rev-parse HEAD).Trim()
    $dirty = @(git status --porcelain --untracked-files=no)
} finally { Pop-Location }
if ($dirty.Count -and -not $AllowDirtyTree) { throw "Tracked files have uncommitted changes; the package records the commit it was built from. Commit first (or -AllowDirtyTree for a throw-away test build).`n$($dirty -join "`n")" }
if (-not $Version) { $Version = $commit }

# ---- the canonical staged candidate -----------------------------------------------------------------------------------------------------------
$manifestStage = Read-NativeOrdinaryPlayStage $Title $Candidate $script:NativeNormalLaunchGenerated -AllowCrashDumps    # fails unless it is an intact, preflighted stage
$game = $manifestStage.GamePath
$stageManifestHash = (Get-FileHash -LiteralPath (Join-Path $manifestStage.StagePath 'manifest.json') -Algorithm SHA256).Hash
$builtExe = [IO.Path]::GetFileName($manifestStage.Build.Executable)
$builtPdb = [IO.Path]::GetFileName($manifestStage.Build.Pdb)
$gameFull = [IO.Path]::GetFullPath($game).TrimEnd('\') + '\'
$legit = @{}      # relative path (lower case, '/') -> SHA-256 of the legitimate game file the stage was built from
foreach ($a in $manifestStage.Assets) { $legit[($a.Path -replace '\\', '/').ToLowerInvariant()] = $a.SHA256 }
$exeCandidate = Join-Path $game $builtExe
if ((Get-PeMachineFromFile $exeCandidate) -ne 0x8664) { throw "$exeCandidate is not an AMD64 (x64) executable." }

function Get-Reason([string]$Relative) {
    if ($Relative -match '^(?i)Data/INI/(CommandButton|CommandSet|PlayerTemplate)\.ini$') { return 'Generals modern extensions: the Zero Hour style special-power shortcut sidebar data (a loose override of the same-named file inside the game archives).' }
    if ($Relative -match '^(?i)Window/GenPowersShortcutBar\w+\.wnd$') { return 'Generals modern extensions: the re-laid-out special-power shortcut sidebar window (a loose override of the same-named file inside the game archives).' }
    return 'Project runtime file of the validated staged candidate (derived, not chosen by hand).'
}

$files = [System.Collections.Generic.List[object]]::new()
function Add-File([string]$Destination, [string]$Source, [string]$BuildTarget, [string]$Reason, [string]$Ownership, [string]$Requirement, [string]$Replaces, [byte[]]$Content) {
    $files.Add([pscustomobject]@{ Destination = $Destination; Source = $Source; BuildTarget = $BuildTarget; Reason = $Reason; Ownership = $Ownership
                                  Requirement = $Requirement; Replaces = $Replaces; Content = $Content })
}
$skippedLegit = 0; $symbols = $null; $ignored = @()
foreach ($f in (Get-ChildItem -LiteralPath $game -Recurse -File | Sort-Object FullName)) {
    $relative = $f.FullName.Substring($gameFull.Length) -replace '\\', '/'
    $key = $relative.ToLowerInvariant()
    if ($relative -in $script:NativeNormalLaunchGenerated -or $f.FullName -match '\\CrashDumps\\') { $ignored += $relative; continue }   # run artifacts, never part of the runtime
    if ($legit.ContainsKey($key) -and (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash -eq $legit[$key]) { ++$skippedLegit; continue }   # the installation's own file
    if ($relative -ieq $builtPdb) { $symbols = $f.FullName; continue }
    if ($relative -ieq $builtExe) {
        Add-File 'Generals.exe' $f.FullName "$builtExe of the staged candidate $Candidate" `
            "The AMD64 / Direct3D 11 $($T.Display) runtime. Steam's Play button starts Generals.exe, so the runtime is shipped under that name." `
            'project-owned (GPL-3.0-or-later, see MKRaudstein\LICENSE.md)' 'required' `
            'REPLACES the original Electronic Arts launcher Generals.exe of the Steam installation (restore with Steam: Verify integrity of game files)' $null
        continue
    }
    $replaces = if ($legit.ContainsKey($key)) { "REPLACES the game file $relative (a modified copy; restore with Steam: Verify integrity of game files)" } else { 'new file (not part of the Steam installation)' }
    Add-File ($relative -replace '/', '\') $f.FullName "$relative of the staged candidate $Candidate" (Get-Reason $relative) 'project-generated / project-modified runtime data' 'required' $replaces $null
}
if (-not ($files | Where-Object Destination -eq 'Generals.exe')) { throw "The staged candidate has no $builtExe" }

# ---- app-local Microsoft runtime (the stage relies on the machine's; a clean PC may not have it) -------------------------------------------------
$imports = Get-PeImportNames $exeCandidate
$runtimeNames = @($imports | Where-Object { $_ -match '^(vcruntime|msvcp|concrt|vccorlib)' } | Sort-Object -Unique)
if (-not $RedistDirectory) {
    $RedistDirectory = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Redist\MSVC\*\x64\Microsoft.VC14*.CRT' -Directory -ErrorAction SilentlyContinue |
        Sort-Object { [version]($_.FullName -replace '^.*\\MSVC\\([^\\]+)\\.*$', '$1') } -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $RedistDirectory -or -not (Test-Path -LiteralPath $RedistDirectory)) { throw 'No x64 Microsoft Visual C++ redistributable directory found; pass -RedistDirectory.' }
foreach ($imported in $runtimeNames) {
    if (-not (Test-Path -LiteralPath (Join-Path $RedistDirectory $imported))) { throw "Redistributable DLL missing: $imported in $RedistDirectory" }
    $n = $imported.ToLowerInvariant()
    Add-File $n (Join-Path $RedistDirectory $imported) 'Microsoft Visual C++ x64 redistributable (Visual Studio)' `
        "Imported by Generals.exe ($n); shipped app-local so the player need not install the Visual C++ x64 redistributable." `
        'Microsoft redistributable (app-local deployment of the Visual C++ runtime)' 'required' 'new file (not part of the Steam installation)' $null
}
Add-File 'MKRaudstein\LICENSE.md' (Join-Path $repo 'LICENSE.md') 'repository LICENSE.md' 'GPL-3.0-or-later with the Electronic Arts additional terms: required for redistributing the executable.' `
    'project-owned' 'required' 'new file' $null

# ---- README (generated from the derived list; same text for the same inputs) -------------------------------------------------------------------
$replaced = @($files | Where-Object { $_.Replaces -like 'REPLACES*' } | ForEach-Object { $_.Destination -replace '\\', '/' } | Sort-Object)
$dataFiles = @($files | Where-Object { $_.Ownership -like 'project-generated*' })
$dataNote = if ($dataFiles.Count) { "`r`n`r`nPROJECT RUNTIME DATA IN THIS ZIP ($($dataFiles.Count) files)`r`nThese files are part of the finished MKRaudstein runtime and are required for its features (they are loose files that the game prefers over the same-named content inside its archives). They were generated for the validated staged runtime; they are not optional. Removing them restores the original behaviour of those features." } else { '' }
$readme = @"
MKRaudstein $($T.Display) x64 - PREVIEW $Version
Modern Experience by MKRaudstein | 64-bit * DirectX 11
================================================================

STATUS: PREVIEW / TESTING PACKAGE. NOT a public release. It is meant for testing on a second PC.

WHAT THIS IS
An overlay for an EXISTING, legitimate installation of $($T.Full). It contains the COMPLETE MKRaudstein runtime of this title
(the executable and every project runtime file it needs) but NO unchanged game data: no archives, music, movies, textures or other
Electronic Arts assets: you must own the game. It is a 64-bit (AMD64) Direct3D 11 version of the $($T.Display) engine, built from the open
GeneralsGameCode sources (GPL-3.0, see MKRaudstein\LICENSE.md).
Targeted at the current Steam release of $($T.Release) (the folder usually is
$($T.Folder)).

HOW TO INSTALL
 1. Install the game in Steam and start it once normally (to prove the unmodified game works). Exit it.
 2. OPTIONAL BACKUP: copy Generals.exe in the game folder to Generals.exe.original (the next step overwrites it).
 3. Extract this ZIP directly into the $($T.Display) game folder. Choose "Replace the files in the destination" when asked.
    Every file is listed below and in MKRaudstein\manifest.json. Files that replace an existing file: $($replaced -join ', ').
 4. Start $($T.Display) with the normal PLAY button in Steam. Steam starts Generals.exe, which is now the 64-bit runtime.
 No installer, no registry changes, no administrator rights beyond what Windows needs to write to the game folder.

WHAT IS IN THE ZIP
$(($files | Sort-Object Destination | ForEach-Object { '  ' + $_.Destination + '  - ' + $_.Replaces }) -join "`r`n")$dataNote

HOW TO CHECK THAT THE 64-BIT RUNTIME IS RUNNING
 Task Manager > Details: the process "Generals.exe" shows Architecture "x64" (no "(32 bit)"). After a run the game folder
 contains generals64.log, whose first lines say normal_launch.

KNOWN LIMITATIONS OF THIS PREVIEW
$($T.Limits -join "`r`n")

HOW TO GO BACK
 In Steam: right-click $($T.Display) > Properties > Installed Files > "Verify integrity of game files". This restores the original
 Generals.exe (and any other replaced file above). The other files of this package are extra files that Steam leaves alone; they are
 harmless and can be deleted by hand (the list above; the MKRaudstein folder and this README too).

SAVES AND SETTINGS
 The game uses the same user-data folder as the original ($($T.Data)). Back it up
 before testing if you care about your saves and settings.

PROJECT
 Sources: https://github.com/martinkraud/GeneralsGameCode (branch dev/modern-engine), built from commit $commitFull.
 Based on https://github.com/TheSuperHackers/GeneralsGameCode and the source code released by Electronic Arts under the GPL.
 "Command & Conquer", "Generals" and "Zero Hour" are trademarks of Electronic Arts Inc. This project is not affiliated with EA.
"@ -replace "`r?`n", "`r`n"
Add-File "MKRaudstein-$($T.File)-README.txt" $null 'generated by New-ZeroHourOverlayPackage.ps1' 'Explains the preview, installation, limitations and restoration.' 'project-owned' 'required (documentation)' 'new file' ([Text.Encoding]::UTF8.GetBytes($readme))

# ---- hashes + manifest (the manifest inside the ZIP lists every other file with its SHA-256) --------------------------------------------------
foreach ($f in $files) {
    $bytes = if ($f.Content) { $f.Content } else { [IO.File]::ReadAllBytes($f.Source) }
    $f | Add-Member -NotePropertyName Bytes -NotePropertyValue $bytes
    $f | Add-Member -NotePropertyName Sha256 -NotePropertyValue ([BitConverter]::ToString([Security.Cryptography.SHA256]::HashData($bytes)).Replace('-', ''))
}
$manifest = [ordered]@{
    package = "MKRaudstein-$($T.File)-x64-Preview-$Version"; status = 'PREVIEW - not a public release'; sourceCommit = $commitFull
    canonicalStagedCandidate = $Candidate; stageManifestSha256 = $stageManifestHash
    rule = 'validated staged runtime == clean Steam installation + this ZIP; contents derived from the staged candidate (file classification), not from a hand-written list'
    targetGame = "$($T.Release) (Steam, English); extract into the game folder; overwrite when asked"
    legitimateGameFilesNotRedistributed = $skippedLegit
    imports = @($imports | Sort-Object -Unique)
    files = @($files | Sort-Object Destination | ForEach-Object {
        [ordered]@{ file = $_.Destination; sourceOrBuildTarget = $_.BuildTarget; reason = $_.Reason; ownership = $_.Ownership; requirement = $_.Requirement
                    replaces = $_.Replaces; size = $_.Bytes.Length; sha256 = $_.Sha256 } })
}
$manifestBytes = [Text.Encoding]::UTF8.GetBytes((($manifest | ConvertTo-Json -Depth 6) -replace "`r?`n", "`r`n"))
Add-File 'MKRaudstein\manifest.json' $null 'generated by New-ZeroHourOverlayPackage.ps1' 'Machine-readable overlay contract (what each file is, why, and its SHA-256).' 'project-owned' 'required (documentation)' 'new file' $manifestBytes
$files[-1] | Add-Member -NotePropertyName Bytes -NotePropertyValue $manifestBytes
$files[-1] | Add-Member -NotePropertyName Sha256 -NotePropertyValue ([BitConverter]::ToString([Security.Cryptography.SHA256]::HashData($manifestBytes)).Replace('-', ''))

# ---- deterministic ZIP ---------------------------------------------------------------------------------------------------------------------------
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$zipName = "MKRaudstein-$($T.File)-x64-Preview-$Version.zip"
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
    Set-Content -LiteralPath (Join-Path $OutputDirectory "MKRaudstein-$($T.File)-x64-Preview-$Version.manifest.json") -Encoding utf8

# The parity record the validator compares against (NOT part of the ZIP): every project file of the staged candidate by relative path and hash,
# the executable mapping, the count of legitimate game files and the stage manifest hash.
$parity = [ordered]@{
    title = $Title; candidate = $Candidate; stageManifestSha256 = $stageManifestHash; stagedExecutable = $builtExe; stagedSymbols = $builtPdb
    legitimateGameFilesNotRedistributed = $skippedLegit
    projectFiles = @($files | Where-Object { $_.Ownership -like 'project-generated*' -or $_.Destination -eq 'Generals.exe' } | Sort-Object Destination | ForEach-Object {
        [ordered]@{ stagePath = if ($_.Destination -eq 'Generals.exe') { $builtExe } else { $_.Destination -replace '\\', '/' }; zipPath = $_.Destination -replace '\\', '/'; sha256 = $_.Sha256 } })
}
($parity | ConvertTo-Json -Depth 6) | Set-Content -LiteralPath (Join-Path $OutputDirectory "MKRaudstein-$($T.File)-x64-Preview-$Version.stage-parity.json") -Encoding utf8

# optional symbols archive (separate, never part of the player ZIP)
$symbolsZip = $null
if ($symbols) {
    $symbolsZip = Join-Path $OutputDirectory "MKRaudstein-$($T.File)-x64-Preview-$Version-Symbols.zip"
    $sym = [pscustomobject]@{ Destination = [IO.Path]::GetFileName($symbols); Bytes = [IO.File]::ReadAllBytes($symbols) }
    Write-Zip $symbolsZip @($sym)
}
Write-Host "Package   : $zipPath"
Write-Host "Size      : $((Get-Item -LiteralPath $zipPath).Length) bytes"
Write-Host "SHA-256   : $zipHash"
Write-Host "Commit    : $commitFull"
Write-Host "Staged candidate: $Candidate ($($files.Count) packaged files, $skippedLegit legitimate game files not redistributed, $($dataFiles.Count) project data files)"
if ($symbolsZip) { Write-Host "Symbols   : $symbolsZip (optional, developers only)" }
[pscustomobject]@{ Zip = $zipPath; Sha256 = $zipHash; Version = $Version; Commit = $commitFull; Symbols = $symbolsZip; Files = ($files | ForEach-Object Destination) }
