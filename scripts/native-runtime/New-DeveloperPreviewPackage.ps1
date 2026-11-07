#Requires -Version 7.0
# Developer Preview distribution: wraps the validated OVERLAY ZIP of a title (New-ZeroHourOverlayPackage.ps1 + Test-ZeroHourOverlayPackage.ps1) into the PLAYER package:
#
#   MKRaudstein-<Title>-DeveloperPreview-<commit>/
#       Install.cmd  Uninstall.cmd  README-FIRST.txt  package-manifest.json  tools/*.ps1     <- what the player runs / reads
#       payload/...                                                                           <- the overlay files, in game-folder layout (Generals.exe x64, VC++ DLLs, loose data)
#
# The payload is DERIVED from the overlay ZIP (never chosen by hand), so "validated staged runtime == retail installation + payload" still holds.
# The package contains NO Electronic Arts asset (the validator Test-DeveloperPreviewPackage.ps1 fails if one ever appears) and no PDB (separate Symbols ZIP).
# Deterministic: same inputs -> same bytes (sorted entries, fixed timestamps, CRLF text). The identity (designation, commit, commit date, arch, title) comes from
# scripts/distribution/identity.json + git, never from the build machine.
[CmdletBinding()]
param([ValidateSet('ZeroHour', 'Generals')][string]$Title = 'ZeroHour',
      [string]$OverlayZip,              # default: build\dist\MKRaudstein-<Title>-x64-Preview-<Version>.zip
      [string]$OutputDirectory,         # default: build\dist
      [string]$Version,                 # default: the short commit id (must match the overlay ZIP)
      [switch]$AllowDirtyTree)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
$repo = $script:ZHRepository
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$dist = Join-Path $repo 'scripts\distribution'

$T = if ($Title -eq 'Generals') {
    @{ Display = 'Command & Conquer Generals'; Short = 'Generals'; File = 'Generals'; Folders = @('Command and Conquer Generals')
       Required = @('INI.big', 'W3D.big', 'Textures.big', 'Terrain.big', 'Window.big', 'Generals.exe'); Data = 'Documents\Command and Conquer Generals Data' }
} else {
    @{ Display = 'Command & Conquer Generals: Zero Hour'; Short = 'Zero Hour'; File = 'ZeroHour'; Folders = @('Command & Conquer Generals - Zero Hour')
       Required = @('INIZH.big', 'W3DZH.big', 'TexturesZH.big', 'TerrainZH.big', 'WindowZH.big', 'MapsZH.big', 'Generals.exe'); Data = 'Documents\Command and Conquer Generals Zero Hour Data' }
}
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo 'build\dist' }

