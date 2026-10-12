# X64.15: native graphical renderer-route feasibility audit

Date: 2026-10-12. Baseline: `1c82677fbbf8a6b55c13ad1e550d6d148b1728cd`
on dev/modern-engine. That documentation-only commit finalized both reviewed
X64.14 runtime passes and was pushed only to origin/dev/modern-engine before
this audit. No production code, build configuration or launcher changed here.
Neither title was launched. This report distinguishes actual source/build/test
observations from the proposed, not-yet-implemented graphical gate.

## Decision

The existing D3D11 backend is reusable and extendable; a generic D3D8 emulation
layer is NOT required to create a native window/device/cleared frame. It is NOT
a selectable/complete game backend. Switching Create_Render_Backend alone is
unsound: W3DDisplay and WW3D still assume DX8 device enumeration, resolution,
capabilities, resources and frame lifecycle outside IRenderBackend.

The smallest sound next slice is a **shared opt-in native graphical smoke host**
inside the actual Generals and Zero Hour EXEs: after the already verified
headless engine initialization, before execute/replay/simulation, create a
separate diagnostic HWND, own an existing D3D11Backend, clear/present a bounded
visible window, destroy graphics/window, then perform the verified normal engine
and process cleanup. Retain headless subsystem selection throughout this mode.
This is deliberately a graphical diagnostic in the native title process, not
ordinary W3DDisplay graphical startup or a shell/menu milestone. It avoids
pretending that unsupported DX8 operations have compatible D3D11 replacements.

An additional bounded engine-to-backend lifecycle/configuration compatibility
adapter is required later for real display
integration. Keep it shared and renderer-neutral, and migrate the necessary
callers incrementally. Do not add a fake IDirect3DDevice8, cast COM resources
between APIs, erase DX8, or silently no-op unsupported world/render states.

## Exact current startup and frame paths

Both titles have separate WinMain and GameMain/GameEngine/GameClient bodies;
the device/display/backend implementation below is shared.

1. OS loader/CRT -> title WinMain. Both WinMain sources configure diagnostics,
   install exception handling, initialize memory and parse startup flags.
   The normal path calls initializeAppWindows when m_headless is false
   (Generals line867, GeneralsMD line895). RegisterClass/CreateWindow, focus,
   ShowWindow/UpdateWindow establish ApplicationHWnd (Generals lines694..767;
   GeneralsMD lines696..769). This creates an HWND before any D3D8 load; a splash
   painted by GDI is not evidence of a GPU frame. The helper currently returns
   true unconditionally; a diagnostic host must explicitly check registration,
   window creation and actual client size instead of trusting that return.
2. WinMain -> GameMain -> CreateGameEngine (title WinMain factory returns
   Win32GameEngine) -> Win32GameEngine::init -> title GameEngine::init ->
   W3DGameClient/GameClient::init -> createGameDisplay -> shared W3DDisplay::init.
   The title W3DGameClient.h factories select W3DDisplay, DirectInputKeyboard,
   W3DMouse and W3DGameWindowManager. GameClient creates keyboard only when
   non-headless, selects MouseDummy and GameWindowManagerDummy for headless,
   and creates the display and Shell in both modes. Headless Shell::init loads
   scheme metadata; it does not establish a rendered main menu.
3. W3DDisplay.cpp:738..970: base init, file factory/math/asset manager; non-headless
   scenes/lights; WW3D::Init(ApplicationHWnd):823 -> ww3d.cpp:269..322 ->
   Create_Render_Backend:281 -> Backend/DX8Backend.cpp:33..60 -> DX8Wrapper::Init.
   dx8wrapper.cpp:297..325 calls LoadLibrary("D3D8.DLL"), GetProcAddress
   Direct3DCreate8, creates IDirect3D8 and enumerates devices. This is the first
   unavoidable graphics DLL blocker on the ordinary non-headless route.
4. W3DDisplay then configures screen bias/Render2D, MSAA and
   WW3D::Set_Render_Device(0,width,height,bits,windowed,true):886..918.
   ww3d.cpp:451 directly delegates to DX8Wrapper, as do device/target resolution,
   enumeration, texture bit depth and other settings. Init continues into
   initAssets, 2D/3D scene init, W3DShaderManager, debug text and
   DX8WebBrowser::Initialize:967. Those are not backend-neutral just because
   WW3D::Init constructed an IRenderBackend.
