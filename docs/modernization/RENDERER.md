# Renderer and display architecture

## X64.16 Generals visible-frame evidence; OS exit receipt pending (2026-10-12)

Reviewed Generals run `20261012T1007219135054Z` in the x64-16-final-2 stage is
preserved unchanged. Developer observed the blue client area for about5 seconds.
Direct inspection verifies286 PID25496 checkpoints: hardware Intel Graphics
8086:7D45/feature0xB000, clear and visible unoccluded S_OK Present,210 visible
frames/0 occluded/5000ms, ordered backend-before-HWND cleanup, execution skipped,
normal engine/MiniDumper cleanup, armed late observer and WinMain complete0.
Empty observer file, empty CrashDumps and matching reviewed EXE/PDB/path identity.
The first visible native AMD64 D3D11 frame is supported by visual + machine evidence.

Full process PASS remains UNVERIFIED: exit.json is missing because the launcher
assigned zero conditional pipeline outputs to null then accessed dumps.Count.
WinMain0 cannot prove final OS exit0 after CRT. No process-exit event was found;
do not invent an exit receipt or infer exit0 from the error. Synthetic tests
show the old code loses receipts for either exit0 or exit1 with an empty dump
folder. The fix normalizes the whole conditional to an array. Six actual
post-processing fixtures (0/1/2 dumps x exit0/1) now preserve receipts/verdicts;
graphics workflow25, native workflow6, startup contracts9 and preflight PASS.
No engine/renderer source, binary, stage or original run evidence changed;
no rebuild, restage or game launch. See the updated
[X64.16 report](X64_16_NATIVE_GRAPHICS_SMOKE_GATE.md) for exact evidence/limits.

Exactly one recommended next task: **developer review/authorization of one
Generals-only confirmation with the fixed launcher and unchanged final-2 stage**
to capture actual exit0 and complete the process milestone. Do not perform now;
Zero Hour stays held. Earlier next-task entries below are historical/superseded.

## X64.16 native graphical smoke gate implemented (2026-10-12)

Shared AMD64-only `-headless -nativeGraphicsSmoke <isolated-root>` now enters a
bounded diagnostic HWND/pump and the existing D3D11Backend directly after engine
init, before execute/replay/simulation. Hardware only, fixed800x600, blue clear,
viewport/Begin/End/Present, explicit HRESULT/visibility/occlusion evidence and
backend-before-HWND teardown. Existing nativeInitOnly, Win32/DX8 factory, normal
cleanup/MiniDumper/late observer and actual-exit0 requirements remain intact.
No shell/world/game updates or game launch occurred in this stage.

Win32 Release/full CTest25/25, direct title tests149 each/focused75 each;
independent AMD64 CTest15/15 and both native title links PASS. Rendering source
contracts28, performance16, startup9, graphics workflow19 checks PASS. Native
imports30 each include D3D11/compiler, no D3D8/D3DX; Win32 has no new graphics
imports. Both fresh **x64-16-final-2** stages/preflights PASS, matching EXE/PDB
identities and system graphics dependencies. All12 reviewed X64.14 evidence/
symbol hashes remain intact. Generated stages/logs remain ignored/uncommitted.

See [X64_16_NATIVE_GRAPHICS_SMOKE_GATE.md](X64_16_NATIVE_GRAPHICS_SMOKE_GATE.md)
for implementation, checkpoints, exact binary identities, validation and guarded
commands. The initial incomplete x64-16-final staging attempts are not candidates.
Hardware/visible title presentation is still UNVALIDATED; ordinary graphical
startup, shell/menu/world integration and simulation remain blocked/later work.
Earlier next-step entries below are historical and superseded by this state.

Exactly one recommended next task: **separately reviewed Generals-only native
graphical smoke confirmation using x64-16-final-2**, preserving receipts and
confirming the blue client area visually. Do not launch automatically; Zero Hour
remains held for review. Do not begin the next renderer implementation here.

## X64.15 graphical renderer-route audit complete (2026-10-12)

