# X64.16: guarded native window/device/clear-present gate

Date: 2026-10-12. Implemented from accepted X64.15 baseline
`e0cfdad2f46cf98fb4b0e73e223e0997303a65db` on dev/modern-engine.
Neither Generals nor Zero Hour was launched during implementation/validation.

## Implemented route and boundaries

Both GameMain implementations retain headless engine initialization. Only an
AMD64 process with exactly `-headless -nativeGraphicsSmoke <isolated-run-root>`
can enter the new branch, after TheGameEngine->init and before the mutually
exclusive replay/execute branches. The existing nativeInitOnly grammar, branch,
launcher and success criteria are retained. Win32 rejects the new option.
Normal/default renderer selection and DX8Backend.cpp are unchanged.

The shared NativeGraphicsSmoke host lives in the existing independent
w3d_d3d11 library. It creates its own checked window class/HWND/WndProc,
800x600 client area, fixed windowed style and bounded PeekMessage pump. It does
not use ApplicationHWnd, the engine WndProc, input/window/display subsystems or
GameEngine execute/update. No replay, simulation, GameLogic, shell/menu/world,
DX8 emulation or factory substitution is entered. The headless subsystems stay
headless; this is a diagnostic host, not an integrated game renderer.

The host calls D3D11Backend::Create directly with software=false. The existing
D3D11CreateDeviceAndSwapChain call selects D3D_DRIVER_TYPE_HARDWARE; there is no
WARP retry. It owns the device/context, windowed single-sample RGBA8 discard swap
chain, backbuffer/RTV and existing shader/state setup. DXGI Alt-Enter/window
changes are suppressed only for the diagnostic configuration. Each iteration
sets viewport, Begin_Scene, clears color to RGBA=(0.08,0.32,0.55,1), then
End_Scene(true)/Present(0,0). Depth clear and geometry draws are not requested.

The default loop holds for 5000ms after backend setup, sleeps20ms per frame and
permits at most300 frames; completion beyond6000ms fails. At least one visible,
non-minimized, unoccluded S_OK Present and current visible/S_OK state are required.
Close/Escape/WM_QUIT, unexpected non-minimized resize, failed Win32 calls,
message bursts over256 per iteration and failed D3D operations fail closed.
A blocked driver/API is bounded externally by the launcher120s watchdog;
forced termination is failure and does not prove cleanup. No CLI size, driver,
frame-count or duration overrides are provided. Hidden/WARP options exist only
in the isolated C++ test configuration, not the title command line.

Backend reset precedes DestroyWindow and UnregisterClass on success and caught
failure paths. Normal engine cleanup, MiniDumper shutdown, late-shutdown observer,
WinMain return and CRT/process teardown remain in place. Actual exit0 remains
mandatory. No destructor, crash support or launcher success check is suppressed.

## Diagnostics and evidence

Optional backend checkpoints distinguish device/swap-chain, adapter/feature/
vendor/device identity, DXGI window association, target/RTV, shader/state/resource
setup and initial viewport. Host checkpoints cover class/window, backend, first
clear/Present, message loop, backend/window/class release, bounded summary and
execution boundary. Win32 errors capture GetLastError; D3D exceptions identify
operation and hexadecimal HRESULT. Present stores its actual HRESULT,
DXGI_STATUS_OCCLUDED and failed-device removal reason before checking failure.
The first Present and first visible S_OK event are logged, followed by aggregate
frame/visible/occluded/elapsed counters. Existing flushed logging/2048-record
capacity is reused; the host does not log every frame.

Separate GraphicsCommon/Stage/Test/Invoke scripts reuse the existing native
PE/PDB identity, asset isolation, inventory, dependency and receipt infrastructure.
Original native-init scripts/Common remain unchanged. Graphics stages add an
immutable graphics.json bound to the SHA256 of manifest.json; they do not rewrite
the immutable base manifest. Preflight verifies current build/stage EXE/PDB
hashes and GUID/age, assets, system AMD64 imports, d3d11.dll, dxgi.dll and
D3DCOMPILER_47.dll. No app-local DLLs are allowed.

The new launcher requires -RunReviewedGraphicsSmoke, refuses existing games and
creates a fresh isolated run/userdata/CrashDumps directory. launch.json records
exact args, module/CWD/user-data/dump/base-asset paths, hashes, symbols and
dependencies. exit.json records actual exit/timeout/error/PID, path agreement,
shutdown evidence length, dump count and detailed graphics-evidence verdict.
The classifier requires unique ordered phase completions, correct sequence/PID,
hardware identity, visible/unoccluded S_OK Present, bounded summary, execution
skip boundary, cleanup, armed late observer and final WinMain. Missing/malformed
logs, failure records, trailers and wrong modes fail closed; classification
errors are preserved in the exit receipt. Empty shutdown-exception.bin, no dump,
no timeout/error and actual exit0 are required in addition to those checkpoints.
API receipts alone do not establish a developer-observed visible-frame PASS.

