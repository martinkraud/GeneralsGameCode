# Safety tests for Cleanup-NativeCandidates.ps1 (runs entirely inside a temporary tree; never touches real candidates).
$ErrorActionPreference = 'Stop'
$script = Join-Path $PSScriptRoot 'Cleanup-NativeCandidates.ps1'
$work = Join-Path ([IO.Path]::GetTempPath()) ('native-cleanup-test-' + [Guid]::NewGuid().ToString('N'))
$native = Join-Path $work 'dev-runtimes\native'
$archive = Join-Path $work 'evidence-archive'
$failures = New-Object System.Collections.Generic.List[string]

function Expect([bool]$condition, [string]$message) {
    if (-not $condition) { $failures.Add($message); Write-Host "FAIL: $message" } else { Write-Host "ok:   $message" }
}
function New-Candidate([string]$title, [string]$name, [int]$ageMinutes, [switch]$Evidence, [string]$Extra) {
    $dir = Join-Path $native "$title\$name"
    New-Item -ItemType Directory -Force -Path (Join-Path $dir 'game') | Out-Null
    Set-Content -LiteralPath (Join-Path $dir 'game\generalsv.exe') -Value ('exe ' + $name)
    Set-Content -LiteralPath (Join-Path $dir 'manifest.json') -Value ('{"name":"' + $name + '"}')
    if ($Evidence) {
        $runs = Join-Path $dir 'runs\20261012T000000Z\userdata\CrashDumps'
        New-Item -ItemType Directory -Force -Path $runs | Out-Null
        Set-Content -LiteralPath (Join-Path $runs 'crash.dmp') -Value ('dump ' + $name)
        Set-Content -LiteralPath (Join-Path $dir 'runs\20261012T000000Z\log.txt') -Value ('log ' + $name)
    }
    if ($Extra) { Set-Content -LiteralPath (Join-Path $dir $Extra) -Value 'unexpected' }
    (Get-Item -LiteralPath $dir).LastWriteTime = (Get-Date).AddMinutes(-$ageMinutes)
}
function Run([string[]]$arguments) {
    $output = & pwsh -NoProfile -File $script @arguments 2>&1 | Out-String
    return [pscustomobject]@{ Code = $LASTEXITCODE; Text = $output }
}
function Plan-Hash([string]$text) { if ($text -match 'PLAN_HASH: ([0-9A-F]{64})') { return $Matches[1] } return $null }

