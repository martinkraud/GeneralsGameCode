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

# ---------------------------------------------------------------------------
# Ordinary-menu profile (X64.23): original interactive MainMenu, bounded
# menu-only loop. Same strict grammar with a distinct mode token.
# ---------------------------------------------------------------------------
function Get-NativeOrdinaryMenuMode {
    return 'nativeOrdinaryMenu; ordinary WinMain/ApplicationHWnd/W3DDisplay + real WindowManager/Shell/mouse/keyboard; headless world selection; hardware D3D11 native presentation; bounded menu-only loop; no GameLogic/execute'
}
function Get-NativeOrdinaryMenuArguments([string]$Root) {
    return @('-win','-xres','800','-yres','600','-nologo','-noshellmap','-winCursors','-nativeOrdinaryMenu',$Root)
}
function Read-NativeOrdinaryMenuStage([string]$Title,[string]$Name) {
    if($Title -ne 'Generals'){throw 'The ordinary menu profile validates Generals only'}
    $manifest=Read-NativeStage $Title $Name
    $path=Join-Path $manifest.StagePath 'ordinarymenu.json'
    if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw 'Not an ordinary-menu stage'}
    $menu=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if($menu.Mode -ne (Get-NativeOrdinaryMenuMode) -or $menu.StagePath -ne $manifest.StagePath -or
        $menu.ManifestSHA256 -ne (Get-FileHash -LiteralPath (Join-Path $manifest.StagePath 'manifest.json')).Hash){
        throw 'Not an intact reviewed ordinary-menu stage'
    }
    $manifest.Dependencies=@(Assert-NativeGraphicsDependencies (Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))) $manifest.GamePath)
    return $manifest
}

function Test-NativeOrdinaryMenuEvidence([string]$Log,[int]$ProcessId,$Receipt) {
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
    $route=@('app_window_class','app_window','TheKeyboard.init','display_config','graphics_device','display_module_audit',
        'display_size','display_ordinary_init','TheWindowManager.init','TheIMEManager.init','TheShell.init',
        'TheMouse.init','engine_init','menu_shell','graphics_present','menu_summary','ordinary_host_boundary',
        'ui_residency_release','display_detach','engine_cleanup','app_window_release','minidumper_shutdown','WinMain')
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
    if(@($rows|Where-Object{$_.Stage -eq 'WinMain' -and $_.Status -eq 'begin' -and $_.Detail -match 'Generals AMD64 ordinary menu$'}).Count -ne 1){$errors.Add('Wrong mode identity')}
    if($completed['app_window'].Detail -ne 'client=800x600 windowed=1 class=Game Window'){$errors.Add('Wrong application window')}
    if($completed['display_config'].Detail -ne 'windowed=1 800x600x32 ApplicationHWnd'){$errors.Add('Wrong display configuration')}
    if($completed['display_size'].Detail -ne 'device=800x600 target=800x600 bits=32 windowed=1'){$errors.Add('Wrong display dimensions')}
    if($completed['graphics_device'].Detail -notmatch '^driver=hardware feature=0x(.*)'){$errors.Add('Missing hardware device identity')}
    if($completed['display_module_audit'].Detail -ne 'd3d8=0 d3dx8=0 d3d11=1 dxgi=1'){$errors.Add('D3D8/D3DX module audit failed or modern modules absent')}
    if($completed['graphics_present'].Detail -notmatch '^HRESULT=0x00000000 visible=1 occluded=0 removed=0x00000000 frame=\d+$'){$errors.Add('No unoccluded visible S_OK first Present')}
    if($completed['ui_residency_release'].Detail -ne 'live_residencies=0 before backend release'){$errors.Add('UI residency not released before backend')}
    if($completed['ordinary_host_boundary'].Detail -ne 'execute/replay/simulation/GameLogic skipped'){$errors.Add('Wrong execution boundary')}
    if($completed['menu_shell'].Detail -notmatch '^top=Menus/MainMenu\.wnd screens=1 winCursors=1 '){$errors.Add('Original MainMenu was not the only presented screen with Windows cursors')}
    $summary=$completed['menu_summary'].Detail
    if($summary -notmatch '^frames=(\d+) visible_frames=(\d+) occluded_frames=(\d+) elapsed_ms=(\d+) moves=(\d+) down=(\d+) up=(\d+) keys=(\d+) exit=(menu_exit|window_close|boundary|idle_bound|time_bound) driver=hardware$'){$errors.Add('Invalid menu summary')}
    else{
        $m=$Matches
        $overflow=$false
        foreach($i in 1..8){if($m[$i].Length -gt 9){$overflow=$true}}
        if($overflow){$errors.Add('Summary numeric overflow')}
        else{
            if([int]$m[1] -lt 1 -or [int]$m[2] -lt 1 -or ([int]$m[2]+[int]$m[3]) -gt [int]$m[1]){$errors.Add('Frame coverage invalid')}
            if([int]$m[5] -lt 1 -or [int]$m[6] -lt 1 -or [int]$m[7] -lt 1){$errors.Add('No proven mouse movement and click interaction')}
            if([int]$m[4] -gt 700000){$errors.Add('Menu loop exceeded its bound')}
        }
    }
    $armed=@(for($i=0;$i -lt $rows.Count;$i++){if($rows[$i].Stage -eq 'native_shutdown' -and $rows[$i].Status -eq 'armed' -and $rows[$i].Code -eq 0){$i}})
    $mini=@(Completions 'minidumper_shutdown')
    if($armed.Count -ne 1 -or $mini.Count -ne 1 -or $armed[0] -le $mini[0] -or $armed[0] -ge $rows.Count-1){$errors.Add('Missing/incorrect late observer boundary')}
    if($rows.Count -eq 0 -or $rows[-1].Stage -ne 'WinMain' -or $rows[-1].Status -ne 'complete'){$errors.Add('Missing final WinMain completion')}
    foreach($stage in 'init_only','display_attach','graphics_class','graphics_window','graphics_backend','graphics_boundary','ui_menu_load','ui_ordinary_content','keyboard'){
        if(@($rows|Where-Object{$_.Stage -eq $stage}).Count){$errors.Add("Unexpected stage $stage")}
    }
    return [pscustomobject]@{Valid=($errors.Count -eq 0);Errors=@($errors);Records=$rows.Count;Summary=$summary}
}

