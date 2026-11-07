# MKRaudstein Modern Engine - Generals "modern extensions" generator (Windows PowerShell 5.1 port of scripts/native-runtime/modern_extensions.py).
#
# WHY IT EXISTS: the Generals special-power sidebar extension (Zero Hour style: the superweapons, the CIA intelligence and the radar scan get shortcut slots) is
# built FROM THE PLAYER'S OWN retail archives (INI.big, Window.big): five cloned command buttons, three extended command sets, three raised slot counts and three
# re-laid-out sidebar windows. Those results are modified copies of Electronic Arts game data, so the Developer Preview package does NOT contain them: the
# installer generates them on the player's machine, from the player's files, exactly like the developer tooling does. scripts/native-runtime/test_distribution_contract.py
# and Test-DeveloperPreviewPackage.ps1 prove that this port writes byte-identical output to the the developer tool.
#
# Nothing is written here: Build-GeneralsModernExtensions returns the files in memory (relative path -> bytes).
Set-StrictMode -Version 2

$script:Latin1 = [Text.Encoding]::GetEncoding(28591)
$script:NewButtons = @(
    @('Command_FireParticleUplinkCannonFromShortcut', 'Command_FireParticleUplinkCannon'),
    @('Command_CIAIntelligenceFromShortcut', 'Command_CIAIntelligence'),
    @('Command_NeutronMissileFromShortcut', 'Command_NeutronMissile'),
    @('Command_ScudStormFromShortcut', 'Command_ScudStorm'),
    @('Command_RadarVanScanFromShortcut', 'Command_RadarVanScan'))
$script:SetAdditions = @(
    @('SpecialPowerShortcutUSA', @(@(6, 'Command_FireParticleUplinkCannonFromShortcut'), @(7, 'Command_SpySatelliteScanFromCommandCenter'), @(8, 'Command_CIAIntelligenceFromShortcut'))),
    @('SpecialPowerShortcutChina', @(, @(6, 'Command_NeutronMissileFromShortcut'))),
    @('SpecialPowerShortcutGLA', @(@(4, 'Command_ScudStormFromShortcut'), @(5, 'Command_RadarVanScanFromShortcut'))))
$script:Bars = @(    # window file, side in PlayerTemplate, slot count, bar bottom edge
    @('GenPowersShortcutBarUS.wnd', 'America', 8, 429),
    @('GenPowersShortcutBarChina.wnd', 'China', 6, 420),
    @('GenPowersShortcutBarGLA.wnd', 'GLA', 5, 421))
$script:SlotX = 752; $script:SlotW = 48; $script:SlotH = 41; $script:SlotPitch = 35
$script:CmdDx = 1; $script:CmdDy = 7; $script:CmdW = 39; $script:CmdH = 27

