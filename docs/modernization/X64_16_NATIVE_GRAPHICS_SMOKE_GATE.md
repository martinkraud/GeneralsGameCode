# X64.16: guarded native window/device/clear-present gate

## Both titles X64.16 hardware visible-presentation/process PASS (2026-10-12)

Zero Hour confirmation `20261012T1149261127844Z` under
`build/dev-runtimes/native/ZeroHour/x64-16-final-2/runs` is verified directly.
Complete exit.json: PID29152, actual exit0, Started/Success/PathsMatch true,
no timeout/error. Independent classifier verifies333 sequential zero-code
records, explicit GeneralsMD AMD64 graphics-smoke mode, Intel Graphics8086:7D45,
feature0xB000,800x600 backbuffer/RTV/setup/viewport and clear. First Present315
is visible/non-minimized/unoccluded S_OK. Summary323:240 visible frames,
0 occluded,5000ms, hardware. Backend release318 precedes HWND320/class322;
boundary325 skips execute/replay/simulation; engine cleanup329, MiniDumper331,
late observer332 and final WinMain333 complete. Observer file0 bytes, no trailer,
CrashDumps empty. Launch/log/exit isolated paths match. Actual EXE/PDB/stage hashes
and GUID7a285453-4110-4cad-a6c8-e5336ba19b1c/age48 match preflight/reviewed identity.
Developer confirms normal blue presentation and automatic close without anomaly.

Together with Generals confirmation20261012T1105248762814Z (PID19592, exit0,
239 visible frames/5000ms), **both native AMD64 hardware-D3D11 diagnostic visible
presentation, bounded cleanup and normal OS exit are PASS; X64.16 is closed.**
This does not validate ordinary graphical startup, integrated renderer, original
shell/menu/world, engine input, execute/simulation, gameplay/save/replay/network,
performance or determinism. Original four files per confirmation, earlier three
Generals run files and twelve X64.14 evidence/symbol files remain immutable.

Offline evidence/source review and both existing stage preflights PASS; graphics
workflow25, native workflow6/all10 scripts parsed, startup contracts9 and rendering
reference28 PASS. No rebuild/restage/game launch. No production changes in this
milestone commit; generated evidence and analysis are ignored/uncommitted.

Autonomous continuation: developer selected **retain shared W3DDisplay/WW3D and
add explicit renderer-neutral lifecycle adapters**, preserving DX8/Win32. Next
slice is bounded display-owned presentation lifecycle integration without shell,
world, engine update or simulation. Neither accepted title needs another X64.16
confirmation. Earlier next-step entries are historical/superseded.

## Generals X64.16 reviewed visible-frame/process PASS (2026-10-12)

Accepted confirmation run:
`build/dev-runtimes/native/Generals/x64-16-final-2/runs/20261012T1105248762814Z`.
Both original receipts were inspected directly. exit.json is complete: PID19592,
Started=true, TimedOut=false, ExitCode=0, Error=null, Success=true, PathsMatch=true.
Independent offline classification agrees with its286-record evidence verdict.
No receipt was reconstructed or amended. This closes the missing OS-exit gate
from the earlier run; that earlier evidence and its limitation remain historical.

All286 startup.log records have sequential numbering, matching PID19592, zero
codes and no failed stage/trailer. launch.json has exactly the reviewed headless/
nativeGraphicsSmoke/run-root arguments; WinMain explicitly identifies graphics
smoke. All required initialization phases finish. Window completion251 reports
800x600/visible; hardware device254 reports Intel(R) Graphics, vendor8086,
device7D45, feature0xB000 (D3D11 feature level11.0). Window association256,
backbuffer/RTV258, setup260 and viewport262 complete; clear266 completes.
First Present268 is HRESULT0x00000000, visible1, occluded0, removed0.
The matching source computes visible from IsWindowVisible && !IsIconic;
summary276 reports239 visible frames,0 occluded and5000ms, driverhardware.

Backend release271 precedes HWND273 and class275 release. Host277 and boundary278
complete with execute/replay/simulation skipped. Engine cleanup282, MiniDumper
shutdown284, late observer armed285 and final WinMain286 all complete with code0.
shutdown-exception.bin is0 bytes, CrashDumps exists and is empty, and there is no
late exception trailer. Exact module/CWD/user-data/dump paths agree with launch,
exit and log. Actual staged EXE/PDB hashes match launch/stage/current build;
preflight independently verifies PE/PDB GUID c7323867-d82e-41f3-b190-20a5da4eae06,
age47, and all asset/inventory/dependency checks. Hashes remain those listed below.

Developer confirms the expected blue client area, normal visible rendering for
the expected duration, automatic close and no flicker/error/unexpected behavior.
**PASS: Generals native AMD64 hardware-D3D11 diagnostic visible presentation,
bounded pump, cleanup and normal OS process exit.** This is the diagnostic route
after headless init, not an integrated game renderer or ordinary graphical startup.
It does not validate shell/menu/world rendering, game input, execute/replay/
simulation, gameplay/save/load/multiplayer, performance or determinism.

## Authorized offline continuation and Zero Hour readiness

The existing ZeroHour/x64-16-final-2 stage requires no implementation/rebuild/
restaging. Read-only graphics preflight PASS, including unchanged full inventory,
EXE/PDB hashes and GUID7a285453-4110-4cad-a6c8-e5336ba19b1c/age48. Both title
actual AMD64 imports remain30, no delay imports, no D3D8/D3DX; system D3D11/DXGI/
compiler checks pass. Zero Hour has no runs directory and no graphical runtime
claim. Shared gate and fixed launcher are already implemented/validated; no
speculative integration work is required before its reviewed confirmation.

