# X64.27: terrain completion and map infrastructure (Control Bar, extra-blend, shoreline, roads, bridges)

Status: **candidate staged, not executed.** One combined developer gate. Builds on accepted X64.25
(first terrain frame) and X64.26 (retail cloud/noise, camera exercise). Same strict token
`-nativeOrdinaryTerrain`; every X64.25/X64.26 gate is retained.

## 1. Control Bar (profile-local, legacy lifecycle verified first)

Legacy lifecycle: game start calls `ShowControlBar(FALSE)` (`GameLogic.cpp:2107`), which switches to
`CONTROL_BAR_STAGE_DEFAULT`, scales the viewport to 0.80 and registers `WIN_ANIMATION_SLIDE_BOTTOM`
(500 ms) with `m_animateWindowManager`; the window starts below the screen and is moved in by
`m_animateWindowManager->update()`, which only `ControlBar::update()` runs. That function returns at
once for `m_headless`, so under the headless world selection the bar stays parked at y = 1216 (X64.26
evidence). The scheme manager's image animations are also driven only from there.

Fix (smallest correct): `ControlBar::update()` keeps its headless early return for every profile;
under `ordinaryTerrain()` it first calls the new `ControlBar::updateNativePresentation()`, which runs
exactly the presentation-only statements of `update()` in their original order and conditions:
scheme manager update, `m_animateWindowManager->update()` plus its reversed/finished hide rule and
the general-shortcut manager. It deliberately does **not** run context evaluation, selection/observer
logic, radar attack glow, video manager, beacon/tooltips or anything touching `TheGameLogic`/drawables.
The contract test asserts that none of those appear in the new function.

Scheme art and the radar: the scheme background/foreground images are drawn by the unchanged
`W3DCommandBarBackgroundDraw`/`ForegroundDraw` (`drawImage`), so they become visible with the bar.
The radar remains `RadarDummy` (bezel stays empty, as agreed). Command buttons/portrait content depends
on selection and context evaluation and stays empty in this profile (no selection exists).

## 2. Legacy findings: what each overlay family really is

All four families share the retail road shader family (`ST_ROAD_BASE*`, `roadnoise2.pso` decoded from
`shaders.big`: `mul r0,t0,t1; mul r0,r0,t2; mul r0,r0,v0` = **all four channels** of texture x cloud x
noise x diffuse), blend `SRCALPHA/INVSRCALPHA`, `ZFUNC=LEQUAL`, `ZWRITE=FALSE`, lighting off.

| Family | Legacy code | Geometry/texture | Notes |
| --- | --- | --- | --- |
| Extra-blend (3-way) | `HeightMapRenderObjClass::renderExtraBlendTiles` | per-frame list of visible tiles from `m_extraBlendTilePositions`, UV from `getExtraAlphaUVData`, diffuse alpha = per-vertex blend alpha, atlas (stage-one texture) | `m_use3WayTerrainBlends` (INI: 1). Mode 2 (debug white) is not reproduced. |
| Roads | `W3DRoadBuffer::drawRoads` | per road type XYZDUV1 buffers, stacking order, bounds = draw window, texture per type (`TERoad*.tga`, MIP_LEVELS_3, repeat) | cloud+noise as above |
| Bridges | `W3DBridgeBuffer::drawBridges` | meshes copied from W3D models into one shared XYZNDUV1 buffer, one range per bridge, texture per bridge/damage state | `detailAlphaShader`: alpha test >= 0x60, **depth write on, culling off**, cloud via `ST_CLOUD_TEXTURE`, no macro noise; shroud pass absent |
| Shoreline | `BaseHeightMapRenderObjClass::renderShoreLinesSorted` | tiles with vertices under the water plane, u = water-depth LUT index | **writes destination alpha only** (`D3DCOLORWRITEENABLE_ALPHA`); no color effect until the water renderer consumes it. Implemented faithfully as an alpha-only pass so it is ready for the water milestone and measurable. |

## 3. Implementation

- **Backend** (`D3D11Backend`, `WorldSubmission.h`): new `WorldMaterial::TerrainOverlay`
  (rgba = tex*[diffuse]*[cloud]*[noise], optional alpha test, optional no-cull, optional alpha-only
  write mask, source-alpha blend), shared cloud/noise texgen with `TerrainCombined`;
  `Create_Dynamic_Geometry` / `Update_Geometry_Indices` for content that changes per frame.
- **Texture decode** (`WorldTextureLoad.h`, SDK-free, unit-tested): dds-first DXT1/3/5 and 24/32-bit
  TGA to RGBA8 with a box-filtered 3-level chain (legacy MIP_LEVELS_3); used for roads and bridges.
- **Engine integration** (profile-gated):
  - `BaseHeightMap` creates the `W3DRoadBuffer` under `ordinaryTerrain()` (CPU-backed buffers; the
    opt-ins are set before `TheTerrainVisual.init`, verified from the startup log order).
  - `W3DBridgeBuffer::allocateBridgeBuffers` allocates under the profile instead of returning for
    headless; `refreshBridgeStates()` was extracted verbatim from `drawBridges` (Win32 path unchanged).
  - `W3DRoadBuffer::forEachNativeBatch` / `W3DBridgeBuffer::forEachNativeBridge` mirror the legacy
    traversal and hand CPU data to `HeightMapRenderObjClass::renderNative`.
  - `renderNative` order: base+cloud+noise terrain, shoreline alpha, extra-blend, roads, bridges.
  - Zero Hour does not compile the Generals-only road/bridge integration (`RTS_GENERALS`, `_WIN64`).

## 4. Offline validation

