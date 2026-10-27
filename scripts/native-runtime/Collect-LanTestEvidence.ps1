# LAN multiplayer test: COPIES the evidence of one PC (host or client) into one folder + one ZIP. Copy only: nothing is moved, deleted, overwritten or changed;
# the game folder, the Steam installation and the user data are read-only here. Works in Windows PowerShell 5.1 and PowerShell 7.
#
#   powershell -ExecutionPolicy Bypass -File Collect-LanTestEvidence.ps1 -GameDir "<game folder>" -Role Host -Tag T07-desync -OutDir C:\lan-test
#
# Run it on EACH PC right after a test case ends (also after a crash, once the game process has exited): the game overwrites "Last Replay.rep" the next time a match starts.
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$GameDir,
      [ValidateSet('Host', 'Client')][string]$Role = 'Host',
      [Parameter(Mandatory = $true)][string]$Tag,
      [string]$OutDir = '.',
      [int]$LastMinutes = 180)          # only replays / dumps / events newer than this are collected
$ErrorActionPreference = 'Continue'
$since = (Get-Date).AddMinutes(-$LastMinutes)
$gameDirFull = (Resolve-Path -LiteralPath $GameDir).Path
$title = if ((Test-Path -LiteralPath (Join-Path $gameDirFull 'ZH_Generals')) -or (Test-Path -LiteralPath (Join-Path $gameDirFull 'INIZH.big'))) { 'ZeroHour' } else { 'Generals' }
$docs = [Environment]::GetFolderPath('MyDocuments')
$userDir = if ($title -eq 'ZeroHour') { Join-Path $docs 'Command and Conquer Generals Zero Hour Data' } else { Join-Path $docs 'Command and Conquer Generals Data' }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$name = "lan-evidence-{0}-{1}-{2}-{3}-{4}" -f $Role, $env:COMPUTERNAME, $title, $Tag, $stamp
$dest = Join-Path $OutDir $name
New-Item -ItemType Directory -Force -Path $dest | Out-Null
$notes = New-Object System.Collections.ArrayList
function Note($t) { [void]$notes.Add($t); Write-Host $t }
function CopyIf($src, $subdir, $newName) {
    if (-not (Test-Path -LiteralPath $src)) { Note ("  absent  : {0}" -f $src); return }
    $d = Join-Path $dest $subdir
    New-Item -ItemType Directory -Force -Path $d | Out-Null
    $leaf = if ($newName) { $newName } else { Split-Path $src -Leaf }
    Copy-Item -LiteralPath $src -Destination (Join-Path $d $leaf) -Force
    $i = Get-Item -LiteralPath $src
    Note ("  copied  : {0}  ({1:N0} bytes, modified {2})" -f $src, $i.Length, $i.LastWriteTime.ToString('HH:mm:ss'))
}

