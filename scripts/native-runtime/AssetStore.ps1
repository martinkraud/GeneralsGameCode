#Requires -Version 7.0
# Content-addressed store of the IMMUTABLE retail assets (.big archives, .bik movies) that every staged runtime needs.
#
# Why: each staged candidate used to carry a private copy of ~1.5 GB (Generals) / ~2.7 GB (Zero Hour) of assets that never change; eleven stages were 32 GB of identical bytes.
# Now a stage file is an NTFS hard link to one store entry named by its SHA-256 (build\dev-runtimes\asset-store\<SHA256>.asset). A stage costs only its executables, symbols and small
# files; removing a stage never deletes shared content; the store is pruned of entries that no stage links to any more (Optimize-StageStorage.ps1 -PruneStore).
#
# Safety properties:
#   * only files whose extension is in $script:AssetStoreExtensions and whose size is at least $script:AssetStoreMinBytes are ever linked (executables and PDBs are rewritten in place by
#     -Refresh style workflows and must never share storage);
#   * a link replaces a file only after the SHA-256 of the store entry equals the SHA-256 of the file it replaces;
#   * nothing outside <dev-runtimes>\native, <dev-runtimes>\zh and <dev-runtimes>\asset-store is touched; reparse points and Steam-looking paths are refused;
#   * if linking is impossible (different volume, no hard-link support) callers fall back to a normal copy; the stage is then merely larger.
$script:AssetStoreExtensions = @('.big', '.bik')
$script:AssetStoreMinBytes = 1MB

Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class StageFileIdentity
{
    [StructLayout(LayoutKind.Sequential)]
    struct FILETIME { public uint Low; public uint High; }
    [StructLayout(LayoutKind.Sequential)]
    struct BY_HANDLE_FILE_INFORMATION
    {
        public uint FileAttributes; public FILETIME CreationTime; public FILETIME LastAccessTime; public FILETIME LastWriteTime;
        public uint VolumeSerialNumber; public uint FileSizeHigh; public uint FileSizeLow; public uint NumberOfLinks; public uint FileIndexHigh; public uint FileIndexLow;
    }
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool GetFileInformationByHandle(SafeFileHandle handle, out BY_HANDLE_FILE_INFORMATION info);
    // "volume:indexHigh:indexLow" plus the number of hard links; two paths with the same identity are the same file on disk.
    public static string Get(string path, out uint links)
    {
        using (FileStream stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
        {
            BY_HANDLE_FILE_INFORMATION info;
            if (!GetFileInformationByHandle(stream.SafeFileHandle, out info)) throw new IOException("GetFileInformationByHandle failed for " + path);
            links = info.NumberOfLinks;
            return info.VolumeSerialNumber.ToString("X8") + ":" + info.FileIndexHigh.ToString("X8") + ":" + info.FileIndexLow.ToString("X8");
        }
    }
}
'@ -ErrorAction SilentlyContinue

function Get-StageFileIdentity([string]$Path) { [uint32]$links = 0; $id = [StageFileIdentity]::Get($Path, [ref]$links); return [pscustomobject]@{ Id = $id; Links = [int]$links } }

function Test-AssetStoreSteamPath([string]$Path) {
    return ($Path -match '(?i)[\\/]steamapps[\\/]') -or ($Path -match '(?i)[\\/]Steam[\\/]') -or ($Path -match '(?i)Program Files \(x86\)[\\/]Steam')
}

function Get-AssetStoreRoot([string]$DevRuntimes) { return (Join-Path $DevRuntimes 'asset-store') }

function Test-AssetStoreEligible([System.IO.FileInfo]$File) {
    return ($script:AssetStoreExtensions -contains $File.Extension.ToLowerInvariant()) -and $File.Length -ge $script:AssetStoreMinBytes -and -not ($File.Attributes -band [IO.FileAttributes]::ReparsePoint)
}

# Makes $Destination (not existing yet) a hard link to the store entry for the file at $Source, creating the entry from $Source when needed.
# $Sha256 is the verified hash of $Source. Returns $true when a link was made, $false when the caller should copy instead (not eligible / linking failed).
function New-StoredAssetLink([string]$Source, [string]$Destination, [string]$Sha256, [string]$DevRuntimes) {
    $file = Get-Item -LiteralPath $Source -Force
    if (-not (Test-AssetStoreEligible $file)) { return $false }
    $store = Get-AssetStoreRoot $DevRuntimes
    $entry = Join-Path $store ($Sha256.ToUpperInvariant() + '.asset')
    try {
        New-Item -ItemType Directory -Force -Path $store | Out-Null
        if (-not (Test-Path -LiteralPath $entry -PathType Leaf)) {
            $tmp = $entry + '.tmp-' + [guid]::NewGuid().ToString('N')
            Copy-Item -LiteralPath $Source -Destination $tmp
            if ((Get-FileHash -LiteralPath $tmp -Algorithm SHA256).Hash -ne $Sha256.ToUpperInvariant()) { Remove-Item -LiteralPath $tmp -Force; return $false }
            Move-Item -LiteralPath $tmp -Destination $entry
        }
        New-Item -ItemType HardLink -Path $Destination -Target $entry -ErrorAction Stop | Out-Null
        return $true
    } catch {
        if (Test-Path -LiteralPath $Destination -PathType Leaf) { Remove-Item -LiteralPath $Destination -Force -ErrorAction SilentlyContinue }
        return $false
    }
}

# Free-space guard shared by the staging, packaging and benchmark workflows: refuses to start when the drive of $Path has less than $RequiredGB free.
function Assert-FreeDiskSpace([string]$Path, [double]$RequiredGB, [string]$What = 'this workflow') {
    $full = [IO.Path]::GetFullPath($Path)
    $drive = Get-PSDrive -Name ([IO.Path]::GetPathRoot($full).Substring(0, 1)) -ErrorAction SilentlyContinue
    if (-not $drive) { return }
    $freeGB = $drive.Free / 1GB
    if ($freeGB -lt $RequiredGB) {
        throw ('Not enough free disk space for {0}: {1:N1} GB free on {2}:, {3:N1} GB required. Run scripts\Clean-DevelopmentArtifacts.ps1 (dry run first) or scripts\native-runtime\Optimize-StageStorage.ps1 -PruneStore.' -f $What, $freeGB, $drive.Name, $RequiredGB)
    }
}
