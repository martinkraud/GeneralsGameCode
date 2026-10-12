#Requires -Version 7.0
. (Join-Path $PSScriptRoot 'Common.ps1')
function Assert-NativeGraphicsDependencies([string]$Executable,[string]$GamePath) {
    $dependencies=@(Assert-NativeDependencies $Executable $GamePath)
    $pe=Get-NativePE $Executable
    foreach($dll in 'd3d11.dll','D3DCOMPILER_47.dll') {
        if($dll -notin $pe.Imports) { throw "Graphics code not linked: $dll" }
    }
    if(@($pe.Imports|Where-Object{$_ -match '^(d3d8|d3dx.*)\.dll$'}).Count){throw 'Unexpected legacy graphics imports'}
    foreach($dll in 'd3d11.dll','dxgi.dll','d3dcompiler_47.dll') {
        $path=Join-Path $env:WINDIR "System32/$dll"; $null=Get-NativePE $path
        if($dll -notin $pe.Imports){$dependencies+=[pscustomobject]@{Name=$dll;Route=$path;SHA256=(Get-FileHash -LiteralPath $path).Hash}}
    }
    return $dependencies
}
function Get-NativeGraphicsMode([switch]$DisplayLifecycle,[switch]$UIImage) {
    if($UIImage){return 'nativeUIImageSmoke; headless engine; hardware D3D11; W3DDisplay/WW3D; original menu image'}
    if($DisplayLifecycle){return 'nativeDisplaySmoke; headless engine; hardware D3D11; W3DDisplay/WW3D'}
    return 'nativeGraphicsSmoke; headless engine; hardware D3D11'
}
function Read-NativeGraphicsStage([string]$Title,[string]$Name,[switch]$DisplayLifecycle,[switch]$UIImage) {
    $manifest=Read-NativeStage $Title $Name
    $graphics=Get-Content -LiteralPath (Join-Path $manifest.StagePath 'graphics.json') -Raw | ConvertFrom-Json
    if($graphics.Mode -ne (Get-NativeGraphicsMode -DisplayLifecycle:$DisplayLifecycle -UIImage:$UIImage) -or
        $graphics.StagePath -ne $manifest.StagePath -or
        $graphics.ManifestSHA256 -ne (Get-FileHash -LiteralPath (Join-Path $manifest.StagePath 'manifest.json')).Hash){throw 'Not an intact reviewed graphics-smoke stage'}
    $manifest.Dependencies=@(Assert-NativeGraphicsDependencies (Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))) $manifest.GamePath)
    return $manifest
}
# Pure evidence classifier. No process creation; shared by launcher and synthetic negative tests.
function Test-NativeGraphicsEvidence([string]$Log,[int]$ProcessId,$Receipt,[switch]$DisplayLifecycle,[switch]$UIImage) {
    if($UIImage){$DisplayLifecycle=$true}
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
    $required=@('engine_init','graphics_class','graphics_window','graphics_device','graphics_window_association',
        'graphics_target','graphics_setup','graphics_viewport','graphics_backend','graphics_clear','graphics_present',
        'graphics_messages','graphics_backend_release','graphics_window_release','graphics_class_release',
        'graphics_summary','graphics_host','graphics_boundary','engine_cleanup','minidumper_shutdown','WinMain')
    $completed=@{};$last=-1
    foreach($stage in $required){$completed[$stage]=[pscustomobject]@{Detail=''}}
    foreach($stage in $required){
        $matches=@(for($i=0;$i -lt $rows.Count;$i++){if($rows[$i].Stage -eq $stage -and $rows[$i].Status -eq 'complete'){ $i }})
        if($matches.Count -ne 1){$errors.Add("Missing/duplicate completion $stage");continue}
        $index=$matches[0];$completed[$stage]=$rows[$index]
        if($index -le $last -or $rows[$index].Code -ne 0){$errors.Add("Order/code mismatch $stage")};$last=$index
    }
    foreach($key in 'module','userdata','dump_path','startup_parse','base_assets'){
        if($key -eq 'base_assets' -and !$Receipt.BaseAssetsPath){continue}
        $want=switch($key){module{$Receipt.Executable};userdata{$Receipt.UserData};dump_path{$Receipt.DumpPath+'\'};startup_parse{$Receipt.WorkingDirectory};base_assets{$Receipt.BaseAssetsPath}}
        $found=@($rows|Where-Object{$_.Stage -eq $key -and $_.Status -ne 'begin'})
        if($found.Count -ne 1 -or $found[0].Detail -ne $want){$errors.Add("Path mismatch $key")}
    }
    if($completed['graphics_device'].Detail -notmatch '^driver=hardware feature=0x(.*)'){$errors.Add('Missing hardware device identity')}
    if($completed['graphics_present'].Detail -notmatch '^HRESULT=0x00000000 visible=1 occluded=0 removed=0x00000000 frame=\d+$'){$errors.Add('No unoccluded visible S_OK Present')}
    if($completed['graphics_boundary'].Detail -ne 'execute/replay/simulation skipped'){$errors.Add('Wrong execution boundary')}
    $summary=$completed['graphics_summary'].Detail
    if($summary -notmatch '^frames=(\d+) visible_frames=(\d+) occluded_frames=(\d+) elapsed_ms=(\d+) driver=hardware$'){$errors.Add('Invalid loop summary')}
    elseif($Matches[1].Length -gt 6 -or $Matches[2].Length -gt 6 -or $Matches[3].Length -gt 6 -or $Matches[4].Length -gt 6){$errors.Add('Summary numeric overflow')}
    elseif([int]$Matches[1] -lt 1 -or [int]$Matches[1] -gt 300 -or [int]$Matches[2] -lt 1 -or
        ([int]$Matches[2]+[int]$Matches[3]) -gt [int]$Matches[1] -or [int]$Matches[4] -lt 5000 -or [int]$Matches[4] -gt 6000){$errors.Add('Loop bounds/coverage invalid')}
    $armed=@(for($i=0;$i -lt $rows.Count;$i++){if($rows[$i].Stage -eq 'native_shutdown' -and $rows[$i].Status -eq 'armed' -and $rows[$i].Code -eq 0){$i}})
    $mini=@(for($i=0;$i -lt $rows.Count;$i++){if($rows[$i].Stage -eq 'minidumper_shutdown' -and $rows[$i].Status -eq 'complete'){$i}})
    if($armed.Count -ne 1 -or $mini.Count -ne 1 -or $armed[0] -le $mini[0] -or $armed[0] -ge $rows.Count-1){$errors.Add('Missing/incorrect late observer boundary')}
    if($rows.Count -eq 0 -or $rows[-1].Stage -ne 'WinMain' -or $rows[-1].Status -ne 'complete'){$errors.Add('Missing final WinMain completion')}
    if(@($rows|Where-Object{$_.Stage -eq 'init_only'}).Count){$errors.Add('Wrong diagnostic mode')}
    $displayRows=@($rows|Where-Object{$_.Stage -like 'display_*'})
    if($DisplayLifecycle){
        $modePattern=if($UIImage){'AMD64 UI image smoke$'}else{'AMD64 display smoke$'}
        # Device setup occurs inside attach; frame and detach checkpoints are
        # interleaved with host clear/Present/release. Verify the actual route.
        $route=@('engine_init','graphics_window','graphics_device','graphics_viewport',
            'display_size','display_attach','display_frame_begin','graphics_clear',
            'display_frame_end','graphics_present','display_detach','graphics_backend_release',
            'graphics_window_release','graphics_boundary','engine_cleanup')
        $previous=-1
        foreach($stage in $route){
            $at=@(for($i=0;$i -lt $rows.Count;$i++){if($rows[$i].Stage -eq $stage -and $rows[$i].Status -eq 'complete'){$i}})
            if($at.Count -ne 1){$errors.Add("Missing/duplicate display completion $stage");continue}
            if($at[0] -le $previous -or $rows[$at[0]].Code -ne 0){$errors.Add("Display route order/code $stage")};$previous=$at[0]
        }
        $mode=@($rows|Where-Object{$_.Stage -eq 'WinMain' -and $_.Status -eq 'begin' -and $_.Detail -match $modePattern})
        if($mode.Count -ne 1){$errors.Add('Wrong display mode')}
        $size=@($rows|Where-Object{$_.Stage -eq 'display_size' -and $_.Detail -eq 'device=800x600 target=800x600 bits=32 windowed=1'})
        if($size.Count -ne 1){$errors.Add('Wrong display dimensions')}
    } elseif($displayRows.Count){$errors.Add('Unexpected display lifecycle in direct-backend mode')}
    $imageRows=@($rows|Where-Object{$_.Stage -like 'ui_image_*'})
    if($UIImage){
        $route=@('display_attach','ui_image_load','display_frame_begin','ui_image_draw',
            'display_frame_end','graphics_present','ui_image_release','display_detach')
        $previous=-1
        foreach($stage in $route){
            $at=@(for($i=0;$i -lt $rows.Count;$i++){if($rows[$i].Stage -eq $stage -and $rows[$i].Status -eq 'complete'){$i}})
            if($at.Count -ne 1){$errors.Add("Missing/duplicate UI completion $stage");continue}
            if($at[0] -le $previous -or $rows[$at[0]].Code -ne 0){$errors.Add("UI route order/code $stage")};$previous=$at[0]
        }
        if(@($imageRows|Where-Object{$_.Stage -eq 'ui_image_load' -and $_.Status -eq 'complete' -and $_.Detail -match '^name=GeneralsLogo file=.+ atlas=\d+x\d+ uv=.+ rect=504,16,791,110$'}).Count -ne 1){$errors.Add('Missing original menu image identity')}
    } elseif($imageRows.Count){$errors.Add('Unexpected UI image mode')}
    return [pscustomobject]@{Valid=($errors.Count -eq 0);Errors=@($errors);Records=$rows.Count;Summary=$summary}
}
