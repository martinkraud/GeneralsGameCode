#Requires -Version 7.0
# X64.52B: REAL-input camera test of a STAGED native candidate (never a Steam directory).
#
# The X64.49 probe (Invoke-CameraProbe.ps1) drives the translator's scroll STATE directly, so it can never notice a broken keyboard / mouse path (it did not notice
# the dead screen-edge scrolling of the X64.52A candidate). This script launches the candidate with GENERALS_CAMERA_PROBE_REAL=1 (the game injects nothing and only
# records the camera of every drawn frame + samples what the look-at translator sees) and sends REAL operating-system input with SendInput:
#   phase KeyHold : the Left arrow key is held
#   phase EdgeHold: the cursor is moved to the RIGHT edge of the client area and left there (the camera travels back over the key phase's way)
#   phase RmbHold : the right mouse button is held while the mouse moves steadily
# then it prints the per-phase summary the game wrote. The window must be allowed to take the foreground; do not touch the keyboard / mouse while it runs (about 40 s).
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals', 'ZeroHour')][string]$Title,
      [string]$Candidate,
      [string]$ExePath,                # alternatively: an installed game's exe (e.g. the Steam install with the overlay ZIP applied) - it is only launched, nothing in it is written by this script
      [string]$Label = 'real',
      [int]$Fps = 120,
      [double]$HoldSeconds = 1.5,
      [switch]$Legacy,                 # run the former post-draw, previous-interval scroll step (proves the gate catches the pre-X64.49 judder)
      [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
$repo = $script:ZHRepository
if ($ExePath) {
    if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) { throw "No such executable: $ExePath" }
    $exe = (Resolve-Path -LiteralPath $ExePath).Path; $game = Split-Path $exe
} else {
    if (-not $Candidate) { throw 'Pass -Candidate <staged candidate> or -ExePath <installed game executable>.' }
    $manifest = Read-NativeOrdinaryPlayStage $Title $Candidate $script:NativeNormalLaunchGenerated -AllowCrashDumps
    $game = $manifest.GamePath
    $exe = Join-Path $game ([IO.Path]::GetFileName($manifest.Build.Executable))
}
$out = Join-Path $repo "build\diag\camera-real-$Title-$Label"
Remove-Item -LiteralPath $out -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $out | Out-Null

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RealInput {
  [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr dwExtraInfo; }
  [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public IntPtr dwExtraInfo; }
  [StructLayout(LayoutKind.Explicit)] public struct U { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
  [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public U u; }
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int left, top, right, bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x, y; }
  [DllImport("user32.dll", SetLastError = true)] public static extern uint SendInput(uint n, INPUT[] inputs, int size);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr value);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
  [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h, uint flags);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  // Take the foreground even though this process does not own it (the foreground lock refuses a plain SetForegroundWindow from a background process).
  public static bool Foreground(IntPtr h) {
    uint pid; uint fg = GetWindowThreadProcessId(GetForegroundWindow(), out pid); uint me = GetCurrentThreadId();
    uint target = GetWindowThreadProcessId(h, out pid);
    ShowWindow(h, 9);
    if (fg != me) AttachThreadInput(me, fg, true);
    if (target != me && target != fg) AttachThreadInput(me, target, true);
    BringWindowToTop(h); bool ok = SetForegroundWindow(h);
    if (target != me && target != fg) AttachThreadInput(me, target, false);
    if (fg != me) AttachThreadInput(me, fg, false);
    return ok && GetForegroundWindow() == h;
  }
  public static bool OwnsPoint(IntPtr h, int x, int y) { POINT p = new POINT(); p.x = x; p.y = y; return GetAncestor(WindowFromPoint(p), 2) == h; }
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
  public static void Key(ushort scan, bool up, bool extended) {
    INPUT[] a = new INPUT[1]; a[0].type = 1; a[0].u.ki.wScan = scan;
    a[0].u.ki.dwFlags = 8u | (up ? 2u : 0u) | (extended ? 1u : 0u);      // KEYEVENTF_SCANCODE | KEYUP | EXTENDEDKEY
    SendInput(1, a, Marshal.SizeOf(typeof(INPUT)));
  }
  public static void MouseButton(bool right, bool up) {
    INPUT[] a = new INPUT[1]; a[0].type = 0; a[0].u.mi.dwFlags = right ? (up ? 0x0010u : 0x0008u) : (up ? 0x0004u : 0x0002u);
    SendInput(1, a, Marshal.SizeOf(typeof(INPUT)));
  }
  public static void MouseRel(int dx, int dy) {
    INPUT[] a = new INPUT[1]; a[0].type = 0; a[0].u.mi.dx = dx; a[0].u.mi.dy = dy; a[0].u.mi.dwFlags = 1u;   // MOVE (relative)
    SendInput(1, a, Marshal.SizeOf(typeof(INPUT)));
  }
}
'@

