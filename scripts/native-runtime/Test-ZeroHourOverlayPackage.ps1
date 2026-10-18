#Requires -Version 7.0
# X64.47A / X64.47B / X64.48B: automated validation of an overlay ZIP (-Title Generals|ZeroHour) against the DISTRIBUTION RULE
#     validated staged MKRaudstein runtime  ==  clean legitimate game installation  +  this ZIP
# on a DISPOSABLE layout (never the real Steam directory). It fails when a project runtime file of the staged candidate is missing from the ZIP, when
# the ZIP carries anything the staged candidate does not have (except the documented runtime DLLs / README / licence / manifest), when an unchanged
# legitimate game file is redistributed, or when a completed feature behaves differently after the overlay install than in the staged candidate.
#  1. package audit: ZIP == its manifest, hashes; ZIP == the project files of the canonical staged candidate (file-for-file, computed live);
#     no legitimate game file inside; Generals.exe is AMD64; only the VC++ runtime DLLs;
#  2. a disposable copy of the candidate is staged with the stager from the read-only Steam assets and proven identical to the canonical candidate;
#  3. REFERENCE RUN: that staged runtime is normal-launched (developer flow) and its log kept;
#  4. it is turned into a clean installation (every project / generated file and every run artifact removed; only unchanged legitimate files remain),
#     the ZIP is extracted over it like a player does, and the installed tree is proven equal to the staged candidate file-for-file;
#  5. every imported DLL resolves; the VC++ runtime comes from the extracted folder;
#  6. OVERLAY RUN: the extracted Generals.exe is normal-launched with a Windows-only PATH and no arguments (what Steam's Play does) through the same flow;
#     the feature probe lines (shortcut bar data, ...) and the flow must match the reference run exactly.
# This is automated validation, NOT the human Steam test.
[CmdletBinding()]
param([ValidateSet('ZeroHour', 'Generals')][string]$Title = 'ZeroHour',
      [Parameter(Mandatory)][string]$Candidate,   # the canonical staged candidate the ZIP was built from
      [string]$Zip,
      [string]$Name,
      [string]$AssetSource,               # default: the read-only Steam Zero Hour folder (Generals: its ZH_Generals base-game folder)
      [switch]$SkipLaunch,
      [switch]$AuditOnly,                 # only the package audit against the canonical staged candidate (section 1): no staging, no launch
      [ValidateRange(60,600)][int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
. (Join-Path $PSScriptRoot 'OverlayPackageCommon.ps1')
$repo = $script:ZHRepository
$steamZh = 'C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour'
if (-not $AssetSource) { $AssetSource = if ($Title -eq 'Generals') { Join-Path $steamZh 'ZH_Generals' } else { $steamZh } }
if (-not $Name) { $Name = if ($Title -eq 'Generals') { 'x64-48b-overlay-test-gen' } else { 'x64-48b-overlay-test-zh' } }
$flow = if ($Title -eq 'Generals') { 'skirmish' } else { 'challenge' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (-not $Zip) { $Zip = Get-ChildItem (Join-Path $repo 'build\dist') -Filter "MKRaudstein-$Title-x64-Preview-*.zip" | Where-Object { $_.Name -notmatch 'Symbols' } | Sort-Object LastWriteTime | Select-Object -Last 1 -ExpandProperty FullName }
if (-not $Zip -or -not (Test-Path -LiteralPath $Zip)) { throw 'Package not found; run New-ZeroHourOverlayPackage.ps1 first.' }
$results = [System.Collections.Generic.List[object]]::new()
function Check([bool]$Condition, [string]$Message) { $results.Add([pscustomobject]@{ Pass = $Condition; Check = $Message }); if (-not $Condition) { Write-Host "FAIL: $Message" -ForegroundColor Red } else { Write-Host "ok  : $Message" } }
function Sha([byte[]]$Bytes) { [BitConverter]::ToString([Security.Cryptography.SHA256]::HashData($Bytes)).Replace('-', '') }
function Read-Entry($Entry) { $s = $Entry.Open(); try { $m = [IO.MemoryStream]::new(); $s.CopyTo($m); return $m.ToArray() } finally { $s.Dispose() } }
$runtimeDll = '^(?i)(vcruntime140|vcruntime140_1|msvcp140|msvcp140_atomic_wait|concrt140|vccorlib140|msvcp140_1|msvcp140_2)\.dll$'

# ---- the canonical staged candidate --------------------------------------------------------------------------------------------------------------
$canonical = Read-NativeOrdinaryPlayStage $Title $Candidate $script:NativeNormalLaunchGenerated -AllowCrashDumps
$canonicalGame = [IO.Path]::GetFullPath($canonical.GamePath).TrimEnd('\') + '\'
$builtExe = [IO.Path]::GetFileName($canonical.Build.Executable); $builtPdb = [IO.Path]::GetFileName($canonical.Build.Pdb)
$legit = @{}; foreach ($a in $canonical.Assets) { $legit[($a.Path -replace '\\', '/').ToLowerInvariant()] = $a.SHA256 }
$legitHashes = @{}; foreach ($h in $legit.Values) { $legitHashes[$h] = $true }
function Get-ProjectFiles([string]$GameDir) {     # (relative '/', full path, sha) of everything that is not an unchanged legitimate game file, the symbols or a run artifact
    $root = [IO.Path]::GetFullPath($GameDir).TrimEnd('\') + '\'
    foreach ($f in (Get-ChildItem -LiteralPath $GameDir -Recurse -File | Sort-Object FullName)) {
        $relative = $f.FullName.Substring($root.Length) -replace '\\', '/'
        if ($relative -in $script:NativeNormalLaunchGenerated -or $f.FullName -match '\\CrashDumps\\' -or $relative -ieq $builtPdb) { continue }
        $sha = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
        if ($legit.ContainsKey($relative.ToLowerInvariant()) -and $legit[$relative.ToLowerInvariant()] -eq $sha) { continue }
        [pscustomobject]@{ Relative = $relative; Path = $f.FullName; Sha = $sha }
    }
}
$stageProject = @(Get-ProjectFiles $canonical.GamePath)
Write-Host "canonical staged candidate $Candidate : $($stageProject.Count) project runtime files ($(($stageProject | ForEach-Object Relative) -join ', '))"

# ---- 1. package audit ----------------------------------------------------------------------------------------------------------------------------
$archive = [IO.Compression.ZipFile]::OpenRead($Zip)
try { $entries = @{}; foreach ($e in $archive.Entries) { $entries[$e.FullName] = Read-Entry $e } } finally { $archive.Dispose() }
$manifest = [Text.Encoding]::UTF8.GetString($entries['MKRaudstein/manifest.json']) | ConvertFrom-Json
$listed = @($manifest.files | ForEach-Object { $_.file -replace '\\', '/' })
Check (($entries.Keys | Sort-Object) -join '|' -eq ((@($listed) + 'MKRaudstein/manifest.json' | Sort-Object -Unique) -join '|')) 'the ZIP contains exactly the files of its manifest (no more, no less)'
$hashOk = $true
foreach ($f in $manifest.files) { $key = $f.file -replace '\\', '/'; if ($key -ne 'MKRaudstein/manifest.json' -and (Sha $entries[$key]) -ne $f.sha256) { $hashOk = $false } }
Check $hashOk 'every packaged file matches the SHA-256 in the manifest'
Check ($manifest.canonicalStagedCandidate -eq $Candidate -and $manifest.stageManifestSha256 -eq (Get-FileHash -LiteralPath (Join-Path $canonical.StagePath 'manifest.json') -Algorithm SHA256).Hash) 'the ZIP was built from this canonical staged candidate (stage manifest hash matches)'
# every project file of the staged candidate is in the ZIP, byte-identical (the executable under its Steam name)
$missing = @(); $mismatch = @()
foreach ($p in $stageProject) {
    $zipPath = if ($p.Relative -ieq $builtExe) { 'Generals.exe' } else { $p.Relative }
    if (-not $entries.ContainsKey($zipPath)) { $missing += $p.Relative } elseif ((Sha $entries[$zipPath]) -ne $p.Sha) { $mismatch += $p.Relative }
}
Check ($missing.Count -eq 0) "no project runtime file of the staged candidate is missing from the ZIP$(if ($missing.Count) { ': ' + ($missing -join ', ') })"
Check ($mismatch.Count -eq 0) "every project runtime file in the ZIP is byte-identical to the staged candidate's$(if ($mismatch.Count) { ': ' + ($mismatch -join ', ') })"
# the ZIP carries nothing else except the documented additions
$expected = @{}; foreach ($p in $stageProject) { $expected[$(if ($p.Relative -ieq $builtExe) { 'Generals.exe' } else { $p.Relative })] = $true }
$extra = @($entries.Keys | Where-Object { -not $expected.ContainsKey($_) -and $_ -notmatch $runtimeDll -and $_ -ne "MKRaudstein-$Title-README.txt" -and $_ -notlike 'MKRaudstein/*' })
Check ($extra.Count -eq 0) "the ZIP carries nothing the staged candidate does not have, except the runtime DLLs, README, licence and manifest$(if ($extra.Count) { ': ' + ($extra -join ', ') })"
$redistributed = @($entries.Keys | Where-Object { $_ -notmatch $runtimeDll -and $_ -notlike 'MKRaudstein*' -and $legitHashes.ContainsKey((Sha $entries[$_])) })
Check ($redistributed.Count -eq 0) "no unchanged legitimate game file is redistributed$(if ($redistributed.Count) { ': ' + ($redistributed -join ', ') })"
Check (-not ($entries.Keys | Where-Object { $_ -match '\.(bik|asi|m3d|flt|ani|cur|tga|dds|wav|mp3|map|w3d|str|csf|dat|ilk|obj|lib|cpp|h|ps1|cmake|log|vdf|acf)$' -or $_ -match '^(Maps|Art|Textures|Movies|Audio)/' })) 'no media / developer file types in the ZIP'
$exes = @($entries.Keys | Where-Object { $_ -match '\.exe$' })
Check ($exes.Count -eq 1 -and $exes[0] -eq 'Generals.exe') 'the only executable is Generals.exe'
Check ((Get-PeMachine $entries['Generals.exe']) -eq 0x8664) 'Generals.exe is an AMD64 (x64) PE image'
Check (-not (@($entries.Keys | Where-Object { $_ -match '\.dll$' -and $_ -notmatch $runtimeDll }))) 'the only DLLs are the Visual C++ runtime DLLs the executable imports'

if ($AuditOnly) {
    $failed = @($results | Where-Object { -not $_.Pass })
    Write-Host "$($results.Count - $failed.Count) of $($results.Count) audit checks passed"
    if ($failed.Count) { throw "$($failed.Count) check(s) failed." }
    Write-Host 'PACKAGE AUDIT PASS (no staging, no launch).'
    return
}

# ---- 2. disposable staged copy of the candidate ----------------------------------------------------------------------------------------------------
$stageRoot = Join-Path $repo "build\dev-runtimes\native\$Title\$Name"
if (Test-Path -LiteralPath $stageRoot) { throw "Disposable stage already exists: $stageRoot (remove it by hand: it is yours, by literal path)." }
& (Join-Path $PSScriptRoot 'Stage-NativeOrdinaryPlay.ps1') -Title $Title -Name $Name -AssetSource $AssetSource | Out-Null
$game = Join-Path $stageRoot 'game'
$refProject = @(Get-ProjectFiles $game)
$sameSet = (($refProject | ForEach-Object { $_.Relative + ':' + $_.Sha } | Sort-Object) -join '|') -eq (($stageProject | ForEach-Object { $_.Relative + ':' + $_.Sha } | Sort-Object) -join '|')
Check $sameSet 'a fresh staging of the same build is file-for-file identical to the canonical staged candidate (so the reference run IS the validated runtime)'

# ---- launch helper -------------------------------------------------------------------------------------------------------------------------------------
function Invoke-Run([string]$ExePath, [string]$Dir, [string]$Tag) {
    if (@(Get-Process -Name generals, generalsv, generalszh -ErrorAction SilentlyContinue).Count) { throw 'Exit all existing games first' }
    $out = Join-Path $repo ("build\diag\overlay-test-$Tag-" + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    $saved = @{ P = $env:PATH; S = $env:GENERALS_AUTOSHOT; F = $env:GENERALS_AUTOFLOW; N = $env:GENERALS_AUTOSHOT_FRAMES }
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"        # Windows only: no Visual Studio / CMake / repository directory can satisfy a dependency
    $env:GENERALS_AUTOSHOT = $out; $env:GENERALS_AUTOFLOW = $flow; $env:GENERALS_AUTOSHOT_FRAMES = '400'
    try {
        $p = Start-Process -FilePath $ExePath -WorkingDirectory $Dir -PassThru     # no arguments: exactly what Steam's Play does
        if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { $p.Kill(); $timedOut = $true } else { $timedOut = $false }
        $exit = if ($timedOut) { -1 } else { $p.ExitCode }
    } finally { $env:PATH = $saved.P; $env:GENERALS_AUTOSHOT = $saved.S; $env:GENERALS_AUTOFLOW = $saved.F; $env:GENERALS_AUTOSHOT_FRAMES = $saved.N }
    $log = Join-Path $Dir 'generals64.log'
    $text = if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Raw } else { '' }
    if ($text) { Copy-Item -LiteralPath $log -Destination (Join-Path $out 'generals64.log') }
    [pscustomobject]@{ Out = $out; Exit = $exit; TimedOut = $timedOut; Text = $text }
}
function Get-Probe([string]$Text) { @(($Text -split "`n") | Where-Object { $_ -match "`tfeature_probe`tobserved`t" } | ForEach-Object { (($_.TrimEnd("`r") -split "`t", 9)[8]) } | Sort-Object) }
function Get-FlowSteps([string]$Text) { @(($Text -split "`n") | Where-Object { $_ -match "`tautoflow`tobserved`t" } | ForEach-Object { ((($_.TrimEnd("`r") -split "`t", 9)[8]) -replace 'step \d+: ', '') }) }

# ---- 3. reference run of the staged candidate ------------------------------------------------------------------------------------------------------
$reference = $null
if (-not $SkipLaunch) {
    $reference = Invoke-Run (Join-Path $game $builtExe) $game 'reference'
    Check ((-not $reference.TimedOut) -and $reference.Exit -eq 0 -and $reference.Text -match 'normal_launch') "REFERENCE: the staged candidate ran and exited cleanly (exit code $($reference.Exit))"
    Check ((Get-Probe $reference.Text).Count -ge 2) "REFERENCE: the feature probe reported the data of $((Get-Probe $reference.Text).Count) playable sides"
}

# ---- 4. clean installation + overlay ------------------------------------------------------------------------------------------------------------------
foreach ($leftover in @(Get-ProjectFiles $game) + @(Get-ChildItem -LiteralPath $game -Recurse -File | Where-Object { $_.Name -in $script:NativeNormalLaunchGenerated -or $_.Name -ieq $builtPdb } | ForEach-Object { [pscustomobject]@{ Path = $_.FullName } })) {
    [IO.File]::Delete($leftover.Path)
}
$cleanBad = @(Get-ChildItem -LiteralPath $game -Recurse -File | Where-Object {
    $relative = $_.FullName.Substring(([IO.Path]::GetFullPath($game).TrimEnd('\') + '\').Length) -replace '\\', '/'
    -not ($legit.ContainsKey($relative.ToLowerInvariant()) -and $legit[$relative.ToLowerInvariant()] -eq (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash) })
Check ($cleanBad.Count -eq 0) 'the disposable installation is clean: only unchanged legitimate game files remain (like a fresh Steam install)'
$beforeNames = @(Get-ChildItem -LiteralPath $game -Recurse -File | ForEach-Object { $_.FullName.Substring(([IO.Path]::GetFullPath($game).TrimEnd('\') + '\').Length) })
Expand-Archive -LiteralPath $Zip -DestinationPath $game -Force      # what the player does: extract over the installation, overwrite
$exe = Join-Path $game 'Generals.exe'
# installed tree == canonical staged candidate, file-for-file
$installedRoot = [IO.Path]::GetFullPath($game).TrimEnd('\') + '\'
$installed = @{}; foreach ($f in (Get-ChildItem -LiteralPath $game -Recurse -File)) { $installed[($f.FullName.Substring($installedRoot.Length) -replace '\\', '/')] = $f.FullName }
$parityBad = @()
foreach ($f in (Get-ChildItem -LiteralPath $canonical.GamePath -Recurse -File)) {
    $relative = $f.FullName.Substring($canonicalGame.Length) -replace '\\', '/'
    if ($relative -in $script:NativeNormalLaunchGenerated -or $f.FullName -match '\\CrashDumps\\' -or $relative -ieq $builtPdb) { continue }
    $installedKey = if ($relative -ieq $builtExe) { 'Generals.exe' } else { $relative }
    if (-not $installed.ContainsKey($installedKey)) { $parityBad += "$relative (missing after the overlay install)"; continue }
    if ((Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $installed[$installedKey] -Algorithm SHA256).Hash) { $parityBad += "$relative (differs)" }
}
$stageNames = @{}; foreach ($f in (Get-ChildItem -LiteralPath $canonical.GamePath -Recurse -File)) { $stageNames[(($f.FullName.Substring($canonicalGame.Length)) -replace '\\', '/').ToLowerInvariant()] = $true }
foreach ($key in $installed.Keys) {
    $isAddition = $key -match $runtimeDll -or $key -like 'MKRaudstein*'
    $isExe = $key -ieq 'Generals.exe'
    if (-not $isAddition -and -not $isExe -and -not $stageNames.ContainsKey($key.ToLowerInvariant())) { $parityBad += "$key (installed but not in the staged candidate)" }
}
Check ($parityBad.Count -eq 0) "FILE-FOR-FILE: clean installation + ZIP == the staged candidate ($($installed.Count) files installed; additions: runtime DLLs, README, licence, manifest)$(if ($parityBad.Count) { ': ' + ($parityBad -join '; ') })"
$added = @(Get-ChildItem -LiteralPath $game -Recurse -File | ForEach-Object { $_.FullName.Substring($installedRoot.Length) } | Where-Object { $_ -notin $beforeNames })
Write-Host "info: files added or replaced by the overlay: $($added -join ', ')"

# ---- 5. static import resolution ---------------------------------------------------------------------------------------------------------------------
$imports = Get-PeImportNames $exe
$system = Join-Path $env:SystemRoot 'System32'
$unresolved = @(); $appLocal = @()
foreach ($i in $imports) {
    if (Test-Path -LiteralPath (Join-Path $game $i)) { $appLocal += $i }
    elseif ($i -like 'api-ms-win-*' -or (Test-Path -LiteralPath (Join-Path $system $i))) { }
    else { $unresolved += $i }
}
Check ($unresolved.Count -eq 0) "every imported DLL resolves ($($imports.Count) imports: $($appLocal.Count) from the extracted folder, the rest Windows system DLLs)$(if ($unresolved.Count) { '; unresolved: ' + ($unresolved -join ', ') })"
Check (@($imports | Where-Object { $_ -match '^(?i)(vcruntime140|msvcp140)' } | Where-Object { $_ -notin $appLocal }).Count -eq 0) 'the Visual C++ runtime comes from the extracted folder (app-local), not from the machine'
Write-Host 'info: the executable embeds its build-time .pdb path (debug directory): informational, not a runtime dependency (release hygiene item)'

# ---- 6. overlay run + feature parity ------------------------------------------------------------------------------------------------------------------
$launch = $null
if (-not $SkipLaunch) {
    $overlay = Invoke-Run $exe $game 'overlay'
    Check ((-not $overlay.TimedOut) -and $overlay.Exit -eq 0) "OVERLAY: the extracted Generals.exe ran and exited cleanly (exit code $($overlay.Exit))"
    Check ($overlay.Text -match 'normal_launch') 'OVERLAY: the log records a normal launch (the Generals.exe name selects the modern runtime)'
    Check (Test-Path -LiteralPath (Join-Path $overlay.Out 'autoshot-shell.bmp')) 'OVERLAY: the main menu / shell was reached and rendered'
    Check ($overlay.Text -match 'flow complete') "OVERLAY: the $(if ($flow -eq 'skirmish') { 'Skirmish setup' } else { 'General''s Challenge' }) screens were driven to the match"
    Check ($overlay.Text -match 'autoshot\s+complete' -and (Test-Path -LiteralPath (Join-Path $overlay.Out 'autoshot-game.bmp'))) 'OVERLAY: a match frame was rendered and captured'
    Check ($overlay.Text -notmatch '\sfail\s') 'OVERLAY: no failed phase in the log'
    $refProbe = Get-Probe $reference.Text; $ovProbe = Get-Probe $overlay.Text
    Check ($refProbe.Count -gt 0 -and ($refProbe -join "`n") -eq ($ovProbe -join "`n")) "FEATURE PARITY: the feature probe of the overlay install equals the staged candidate's ($($ovProbe.Count) lines)$(if (($refProbe -join '|') -ne ($ovProbe -join '|')) { '; reference: ' + ($refProbe -join ' / ') + '; overlay: ' + ($ovProbe -join ' / ') })"
    Check (((Get-FlowSteps $reference.Text) -join '|') -eq ((Get-FlowSteps $overlay.Text) -join '|')) 'FEATURE PARITY: the real screens were driven through the same steps in both runs'
    $launch = [pscustomobject]@{ Reference = $reference.Out; Overlay = $overlay.Out; OverlayExit = $overlay.Exit; Probe = $ovProbe }
}

$failed = @($results | Where-Object { -not $_.Pass })
$report = [ordered]@{ zip = (Split-Path $Zip -Leaf); zipSha256 = (Get-FileHash -LiteralPath $Zip -Algorithm SHA256).Hash; candidate = $Candidate; stage = $stageRoot; projectFiles = @($stageProject | ForEach-Object Relative); launch = $launch; checks = $results; imports = $imports }
$reportPath = Join-Path (Split-Path $Zip -Parent) ((Split-Path $Zip -Leaf) -replace '\.zip$', '.overlay-test.json')
($report | ConvertTo-Json -Depth 6) | Set-Content -LiteralPath $reportPath -Encoding utf8
Write-Host "$($results.Count - $failed.Count) of $($results.Count) checks passed; report: $reportPath"
if ($failed.Count) { throw "$($failed.Count) check(s) failed." }
Write-Host 'OVERLAY TEST PASS (automated, disposable layout; this is NOT the real Steam test).'
