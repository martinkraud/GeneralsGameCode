#Requires -Version 7.0
# X64.47A: automated validation of the Zero Hour overlay ZIP on a DISPOSABLE installation layout (never the real Steam directory).
#  1. package audit: only the manifest's files, hashes match, no EA/proprietary file types, Generals.exe is AMD64;
#  2. builds a disposable "installation" with the existing stager (legitimate assets copied from the read-only Steam directory), puts a
#     placeholder where Steam's Generals.exe is, and extracts the ZIP over it exactly like a player (overwrite);
#  3. static import check: every DLL the executable imports resolves either to a file shipped in the ZIP or to a Windows system DLL;
#  4. unless -SkipLaunch: normal-launches the extracted Generals.exe (no diagnostic token, PATH reduced to Windows only so no Visual
#     Studio / CMake directory can satisfy a dependency) through the General's Challenge screens with the developer capture aid, and
#     checks the log (normal launch, a world, clean exit).
# This is automated validation, NOT the human Steam test.
[CmdletBinding()]
param([string]$Zip,
      [string]$Name = 'x64-47a-overlay-test',
      [string]$AssetSource = 'C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour',
      [switch]$SkipLaunch,
      [ValidateRange(60,600)][int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$repo = $script:ZHRepository
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (-not $Zip) { $Zip = Get-ChildItem (Join-Path $repo 'build\dist') -Filter 'MKRaudstein-ZeroHour-x64-Preview-*.zip' | Where-Object { $_.Name -notmatch 'Symbols' } | Sort-Object LastWriteTime | Select-Object -Last 1 -ExpandProperty FullName }
if (-not $Zip -or -not (Test-Path -LiteralPath $Zip)) { throw 'Package not found; run New-ZeroHourOverlayPackage.ps1 first.' }
$results = [System.Collections.Generic.List[object]]::new()
function Check([bool]$Condition, [string]$Message) { $results.Add([pscustomobject]@{ Pass = $Condition; Check = $Message }); if (-not $Condition) { Write-Host "FAIL: $Message" -ForegroundColor Red } else { Write-Host "ok  : $Message" } }
function Sha([byte[]]$Bytes) { [BitConverter]::ToString([Security.Cryptography.SHA256]::HashData($Bytes)).Replace('-', '') }
function Read-Entry($Entry) { $s = $Entry.Open(); try { $m = [IO.MemoryStream]::new(); $s.CopyTo($m); return $m.ToArray() } finally { $s.Dispose() } }

# ---- 1. package audit -----------------------------------------------------------------------------------------------------------------
$archive = [IO.Compression.ZipFile]::OpenRead($Zip)
try {
    $entries = @{}
    foreach ($e in $archive.Entries) { $entries[$e.FullName] = Read-Entry $e }
} finally { $archive.Dispose() }
$manifest = [Text.Encoding]::UTF8.GetString($entries['MKRaudstein/manifest.json']) | ConvertFrom-Json
$listed = @($manifest.files | ForEach-Object { $_.file -replace '\\', '/' })
Check (($entries.Keys | Sort-Object) -join '|' -eq ((@($listed) + 'MKRaudstein/manifest.json' | Sort-Object -Unique) -join '|')) 'the ZIP contains exactly the files of its manifest (no more, no less)'
$hashOk = $true
foreach ($f in $manifest.files) { $key = $f.file -replace '\\', '/'; if ($key -ne 'MKRaudstein/manifest.json' -and (Sha $entries[$key]) -ne $f.sha256) { $hashOk = $false } }
Check $hashOk 'every packaged file matches the SHA-256 in the manifest'
$prohibited = @($entries.Keys | Where-Object { $_ -match '\.(big|bik|asi|m3d|flt|ini|wnd|tga|dds|wav|mp3|map|w3d|str|csf|dat|pdb|ilk|obj|lib|cpp|h|ps1|cmake|log|vdf|acf)$' -or $_ -match '^(Data|Maps|Art|Textures|Movies|Audio)/' })
Check ($prohibited.Count -eq 0) "no proprietary asset / developer file types in the ZIP$(if ($prohibited.Count) { ': ' + ($prohibited -join ', ') })"
$exes = @($entries.Keys | Where-Object { $_ -match '\.(exe)$' })
Check ($exes.Count -eq 1 -and $exes[0] -eq 'Generals.exe') 'the only executable is Generals.exe'
$machine = [BitConverter]::ToUInt16($entries['Generals.exe'], [BitConverter]::ToInt32($entries['Generals.exe'], 0x3c) + 4)
Check ($machine -eq 0x8664) 'Generals.exe is an AMD64 (x64) PE image'
$dllNames = @($entries.Keys | Where-Object { $_ -match '\.dll$' })
Check ($dllNames.Count -eq 4 -and -not ($dllNames | Where-Object { $_ -notmatch '^(?i)(vcruntime140|vcruntime140_1|msvcp140|msvcp140_atomic_wait)\.dll$' })) 'the only DLLs are the four Visual C++ runtime DLLs'

# ---- 2. disposable installation + overlay ----------------------------------------------------------------------------------------------
$stageRoot = Join-Path $repo "build\dev-runtimes\native\ZeroHour\$Name"
if (Test-Path -LiteralPath $stageRoot) { throw "Disposable stage already exists: $stageRoot (remove it by hand: it is yours, by literal path)." }
& (Join-Path $PSScriptRoot 'Stage-NativeOrdinaryPlay.ps1') -Title ZeroHour -Name $Name -AssetSource $AssetSource | Out-Null
$game = Join-Path $stageRoot 'game'
foreach ($leftover in 'generalszh.exe', 'generalszh.pdb') { [IO.File]::Delete((Join-Path $game $leftover)) }   # the developer stage's own runtime: not part of an installation
[IO.File]::WriteAllText((Join-Path $game 'Generals.exe'), 'placeholder standing in for the Electronic Arts Generals.exe of the Steam installation')
$beforeNames = @(Get-ChildItem -LiteralPath $game -File | ForEach-Object Name)
Expand-Archive -LiteralPath $Zip -DestinationPath $game -Force      # what the player does: extract over the installation, overwrite
$exe = Join-Path $game 'Generals.exe'
Check ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -eq ($manifest.files | Where-Object file -eq 'Generals.exe').sha256) 'extracting replaced the placeholder Generals.exe with the packaged runtime'
$added = @(Get-ChildItem -LiteralPath $game -File | ForEach-Object Name | Where-Object { $_ -notin $beforeNames })
Check (-not ($added | Where-Object { $_ -notmatch '^(?i)(vcruntime140|vcruntime140_1|msvcp140|msvcp140_atomic_wait)\.dll$|^MKRaudstein-ZeroHour-README\.txt$' })) "the overlay added only runtime DLLs and the README to the installation root ($($added -join ', '))"

# ---- 3. static import resolution ------------------------------------------------------------------------------------------------------
function Get-Imports([string]$Path) {
    $d = [IO.File]::ReadAllBytes($Path); $pe = [BitConverter]::ToInt32($d, 0x3c)
    $sections = [BitConverter]::ToUInt16($d, $pe + 6); $optSize = [BitConverter]::ToUInt16($d, $pe + 20)
    $importRva = [BitConverter]::ToUInt32($d, $pe + 24 + 112 + 8); $table = $pe + 24 + $optSize
    $map = @(for ($i = 0; $i -lt $sections; $i++) { $o = $table + $i * 40; [pscustomobject]@{ Va = [BitConverter]::ToUInt32($d, $o + 12); Size = [BitConverter]::ToUInt32($d, $o + 8); Raw = [BitConverter]::ToUInt32($d, $o + 20) } })
    function ToOffset([uint32]$Rva) { foreach ($s in $map) { if ($Rva -ge $s.Va -and $Rva -lt $s.Va + [Math]::Max($s.Size, 1)) { return [int]($Rva - $s.Va + $s.Raw) } } }
    $names = @(); $o = ToOffset $importRva
    while ($true) { $n = [BitConverter]::ToUInt32($d, $o + 12); if (-not $n) { break }; $s = ToOffset $n; $e = [Array]::IndexOf($d, [byte]0, $s); $names += [Text.Encoding]::ASCII.GetString($d, $s, $e - $s); $o += 20 }
    return $names
}
$imports = Get-Imports $exe
$system = Join-Path $env:SystemRoot 'System32'
$unresolved = @(); $appLocal = @()
foreach ($i in $imports) {
    if (Test-Path -LiteralPath (Join-Path $game $i)) { $appLocal += $i }
    elseif ($i -like 'api-ms-win-*' -or (Test-Path -LiteralPath (Join-Path $system $i))) { }
    else { $unresolved += $i }
}
Check ($unresolved.Count -eq 0) "every imported DLL resolves ($($imports.Count) imports: $($appLocal.Count) from the extracted folder, the rest Windows system DLLs)$(if ($unresolved.Count) { '; unresolved: ' + ($unresolved -join ', ') })"
Check (@($imports | Where-Object { $_ -match '^(?i)(vcruntime140|msvcp140)' } | Where-Object { $_ -notin $appLocal }).Count -eq 0) 'the Visual C++ runtime comes from the extracted folder (app-local), not from the machine'
$repoPaths = @(); $exeText = [Text.Encoding]::ASCII.GetString($entries['Generals.exe'])
foreach ($pattern in 'ProjectsX', 'GeneralsGameCode\\build') { if ($exeText.Contains($pattern)) { $repoPaths += $pattern } }
Write-Host "info: build-tree strings inside the executable (debug directory path to the .pdb): $(if ($repoPaths) { $repoPaths -join ', ' } else { 'none' }) - informational, not a runtime dependency"

# ---- 4. normal launch of the extracted overlay ---------------------------------------------------------------------------------------------
$launch = $null
if (-not $SkipLaunch) {
    if (@(Get-Process -Name generals, generalsv, generalszh -ErrorAction SilentlyContinue).Count) { throw 'Exit all existing games first' }
    $out = Join-Path $repo ('build\diag\overlay-test-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    $saved = @{ P = $env:PATH; S = $env:GENERALS_AUTOSHOT; F = $env:GENERALS_AUTOFLOW; N = $env:GENERALS_AUTOSHOT_FRAMES }
    # Windows only on the DLL search path: no Visual Studio, CMake or repository directory can satisfy a dependency.
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $env:GENERALS_AUTOSHOT = $out; $env:GENERALS_AUTOFLOW = 'challenge'; $env:GENERALS_AUTOSHOT_FRAMES = '400'
    try {
        $p = Start-Process -FilePath $exe -WorkingDirectory $game -PassThru     # no arguments: exactly what Steam's Play does
        if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { $p.Kill(); $timedOut = $true } else { $timedOut = $false }
        $exit = if ($timedOut) { -1 } else { $p.ExitCode }
    } finally { $env:PATH = $saved.P; $env:GENERALS_AUTOSHOT = $saved.S; $env:GENERALS_AUTOFLOW = $saved.F; $env:GENERALS_AUTOSHOT_FRAMES = $saved.N }
    $log = Join-Path $game 'generals64.log'
    $text = if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Raw } else { '' }
    if ($text) { Copy-Item -LiteralPath $log -Destination (Join-Path $out 'generals64.log') }
    Check ((-not $timedOut) -and $exit -eq 0) "the extracted Generals.exe ran and exited cleanly (exit code $exit)"
    Check ($text -match 'normal_launch') 'the log records a normal launch (the Generals.exe name selects the modern runtime)'
    Check ($text -match 'autoflow\s+observed\s+0\s+flow complete' -or $text -match 'flow complete') 'the Challenge screens were driven to the match'
    Check ($text -match 'autoshot\s+complete' -and (Test-Path -LiteralPath (Join-Path $out 'autoshot-game.bmp'))) 'a match frame was rendered and captured'
    Check ($text -notmatch '\sfail\s') 'no failed phase in the log'
    $launch = [pscustomobject]@{ Output = $out; ExitCode = $exit }
}

$failed = @($results | Where-Object { -not $_.Pass })
$report = [ordered]@{ zip = (Split-Path $Zip -Leaf); zipSha256 = (Get-FileHash -LiteralPath $Zip -Algorithm SHA256).Hash; stage = $stageRoot; launch = $launch; checks = $results; imports = $imports }
$reportPath = Join-Path (Split-Path $Zip -Parent) ((Split-Path $Zip -Leaf) -replace '\.zip$', '.overlay-test.json')
($report | ConvertTo-Json -Depth 6) | Set-Content -LiteralPath $reportPath -Encoding utf8
Write-Host "$($results.Count - $failed.Count) of $($results.Count) checks passed; report: $reportPath"
if ($failed.Count) { throw "$($failed.Count) check(s) failed." }
Write-Host 'OVERLAY TEST PASS (automated, disposable layout; this is NOT the real Steam test).'