# Physical pixels everywhere: a DPI-unaware script would get 1536x960 for a 1920x1200 window on a 125 % desktop and park the cursor in the wrong place (found while re-running the gate).
[void][RealInput]::SetProcessDpiAwarenessContext([IntPtr]::new(-4))
function Get-Client($hwnd) {
    $r = New-Object RealInput+RECT; [void][RealInput]::GetClientRect($hwnd, [ref]$r)
    $p = New-Object RealInput+POINT; $p.x = 0; $p.y = 0; [void][RealInput]::ClientToScreen($hwnd, [ref]$p)
    [pscustomobject]@{ X = $p.x; Y = $p.y; W = $r.right - $r.left; H = $r.bottom - $r.top }
}

if (@(Get-Process -Name generals, generalsv, generalszh -ErrorAction SilentlyContinue).Count) { throw 'Exit all existing games first' }
$names = 'GENERALS_AUTOSHOT', 'GENERALS_AUTOSHOT_FRAMES', 'GENERALS_AUTOFLOW', 'GENERALS_CAMERA_PROBE', 'GENERALS_CAMERA_PROBE_FPS', 'GENERALS_CAMERA_PROBE_HOLD', 'GENERALS_CAMERA_PROBE_REAL', 'GENERALS_CAMERA_PROBE_VSYNC', 'GENERALS_CAMERA_PROBE_LEGACY'
$saved = @{}; foreach ($n in $names) { $saved[$n] = [Environment]::GetEnvironmentVariable($n) }
$env:GENERALS_AUTOSHOT = $out; $env:GENERALS_AUTOSHOT_FRAMES = '1000000'; $env:GENERALS_AUTOFLOW = 'skirmish'
$env:GENERALS_CAMERA_PROBE = $out; $env:GENERALS_CAMERA_PROBE_FPS = "$Fps"; $env:GENERALS_CAMERA_PROBE_HOLD = "$HoldSeconds"; $env:GENERALS_CAMERA_PROBE_REAL = '1'; $env:GENERALS_CAMERA_PROBE_VSYNC = '0'; $env:GENERALS_CAMERA_PROBE_LEGACY = $(if ($Legacy) { '1' } else { '' })
$phaseFile = Join-Path $out 'camera-probe-phase.txt'
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory $game -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $inconclusive = $false; $seen = 0; $held = $null; $rmbDown = $false; $dir = 1; $rmbMoves = 0
    while (-not $p.HasExited -and [DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 8
        $p.Refresh()
        $hwnd = $p.MainWindowHandle
        $lines = if (Test-Path -LiteralPath $phaseFile) { @(Get-Content -LiteralPath $phaseFile -ErrorAction SilentlyContinue) } else { @() }
        $phase = $null
        foreach ($l in $lines) { if ($l -match ' phase (\d+)$') { $phase = [int]$Matches[1] } }
        if ($phase -ne $null -and $phase -ne $held) {
            # leave the previous phase's input, enter the new one. Phase numbers: 2 key, 4 edge, 6 rmb (CameraProbe::Phase)
            if ($held -eq 2) { [RealInput]::Key(0x4B, $true, $true) }
            if ($held -eq 6 -and $rmbDown) { [RealInput]::MouseButton($true, $true); $rmbDown = $false }
            $held = $phase
            if ($hwnd -ne [IntPtr]::Zero) { $fg = [RealInput]::Foreground($hwnd); if (-not $fg) { Write-Host 'driver: WARNING the game window did not take the foreground' -ForegroundColor Yellow; $inconclusive = $true } }
            # outside the hold phases the cursor rests in the middle of the client area: a cursor left in a corner by an earlier run would edge-scroll the camera during the key phase
            if (($phase -in 1, 3, 5, 7) -and $hwnd -ne [IntPtr]::Zero) { $c = Get-Client $hwnd; [void][RealInput]::SetCursorPos($c.X + [int]($c.W / 2), $c.Y + [int]($c.H / 2)); [RealInput]::MouseRel(1, 0); [RealInput]::MouseRel(-1, 0) }
            if ($phase -eq 2) { [RealInput]::Key(0x4B, $false, $true) }
            elseif ($phase -eq 4 -and $hwnd -ne [IntPtr]::Zero) { $c = Get-Client $hwnd; [void][RealInput]::SetCursorPos($c.X + $c.W - 2, $c.Y + [int]($c.H / 2)); [RealInput]::MouseRel(3, 0) }
            elseif ($phase -eq 6 -and $hwnd -ne [IntPtr]::Zero) { $c = Get-Client $hwnd; [void][RealInput]::SetCursorPos($c.X + [int]($c.W / 2), $c.Y + [int]($c.H / 2)); Start-Sleep -Milliseconds 50; [RealInput]::MouseButton($true, $false); $rmbDown = $true; $rmbMoves = 0 }
            $cc = if ($hwnd -ne [IntPtr]::Zero) { Get-Client $hwnd } else { $null }; $owns = if ($cc) { [RealInput]::OwnsPoint($hwnd, $cc.X + [int]($cc.W / 2), $cc.Y + [int]($cc.H / 2)) } else { $false }; Write-Host "driver: entered phase $phase owns_centre=$owns hwnd=$hwnd client=$($cc.X),$($cc.Y) $($cc.W)x$($cc.H)"
        }
        if ($held -eq 6 -and $rmbDown -and $rmbMoves -lt 12) { [RealInput]::MouseRel(16 * $dir, 0); $rmbMoves++ }     # pull the mouse 192 px from the anchor, then hold it still: a constant scroll speed
        if ($held -eq 4 -and $hwnd -ne [IntPtr]::Zero) { $c = Get-Client $hwnd; [void][RealInput]::SetCursorPos($c.X + $c.W - 2, $c.Y + [int]($c.H / 2)) }   # keep it at the edge
    }
    if (-not $p.HasExited) { $p.Kill(); Write-Host 'TIMED OUT' -ForegroundColor Red }
}
finally {
    if ($rmbDown) { [RealInput]::MouseButton($true, $true) }
    [RealInput]::Key(0x4B, $true, $true)
    foreach ($n in $names) { [Environment]::SetEnvironmentVariable($n, $saved[$n]) }
}
$log = Join-Path $game 'generals64.log'
if (Test-Path -LiteralPath $log) { Copy-Item -LiteralPath $log -Destination (Join-Path $out 'generals64.log') -Force }
$summary = Join-Path $out 'camera-probe-summary.txt'
Write-Host "==== $Title real input, cap $Fps ===="
if (Test-Path -LiteralPath $summary) { Get-Content -LiteralPath $summary } else { Write-Host 'no summary written' -ForegroundColor Red }
if (Test-Path -LiteralPath $phaseFile) { Write-Host '---- translator state samples ----'; Get-Content -LiteralPath $phaseFile | Where-Object { $_ -match 'state' } | Select-Object -First 40 }
if ($inconclusive) {
    Write-Host 'INCONCLUSIVE: the game window could not take the foreground (locked workstation, screen saver or another window on top). The numbers above are NOT a camera result.' -ForegroundColor Yellow
    exit 3
}
