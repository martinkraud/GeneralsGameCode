# LAN multiplayer test: READ-ONLY fingerprint of one PC (host or client) so two clients can be proven to run the same build and compatible game data BEFORE a result is trusted.
# Changes nothing: no file is written except the report you ask for, no registry / firewall / user-data change. Works in Windows PowerShell 5.1 and PowerShell 7.
#
#   powershell -ExecutionPolicy Bypass -File Get-LanTestFingerprint.ps1 -GameDir "C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour" -Role Host -OutDir C:\lan-test
#   (laptop dev-runtime: -GameDir <repo>\build\dev-runtimes\native\ZeroHour\dist-a885d798f-zh\game)
#
# Expected executable SHA-256 of the validated a885d798f distribution (identical in the ZIP and in the staged dist-a885d798f-* candidates):
#   Generals  : C182DF6F0F2705EADF00297D4B36521014BEB2A01A01DB15D4B17A0F7429B07C   (Generals.exe in the ZIP, generalsv.exe in the dev stage)
#   Zero Hour : ECBDB6D4044485E8ED98E3AD577BAE17CE0E7FDCDAAC66774DE44CF4D923BB42  (Generals.exe in the ZIP, generalszh.exe in the dev stage)
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$GameDir,
      [ValidateSet('Host', 'Client')][string]$Role = 'Host',
      [string]$OutDir = '.')
$ErrorActionPreference = 'Continue'
$expected = @{ Generals = 'C182DF6F0F2705EADF00297D4B36521014BEB2A01A01DB15D4B17A0F7429B07C'; ZeroHour = 'ECBDB6D4044485E8ED98E3AD577BAE17CE0E7FDCDAAC66774DE44CF4D923BB42' }
$lines = New-Object System.Collections.ArrayList
function Out-Line($text) { [void]$lines.Add($text); Write-Host $text }