try {
    # newest first: n9 (active), n8, ... ; protected and accepted candidates are old.
    New-Candidate Generals x64-30-old-evidence 900 -Evidence
    New-Candidate Generals x64-31-old-plain 800
    New-Candidate Generals x64-32-protected 700 -Evidence
    New-Candidate Generals x64-33-accepted 600
    New-Candidate Generals x64-34-ambiguous 500 -Evidence -Extra 'mystery.bin'
    New-Candidate Generals x64-35-mid 400 -Evidence
    New-Candidate Generals x64-36-newer 300
    New-Candidate Generals x64-37-active 10
    New-Candidate ZeroHour x64-20-zh-old 900 -Evidence
    New-Candidate ZeroHour x64-21-zh-new 20
    New-Candidate ZeroHour x64-22-zh-newest 10
    # a reparse point inside a candidate must make it ambiguous (junction needs no privileges)
    New-Candidate Generals x64-29-junction 950 -Evidence
    $target = Join-Path $work 'outside'
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Set-Content -LiteralPath (Join-Path $target 'precious.txt') -Value 'do not touch'
    New-Item -ItemType Junction -Path (Join-Path $native 'Generals\x64-29-junction\game\link') -Target $target | Out-Null

    $common = @('-NativeRoot', $native, '-ArchiveRoot', $archive, '-Active', 'x64-37-active', '-LatestAccepted', 'x64-33-accepted',
                '-Protect', 'x64-32-protected', '-KeepNewest', '1', '-PolicyFile', (Join-Path $work 'no-policy.json'))

    # 1. dry run changes nothing and prints a plan hash and the expected decisions
    $dry = Run $common
    Expect ($dry.Code -eq 0) 'dry run exits 0'
    $hash = Plan-Hash $dry.Text
    Expect ($null -ne $hash) 'dry run prints PLAN_HASH'
    Expect ($dry.Text -match 'DRY RUN') 'dry run says nothing will be changed'
    Expect (-not (Test-Path $archive)) 'dry run creates no archive'
    Expect ((Get-ChildItem (Join-Path $native 'Generals') -Directory).Count -eq 9) 'dry run removes nothing'
    Expect ($dry.Text -match 'KEEP\s+Generals\\x64-37-active') 'active candidate: KEEP'
    Expect ($dry.Text -match 'KEEP\s+Generals\\x64-33-accepted') 'latest accepted: KEEP'
    Expect ($dry.Text -match 'KEEP\s+Generals\\x64-32-protected') 'protected: KEEP'
    Expect ($dry.Text -match 'AMBIGUOUS\s+Generals\\x64-34-ambiguous') 'unexpected entry: AMBIGUOUS (fails closed)'
    Expect ($dry.Text -match 'AMBIGUOUS\s+Generals\\x64-29-junction') 'reparse point inside: AMBIGUOUS'
    Expect ($dry.Text -match 'ARCHIVE\s+Generals\\x64-30-old-evidence') 'superseded with evidence: ARCHIVE'
    Expect ($dry.Text -match 'REMOVE\s+Generals\\x64-31-old-plain') 'superseded without evidence: REMOVE'
    Expect ($dry.Text -match 'KEEP\s+ZeroHour\\x64-22-zh-newest') 'newest per title kept'
    Expect ($dry.Text -match 'estimated reclaim') 'prints estimated reclaim'

    # 2. execute without / with a wrong approval hash is refused and changes nothing
    $noHash = Run ($common + '-Execute')
    Expect ($noHash.Code -ne 0) 'execute without -ApprovedPlan is refused'
    $bad = Run ($common + @('-Execute', '-ApprovedPlan', ('0' * 64)))
    Expect ($bad.Code -ne 0) 'execute with a wrong plan hash is refused'
    Expect ((Get-ChildItem (Join-Path $native 'Generals') -Directory).Count -eq 9) 'refused execute removes nothing'

    # 3. guards
    $steam = Run @('-NativeRoot', 'C:\Program Files (x86)\Steam\steamapps\common\X\dev-runtimes\native')
    Expect ($steam.Code -ne 0) 'Steam-looking root is refused'
    $badRoot = Run @('-NativeRoot', $work)
    Expect ($badRoot.Code -ne 0) 'root that is not dev-runtimes\native is refused'
    $inside = Run @('-NativeRoot', $native, '-ArchiveRoot', (Join-Path $native 'archive'))
    Expect ($inside.Code -ne 0) 'archive root inside the native root is refused'

    # 4. approved execute: archive verified, superseded removed, everything else intact
    $run = Run ($common + @('-Execute', '-ApprovedPlan', $hash))
    Expect ($run.Code -eq 0) 'approved execute succeeds'
    foreach ($name in 'x64-37-active', 'x64-33-accepted', 'x64-32-protected', 'x64-34-ambiguous', 'x64-29-junction') {
        Expect (Test-Path (Join-Path $native "Generals\$name")) "kept: $name"
    }
    foreach ($name in 'x64-30-old-evidence', 'x64-31-old-plain', 'x64-35-mid', 'x64-36-newer') {
        Expect (-not (Test-Path (Join-Path $native "Generals\$name"))) "removed: $name"
    }
    Expect (Test-Path (Join-Path $archive 'Generals\x64-30-old-evidence\runs\20261012T000000Z\userdata\CrashDumps\crash.dmp')) 'crash dump archived'
    Expect (Test-Path (Join-Path $archive 'Generals\x64-30-old-evidence\manifest.json')) 'manifest archived'
    Expect (Test-Path (Join-Path $archive 'Generals\x64-31-old-plain\manifest.json')) 'manifest archived for a candidate without runs'
    Expect (Test-Path (Join-Path $target 'precious.txt')) 'junction target outside the root untouched'
    Expect (-not (Test-Path (Join-Path $native 'ZeroHour\x64-20-zh-old'))) 'ZeroHour superseded removed'
    Expect (Test-Path (Join-Path $archive 'ZeroHour\x64-20-zh-old\runs')) 'ZeroHour evidence archived'
    Expect (Test-Path (Join-Path $native 'ZeroHour\x64-22-zh-newest')) 'ZeroHour newest kept'
    $rerun = Run $common
    Expect ($rerun.Code -eq 0 -and $rerun.Text -cnotmatch '(?m)^(REMOVE|ARCHIVE)\s') 'second dry run has nothing left to remove'
    if ($rerun.Code -ne 0 -or $rerun.Text -cmatch '(?m)^(REMOVE|ARCHIVE)\s') { Write-Host $rerun.Text }
}
finally {
    if (Test-Path -LiteralPath $work) {
        # remove the junction first so the target is never traversed by the cleanup of the test tree
        $link = Join-Path $native 'Generals\x64-29-junction\game\link'
        if (Test-Path -LiteralPath $link) { (Get-Item -LiteralPath $link).Delete() }
        Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
    }
}
if ($failures.Count) {
    Write-Host ("{0} failure(s)" -f $failures.Count)
    exit 1
}
Write-Host 'Native cleanup workflow tests passed.'
exit 0
