#Requires -Version 7.0
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:ZHRepository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$script:ZHProtected = 'C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour'
$script:ZHRoot = Join-Path $script:ZHRepository 'build/dev-runtimes/zh'

function Get-ZHPath([string]$Path) {
    if (-not [IO.Path]::IsPathFullyQualified($Path) -or $Path -notmatch '^[A-Za-z]:[\\/]' -or
        $Path.Substring(2) -match '[:*?]') { throw "Expected an absolute local filesystem path: $Path" }
    return [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
}

function Test-ZHWithin([string]$Path, [string]$Parent) {
    return $Path.Equals($Parent, [StringComparison]::OrdinalIgnoreCase) -or
        $Path.StartsWith($Parent.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
}

function Assert-ZHNoLinks([string]$Path) {
    # Check ancestors as well as the requested path; lexical containment alone is insufficient.
    $current = [IO.Path]::GetFullPath($Path)
    while ($current) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse point refused: $current" }
        }
        $current = [IO.Path]::GetDirectoryName($current)
    }
}

function Get-ZHTree([string]$Path) {
    Assert-ZHNoLinks $Path
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($Path)
    while ($pending.Count) {
        foreach ($item in Get-ChildItem -LiteralPath $pending.Pop() -Force) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse point refused: $($item.FullName)" }
            $item
            if ($item.PSIsContainer) { $pending.Push($item.FullName) }
        }
    }
}

function Get-ZHStage([string]$Name) {
    if ($Name -notmatch '^[a-zA-Z0-9][a-zA-Z0-9_-]{0,47}$' -or
        $Name -match '^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])$') { throw "Unsafe runtime name: $Name" }
    $path = Get-ZHPath (Join-Path $script:ZHRoot $Name)
    if (-not (Test-ZHWithin $path $script:ZHRoot) -or (Test-ZHWithin $path $script:ZHProtected)) {
        throw "Unsafe staging destination: $path"
    }
    Assert-ZHNoLinks $path
    return $path
}

function Assert-ZHAssets([string]$Path) {
    foreach ($directory in @('Data', 'MSS', 'ZH_Generals', 'ZH_Generals/Data', 'ZH_Generals/MSS')) {
        if (-not (Test-Path -LiteralPath (Join-Path $Path $directory) -PathType Container)) { throw "Missing asset directory: $directory" }
    }
    foreach ($file in @('INIZH.big', 'W3DZH.big', 'TerrainZH.big', 'TexturesZH.big', 'WindowZH.big',
            'AudioZH.big', 'SpeechZH.big', 'binkw32.dll', 'mss32.dll', 'ZH_Generals/INI.big',
            'ZH_Generals/W3D.big', 'ZH_Generals/Terrain.big')) {
        $candidate = Join-Path $Path $file
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf) -or (Get-Item -LiteralPath $candidate).Length -eq 0) {
            throw "Missing or empty asset/runtime file: $file"
        }
    }
}

function Get-ZHSource([string]$Path) {
    $path = Get-ZHPath $Path
    Assert-ZHNoLinks $path
    if (-not (Test-Path -LiteralPath $path -PathType Container)) { throw "Source directory does not exist: $path" }
    $parent = Split-Path $path -Parent
    if ((Split-Path $path -Leaf) -ne 'Command & Conquer Generals - Zero Hour' -or
        (Split-Path $parent -Leaf) -ne 'common' -or
        (Split-Path (Split-Path $parent -Parent) -Leaf) -ne 'steamapps') {
        throw 'Source must explicitly name the Zero Hour installation under steamapps/common.'
    }
    if (Test-ZHWithin $path (Join-Path $script:ZHRepository 'build/dev-runtimes')) { throw 'A staged runtime cannot be a source installation.' }
    Assert-ZHAssets $path
    return $path
}

function Get-ZHInventory([string]$Path) {
    foreach ($file in @(Get-ZHTree $Path | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)) {
        [pscustomobject]@{
            Path = [IO.Path]::GetRelativePath($Path, $file.FullName)
            Length = $file.Length
            LastWriteTimeUtc = $file.LastWriteTimeUtc.ToString('o')
            SHA256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        }
    }
}

