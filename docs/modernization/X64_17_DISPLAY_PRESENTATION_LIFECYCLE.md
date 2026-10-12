# X64.17: retained-display native presentation lifecycle

Date: 2026-10-12. Baseline: `c8aaccbee77e32f7ad21dbaa54682b7ea6537d3e`
on dev/modern-engine. X64.16 is closed for both titles. Developer explicitly
selected retaining shared W3DDisplay/WW3D ownership with renderer-neutral
lifecycle adapters instead of introducing a separate ModernDisplay frontend.

## Scope and actual route

This slice implements a **presentation-only attachment to the already initialized
headless W3DDisplay**, not ordinary graphical startup or a complete game renderer.
Both title GameMain bodies retain their normal engine init and cleanup and call:

`StartupDiagnostics::displaySmoke -> TheDisplay->runNativePresentationSmoke ->`
`W3DDisplayPresentation.cpp -> NativeGraphicsSmoke checked HWND/pump ->`
`W3DDisplay Driver::Initialize -> WW3D::Init_Native_Presentation ->`
`D3D11Backend::Create_For_W3D -> PresentationLifecycle owned backend`.

Each frame uses the same retained owner:

`host -> display Driver::BeginClear -> WW3D::Begin_Render ->`
`PresentationLifecycle::Begin -> Set_Viewport/Clear/Begin_Scene`, followed by
`Driver::EndPresent -> WW3D::End_Render -> PresentationLifecycle::End ->`
`End_Scene(Present)/Invalidate_Cached_Render_States`.

WW3D owns the neutral lifecycle and its backend; Get_Render_Backend exposes only
a borrowed alias. The host owns the checked HWND and message pump. W3DDisplay
borrows that HWND for attachment and does not take Windows destruction ownership.
Device/target resolution queries now truthfully use the active fixed target in
this mode. Display width/height/bits/windowed values are checked and restored on
detach. Qualified base setters avoid the existing graphical setters, which
would dereference the Render2D helper that is null in headless mode.

Cleanup is display detach -> clear WW3D alias -> owned backend/device release ->
restore display dimensions -> HWND -> class -> normal engine/MiniDumper/late
observer/WinMain/CRT. Partial attach and failed frame paths also detach before
HWND release. Status and cleanup callbacks cannot throw across failure handling.

The new implementation is isolated in W3DDisplayPresentation.cpp; the complete
original W3DDisplay.cpp producer/rendering source remains unchanged. Win32
contains no new Display virtual entry or WW3D lifecycle branches. On AMD64,
non-W3D Display subclasses explicitly throw if asked for this diagnostic rather
than silently succeeding or becoming abstract. Existing init-only and direct
X64.16 graphics routes remain separate and keep their accepted behavior.

## Why this bounded attachment is necessary

Actual source/build evidence establishes:

- Both W3DGameClient factories still create shared W3DDisplay. W3DView and
  Render2D/texture/font producers depend on that frontend and WW3D active backend.
  Retaining those owners preserves the eventual UI integration contracts.
- Ordinary W3DDisplay::init calls WW3D::Init/Create_Render_Backend/DX8Wrapper and
  dynamically loads D3D8. The accepted native build does not provide a viable
  native ordinary DX8 game device. Replacing the factory alone cannot fix this.
- Ordinary Begin_Render additionally performs DX8 loss/reset, TextureLoader,
  dynamic buffers and target queries; End_Render flushes legacy sorting. The
  explicit active presentation lifecycle bypasses only that legacy frame work.
  All original legacy code remains behind the inactive branch.
- Ordinary W3DDisplay::draw calls TestCooperativeLevel, clears depth, draws views,
  world and client/UI. It remains unreachable. All four WW3D world Render
  overloads reject attempts while the limited modern lifecycle is active.
  Depth clear and network callbacks also reject; there is no fake DX8 device,
  generic emulation, silent world no-op or replacement DLL.
- Headless selection remains unchanged for paired initialization/destruction.
  ApplicationHWnd and engine input remain unset/dummy. The platform diagnostic
  window is not yet the real application window; this is a measured intermediate
  ownership/frame integration, not a claim to have migrated all window/input
  handling. Win32GameEngine::update would execute engine work before pumping
  messages, so this gate never uses it.

D3D11 device creation, swap chain/backbuffer/RTV, embedded shaders, viewport,
clear, explicit Present status, visibility checks, limits and system dependencies
reuse the accepted implementation. Hardware is explicit; failure never chooses
WARP. WARP remains an explicit choice only in isolated automated tests.

