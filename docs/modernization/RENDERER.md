# Renderer and display architecture

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
