#Requires -Version 7.0
# Synthetic receipts and guard rejection only. Never launches a title.
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
$receipt=[pscustomobject]@{Executable='C:\stage\game\generalsv.exe';UserData='C:\run\userdata\';DumpPath='C:\run\userdata\CrashDumps';WorkingDirectory='C:\stage\game'}
$lines=[Collections.Generic.List[string]]::new()
function Add([string]$Stage,[string]$Status='complete',[string]$Detail='',[uint32]$Code=0){
    $lines.Add("$($lines.Count+1)`t2026-10-12`t0`t123`t456`t$Stage`t$Status`t$Code`t$Detail")
}
Add WinMain begin 'Generals AMD64 ordinary menu'
Add module recorded $receipt.Executable
Add userdata isolated $receipt.UserData
Add dump_path recorded ($receipt.DumpPath+'\')
Add startup_parse complete $receipt.WorkingDirectory
Add app_window begin 'ordinary WndProc; windowed 800x600 client'
Add app_window_class complete 'Game Window'
Add app_window complete 'client=800x600 windowed=1 class=Game Window'
Add engine_init begin
Add TheKeyboard.init complete
Add display_ordinary_init begin
Add display_config complete 'windowed=1 800x600x32 ApplicationHWnd'
Add graphics_device complete 'driver=hardware feature=0xB000 vendor=0x8086 device=0x7D45 adapter=test size=800x600'
Add display_module_audit complete 'd3d8=0 d3dx8=0 d3d11=1 dxgi=1'
Add display_size complete 'device=800x600 target=800x600 bits=32 windowed=1'
Add display_ordinary_init complete
Add TheWindowManager.init complete
Add TheIMEManager.init complete
Add TheShell.init complete
Add TheMouse.init complete
Add engine_init complete
Add menu_shell begin 'x'
Add menu_shell complete 'top=Menus/MainMenu.wnd screens=1 winCursors=1 windows loaded by the real manager; transitions real'
Add menu_loop begin 'x'
Add graphics_present complete 'HRESULT=0x00000000 visible=1 occluded=0 removed=0x00000000 frame=1'
Add menu_input observed 'first input'
Add menu_summary complete 'frames=900 visible_frames=900 occluded_frames=0 elapsed_ms=21000 moves=120 down=3 up=3 keys=2 exit=menu_exit driver=hardware'
Add ordinary_host_boundary complete 'execute/replay/simulation/GameLogic skipped'
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
    $r=Test-NativeOrdinaryMenuEvidence $Text 123 $receipt
    if($r.Valid -ne $Valid){throw "Unexpected classifier result for ${Name}: $($r.Errors -join '; ')"}
    $script:checks++
}
Verify $log $true 'baseline'
foreach($reason in 'window_close','boundary','idle_bound','time_bound'){Verify ($log.Replace('exit=menu_exit',"exit=$reason")) $true "reason $reason"}
Verify ($log.Replace('exit=menu_exit','exit=crash')) $false 'unknown reason'
Verify '' $false 'empty'
Verify ($log.Replace('moves=120','moves=0')) $false 'no mouse movement'
Verify ($log.Replace('down=3 up=3','down=0 up=0')) $false 'no click'
Verify ($log.Replace('frames=900 visible_frames=900','frames=900 visible_frames=0')) $false 'no visible frame'
Verify ($log.Replace('elapsed_ms=21000','elapsed_ms=800000')) $false 'unbounded'
Verify ($log.Replace('d3d8=0','d3d8=1')) $false 'd3d8 loaded'
Verify ($log.Replace('live_residencies=0','live_residencies=3')) $false 'residency leak'
Verify ($log.Replace('top=Menus/MainMenu.wnd screens=1','top=Menus/OptionsMenu.wnd screens=2')) $false 'wrong screen'
Verify ($log.Replace('Generals AMD64 ordinary menu','Generals AMD64 ordinary host')) $false 'wrong mode'
Verify ($log.Replace('execute/replay/simulation/GameLogic skipped','execute started')) $false 'boundary'
Verify ($log.Replace("`t123`t","`t99999999999999999999`t")) $false 'pid overflow'
Verify ($log.Replace('driver=hardware feature','driver=WARP feature')) $false 'WARP'
foreach($stage in 'app_window','TheKeyboard.init','TheWindowManager.init','TheShell.init','TheMouse.init','menu_shell','menu_summary','ui_residency_release','app_window_release'){
    Verify ($log.Replace("`t$stage`tcomplete","`t$stage`tmissing")) $false "missing $stage"
}
Verify ($log + "`n99`t0`t0`t123`t456`tui_ordinary_content`tcomplete`t0`t") $false 'host-gate content in menu mode'
$expected=@('-win','-xres','800','-yres','600','-nologo','-noshellmap','-winCursors','-nativeOrdinaryMenu','C:\run')
if(((Get-NativeOrdinaryMenuArguments 'C:\run') -join ' ') -ne ($expected -join ' ')){throw 'Wrong strict argument list'};$checks++
if((Get-NativeOrdinaryMenuMode) -eq (Get-NativeOrdinaryHostMode)){throw 'Modes must differ'};$checks++
$rejected=$false
try{& (Join-Path $PSScriptRoot 'Invoke-NativeOrdinaryMenu.ps1') -Title Generals -Name unused | Out-Null}catch{$rejected=$_.Exception.Message -like '*requires explicit developer review*'}
if(!$rejected){throw 'Missing review switch did not fail closed'};$checks++
$rejected=$false
try{& (Join-Path $PSScriptRoot 'Test-NativeOrdinaryMenu.ps1') -Title ZeroHour -Name unused | Out-Null}catch{$rejected=$true}
if(!$rejected){throw 'Zero Hour must be rejected'};$checks++
Write-Host "PASS: $checks ordinary-menu evidence/guard checks. No game launched."
