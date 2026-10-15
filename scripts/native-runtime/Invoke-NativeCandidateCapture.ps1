<#
.SYNOPSIS
  Launches ONE named native candidate for a performance capture with fixed, documented arguments, and verifies afterwards
  that the capture really came from that candidate. (Added after a capture was taken with a different candidate's exe.)

.DESCRIPTION
  * The executable is always <repo>\build\dev-runtimes\native\<Title>\<Candidate>\game\generalsv.exe (never the Steam install).
  * Prints the candidate path and the SHA256 of the executable before launching; -ExpectedSha256 refuses a mismatch.
  * Creates the (absolute, existing) output directory and passes -performanceProfile to it.
  * After the game exits, reads the newest *-summary.txt and prints its git= / build= lines and whether it contains the
    X64.35 native_frame_phases table; -ExpectPhases fails the run (exit 3) when the table is missing.
  * -WhatIf prints the exact command without launching anything.

.EXAMPLE
  pwsh scripts\native-runtime\Invoke-NativeCandidateCapture.ps1 -Candidate x64-35-attribution -OutputDirectory C:\perf\x64-35-attribution\C-A2 -ExpectPhases
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory = $true)][string]$Candidate,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$Title = 'Generals',
    [string[]]$GameArguments = @('-devMode', '-quickGame', '-devFps', 'uncapped'),
    [string]$ExpectedSha256,
    [switch]$ExpectPhases
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$gameDir = Join-Path $repo "build\dev-runtimes\native\$Title\$Candidate\game"
$exe = Join-Path $gameDir 'generalsv.exe'
if (-not (Test-Path -LiteralPath $exe)) { Write-Error "Candidate executable not found: $exe"; exit 1 }
if ($exe -match '(?i)steamapps') { Write-Error 'Refusing a Steam path.'; exit 1 }
$hash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
Write-Host "Candidate : $Candidate"
Write-Host "Executable: $exe"
Write-Host "SHA256    : $hash"
if ($ExpectedSha256 -and $ExpectedSha256 -ne $hash) { Write-Error 'Executable hash does not match -ExpectedSha256.'; exit 1 }
if (-not [IO.Path]::IsPathRooted($OutputDirectory)) { Write-Error 'OutputDirectory must be an absolute path.'; exit 1 }
$arguments = @($GameArguments) + @('-performanceProfile', $OutputDirectory)
Write-Host ("Command   : generalsv.exe {0}" -f ($arguments -join ' '))
if ($WhatIfPreference) { Write-Host 'WhatIf: nothing launched.'; exit 0 }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
Write-Host 'Launching. Create the scenario in game, then press Ctrl+Alt+Shift+F11 to start the capture; exit via the menu.'
$process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $gameDir -PassThru -Wait
Write-Host "Game exited with code $($process.ExitCode)."
$summary = Get-ChildItem -LiteralPath $OutputDirectory -Filter '*-summary.txt' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $summary) { Write-Host 'No *-summary.txt was written (was the capture started with F11?).'; exit 2 }
$text = Get-Content -LiteralPath $summary.FullName -Raw
foreach ($line in ($text -split "`n" | Where-Object { $_ -match '^(git|build|capture_label|elapsed_seconds)=' })) { Write-Host $line.Trim() }
$phases = $text -match 'native_frame_phases'
Write-Host ("native_frame_phases present: {0}" -f $phases)
if ($ExpectPhases -and -not $phases) {
    Write-Host 'This capture does NOT contain the attribution table: it was not produced by an attribution build.'
    exit 3
}
exit 0