# ---------------------------------------------------------------------------
# Ordinary-game profile (X64.24): original MainMenu -> Skirmish -> map load ->
# live GameLogic on the ordinary engine loop; UI-only presentation (no world).
# ---------------------------------------------------------------------------
function Get-NativeOrdinaryGameMode {
    return 'nativeOrdinaryGame; ordinary WinMain/ApplicationHWnd/W3DDisplay + real WindowManager/Shell; ordinary engine loop with live GameLogic; headless world selection; hardware D3D11 native presentation; UI-only draw (no world rendering)'
}
function Get-NativeOrdinaryGameArguments([string]$Root) {
    return @('-win','-xres','800','-yres','600','-nologo','-noshellmap','-winCursors','-nativeOrdinaryGame',$Root)
}
function Read-NativeOrdinaryGameStage([string]$Title,[string]$Name) {
    if($Title -ne 'Generals'){throw 'The ordinary game profile validates Generals only'}
    $manifest=Read-NativeStage $Title $Name
    $path=Join-Path $manifest.StagePath 'ordinarygame.json'
    if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw 'Not an ordinary-game stage'}
    $game=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if($game.Mode -ne (Get-NativeOrdinaryGameMode) -or $game.StagePath -ne $manifest.StagePath -or
        $game.ManifestSHA256 -ne (Get-FileHash -LiteralPath (Join-Path $manifest.StagePath 'manifest.json')).Hash){
        throw 'Not an intact reviewed ordinary-game stage'
    }
    $manifest.Dependencies=@(Assert-NativeGraphicsDependencies (Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))) $manifest.GamePath)
    return $manifest
}

