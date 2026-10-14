#Requires -Version 7.0
. (Join-Path $PSScriptRoot '../zh-runtime/Common.ps1')

function Get-NativeStage([ValidateSet('Generals','ZeroHour')][string]$Title, [string]$Name) {
    if ($Name -notmatch '^[a-zA-Z][a-zA-Z0-9_-]{0,47}$' -or $Name -match '^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])$') { throw 'Unsafe stage name' }
    $root = Join-Path $script:ZHRepository 'build/dev-runtimes/native'
    $path = Join-Path $root "$Title/$Name"
    Assert-ZHNoLinks $path
    return $path
}

function Get-NativePE([string]$Path) {
    $b = [IO.File]::ReadAllBytes($Path)
    if ($b.Length -lt 64 -or $b[0] -ne 77 -or $b[1] -ne 90) { throw "Invalid PE: $Path" }
    $pe = [BitConverter]::ToInt32($b,60)
    if ($pe -lt 64 -or $pe + 264 -gt $b.Length -or [BitConverter]::ToUInt32($b,$pe) -ne 0x4550) { throw 'Invalid PE header' }
    $machine = [BitConverter]::ToUInt16($b,$pe+4)
    $optional = $pe+24
    $magic = [BitConverter]::ToUInt16($b,$optional)
    if ($machine -ne 0x8664 -or $magic -ne 0x20b) { throw "Expected AMD64 PE32+: $Path" }
    $sections = $optional+[BitConverter]::ToUInt16($b,$pe+20)
    $count = [BitConverter]::ToUInt16($b,$pe+6)
    function Offset([uint32]$Rva) {
        for ($i=0; $i -lt $count; ++$i) {
            $s=$sections+40*$i
            $start=[BitConverter]::ToUInt32($b,$s+12); $size=[BitConverter]::ToUInt32($b,$s+16)
            if ($Rva -ge $start -and $Rva -lt $start+$size) { return [int]($Rva-$start+[BitConverter]::ToUInt32($b,$s+20)) }
        }
        throw 'Invalid import RVA'
    }
    $imports=@(); $rva=[BitConverter]::ToUInt32($b,$optional+120)
    if ($rva) {
        $at=Offset $rva
        for ($n=0; $n -lt 256; ++$n) {
            $nameRva=[BitConverter]::ToUInt32($b,$at+12)
            if (!$nameRva) { break }
            $nameAt=Offset $nameRva; $end=$nameAt
            while ($end -lt $b.Length -and $b[$end]) { ++$end }
            if ($end-$nameAt -gt 256 -or $end -eq $b.Length) { throw 'Invalid import name' }
            $imports += [Text.Encoding]::ASCII.GetString($b,$nameAt,$end-$nameAt)
            $at+=20
        }
    }
    return [pscustomobject]@{ Machine='8664'; Magic='20b'; Imports=$imports
        HasDelayImports=([BitConverter]::ToUInt32($b,$optional+112+13*8) -ne 0) }
}

function Assert-NativeDependencies([string]$Executable, [string]$GamePath) {
    if ([IntPtr]::Size -ne 8) { throw 'Use native 64-bit PowerShell 7' }
    if (@(Get-ZHTree $GamePath | Where-Object { !$_.PSIsContainer -and $_.Extension -ieq '.dll' }).Count) {
        throw 'No app-local DLLs allowed in the first headless runtime'
    }
    $pe=Get-NativePE $Executable; $result=@()
    if ($pe.HasDelayImports) { throw 'Unexpected game delay imports: review dependencies' }
    foreach ($dll in $pe.Imports) {
        if ($dll -like 'api-ms-win-crt-*-l1-1-0.dll') {
            $result += [pscustomobject]@{Name=$dll; Route='Windows UCRT API set'}; continue
        }
        if ($dll -notmatch '^[a-zA-Z0-9_.-]+\.dll$') { throw 'Invalid DLL name' }
        $path=Join-Path $env:WINDIR "System32/$dll"
        $null=Get-NativePE $path
        $result += [pscustomobject]@{Name=$dll; Route=$path; SHA256=(Get-FileHash -LiteralPath $path).Hash}
    }
    foreach ($dll in @('ucrtbase.dll','dbghelp.dll')) { $null=Get-NativePE (Join-Path $env:WINDIR "System32/$dll") }
    return $result
}

# Files a normal launch is expected to create next to the executable. Only an *added* file with exactly
# one of these root-level names is tolerated; any other added/changed/removed file still fails.
$script:NativeNormalLaunchGenerated=@('generals64.log')

# Crash dumps written by the engine's MiniDumper (CrashFG/CrashMG-<date>-<build>-pid<N>.dmp). They are evidence of
# a failed run, never an accepted outcome: the launcher moves them out of the stage and still fails the run.
$script:NativeCrashDumpPattern='^Added: CrashDumps\\Crash(FG|MG)-\d{8}-\d{6}-[0-9a-f]+-pid\d+\.dmp$'

