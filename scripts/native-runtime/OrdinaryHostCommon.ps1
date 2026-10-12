#Requires -Version 7.0
# Ordinary-host profile (X64.22) shared contract: stage identity and a pure
# evidence classifier. No function here creates a process.
. (Join-Path $PSScriptRoot 'GraphicsCommon.ps1')

function Get-NativeOrdinaryHostMode {
    return 'nativeOrdinaryHost; ordinary WinMain/ApplicationHWnd/W3DDisplay; headless world selection; hardware D3D11 native presentation; UI-only; no execute'
}
# Exact strict grammar (StartupDiagnostics::validOrdinaryHostArguments).
function Get-NativeOrdinaryHostArguments([string]$Root) {
    return @('-win','-xres','800','-yres','600','-nologo','-noshellmap','-winCursors','-nativeOrdinaryHost',$Root)
}
function Read-NativeOrdinaryHostStage([string]$Title,[string]$Name) {
    if($Title -ne 'Generals'){throw 'The ordinary host profile validates Generals only'}
    $manifest=Read-NativeStage $Title $Name
    $path=Join-Path $manifest.StagePath 'ordinaryhost.json'
    if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw 'Not an ordinary-host stage'}
    $host_=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if($host_.Mode -ne (Get-NativeOrdinaryHostMode) -or $host_.StagePath -ne $manifest.StagePath -or
        $host_.ManifestSHA256 -ne (Get-FileHash -LiteralPath (Join-Path $manifest.StagePath 'manifest.json')).Hash){
        throw 'Not an intact reviewed ordinary-host stage'
    }
    $manifest.Dependencies=@(Assert-NativeGraphicsDependencies (Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))) $manifest.GamePath)
    return $manifest
}