- `d3d11_ui_test` 29 (WARP): overlay math incl. alpha channel participation (roadnoise2), source-alpha
  blend over target, diffuse on/off, alpha test 0x60 accept/reject, double-sided vs culled, alpha-only
  write (colour untouched, destination alpha = LUT alpha), dynamic geometry partial updates and bounds.
- `g_googletest` 165 / Win32 157: world texture decode (DXT1 4-/3-colour, DXT3, DXT5 interpolation,
  dds-first, TGA orientation, mips), plus all earlier terrain/atlas/transform tests.
- Contract tests (terrain/game/display/host/menu) and the 59-case PowerShell classifier workflow.
- Win64 and Win32 builds (Generals, Zero Hour, tests, WorldBuilder) clean.
- Real-map-derived selection (offline): all shipped maps in `maps.big` were decompressed (RefPack/zlib) and
  scanned for 3-way blend cells, road/bridge points and water polygons; see the map choice below.

## 5. Native acceptance map

`maps\Lone Eagle` (Skirmish, 470x470 cells), chosen from the scan because it contains **all four
families**: 564 extra-blend (3-way) cells, 224 road points, 8 bridge points (4 bridges) and 11 water
areas (soft shoreline). Alternative with the same families if it is unavailable in the Skirmish list:
`USA07` (campaign) or `Heartland Shield`. Alpine Assault (used in X64.25/26) has extra-blend and roads
but no bridges and no water, so it would **not** exercise bridge/shoreline code. The gate classifier
only requires a family when the loaded map reports it, so coverage is never claimed beyond the map:
`terrain_first_frame` records `map_extra/map_shore/map_roads/map_bridges` and the summary carries the
per-family draw counters.

## 6. Developer run

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeOrdinaryTerrain.ps1 -Title Generals -Name x64-27-terrain-completion
./scripts/native-runtime/Invoke-NativeOrdinaryTerrain.ps1 -Title Generals -Name x64-27-terrain-completion -RunReviewedOrdinaryTerrain
```

Skirmish setup: choose **Lone Eagle**, any faction, Start. Do not touch camera controls (the camera
moves itself through the six exercise steps). The run exits after 1800 logic frames.

### Machine PASS (in addition to all X64.25/X64.26 requirements)

- `map_extra/map_shore/map_roads/map_bridges` reported; each family with a non-zero map count has a
  non-zero draw count and each zero count has zero draws; total draws == frames x tiles + all overlay
  draws; shoreline required only when soft water edge is enabled.
- Road/bridge world textures loaded exactly when road/bridge draws occurred.
- Control Bar parent hidden=0 and slid into the display (y in [400,600)), `strip_nonblack_pct >= 25`,
  composed frame >= 80 % non-black; evidence images `terrain_world_frame.bmp`/`terrain_ui_frame.bmp`.
- `live_residencies=0`: atlas, tiles, macro textures, dynamic road/extra/shore/bridge geometry, LUT and
  decoded road/bridge textures all released before the backend.

### Visual expectation

Terrain as X64.26, plus: smooth 3-way texture corners (no hard texture cut where three terrain types
meet), asphalt/dirt roads on the ground following terrain with soft alpha edges and cloud shadows
passing over them, bridges standing in their water gaps with their original textures and no
see-through holes except the alpha-tested railing detail, the original Control Bar frame/art along the
bottom 20 % with an empty radar bezel and no command buttons (no selection). There is **no visible
water** (not implemented) so water gaps show terrain; shoreline is invisible by design (alpha only).

### Defects to look for

Black or wrong-textured roads (UV/texture decode), roads floating or sinking (z), flipped road/bridge
triangles or missing bridge sides (cull/winding), bridges drawn translucent/blocky (alpha test),
bridges missing for a destroyed state, road texture stretched (repeat/UV), extra-blend corners
showing a visible diagonal seam or double-lit tiles (blend/lighting), Control Bar missing,
half-slid, offset or with obvious missing artwork, a UI bar covering world pixels above row 480,
flicker when scrolling (dynamic geometry), and any crash/hang at map start (bridge/road buffer load).

## 7. Deliberately out of scope

R3 meshes (buildings, units), water, shadows, particles, shroud/fog, props/scorches, radar rendering,
extended camera/zoom (recorded for the later Modern Experience phase).

## First run: FAIL (20261013T0626246230411Z) and fix1

Machine evidence: exit `0xC0000005` (access violation) 4 seconds after start, 159 records, last row
`TheTerrainVisual.init begin` with no completion; no graphics/game rows reached. The failure is inside
the terrain render object constructor, i.e. before any map is loaded, so none of the X64.27 rendering
code ran.

Root cause (source-confirmed): the bridge buffer constructor now allocates under the profile and
`W3DBridgeBuffer::allocateBridgeBuffers` called `VertexMaterialClass::Get_Preset(PRELIT_DIFFUSE)`.
The vertex-material presets are created by `DX8Wrapper::Init` (`VertexMaterialClass::Init`), which the
native presentation never runs, so `Presets[type]->Add_Ref()` dereferenced null. (Roads allocate no
material; the bridge allocation was previously skipped entirely for headless.)

Fix: the vertex material is not requested in the terrain profile (the native path has no use for it).
Defensive hardening in the same place: the legacy `TextureClass` objects for road and bridge textures
are no longer created in the profile (the native path decodes the files itself), so no legacy texture
loader request is made either. New receipts `terrain_bridge_buffer_init`, `terrain_road_buffer_init`,
`terrain_roads_load`, `terrain_bridges_load` bracket the new engine integration so any further fault
is localized to the exact phase. Candidate: `x64-27-terrain-completion-fix1`.