function Test-NativeOrdinaryGameEvidence([string]$Log,[int]$ProcessId,$Receipt) {
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
    # The in-process log is capped at 2048 records and fails closed; a run that fills it
    # lost its tail evidence (X64.24 run 20261012T2112467768379Z: unthrottled FP receipts).
    if($rows.Count -ge 2000){$errors.Add('Startup log capacity exhausted; tail evidence lost')}
    $route=@('app_window_class','app_window','TheKeyboard.init','display_config','graphics_device','display_module_audit',
        'display_size','display_ordinary_init','TheWindowManager.init','TheShell.init','TheMouse.init','engine_init',
        'graphics_present','game_start','game_summary','ordinary_host_boundary','ui_residency_release',
        'display_detach','engine_cleanup','app_window_release','minidumper_shutdown','WinMain')
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
    if(@($rows|Where-Object{$_.Stage -eq 'WinMain' -and $_.Status -eq 'begin' -and $_.Detail -match 'Generals AMD64 ordinary game$'}).Count -ne 1){$errors.Add('Wrong mode identity')}
    if($completed['app_window'].Detail -ne 'client=800x600 windowed=1 class=Game Window'){$errors.Add('Wrong application window')}
    if($completed['display_size'].Detail -ne 'device=800x600 target=800x600 bits=32 windowed=1'){$errors.Add('Wrong display dimensions')}
    if($completed['graphics_device'].Detail -notmatch '^driver=hardware feature=0x(.*)'){$errors.Add('Missing hardware device identity')}
    if($completed['display_module_audit'].Detail -ne 'd3d8=0 d3dx8=0 d3d11=1 dxgi=1'){$errors.Add('D3D8/D3DX module audit failed or modern modules absent')}
    if($completed['ui_residency_release'].Detail -ne 'live_residencies=0 before backend release'){$errors.Add('UI residency not released before backend')}
    if($completed['ordinary_host_boundary'].Detail -ne 'ordinary game loop; GameLogic live; no world rendering; execute/replay not entered'){$errors.Add('Wrong execution boundary label')}
    if($completed['game_start'].Detail -notmatch '^mode=\d+ map=\S.* players=([2-9]|\d\d) frame=\d+$'){$errors.Add('Game did not start with at least two players')}
    $progress=@($rows|Where-Object{$_.Stage -eq 'game_progress' -and $_.Status -eq 'observed' -and $_.Detail -match '^logic_frame=\d+ crc=[0-9A-F]{8} presents=\d+$'})
    if($progress.Count -lt 3){$errors.Add('Fewer than three logic progress observations')}
    $summary=$completed['game_summary'].Detail
    if($summary -notmatch '^presents=(\d+) updates=(\d+) elapsed_ms=(\d+) game=(\d) logic_frames=(\d+) exit=(frame_bound|game_quit) driver=hardware$'){$errors.Add('Invalid game summary or exit reason')}
    else{
        $m=$Matches
        $overflow=$false
        foreach($i in 1..5){if($m[$i].Length -gt 9){$overflow=$true}}
        if($overflow){$errors.Add('Summary numeric overflow')}
        else{
            if([int]$m[1] -lt 1 -or [int]$m[2] -lt 1){$errors.Add('No presented frames')}
            if([int]$m[4] -ne 1 -or [int]$m[5] -lt 150){$errors.Add('Fewer than 150 logic frames in game')}
            if([int]$m[3] -gt 960000){$errors.Add('Game loop exceeded its bound')}
        }
    }
    $armed=@(for($i=0;$i -lt $rows.Count;$i++){if($rows[$i].Stage -eq 'native_shutdown' -and $rows[$i].Status -eq 'armed' -and $rows[$i].Code -eq 0){$i}})
    $mini=@(Completions 'minidumper_shutdown')
    if($armed.Count -ne 1 -or $mini.Count -ne 1 -or $armed[0] -le $mini[0] -or $armed[0] -ge $rows.Count-1){$errors.Add('Missing/incorrect late observer boundary')}
    if($rows.Count -eq 0 -or $rows[-1].Stage -ne 'WinMain' -or $rows[-1].Status -ne 'complete'){$errors.Add('Missing final WinMain completion')}
    foreach($stage in 'init_only','display_attach','graphics_class','graphics_window','graphics_backend','graphics_boundary','ui_menu_load','ui_ordinary_content','keyboard','menu_summary'){
        if(@($rows|Where-Object{$_.Stage -eq $stage}).Count){$errors.Add("Unexpected stage $stage")}
    }
    if(@($rows|Where-Object{$_.Stage -eq 'menu_boundary'}).Count){$errors.Add('Shell push boundary reached')}
    return [pscustomobject]@{Valid=($errors.Count -eq 0);Errors=@($errors);Records=$rows.Count;Summary=$summary}
}

