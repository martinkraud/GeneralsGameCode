#Requires -Version 7.0
# X64.52B: ONE command that runs the permanent regression gates of the accepted modernization behaviour, so a new milestone does not have to rediscover old bugs by hand.
#
#   pwsh scripts/native-runtime/Invoke-RegressionGates.ps1                                   # fast: Python contracts + unit gates (no game started)
#   pwsh scripts/native-runtime/Invoke-RegressionGates.ps1 -GoogleTest                       # + Generals / Zero Hour gtest executables (must be built)
#   pwsh scripts/native-runtime/Invoke-RegressionGates.ps1 -Native -ZeroHourCandidate <stage> -GeneralsCandidate <stage>
#                                                                                           # + the native gates that start the game (about 3 minutes per title,
#                                                                                           #   needs the desktop and the keyboard / mouse for ~40 s: do not touch them)
#
# The map of gate -> accepted invariant is in docs/modernization/X64_52B_CAMERA_INPUT_REGRESSION.md section 7. Native gates:
#   camera with REAL OS input (key / screen edge / RMB) at 60 / 120 / 240 FPS      Invoke-CameraRealInput.ps1 + analyze_camera_real.py
#   Original AI replay digests == the reference executable, Enhanced repeatable    Invoke-AiBehaviorGate.ps1 (needs -ReferenceExe, a previous milestone's executable)
# Exit code 0 = every selected gate passed.
[CmdletBinding()]
param([switch]$GoogleTest,
      [switch]$Native,
      [string]$GeneralsCandidate,
      [string]$ZeroHourCandidate,
      [int[]]$Fps = @(60, 120, 240))
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$failed = [System.Collections.Generic.List[string]]::new()
function Step([string]$name, [scriptblock]$body) {
    Write-Host "== $name"
    try { & $body; if ($LASTEXITCODE) { throw "exit code $LASTEXITCODE" }; Write-Host "   PASS: $name" -ForegroundColor Green }
    catch { Write-Host "   FAIL: $name : $_" -ForegroundColor Red; $failed.Add($name) }
}

foreach ($dir in 'native-runtime', 'performance', 'diagnostics') {
    Step "python $dir" {
        $out = python -m unittest discover -s (Join-Path $repo "scripts\$dir") -p 'test_*.py' 2>&1
        $code = $LASTEXITCODE
        $out | Select-Object -Last 4 | ForEach-Object { Write-Host "   $_" }
        $global:LASTEXITCODE = 0
        if ($code) { throw 'unittest failed' }
    }
}

if ($GoogleTest) {
    foreach ($exe in 'Generals\Release\g_googletest.exe', 'GeneralsMD\Release\z_googletest.exe', 'Tests\Google\Release\g_googletest.exe', 'Tests\Google\Release\z_googletest.exe') {
        $path = Join-Path $repo "build\win64\$exe"
        if (Test-Path -LiteralPath $path) { Step "gtest $exe" { & $path | Select-Object -Last 3 | ForEach-Object { Write-Host "   $_" } } }
    }
}

if ($Native) {
    foreach ($t in @(@{ Title = 'Generals'; Candidate = $GeneralsCandidate }, @{ Title = 'ZeroHour'; Candidate = $ZeroHourCandidate })) {
        if (-not $t.Candidate) { Write-Host "skip native camera gate for $($t.Title): no -$($t.Title)Candidate"; continue }
        foreach ($fps in $Fps) {
            $label = "gate$fps"
            Step "camera real-input gate $($t.Title) @ $fps" {
                & pwsh -NoProfile -File (Join-Path $PSScriptRoot 'Invoke-CameraRealInput.ps1') -Title $t.Title -Candidate $t.Candidate -Label $label -Fps $fps | Out-Null
                python (Join-Path $PSScriptRoot 'analyze_camera_real.py') (Join-Path $repo "build\diag\camera-real-$($t.Title)-$label")
            }
        }
    }
}

if ($failed.Count) { Write-Host "REGRESSION GATES FAILED: $($failed -join '; ')" -ForegroundColor Red; exit 1 }
Write-Host 'REGRESSION GATES PASSED' -ForegroundColor Green