function Get-NativeRuntimeDifferences($Expected,$Actual,[string[]]$AllowAdded=@(),[switch]$AllowCrashDumps) {
    $allowed=@($AllowAdded | ForEach-Object { "Added: $_" })
    @(Get-ZHInventoryDifferences $Expected $Actual | Where-Object {
        $_ -notin $allowed -and !($AllowCrashDumps -and $_ -match $script:NativeCrashDumpPattern) })
}

# Moves crash dumps out of the stage into the evidence directory. Returns the preserved paths (empty: no crash).
function Restore-NativeRuntimeCrashDumps([string]$GamePath,[string]$EvidenceDirectory,[string]$Stamp) {
    $dumps=Join-Path $GamePath 'CrashDumps'
    $kept=@()
    if (!(Test-Path -LiteralPath $dumps -PathType Container)) { return $kept }
    foreach ($file in @(Get-ChildItem -LiteralPath $dumps -File -Filter 'Crash*.dmp')) {
        if ("Added: CrashDumps\$($file.Name)" -notmatch $script:NativeCrashDumpPattern) { continue }
        $target=Join-Path $EvidenceDirectory "crash-$Stamp"
        New-Item -ItemType Directory -Force -Path $target | Out-Null
        $destination=Join-Path $target $file.Name
        Move-Item -LiteralPath $file.FullName -Destination $destination
        $kept+=$destination
    }
    return $kept
}

# Preserves expected runtime-generated files into the evidence directory, then removes them from the stage so
# the staged candidate is byte-identical to its manifest again. Returns the preserved paths.
function Restore-NativeRuntimeGeneratedFiles([string]$GamePath,[string[]]$Names,[string]$EvidenceDirectory,[string]$Stamp) {
    $kept=@()
    foreach ($name in $Names) {
        if ($name -notmatch '^[A-Za-z0-9_.-]+$') { throw "Invalid generated file name: $name" }
        $source=Join-Path $GamePath $name
        if (!(Test-Path -LiteralPath $source -PathType Leaf)) { continue }
        New-Item -ItemType Directory -Force -Path $EvidenceDirectory | Out-Null
        $target=Join-Path $EvidenceDirectory "$Stamp.$name"
        Copy-Item -LiteralPath $source -Destination $target
        if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $target).Hash) { throw "Evidence copy mismatch: $name" }
        Remove-Item -LiteralPath $source -Force
        $kept+=$target
    }
    return $kept
}

function Read-NativeStage([string]$Title,[string]$Name,[string[]]$AllowAdded=@(),[switch]$AllowCrashDumps) {
    $stage=Get-NativeStage $Title $Name
    $manifest=Get-Content -LiteralPath (Join-Path $stage 'manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.Schema -ne 1 -or $manifest.Title -ne $Title -or $manifest.StagePath -ne $stage -or
        $manifest.GamePath -ne (Join-Path $stage 'game')) { throw 'Stage identity mismatch' }
    $diff=@(Get-NativeRuntimeDifferences $manifest.Inventory @(Get-ZHInventory $manifest.GamePath) $AllowAdded -AllowCrashDumps:$AllowCrashDumps)
    if ($diff.Count) { throw "Runtime changed: $($diff -join '; ')" }
    $current=Get-NativeBuild $Title
    if ($current.ExecutableSHA256 -ne $manifest.Build.ExecutableSHA256 -or $current.PdbSHA256 -ne $manifest.Build.PdbSHA256) {
        throw 'Build changed; stage a fresh runtime'
    }
    $null=Assert-NativeDependencies (Join-Path $manifest.GamePath ([IO.Path]::GetFileName($current.Executable))) $manifest.GamePath
    return $manifest
}