5. Normal GameMain enters ReplaySimulation or GameEngine::execute. execute calls
   update; Win32GameEngine::update calls GameEngine::update (client and logic)
   before serviceWindowsOS. The latter PeekMessage/GetMessage/TranslateMessage/
   DispatchMessage loop is at Win32GameEngine.cpp:134..167. Therefore execute or
   engine.update is not an acceptable way to get a diagnostic message pump.
6. Client update/draw reaches W3DDisplay::draw:1844. It returns in headless mode,
   checks DX8 TestCooperativeLevel:1981 and calls WW3D::Begin_Render(true,true):2016,
   drawViews, UI/client/cursor/video drawing, then End_Render. WW3D Begin_Render
   :807..876 tests/resets the DX8 device, services TextureLoader, resets legacy
   dynamic VB/IB and queries DX8 target resolution before neutral clear/begin.
   End_Render:1097..1135 flushes the legacy SortingRenderer before neutral end/
   present. D3D11 Clear explicitly rejects depth=true. The normal loop cannot
   produce even a clean D3D11 empty frame by changing the backend pointer alone.
7. W3DDisplay destructor:420..482 releases display strings, 2D helper/views/
   scenes, calls W3DShaderManager::shutdown, frees assets, WW3D::Shutdown and
   browser shutdown on non-headless paths. WW3D::Shutdown:338..389 releases assets,
   shuts down DX8TextureManager and deletes the backend. DX8Backend destructor
   owns DX8Wrapper::Shutdown. Replacing this route requires matching partial-init
   and resource ownership; changing m_headless midway would violate the paired
   init/destructor decisions.

The reviewed native init-only route currently branches in each GameMain:51..55,
logs init_only and skips execute/replay/simulation before ordinary cleanup.
StartupDiagnostics.h strictly accepts only the existing four-argument AMD64
-headless/-nativeInitOnly grammar. The proposed new diagnostic mode needs its
own explicit grammar/mode selection; it must not repurpose existing init-only
success or accept arbitrary gameplay switches.

## Hard blockers versus temporarily retained legacy code

| Actual dependency/assumption | Ordinary D3D11 display impact | First diagnostic-frame treatment |
| --- | --- | --- |
| Unconditional DX8 factory and D3D8 LoadLibrary | Hard startup blocker; system AMD64 D3D8 absent, historical app-local DLL is I386 | Keep existing factory unchanged; diagnostic host owns D3D11Backend directly |
| WW3D device enumeration, Set_Render_Device, Get_Device/Target_Resolution, MSAA/texture bit depth/filter policy | Still call DX8; neutral device/configuration adapter missing | Use explicit bounded client size/RGBA8/single sample/viewport from host, no saved settings mutation |
| W3DDisplay::draw TestCooperativeLevel and Begin_Render(true,true) | No DX8 device; D3D11 rejects depth clear | Direct existing backend color-clear/begin/end/present, no W3DDisplay draw |
| WW3D frame TextureLoader, dynamic buffers, sorting flush | Legacy resources and device lifecycle remain coupled | Do not enter these functions for a clear-only diagnostic |
| W3DShaderManager init/getChipset/render targets/legacy shader handles | IDirect3DDevice8, D3DSURFACE_DESC, DX8 caps and CreateTexture/GetRenderTarget; legacy .pso shaders do not become modern HLSL | Keep headless skips; use existing embedded D3D11 VS/PS only |
| SurfaceClass/native texture/vertex/index buffers, missing-texture native fallback | Live DX8 COM ownership and capability policy; X64.10 resource compatibility removes D3DX, not D3D8 | Can remain compiled and unreachable; never reinterpret as D3D11 resources |
| Gamma, ambient, light environment, surface copy | D3D11 overrides currently throw Unsupported | Exclude from diagnostic; implement only when a later reachable contract needs them |
| W3DMouse DX8/hardware or W3D/poly cursors; keyboard cooperative level | DX8 cursor calls/thread in RM_DX8; DirectInput8 uses ApplicationHWnd | Keep dummy engine input; diagnostic uses ordinary Windows cursor/close/Escape |
| Window callbacks | Existing WndProc can mutate engine/input/audio, queue quit-game messages, paint splash; WM_SIZE has no D3D11 Resize routing | Dedicated bounded diagnostic WndProc/message pump without engine callbacks |
| Browser COM/media/network/world rendering | Not required for an empty diagnostic frame; not proven AMD64 graphical behavior | Leave skipped/unused, not replaced with success stubs |
| DX8 enums, headers, CPU math/resource conversion types | Compile/reference contracts, not an external native DLL dependency themselves | Keep behind existing boundaries and preserve all accepted tests |