# ---------------------------------------------------------------------------
# Ordinary-terrain profile (X64.25): the X64.24 game path plus the first native
# D3D11 terrain world frame (terrain only, under the original UI).
# ---------------------------------------------------------------------------
function Get-NativeOrdinaryTerrainMode {
    return 'nativeOrdinaryTerrain; ordinary WinMain/ApplicationHWnd/W3DDisplay + real WindowManager/Shell; ordinary engine loop with live GameLogic; headless world selection; hardware D3D11 native presentation; UI-only draw (no world rendering)'
}
function Get-NativeOrdinaryTerrainArguments([string]$Root) {
    return @('-win','-xres','800','-yres','600','-nologo','-noshellmap','-winCursors','-nativeOrdinaryTerrain',$Root)
}
function Read-NativeOrdinaryTerrainStage([string]$Title,[string]$Name) {
    if($Title -ne 'Generals'){throw 'The ordinary terrain profile validates Generals only'}
    $manifest=Read-NativeStage $Title $Name
    $path=Join-Path $manifest.StagePath 'ordinaryterrain.json'
    if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw 'Not an ordinary-terrain stage'}
    $terrain=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if($terrain.Mode -ne (Get-NativeOrdinaryTerrainMode) -or $terrain.StagePath -ne $manifest.StagePath -or
        $terrain.ManifestSHA256 -ne (Get-FileHash -LiteralPath (Join-Path $manifest.StagePath 'manifest.json')).Hash){
        throw 'Not an intact reviewed ordinary-terrain stage'
    }
    $manifest.Dependencies=@(Assert-NativeGraphicsDependencies (Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))) $manifest.GamePath)
    return $manifest
}

