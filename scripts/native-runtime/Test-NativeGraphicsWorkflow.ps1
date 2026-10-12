#Requires -Version 7.0
# Synthetic receipts and guard rejection only. Never launches a title.
. (Join-Path $PSScriptRoot 'GraphicsCommon.ps1')
$receipt=[pscustomobject]@{Executable='C:\stage\game\generals.exe';UserData='C:\run\userdata\';DumpPath='C:\run\userdata\CrashDumps';WorkingDirectory='C:\stage\game';BaseAssetsPath=$null}
$lines=[Collections.Generic.List[string]]::new()
function Add([string]$Stage,[string]$Status='complete',[string]$Detail='',[uint32]$Code=0){
    $lines.Add("$($lines.Count+1)`t2026-10-12`t0`t123`t456`t$Stage`t$Status`t$Code`t$Detail")
}
Add module recorded $receipt.Executable
Add userdata recorded $receipt.UserData
Add dump_path recorded ($receipt.DumpPath+'\')
Add startup_parse recorded $receipt.WorkingDirectory
foreach($s in 'engine_init','graphics_class','graphics_window'){Add $s}
Add graphics_device complete 'driver=hardware feature=0xB000 vendor=0x1234 device=0x5678 adapter=test size=800x600'
foreach($s in 'graphics_window_association','graphics_target','graphics_setup','graphics_viewport','graphics_backend','graphics_clear'){Add $s}
Add graphics_present complete 'HRESULT=0x00000000 visible=1 occluded=0 removed=0x00000000 frame=1'
foreach($s in 'graphics_messages','graphics_backend_release','graphics_window_release','graphics_class_release'){Add $s}
Add graphics_summary complete 'frames=250 visible_frames=250 occluded_frames=0 elapsed_ms=5004 driver=hardware'
Add graphics_host
Add graphics_boundary complete 'execute/replay/simulation skipped'
Add engine_cleanup
Add minidumper_shutdown
Add native_shutdown armed
Add WinMain
$log=$lines -join "`n"
$checks=0
function Verify([string]$Text,[bool]$Valid){
    $r=Test-NativeGraphicsEvidence $Text 123 $receipt
    if($r.Valid -ne $Valid){throw "Unexpected classifier result: $($r.Errors -join '; ')"}
    $script:checks++
}
Verify $log $true
Verify '' $false
Verify ($log.Replace('frames=250','frames=999999999999999999999')) $false
Verify ($log.Replace("`t123`t","`t99999999999999999999`t")) $false
Verify ($log.Replace('driver=hardware','driver=WARP')) $false
Verify ($log.Replace('visible=1 occluded=0','visible=0 occluded=1')) $false
Verify ($log.Replace('elapsed_ms=5004','elapsed_ms=4999')) $false
Verify ($log.Replace('frames=250 visible_frames=250','frames=301 visible_frames=250')) $false
Verify ($log.Replace('graphics_backend_release','missing_backend_release')) $false
Verify ($log.Replace('native_shutdown','missing_observer')) $false
Verify ($log.Replace('execute/replay/simulation skipped','execute allowed')) $false
Verify ($log.Replace('C:\stage\game\generals.exe','C:\wrong\generals.exe')) $false
Verify ($log+"`nnative_shutdown`texception") $false
Verify ($log.Replace("`t123`t","`t999`t")) $false
Verify ($log.Replace("`tgraphics_clear`tcomplete`t0`t","`tgraphics_clear`tfail`t1`t")) $false
Verify ($log.Replace('graphics_boundary','init_only')) $false
# Distinct integrated-display evidence, using the actual shared classifier.
$displayLines=[Collections.Generic.List[string]]::new()
function DisplayAdd([string]$Stage,[string]$Status='complete',[string]$Detail=''){
    $displayLines.Add("$($displayLines.Count+1)`t2026-10-12`t0`t123`t456`t$Stage`t$Status`t0`t$Detail")
}
DisplayAdd WinMain begin 'Generals AMD64 display smoke'
foreach($line in $lines){
    $f=$line -split "`t",9
    if($f[5] -eq 'graphics_backend'){
        DisplayAdd display_size complete 'device=800x600 target=800x600 bits=32 windowed=1'
        DisplayAdd display_attach complete 'W3DDisplay -> WW3D active backend; presentation-only'
    }
    if($f[5] -eq 'graphics_clear'){DisplayAdd display_frame_begin complete 'WW3D::Begin_Render color-only'}
    if($f[5] -eq 'graphics_present'){DisplayAdd display_frame_end complete 'WW3D::End_Render Present'}
    if($f[5] -eq 'graphics_backend_release'){DisplayAdd display_detach complete 'device released; display dimensions restored'}
    DisplayAdd $f[5] $f[6] $f[8]
}
$displayLog=$displayLines -join "`n"
function VerifyDisplay([string]$Text,[bool]$Valid){
    $r=Test-NativeGraphicsEvidence $Text 123 $receipt -DisplayLifecycle
    if($r.Valid -ne $Valid){throw "Unexpected display classifier result: $($r.Errors -join '; ')"};$script:checks++
}
VerifyDisplay $displayLog $true
VerifyDisplay $log $false
Verify $displayLog $false
foreach($phase in 'display_attach','display_size','display_frame_begin','display_frame_end','display_detach'){
    VerifyDisplay ($displayLog.Replace($phase,'missing_'+$phase)) $false
}
VerifyDisplay ($displayLog.Replace('device=800x600','device=1024x768')) $false
VerifyDisplay ($displayLog.Replace('AMD64 display smoke','AMD64 graphics smoke')) $false
VerifyDisplay ($displayLog.Replace('display_frame_begin','temporary').Replace('display_frame_end','display_frame_begin').Replace('temporary','display_frame_end')) $false
if((Get-NativeGraphicsMode) -eq (Get-NativeGraphicsMode -DisplayLifecycle)){throw 'Modes must be distinct'};$checks++
# Run the actual launcher post-processing block with synthetic process outcomes.
# The original empty-directory expression emits zero pipeline objects, yielding
# $null under StrictMode; a single FileInfo also must remain a collection.
$launcher=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Invoke-NativeGraphicsSmoke.ps1') -Raw
$start=$launcher.IndexOf('$dumps=')
$end=$launcher.IndexOf('Write-Host "Receipts and flushed log:', $start)
if($start -lt 0 -or $end -lt 0){throw 'Post-processing block not found'}
$block=$launcher.Substring($start,$end-$start)
$postprocess=[scriptblock]::Create($block)
$legacyAssignment='$dumps=if(Test-Path -LiteralPath $receipt.DumpPath -PathType Container){@(Get-ChildItem -LiteralPath $receipt.DumpPath -Recurse -Force -File)}else{@()}'
$legacyPostprocess=[scriptblock]::Create($legacyAssignment+$block.Substring($block.IndexOf("`n")))
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('native-graphics-receipt-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
try {
    foreach($count in 0,1,2){
        foreach($processExit in 0,1){
            $run=Join-Path $fixture "$count-$processExit"
            $dumpPath=Join-Path $run 'CrashDumps'
            New-Item -ItemType Directory -Path $dumpPath | Out-Null
            for($i=0;$i -lt $count;$i++){[IO.File]::WriteAllText((Join-Path $dumpPath "$i.dmp"),'synthetic')}
            if($count -eq 0){
                $old=if(Test-Path -LiteralPath $dumpPath -PathType Container){@(Get-ChildItem -LiteralPath $dumpPath -Recurse -Force -File)}else{@()}
                if($null -ne $old){throw 'Did not reproduce zero-output null shape'}
                $threw=$false;try{$null=$old.Count}catch{$threw=$_.Exception.Message -like "*property 'Count'*"}
                if(!$threw){throw 'Did not reproduce original strict Count failure'}
            }
            $receipt=[pscustomobject]@{DumpPath=$dumpPath;ShutdownEvidencePath=(Join-Path $run 'shutdown-exception.bin')}
            $started=$true;$timeout=$false;$errorText=$null;$exit=$processExit;$pidValue=123
            $shutdownBytes=0;$pathsMatch=$true;$pathReceipts=@{}
            $evidence=[pscustomobject]@{Valid=$true};$log='synthetic completed checkpoints'
            if($count -eq 0){
                $threw=$false
                try{& $legacyPostprocess}catch{$threw=$_.Exception.Message -like "*property 'Count'*"}
                if(!$threw -or (Test-Path -LiteralPath (Join-Path $run 'exit.json'))){throw 'Legacy block did not fail before exit receipt'}
            }
            & $postprocess
            $written=Get-Content -LiteralPath (Join-Path $run 'exit.json') -Raw | ConvertFrom-Json
            if($written.CrashDumpCount -ne $count -or $written.ExitCode -ne $processExit -or
                $written.Success -ne ($count -eq 0 -and $processExit -eq 0)){
                throw "Wrong post-processing verdict for dumps=$count exit=$processExit"
            }
            $checks++
        }
    }
} finally {
    # Fresh test-owned absolute directory, never a runtime/run evidence directory.
    $resolved=[IO.Path]::GetFullPath($fixture)
    $tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if(!$resolved.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($resolved) -notmatch '^native-graphics-receipt-[a-f0-9]{32}$'){throw 'Unsafe fixture cleanup'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
$rejected=$false
try{& (Join-Path $PSScriptRoot 'Invoke-NativeGraphicsSmoke.ps1') -Title Generals -Name unused | Out-Null}catch{$rejected=$_.Exception.Message -like '*requires explicit developer review*'}
if(!$rejected){throw 'Missing review switch did not fail closed'};$checks++
foreach($title in 'Generals','ZeroHour'){
    $build=Get-NativeBuild $title
    $null=Assert-NativeGraphicsDependencies $build.Executable (Split-Path $build.Executable -Parent)
    $checks++
}
Write-Host "PASS: $checks graphics evidence/dependency/guard checks. No game launched."