## Diagnostic mode and evidence

Strict AMD64-only grammar: `-headless -nativeDisplaySmoke <isolated run root>`.
The existing scripts take `-DisplayLifecycle`, bind it to a distinct hashed-stage
mode and choose that exact argument. Missing review switch, mixed modes, extra
arguments, mutated stage/build/assets or unsupported dependencies fail closed.
No engine execute/update, replay, simulation or GameLogic runs.

800x600 windowed color-only blue clear, hardware device, at most300 frames and
5000ms target (6000ms hard observed bound), no fullscreen/resize/recovery. Close,
Escape, unexpected resize, occlusion without a visible success, device failure
or bad logging fail the gate. Visible non-minimized/unoccluded S_OK Present and
actual OS exit0 remain mandatory. There is no next automatic trial.

Additional flushed checkpoints: display_attach begin/complete, display_size,
display_frame_begin, display_frame_end, display_detach begin/complete. Existing
hardware adapter/feature identity, setup/viewport, first clear/Present, loop
summary, checked release boundaries, normal cleanup and late observer stay
required. The classifier verifies the interleaved display/host completion order,
fixed dimensions and explicit display-smoke WinMain mode, and rejects display
records in the old direct-backend mode.

## Offline validation

Final current-source offline validation PASS:

| Validation actually run | Result |
| --- | --- |
| Complete Win32 Release game/tool/test build | PASS |
| Win32 CTest, repeated after final incremental relink | 25/25 PASS |
| Direct Win32 Generals / Zero Hour tests | 149/149 each PASS; existing six disabled tests per title remain disabled |
| Focused performance/producer/reference title suite | 75/75 each PASS |
| Sentence producer / neutral submission tests | 5/5 and15/15 PASS |
| Independent AMD64 CTest | 15/15 PASS; established full-title g/z_googletest exclusions retained |
| Native Generals and Zero Hour links | Both PASS, no new blocker |
| Focused lifecycle/WARP/host tests | 11/11 AMD64 PASS; Win32 also covered by full CTest |
| Focused startup gate tests | 9/9 AMD64 PASS; Win32 also covered by full CTest |
| Rendering reference source contracts | 28/28 PASS; no frozen check relaxed |
| Startup/display source contracts | 13/13 PASS |
| Performance Python reference suite | 16/16 PASS |
| PowerShell graphics / native workflow | 37 and6 PASS; all10 scripts parse |
| Current native imports/dependencies/EXE-PDB pairing | Both PASS;30 imports each, no delay or D3D8/D3DX import |
| Accepted X64.14/X64.16 preservation | All23 inventoried evidence files unchanged; both accepted final-2 EXE/PDB stage hashes unchanged |

Final incremental source/build confirmation found native outputs current and
relinked Win32 outputs for the final source; full Win32 regression/direct/focused
checks were then rerun successfully. Native full build/link log, failure logs,
final incremental logs, CTest/direct/reference outputs and identity analysis stay
under ignored build/x64-17-* paths. No native title execution occurred.

Focused coverage extends the existing native_graphics_smoke_test harness:
neutral frame pairing/depth rejection/status/size, backend ownership including
invalid construction, Begin/End failure accounting, actual offscreen WARP blue
readback, borrowed-display real WARP swap-chain frames and partial-init/clear/
Present failure cleanup while HWND is still alive. These tests use isolated test
executables, not either game. Startup tests cover separate strict grammar,
Win32 rejection, flushed isolated paths and old-mode separation.

New source preservation checks independently remove only native-only additions
and compare retained WW3D/display legacy source to the accepted baseline. All
existing rendering reference checks remain intact. Initial test failures from
placing the new implementation inside the frozen W3DDisplay.cpp were resolved
by isolating the new TU, not weakening those references. An initial compiler
failure (GameMain could not include a device-layer header) was resolved by the
neutral Display virtual diagnostic entry, keeping engine/device layering.
Failure logs are retained under ignored build/x64-17-* paths. The device TU
uses its target-provided PCH rather than engine-only PreRTS; a test Run lookup
collision was corrected with an explicit NativeGraphicsSmoke namespace. These
were bounded compile/test integration corrections, not renderer policy changes.

## Remaining route to original menu

