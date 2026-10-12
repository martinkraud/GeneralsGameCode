#Requires -Version 7.0
# Synthetic receipts and guard rejection only. Never launches a title.
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
$receipt=[pscustomobject]@{Executable='C:\stage\game\generalsv.exe';UserData='C:\run\userdata\';DumpPath='C:\run\userdata\CrashDumps';WorkingDirectory='C:\stage\game'}
$lines=[Collections.Generic.List[string]]::new()
function Add([string]$Stage,[string]$Status='complete',[string]$Detail='',[uint32]$Code=0){
    $lines.Add("$($lines.Count+1)`t2026-10-12`t0`t123`t456`t$Stage`t$Status`t$Code`t$Detail")
}
Add WinMain begin 'Generals AMD64 ordinary host'
Add module recorded $receipt.Executable
Add userdata isolated $receipt.UserData
Add dump_path recorded ($receipt.DumpPath+'\')
Add startup_parse complete $receipt.WorkingDirectory
Add app_window begin 'ordinary WndProc; windowed 800x600 client'
Add app_window_class complete 'Game Window'
Add app_window complete 'client=800x600 windowed=1 class=Game Window'
Add engine_init begin
Add display_ordinary_init begin
Add display_config complete 'windowed=1 800x600x32 ApplicationHWnd'
foreach($b in 'legacy_ww3d_init: x','legacy_render_device: x','legacy_scenes: x','legacy_shader_manager: x','legacy_debug_display_browser: x','legacy_gamma_lod: x'){Add display_bypass complete $b}
Add graphics_device complete 'driver=hardware feature=0xB000 vendor=0x8086 device=0x7D45 adapter=test size=800x600'
foreach($s in 'graphics_window_association','graphics_target','graphics_setup','graphics_viewport'){Add $s}
Add display_module_audit complete 'd3d8=0 d3dx8=0 d3d11=1 dxgi=1'
Add display_size complete 'device=800x600 target=800x600 bits=32 windowed=1'
Add display_ordinary_init complete
Add engine_init complete
Add ui_ordinary_content complete 'file=Art\Textures\x.tga label=GUI:SelectDifficulty font=Arial size=14'
Add display_frame_begin complete 'ordinary W3DDisplay::draw color-only; no views/world'
Add ui_ordinary_draw complete 'W3DDisplay fill/open rect, global-cache GeneralsLogo, original label, DisplayString caption -> Render2D -> D3D11'
Add display_frame_end complete 'WW3D::End_Render Present'
Add graphics_present complete 'HRESULT=0x00000000 visible=1 occluded=0 removed=0x00000000 frame=1'
Add ordinary_host_summary complete 'frames=235 visible_frames=235 occluded_frames=0 elapsed_ms=5015 driver=hardware'
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
    $r=Test-NativeOrdinaryHostEvidence $Text 123 $receipt
    if($r.Valid -ne $Valid){throw "Unexpected classifier result for ${Name}: $($r.Errors -join '; ')"}
    $script:checks++
}
Verify $log $true 'baseline'
Verify '' $false 'empty'
Verify ($log.Replace('d3d8=0','d3d8=1')) $false 'd3d8 loaded'
Verify ($log.Replace('live_residencies=0','live_residencies=2')) $false 'residency leak'
Verify ($log.Replace('visible=1 occluded=0','visible=0 occluded=1')) $false 'occluded'
Verify ($log.Replace('driver=hardware feature','driver=WARP feature')) $false 'WARP'
Verify ($log.Replace('elapsed_ms=5015','elapsed_ms=4000')) $false 'short hold'
Verify ($log.Replace('frames=235 visible_frames=235','frames=301 visible_frames=235')) $false 'frame bound'
Verify ($log.Replace("`t123`t","`t99999999999999999999`t")) $false 'pid overflow'
Verify ($log.Replace('windowed=1 800x600x32','windowed=0 800x600x32')) $false 'fullscreen'
Verify ($log.Replace('execute/replay/simulation/GameLogic skipped','execute started')) $false 'boundary'
Verify ($log.Replace('app_window_release','missing_app_window_release')) $false 'window never released'
Verify ($log.Replace('legacy_scenes','missing_bypass')) $false 'missing receipted bypass'
Verify ($log.Replace('Generals AMD64 ordinary host','Generals AMD64 UI menu smoke')) $false 'other mode'
Verify ($log + "`n99`t0`t0`t123`t456`tinit_only`tcomplete`t0`t") $false 'foreign trailing stage'
# Teardown order: window must follow cleanup; swap them.
$swapped=$log.Replace("engine_cleanup`tcomplete`t0`t`n","TMP`n").Replace("app_window_release`tcomplete`t0`t`n","engine_cleanup`tcomplete`t0`t`n").Replace("TMP`n","app_window_release`tcomplete`t0`t`n")
Verify $swapped $false 'window destroyed before engine cleanup'
foreach($stage in 'app_window','display_config','display_module_audit','display_size','ui_ordinary_content','ui_ordinary_draw','ordinary_host_summary','ui_residency_release','display_detach','app_window_release'){
    Verify ($log.Replace("`t$stage`tcomplete","`t$stage`tmissing")) $false "missing $stage"
}
$expected=@('-win','-xres','800','-yres','600','-nologo','-noshellmap','-winCursors','-nativeOrdinaryHost','C:\run')
if((Get-NativeOrdinaryHostArguments 'C:\run') -join ' ' -ne ($expected -join ' ')){throw 'Wrong strict argument list'};$checks++
if((Get-NativeOrdinaryHostMode) -eq (Get-NativeGraphicsMode -UIMenu)){throw 'Modes must differ'};$checks++
$rejected=$false
try{& (Join-Path $PSScriptRoot 'Invoke-NativeOrdinaryHost.ps1') -Title Generals -Name unused | Out-Null}catch{$rejected=$_.Exception.Message -like '*requires explicit developer review*'}
if(!$rejected){throw 'Missing review switch did not fail closed'};$checks++
$rejected=$false
try{& (Join-Path $PSScriptRoot 'Test-NativeOrdinaryHost.ps1') -Title ZeroHour -Name unused | Out-Null}catch{$rejected=$true}
if(!$rejected){throw 'Zero Hour must be rejected'};$checks++
$build=Get-NativeBuild Generals
$null=Assert-NativeGraphicsDependencies $build.Executable (Split-Path $build.Executable -Parent)
$checks++
Write-Host "PASS: $checks ordinary-host evidence/guard/dependency checks. No game launched."
