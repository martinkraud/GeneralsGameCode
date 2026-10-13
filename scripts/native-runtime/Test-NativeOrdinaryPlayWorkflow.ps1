#Requires -Version 7.0
# Synthetic receipts and guard rejection only. Never launches a title.
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
$receipt=[pscustomobject]@{Executable='C:\stage\game\generalsv.exe';UserData='C:\run\userdata\';DumpPath='C:\run\userdata\CrashDumps';WorkingDirectory='C:\stage\game'}
$lines=[Collections.Generic.List[string]]::new()
function Add([string]$Stage,[string]$Status='complete',[string]$Detail='',[uint32]$Code=0){
    $lines.Add("$($lines.Count+1)`t2026-10-14`t0`t123`t456`t$Stage`t$Status`t$Code`t$Detail")
}
$play='logic_frame=450 crc=0000A1B2 money=9000 selected=3 cam=(1200,800) objects_drawn=41 triangles=52000 left=12 right=7 keys=30 wheel=2 select=3 move=2 attack=1 stop=0 guard=0 build=1 produce=2 upgrade=0 rally=0 sell=0'
Add WinMain begin 'Generals AMD64 ordinary play'
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
Add terrain_optin complete 'cpu_backed_geometry=1 depth_buffer=1 (terrain-only world pass; meshes/water/shadows/particles absent)'
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
Add game_start complete 'mode=2 map=Maps\Lone Eagle\Lone Eagle.map players=3 frame=0'
Add terrain_first_frame complete 'tiles=12 draws=16 atlas_uploads=1 use_cloud=1 use_noise=1 soft_edge=1 map_extra=350 map_shore=120 map_roads=792 map_bridges=4 (terrain combined single pass + overlays; UI follows)'
Add world_objects_first_frame complete 'registered=312 objects_drawn=41 batches=420 triangles=52000 meshes_drawn=380 skinned=12 textures=60 texture_failures=0 team_colored=44 unsupported_states=3'
Add game_progress observed 'logic_frame=1 crc=0000A1B2 presents=40'
Add game_progress observed 'logic_frame=30 crc=11223344 presents=70'
Add game_progress observed 'logic_frame=150 crc=55667788 presents=200'
Add play_progress observed $play
Add terrain_probe observed 'nonblack_pct=62 center_rgb=3A4B2C cam=(1200,800,300) fwd=(0.00,0.71,-0.71)'
Add ui_probe observed 'whole_nonblack_pct=88 strip_nonblack_pct=64 strip_left=20 strip_mid=96 strip_right=98'
Add ui_probe_window observed 'ControlBar.wnd:ControlBarParent hidden=0 pos=0,416 size=800x184'
Add ui_probe_window observed 'ControlBar.wnd:LeftHUD hidden=0 pos=7,443 size=167x152'
Add ui_probe_facts observed 'radar=real control_bar=1 viewport_scale=0.80 view_height=480 display_height=600'
Add game_exit complete 'match ended by the player or the rules; logic not in game'
Add play_summary complete 'receipts=6 images=3 left=120 right=70 keys=300 wheel=20 select=30 move=22 attack=11 stop=2 guard=1 build=4 produce=9 upgrade=1 rally=0 sell=0'
Add terrain_summary complete 'frames=2800 draws=54900 atlas_uploads=1 tile_uploads=12 tile_updates=34 cloud_frames=2800 noise_frames=2800 macro_loads=2 extra_frames=2500 extra_tiles=40000 road_frames=2700 road_draws=5200 bridge_frames=2800 bridge_draws=11200 shore_frames=2400 shore_tiles=60000 texture_loads=5'
Add world_objects_summary complete 'frames=2800 registered=312 objects=120000 objects_drawn=90000 hidden=9000 culled=20000 meshes=400000 meshes_drawn=380000 skinned=30000 batches=600000 translucent_batches=8000 vertices=9000000 triangles=70000000 texture_loads=80 texture_failures=0 team_colored=2000 unsupported_class=0 unsupported_state=90 chunk_overflow=0'
Add game_summary complete 'presents=3000 updates=3000 elapsed_ms=900000 game=1 logic_frames=2400 exit=game_ended driver=hardware'
Add ordinary_host_boundary complete 'ordinary game loop; GameLogic live; native D3D11 world objects and interactive input; execute/replay not entered'
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
    $r=Test-NativeOrdinaryPlayEvidence $Text 123 $receipt
    if($r.Valid -ne $Valid){throw "Unexpected classifier result for ${Name}: $($r.Errors -join '; ')"}
    $script:checks++
}
Verify $log $true 'baseline'
foreach($reason in 'game_quit','boundary','time_bound'){Verify ($log.Replace('exit=game_ended',"exit=$reason")) $true "reason $reason"}
foreach($reason in 'frame_bound','menu_exit','failure'){Verify ($log.Replace('exit=game_ended',"exit=$reason")) $false "reason $reason"}
Verify '' $false 'empty'
Verify ($log.Replace('game=1 logic_frames=2400','game=0 logic_frames=0')) $false 'no game'
Verify ($log.Replace('logic_frames=2400','logic_frames=100')) $false 'too few logic frames'
Verify ($log.Replace('players=3','players=1')) $false 'one player'
Verify ($log.Replace('d3d8=0','d3d8=1')) $false 'd3d8 loaded'
Verify ($log.Replace('Generals AMD64 ordinary play','Generals AMD64 ordinary terrain')) $false 'wrong mode'
Verify ($log.Replace('native D3D11 world objects and interactive input','terrain-only D3D11 world pass')) $false 'wrong boundary label'
# Terrain contracts retained.
Verify ($log.Replace('draws=16 atlas_uploads=1','draws=16 atlas_uploads=2')) $false 'second atlas upload at first frame'
Verify ($log.Replace('frames=2800 draws=54900','frames=3 draws=36')) $false 'too few terrain frames'
Verify ($log.Replace('draws=54900','draws=54899')) $false 'unaccounted draw'
Verify ($log.Replace('road_frames=2700 road_draws=5200','road_frames=0 road_draws=0').Replace('draws=54900','draws=49700')) $false 'roads missing on a map with roads'
Verify ($log.Replace('tile_updates=34 cloud','tile_updates=0 cloud')) $false 'no geometry updates'
Verify ($log.Replace('cloud_frames=2800','cloud_frames=0')) $false 'cloud pass missing'
Verify ($log.Replace('pos=0,416 size=800x184','pos=0,1216 size=800x184')) $false 'control bar parked off screen'
Verify ($log.Replace('strip_nonblack_pct=64','strip_nonblack_pct=0')) $false 'control bar art missing'
Verify ($log.Replace('nonblack_pct=62','nonblack_pct=0')) $false 'black frame'
Verify ($log.Replace('live_residencies=0','live_residencies=5')) $false 'residency leak'
# The scripted terrain exercise must not run under play.
Verify ($log + "`n99`t0`t0`t123`t456`tterrain_move`tobserved`t0`tstep=1 logic_frame=120 target=(1,1) origin=(0,0)") $false 'scripted camera present'
# World-object contracts.
Verify ($log.Replace('objects_drawn=41 batches=420','objects_drawn=0 batches=420')) $false 'nothing drawn in the first frame'
Verify ($log.Replace('registered=312 objects_drawn=41 batches=420 triangles=52000','registered=312 objects_drawn=41 batches=420 triangles=0')) $false 'zero triangles in the first frame'
Verify ($log.Replace('chunk_overflow=0','chunk_overflow=3')) $false 'chunk overflow'
Verify ($log.Replace('objects_drawn=90000 hidden=9000','objects_drawn=0 hidden=9000')) $false 'objects never drawn'
Verify ($log.Replace('frames=2800 registered=312','frames=3 registered=312')) $false 'too few object frames'
Verify ($log.Replace('texture_loads=80 texture_failures=0','texture_loads=0 texture_failures=0')) $false 'no object textures'
Verify ($log.Replace('world_objects_first_frame','world_objects_first_framX')) $false 'missing first-frame receipt'
Verify ($log.Replace('receipts=6 images=3','receipts=0 images=3')) $false 'no progress receipt'
foreach($stage in 'play_summary','world_objects_summary','world_objects_first_frame','terrain_first_frame','terrain_summary','game_summary','ui_residency_release'){
    Verify ($log.Replace("`t$stage`t","`t${stage}_x`t")) $false "missing $stage"
}
$logPlain=$log
Verify (($logPlain -split "`n" | Where-Object {$_ -notlike "*`tplay_progress`t*"}) -join "`n") $false 'sequence break'
$expected=@('-win','-xres','800','-yres','600','-nologo','-noshellmap','-winCursors','-nativeOrdinaryPlay','C:\run')
if(((Get-NativeOrdinaryPlayArguments 'C:\run') -join ' ') -ne ($expected -join ' ')){throw 'Wrong strict argument list'};$checks++
foreach($other in (Get-NativeOrdinaryMenuMode),(Get-NativeOrdinaryHostMode),(Get-NativeOrdinaryGameMode),(Get-NativeOrdinaryTerrainMode)){if((Get-NativeOrdinaryPlayMode) -eq $other){throw 'Modes must differ'}};$checks++
$rejected=$false
try{& (Join-Path $PSScriptRoot 'Invoke-NativeOrdinaryPlay.ps1') -Title Generals -Name unused | Out-Null}catch{$rejected=$_.Exception.Message -like '*requires explicit developer review*'}
if(!$rejected){throw 'Missing review switch did not fail closed'};$checks++
$rejected=$false
try{& (Join-Path $PSScriptRoot 'Test-NativeOrdinaryPlay.ps1') -Title ZeroHour -Name unused | Out-Null}catch{$rejected=$true}
if(!$rejected){throw 'Zero Hour must be rejected'};$checks++
Write-Host "PASS: $checks ordinary-play evidence/guard checks. No game launched."