This gate must be reviewed before proceeding to a game-integrated window/input
lifecycle. Ordinary ApplicationHWnd creation/focus/WM_SIZE/close callbacks and
engine input are not migrated. WW3D device enumeration/settings/gamma/loss and
other DX8 contracts outside this bounded lifecycle remain legacy. Do not flip
m_headless or call ordinary draw/execute to bypass those requirements.

Next UI work must establish original shell asset/state requirements and safely
create the existing Render2D/font helpers, release all modern texture residencies
before device shutdown and retain view sizing. Existing D3D11 sentence and normal
alpha UI textures are reusable; solid/additive/grayscale state coverage and
actual title asset/missing-resource policies remain incomplete. Shell::showShell
also enqueues shell-map gameplay work and changes RNG state; it is not a safe
menu-only gate. No shell/menu/world rendering, gameplay, save/replay/network,
performance or determinism is validated here. Original UI is the reference;
no redesign is authorized by this slice.

Exactly one next task: **separately reviewed Generals-only X64.17 retained-display
hardware presentation confirmation**, with visual review and preserved machine
receipts. Do not run Zero Hour or another trial automatically.

## Validated candidate build identity

- Generals: GUID `c7323867-d82e-41f3-b190-20a5da4eae06`, age48; EXE SHA256 `F0A8EDB1A9614A6EAF602E2257DAB105F179359123026734DBBA4AB3B329E879`; PDB SHA256 `77E0ABF8F28C83F674CA2DBE9246BBCE024303FB84B79A9AA87201F73664D5AB`.
- ZeroHour: GUID `7a285453-4110-4cad-a6c8-e5336ba19b1c`, age49; EXE SHA256 `F0DC29393A8B8E6DFD59171A796CF4B8EEB7A2B8C0EF9CB612F16CE644FE274B`; PDB SHA256 `9F1D20D18398737F37E33C8EC65715A876F77928B2A5B70E95FAE2CA34C728AD`.

System AMD64 d3d11.dll, dxgi.dll and d3dcompiler_47.dll are preflighted; no
app-local replacement DLL is supplied. This proves dependencies/links, not a
new hardware runtime PASS. Both reviewed X64.16 stage identities remain intact.
## Fresh stages and first reviewed developer procedure

After all final validation, both new isolated stages were prepared and preflighted:

- build/dev-runtimes/native/Generals/x64-17-final
- build/dev-runtimes/native/ZeroHour/x64-17-final

Both bind graphics.json to `nativeDisplaySmoke; headless engine; hardware D3D11;
W3DDisplay/WW3D` and pass matching current AMD64 EXE/PDB identity, asset inventory,
hashes and system graphics dependency checks. Both reject preflight in the old
direct-backend mode. Neither has a runs directory; no game was launched.
The installed Steam assets were read/copied only. Accepted final-2 stages and
all original evidence remain unchanged. Direct isolated Win32 confirmations also
pass: lifecycle11/11 and startup8/8 (AMD64 startup9/9).

The next step requires separate developer review and execution. Use PowerShell7
from the repository; no rebuild/restage is needed:

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-17-final -DisplayLifecycle
./scripts/native-runtime/Invoke-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-17-final -DisplayLifecycle -RunReviewedGraphicsSmoke
```

The first command is read-only preflight and launches nothing. The second runs
exactly one Generals process, with strict `-headless -nativeDisplaySmoke` and a
fresh isolated run directory. Expect a fixed800x600 blue client area for about
5 seconds, followed by automatic close and normal process exit. No menu/map is
expected. Do not click, resize, minimize, press Escape or manually close it;
close/Escape/unexpected resize deliberately fail the diagnostic. No interaction
is needed, and no next trial/title starts automatically.

On any error, preserve evidence and stop; do not rerun or launch Zero Hour.
Return the exact new directory under
`build/dev-runtimes/native/Generals/x64-17-final/runs/<UTC>` and visual observations.
Preserve launch.json, exit.json, startup.log, shutdown-exception.bin and
userdata/CrashDumps. Acceptance requires display mode/attach/size/frame/detach
checkpoints, hardware adapter/feature identity, visible unoccluded S_OK Present,
bounded frame/time summary, device-before-HWND cleanup, execution skipped,
normal engine/MiniDumper cleanup, armed observer, final WinMain, actual OS exit0,
empty observer/no trailer/no dump and matching paths/build/stage identity.
Blue visual evidence alone is insufficient.

Autonomous work stops at this new native/visual gate. The accepted ownership fork
is resolved and does not need another choice. No original-menu, world or
simulation gate is implied by successful diagnostic presentation.