## Validation actually run

- Full Win32 Release build PASS. Full CTest **25/25 PASS**, including live legacy
  resource/filter/DXT reference contracts, FP, pool and renderer tests.
- Direct Win32 title GoogleTest: **149/149 each**, with6 existing disabled tests
  each. Focused preservation selection: **75/75 each**.
- Native Release build/check of BOTH g_generals and z_generals PASS; both AMD64
  EXEs link without unresolved symbols. Independent AMD64 CTest **15/15 PASS**.
  Native full-title GoogleTest targets remain excluded as previously documented;
  this stage does not claim they passed.
- New isolated graphics host **5/5 tests per architecture**: completion/bounds
  decisions, real hidden WARP Present without visible-success inference, Close
  abort, resize rejection and release order, invalid limits without creation.
  Backend sentence target **8/8**, UI target **9/9** per architecture. Existing
  real WARP upload/draw/readback and hidden resize/Present contracts remain green.
- Startup-gate tests **7 AMD64 / 6 Win32**, FP **4 each**, pool **7 each** PASS.
  Source preservation **9/9**, rendering Python **28/28**, performance Python
  **16/16** PASS. Native workflow **6 checks**, graphics synthetic evidence/
  guard/dependency workflow **19 checks**, all10 native PowerShell scripts parsed.
- Both actual AMD64 import tables: **30 contracts, no delay imports**, D3D11 and
  D3DCOMPILER_47 now live, **0 external D3DX helpers and no D3D8 import**.
  DXGI is obtained through the device; its system component is checked explicitly.
  Win32 actual EXEs retain27 imports each, with no D3D11/DXGI/compiler import.
- All12 preserved X64.14 reviewed evidence/EXE/PDB hashes remain unchanged.
  git diff --check PASS. Build logs, stages, receipts and generated analysis are
  ignored and excluded from the source commit.

## Fresh stages and identities

Both **x64-16-final-2** stages were prepared only after final build/test validation,
then Stage and read-only Test preflight passed for both titles. Neither has a
reviewed run yet. An initial staging-wrapper attempt at x64-16-final stopped at
immutable-manifest protection; those incomplete stages have no graphics.json
and are not launch candidates. Use only final-2 below.

| Title | EXE/PDB GUID and age | EXE SHA256 | PDB SHA256 |
|---|---|---|---|
| Generals | c7323867-d82e-41f3-b190-20a5da4eae06 /47 | 0916328600F6484ACC96B8B8B13EBE541E32D222104C6BBE14C635B789674888 | 983E9B0F26EDFC9F89AD62B15136E7E20919EBBAF3B7F4A00507BA95FB855E40 |
| Zero Hour | 7a285453-4110-4cad-a6c8-e5336ba19b1c /48 | 2B8627EE0384A79532E6F421E2E20F62FA7814AD50340A172324C521790C5699 | E27E42FD0591F13E7843A6310D978F016A2E830AE4AB5DF6BA76019FCA3D8876 |

## First separately reviewed developer run: Generals only

After developer review, in native PowerShell7, with both games closed:

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-16-final-2
./scripts/native-runtime/Invoke-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-16-final-2 -RunReviewedGraphicsSmoke
```

Do not rebuild/re-stage between preflight and this run. Preflight never launches.
The second command launches exactly one process; no automatic retry/next title.
Following headless initialization, expect an800x600 blue diagnostic window titled
"Native AMD64 D3D11 smoke - no gameplay" for about5 seconds, then automatic
normal cleanup/exit. No click/key press is needed. Keep it visible and do not
minimize/close/resize; Escape/Close aborts. No menus or gameplay should appear.

Preserve the printed fresh runs directory, including launch.json, exit.json,
startup.log, shutdown-exception.bin (even empty), userdata/CrashDumps and any
additional failure files. Report whether the blue client area was actually
visible, its approximate duration and any flicker/error. A launcher PASS must
be independently verified from these files plus that visual observation before
claiming a first visible native frame. On failure stop; do not rerun automatically
or launch Zero Hour. The already prepared Zero Hour stage stays held for review.

## Remaining limitations and exactly one next task

Hardware device creation/visible presentation inside either title is untested.
Ordinary graphical startup still selects DX8 and is blocked by unavailable
native D3D8. Shell/menu/world integration, engine input/window callbacks,
device-loss recovery and full renderer resources remain later work. No execute,
simulation, gameplay, save/load, replay, multiplayer, performance or determinism
claim follows from this diagnostic gate or the earlier headless milestones.

**Exactly one next task: separately reviewed Generals-only native graphical
smoke confirmation using x64-16-final-2, with evidence and visual verification.**
Do not begin another renderer implementation or a Zero Hour run before review.
