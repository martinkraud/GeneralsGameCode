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
$rejected=$false
try{& (Join-Path $PSScriptRoot 'Invoke-NativeGraphicsSmoke.ps1') -Title Generals -Name unused | Out-Null}catch{$rejected=$_.Exception.Message -like '*requires explicit developer review*'}
if(!$rejected){throw 'Missing review switch did not fail closed'};$checks++
foreach($title in 'Generals','ZeroHour'){
    $build=Get-NativeBuild $title
    $null=Assert-NativeGraphicsDependencies $build.Executable (Split-Path $build.Executable -Parent)
    $checks++
}
Write-Host "PASS: $checks graphics evidence/dependency/guard checks. No game launched."