The combined Generals/Zero Hour X64.14 headless init-only milestone was finalized
and pushed in documentation commit1c82677fbbf8a6b55c13ad1e550d6d148b1728cd before
this audit. Both reviewed runs/symbols remain unchanged. No game was launched.
Actual source/factory and current AMD64 imports still select DX8: missing system
AMD64 D3D8 blocks ordinary graphical startup. Modern libraries occur in both
link inputs, but their imports are absent from the actual titles; no active
D3D11 backend is inferred from successful linkage.

D3D11 already owns device/context, windowed swap chain/backbuffer/RTV, viewport,
color-clear/begin/end/present, resize and bounded sentence/normal-alpha file-UI
rendering. Existing isolated tests pass4/4 per architecture; rendering Python28
passes. Hidden WARP presentation is not visible hardware or game-backend proof.
Factory switching alone leaves DX8 device/caps/resolution, depth clear, texture/
sorting/shader manager/cursor and teardown assumptions. Shell/menu is a separate
integration gate; do not enter execute/update to obtain a message loop.

See [X64_15_GRAPHICAL_ROUTE_FEASIBILITY.md](X64_15_GRAPHICAL_ROUTE_FEASIBILITY.md)
for exact call paths, blocker inventory, reusable/missing capabilities and gate
acceptance. This is documentation-only: no renderer/source changes, rebuild or
native link rerun. Existing Win32/resource/FP contracts and strict init-only
success remain binding. Rendering/gameplay/simulation/save/replay/multiplayer,
performance and determinism are not validated by the headless milestones.

Exactly one recommended next task: **X64.16 shared opt-in native window/device/
clear-present diagnostic gate** inside both title processes after verified
headless init, before execute/replay/simulation. Reuse D3D11Backend directly in
a shared bounded diagnostic host, without selecting it as the W3DDisplay game
backend. Add flushed phases, truthful Present/occlusion/error receipts, checked
window/pump and teardown, modern dependency preflight and strict separate mode.
Validate without launching either title and prepare fresh stages for separate
developer-reviewed execution. No shell/menu, world rendering or generic D3D8
emulation in this slice. This recommendation has not been implemented; no game
launch is authorized by the audit. Earlier next-step entries are historical.


## X64.12 headless diagnostic preparation (2026-10-12)

The DX8 factory/device route remains unchanged. The new strict native init-only
gate uses existing headless skips, logs subsystem initialization and exits before
execute/replay/simulation. Native stages contain no x86 renderer/media overlays.
Both preflight successfully; neither title has run. Graphical native D3D8 remains
unavailable and the D3D11 UI slice is not selected. Exactly one next task is the
developer-reviewed first dual-title native init-only validation described in
[X64_12_NATIVE_INIT_ONLY_GATE.md](X64_12_NATIVE_INIT_ONLY_GATE.md).


## X64.11 native startup route (2026-10-12)

Both AMD64 titles now link with no external D3DX helpers, but graphical startup
still selects DX8Backend unconditionally and loads D3D8.DLL. Native system D3D8
is absent; existing app-local copy is x86. D3D11.1/.2 remain independent sentence/
UI slices; changing the factory would leave direct DX8 world/resource calls
invalid. No renderer selection or implementation changed in this audit.

Existing headless startup bypasses WW3D/device/browser initialization, while still
constructing W3D fonts/metadata/terrain/video objects. It is a bounded diagnostic
candidate, not a native renderer or proven gameplay route. Exactly one next task
is **X64.12 bounded native headless startup diagnostic gate**, with flushed stages
and deliberate init-only exit prepared for developer review. No game launched.
See [X64_11_NATIVE_STARTUP_READINESS.md](X64_11_NATIVE_STARTUP_READINESS.md) for
actual call graph, dependencies, FP limits and diagnostics. Earlier entries below
are historical; accepted X64.10 resource contracts remain binding.


## X64.9D numerical convergence complete (2026-10-11)

The independent test-only RGB/DXT5 numerical checker closes **13/13 compressed
gates and 9/9 overall X64.9 readiness gates** for the frozen production contract.
It reproduces endpoint refinement, projection/rounding/ties, degenerate exits,
six/eight-step alpha decisions, x87 binary32 store order and the pinned 8/7
coefficient. Production resource helpers remain unchanged; DX8 remains default.

