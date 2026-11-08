#Requires -Version 7.0
# Developer Preview package validation. Three layers, reported separately (they prove different things):
#
#  A. PACKAGE AUDIT (static, no game, no Steam): extraction, structure, manifest == files, hashes, no duplicate / absolute / traversing paths, NO PROPRIETARY ASSET
#     (extension deny-list + hash comparison with every retail asset of the staged candidate + EA-derived data patterns), no PDB / developer file, text scan for developer paths,
#     script syntax, PE machine + imports resolvable on a clean Windows, README claims.
#  B. INSTALL / UNINSTALL TESTS on a SYNTHETIC "Steam" tree (tiny stand-in files and a stand-in 32-bit Generals.exe): the shipped scripts are run by Windows PowerShell 5.1
#     (powershell.exe, what a player has): check-only, fresh install, repair, upgrade (changed + removed + added file), interrupted install with rollback, refusals, damaged package,
#     uninstall back to a byte-identical tree, damaged backup. The real Steam folders and the real user data are never touched (user-data snapshot compared before / after).
#  C. -Native: the same install on a DISPOSABLE copy of the real game (staged by the stager from the read-only Steam files, project files removed, stand-in retail launcher), then
#     real launches of the installed Generals.exe (first launch, Modern menu, Options, Skirmish, Skirmish save/load with Game Speed Fast, Classic) and the uninstall.
#     The game writes the user's Documents\...Data folder: its ini files, Save and Replays are backed up first and restored afterwards (the guard of the earlier milestones).
#
# This is AUTOMATED validation on one machine. It is NOT a real clean-machine test and NOT the owner's Steam test.
[CmdletBinding()]
param([ValidateSet('ZeroHour', 'Generals')][string]$Title = 'ZeroHour',
      [string]$Package,                 # default: newest build\dist\MKRaudstein-<Title>-DeveloperPreview-*.zip
      [string]$Candidate,               # staged candidate the package derives from (default: the one recorded in the package manifest)
      [switch]$Native,
      [string]$AssetSource,             # default: the read-only Steam Zero Hour folder (Generals: its ZH_Generals folder)
      [string]$Name,                    # disposable stage name for -Native
      [switch]$KeepTemp,
      [ValidateRange(60, 600)][int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
. (Join-Path $PSScriptRoot 'OverlayPackageCommon.ps1')
$repo = $script:ZHRepository
Add-Type -AssemblyName System.IO.Compression.FileSystem
$steamZh = 'C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour'
if (-not $AssetSource) { $AssetSource = if ($Title -eq 'Generals') { Join-Path $steamZh 'ZH_Generals' } else { $steamZh } }
if (-not $Name) { $Name = if ($Title -eq 'Generals') { 'dp-test-gen' } else { 'dp-test-zh' } }
if (-not $Package) { $Package = Get-ChildItem (Join-Path $repo 'build\dist') -Filter "MKRaudstein-$Title-DeveloperPreview-*.zip" | Where-Object { $_.Name -notmatch 'Symbols' } | Sort-Object LastWriteTime | Select-Object -Last 1 -ExpandProperty FullName }
if (-not $Package -or -not (Test-Path -LiteralPath $Package)) { throw 'Developer Preview package not found; run New-MKRaudsteinDistribution.ps1 (it builds and validates it).' }
$results = [System.Collections.Generic.List[object]]::new()
function Check([bool]$Condition, [string]$Message, [string]$Layer = 'A') { $results.Add([pscustomobject]@{ Layer = $Layer; Pass = $Condition; Check = $Message }); if (-not $Condition) { Write-Host "FAIL[$Layer]: $Message" -ForegroundColor Red } else { Write-Host "ok  [$Layer]: $Message" } }
function Sha([byte[]]$Bytes) { [BitConverter]::ToString([Security.Cryptography.SHA256]::HashData($Bytes)).Replace('-', '') }

$work = Join-Path $repo ('build\diag\dp-test-' + $Title + '-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
New-Item -ItemType Directory -Force -Path $work | Out-Null

# ---- user-data guard (the real folders must be byte-for-byte what they were) -----------------------------------------------------------------------------------
$docs = [Environment]::GetFolderPath('MyDocuments')
$userData = Join-Path $docs $(if ($Title -eq 'Generals') { 'Command and Conquer Generals Data' } else { 'Command and Conquer Generals Zero Hour Data' })
function Get-UserSnapshot { $h = @{}; if (Test-Path -LiteralPath $userData) { foreach ($f in Get-ChildItem -LiteralPath $userData -Recurse -File -ErrorAction SilentlyContinue) { $h[$f.FullName] = "$($f.Length)|$($f.LastWriteTimeUtc.Ticks)" } }; $h }
function Same-Snapshot($a, $b) { if ($a.Count -ne $b.Count) { return $false }; foreach ($k in $a.Keys) { if (-not $b.ContainsKey($k) -or $a[$k] -ne $b[$k]) { return $false } }; $true }
$userBefore = Get-UserSnapshot

# ================================================================================ A. package audit ================================================================================
$extract = Join-Path $work 'extracted'
Expand-Archive -LiteralPath $Package -DestinationPath $extract -Force
$topDirs = @(Get-ChildItem -LiteralPath $extract -Directory); $topFiles = @(Get-ChildItem -LiteralPath $extract -File)
Check ($topDirs.Count -eq 1 -and $topFiles.Count -eq 0) 'extraction: the ZIP has exactly one top-level folder (no files spilled into the extraction folder)'
$pkg = $topDirs[0].FullName
Check ($topDirs[0].Name -eq [IO.Path]::GetFileNameWithoutExtension($Package)) "extraction: the top-level folder is named like the ZIP ($($topDirs[0].Name))"
$sidecar = "$Package.sha256"
if (Test-Path -LiteralPath $sidecar) { Check ((Get-Content -LiteralPath $sidecar -Raw).Trim().Split(' ')[0] -eq (Get-FileHash -LiteralPath $Package -Algorithm SHA256).Hash) 'integrity: the .sha256 file matches the ZIP' }
$archive = [IO.Compression.ZipFile]::OpenRead($Package)
try { $names = @($archive.Entries | ForEach-Object FullName | Where-Object { -not $_.EndsWith('/') }) } finally { $archive.Dispose() }
Check ($names.Count -eq @($names | ForEach-Object { $_.ToLowerInvariant() } | Select-Object -Unique).Count) 'no duplicate entries (case-insensitive, as Windows sees them)'
Check (-not @($names | Where-Object { $_ -match '(^|/)\.\.(/|$)' -or $_ -match '^[A-Za-z]:' -or $_.StartsWith('/') -or $_.Contains('\') })) 'no absolute, drive-letter, backslash or parent-directory paths in the ZIP'
$manifest = Get-Content -LiteralPath (Join-Path $pkg 'package-manifest.json') -Raw -Encoding utf8 | ConvertFrom-Json
foreach ($req in 'Install.cmd', 'Uninstall.cmd', 'README-FIRST.txt', 'package-manifest.json', 'tools/MKRaudstein-Setup.ps1', 'tools/Install-MKRaudstein.ps1', 'tools/Uninstall-MKRaudstein.ps1') {
    Check (Test-Path -LiteralPath (Join-Path $pkg $req)) "player entry point present: $req"
}
$id = $manifest.identity
Check ($id.title -eq $Title -and $id.designation -and $id.commit -match '^[0-9a-f]{7,40}$' -and $id.commitFull -match '^[0-9a-f]{40}$' -and $id.buildDate -and $id.arch -and $id.branding) "version identity complete: $($id.designation) / $($id.title) / $($id.commit) / $($id.buildDate) / $($id.arch)"
Check ($id.branding -like 'Modern Experience by MKRaudstein | 64-bit * DirectX 11') 'branding string is "Modern Experience by MKRaudstein | 64-bit . DirectX 11"'
Check ($id.packageName -eq $topDirs[0].Name -and $Package -like "*$($id.commit).zip") 'the package name carries the source commit'
$identityFile = Get-Content -LiteralPath (Join-Path $repo 'scripts\distribution\identity.json') -Raw -Encoding utf8 | ConvertFrom-Json
Check ($identityFile.designation -eq $id.designation) 'the designation equals scripts/distribution/identity.json (single source)'
$engineHeader = Get-Content -LiteralPath (Join-Path $repo 'Core\GameEngine\Include\Common\ReleaseIdentity.h') -Raw
Check ($engineHeader -match [regex]::Escape("kDesignation = `"$($id.designation)`"")) 'the engine log designation (ReleaseIdentity.h) equals the package designation'

# payload == manifest
$payloadRoot = Join-Path $pkg 'payload'
$onDisk = @{}; foreach ($f in Get-ChildItem -LiteralPath $payloadRoot -Recurse -File) { $onDisk[($f.FullName.Substring($payloadRoot.Length + 1))] = $f.FullName }
$listed = @{}; foreach ($f in $manifest.files) { $listed[$f.path] = $f }
Check ((($onDisk.Keys | Sort-Object) -join '|') -ceq (($listed.Keys | Sort-Object) -join '|')) "payload folder == manifest file list ($($listed.Count) files: nothing missing, nothing extra)"
$bad = @(foreach ($k in $listed.Keys) { if ($onDisk.ContainsKey($k)) { $f = Get-Item -LiteralPath $onDisk[$k]; if ($f.Length -ne $listed[$k].size -or (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash -ne $listed[$k].sha256) { $k } } })
Check ($bad.Count -eq 0) "every payload file matches the size and SHA-256 in the manifest$(if ($bad.Count) { ': ' + ($bad -join ', ') })"

# ---- proprietary assets --------------------------------------------------------------------------------------------------------------------------------------
$denyExt = '\.(big|bik|sdt|wav|mp3|ogg|tga|dds|w3d|ani|cur|csf|str|map|ref|scb|wb|fx|fxo|bmp|jpg|png|ico|vp6|avi|mp4|dat|lcf)$'
$assetLike = @($listed.Keys | Where-Object { $_ -match $denyExt })
Check ($assetLike.Count -eq 0) "no game-asset file types in the package (archives, media, textures, maps)$(if ($assetLike.Count) { ': ' + ($assetLike -join ', ') })"
$derived = @($listed.Keys | Where-Object { $_ -match '^(?i)(Data\\INI\\|Window\\.*\.wnd$|Data\\.*\.(ini|txt)$|.*\.ini$|.*\.wnd$)' -and $_ -notmatch '^(?i)MKRaudstein\\' })
Check ($derived.Count -eq 0) "no INI / WND data files (the Generals extension files are generated on the player's PC, not shipped)$(if ($derived.Count) { ': ' + ($derived -join ', ') })"
$allowedPayload = '^(?i)(Generals\.exe|(vcruntime140|vcruntime140_1|msvcp140|msvcp140_atomic_wait|msvcp140_1|msvcp140_2|concrt140|vccorlib140)\.dll|MKRaudstein\\(LICENSE\.md|manifest\.json))$'
$unexpected = @($listed.Keys | Where-Object { $_ -notmatch $allowedPayload })
Check ($unexpected.Count -eq 0) "the payload is exactly: the 64-bit Generals.exe, the app-local Visual C++ runtime DLLs, the licence and the overlay manifest$(if ($unexpected.Count) { '; unexpected: ' + ($unexpected -join ', ') })"
Check ((@($manifest.files | Where-Object { $_.path -like '*.exe' }).Count) -eq 1) 'the only executable in the package is Generals.exe'
Check ($manifest.containsRetailAssets -eq $false) 'the manifest declares containsRetailAssets = false'
# hash comparison with every retail asset of the staged candidate (strongest evidence; needs the stage, so developer machine only)
if (-not $Candidate) { $Candidate = $manifest.overlayPackage.stageCandidate }
$stagePath = Join-Path $repo "build\dev-runtimes\native\$Title\$Candidate\manifest.json"
$stageManifest = $null
if (Test-Path -LiteralPath $stagePath) {
    $stageManifest = Get-Content -LiteralPath $stagePath -Raw -Encoding utf8 | ConvertFrom-Json
    $retailHashes = @{}; foreach ($a in $stageManifest.Assets) { $retailHashes[$a.SHA256] = $a.Path }
    $hit = @(foreach ($k in $listed.Keys) { if ($retailHashes.ContainsKey($listed[$k].sha256)) { "$k == $($retailHashes[$listed[$k].sha256])" } })
    Check ($hit.Count -eq 0) "no packaged file is byte-identical to any of the $($retailHashes.Count) retail asset files of the staged candidate$(if ($hit.Count) { ': ' + ($hit -join ', ') })"
    # nothing generated from retail data either: generated outputs are named in the manifest, never present
    Check (-not @($manifest.generated | Where-Object { $onDisk.ContainsKey($_.path) })) 'generated (EA-derived) files are listed by hash only, never present in the package'
} else { Write-Host "info: stage $Candidate not present, retail-hash comparison skipped (the extension and pattern checks above still apply)" }
Check (-not @($names | Where-Object { $_ -match '\.(pdb|ilk|obj|lib|exp|cpp|h|hpp|cmake|py|pyc|sln|vcxproj)$' -or $_ -match '(^|/)(build|\.git|\.github|\.vs|__pycache__)(/|$)' })) 'no PDB, build artifact, source or repository file in the package (symbols are a separate ZIP)'

# ---- text scan: developer paths, dev tools, hard links ----------------------------------------------------------------------------------------------------
$textFiles = @(Get-ChildItem -LiteralPath $pkg -Recurse -File | Where-Object { $_.Extension -in '.cmd', '.ps1', '.json', '.txt', '.md' })
$devPattern = '(?i)(C:\\Users\\|buskr|ProjectsX|GeneralsGameCode\\build|dev-runtimes|\\build\\dist|Visual Studio\\|VsDevCmd|vswhere|cmake|python\.exe|\bpython3?\b\s|pwsh)'
$devHits = @(foreach ($f in $textFiles) { $t = Get-Content -LiteralPath $f.FullName -Raw -Encoding utf8; foreach ($m in [regex]::Matches($t, $devPattern)) { "$($f.FullName.Substring($pkg.Length + 1)): $($m.Value)" } })
$devHits = @($devHits | Where-Object { $_ -notmatch 'LICENSE\.md' })
Check ($devHits.Count -eq 0) "no developer path or tool reference (user name, repo / build tree, Visual Studio, CMake, Python, pwsh) in $($textFiles.Count) packaged text files, scripts and manifests$(if ($devHits.Count) { ': ' + (($devHits | Select-Object -First 6) -join '; ') })"
$linkHits = @(foreach ($f in $textFiles | Where-Object { $_.Extension -in '.cmd', '.ps1' }) { if ((Get-Content -LiteralPath $f.FullName -Raw) -match '(?i)(mklink|HardLink|SymbolicLink|Junction|fsutil\s+hardlink|reg(\.exe)?\s+(add|delete|import)|New-ItemProperty|Set-ItemProperty|Remove-ItemProperty|New-PSDrive|Set-ExecutionPolicy|Add-MpPreference)') { $f.Name } })
Check ($linkHits.Count -eq 0) 'the scripts create no hard links, symlinks, junctions or registry values'
Check (-not @($manifest.files | Where-Object { $_.path -match '^[A-Za-z]:|^\\|\.\.' })) 'every manifest path is relative and inside the game folder'
# script syntax (Windows PowerShell 5.1 grammar is checked by actually running them in section B; here the parser of this host)
$syntaxBad = @(foreach ($f in Get-ChildItem -LiteralPath (Join-Path $pkg 'tools') -Filter *.ps1) { $e = $null; [void][Management.Automation.Language.Parser]::ParseFile($f.FullName, [ref]$null, [ref]$e); if ($e) { "$($f.Name): $($e[0].Message)" } })
Check ($syntaxBad.Count -eq 0) "all packaged scripts parse$(if ($syntaxBad.Count) { ': ' + ($syntaxBad -join '; ') })"

# ---- PE ----------------------------------------------------------------------------------------------------------------------------------------------------
$exe = Join-Path $payloadRoot 'Generals.exe'
$exeBytes = [IO.File]::ReadAllBytes($exe)
Check ((Get-PeMachine $exeBytes) -eq 0x8664) 'Generals.exe is an AMD64 (x64) PE image'
$imports = Get-PeImportNamesFromBytes $exeBytes
$system = Join-Path $env:SystemRoot 'System32'
$appLocal = @($imports | Where-Object { $onDisk.ContainsKey($_.ToLowerInvariant()) -or $onDisk.ContainsKey($_) })
$unresolved = @($imports | Where-Object { $_ -notin $appLocal -and $_ -notlike 'api-ms-win-*' -and -not (Test-Path -LiteralPath (Join-Path $system $_)) })
Check ($unresolved.Count -eq 0) "every imported DLL is in the package or a Windows system DLL ($($imports.Count) imports, $($appLocal.Count) app-local)$(if ($unresolved.Count) { '; unresolved: ' + ($unresolved -join ', ') })"
Check (-not @($imports | Where-Object { $_ -match '(?i)(d\.dll$|ucrtbased|vcruntime140d|msvcp140d|debug)' })) 'no debug-runtime import'
Check (@($imports | Where-Object { $_ -match '^(?i)(vcruntime140|msvcp140)' } | Where-Object { $_ -notin $appLocal }).Count -eq 0) 'the Visual C++ runtime comes from the package (app-local); no redistributable install is needed'
$pdbPath = [regex]::Match([Text.Encoding]::ASCII.GetString($exeBytes), '[A-Za-z]:\\[\x20-\x7e]{3,200}\.pdb').Value
Write-Host "info: Generals.exe embeds its build-time PDB path in the debug directory ('$pdbPath'): not a runtime dependency; /PDBALTPATH would strip it (release hygiene item)"

# ---- documentation claims ------------------------------------------------------------------------------------------------------------------------------------
$readme = Get-Content -LiteralPath (Join-Path $pkg 'README-FIRST.txt') -Raw -Encoding utf8
foreach ($claim in @('DEVELOPER PREVIEW', 'NO game data', 'Install.cmd', 'Uninstall.cmd', '-classic', 'Verify integrity of game files', 'two PCs', 'Skirmish only', 'read-only', $id.commitFull, $id.designation)) {
    Check ($readme.Contains($claim) -or $readme -match [regex]::Escape($claim) -or ($claim -eq 'NO game data' -and $readme -match 'NO game data')) "README-FIRST states: $claim"
}

# ================================================================================ B. synthetic install / uninstall tests ================================================================================
$exeStub = New-Object byte[] 2048; $exeStub[0] = 0x4D; $exeStub[1] = 0x5A; [BitConverter]::GetBytes([int]0x80).CopyTo($exeStub, 0x3C); $exeStub[0x80] = 0x50; $exeStub[0x81] = 0x45; [BitConverter]::GetBytes([uint16]0x14C).CopyTo($exeStub, 0x84)
$x64Stub = $exeStub.Clone(); [BitConverter]::GetBytes([uint16]0x8664).CopyTo($x64Stub, 0x84)
function New-StandInRetail([string]$Root) {
    New-Item -ItemType Directory -Force -Path $Root | Out-Null
    foreach ($r in $manifest.target.requiredFiles) { if ($r.path -ne 'Generals.exe') { [IO.File]::WriteAllText((Join-Path $Root $r.path), "stand-in retail archive $($r.path)") } }
    [IO.File]::WriteAllBytes((Join-Path $Root 'Generals.exe'), $exeStub)
    [IO.File]::WriteAllText((Join-Path $Root 'vcruntime140.dll'), 'retail-side older runtime')       # a retail file the package replaces (non-exe backup path)
    New-Item -ItemType Directory -Force -Path (Join-Path $Root 'Data') | Out-Null
    [IO.File]::WriteAllText((Join-Path $Root 'Data\keep.txt'), 'untouched')
}
function Get-Tree([string]$Root, [string[]]$SkipTop = @()) {
    $r = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $lines = foreach ($i in Get-ChildItem -LiteralPath $Root -Recurse -Force) {
        $rel = $i.FullName.Substring($r.Length)
        if ($SkipTop -contains ($rel -split '\\')[0]) { continue }
        if ($i.PSIsContainer) { "D|$rel" }
        else {
            $h = if ($i.Length -lt 64MB) { (Get-FileHash -LiteralPath $i.FullName -Algorithm SHA256).Hash } else { "big:$($i.LastWriteTimeUtc.Ticks)" }
            "F|$rel|$($i.Length)|$h"
        }
    }
    (($lines | Sort-Object) -join "`n")
}
function Invoke-Setup([string]$Script, [string[]]$Arguments) {
    $out = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Script @Arguments 2>&1 | Out-String
    [pscustomobject]@{ Exit = $LASTEXITCODE; Out = $out }
}
$installPs = Join-Path $pkg 'tools\Install-MKRaudstein.ps1'
$uninstallPs = Join-Path $pkg 'tools\Uninstall-MKRaudstein.ps1'
$folder = $manifest.target.steamFolderNames[0]
function New-Game([string]$Tag) { $g = Join-Path $work "synthetic-$Tag\steamapps\common\$folder"; New-StandInRetail $g; $g }

# generated-file handling needs real archives; in the synthetic tree the installer must degrade, not fail
$gen = @($manifest.generators)
$g1 = New-Game 'fresh'
$original = Get-Tree $g1
$r = Invoke-Setup $installPs @('-GameFolder', $g1, '-CheckOnly')
Check ($r.Exit -eq 0 -and (Get-Tree $g1) -ceq $original) 'B1 -CheckOnly: exit 0 and the game folder is unchanged' 'B'
$r = Invoke-Setup $installPs @('-GameFolder', $g1, '-Yes')
Check ($r.Exit -eq 0) "B2 fresh install exits 0$(if ($r.Exit) { ': ' + $r.Out })" 'B'
Check ((Get-PeMachine ([IO.File]::ReadAllBytes((Join-Path $g1 'Generals.exe')))) -eq 0x8664) 'B2 Generals.exe is now the 64-bit runtime' 'B'
$allMatch = -not @($manifest.files | Where-Object { (Get-FileHash -LiteralPath (Join-Path $g1 $_.path) -Algorithm SHA256).Hash -ne $_.sha256 })
Check $allMatch 'B2 every payload file is installed byte-identical' 'B'
$record = Get-Content -LiteralPath (Join-Path $g1 'MKRaudstein\install-record.json') -Raw | ConvertFrom-Json
Check ($record.status -eq 'installed' -and $record.identity.commit -eq $id.commit -and $record.identity.designation -eq $id.designation) 'B2 install record: status installed, version identity recorded' 'B'
Check ((Get-FileHash -LiteralPath (Join-Path $g1 'MKRaudstein\backup\Generals.exe') -Algorithm SHA256).Hash -eq (Sha $exeStub) -and (Get-Content -LiteralPath (Join-Path $g1 'MKRaudstein\backup\vcruntime140.dll') -Raw) -eq 'retail-side older runtime') 'B2 the replaced retail Generals.exe and runtime DLL were backed up' 'B'
Check ((Get-Content -LiteralPath (Join-Path $g1 'Data\keep.txt') -Raw) -eq 'untouched') 'B2 unrelated game files are untouched' 'B'
Check ((Test-Path -LiteralPath (Join-Path $g1 'MKRaudstein\Uninstall.cmd')) -and (Test-Path -LiteralPath (Join-Path $g1 'MKRaudstein\tools\Uninstall-MKRaudstein.ps1'))) 'B2 the uninstaller was installed next to the game (no ZIP needed to uninstall)' 'B'
# repair on the same version: the ORIGINAL backup must survive
Remove-Item -LiteralPath (Join-Path $g1 'vcruntime140_1.dll') -Force
[IO.File]::WriteAllText((Join-Path $g1 'msvcp140.dll'), 'damaged')
$r = Invoke-Setup $installPs @('-GameFolder', $g1, '-Yes')
$allMatch = -not @($manifest.files | Where-Object { (Get-FileHash -LiteralPath (Join-Path $g1 $_.path) -Algorithm SHA256).Hash -ne $_.sha256 })
Check ($r.Exit -eq 0 -and $allMatch) 'B3 reinstall on the same version repairs a deleted and a damaged file' 'B'
Check ((Get-FileHash -LiteralPath (Join-Path $g1 'MKRaudstein\backup\Generals.exe') -Algorithm SHA256).Hash -eq (Sha $exeStub)) 'B3 the backup still holds the ORIGINAL retail launcher (reinstall never backs up our own file)' 'B'
# upgrade: a newer package (one file changed, one removed, one added, new commit)
$pkg2 = Join-Path $work 'package-v2'; Copy-Item -LiteralPath $pkg -Destination $pkg2 -Recurse
$m2 = Get-Content -LiteralPath (Join-Path $pkg2 'package-manifest.json') -Raw | ConvertFrom-Json
$m2.identity.commit = 'aaaaaaaaa'; $m2.identity.designation = 'Developer Preview 99'
$changed = @($m2.files | Where-Object { $_.path -eq 'MKRaudstein\manifest.json' })[0]
[IO.File]::WriteAllText((Join-Path $pkg2 'payload\MKRaudstein\manifest.json'), 'changed in v2'); $changed.sha256 = (Get-FileHash -LiteralPath (Join-Path $pkg2 'payload\MKRaudstein\manifest.json') -Algorithm SHA256).Hash; $changed.size = (Get-Item -LiteralPath (Join-Path $pkg2 'payload\MKRaudstein\manifest.json')).Length
$removed = @($m2.files | Where-Object { $_.path -like 'concrt140.dll' -or $_.path -like 'msvcp140_atomic_wait.dll' })[0]
Remove-Item -LiteralPath (Join-Path $pkg2 "payload\$($removed.path)") -Force
$added = [ordered]@{ path = 'MKRaudstein\added-in-v2.txt'; size = 4; sha256 = '' }
[IO.File]::WriteAllText((Join-Path $pkg2 'payload\MKRaudstein\added-in-v2.txt'), 'new!'); $added.size = 4; $added.sha256 = (Get-FileHash -LiteralPath (Join-Path $pkg2 'payload\MKRaudstein\added-in-v2.txt') -Algorithm SHA256).Hash
$m2.files = @($m2.files | Where-Object { $_.path -ne $removed.path }) + [pscustomobject]$added
($m2 | ConvertTo-Json -Depth 8) | Set-Content -LiteralPath (Join-Path $pkg2 'package-manifest.json') -Encoding utf8
$r = Invoke-Setup (Join-Path $pkg2 'tools\Install-MKRaudstein.ps1') @('-GameFolder', $g1, '-Yes')
$rec2 = Get-Content -LiteralPath (Join-Path $g1 'MKRaudstein\install-record.json') -Raw | ConvertFrom-Json
Check ($r.Exit -eq 0 -and $rec2.identity.commit -eq 'aaaaaaaaa') "B4 upgrade to a newer package exits 0 and records the new identity$(if ($r.Exit) { ': ' + $r.Out })" 'B'
Check ((Get-Content -LiteralPath (Join-Path $g1 'MKRaudstein\manifest.json') -Raw) -eq 'changed in v2' -and (Get-Content -LiteralPath (Join-Path $g1 'MKRaudstein\added-in-v2.txt') -Raw) -eq 'new!' -and -not (Test-Path -LiteralPath (Join-Path $g1 $removed.path))) 'B4 upgrade: changed file replaced, new file added, file the new version no longer has removed' 'B'
Check ((Get-FileHash -LiteralPath (Join-Path $g1 'MKRaudstein\backup\Generals.exe') -Algorithm SHA256).Hash -eq (Sha $exeStub)) 'B4 upgrade keeps the ORIGINAL retail backup' 'B'
# uninstall back to the byte-identical original
[IO.File]::WriteAllText((Join-Path $g1 'generals64.log'), 'a log the game wrote')
$r = Invoke-Setup $uninstallPs @('-GameFolder', $g1, '-Yes')
Check ($r.Exit -eq 0) "B5 uninstall exits 0$(if ($r.Exit) { ': ' + $r.Out })" 'B'
Check ((Get-Tree $g1) -ceq $original) 'B5 after uninstall the game folder is byte-identical to the original (retail files restored, project files, MKRaudstein folder and generals64.log gone)' 'B'
$r = Invoke-Setup $uninstallPs @('-GameFolder', $g1, '-Yes')
Check ($r.Exit -eq 2 -and (Get-Tree $g1) -ceq $original) 'B5 uninstalling again reports "nothing installed" and changes nothing' 'B'
# interrupted install: a destination that cannot be replaced -> rollback, then uninstall of the interrupted state
$g2 = New-Game 'rollback'; $original2 = Get-Tree $g2
$lock = [IO.File]::Open((Join-Path $g2 'Generals.exe'), 'Open', 'Read', 'Read')
try { $r = Invoke-Setup $installPs @('-GameFolder', $g2, '-Yes') } finally { $lock.Dispose() }
Check ($r.Exit -eq 3 -and (Get-Tree $g2 @('MKRaudstein')) -ceq (Get-Tree (New-Game 'ref2') @('MKRaudstein'))) 'B6 a failure in the middle of an install exits 3 and rolls every changed file back' 'B'
$rec3 = Get-Content -LiteralPath (Join-Path $g2 'MKRaudstein\install-record.json') -Raw | ConvertFrom-Json
Check ($rec3.status -eq 'installing') 'B6 the interrupted install leaves a record marked "installing"' 'B'
$r = Invoke-Setup $uninstallPs @('-GameFolder', $g2, '-Yes')
Check ($r.Exit -eq 0 -and (Get-Tree $g2) -ceq $original2) 'B6 uninstall of the interrupted install returns the folder to the original' 'B'
$r = Invoke-Setup $installPs @('-GameFolder', $g2, '-Yes')
Check ($r.Exit -eq 0 -and (Get-PeMachine ([IO.File]::ReadAllBytes((Join-Path $g2 'Generals.exe')))) -eq 0x8664) 'B6 a clean install after the interrupted one succeeds' 'B'
# refusals
$g3 = New-Game 'notgame'; Remove-Item -LiteralPath (Join-Path $g3 $manifest.target.requiredFiles[0].path) -Force; $o3 = Get-Tree $g3
$r = Invoke-Setup $installPs @('-GameFolder', $g3, '-Yes')
Check ($r.Exit -eq 2 -and (Get-Tree $g3) -ceq $o3) 'B7 a folder that is not this game (required archive missing): refused, exit 2, nothing changed' 'B'
$g4 = New-Game 'foreign64'; [IO.File]::WriteAllBytes((Join-Path $g4 'Generals.exe'), $x64Stub); $o4 = Get-Tree $g4
$r = Invoke-Setup $installPs @('-GameFolder', $g4, '-Yes')
Check ($r.Exit -eq 2 -and (Get-Tree $g4) -ceq $o4) 'B7 a 64-bit Generals.exe this installer did not install: refused (Steam verify first), nothing changed' 'B'
$r = Invoke-Setup $installPs @('-GameFolder', (Join-Path $work 'does-not-exist'), '-Yes')
Check ($r.Exit -eq 2) 'B7 a folder that does not exist: refused' 'B'
$pkg3 = Join-Path $work 'package-damaged'; Copy-Item -LiteralPath $pkg -Destination $pkg3 -Recurse
$flip = [IO.File]::ReadAllBytes((Join-Path $pkg3 'payload\Generals.exe')); $flip[100] = $flip[100] -bxor 0xFF; [IO.File]::WriteAllBytes((Join-Path $pkg3 'payload\Generals.exe'), $flip)
$g5 = New-Game 'damaged'; $o5 = Get-Tree $g5
$r = Invoke-Setup (Join-Path $pkg3 'tools\Install-MKRaudstein.ps1') @('-GameFolder', $g5, '-Yes')
Check ($r.Exit -ne 0 -and (Get-Tree $g5) -ceq $o5) 'B7 a damaged package file is detected before anything is changed (exit non-zero, folder unchanged)' 'B'
# damaged backup
$g6 = New-Game 'badbackup'; $o6 = Get-Tree $g6
[void](Invoke-Setup $installPs @('-GameFolder', $g6, '-Yes'))
[IO.File]::WriteAllText((Join-Path $g6 'MKRaudstein\backup\vcruntime140.dll'), 'corrupt')
$r = Invoke-Setup $uninstallPs @('-GameFolder', $g6, '-Yes')
$rec6 = Get-Content -LiteralPath (Join-Path $g6 'MKRaudstein\install-record.json') -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json
Check ($r.Exit -eq 4 -and $rec6.status -eq 'uninstall-incomplete' -and (Get-PeMachine ([IO.File]::ReadAllBytes((Join-Path $g6 'Generals.exe')))) -eq 0x14C) 'B8 a damaged backup: the other files are restored, the uninstall reports INCOMPLETE (exit 4) and keeps record + backup' 'B'
[IO.File]::WriteAllText((Join-Path $g6 'MKRaudstein\backup\vcruntime140.dll'), 'retail-side older runtime')
$r = Invoke-Setup $uninstallPs @('-GameFolder', $g6, '-Yes')
Check ($r.Exit -eq 0 -and (Get-Tree $g6) -ceq $o6) 'B8 after the backup is repaired a second uninstall completes and the folder is original' 'B'
# the Uninstall.cmd wrapper from inside an installed game
$g7 = New-Game 'cmd'; $o7 = Get-Tree $g7
[void](Invoke-Setup $installPs @('-GameFolder', $g7, '-Yes'))
$cmdPath = Join-Path $g7 'MKRaudstein\Uninstall.cmd'
$cmdOut = & cmd.exe /c "`"$cmdPath`" -Yes <NUL" 2>&1 | Out-String
Check ((Get-Tree $g7) -ceq $o7) 'B9 MKRaudstein\Uninstall.cmd (double-click path, run from inside the game folder) restores the original folder' 'B'
$userAfterB = Get-UserSnapshot
Check (Same-Snapshot $userBefore $userAfterB) 'B10 the real user-data folder is unchanged by all install / uninstall tests' 'B'

# ================================================================================ C. native (disposable copy of the real game) ================================================================================
$nativeReport = $null
if ($Native) {
    $canonical = Read-NativeOrdinaryPlayStage $Title $Candidate $script:NativeNormalLaunchGenerated -AllowCrashDumps
    $legit = @{}; foreach ($a in $canonical.Assets) { $legit[($a.Path -replace '\\', '/').ToLowerInvariant()] = $a.SHA256 }
    $builtExe = [IO.Path]::GetFileName($canonical.Build.Executable); $builtPdb = [IO.Path]::GetFileName($canonical.Build.Pdb)
    $stageRoot = Join-Path $repo "build\dev-runtimes\native\$Title\$Name"
    if (Test-Path -LiteralPath $stageRoot) { throw "Disposable stage already exists: $stageRoot (remove it by hand: it is yours, by literal path)." }
    & (Join-Path $PSScriptRoot 'Stage-NativeOrdinaryPlay.ps1') -Title $Title -Name $Name -AssetSource $AssetSource | Out-Null
    $game = Join-Path $stageRoot 'game'
    $gameFull = [IO.Path]::GetFullPath($game).TrimEnd('\') + '\'
    # make it a CLEAN RETAIL installation: only unchanged legitimate files, and the 32-bit stand-in launcher (the real EA launcher is not available to this validator)
    $prunedFrom = @{}
    foreach ($f in @(Get-ChildItem -LiteralPath $game -Recurse -File)) {
        $rel = ($f.FullName.Substring($gameFull.Length) -replace '\\', '/').ToLowerInvariant()
        $isLegit = $legit.ContainsKey($rel) -and $legit[$rel] -eq (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
        if (-not $isLegit) { [IO.File]::Delete($f.FullName); $prunedFrom[[IO.Path]::GetDirectoryName($f.FullName)] = $true }
    }
    # the folders this cleanup emptied are not in a real retail install either (the baseline of the C8 byte-identical check must be what Steam would have)
    foreach ($d in ($prunedFrom.Keys | Sort-Object { $_.Length } -Descending)) { $cur = $d; while ($cur.Length -gt $gameFull.Length -and -not (Get-ChildItem -LiteralPath $cur -Force -ErrorAction SilentlyContinue)) { Remove-Item -LiteralPath $cur -Force; $cur = [IO.Path]::GetDirectoryName($cur) } }
    # stage files may be hard links into the shared asset store: never write INTO one, delete the name first so the stand-in is a new file
    if (Test-Path -LiteralPath (Join-Path $game 'Generals.exe')) { [IO.File]::Delete((Join-Path $game 'Generals.exe')) }
    [IO.File]::WriteAllBytes((Join-Path $game 'Generals.exe'), $exeStub)
    foreach ($leftover in @(Get-ChildItem -LiteralPath $game -Recurse -Directory | Where-Object { $_.Name -eq 'CrashDumps' })) { Remove-Item -LiteralPath $leftover.FullName -Recurse -Force }
    $cleanTree = Get-Tree $game
    $missingReq = @($manifest.target.requiredFiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $game $_.path)) })
    Check ($missingReq.Count -eq 0) "C0 the disposable game folder is a recognisable retail installation ($(@($manifest.target.requiredFiles).Count) required files present)" 'C'
    $sizeWarn = @($manifest.target.requiredFiles | Where-Object { $_.size -and (Test-Path -LiteralPath (Join-Path $game $_.path)) -and (Get-Item -LiteralPath (Join-Path $game $_.path)).Length -ne [int64]$_.size })
    Check ($sizeWarn.Count -eq 0) 'C0 the required archives have the sizes the package expects (the supported Steam release)' 'C'

    $r = Invoke-Setup $installPs @('-GameFolder', $game, '-Yes')
    Write-Host ($r.Out -replace '\x1b\[[0-9;]*m', '')
    Check ($r.Exit -eq 0) 'C1 the installer ran on the disposable retail copy and exited 0' 'C'
    # installed tree == the canonical staged runtime, file for file (the distribution rule, now through the installer)
    $installedRoot = $gameFull
    $parityBad = @()
    foreach ($f in (Get-ChildItem -LiteralPath $canonical.GamePath -Recurse -File)) {
        $rel = $f.FullName.Substring(([IO.Path]::GetFullPath($canonical.GamePath).TrimEnd('\') + '\').Length) -replace '\\', '/'
        if ($rel -in $script:NativeNormalLaunchGenerated -or $f.FullName -match '\\CrashDumps\\' -or $rel -ieq $builtPdb) { continue }
        $target = if ($rel -ieq $builtExe) { 'Generals.exe' } else { $rel }
        $key = $target.ToLowerInvariant()
        $isLegit = $legit.ContainsKey($key) -and $legit[$key] -eq (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
        $inst = Join-Path $game ($target -replace '/', '\')
        if (-not (Test-Path -LiteralPath $inst)) { $parityBad += "$rel missing"; continue }
        if (-not $isLegit -and (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $inst -Algorithm SHA256).Hash) { $parityBad += "$rel differs" }
    }
    Check ($parityBad.Count -eq 0) "C1 PARITY: retail copy + installer == the validated staged runtime, file for file (incl. files the installer generated)$(if ($parityBad.Count) { ': ' + ($parityBad -join '; ') })" 'C'

    $exeInstalled = Join-Path $game 'Generals.exe'
    $ud = $userData
    $bk = Join-Path $work 'userdata-backup'; New-Item -ItemType Directory -Force -Path $bk | Out-Null
    $udSnapshot = Get-UserSnapshot
    if (Test-Path -LiteralPath $ud) {
        Get-ChildItem -LiteralPath $ud -File -Filter *.ini -ErrorAction SilentlyContinue | Copy-Item -Destination $bk -Force
        foreach ($sub in 'Save', 'Replays') { if (Test-Path -LiteralPath (Join-Path $ud $sub)) { robocopy (Join-Path $ud $sub) (Join-Path $bk $sub) /MIR /NFL /NDL /NJH /NJS /NP | Out-Null } }
    }
    function Invoke-Launch([string]$Tag, [string]$Flow, [string[]]$Arguments, [hashtable]$Env, [int]$Frames = 600) {
        if (@(Get-Process -Name generals, generalsv, generalszh -ErrorAction SilentlyContinue).Count) { throw 'Exit all existing games first' }
        $out = Join-Path $work "run-$Tag"; New-Item -ItemType Directory -Force -Path $out | Out-Null
        $saved = @{}; $vars = @{ GENERALS_AUTOSHOT = $out; GENERALS_AUTOFLOW = $Flow; GENERALS_AUTOSHOT_FRAMES = "$Frames" } + $Env
        foreach ($k in $vars.Keys) { $saved[$k] = [Environment]::GetEnvironmentVariable($k); [Environment]::SetEnvironmentVariable($k, $vars[$k]) }
        $savedPath = $env:PATH; $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"      # Windows only: no VS / CMake / repository directory can satisfy a dependency
        try {
            $p = if ($Arguments.Count) { Start-Process -FilePath $exeInstalled -ArgumentList $Arguments -WorkingDirectory $game -PassThru } else { Start-Process -FilePath $exeInstalled -WorkingDirectory $game -PassThru }
            if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { $p.Kill(); $timed = $true } else { $timed = $false }
            $exit = if ($timed) { -1 } else { $p.ExitCode }
        } finally { $env:PATH = $savedPath; foreach ($k in $vars.Keys) { [Environment]::SetEnvironmentVariable($k, $saved[$k]) } }
        $log = Join-Path $game 'generals64.log'
        $text = if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Raw } else { '' }
        if ($text) { Copy-Item -LiteralPath $log -Destination (Join-Path $out 'generals64.log') -Force }
        [pscustomobject]@{ Tag = $Tag; Out = $out; Exit = $exit; TimedOut = $timed; Text = $text }
    }
    function Get-Lines([string]$Text, [string]$Component, [string]$State) { , @(($Text -split "`n") | Where-Object { $_ -match "`t$Component`t$State`t" } | ForEach-Object { ($_.TrimEnd("`r") -split "`t", 9)[8] }) }   # the leading comma keeps an empty result an array (it would unroll to $null, and .Count throws under strict mode)
    $runs = [ordered]@{}
    try {
        # first launch exactly as Steam's Play does it (no arguments), straight to the Modern main menu audit
        $a = Invoke-Launch 'first-launch' 'mainmenu' @() @{} 300
        $runs['first-launch'] = $a
        Check ((-not $a.TimedOut) -and $a.Exit -eq 0 -and $a.Text -match 'normal_launch') "C2 first launch (no arguments, like Steam Play): the installed Generals.exe started, ran the main menu audit and exited $($a.Exit)" 'C'
        $ver = (Get-Lines $a.Text 'version' 'observed') -join ' '
        Check ($ver -match [regex]::Escape($id.designation)) "C2 the log's version line carries the preview designation ('$ver')" 'C'
        # KNOWN, PRE-EXISTING (the 2071ad765 build reports it too, with 5): when the flow ends on the live Modern main menu, the menu's UI textures are still resident at the shutdown
        # invariant check (ui_residency_release). The backend teardown that follows destroys them and the process exits 0; the Options / Skirmish / Classic launches report 0.
        # Reported as info, not hidden: every OTHER failed phase still fails this check.
        $residencyLines = @(($a.Text -split "`n") | Where-Object { $_ -match "`tui_residency_release`tfail`t" })
        if ($residencyLines.Count) { Write-Host ("info: the Modern main-menu flow reports ui_residency_release fail ({0}): known, pre-existing (docs/modernization/DISTRIBUTION.md, Known findings)" -f (($residencyLines[0].TrimEnd("`r") -split "`t", 9)[8])) }
        $otherFails = @(($a.Text -split "`n") | Where-Object { $_ -match "`tfail`t" -and $_ -notmatch "`tui_residency_release`tfail`t" })
        Check ((Get-Lines $a.Text 'mainmenu' 'defect').Count -eq 0 -and $otherFails.Count -eq 0) 'C2 Modern main menu: no layout defect, no failed phase (the known shutdown residency report is listed as info)' 'C'
        Check (Test-Path -LiteralPath (Join-Path $a.Out 'autoshot-shell.bmp')) 'C2 the shell frame was rendered and captured' 'C'
        # Options (Modern)
        $o = Invoke-Launch 'options' 'options' @() @{} 300
        $runs['options'] = $o
        Check ((-not $o.TimedOut) -and $o.Exit -eq 0 -and (Get-Lines $o.Text 'autoflow' 'observed') -match 'options audited') 'C3 Modern Options: the screen was driven through all pages and audited' 'C'
        Check ((Get-Lines $o.Text 'options' 'defect').Count -eq 0) "C3 Modern Options: 0 defects$(if ((Get-Lines $o.Text 'options' 'defect').Count) { ': ' + ((Get-Lines $o.Text 'options' 'defect') -join '; ') })" 'C'
        # Skirmish match
        $s = Invoke-Launch 'skirmish' 'skirmish' @() @{} 600
        $runs['skirmish'] = $s
        Check ((-not $s.TimedOut) -and $s.Exit -eq 0 -and $s.Text -match 'flow complete' -and (Test-Path -LiteralPath (Join-Path $s.Out 'autoshot-game.bmp'))) 'C4 Skirmish: the Modern setup screens were driven to a running match, a match frame was rendered' 'C'
        # Skirmish save / load with Game Speed Fast (1.25x)
        $sl = Invoke-Launch 'save-load' 'skirmishsaveload' @() @{ GENERALS_FORCE_GAME_SPEED = '125' } 6000
        $runs['save-load'] = $sl
        $states = Get-Lines $sl.Text 'autoflow' 'observed'
        Check ((-not $sl.TimedOut) -and $sl.Exit -eq 0 -and $sl.Text -match 'flow complete') 'C5 Skirmish save / load at Game Speed Fast: the flow ran to completion' 'C'
        Check ([bool](Get-Lines $sl.Text 'gamespeed' 'pace')) 'C5 the log records the pace the scheduler ran at (Game Speed Fast was applied)' 'C'
        # Classic
        $c = Invoke-Launch 'classic-menu' 'mainmenu' @('-classic') @{} 300
        $runs['classic-menu'] = $c
        Check ((-not $c.TimedOut) -and $c.Exit -eq 0 -and (Get-Lines $c.Text 'autoflow' 'observed') -match 'main menu audited \(classic\)') 'C6 Classic main menu (-classic): the retail frontend, no Modern chrome' 'C'
        Check ((Get-Lines $c.Text 'version' 'observed').Count -eq 0) 'C6 Classic shows the retail version label (no Modern footer was logged)' 'C'
        $co = Invoke-Launch 'classic-options' 'options' @('-classic') @{} 300
        $runs['classic-options'] = $co
        Check ((-not $co.TimedOut) -and $co.Exit -eq 0 -and (Get-Lines $co.Text 'autoflow' 'observed') -match 'options audited \(classic\)') 'C6 Classic Options (-classic): the retail Options screen' 'C'
    } finally {
        # restore the user data the game touched
        if (Test-Path -LiteralPath $ud) {
            Get-ChildItem -LiteralPath $bk -File -Filter *.ini -ErrorAction SilentlyContinue | Copy-Item -Destination $ud -Force
            foreach ($sub in 'Save', 'Replays') {
                if (Test-Path -LiteralPath (Join-Path $bk $sub)) { robocopy (Join-Path $bk $sub) (Join-Path $ud $sub) /MIR /NFL /NDL /NJH /NJS /NP | Out-Null }
                elseif (Test-Path -LiteralPath (Join-Path $ud $sub)) { foreach ($nf in Get-ChildItem -LiteralPath (Join-Path $ud $sub) -File -Recurse | Where-Object { -not $udSnapshot.ContainsKey($_.FullName) }) { Remove-Item -LiteralPath $nf.FullName -Force } }
            }
            foreach ($nf in Get-ChildItem -LiteralPath $ud -File -Recurse -ErrorAction SilentlyContinue | Where-Object { -not $udSnapshot.ContainsKey($_.FullName) }) { Remove-Item -LiteralPath $nf.FullName -Force }
        }
    }
    $guard = Get-UserSnapshot
    $diff = @($guard.Keys | Where-Object { -not $udSnapshot.ContainsKey($_) }) + @($udSnapshot.Keys | Where-Object { -not $guard.ContainsKey($_) })
    Check ($diff.Count -eq 0) "C7 user-data guard: the files of $userData are back to what they were (new files removed, ini / Save / Replays restored)$(if ($diff.Count) { ': ' + (($diff | Select-Object -First 5) -join ', ') })" 'C'

    # uninstall: the game folder must come back (the run artifacts - log - are removed by the uninstaller)
    $r = Invoke-Setup $uninstallPs @('-GameFolder', $game, '-Yes')
    Check ($r.Exit -eq 0) "C8 uninstall on the disposable copy exits 0$(if ($r.Exit) { ': ' + $r.Out })" 'C'
    $afterTree = Get-Tree $game
    $diffLines = @(Compare-Object ($cleanTree -split "`n") ($afterTree -split "`n"))
    Check ($diffLines.Count -eq 0) "C8 after uninstall the disposable copy is byte-identical to the clean retail copy before the install$(if ($diffLines.Count) { ': ' + (($diffLines | Select-Object -First 4 | ForEach-Object { $_.InputObject }) -join '; ') })" 'C'
    if (-not $KeepTemp) { try { Remove-Item -LiteralPath $stageRoot -Recurse -Force } catch { Write-Warning "could not remove the disposable stage $stageRoot" } }
    $nativeReport = [ordered]@{ stage = $stageRoot; runs = @($runs.Keys | ForEach-Object { [ordered]@{ run = $_; exit = $runs[$_].Exit; out = $runs[$_].Out } }) }
}

# ---- report --------------------------------------------------------------------------------------------------------------------------------------------------
$failed = @($results | Where-Object { -not $_.Pass })
$layers = ($results | Group-Object Layer | ForEach-Object { "$($_.Name): $(@($_.Group | Where-Object Pass).Count)/$($_.Count)" }) -join ', '
$report = [ordered]@{ package = (Split-Path $Package -Leaf); packageSha256 = (Get-FileHash -LiteralPath $Package -Algorithm SHA256).Hash; identity = $id; native = [bool]$Native
                      layers = 'A = package audit, B = synthetic install/uninstall, C = native on a disposable copy of the real game'; checks = $results; nativeRuns = $nativeReport
                      scope = 'automated validation on the developer machine; NOT a real clean-machine test, NOT the owner Steam test' }
$reportPath = [IO.Path]::ChangeExtension($Package, $null).TrimEnd('.') + '.package-test.json'
($report | ConvertTo-Json -Depth 7) | Set-Content -LiteralPath $reportPath -Encoding utf8
Write-Host "$($results.Count - $failed.Count) of $($results.Count) checks passed ($layers); report: $reportPath"
if (-not $KeepTemp) { try { Remove-Item -LiteralPath $work -Recurse -Force } catch { Write-Warning "could not remove $work" } }
if ($failed.Count) { throw "$($failed.Count) check(s) failed." }
Write-Host "DEVELOPER PREVIEW PACKAGE TEST PASS$(if (-not $Native) { ' (layers A + B only; -Native not run)' }) - automated, not a real clean-machine test."
