#Requires -Version 7.0
# Synthetic receipts and guard rejection only. Never launches a title.
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
$receipt=[pscustomobject]@{Executable='C:\stage\game\generalsv.exe';UserData='C:\run\userdata\';DumpPath='C:\run\userdata\CrashDumps';WorkingDirectory='C:\stage\game'}
$lines=[Collections.Generic.List[string]]::new()
function Add([string]$Stage,[string]$Status='complete',[string]$Detail='',[uint32]$Code=0){
    $lines.Add("$($lines.Count+1)`t2026-10-12`t0`t123`t456`t$Stage`t$Status`t$Code`t$Detail")
}
Add WinMain begin 'Generals AMD64 ordinary game'
Add module recorded $receipt.Executable
Add userdata isolated $receipt.UserData
Add dump_path recorded ($receipt.DumpPath+'\')
Add startup_parse complete $receipt.WorkingDirectory
Add app_window begin 'x'
Add app_window_class complete 'Game Window'
Add app_window complete 'client=800x600 windowed=1 class=Game Window'
Add engine_init begin
Add TheKeyboard.init complete
Add display_config complete 'windowed=1 800x600x32 ApplicationHWnd'
Add graphics_device complete 'driver=hardware feature=0xB000 vendor=0x8086 device=0x7D45 adapter=test size=800x600'
Add display_module_audit complete 'd3d8=0 d3dx8=0 d3d11=1 dxgi=1'
Add display_size complete 'device=800x600 target=800x600 bits=32 windowed=1'
Add display_ordinary_init complete
Add TheWindowManager.init complete
Add TheShell.init complete
Add TheMouse.init complete
Add engine_init complete
Add game_loop begin 'x'
Add graphics_present complete 'HRESULT=0x00000000 visible=1 occluded=0 removed=0x00000000 frame=1'
Add game_start complete 'mode=2 map=Maps\Alpine Assault\Alpine Assault.map players=3 frame=0'
Add game_progress observed 'logic_frame=1 crc=0000A1B2 presents=40'
Add game_progress observed 'logic_frame=30 crc=11223344 presents=70'
Add game_progress observed 'logic_frame=150 crc=55667788 presents=200'
Add game_exit complete 'MSG_CLEAR_GAME_DATA processed; logic not in game'
Add game_summary complete 'presents=3000 updates=3000 elapsed_ms=90000 game=1 logic_frames=1800 exit=frame_bound driver=hardware'
Add ordinary_host_boundary complete 'ordinary game loop; GameLogic live; no world rendering; execute/replay not entered'
Add engine_cleanup begin
Add ui_residency_release complete 'live_residencies=0 before backend release'
Add display_detach complete 'backend/device released; HWND still owned by WinMain'
Add engine_cleanup complete
Add app_window_release complete
Add minidumper_shutdown complete
Add native_shutdown armed
Add WinMain complete 'exit'
$log=$lines -join "`n"
$checks=0
function Verify([string]$Text,[bool]$Valid,[string]$Name){
    $r=Test-NativeOrdinaryGameEvidence $Text 123 $receipt
    if($r.Valid -ne $Valid){throw "Unexpected classifier result for ${Name}: $($r.Errors -join '; ')"}
    $script:checks++
}
Verify $log $true 'baseline'
Verify ($log.Replace('exit=frame_bound','exit=game_quit')) $true 'player quit'
foreach($reason in 'boundary','time_bound','menu_exit','failure'){Verify ($log.Replace('exit=frame_bound',"exit=$reason")) $false "reason $reason"}
Verify '' $false 'empty'
Verify ($log.Replace('game=1 logic_frames=1800','game=0 logic_frames=0')) $false 'no game'
Verify ($log.Replace('logic_frames=1800','logic_frames=100')) $false 'too few logic frames'
Verify ($log.Replace('players=3','players=1')) $false 'one player'
Verify ($log.Replace('d3d8=0','d3d8=1')) $false 'd3d8 loaded'
Verify ($log.Replace('live_residencies=0','live_residencies=5')) $false 'residency leak'
Verify ($log.Replace('Generals AMD64 ordinary game','Generals AMD64 ordinary menu')) $false 'wrong mode'
Verify ($log.Replace("`t123`t","`t99999999999999999999`t")) $false 'pid overflow'
Verify ($log.Replace('elapsed_ms=90000','elapsed_ms=990000')) $false 'unbounded'
Verify ($log.Replace('driver=hardware feature','driver=WARP feature')) $false 'WARP'
Verify ($log + "`n99`t0`t0`t123`t456`tmenu_boundary`tobserved`t0`treason=shell_push x") $false 'boundary row'
foreach($stage in 'app_window','TheKeyboard.init','TheWindowManager.init','game_start','game_summary','ui_residency_release','app_window_release'){
    Verify ($log.Replace("`t$stage`tcomplete","`t$stage`tmissing")) $false "missing $stage"
}
$expected=@('-win','-xres','800','-yres','600','-nologo','-noshellmap','-winCursors','-nativeOrdinaryGame','C:\run')
if(((Get-NativeOrdinaryGameArguments 'C:\run') -join ' ') -ne ($expected -join ' ')){throw 'Wrong strict argument list'};$checks++
foreach($other in (Get-NativeOrdinaryMenuMode),(Get-NativeOrdinaryHostMode)){if((Get-NativeOrdinaryGameMode) -eq $other){throw 'Modes must differ'}};$checks++
$rejected=$false
try{& (Join-Path $PSScriptRoot 'Invoke-NativeOrdinaryGame.ps1') -Title Generals -Name unused | Out-Null}catch{$rejected=$_.Exception.Message -like '*requires explicit developer review*'}
if(!$rejected){throw 'Missing review switch did not fail closed'};$checks++
$rejected=$false
try{& (Join-Path $PSScriptRoot 'Test-NativeOrdinaryGame.ps1') -Title ZeroHour -Name unused | Out-Null}catch{$rejected=$true}
if(!$rejected){throw 'Zero Hour must be rejected'};$checks++
Write-Host "PASS: $checks ordinary-game evidence/guard checks. No game launched."