Note ("LAN evidence  role={0} title={1} tag={2} computer={3} at {4}" -f $Role, $title, $Tag, $env:COMPUTERNAME, (Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'))
Note ("collecting files newer than {0}" -f $since.ToString('yyyy-MM-dd HH:mm:ss'))

# 1. the game's own launch log (next to the executable): startup, display, normal_launch, shutdown. Release builds have no network debug log.
CopyIf (Join-Path $gameDirFull 'generals64.log') 'game' $null
# 2. replays: "Last Replay.rep" is written for EVERY match including LAN matches and carries the desync flag; copy it before the next match overwrites it
$replays = Join-Path $userDir 'Replays'
if (Test-Path -LiteralPath $replays) {
    Get-ChildItem -LiteralPath $replays -Filter *.rep -File | Where-Object { $_.LastWriteTime -gt $since } | ForEach-Object {
        $copyName = '{0}-{1}-{2}' -f $Role, $_.LastWriteTime.ToString('HHmmss'), ($_.Name -replace '\s', '_')
        CopyIf $_.FullName 'replays' $copyName
    }
} else { Note ("  absent  : {0}" -f $replays) }
# 3. crash dumps (the engine writes them to <user data>\CrashDumps)
$dumps = Join-Path $userDir 'CrashDumps'
if (Test-Path -LiteralPath $dumps) { Get-ChildItem -LiteralPath $dumps -File | Where-Object { $_.LastWriteTime -gt $since } | ForEach-Object { CopyIf $_.FullName 'crashdumps' $null } } else { Note ("  absent  : {0}" -f $dumps) }
# 4. settings that influence LAN play (a copy of Options.ini; no saves, no profiles)
CopyIf (Join-Path $userDir 'Options.ini') 'settings' $null
# 5. the system view of the network at this moment
$sys = Join-Path $dest 'system'
New-Item -ItemType Directory -Force -Path $sys | Out-Null
try { ipconfig /all | Set-Content -LiteralPath (Join-Path $sys 'ipconfig-all.txt') -Encoding UTF8 } catch { }
try { route print | Set-Content -LiteralPath (Join-Path $sys 'route-print.txt') -Encoding UTF8 } catch { }
try { arp -a | Set-Content -LiteralPath (Join-Path $sys 'arp-a.txt') -Encoding UTF8 } catch { }
try { netstat -ano -p UDP | Set-Content -LiteralPath (Join-Path $sys 'netstat-udp.txt') -Encoding UTF8 } catch { }
try { Get-NetConnectionProfile | Format-List | Out-String | Set-Content -LiteralPath (Join-Path $sys 'net-profiles.txt') -Encoding UTF8 } catch { }
try { Get-NetUDPEndpoint -ErrorAction SilentlyContinue | Where-Object { $_.LocalPort -in 8086, 8088 } | Format-Table LocalAddress, LocalPort, OwningProcess -AutoSize | Out-String | Set-Content -LiteralPath (Join-Path $sys 'udp-8086-8088.txt') -Encoding UTF8 } catch { }
try { netsh advfirewall show allprofiles state | Set-Content -LiteralPath (Join-Path $sys 'firewall-state.txt') -Encoding UTF8 } catch { }
try {
    Get-NetFirewallApplicationFilter -ErrorAction SilentlyContinue | Where-Object { $_.Program -like '*generals*' } | ForEach-Object {
        $r = Get-NetFirewallRule -AssociatedNetFirewallApplicationFilter $_ -ErrorAction SilentlyContinue
        if ($r) { '{0} enabled={1} action={2} dir={3} profile={4} program={5}' -f $r.DisplayName, $r.Enabled, $r.Action, $r.Direction, $r.Profile, $_.Program }
    } | Set-Content -LiteralPath (Join-Path $sys 'firewall-rules-generals.txt') -Encoding UTF8
} catch { }
try { w32tm /query /status | Set-Content -LiteralPath (Join-Path $sys 'w32tm-status.txt') -Encoding UTF8 } catch { }
Note "  system  : ipconfig, routes, arp, UDP endpoints (8086/8088), network profiles, firewall state / rules, clock status"
# 6. Windows Application log: crashes / hangs of the game (needs no admin rights)
try {
    Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $since } -ErrorAction SilentlyContinue |
        Where-Object { $_.ProviderName -match 'Application Error|Windows Error Reporting|Application Hang|\.NET' -and $_.Message -match 'generals' } |
        Select-Object TimeCreated, ProviderName, Id, Message | Format-List | Out-String | Set-Content -LiteralPath (Join-Path $sys 'eventlog-application.txt') -Encoding UTF8
    Note "  events  : Application log entries mentioning the game since the cut-off"
} catch { Note "  events  : not readable" }
# 7. the fingerprint of this PC (same folder as this script) when it is available
$fp = Join-Path $PSScriptRoot 'Get-LanTestFingerprint.ps1'
if (Test-Path -LiteralPath $fp) { try { & $fp -GameDir $gameDirFull -Role $Role -OutDir (Join-Path $dest 'fingerprint') | Out-Null; Note "  fingerprint written" } catch { Note "  fingerprint failed: $_" } }

$notes | Set-Content -LiteralPath (Join-Path $dest 'COLLECTED.txt') -Encoding UTF8
$zip = $dest + '.zip'
try { Compress-Archive -LiteralPath $dest -DestinationPath $zip -Force; Note ("ZIP: {0}" -f $zip) } catch { Note "ZIP not created: $_" }
Write-Host ""
Write-Host "Also keep by hand: screenshots / phone photos of any dialog (CRC mismatch, 'Connection lost', Join denied) and a note with the wall-clock time of the event."