All 1,710 existing direct encoder records and eighteen small-block extensions
are predicted independently. One fixed 32-block validation set adds 96 complete
encodings, all matching; all four RGB orientations and both DXT5 modes occur.
The conventional-coefficient negative control differs on eighteen blocks.
No lookup of reference outputs occurs in the numerical model. No further broad
fixture expansion was needed. Earlier OPEN entries below are historical.

Final validation: full Win32 Release build, CTest20/20 (resource target 8/8),
direct149/title (six existing disabled/title), focused75/title, native independent
CTest10/10, rendering Python28/28, performance Python16/16, PowerShell parser9,
and preservation audit PASS. Six earlier evidence streams and all three compact
reference corpora are unchanged. Native title link diagnostics still contain
exactly the same three D3DX helpers/title; the fixed tool probe retains the exact
30-error multiset. Native full-graph failure remains expected, not resolved.

This checkpoint is the combined twelve-file X64.9 characterization commit,
`X64.9: characterize legacy resource conversion and filtering`, based on
`3283218029fea5eb5e1785d6220a661d5d965815`, on `dev/modern-engine`.
Only these tests, compact references, CMake test registration and modernization
documentation belong in the commit; generated evidence/logs/runtimes remain
ignored. Commit/push to origin/dev/modern-engine is authorized after validation.
No game was launched. See the final X64.9D section in
[X64_9_RESOURCE_CHARACTERIZATION.md](X64_9_RESOURCE_CHARACTERIZATION.md).

Exactly one recommended next task for developer review: **X64.10, implement and
migrate the final bounded resource-helper group** (D3DXCreateTextureFromFileExA,
D3DXFilterTexture, D3DXLoadSurfaceFromSurface) against this accumulated contract.
Preserve file/RLE quarantine disposition, authored mip handling, packed fields,
float filter handoff, exact DXT decisions, access/staging failure cleanup and
snapshot-equivalent aliasing. Do not substitute a generic compressor or silently
reassociate x87 arithmetic. No characterization blocker remains within this
frozen matrix; production implementation and existing tool ABI failures remain.
X64.10 is a recommendation only and was not started. Stage4A.3 stays paused.

## X64.9C compressed encoder continuation (2026-10-11, review pending)

Readiness remains **OPEN**. The pinned encoder route, RGB preconditioning,
DXT1 transparency, periodic small-mip extension and float filter handoff are
bounded. A test-only independent DXT3 alpha checker matches 762 inputs. New
ignored evidence contains 1,899 CPU records and 171/171 real-device agreements;
128 compact encoder references are exercised. Earlier 3,010/371 and 334/380
streams and 58/94 references remain byte-identical.