function Get-NativeBuild([ValidateSet('Generals','ZeroHour')][string]$Title) {
    $Preset = 'win64'
    $root = Join-Path $script:ZHRepository 'build/win64'
    $relative = if ($Title -eq 'Generals') { 'Generals/Release/generalsv' } else { 'GeneralsMD/Release/generalszh' }
    $exe = Join-Path $root ($relative + '.exe')
    $pdb = Join-Path $root ($relative + '.pdb')
    foreach ($path in @($exe, $pdb, (Join-Path $root 'CMakeCache.txt'))) {
        Assert-ZHNoLinks $path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing build artifact: $path" }
    }
    # Read PE CodeView and MSF7 PDB info stream to prove GUID+age pairing, not just filenames.
    $bytes = [IO.File]::ReadAllBytes($exe)
    $pe = [BitConverter]::ToInt32($bytes, 60)
    if ([BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x8664) { throw 'Expected native AMD64 PE build.' }
    $optional = $pe + 24
    if ([BitConverter]::ToUInt16($bytes, $optional) -ne 0x20b) { throw 'Expected PE32+ optional header.' }
    $debugRva = [BitConverter]::ToUInt32($bytes, $optional + 112 + 6 * 8)
    $debugSize = [BitConverter]::ToUInt32($bytes, $optional + 116 + 6 * 8)
    $sections = $optional + [BitConverter]::ToUInt16($bytes, $pe + 20)
    $debugOffset = $null
    for ($i = 0; $i -lt [BitConverter]::ToUInt16($bytes, $pe + 6); $i++) {
        $s = $sections + 40 * $i
        $rva = [BitConverter]::ToUInt32($bytes, $s + 12)
        $size = [Math]::Max([BitConverter]::ToUInt32($bytes, $s + 8), [BitConverter]::ToUInt32($bytes, $s + 16))
        if ($debugRva -ge $rva -and $debugRva -lt $rva + $size) {
            $debugOffset = [int]($debugRva - $rva + [BitConverter]::ToUInt32($bytes, $s + 20))
        }
    }
    if ($null -eq $debugOffset) { throw 'PE debug directory unavailable.' }
    $codeView = $null
    for ($i = 0; $i -lt $debugSize; $i += 28) {
        if ([BitConverter]::ToUInt32($bytes, $debugOffset + $i + 12) -eq 2) {
            $offset = [int][BitConverter]::ToUInt32($bytes, $debugOffset + $i + 24)
            if ([Text.Encoding]::ASCII.GetString($bytes, $offset, 4) -eq 'RSDS') { $codeView = $offset }
        }
    }
    if ($null -eq $codeView) { throw 'RSDS debug identity unavailable.' }
    $exeGuid = [Guid]::new([byte[]]$bytes[($codeView + 4)..($codeView + 19)])
    $exeAge = [BitConverter]::ToUInt32($bytes, $codeView + 20)
    $stream = [IO.File]::OpenRead($pdb)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        $header = $reader.ReadBytes(56)
        if (-not [Text.Encoding]::ASCII.GetString($header).StartsWith('Microsoft C/C++ MSF 7.00')) { throw 'Expected MSF7 PDB.' }
        $blockSize = [BitConverter]::ToUInt32($header, 32)
        $directorySize = [BitConverter]::ToUInt32($header, 44)
        $blockMap = [BitConverter]::ToUInt32($header, 52)
        if ($blockSize -lt 512 -or $blockSize -gt 65536 -or $directorySize -gt $blockSize * $blockSize / 4) { throw 'Unsupported PDB directory layout.' }
        $stream.Position = [long]$blockMap * $blockSize
        $blocks = @(); for ($i = 0; $i -lt [Math]::Ceiling($directorySize / $blockSize); $i++) { $blocks += $reader.ReadUInt32() }
        $directory = [IO.MemoryStream]::new()
        try {
            foreach ($block in $blocks) { $stream.Position = [long]$block * $blockSize; $chunk = $reader.ReadBytes($blockSize); $directory.Write($chunk, 0, $chunk.Length) }
            $d = $directory.ToArray()
        } finally { $directory.Dispose() }
        $count = [BitConverter]::ToUInt32($d, 0)
        if ($count -lt 2 -or $count -gt ($directorySize - 4) / 4) { throw 'Invalid PDB stream count.' }
        $stream0Size = [BitConverter]::ToUInt32($d, 4)
        $stream1Size = [BitConverter]::ToUInt32($d, 8)
        if ($stream1Size -lt 28 -or $stream1Size -eq [uint32]::MaxValue) { throw 'Missing PDB info stream.' }
        $skip = 0; if ($stream0Size -ne [uint32]::MaxValue) { $skip = [int][Math]::Ceiling($stream0Size / $blockSize) }
        $infoBlock = [BitConverter]::ToUInt32($d, 4 + 4 * $count + 4 * $skip)
        $stream.Position = [long]$infoBlock * $blockSize
        $info = $reader.ReadBytes(28)
        $pdbGuid = [Guid]::new([byte[]]$info[12..27])
        $pdbAge = [BitConverter]::ToUInt32($info, 8)
        if ($pdbGuid -ne $exeGuid -or $pdbAge -ne $exeAge) { throw 'EXE/PDB GUID or age mismatch. Build matching artifacts first.' }
    } finally { $stream.Dispose() }
    return [pscustomobject]@{
        Preset = $Preset; Executable = $exe; Pdb = $pdb; DebugGuid = $exeGuid.ToString(); DebugAge = $exeAge
        ExecutableSHA256 = (Get-FileHash -LiteralPath $exe).Hash; PdbSHA256 = (Get-FileHash -LiteralPath $pdb).Hash
        Cache = @(Get-Content -LiteralPath (Join-Path $root 'CMakeCache.txt') | Where-Object { $_ -match '^(CMAKE_(CXX_COMPILER|GENERATOR|MSVC_RUNTIME_LIBRARY)|RTS_BUILD_OPTION)[^=]*=' })
    }
}