Push-Location $repo
try {
    $commit = (git rev-parse --short=9 HEAD).Trim()
    $commitFull = (git rev-parse HEAD).Trim()
    $buildDate = (git show -s --format=%cI HEAD).Trim()
    $dirty = @(git status --porcelain --untracked-files=no)
} finally { Pop-Location }
if ($dirty.Count -and -not $AllowDirtyTree) { throw "Tracked files have uncommitted changes; a Developer Preview package records the commit it was built from. Commit first.`n$($dirty -join "`n")" }
if (-not $Version) { $Version = $commit }
$identity = Get-Content -LiteralPath (Join-Path $dist 'identity.json') -Raw -Encoding utf8 | ConvertFrom-Json
if (-not $OverlayZip) { $OverlayZip = Join-Path $OutputDirectory "MKRaudstein-$($T.File)-x64-Preview-$Version.zip" }
if (-not (Test-Path -LiteralPath $OverlayZip)) { throw "Overlay ZIP missing: $OverlayZip (run New-MKRaudsteinDistribution.ps1 first; it builds and VALIDATES the overlay)." }
$overlayName = [IO.Path]::GetFileName($OverlayZip)
$parityPath = [IO.Path]::ChangeExtension($OverlayZip, $null).TrimEnd('.') + '.stage-parity.json'
$overlayTestPath = [IO.Path]::ChangeExtension($OverlayZip, $null).TrimEnd('.') + '.overlay-test.json'
if (-not (Test-Path -LiteralPath $overlayTestPath)) { throw "The overlay ZIP has no validation record ($overlayTestPath): only a validated overlay may become a Developer Preview package." }
$parity = Get-Content -LiteralPath $parityPath -Raw | ConvertFrom-Json

# retail archive sizes (to recognise the supported Steam release): taken from the staged candidate's asset list
$stageManifest = Get-Content -LiteralPath (Join-Path $repo "build\dev-runtimes\native\$Title\$($parity.candidate)\manifest.json") -Raw -Encoding utf8 | ConvertFrom-Json
$required = @(foreach ($name in $T.Required) {
        if ($name -eq 'Generals.exe') { continue }
        $a = @($stageManifest.Assets | Where-Object { $_.Path -ieq $name })[0]
        if (-not $a) { throw "The staged candidate has no retail file $name to record as a required file." }
        [ordered]@{ path = $name; size = [int64]$a.Length }
    })
$required += [ordered]@{ path = 'Generals.exe'; size = 0 }

# ---- payload: the overlay ZIP's entries (minus its own README, replaced by README-FIRST) -----------------------------------------------------------
$entries = [System.Collections.Generic.List[object]]::new()      # Destination (zip path), Bytes
function Add-Entry([string]$Path, [byte[]]$Bytes) { $entries.Add([pscustomobject]@{ Destination = $Path; Bytes = $Bytes; Sha256 = [BitConverter]::ToString([Security.Cryptography.SHA256]::HashData($Bytes)).Replace('-', '') }) }
$payload = [System.Collections.Generic.List[object]]::new()
$zipIn = [IO.Compression.ZipFile]::OpenRead($OverlayZip)
try {
    foreach ($e in ($zipIn.Entries | Sort-Object FullName -CaseSensitive)) {
        if ($e.FullName -match "^MKRaudstein-$($T.File)-README\.txt$") { continue }
        $ms = [IO.MemoryStream]::new(); $s = $e.Open(); try { $s.CopyTo($ms) } finally { $s.Dispose() }
        $bytes = $ms.ToArray()
        $payload.Add([pscustomobject]@{ Path = ($e.FullName -replace '/', '\'); Bytes = $bytes; Size = $bytes.Length; Sha256 = [BitConverter]::ToString([Security.Cryptography.SHA256]::HashData($bytes)).Replace('-', '') })
    }
} finally { $zipIn.Dispose() }
if (-not ($payload | Where-Object Path -eq 'Generals.exe')) { throw 'The overlay ZIP has no Generals.exe.' }

# EA-derived data must not be shipped. The Generals "modern extensions" files are modified copies of the game's own INI / WND data: the overlay ZIP carries them
# (it is a developer-side artifact), the PLAYER package does not. Its installer regenerates them from the player's own archives (tools/GeneralsModernExtensions.ps1, a
# byte-identical port of modern_extensions.py); the expected hashes below let the installer say whether the player's archives match the tested release.
$derivedPattern = '^(?i)(Data\\INI\\(CommandButton|CommandSet|PlayerTemplate)\.ini|Window\\GenPowersShortcutBar\w+\.wnd)$'
$derived = @($payload | Where-Object { $_.Path -match $derivedPattern })
$payload = [System.Collections.Generic.List[object]]@($payload | Where-Object { $_.Path -notmatch $derivedPattern })
if ($Title -eq 'ZeroHour' -and $derived.Count) { throw "The Zero Hour overlay unexpectedly carries EA-derived data files: $(($derived | ForEach-Object Path) -join ', ')" }
if ($Title -eq 'Generals' -and $derived.Count -ne 6) { throw "The Generals overlay should carry exactly the 6 generated extension files, found $($derived.Count)." }

function Get-TextBytes([string]$Text) { [Text.Encoding]::UTF8.GetBytes(($Text -replace "`r?`n", "`r`n")) }
function Get-Tool([string]$Name) { [IO.File]::ReadAllBytes((Join-Path $dist $Name)) }

$packageName = "MKRaudstein-$($T.File)-DeveloperPreview-$Version"
$manifest = [ordered]@{
    schema = 'mkraudstein-package/1'
    identity = [ordered]@{ designation = $identity.designation; title = $Title; displayTitle = $T.Display; commit = $commit; commitFull = $commitFull; buildDate = $buildDate
                           arch = $identity.architecture; graphics = $identity.graphics; branding = $identity.branding; packageName = $packageName }
    target = [ordered]@{ title = $Title; displayName = $T.Short; steamFolderNames = @($T.Folders | ForEach-Object { $_ }); requiredFiles = $required; userDataFolder = $T.Data }
    generatedArtifacts = @($script:NativeNormalLaunchGenerated)
    overlayPackage = [ordered]@{ name = $overlayName; zipSha256 = (Get-FileHash -LiteralPath $OverlayZip -Algorithm SHA256).Hash; stageCandidate = $parity.candidate; stageManifestSha256 = $parity.stageManifestSha256 }
    containsRetailAssets = $false
    generators = @(if ($derived.Count) { 'generals-modern-extensions' })
    generated = @($derived | Sort-Object Path | ForEach-Object { [ordered]@{ path = $_.Path; size = $_.Size; sha256 = $_.Sha256 } })
    files = @($payload | Sort-Object Path | ForEach-Object { [ordered]@{ path = $_.Path; size = $_.Size; sha256 = $_.Sha256 } })
}
$manifestBytes = Get-TextBytes ($manifest | ConvertTo-Json -Depth 8)

# ---- README-FIRST from the template (same text for the same inputs) ----------------------------------------------------------------------------
$tpl = Get-Content -LiteralPath (Join-Path $dist 'README-FIRST.template.txt') -Raw -Encoding utf8
$readme = $tpl.Replace('{{TITLE}}', $T.Display).Replace('{{SHORT}}', $T.Short).Replace('{{DESIGNATION}}', $identity.designation).Replace('{{COMMIT}}', $commit).Replace('{{COMMITFULL}}', $commitFull).
    Replace('{{BUILDDATE}}', $buildDate).Replace('{{ARCH}}', $identity.architecture).Replace('{{BRANDING}}', $identity.branding).Replace('{{FOLDER}}', $T.Folders[0]).Replace('{{DATA}}', $T.Data)
if ($readme -match '\{\{[A-Z]+\}\}') { throw "README-FIRST.template.txt has an unreplaced token: $($Matches[0])" }

$root = "$packageName/"
Add-Entry ($root + 'Install.cmd') (Get-TextBytes ([IO.File]::ReadAllText((Join-Path $dist 'Install.cmd'))))
Add-Entry ($root + 'Uninstall.cmd') (Get-TextBytes ([IO.File]::ReadAllText((Join-Path $dist 'Uninstall.cmd'))))
Add-Entry ($root + 'README-FIRST.txt') (Get-TextBytes $readme)
Add-Entry ($root + 'package-manifest.json') $manifestBytes
$tools = @('MKRaudstein-Setup.ps1', 'Install-MKRaudstein.ps1', 'Uninstall-MKRaudstein.ps1') + @(if ($derived.Count) { 'GeneralsModernExtensions.ps1' })
foreach ($tool in $tools) { Add-Entry ($root + 'tools/' + $tool) (Get-TextBytes ([IO.File]::ReadAllText((Join-Path $dist $tool)))) }
foreach ($p in $payload) { Add-Entry ($root + 'payload/' + ($p.Path -replace '\\', '/')) $p.Bytes }

# ---- deterministic ZIP ---------------------------------------------------------------------------------------------------------------------------
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
function Write-Zip([string]$Path, $Items) {
    $temp = "$Path.tmp"
    if (Test-Path -LiteralPath $temp) { [IO.File]::Delete($temp) }
    $stream = [IO.File]::Open($temp, 'CreateNew')
    try {
        $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $false)
        try {
            foreach ($e in ($Items | Sort-Object { $_.Destination } -CaseSensitive)) {
                $entry = $zip.CreateEntry($e.Destination, [IO.Compression.CompressionLevel]::Optimal)
                $entry.LastWriteTime = [DateTimeOffset]::new(2026, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
                $out = $entry.Open()
                try { $out.Write($e.Bytes, 0, $e.Bytes.Length) } finally { $out.Dispose() }
            }
        } finally { $zip.Dispose() }
    } finally { $stream.Dispose() }
    if (Test-Path -LiteralPath $Path) { [IO.File]::Delete($Path) }
    [IO.File]::Move($temp, $Path)
}
$zipPath = Join-Path $OutputDirectory "$packageName.zip"
Write-Zip $zipPath $entries
$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash
"$zipHash  $packageName.zip" | Set-Content -LiteralPath "$zipPath.sha256" -Encoding ascii

# symbols stay out of the player package: re-publish the overlay's Symbols ZIP under the Developer Preview name
$overlaySymbols = [IO.Path]::ChangeExtension($OverlayZip, $null).TrimEnd('.') + '-Symbols.zip'
$symbolsZip = $null
if (Test-Path -LiteralPath $overlaySymbols) {
    $symbolsZip = Join-Path $OutputDirectory "$packageName-Symbols.zip"
    Copy-Item -LiteralPath $overlaySymbols -Destination $symbolsZip -Force
}
Write-Host "Package   : $zipPath"
Write-Host ("Size      : {0:N0} bytes, {1} entries ({2} payload files)" -f (Get-Item -LiteralPath $zipPath).Length, $entries.Count, $payload.Count)
Write-Host "SHA-256   : $zipHash"
Write-Host "Identity  : $($identity.designation) / $Title / commit $commit / $buildDate / $($identity.architecture)"
if ($symbolsZip) { Write-Host "Symbols   : $symbolsZip (developers only, not part of the player package)" }
[pscustomobject]@{ Zip = $zipPath; Sha256 = $zipHash; Version = $Version; Commit = $commitFull; Symbols = $symbolsZip; PackageName = $packageName; Title = $Title }