Offline checks rerun: graphics workflow25, native workflow6/all10 scripts parsed,
startup source contracts9/9, both stage preflights and git diff --check PASS.
This task is documentation-only; no game execution, source change, rebuild or
restaging. Earlier Win32/AMD64 build/reference/link results remain the unchanged
validated baseline and are not claimed as newly rerun. All4 confirmation files,
all3 earlier graphical-run files and all12 preserved X64.14 evidence/symbol files
retain their original hashes. Generated preservation manifests remain ignored.

Exactly one next task: **a single separately reviewed Zero Hour X64.16 graphical
smoke confirmation**. Offline continuation stops at this mandatory execution/
visual-review boundary; no renderer integration work begins before both title
gates are accepted. The following commands are for the developer after review,
not authorization for an agent to launch:

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeGraphicsSmoke.ps1 -Title ZeroHour -Name x64-16-final-2
./scripts/native-runtime/Invoke-NativeGraphicsSmoke.ps1 -Title ZeroHour -Name x64-16-final-2 -RunReviewedGraphicsSmoke
```

Do not rebuild/restage between preflight and run. Expect the same800x600 blue
window for about5 seconds then automatic cleanup/exit, with no menu/gameplay.
No input is needed; keep it visible, do not minimize/resize/close. Record the
visual result. Preserve the fresh printed
`build/dev-runtimes/native/ZeroHour/x64-16-final-2/runs/<UTC>` directory, including
launch.json, exit.json, startup.log, shutdown-exception.bin even empty, and
userdata/CrashDumps. On any failure, missing receipt or visual anomaly, preserve
all evidence and stop: no automatic rerun, next trial or Generals launch. Both
visual and full machine evidence must be reviewed before the combined milestone.
This recommendation supersedes the earlier next-step entries below.


## Reviewed Generals visible frame; launcher receipt fix (2026-10-12)

Reviewed run: `build/dev-runtimes/native/Generals/x64-16-final-2/runs/20261012T1007219135054Z`.
Developer saw the expected blue diagnostic client area for about5 seconds and
automatic close. Direct offline inspection verifies286 sequential PID25496
records, no failure/nonzero checkpoint/trailer, explicit graphics-smoke mode,
matching module/CWD/user-data/dump paths and reviewed EXE/PDB identity. Hardware
Intel(R) Graphics (8086:7D45), feature0xB000; backbuffer/RTV/setup/viewport and
clear complete, first visible/unoccluded S_OK Present,210 visible frames,
0 occluded,5000ms. Backend/window/class release completed in that order,
execute/replay/simulation skipped, engine cleanup/MiniDumper shutdown complete,
late observer armed and final WinMain complete0. Observer evidence is empty and
CrashDumps is empty. This supports the first visible native AMD64 D3D11 frame.

**Full reviewed process PASS is NOT established:** exit.json is missing, not
partial. The launcher lost its in-memory OS exit/timeout/error values before
writing the receipt. Final WinMain0 is not proof of final OS exit0 after CRT
teardown; no scoped Windows process-exit event was available to recover it.
Do not infer exit0 from the Count error: synthetic reproduction shows the same
error with exit0 or exit1. No original evidence is reconstructed/overwritten.

Root cause in the accepted launcher: line57 assigns `$dumps=if(...){@(...)}else{@()}`.
The conditional pipeline enumerates the inner empty array, emitting nothing;
`$dumps` is therefore null for this exact empty CrashDumps directory. StrictMode
rejects `.Count` at line59 (success expression), or line64 (receipt construction
if a prior success condition was false). The retained error does not identify
which dereference threw; both precede Write-ZHJson execution. Only launch.json,
startup.log and the empty shutdown-exception.bin exist; no exit.json was written.
A single emitted FileInfo also needs collection normalization.

The smallest fix wraps the ENTIRE conditional in `@(...)`, retaining an array
for zero/one/multiple files. No success conditions, engine code, D3D11 code,
binary, PDB, stage or nativeInitOnly script changes. Regression executes the
actual production post-processing block against six isolated synthetic fixtures
(0/1/2 dumps x exit0/1), verifies receipt counts/exit/verdict, and reproduces the
legacy null/Count failure before receipt writing for both exit outcomes. Fixed
zero-dump exit0 succeeds; nonzero exit and any dump still fail.

Offline validation: graphics workflow25 checks, native workflow6 checks/all10
scripts parsed, startup source contracts9/9, Generals stage preflight PASS,
git diff --check PASS. No rebuild/restage or game launch; native regression/link
results above remain the unchanged binary baseline, not newly rerun claims.
Original three run-file SHA256 hashes, directory inventory and matching staged
EXE/PDB remain unchanged. Preservation/analysis files stay ignored outside the run.

**Exactly one next task:** developer review/authorization of a Generals-only
confirmation using the fixed launcher and the unchanged x64-16-final-2 stage,
to retain actual OS exit0 and complete the process gate. Do not execute now,
retry automatically or launch Zero Hour. The visible-frame subgate is verified;
the complete runtime milestone waits for the missing exit evidence. This
supersedes the earlier recommended-next-task entry below.


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