function Get-ZHInventoryDifferences([object[]]$Expected, [object[]]$Actual) {
    $left = @{}; $right = @{}
    foreach ($entry in $Expected) { $left[$entry.Path] = "$($entry.Length)|$($entry.SHA256)" }
    foreach ($entry in $Actual) { $right[$entry.Path] = "$($entry.Length)|$($entry.SHA256)" }
    foreach ($path in $left.Keys) {
        if (-not $right.ContainsKey($path)) { "Removed: $path" }
        elseif ($right[$path] -ne $left[$path]) { "Changed: $path" }
    }
    foreach ($path in $right.Keys) { if (-not $left.ContainsKey($path)) { "Added: $path" } }
}

function Copy-ZHTree([string]$Source, [string]$Destination, [object[]]$Inventory) {
    if (Test-Path -LiteralPath $Destination) { throw "Copy destination already exists: $Destination" }
    Assert-ZHNoLinks $Destination
    New-Item -ItemType Directory -Path $Destination | Out-Null
    foreach ($directory in @(Get-ZHTree $Source | Where-Object PSIsContainer | Sort-Object FullName)) {
        $target = Join-Path $Destination ([IO.Path]::GetRelativePath($Source, $directory.FullName))
        Write-Host "Create directory: $target"
        New-Item -ItemType Directory -Path $target -Force | Out-Null
    }
    foreach ($entry in $Inventory) {
        $target = Join-Path $Destination $entry.Path
        Write-Host "Copy and verify: $($entry.Path)"
        Copy-Item -LiteralPath (Join-Path $Source $entry.Path) -Destination $target
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $entry.SHA256) {
            throw "Copy verification failed (source may have changed): $($entry.Path)"
        }
    }
}

function Write-ZHJson([string]$Path, $Value) {
    if (Test-Path -LiteralPath $Path) { throw "Metadata already exists: $Path" }
    Write-Host "Write metadata: $Path"
    $Value | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $Path -Encoding utf8
}

function Get-ZHBuild([ValidateSet('win32', 'win32-profile')][string]$Preset) {
    $root = Join-Path $script:ZHRepository "build/$Preset"
    $exe = Join-Path $root 'GeneralsMD/Release/generalszh.exe'
    $pdb = Join-Path $root 'GeneralsMD/Release/generalszh.pdb'
    foreach ($path in @($exe, $pdb, (Join-Path $root 'CMakeCache.txt'))) {
        Assert-ZHNoLinks $path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing build artifact: $path" }
    }
    # Read PE CodeView and MSF7 PDB info stream to prove GUID+age pairing, not just filenames.
    $bytes = [IO.File]::ReadAllBytes($exe)
    $pe = [BitConverter]::ToInt32($bytes, 60)
    if ([BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x14c) { throw 'Expected native x86 PE build.' }
    $optional = $pe + 24
    if ([BitConverter]::ToUInt16($bytes, $optional) -ne 0x10b) { throw 'Expected PE32 optional header.' }
    $debugRva = [BitConverter]::ToUInt32($bytes, $optional + 96 + 6 * 8)
    $debugSize = [BitConverter]::ToUInt32($bytes, $optional + 100 + 6 * 8)
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

function Read-ZHStage([string]$Name) {
    $stage = Get-ZHStage $Name
    $manifestPath = Join-Path $stage 'manifest.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw "No completed staging manifest: $stage" }
    $null = @(Get-ZHTree $stage)
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.Schema -ne 1 -or $manifest.StagePath -ne $stage -or $manifest.GamePath -ne (Join-Path $stage 'game')) {
        throw 'Stage manifest identity/path mismatch.'
    }
    Assert-ZHAssets $manifest.GamePath
    foreach ($entry in @(@{Path='generalszh.exe'; Hash=$manifest.Build.ExecutableSHA256}, @{Path='generalszh.pdb'; Hash=$manifest.Build.PdbSHA256})) {
        $file = Join-Path $manifest.GamePath $entry.Path
        if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or (Get-FileHash -LiteralPath $file).Hash -ne $entry.Hash) {
            throw "Staged build changed or missing: $file. Stage a fresh runtime."
        }
    }
    return $manifest
}