IRenderBackend covers viewport, clear/begin/end/present, Render2D submission,
texture residency/preparation, gamma/lighting/copy boundaries. It does NOT cover
native device creation/configuration/enumeration, size/capabilities, loss/recovery,
world/mesh rendering or full shell resource semantics. Neutral Render2DSubmission,
Write_Render2D_Vertices, Texture2D content/residency/sampler intent and CPU font
production are reusable. Generals Render2D::Render uses ScreenResolution;
Zero Hour Render2D::Render:593..607 queries WW3D::Get_Device_Resolution, still
DX8. Preserve that difference while eventually supplying truthful backend sizes.

## Existing D3D11 capability and limits, verified in current source

Backend/D3D11Backend.{h,cpp} already supplies:

- Owned device/immediate context, swap chain/backbuffer texture/RTV, shaders,
  buffers and states through private ComPtr ownership (Impl:88..134).
- D3D11CreateDeviceAndSwapChain:139..189; explicit hardware or explicit WARP,
  feature levels11_0/10_1/10_0, one RGBA8 single-sample windowed discard buffer.
  Null HWND selects offscreen. Hardware failure propagates without fallback.
- Embedded shader compilation (D3DCOMPILER_47), input layout, straight-alpha
  normal sentence/UI pipeline, disabled depth, explicit rasterizer/sampler state.
- Color clear, scene pairing, Present(0,0), bounds-checked viewport, resize outside
  a scene with RTV/backbuffer release and recreation:334..359. No DSV/depth clear.
- Real indexed Render2D drawing with dynamic buffers and existing vertex writer,
  ARGB swizzle and half-pixel placement correction. Current shader predicate only
  accepts the exact sentence normal-alpha bits; grayscale/additive/solid and
  other state combinations reject. Null-texture diffuse drawing does not imply
  that every untextured/solid legacy ShaderClass variation is accepted.
- Immutable single-mip A4R4G4B4 sentence and A8/X8/R5G6B5/BC1/2/3 UI uploads,
  retained SRV residency, supported clamp/repeat filtering, pitch-aware CPU
  snapshots, and validation readback. No generic compressed encoder substitution.
- D3D11TextureBridge compiles in both title archives and uses real _TheFileFactory,
  TextureClass snapshot/refcount/residency. File Init intercepts before legacy
  loading; unsupported formats/HSV/missing assets/mips/mutable resources reject.
  Create_For_W3D requires the backend to be active. It is not a factory selector.

Missing from this slice: ordinary game factory/display integration; complete
shell state and asset coverage; depth/world/render targets; mutable radar/video,
Font3D and missing-resource policy; gamma/lighting/copy implementations; fullscreen,
minimized/occluded handling, reliable visible-present result reporting, device
removal/recovery and production resizing policy. Current Check only rejects
FAILED(HRESULT); successful-status occlusion is allowed by the hidden-window test.
Flip_To_Primary returns void, so first-visible-frame evidence needs explicit raw
Present result/occlusion receipts, not merely an End_Scene completion. No existing
MakeWindowAssociation/Alt-Enter suppression, GetDeviceRemovedReason, debug-layer
or info-queue diagnostics were found in D3D11Backend. Add only bounded diagnostics
required by the next gate; do not implement recovery yet.

Impl destruction ClearState/Flush precedes member COM release. The accepted
hidden-window test explicitly destroys backend before HWND. Keep that order for
the host, including failure/unwind. Later real display integration must release
logical texture/residency owners before destroying the backend; it must not mix
DX8 and D3D11 assets or hot-swap existing live resources.

## Minimum capabilities by milestone

| Milestone | Already exists | Missing before claiming it in the native title |
| --- | --- | --- |
| a. Window created | Windows imports and title window helper; tests create hidden HWND | Checked diagnostic window/class, visible client sizing and bounded independent pump, close/error receipt; no engine-side message callbacks |
| b. Device/swap chain | Existing D3D11Backend::Create(window,size) with real WARP proof | Invoke/own it in the title diagnostic branch; record actual driver/adapter/feature level/size and constructor substage/HRESULT; native staging preflight for newly live modern imports |
| c. First clear/present | Backend color clear/begin/end/Present, viewport, hidden swap-chain readback/resize tests | Visible window, successful unoccluded Present receipt, bounded display interval and manual visibility confirmation; orderly graphics/window teardown and actual exit0 |
| d. Shell/menu | Layout/WindowManager, W3DDisplayString -> Render2DSentence, mapped UI texture and Render2D producer boundaries | Real display/device lifecycle and resolution/capability adapters, actual title bridge/archives/font integration, all reachable GUI states/formats, input/cursor, frame and cleanup paths; exclude shell-map/world/intro/video for an initial static menu gate |