Out-Line ("== LAN test fingerprint ({0}) {1}" -f $Role, (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
Out-Line ("computer      : {0}   user: {1}" -f $env:COMPUTERNAME, $env:USERNAME)
try { $os = Get-CimInstance Win32_OperatingSystem; Out-Line ("os            : {0} build {1}" -f $os.Caption, $os.BuildNumber) } catch { }
try { $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1; Out-Line ("cpu           : {0} ({1} cores)" -f $cpu.Name.Trim(), $cpu.NumberOfCores) } catch { }
try { $gpu = (Get-CimInstance Win32_VideoController | ForEach-Object { $_.Name }) -join '; '; Out-Line ("gpu           : {0}" -f $gpu) } catch { }

# ---- which executable
$gameDirFull = (Resolve-Path -LiteralPath $GameDir).Path
$exe = $null
foreach ($name in @('generalszh.exe', 'generalsv.exe', 'Generals.exe')) {
    $p = Join-Path $gameDirFull $name
    if (Test-Path -LiteralPath $p) { $exe = $p; break }
}
if (-not $exe) { Out-Line "ERROR: no Generals executable found in $gameDirFull"; exit 2 }
$title = if ((Test-Path -LiteralPath (Join-Path $gameDirFull 'ZH_Generals')) -or (Test-Path -LiteralPath (Join-Path $gameDirFull 'INIZH.big'))) { 'ZeroHour' } else { 'Generals' }
$item = Get-Item -LiteralPath $exe
$hash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
Out-Line ("game folder   : {0}" -f $gameDirFull)
Out-Line ("title         : {0} (guessed from the folder contents)" -f $title)
Out-Line ("executable    : {0}  {1:N0} bytes  modified {2}" -f $item.Name, $item.Length, $item.LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss'))
Out-Line ("exe SHA-256   : {0}" -f $hash)
$verdict = if ($hash -eq $expected[$title]) { 'MATCHES the validated a885d798f executable' } else { 'DIFFERS from the validated a885d798f executable (not the package build, or another title)' }
Out-Line ("exe verdict   : {0}" -f $verdict)
# x64 check: PE machine field
try {
    $fs = [System.IO.File]::OpenRead($exe); $br = New-Object System.IO.BinaryReader($fs)
    $fs.Seek(0x3C, 'Begin') | Out-Null; $pe = $br.ReadInt32(); $fs.Seek($pe + 4, 'Begin') | Out-Null; $machine = $br.ReadUInt16(); $br.Close()
    Out-Line ("PE machine    : 0x{0:X4} ({1})" -f $machine, $(if ($machine -eq 0x8664) { 'x64' } elseif ($machine -eq 0x14C) { 'x86 / 32-bit' } else { 'other' }))
} catch { Out-Line "PE machine    : unreadable" }

# ---- overlay manifest (the package lists every file it adds; each is re-hashed)
$manifest = Join-Path $gameDirFull 'MKRaudstein\manifest.json'
if (Test-Path -LiteralPath $manifest) {
    Out-Line "package manifest present: re-hashing the overlay files"
    try {
        $m = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
        $files = @($m.files)
        foreach ($f in $files) {
            $rel = if ($f.file) { $f.file } elseif ($f.path) { $f.path } else { $null }
            if (-not $rel) { continue }
            $target = Join-Path $gameDirFull $rel
            $want = if ($f.sha256) { $f.sha256 } elseif ($f.hash) { $f.hash } else { $null }
            if (-not (Test-Path -LiteralPath $target)) { Out-Line ("  MISSING {0}" -f $rel); continue }
            $got = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
            $state = if ($want) { if ($got -eq $want.ToUpper()) { 'ok' } else { 'DIFFERENT' } } else { 'hash n/a' }
            Out-Line ("  {0,-9} {1}" -f $state, $rel)
        }
    } catch { Out-Line "  manifest unreadable: $_" }
} else { Out-Line "package manifest: none (a dev-runtime stage has its own manifest.json one level above the game folder)" }
# overlay data that changes the INI CRC (Generals): hash them even without a manifest
foreach ($rel in @('Data\INI\CommandButton.ini', 'Data\INI\CommandSet.ini', 'Data\INI\PlayerTemplate.ini', 'Window\GenPowersShortcutBarUS.wnd', 'Window\GenPowersShortcutBarChina.wnd', 'Window\GenPowersShortcutBarGLA.wnd')) {
    $p = Join-Path $gameDirFull $rel
    if (Test-Path -LiteralPath $p) { Out-Line ("data override : {0}  {1}" -f $rel, (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.Substring(0, 16)) }
}
# loose INI folders that would change the INI CRC
foreach ($d in @('Data\INI', 'Data\Scripts', 'Maps')) {
    $p = Join-Path $gameDirFull $d
    if (Test-Path -LiteralPath $p) { $n = @(Get-ChildItem -LiteralPath $p -Recurse -File -ErrorAction SilentlyContinue).Count; Out-Line ("loose folder  : {0}  ({1} files)  <- anything here can change the INI / map CRC" -f $d, $n) }
}

# ---- game data version: archive names, sizes, dates
Out-Line "game data archives (name  size  modified) - both PCs must show the same sizes:"
Get-ChildItem -LiteralPath $gameDirFull -Filter *.big -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object { Out-Line ("  {0,-24} {1,14:N0}  {2}" -f $_.Name, $_.Length, $_.LastWriteTime.ToString('yyyy-MM-dd')) }

# ---- user data folder (settings / saves / replays are NOT read or changed, only located)
$docs = [Environment]::GetFolderPath('MyDocuments')
$userDir = if ($title -eq 'ZeroHour') { Join-Path $docs 'Command and Conquer Generals Zero Hour Data' } else { Join-Path $docs 'Command and Conquer Generals Data' }
Out-Line ("user data     : {0}  exists={1}" -f $userDir, (Test-Path -LiteralPath $userDir))
foreach ($sub in @('Replays', 'CrashDumps', 'Save')) { $p = Join-Path $userDir $sub; Out-Line ("  {0,-10}: exists={1}  files={2}" -f $sub, (Test-Path -LiteralPath $p), @(Get-ChildItem -LiteralPath $p -File -ErrorAction SilentlyContinue).Count) }
$opt = Join-Path $userDir 'Options.ini'
if (Test-Path -LiteralPath $opt) { Out-Line "  Options.ini lines of interest:"; Get-Content -LiteralPath $opt | Where-Object { $_ -match '^(IPAddress|LANIPAddress|GameSpyIPAddress|Resolution|StaticGameLOD|SendDelay|Gamma|MusicVolume)\b' } | ForEach-Object { Out-Line ("    {0}" -f $_) } }

# ---- network
Out-Line "network adapters (IPv4):"
try {
    $profiles = @{}
    Get-NetConnectionProfile -ErrorAction SilentlyContinue | ForEach-Object { $profiles[$_.InterfaceIndex] = $_ }
    Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' } | ForEach-Object {
        $cat = if ($profiles.ContainsKey($_.InterfaceIndex)) { $profiles[$_.InterfaceIndex].NetworkCategory } else { 'n/a' }
        Out-Line ("  {0,-34} {1,-16}/{2}  profile={3}" -f $_.InterfaceAlias, $_.IPAddress, $_.PrefixLength, $cat)
    }
} catch { ipconfig | Where-Object { $_ -match 'IPv4|adapter' } | ForEach-Object { Out-Line ("  {0}" -f $_.Trim()) } }
Out-Line "  (the game must be told which one to use: Options > Network > LAN IP; a 'Public' profile blocks inbound LAN traffic by default)"
try {
    $rules = Get-NetFirewallApplicationFilter -ErrorAction SilentlyContinue | Where-Object { $_.Program -like '*generals*' -or $_.Program -like (Join-Path $gameDirFull '*') }
    if ($rules) { Out-Line "firewall rules for the game:"; $rules | ForEach-Object { $r = Get-NetFirewallRule -AssociatedNetFirewallApplicationFilter $_ -ErrorAction SilentlyContinue; if ($r) { Out-Line ("  {0}  enabled={1} action={2} dir={3} profile={4}  {5}" -f $r.DisplayName, $r.Enabled, $r.Action, $r.Direction, $r.Profile, $_.Program) } } }
    else { Out-Line "firewall rules for the game: none found (Windows will ask on the first LAN start: allow on PRIVATE networks)" }
} catch { Out-Line "firewall rules: not readable without admin rights" }
Out-Line "UDP ports used by LAN play: 8086 (lobby discovery, broadcast) and 8088 (game traffic)"
Out-Line ("clock         : {0} (use the same clock source on both PCs: w32tm /resync)" -f (Get-Date -Format 'HH:mm:ss.fff'))

if (-not (Test-Path -LiteralPath $OutDir)) { New-Item -ItemType Directory -Force -Path $OutDir | Out-Null }
$report = Join-Path $OutDir ("lan-fingerprint-{0}-{1}-{2}.txt" -f $Role, $env:COMPUTERNAME, (Get-Date -Format 'yyyyMMdd-HHmmss'))
$lines | Set-Content -LiteralPath $report -Encoding UTF8
Write-Host ""
Write-Host "report written: $report"