function Test-NativeOrdinaryTerrainEvidence([string]$Log,[int]$ProcessId,$Receipt) {
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
    # The in-process log is capped at 2048 records and fails closed; a run that fills it
    # lost its tail evidence (X64.25 run 20261012T2112467768379Z: unthrottled FP receipts).
    if($rows.Count -ge 2000){$errors.Add('Startup log capacity exhausted; tail evidence lost')}
    $route=@('app_window_class','app_window','TheKeyboard.init','display_config','terrain_optin','graphics_device','display_module_audit',
        'display_size','display_ordinary_init','TheWindowManager.init','TheShell.init','TheMouse.init','engine_init',
        'graphics_present','game_start','terrain_first_frame','terrain_summary','game_summary','ordinary_host_boundary','ui_residency_release',
        'display_detach','engine_cleanup','app_window_release','minidumper_shutdown','WinMain')
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
    if(@($rows|Where-Object{$_.Stage -eq 'WinMain' -and $_.Status -eq 'begin' -and $_.Detail -match 'Generals AMD64 ordinary terrain$'}).Count -ne 1){$errors.Add('Wrong mode identity')}
    if($completed['app_window'].Detail -ne 'client=800x600 windowed=1 class=Game Window'){$errors.Add('Wrong application window')}
    if($completed['display_size'].Detail -ne 'device=800x600 target=800x600 bits=32 windowed=1'){$errors.Add('Wrong display dimensions')}
    if($completed['graphics_device'].Detail -notmatch '^driver=hardware feature=0x(.*)'){$errors.Add('Missing hardware device identity')}
    if($completed['display_module_audit'].Detail -ne 'd3d8=0 d3dx8=0 d3d11=1 dxgi=1'){$errors.Add('D3D8/D3DX module audit failed or modern modules absent')}
    if($completed['ui_residency_release'].Detail -ne 'live_residencies=0 before backend release'){$errors.Add('UI residency not released before backend')}
    if($completed['ordinary_host_boundary'].Detail -ne 'ordinary game loop; GameLogic live; terrain-only D3D11 world pass; execute/replay not entered'){$errors.Add('Wrong execution boundary label')}
    if($completed['game_start'].Detail -notmatch '^mode=\d+ map=\S.* players=([2-9]|\d\d) frame=\d+$'){$errors.Add('Game did not start with at least two players')}
    $progress=@($rows|Where-Object{$_.Stage -eq 'game_progress' -and $_.Status -eq 'observed' -and $_.Detail -match '^logic_frame=\d+ crc=[0-9A-F]{8} presents=\d+$'})
    if($progress.Count -lt 3){$errors.Add('Fewer than three logic progress observations')}
    $summary=$completed['game_summary'].Detail
    if($summary -notmatch '^presents=(\d+) updates=(\d+) elapsed_ms=(\d+) game=(\d) logic_frames=(\d+) exit=(frame_bound|game_quit) driver=hardware$'){$errors.Add('Invalid game summary or exit reason')}
    else{
        $m=$Matches
        $overflow=$false
        foreach($i in 1..5){if($m[$i].Length -gt 9){$overflow=$true}}
        if($overflow){$errors.Add('Summary numeric overflow')}
        else{
            if([int]$m[1] -lt 1 -or [int]$m[2] -lt 1){$errors.Add('No presented frames')}
            if([int]$m[4] -ne 1 -or [int]$m[5] -lt 150){$errors.Add('Fewer than 150 logic frames in game')}
            if([int]$m[3] -gt 960000){$errors.Add('Game loop exceeded its bound')}
        }
    }
    if($completed['terrain_optin'].Detail -notmatch '^cpu_backed_geometry=1 depth_buffer=1 '){$errors.Add('Terrain opt-ins not recorded')}
    $tileCount=0;$useCloud=0;$useNoise=0;$softEdge=0;$mapExtra=0;$mapShore=0;$mapRoads=0;$mapBridges=0
    if($completed['terrain_first_frame'].Detail -notmatch '^tiles=(\d+) draws=(\d+) atlas_uploads=1 use_cloud=([01]) use_noise=([01]) soft_edge=([01]) map_extra=(\d+) map_shore=(\d+) map_roads=(\d+) map_bridges=(\d+) \(terrain combined single pass \+ overlays; UI follows\)$'){$errors.Add('Missing first terrain frame with one atlas upload')}
    else{
        $m=$Matches
        $over=$false;foreach($i in 1..9){if($m[$i].Length -gt 6){$over=$true}}
        if($over){$errors.Add('Terrain numeric overflow')}
        elseif([int]$m[1] -lt 1 -or [int]$m[2] -lt [int]$m[1]){$errors.Add('First terrain frame did not draw every uploaded tile')}
        else{$tileCount=[int]$m[1];$useCloud=[int]$m[3];$useNoise=[int]$m[4];$softEdge=[int]$m[5]
             $mapExtra=[int]$m[6];$mapShore=[int]$m[7];$mapRoads=[int]$m[8];$mapBridges=[int]$m[9]}
    }
    $probe=@($rows|Where-Object{$_.Stage -eq 'terrain_probe' -and $_.Status -eq 'observed'})
    if($probe.Count -ne 1 -or $probe[0].Detail -notmatch '^nonblack_pct=(\d{1,3}) center_rgb='){$errors.Add('Missing terrain pixel probe')}
    elseif([int]$Matches[1] -lt 5){$errors.Add('Terrain pass left the frame black (probe)')}
    if($completed['terrain_summary'].Detail -notmatch '^frames=(\d+) draws=(\d+) atlas_uploads=(\d+) tile_uploads=(\d+) tile_updates=(\d+) cloud_frames=(\d+) noise_frames=(\d+) macro_loads=(\d+) extra_frames=(\d+) extra_tiles=(\d+) road_frames=(\d+) road_draws=(\d+) bridge_frames=(\d+) bridge_draws=(\d+) shore_frames=(\d+) shore_tiles=(\d+) texture_loads=(\d+)$'){$errors.Add('Invalid terrain summary')}
    else{
        $m=$Matches
        $over=$false;foreach($i in 1..17){if($m[$i].Length -gt 9){$over=$true}}
        if($over){$errors.Add('Terrain summary numeric overflow')}
        else{
            $frames=[int]$m[1]
            if($frames -lt 30 -or [int]$m[3] -lt 1 -or [int]$m[4] -lt 1){$errors.Add('Too few terrain frames or no residency uploads')}
            # Every draw is accounted for: one combined draw per tile per frame plus each overlay family.
            $expected=$frames*$tileCount+[int]$m[9]+[int]$m[12]+[int]$m[14]+[int]$m[15]
            if($tileCount -gt 0 -and [int]$m[2] -ne $expected){$errors.Add('Terrain draws differ from frames x tiles + overlay draws')}
            # X64.26 contracts (retained): dynamic terrain really rewritten, macro passes ran.
            if([int]$m[5] -lt 1){$errors.Add('No terrain geometry updates (tile_updates=0)')}
            if($useCloud -eq 1 -and [int]$m[6] -ne $frames){$errors.Add('Cloud pass did not run every terrain frame')}
            if($useCloud -eq 0 -and [int]$m[6] -ne 0){$errors.Add('Cloud pass ran while disabled')}
            if($useNoise -eq 1 -and [int]$m[7] -ne $frames){$errors.Add('Noise pass did not run every terrain frame')}
            if($useNoise -eq 0 -and [int]$m[7] -ne 0){$errors.Add('Noise pass ran while disabled')}
            if([int]$m[8] -ne ($useCloud+$useNoise)){$errors.Add('Unexpected macro texture load count')}
            # X64.27: overlay families must appear exactly when the loaded map contains them.
            if($mapExtra -gt 0 -and [int]$m[9] -lt 1){$errors.Add('Map has extra-blend tiles but none were drawn')}
            if($mapExtra -eq 0 -and [int]$m[9] -ne 0){$errors.Add('Extra-blend draws on a map without extra-blend tiles')}
            if($mapRoads -gt 0 -and [int]$m[11] -lt 1){$errors.Add('Map has road segments but no road draw was observed')}
            if($mapRoads -eq 0 -and [int]$m[12] -ne 0){$errors.Add('Road draws on a map without roads')}
            if($mapBridges -gt 0 -and [int]$m[13] -lt 1){$errors.Add('Map has bridges but no bridge draw was observed')}
            if($mapBridges -eq 0 -and [int]$m[14] -ne 0){$errors.Add('Bridge draws on a map without bridges')}
            if($mapShore -gt 0 -and $softEdge -eq 1 -and [int]$m[15] -lt 1){$errors.Add('Map has shoreline tiles and soft water edge is enabled but no shoreline pass ran')}
            if(($mapShore -eq 0 -or $softEdge -eq 0) -and [int]$m[15] -ne 0){$errors.Add('Shoreline pass ran without shoreline tiles or soft water edge')}
            # Decoded road/bridge textures: at least one per drawn family, none when no family drew.
            if(([int]$m[12]+[int]$m[14]) -gt 0 -and [int]$m[17] -lt 1){$errors.Add('Road/bridge draws without a loaded world texture')}
            if(([int]$m[12]+[int]$m[14]) -eq 0 -and [int]$m[17] -ne 0){$errors.Add('World textures loaded without road/bridge draws')}
        }
    }
    # Scripted camera exercise: six moves, six probes, all frames non-black, geometry updated, pivot moved.
    $moves=@($rows|Where-Object{$_.Stage -eq 'terrain_move' -and $_.Status -eq 'observed'})
    $motions=@($rows|Where-Object{$_.Stage -eq 'terrain_motion' -and $_.Status -eq 'observed'})
    if($moves.Count -ne 6){$errors.Add('Camera exercise did not issue exactly six moves')}
    if($motions.Count -ne 6){$errors.Add('Camera exercise did not record exactly six motion probes')}
    else{
        $updates=@();$pivots=@{}
        foreach($row in $motions){
            if($row.Detail -notmatch '^step=(\d+) pivot=\((-?\d+),(-?\d+)\) nonblack_pct=(\d{1,3}) tile_updates=(\d+) tile_uploads=(\d+) cloud_frames=(\d+) noise_frames=(\d+) cam='){$errors.Add('Malformed motion probe');continue}
            if([int]$Matches[4] -lt 5){$errors.Add("Motion step $($Matches[1]) left the frame black")}
            $updates+=[int]$Matches[5];$pivots["$($Matches[2]),$($Matches[3])"]=1
        }
        $increases=0;for($i=1;$i -lt $updates.Count;$i++){if($updates[$i] -gt $updates[$i-1]){$increases++}}
        if($updates.Count -eq 6 -and ($increases -lt 3 -or $updates[0] -lt 1)){$errors.Add('Terrain updates did not increase across the camera steps')}
        if($pivots.Count -lt 4){$errors.Add('Camera pivot did not move to enough distinct positions')}
    }
    # Control Bar visibility/state (X64.27): the bar must have slid into the 600-pixel display and
    # painted opaque art into the bottom strip (radar bezel may stay empty: RadarDummy).
    $ui=@($rows|Where-Object{$_.Stage -eq 'ui_probe' -and $_.Status -eq 'observed'})
    if($ui.Count -ne 1 -or $ui[0].Detail -notmatch '^whole_nonblack_pct=(\d+) strip_nonblack_pct=(\d+) strip_left=(\d+) strip_mid=(\d+) strip_right=(\d+)$'){$errors.Add('Missing Control Bar/UI probe receipt')}
    else{
        if([int]$Matches[2] -lt 25){$errors.Add('Control Bar art did not paint into the bottom strip')}
        if([int]$Matches[1] -lt 80){$errors.Add('Composed frame lost world pixels after the UI')}
    }
    $barRows=@($rows|Where-Object{$_.Stage -eq 'ui_probe_window' -and $_.Detail -like 'ControlBar.wnd:ControlBarParent *'})
    if($barRows.Count -ne 1 -or $barRows[0].Detail -notmatch 'hidden=0 pos=(-?\d+),(-?\d+) size=(\d+)x(\d+)$'){$errors.Add('Missing Control Bar window state')}
    elseif([int]$Matches[2] -lt 400 -or [int]$Matches[2] -ge 600){$errors.Add('Control Bar is not slid into the display')}
    $armed=@(for($i=0;$i -lt $rows.Count;$i++){if($rows[$i].Stage -eq 'native_shutdown' -and $rows[$i].Status -eq 'armed' -and $rows[$i].Code -eq 0){$i}})
    $mini=@(Completions 'minidumper_shutdown')
    if($armed.Count -ne 1 -or $mini.Count -ne 1 -or $armed[0] -le $mini[0] -or $armed[0] -ge $rows.Count-1){$errors.Add('Missing/incorrect late observer boundary')}
    if($rows.Count -eq 0 -or $rows[-1].Stage -ne 'WinMain' -or $rows[-1].Status -ne 'complete'){$errors.Add('Missing final WinMain completion')}
    foreach($stage in 'init_only','display_attach','graphics_class','graphics_window','graphics_backend','graphics_boundary','ui_menu_load','ui_ordinary_content','keyboard','menu_summary'){
        if(@($rows|Where-Object{$_.Stage -eq $stage}).Count){$errors.Add("Unexpected stage $stage")}
    }
    if(@($rows|Where-Object{$_.Stage -eq 'menu_boundary'}).Count){$errors.Add('Shell push boundary reached')}
    return [pscustomobject]@{Valid=($errors.Count -eq 0);Errors=@($errors);Records=$rows.Count;Summary=$summary}
}