function Get-ZHUserData {
    $documents = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
    if (-not $documents) { throw 'Windows Documents known folder could not be resolved.' }
    $leaf = 'Command and Conquer Generals Zero Hour Data'
    $origin = 'engine default'
    foreach ($hive in @([Microsoft.Win32.RegistryHive]::CurrentUser, [Microsoft.Win32.RegistryHive]::LocalMachine)) {
        $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey($hive, [Microsoft.Win32.RegistryView]::Registry32)
        try {
            $key = $base.OpenSubKey('SOFTWARE\Electronic Arts\EA Games\Command and Conquer Generals Zero Hour', $false)
            if ($null -ne $key) {
                try {
                    $value = $key.GetValue('UserDataLeafName', $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                    if ($null -ne $value) {
                        if ($key.GetValueKind('UserDataLeafName') -notin @([Microsoft.Win32.RegistryValueKind]::String, [Microsoft.Win32.RegistryValueKind]::ExpandString)) { throw 'UserDataLeafName is not a string.' }
                        $leaf = [string]$value; $origin = "$hive Registry32"; break
                    }
                } finally { $key.Dispose() }
            }
        } finally { $base.Dispose() }
    }
    if ([string]::IsNullOrWhiteSpace($leaf) -or $leaf -in @('.', '..') -or $leaf -match '[\\/:*?"<>|]' -or $leaf.EndsWith('.') -or $leaf.EndsWith(' ')) {
        throw 'Unusual UserDataLeafName; cannot safely resolve user data. No registry changes made.'
    }
    $path = Get-ZHPath (Join-Path $documents $leaf)
    Assert-ZHNoLinks $path
    if (Test-ZHWithin $path $script:ZHProtected) { throw 'User data resolves into protected Steam directory.' }
    return [pscustomobject]@{Path=$path; Resolution=$origin; Documents=$documents; User=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value}
}

function Assert-ZHNoGameProcess {
    if (Get-Process -Name generalszh,generalsv,generals,game.dat -ErrorAction SilentlyContinue) {
        throw 'Close all Generals/Zero Hour processes before staging, backup or launch.'
    }
}

function Assert-ZHGameArguments([string[]]$GameArguments) {
    foreach ($argument in $GameArguments) {
        if ($argument -match '(?i)^-(setCwd|useCwd)(?:$|[\s=:])' -or $argument -match '[\x00\r\n]') {
            throw 'Additional arguments may not override the working directory or contain control characters.'
        }
    }
}

function Invoke-ZHUserDataBackup($Data, [switch]$ValidateOnly) {
    # Public helper resolves Data from Windows; the function also supports harmless synthetic tests.
    $dataPath = Get-ZHPath $Data.Path
    Assert-ZHNoLinks $dataPath
    $backupRoot = Join-Path $script:ZHRepository 'build/dev-runtimes/userdata-backups'
    Assert-ZHNoLinks $backupRoot
    $backup = Join-Path $backupRoot ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ') + '-' + [Guid]::NewGuid().ToString('N'))
    if ((Test-ZHWithin $backup $dataPath) -or (Test-ZHWithin $dataPath $backup) -or
        (Test-ZHWithin $backup $script:ZHProtected)) { throw 'Unsafe user-data backup destination.' }
    $exists = Test-Path -LiteralPath $dataPath
    if ($exists -and -not (Test-Path -LiteralPath $dataPath -PathType Container)) { throw 'User-data path is not a directory.' }
    if ($exists) { $null = @(Get-ZHTree $dataPath) }
    Write-Host "Resolved shared user data: $dataPath ($($Data.Resolution))"
    Write-Host "New backup: $backup; existing data is never overwritten, deleted or restored by this helper."
    if ($ValidateOnly) { Write-Host "VALIDATION ONLY: directory exists = $exists; no backup written."; return }
    $files = @(); if ($exists) { $files = @(Get-ZHInventory $dataPath) }
    New-Item -ItemType Directory -Path $backup | Out-Null
    if ($exists) {
        Copy-ZHTree $dataPath (Join-Path $backup 'data') $files
        $after = @(Get-ZHInventory $dataPath)
        if (@(Get-ZHInventoryDifferences $files $after).Count) {
            throw 'User data changed during backup. Partial backup retained; close other writers and retry.'
        }
    }
    $receipt = [ordered]@{
        Schema=1; CreatedUtc=[DateTime]::UtcNow.ToString('o'); User=$Data.User
        UserDataPath=$dataPath; Resolution=$Data.Resolution; SourceExisted=$exists
        BackupPath=$backup; Files=$files; Verified=$true
    }
    Write-ZHJson (Join-Path $backup 'backup.json') $receipt
    if (-not $exists) { Write-Host 'User-data directory is absent. Receipt records that pre-launch state; no user-data directory was created.' }
    [pscustomobject]$receipt
}