Exact RGB endpoint refinement/color-selector ties and DXT5 alpha fitting/ties
still lack an independent numerical reproduction. Compressed gate items
1/3/5/7/8/9/13 remain NO; overall readiness 5/9 remains NO. Next task: finish
only that numerical decision checker in the same characterization harness,
reusing the existing cases and pinned tables (including pD8's 8/7 anomaly).
Do not migrate helpers or start X64.10. See the X64.9C sections in
[X64_9_RESOURCE_CHARACTERIZATION.md](X64_9_RESOURCE_CHARACTERIZATION.md).

Full Win32 Release/CTest20, direct149/title, focused75/title, native CTest10,
Python28/16, parser9 and preservation checks pass. Three unresolved D3DX helpers
per title are unchanged. The fixed baseline tool-target probe reproduces the
exact 30-error multiset; the parallel full graph additionally reached two
unchanged Generals WorldBuilder objects (four extra ABI errors), documented in
the report. No tool ABI fix or native-link progress. All eleven combined files
remain unstaged/uncommitted; no push or game launch. Earlier entries are
historical checkpoints; DX8 remains default.


## X64.9B sampling/access continuation (2026-10-11, review pending)

The original X64.9 evidence/assertions are preserved byte-for-byte. Added 334
CPU records, 94 compact references and 380 real-device/access records. Supported
surface cases agree 294/294; sixteen DXT3/5 compressed mip levels also agree.
BOX dispatch/odd-edge trimming, general TRIANGLE/2D float accumulation and native
packed interaction are now bounded. Scaled owned/standalone/back-buffer access,
allocation/readback/upload failures and snapshot-equivalent aliases were tested.

**Readiness remains OPEN: required exact compressed re-encoding is not bounded.**
Compressed SurfaceClass copies/mips are reachable; a generic DXT compressor is
not an acceptable substitute. Next task: inspect only the required pinned
D3DXEncodeDXT1/3/5 decisions and add distinguishing block fixtures in the same
harness. Do not start X64.10 or migrate any helper. Full accepted regressions
pass; native three-helper/title and 30-error tool multisets are unchanged.
See [X64_9_RESOURCE_CHARACTERIZATION.md](X64_9_RESOURCE_CHARACTERIZATION.md),
X64.9B sections, for confidence levels, exact findings and nine readiness answers.
All ten combined files remain unstaged/uncommitted; no push or game launch.
Earlier checkpoints below are historical; DX8 remains default.

## X64.9 resource characterization (2026-10-11, review pending)

No production D3DX helper was migrated. The pinned Win32 observer/device suite
now records 3,010 CPU rows and 371 device rows; 58 compact exact CPU references
and exhaustive packed expansion checks pass. All 108 matching surface copies
and 40 readable mip levels agree between CPU recording and the real device.
The remaining file/filter/surface helpers and exact native/tool diagnostics are
unchanged. Full Win32 Release/CTest20, direct149/title, focused75/title,
native CTest10, Python28/16, parser9 and preservation audit pass. No game launch.

**Readiness decision C: additional characterization before migration.** Exact
general BOX/TRIANGLE dispatch, edge and mixed-channel rounding rules, relevant
compressed conversion and scaled/non-lockable staging boundaries remain open.
Next task: complete that contract using the existing pinned oracle; do not begin
X64.10 implementation. The isolated RLE decoder heap-corruption observation is
explicitly quarantined, not treated as a proven production requirement.
See [X64_9_RESOURCE_CHARACTERIZATION.md](X64_9_RESOURCE_CHARACTERIZATION.md) for
caller classification, exact bytes/rules, device restrictions and validation.
X64.9 is unstaged and uncommitted because its conditional acceptance gate has
not passed. Older checkpoints below are historical; DX8 remains default,
Stage4A.3 remains PAUSED and Pathfinding 2.0 remains backlog.

## X64.7 fixed legacy shader bytecode (2026-10-11)

The four immutable pixel shaders (three water ps.1.1, one profiler ps.1.4)
now use exact pinned-D3DX8 token arrays, including assembler comments and END.
Actual original source bytes are compile-time identity-checked and compared
exactly in independent recorded/live reference tests. Real Win32 DX8 creation
passed for all four through both routes. Runtime assembly/buffer allocation is
retired; device creation/failures, handles, reset/release and profiler gates are
preserved. Both titles' actual profiler-enabled TU also compiles on Win32/AMD64.
No D3D11 migration or game launch occurred; DX8 remains default.

Native probes remove only D3DXAssembleShader: 5 -> 4 helpers/title, combined
LNK2019 10 -> 8, LNK2001=8, LNK1120=2. Neither game links; the exact 30 separate
tool ABI errors are unchanged. Full Win32 Release, CTest17, direct149/title,
focused75/title and native CTest9 pass, including prior texture/FVF/math,
Bezier, RB1/RB2/RB3 and D3D11.1/.2 coverage. Native CPU proof does not establish
AMD64 DX8 shader execution or whole-game visual equivalence.
See [X64_7_FIXED_SHADER_BYTECODE.md](X64_7_FIXED_SHADER_BYTECODE.md) for exact
sources/tokens, reference/device evidence, contract change and remaining risks.

**Next proposed task: X64.8 - pinned-reference CPU diagnostic error-string
compatibility for D3DXGetErrorStringA.** The file/filter/surface group remains
coherent but allocation work does not bound its uncharacterized conversion,
filtering and file semantics. No next task has begun. Pathfinding 2.0 remains
backlog; Stage4A.3 remains PAUSED. Older checkpoints are historical.

## X64.6 legacy resource creation boundary (2026-10-11, historical checkpoint)

The existing DX8 wrapper now routes its ten 2D/cube/volume creation calls through
engine-owned LegacyD3DX requirement negotiation and real DX8 COM device calls.
No D3D11 dependency or IRenderBackend policy expansion is introduced. Pinned
Win32 transcripts and native policy tests pass; separate real Win32 DX8 resource
validation matches 108 2D, 108 cube and 81 volume successful pairs plus failures.
Default/cap/format/mip/ownership rules and wrapper retries are preserved. No file
loading, filtering, surface conversion or shader implementation changed.

Full Win32/CTest14 and native CTest8 (including accepted D3D11 slices) pass.
Native game helpers fall 8 -> 5/title, but neither game links yet. Real AMD64
resource availability and whole-game rendering remain unproven. DX8 stays the
default and no game was launched. See [X64_6_TEXTURE_CREATION.md](X64_6_TEXTURE_CREATION.md)
for evidence and the proposed X64.7 fixed legacy shader-bytecode task; it has
not begun. Earlier architecture findings below remain historical context.

Current D3D11 evidence (2026-10-11):
[D3D11.2 UI file textures](D3D11_2_FILE_UI_TEXTURES.md) adds bounded TGA/DDS
single-mip CPU content, explicit 16/32-bit policy, immutable D3D11 upload and
normal-alpha Render2D execution. Real WARP tests and an installed retail UI
atlas proof pass on x86/AMD64; DX8 remains the game default. Nonzero HSV,
rescaling, native missing fallback and broader renderer features reject.
The FileFactory/TextureClass bridge compiles but is not game-validated.
D3D11 implementation and default selection are unchanged by X64.4. Current
native math/link evidence and the proposed next task are recorded in
[X64_4_D3DX_MATH.md](X64_4_D3DX_MATH.md). Whole-game D3D11 and playable Win64
remain unproven. Older findings below are historical context.

Scope: revision `adac468d7`, investigated 2026-10-07. No renderer replacement,
display-mode change or game launch was performed.

## Layers and graphics API

```text
GameClient / Drawable / draw modules / terrain / GUI
  Display + View interfaces
    W3DDisplay / W3DView / RTS scenes and W3D render objects
      WW3D / scene, mesh, materials, animation, sorting and render buffers
        IRenderBackend -> DX8Backend -> legacy DX8Wrapper
          Direct3D 8 device -> Present
```

The engine uses Direct3D 8 through Westwood W3D/WW3D. It has an existing
`IRenderBackend` abstraction; do not propose adding one as if absent. Current
`Backend/DX8Backend.cpp::Create_Render_Backend` selects DX8Backend, which owns
the DX8Wrapper lifecycle and forwards scene/clear/viewport/light operations.
This is a partial abstraction: W3DDisplay and other consumers still directly
use DX8Wrapper and Direct3D types/device calls. It is not a complete modern
renderer interface or evidence of an alternative shipping backend.

Important directories:

- `Core/Libraries/Source/WWVegas/WW3D2`: scene/mesh/hierarchical models, materials,
  textures, animation, DX8 buffers/state/sorting, renderer and backend interfaces.
- `Core/GameEngineDevice/Source/W3DDevice/GameClient`: display/views, terrain,
  water, roads, trees, particles/shadows, drawable modules and profiler capture.
- Title `GameEngineDevice/Source/W3DDevice/GameClient`: title factories, assets,
  scenes, buffers and expansion-specific rendering behavior.
- Core/title `GameClient` GUI and InGameUI: UI state and drawing.

## Initialization and device ownership

`W3DDisplay::init` initializes base Display, installs the W3D filesystem,
initializes WWMath, creates RTS 3D/world/interface and 2D scenes, lights,
Render2D and the selected display configuration. Headless mode skips graphics.
WW3D/backend initialization reaches `DX8Wrapper::Init`; actual device creation
is `dx8wrapper.cpp::Create_Device` (CreateDevice near line 551), with reset and
lost-device handling elsewhere in the wrapper.
`DX8Wrapper::Init` dynamically loads D3D8.DLL and resolves Direct3DCreate8
(near lines 295-311); absence from the executable's static import list does not
mean Direct3D is unused.

Window creation is in each title's `Main/WinMain.cpp::initializeAppWindows`.
Zero Hour starts with WS_POPUP|WS_VISIBLE; its windowed branch adds caption,
dialog frame, system menu and minimize box. An ordinary windowed mode is not
proof of an implemented borderless desktop mode. A complete borderless/multi-
monitor/DPI policy was not established by this investigation.

## Resolution, fullscreen and refresh

| Concern | Current source behavior |
|---|---|
| User selection | GlobalData/OptionPreferences, command-line `-xres`, `-yres`, windowed settings |
| Mode list | `W3DDisplay::getDisplayModeCount/getDisplayModeDescription`; first device enumeration |
| Filtering | `isResolutionSupported` near line 483 accepts minimum width and >=24-bit depth; upstream removed the 4:3 filter |
| Change mode | `W3DDisplay::setDisplayMode` calls WW3D::Set_Device_Resolution; updates 2D coordinates and falls back to previous mode on failure |
| Startup fallback | W3DDisplay init tries safe resolution if the custom choice fails |
| Device parameters | `DX8Wrapper::Set_Render_Device` sets buffer dimensions, windowed flag, formats/depth and multisampling |
| Swap arrangement | DISCARD swap effect; back-buffer count is 1 windowed / 2 fullscreen |
| Refresh | FullScreen_RefreshRateInHz defaults to D3DPRESENT_RATE_DEFAULT (near line 990) |
| Presentation interval | Starts D3DPRESENT_INTERVAL_DEFAULT; Set_Swap_Interval maps 0/1/2/3 to immediate/one/two/three |
| Aspect | `W3DView::setWidth/setHeight` updates camera aspect to width/height |

Do not equate a configurable FPS cap with a refresh-rate selector. Exact
achieved refresh, synchronization and tearing behavior depend on the driver,
window mode and any local wrapper and are **UNKNOWN until measured**.

## Frame presentation and UI

`W3DDisplay::update` advances visual timing and views; `draw` wraps rendering
in WW3D::Begin_Render/End_Render, draws views/world/overlays/UI and ends the
frame. WW3D forwards rendering lifecycle to its backend; DX8Wrapper presents
through `IDirect3DDevice8::Present` (near lines 1681 and 1757). Reset/device-
cooperative-state checks can skip drawing while the engine continues updates.
Trace blocking Present separately from CPU work and the FramePacer limiter.

Render2D handles screen-space images/primitives and batching; W3DDisplay
maintains coordinate ranges and display-string drawing. WindowManager,
GameWindow, Shell, InGameUI and ControlBar combine data-defined GUI with code
overlays. Any UI scaling work must align visual rectangles, text metrics,
clipping, mouse hit-tests, selection rectangles and tooltips.

## Ultrawide status and likely constraints

Observed: arbitrary-aspect mode filtering and camera aspect updates already
exist. Therefore do not implement "remove the 4:3 filter" again. No runtime
verification establishes 21:9, 32:9 or 5120x1440 correctness.

Hypotheses requiring a visual/runtime audit:

- GUI WND dimensions, control-bar assets, text and overlays may stretch or
  remain too small; mod UIs may have additional assumptions.
- Camera FOV policy, terrain visibility, map-edge constraints and HUD margins
  may behave poorly at extreme aspect ratios despite a correct aspect value.
- Render-target dimensions, GPU caps, shadows, water and fill cost can limit
  extreme resolutions; minimum mode filtering does not prove every pass works.
- Cursor coordinates, DPI virtualization, desktop scaling, Alt-Tab, minimize,
  reset and multi-monitor placement require testing as a system.

Use reproducible captures and input tests at 4:3, 16:9, 21:9 and 32:9 before
changing projection or layouts. Keep world simulation and selection command
semantics stable.

## Modernization boundaries

First characterize current pacing/display behavior. Then isolate small display
or UI fixes behind options with default-compatible behavior. The existing
IRenderBackend and remaining direct DX8 dependencies are useful future seam
maps. API replacement, shader conversion and broad renderer rewrites are outside
this task and should follow a separate feasibility study.

The installed Steam directory contains `d3d8.dll` and `d3d8.cfg`. Their identity
and effect are UNKNOWN. An app-local DLL can affect the actual graphics path,
so record it in every benchmark and never infer the effective runtime API solely
from the engine's Direct3D 8 source calls.
