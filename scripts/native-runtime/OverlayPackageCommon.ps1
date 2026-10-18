#Requires -Version 7.0
# X64.48B: small PE helpers shared by the overlay package builder and validator (no external tools; the import table is read from the file).
function Get-PeMachineFromFile([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    return Get-PeMachine $bytes
}
function Get-PeMachine([byte[]]$Bytes) {
    $pe = [BitConverter]::ToInt32($Bytes, 0x3c)
    return [BitConverter]::ToUInt16($Bytes, $pe + 4)
}
# The DLL names a PE32+ image imports (static import table), derived from the file - never guessed.
function Get-PeImportNamesFromBytes([byte[]]$d) {
    $pe = [BitConverter]::ToInt32($d, 0x3c)
    $sections = [BitConverter]::ToUInt16($d, $pe + 6)
    $optSize = [BitConverter]::ToUInt16($d, $pe + 20)
    $importRva = [BitConverter]::ToUInt32($d, $pe + 24 + 112 + 8)           # PE32+ data directory 1
    $table = $pe + 24 + $optSize
    $map = @(for ($i = 0; $i -lt $sections; $i++) {
        $o = $table + $i * 40
        [pscustomobject]@{ Va = [BitConverter]::ToUInt32($d, $o + 12); Size = [BitConverter]::ToUInt32($d, $o + 8); Raw = [BitConverter]::ToUInt32($d, $o + 20) } })
    function ToOffset([uint32]$Rva) { foreach ($s in $map) { if ($Rva -ge $s.Va -and $Rva -lt $s.Va + [Math]::Max($s.Size, 1)) { return [int]($Rva - $s.Va + $s.Raw) } } }
    $names = @()
    $o = ToOffset $importRva
    while ($true) {
        $nameRva = [BitConverter]::ToUInt32($d, $o + 12)
        if (-not $nameRva) { break }
        $s = ToOffset $nameRva
        $e = [Array]::IndexOf($d, [byte]0, $s)
        $names += [Text.Encoding]::ASCII.GetString($d, $s, $e - $s)
        $o += 20
    }
    return $names
}
function Get-PeImportNames([string]$Path) { return Get-PeImportNamesFromBytes ([IO.File]::ReadAllBytes($Path)) }