# {lower-case backslash name -> bytes} of the entries the caller asks for; reads the table of a BIGF/BIG4 archive and only the wanted entries.
function Read-BigEntries([string]$Path, [string[]]$Wanted) {
    $result = @{}
    $stream = [IO.File]::Open($Path, 'Open', 'Read', 'ReadWrite')
    try {
        $buffered = New-Object IO.BufferedStream ($stream, 1048576)
        $head = New-Object byte[] 16
        if ($buffered.Read($head, 0, 16) -ne 16) { throw "not a BIG archive: $Path" }
        $magic = [Text.Encoding]::ASCII.GetString($head, 0, 4)
        if ($magic -ne 'BIGF' -and $magic -ne 'BIG4') { throw "not a BIG archive: $Path" }
        $count = ([int64]$head[8] -shl 24) -bor ([int64]$head[9] -shl 16) -bor ([int64]$head[10] -shl 8) -bor [int64]$head[11]
        $table = New-Object System.Collections.Generic.List[object]
        $raw = New-Object byte[] 8
        for ($i = 0; $i -lt $count; $i++) {
            if ($buffered.Read($raw, 0, 8) -ne 8) { throw "truncated BIG table: $Path" }
            $off = ([int64]$raw[0] -shl 24) -bor ([int64]$raw[1] -shl 16) -bor ([int64]$raw[2] -shl 8) -bor [int64]$raw[3]
            $size = ([int64]$raw[4] -shl 24) -bor ([int64]$raw[5] -shl 16) -bor ([int64]$raw[6] -shl 8) -bor [int64]$raw[7]
            $nameBytes = New-Object System.Collections.Generic.List[byte]
            while ($true) { $b = $buffered.ReadByte(); if ($b -lt 1) { break }; $nameBytes.Add([byte]$b) }
            $name = $script:Latin1.GetString($nameBytes.ToArray()).ToLowerInvariant().Replace('/', '\')
            if ($Wanted -contains $name) { $table.Add([pscustomobject]@{ Name = $name; Off = $off; Size = $size }) }
        }
        foreach ($e in $table) {
            [void]$stream.Seek($e.Off, 'Begin')
            $data = New-Object byte[] ([int]$e.Size)
            $got = 0
            while ($got -lt $data.Length) { $n = $stream.Read($data, $got, $data.Length - $got); if ($n -lt 1) { throw "truncated BIG entry $($e.Name)" }; $got += $n }
            $result[$e.Name] = $data
        }
    } finally { $stream.Dispose() }
    return $result
}

function ConvertTo-LfText([byte[]]$Bytes) { return $script:Latin1.GetString($Bytes).Replace("`r`n", "`n").Replace("`r", "`n") }
function ConvertTo-CrlfBytes([string]$Text) { return ,$script:Latin1.GetBytes($Text.Replace("`n", "`r`n")) }
function Get-Slice($Items, [int]$From, [int]$To) { if ($To -le $From) { return @() } else { return @($Items[$From..($To - 1)]) } }

function New-ClonedButton([string]$IniText, [string]$Source, [string]$NewName) {
    $m = [regex]::Match($IniText, "\nCommandButton " + [regex]::Escape($Source) + "\n([\s\S]*?)\nEnd\b")
    if (-not $m.Success) { throw "retail command button not found: $Source" }
    $body = $m.Groups[1].Value
    $body = [regex]::Replace($body, '(?m)^(\s*Command\s*=\s*)SPECIAL_POWER\b', '${1}SPECIAL_POWER_FROM_COMMAND_CENTER')
    if (-not $body.Contains('SPECIAL_POWER_FROM_COMMAND_CENTER')) { throw "$Source is not a plain SPECIAL_POWER button" }
    $opt = [regex]::Match($body, '(?m)^(\s*)Options(\s*=\s*)([^\n;]*)')
    if ($opt.Success) {
        if (-not $opt.Groups[3].Value.Contains('NEED_SPECIAL_POWER_SCIENCE')) { $body = $body.Substring(0, $opt.Groups[3].Index) + 'NEED_SPECIAL_POWER_SCIENCE ' + $body.Substring($opt.Groups[3].Index) }
    } else {
        $re = New-Object Text.RegularExpressions.Regex ('(?m)^(\s*SpecialPower\s*=[^\n]*)\n')
        $body = $re.Replace($body, ('${1}' + "`n  Options           = NEED_SPECIAL_POWER_SCIENCE`n"), 1)
    }
    return "CommandButton $NewName`n$body`nEnd`n"
}

function Update-CommandButtons([string]$IniText) {
    $out = $IniText.TrimEnd("`n") + "`n"
    $out += "`n; ---- X64.32 modern extension: FromShortcut variants cloned from the retail Generals buttons ----`n"
    foreach ($pair in $script:NewButtons) {
        if ([regex]::IsMatch($IniText, "\nCommandButton " + [regex]::Escape($pair[0]) + "\n")) { throw "already defined: $($pair[0])" }
        $out += "`n" + (New-ClonedButton $IniText $pair[1] $pair[0])
    }
    return $out
}

function Update-CommandSets([string]$IniText) {
    $out = $IniText
    foreach ($set in $script:SetAdditions) {
        $m = [regex]::Match($out, '(CommandSet ' + $set[0] + "\n)([\s\S]*?)(\nEND\b)", 'IgnoreCase')
        if (-not $m.Success) { throw "retail command set not found: $($set[0])" }
        $used = @{}
        foreach ($u in [regex]::Matches($m.Groups[2].Value, '(?m)^\s*(\d+)\s*=')) { $used[[int]$u.Groups[1].Value] = $true }
        $lines = ''
        foreach ($add in $set[1]) {
            if ($used.ContainsKey([int]$add[0])) { throw "$($set[0]) slot $($add[0]) already used" }
            $lines += "`n  $($add[0]) = $($add[1])"
        }
        $at = $m.Groups[2].Index + $m.Groups[2].Length
        $out = $out.Substring(0, $at) + $lines + $out.Substring($at)
    }
    return $out
}

function Update-PlayerTemplate([string]$IniText) {
    $out = $IniText
    foreach ($bar in $script:Bars) {
        $m = [regex]::Match($out, '(?m)^(\s*Side\s*=\s*' + $bar[1] + '\s*(?:;[^\n]*)?\n)')
        if (-not $m.Success) { throw "player template side not found: $($bar[1])" }
        $rest = $out.Substring($m.Index + $m.Length)
        $c = [regex]::Match($rest, '(?m)^(\s*SpecialPowerShortcutButtonCount\s*=\s*)(\d+)')
        $nxt = [regex]::Match($rest, '(?m)^\s*Side\s*=')
        if (-not $c.Success -or ($nxt.Success -and $c.Index -gt $nxt.Index)) { throw "ButtonCount not found for $($bar[1])" }
        $at = $m.Index + $m.Length + $c.Groups[2].Index
        $out = $out.Substring(0, $at) + [string]$bar[2] + $out.Substring($at + $c.Groups[2].Length)
    }
    return $out
}

function New-ShortcutWnd([string]$RetailText, [int]$Slots, [int]$Bottom) {
    $lines = $RetailText.Split([char]10)
    $starts = @(); $ends = @()
    for ($i = 0; $i -lt $lines.Length; $i++) { if ($lines[$i] -ceq '  CHILD') { $starts += $i }; if ($lines[$i] -ceq '  ENDALLCHILDREN') { $ends += $i } }
    if ($starts.Count -eq 0 -or $ends.Count -eq 0) { throw 'unexpected WND structure' }
    $end = ($ends | Measure-Object -Maximum).Maximum
    $tplEnd = if ($starts.Count -gt 1) { $starts[1] } else { $end }
    $head = Get-Slice $lines 0 $starts[0]
    $template = Get-Slice $lines $starts[0] $tplEnd
    $tail = Get-Slice $lines $end $lines.Length
    $top = $Bottom - $script:SlotH - $script:SlotPitch * ($Slots - 1)
    $rect = New-Object Text.RegularExpressions.Regex ('UPPERLEFT: \d+ \d+,\n(\s*)BOTTOMRIGHT: \d+ \d+,')
    $headText = $head -join "`n"
    if (-not $rect.IsMatch($headText)) { throw 'bar parent rectangle not found' }
    $headText = $rect.Replace($headText, ("UPPERLEFT: {0} {1},`n" -f $script:SlotX, $top) + '${1}' + ("BOTTOMRIGHT: {0} {1}," -f ($script:SlotX + $script:SlotW), $Bottom), 1)
    $blocks = @()
    for ($k = 1; $k -le $Slots; $k++) {
        $y = $top + $script:SlotPitch * ($k - 1)
        $rects = @(@($script:SlotX, $y, ($script:SlotX + $script:SlotW), ($y + $script:SlotH)),
                   @(($script:SlotX + $script:CmdDx), ($y + $script:CmdDy), ($script:SlotX + $script:CmdDx + $script:CmdW), ($y + $script:CmdDy + $script:CmdH)))
        $text = $template -join "`n"
        $hits = @(0)
        $evaluator = [Text.RegularExpressions.MatchEvaluator]{
            param($m)
            $r = $hits[0]; $hits[0] = $hits[0] + 1
            $p = $rects[[Math]::Min($r, 1)]
            return ("UPPERLEFT: {0} {1},`n{2}BOTTOMRIGHT: {3} {4}," -f $p[0], $p[1], $m.Groups[1].Value, $p[2], $p[3])
        }
        $text = $rect.Replace($text, $evaluator)
        if ($hits[0] -ne 2) { throw 'slot template must hold a parent and a command rectangle' }
        $text = [regex]::Replace($text, 'ButtonParent\d+', "ButtonParent$k")
        $text = [regex]::Replace($text, 'ButtonCommand\d+', "ButtonCommand$k")
        $blocks += $text
    }
    return $headText + "`n" + ($blocks -join "`n") + "`n" + ($tail -join "`n")
}

# Returns an ordered list of [pscustomobject]@{ Path = 'Data\INI\CommandButton.ini'; Bytes = ... } generated from the player's INI.big and Window.big.
function Build-GeneralsModernExtensions([string]$GameFolder) {
    $ini = Read-BigEntries (Join-Path $GameFolder 'INI.big') @('data\ini\commandbutton.ini', 'data\ini\commandset.ini', 'data\ini\playertemplate.ini')
    $wanted = @($script:Bars | ForEach-Object { 'window\' + $_[0].ToLowerInvariant() })
    $win = Read-BigEntries (Join-Path $GameFolder 'Window.big') $wanted
    foreach ($n in @('data\ini\commandbutton.ini', 'data\ini\commandset.ini', 'data\ini\playertemplate.ini')) { if (-not $ini.ContainsKey($n)) { throw "INI.big has no $n" } }
    $files = New-Object System.Collections.Generic.List[object]
    $files.Add([pscustomobject]@{ Path = 'Data\INI\CommandButton.ini'; Bytes = (ConvertTo-CrlfBytes (Update-CommandButtons (ConvertTo-LfText $ini['data\ini\commandbutton.ini']))) })
    $files.Add([pscustomobject]@{ Path = 'Data\INI\CommandSet.ini'; Bytes = (ConvertTo-CrlfBytes (Update-CommandSets (ConvertTo-LfText $ini['data\ini\commandset.ini']))) })
    $files.Add([pscustomobject]@{ Path = 'Data\INI\PlayerTemplate.ini'; Bytes = (ConvertTo-CrlfBytes (Update-PlayerTemplate (ConvertTo-LfText $ini['data\ini\playertemplate.ini']))) })
    foreach ($bar in $script:Bars) {
        $key = 'window\' + $bar[0].ToLowerInvariant()
        if (-not $win.ContainsKey($key)) { throw "Window.big has no $key" }
        $files.Add([pscustomobject]@{ Path = ('Window\' + $bar[0]); Bytes = (ConvertTo-CrlfBytes (New-ShortcutWnd (ConvertTo-LfText $win[$key]) $bar[2] $bar[3])) })
    }
    return $files.ToArray()
}