Shell::init:164 loads menu-scheme metadata. Shell::showShell:484 can push
Menus/MainMenu.wnd:542; showShellMap:547 can enqueue GAME_SHELL and reset RNG.
W3DMainMenu callbacks draw the scheme; W3DDisplayString builds/draws sentences.
This is more than an empty swap-chain frame. GameEngine::execute/update would
also update GameLogic. A later shell-only gate must explicitly avoid shell-map
loading and simulation, drive only audited UI work and handle reachable normal,
disabled/grayscale/solid/additive drawing states and actual assets. That later
scope is not part of the first clear/present implementation recommendation.

## Proposed bounded graphical-startup gate (implementation not started)

Recommend X64.16: shared native window/device/frame diagnostic gate, in both
native title processes, with tiny matching call sites in their GameMain bodies.
Use the existing accepted headless init and normal cleanup; own graphics in a
shared diagnostic host outside WW3D::RenderBackend. Call D3D11Backend::Create,
not Create_For_W3D; a clear-only frame needs no engine texture bridge. Do not
fork another renderer implementation or pretend to initialize W3DDisplay.

1. Add a distinct opt-in AMD64-only diagnostic mode with strict arguments,
   canonical fresh evidence root, isolated user data/dumps and forbidden
   scenario/replay/developer/performance switches. Compile the title diagnostic
   call sites only for AMD64 so Win32 selection/import behavior stays intact.
   Keep existing init-only mode,
   strict success checks and Win32 behavior exactly intact. Retain headless
   subsystem selection; do not toggle it for the diagnostic window.
2. After successful engine init and before execute/replay/simulation, register a
   diagnostic window class, check every API result and create/show a fixed-size
   windowed client (proposed800x600). Use a standard system cursor, no capture,
   no fullscreen/topmost, no DirectInput/browser/video/shell/world creation.
   Keep ApplicationHWnd/legacy WndProc out of this host; handle messages locally.
3. Create the existing backend on that HWND with explicit hardware driver for
   the first reviewed visible gate. WARP remains an explicit, separately labeled
   test choice, never silent fallback. Disable DXGI automatic Alt-Enter for this
   bounded windowed mode; no resolution preferences/desktop-mode mutation.
4. Pump messages independently, set viewport from verified client dimensions,
   color-clear with a distinctive fixed color, Begin_Scene/End_Scene(true), and
   hold/repaint for a short bounded interval (proposed5s, hard frame/time bound).
   No engine/client/GameLogic updates. Validate a bounded readback once for the
   clear color if useful; readback is not evidence of desktop visibility.
   Occluded/minimized frames cannot satisfy visible-frame success. Retry only
   inside the bounded same-run visibility interval; no process relaunch/fallback.
   Either handle nonzero resize at a scene boundary using existing Resize, or
   fail with an explicit unsupported-resize receipt; never Resize(0,0).
5. Destroy backend (unbind/flush/release) before HWND/class, then normal engine,
   MiniDumper and WinMain cleanup. Preserve the late observer and actual exit0
   criterion. Close/Escape/timeout/device error must be explicit abort/failure,
   not a successful frame receipt. No next title/trial starts automatically.
6. Prepare fresh per-title native stages only after implementation validation.
   Document an exact guarded developer procedure; first process execution is
   a separate review, one title/run at a time. The accepted X64.14 stages/runs
   and symbols must remain untouched. No launch command is authorized by this audit.

### Diagnostics and acceptance

Reuse flushed startup logging/isolation/minidumps and late shutdown observer,
with distinct mode identity. Current observer is armed at end of WinMain; normal
MiniDumper/unhandled exception reporting covers earlier failure best-effort.
Do not promise a complete pre-WinMain or GPU-hang dump from these facilities.

