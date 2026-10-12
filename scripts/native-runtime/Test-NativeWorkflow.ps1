#Requires -Version 7.0
# Read-only / failure-path tests. Never creates or starts a game process.
. (Join-Path $PSScriptRoot 'Common.ps1')
$checks=0
function Reject([scriptblock]$Action,[string]$Reason) {
    $rejected=$false
    try { & $Action | Out-Null } catch { $rejected=$true }
    if (!$rejected) { throw "Expected rejection: $Reason" }
    $script:checks++
}
Reject { Get-NativeStage 'Generals' '../escape' } 'stage traversal'
Reject { Get-NativeStage 'ZeroHour' 'C:\escape' } 'absolute stage name'
Reject { Get-NativePE (Join-Path $script:ZHRepository 'build/win32/GeneralsMD/Release/generalszh.exe') } 'x86 executable'
Reject { & (Join-Path $PSScriptRoot 'Invoke-NativeInitOnly.ps1') -Title Generals -Name unused } 'missing launch review switch'
foreach ($title in @('Generals','ZeroHour')) {
    $build=Get-NativeBuild $title
    $pe=Get-NativePE $build.Executable
    if ($pe.Imports.Count -ne 30 -or $pe.HasDelayImports -or @($pe.Imports | Where-Object { $_ -like '*d3dx*' }).Count) {
        throw "Unexpected dependency baseline: $title"
    }
    $null=Assert-NativeDependencies $build.Executable (Split-Path $build.Executable -Parent)
    $checks++
}
$parseCount=0
Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.ps1' | ForEach-Object {
    $tokens=$null;$errors=$null
    $null=[Management.Automation.Language.Parser]::ParseFile($_.FullName,[ref]$tokens,[ref]$errors)
    if ($errors.Count) { throw ($errors -join '; ') }
    $parseCount++
}
Write-Host "PASS: $checks native preflight/guard checks; $parseCount PowerShell scripts parsed. No game launched."