function Test-NativeOrdinaryHostEvidence([string]$Log,[int]$ProcessId,$Receipt) {
    $rows=@();$errors=[Collections.Generic.List[string]]::new()
    foreach($line in ($Log -split "`n")) {
        if(!$line){continue}; $f=$line.TrimEnd("`r") -split "`t",9
        if($f.Count -ne 9 -or $f[0] -notmatch '^\d+$' -or $f[3] -notmatch '^\d+$' -or $f[7] -notmatch '^\d+$'){$errors.Add('Malformed checkpoint/trailer');continue}
        $sequence=0;$rowPid=0;$code=[uint32]0
        if(![int]::TryParse($f[0],[ref]$sequence) -or ![int]::TryParse($f[3],[ref]$rowPid) -or ![uint32]::TryParse($f[7],[ref]$code)){$errors.Add('Numeric checkpoint overflow');continue}
        if($sequence -ne $rows.Count+1 -or $rowPid -ne $ProcessId){$errors.Add('Sequence/PID mismatch')}
        if($f[6] -eq 'fail'){$errors.Add("Failed phase $($f[5])")}
        $rows+=,[pscustomobject]@{Stage=$f[5];Status=$f[6];Code=$code;Detail=$f[8]}
    }
    function Completions([string]$Stage){@(for($i=0;$i -lt $rows.Count;$i++){if($rows[$i].Stage -eq $Stage -and $rows[$i].Status -eq 'complete'){$i}})}
    # One complete row each, in this strict order (host window precedes engine init;
    # display/device setup happens inside engine init; teardown inside cleanup).
    $route=@('app_window_class','app_window','display_config','graphics_device','display_module_audit',
        'display_size','display_ordinary_init','engine_init','ui_ordinary_content','display_frame_begin',
        'ui_ordinary_draw','display_frame_end','graphics_present','ordinary_host_summary',
        'ordinary_host_boundary','ui_residency_release','display_detach','engine_cleanup',
        'app_window_release','minidumper_shutdown','WinMain')
    $completed=@{};$previous=-1
    foreach($stage in $route){
        $at=@(Completions $stage)
        if($at.Count -ne 1){$errors.Add("Missing/duplicate completion $stage");$completed[$stage]=[pscustomobject]@{Detail='';Code=1};continue}
        $completed[$stage]=$rows[$at[0]]
        if($at[0] -le $previous -or $rows[$at[0]].Code -ne 0){$errors.Add("Order/code mismatch $stage")};$previous=$at[0]
    }
    foreach($key in 'module','userdata','dump_path','startup_parse'){
        $want=switch($key){module{$Receipt.Executable};userdata{$Receipt.UserData};dump_path{$Receipt.DumpPath+'\'};startup_parse{$Receipt.WorkingDirectory}}
        $found=@($rows|Where-Object{$_.Stage -eq $key -and $_.Status -ne 'begin'})
        if($found.Count -ne 1 -or $found[0].Detail -ne $want){$errors.Add("Path mismatch $key")}
    }
    $mode=@($rows|Where-Object{$_.Stage -eq 'WinMain' -and $_.Status -eq 'begin' -and $_.Detail -match 'Generals AMD64 ordinary host$'})
    if($mode.Count -ne 1){$errors.Add('Wrong mode identity')}
    if($completed['app_window'].Detail -ne 'client=800x600 windowed=1 class=Game Window'){$errors.Add('Wrong application window')}
    if($completed['display_config'].Detail -ne 'windowed=1 800x600x32 ApplicationHWnd'){$errors.Add('Wrong display configuration')}
    if($completed['display_size'].Detail -ne 'device=800x600 target=800x600 bits=32 windowed=1'){$errors.Add('Wrong display dimensions')}
    if($completed['graphics_device'].Detail -notmatch '^driver=hardware feature=0x(.*)'){$errors.Add('Missing hardware device identity')}
    if($completed['display_module_audit'].Detail -ne 'd3d8=0 d3dx8=0 d3d11=1 dxgi=1'){$errors.Add('D3D8/D3DX module audit failed or modern modules absent')}
    if($completed['graphics_present'].Detail -notmatch '^HRESULT=0x00000000 visible=1 occluded=0 removed=0x00000000 frame=\d+$'){$errors.Add('No unoccluded visible S_OK Present')}
    if($completed['ui_residency_release'].Detail -ne 'live_residencies=0 before backend release'){$errors.Add('UI residency not released before backend')}
    if($completed['ordinary_host_boundary'].Detail -ne 'execute/replay/simulation/GameLogic skipped'){$errors.Add('Wrong execution boundary')}
    if($completed['ui_ordinary_content'].Detail -notmatch '^file=.+ label=GUI:SelectDifficulty font=Arial size=14$'){$errors.Add('Missing original UI content identity')}
    if($completed['ui_ordinary_draw'].Detail -ne 'W3DDisplay fill/open rect, global-cache GeneralsLogo, original label, DisplayString caption -> Render2D -> D3D11'){$errors.Add('Wrong ordinary draw route')}
    $bypass=@($rows|Where-Object{$_.Stage -eq 'display_bypass' -and $_.Status -eq 'complete'})
    foreach($name in 'legacy_ww3d_init','legacy_render_device','legacy_scenes','legacy_shader_manager','legacy_debug_display_browser','legacy_gamma_lod'){
        if(@($bypass|Where-Object{$_.Detail -like "${name}:*"}).Count -ne 1){$errors.Add("Missing receipted bypass $name")}
    }
    $summary=$completed['ordinary_host_summary'].Detail
    if($summary -notmatch '^frames=(\d+) visible_frames=(\d+) occluded_frames=(\d+) elapsed_ms=(\d+) driver=hardware$'){$errors.Add('Invalid loop summary')}
    elseif($Matches[1].Length -gt 6 -or $Matches[2].Length -gt 6 -or $Matches[3].Length -gt 6 -or $Matches[4].Length -gt 6){$errors.Add('Summary numeric overflow')}
    elseif([int]$Matches[1] -lt 1 -or [int]$Matches[1] -gt 300 -or [int]$Matches[2] -lt 1 -or
        ([int]$Matches[2]+[int]$Matches[3]) -gt [int]$Matches[1] -or [int]$Matches[4] -lt 5000 -or [int]$Matches[4] -gt 6000){$errors.Add('Loop bounds/coverage invalid')}
    # Teardown strictly: residency, backend, engine cleanup, only then the window.
    $armed=@(for($i=0;$i -lt $rows.Count;$i++){if($rows[$i].Stage -eq 'native_shutdown' -and $rows[$i].Status -eq 'armed' -and $rows[$i].Code -eq 0){$i}})
    $mini=@(Completions 'minidumper_shutdown')
    if($armed.Count -ne 1 -or $mini.Count -ne 1 -or $armed[0] -le $mini[0] -or $armed[0] -ge $rows.Count-1){$errors.Add('Missing/incorrect late observer boundary')}
    if($rows.Count -eq 0 -or $rows[-1].Stage -ne 'WinMain' -or $rows[-1].Status -ne 'complete'){$errors.Add('Missing final WinMain completion')}
    # Rows that belong to other modes or to class C must be absent.
    foreach($stage in 'init_only','display_attach','graphics_class','graphics_window','graphics_backend','graphics_boundary','ui_menu_load','ui_image_load'){
        if(@($rows|Where-Object{$_.Stage -eq $stage}).Count){$errors.Add("Unexpected stage $stage")}
    }
    return [pscustomobject]@{Valid=($errors.Count -eq 0);Errors=@($errors);Records=$rows.Count;Summary=$summary}
}