Checkpoint begin/complete/fail for host entry, window class, HWND/client size,
D3D11 device/swap chain, RTV/backbuffer, shader/state setup, viewport, first clear,
first Present and raw HRESULT, bounded loop summary, backend release, HWND release,
host exit, then the established engine/MiniDumper/WinMain checkpoints. Capture
GetLastError immediately for failed Win32 calls; operation/HRESULT in hex for
D3D failures, adapter/driver/feature level, device-removed reason when applicable,
client/backbuffer dimensions and visibility/occlusion. Constructor currently
combines device/target/shader setup: a minimal optional diagnostic callback or
phase reporting is needed to distinguish failure stages without a new profiler.
Log only first events/bounded aggregate counts to stay within2048 records.
Do not make D3D debug-layer installation a runtime dependency; optional validation
may use it if available and label the result.

Receipts must prove matching AMD64 EXE/PDB GUID/age/hashes, staged assets/imports,
mode/arguments, paths, PID, actual exit0, no timeout/error, all required completed
phases, no exception trailer, empty shutdown evidence and no crash dump. Device
creation success must record the selected driver/feature level; GPU failures
must fail closed. A Present S_OK/unoccluded receipt plus developer-observed
window/clear color is required for the first visible-frame claim. API success,
readback, hidden Present or GDI window paint alone cannot make that claim.

Implementation acceptance: dual-architecture neutral/backend tests and Win32
reference contracts remain green; AMD64 stage/diagnostic tests cover strict mode
rejection, errors/aborts, bounded loop and cleanup without running a title;
both native title links and modern dependency preflight pass. Win32 default
factory/reference, X64.9/X64.10 resource contracts, FP policy and all simulation/
RNG/save/replay/network behavior remain unchanged. The reviewer separately
executes and confirms the first visible run; implementation never launches a game.

## Actual build/import/dependency evidence and validation

- Current Generals/Zero Hour PE32+8664 EXEs pair with PDB GUID/age45 and46 and
  hashes exactly as recorded in X64.14. Reviewed run/symbol hashes remain intact.
  Neither link was rerun and no source was rebuilt for this documentation audit.
- Current Ninja Release link inputs for BOTH EXEs include w3d_d3d11.lib,
  d3d11.lib, dxgi.lib and d3dcompiler.lib. Core WW3D2 CMake adds the bridge to
  title sources and links the independent modern-SDK static library.
- Actual native import tables remain28 contracts/title, no delay imports,
  no D3D8/D3DX/D3D11/DXGI/compiler imports. Combined with the unconditional DX8
  factory and absence of a D3D11 create caller in game startup, this supports
  unused static-backend/link-input elimination, not a live modern game backend.
- Actual AMD64 d3d11_sentence_test imports d3d11.dll and D3DCOMPILER_47.dll;
  DXGI objects are obtained through device/swap-chain creation rather than a
  guaranteed direct dxgi.dll import. Future preflight must inspect actual imports
  and required graphics components, not freeze the old28-count expectation.
- Current System32 d3d8.dll absent. d3d11.dll, dxgi.dll and d3dcompiler_47.dll were
  read and verified AMD64/PE32+. Headless runtime staging forbids app-local DLLs;
  retain that default policy for the modern smoke stage, with system dependency
  validation. Do not copy/replace any installed DLL or import an I386 overlay.
- Reran existing CTest targets render2d_submission_test, sentence_content_test,
  d3d11_sentence_test and d3d11_ui_test: **4/4 PASS Win32,4/4 PASS AMD64**.
  D3D11 sentence/UI targets exercise7 and9 cases respectively, including real
  WARP upload/draw/readback and hidden HWND present/resize/release. These run test
  processes, not either game; visible desktop/hardware-driver proof is still missing.
- Rendering Python source/reference checks: **28/28 PASS**. No full Win32/game
  regression or native full-title GoogleTest was rerun; no production change
  requires such a claim. Existing native full-title test exclusions remain.
- git diff --check PASS. Audit delivery is only this report and current-state
  updates in AGENT_HANDOFF.md, ROADMAP.md, X64.md and RENDERER.md. Generated test
  logs and preservation receipts remain ignored; no runtime/capture files staged.

## Exactly one recommended next task

**X64.16: implement the shared opt-in native window/device/clear-present
smoke gate specified above, validate offline/with isolated tests and prepare
fresh guarded stages for separate developer-reviewed execution.** No shell/menu,
world rendering, simulation, full D3D8 emulation or game launch in that
implementation stage. No X64.16 work was started by this audit.
