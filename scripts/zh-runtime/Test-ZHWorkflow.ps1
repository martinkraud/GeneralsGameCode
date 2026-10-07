#Requires -Version 7.0
# Non-destructive infrastructure checks. No process launch, registry write or real user-data backup.
[CmdletBinding()]
param()
. "$PSScriptRoot/Common.ps1"
$testId = [Guid]::NewGuid().ToString('N').Substring(0, 12)
$testRoot = Join-Path $script:ZHRepository "build/workflow-tests/$testId"
Assert-ZHNoLinks $testRoot
New-Item -ItemType Directory -Path $testRoot | Out-Null
$testName = "workflow-check-$testId"
$checks = 0
function Assert-Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAILED: $Message" }
    $script:checks++; Write-Host "PASS: $Message"
}
function Assert-Throws([scriptblock]$Action, [string]$Message) {
    $threw = $false
    try { & $Action } catch { $threw = $true; Write-Host "Expected refusal: $($_.Exception.Message)" }
    Assert-Check $threw $Message
}
foreach ($name in @('..', '.', 'CON', 'baseline/child', 'C:\Windows', 'bad name')) {
    Assert-Throws { Get-ZHStage $name } "unsafe name refused: $name"
}
Assert-Check (-not (Test-ZHWithin 'C:\test-other' 'C:\test')) 'path containment enforces separator boundary'
Assert-Throws { Get-ZHSource 'C:\Windows' } 'non-installation source refused'
foreach ($argument in @('-setCwd', '-SETCWD=C:\elsewhere', '-useCwd', "-useCwd`n")) {
    Assert-Throws { Assert-ZHGameArguments @($argument) } "CWD override refused: $argument"
}
Assert-ZHGameArguments @('-win', '-fps', '30', '-xres', '5120', '-yres', '1440')
Assert-Check $true 'ordinary optional arguments accepted'
Assert-Check (@(Get-ZHInventoryDifferences @() @()).Count -eq 0) 'empty backup inventory comparison works'
$fakeData = [pscustomobject]@{Path=(Join-Path $testRoot 'synthetic-user-data'); Resolution='synthetic fixture'; User='synthetic-test'}
$absentBackup = Invoke-ZHUserDataBackup $fakeData
Assert-Check ($absentBackup.Verified -and -not $absentBackup.SourceExisted -and -not (Test-Path -LiteralPath $fakeData.Path)) 'absent user data records a receipt without creating the user directory'
New-Item -ItemType Directory -Path $fakeData.Path | Out-Null
$emptyBackup = Invoke-ZHUserDataBackup $fakeData
Assert-Check ($emptyBackup.Verified -and $emptyBackup.Files.Count -eq 0) 'empty user-data directory backs up successfully'
New-Item -ItemType Directory -Path (Join-Path $fakeData.Path 'Empty') | Out-Null
Set-Content -LiteralPath (Join-Path $fakeData.Path 'personal-save.txt') -Value 'Synthetic save; preserve me.'
$userBefore = @(Get-ZHInventory $fakeData.Path)
$fullBackup = Invoke-ZHUserDataBackup $fakeData
Assert-Check (@(Get-ZHInventoryDifferences $userBefore @(Get-ZHInventory (Join-Path $fullBackup.BackupPath 'data'))).Count -eq 0) 'backup contents match all synthetic user files'
Assert-Check (@(Get-ZHInventoryDifferences $userBefore @(Get-ZHInventory $fakeData.Path)).Count -eq 0) 'backup never changes existing synthetic user data'
Assert-Check (Test-Path -LiteralPath (Join-Path $fullBackup.BackupPath 'data/Empty') -PathType Container) 'backup preserves empty directories'
Assert-Check ($absentBackup.BackupPath -ne $emptyBackup.BackupPath -and $emptyBackup.BackupPath -ne $fullBackup.BackupPath) 'backups always use fresh destinations'

# Synthetic, clearly labeled asset tree: infrastructure tests do not assert game validity.
$source = Join-Path $testRoot 'steamapps/common/Command & Conquer Generals - Zero Hour'
foreach ($dir in @('Data', 'Data/Empty', 'MSS', 'ZH_Generals/Data', 'ZH_Generals/MSS')) {
    New-Item -ItemType Directory -Path (Join-Path $source $dir) -Force | Out-Null
}
foreach ($file in @('INIZH.big', 'W3DZH.big', 'TerrainZH.big', 'TexturesZH.big', 'WindowZH.big',
        'AudioZH.big', 'SpeechZH.big', 'binkw32.dll', 'mss32.dll', 'ZH_Generals/INI.big',
        'ZH_Generals/W3D.big', 'ZH_Generals/Terrain.big', 'Data/example with spaces.txt')) {
    Set-Content -LiteralPath (Join-Path $source $file) -Value 'Synthetic infrastructure fixture, not game content.'
}
$sourceBefore = @(Get-ZHInventory $source)
& "$PSScriptRoot/Stage-ZHRuntime.ps1" -SourcePath $source -Name $testName -ValidateOnly
Assert-Check (-not (Test-Path -LiteralPath (Get-ZHStage $testName))) 'stage validation does not create destination'
& "$PSScriptRoot/Stage-ZHRuntime.ps1" -SourcePath $source -Name $testName
$stage = Get-ZHStage $testName
Assert-Check (Test-Path -LiteralPath (Join-Path $stage 'game/Data/Empty') -PathType Container) 'empty directories preserved'
Assert-Check (@(Get-ZHInventoryDifferences $sourceBefore @(Get-ZHInventory $source)).Count -eq 0) 'copy leaves synthetic source hashes unchanged'
$manifestBefore = (Get-FileHash -LiteralPath (Join-Path $stage 'manifest.json')).Hash
Assert-Throws { & "$PSScriptRoot/Stage-ZHRuntime.ps1" -SourcePath $source -Name $testName } 'existing destination refused'
& "$PSScriptRoot/Stage-ZHRuntime.ps1" -SourcePath $source -Name $testName -Refresh -ValidateOnly
Assert-Check ((Get-FileHash -LiteralPath (Join-Path $stage 'manifest.json')).Hash -eq $manifestBefore) 'refresh validation retains existing stage'
& "$PSScriptRoot/Test-ZHRuntime.ps1" -Name $testName
& "$PSScriptRoot/Test-ZHRuntime.ps1" -Name $testName -CheckSource
& "$PSScriptRoot/Launch-ZHRuntime.ps1" -Name $testName -GameArguments @('-win', '-fps', '30') -ValidateOnly
Assert-Check (@(Get-ChildItem -LiteralPath $stage -Filter 'launch-*').Count -eq 0) 'launch validation writes no launch receipt'
Assert-Throws { & "$PSScriptRoot/Launch-ZHRuntime.ps1" -Name $testName } 'unacknowledged launch blocked before process creation'
Assert-Check (@(Get-ChildItem -LiteralPath $stage -Filter 'launch-*').Count -eq 0) 'blocked launch writes no launch receipt'

# Modify only our generated fixture to check drift detection; preserve it for inspection.
Add-Content -LiteralPath (Join-Path $stage 'game/Data/example with spaces.txt') -Value 'test drift'
Assert-Throws { & "$PSScriptRoot/Test-ZHRuntime.ps1" -Name $testName } 'runtime content drift detected'
Write-Host "Completed $checks assertions. No game launched. Synthetic fixtures retained under $testRoot and $stage."
Write-Host 'These synthetic runtimes contain placeholder assets and must never be launched.'
