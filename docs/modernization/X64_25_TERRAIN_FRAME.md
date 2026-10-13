# X64.25: first native D3D11 terrain world frame (Generals, AMD64)

Status: **candidate staged, not executed.** Source commit `4f475a99d`; stage `x64-25-ordinary-terrain`
(EXE SHA256 `BA18DAABA3E2B47C7FE2BC53637148292AA472149CBC22B8ADFC79B56D74D814`, PDB SHA256
`52B62EE4B47533B3DC1720C6262432887D6F62F0E37F6711029E68B8924230B5`, GUID
`c7323867-d82e-41f3-b190-20a5da4eae06` age 75, no app-local DLLs, no `runs` directory). This document
is the integration record, the explicit fidelity scope, the PASS/FAIL contract and the
developer commands. X64.22-24 profiles are unchanged and remain accepted.

## What the profile does

`-nativeOrdinaryTerrain` is a fourth strict token with the same 11-token grammar as
the earlier ordinary profiles. It implies `ordinaryGame`/`ordinaryMenu`/`ordinaryHost`
(real window, real WindowManager/Shell, ordinary engine loop, live GameLogic, headless
world/audio/radar selection) and additionally draws the **terrain only** with the
native D3D11 backend under the original UI.

Everything outside terrain stays absent and explicit: no meshes, units, buildings,
trees, props, roads, bridges, scorches, water, shadows, particles, shroud, fog, cloud/
noise multiply pass, shoreline/extra-blend tiles, selection or in-world UI hints.

## Integration (all source-backed; each step has a gate on `ordinaryTerrain()`)

| Step | Change | Reference behavior |
| --- | --- | --- |
| Profile opt-ins | `D3D11Backend::Enable_CPU_Backed_Geometry(true)` and `Enable_Depth_Buffer_For_W3D(true)` only in `initOrdinaryHostPresentation` terrain branch, before the backend exists. Depth is requested through the backend (`Create_For_W3D`), so `ww3d.cpp`, `PresentationLifecycle` and `DX8Backend.cpp` stay byte-identical to the accepted baseline. | Win32 never sets them. |
| Allocation | `HeightMapRenderObjClass::initHeightData` allocates the CPU-backed index/vertex tiles when `m_treeBuffer != nullptr \|\| nativeTerrain`; once per map size in the profile. No tree/prop/bib buffer is created. | Unchanged when the profile is off. |
| Atlas + UV state | `WorldHeightMap::getTerrainTexture` builds the CPU atlas (`BuildTerrainAtlas`: inverted rows, A1R5G5B5 quantization, 4-texel class borders, 3 box mips like `MIP_LEVELS_3`) and the same `m_terrainTexHeight`/flip-state tables the D3D8 path produced; no D3D8 surface or `FilterTexture`. | D3D8 `TerrainTextureClass` path untouched. |
| Camera | The tactical view is a real `W3DView` in this profile (`W3DInGameUI::createView`), driven by `W3DDisplay::update` -> `updateViews()` (camera lock/scripted movement, `updateCameraTransform`, terrain draw window via `updateCenter`). Drawable iteration, scene light iterator and picking are null-scene safe. | Dummy view elsewhere. |
| Terrain pass | `HeightMapRenderObjClass::renderNative`: per VB tile, base (`TerrainBase`, opaque, depth write) then blend (`TerrainBlend`, SRCALPHA/INVSRCALPHA, no depth write); world = render-object transform, view = `CameraInvTransform`, projection = `Get_D3D_Projection_Matrix`, viewport = camera viewport x render target; sampler = legacy global bilinear/trilinear flags, clamp. Tile geometry is uploaded once and re-uploaded only when the legacy buffer version changes. | Same `updateVB` vertices (prelit diffuse, uv0/uv1). |
| Frame | `drawOrdinaryMenuFrame`: color-only `Begin_Render`, backend depth clear, terrain pass, then the original UI (`winRepaint`, cursor). | UI composition unchanged. |

## Known, deliberate differences from the DX8 terrain (not defects of this gate)

1. Single base+blend combiner: no cloud/noise multiply, no `ST_TERRAIN_BASE_NOISE*` variants.
2. Dynamic (object/particle) lights are not applied: only static baked lighting.
3. No shoreline, 3-way extra-blend, roads, bridges, props, scorches, shroud/fog.
4. Texture reduction/LOD options are ignored (full-resolution atlas, 3 mips).
5. A1R5G5B5 quantization is replicated on the CPU and re-expanded by 5-bit replication;
   the D3D8 sampler's exact expansion/rounding may differ by at most one 8-bit step.
6. The legacy 8-stage Nvidia single-pass path is not reproduced; two passes are used.

## Offline validation performed

- AMD64 and Win32 builds (Generals + Zero Hour + tests + WorldBuilder) clean.
- `g_googletest` Win64 161 / Win32 157 passed; `native_startup_gate_test` 29,
  `d3d11_ui_test` 21 (WARP: depth order, blend, mipped atlas, residency counts).
- New: `WorldTransformTest` (legacy column-vector chain == D3D row-vector chain for
  world/view/projection), terrain-token grammar/implication tests, terrain contract
  test (gates, baseline-identical `ww3d.cpp`/lifecycle, null-scene rules, explicit pass),
  38-case PowerShell classifier workflow.
- Not covered offline (requires developer run): the integration with real map data,
  camera behavior in a live Skirmish, and visual correctness.

## Developer run

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeOrdinaryTerrain.ps1 -Title Generals -Name x64-25-ordinary-terrain
./scripts/native-runtime/Invoke-NativeOrdinaryTerrain.ps1 -Title Generals -Name x64-25-ordinary-terrain -RunReviewedOrdinaryTerrain
```

Interact exactly as in X64.24: MainMenu -> Solo/Skirmish -> Skirmish setup -> Start.
Let the game run; it exits itself after 1800 logic frames (about 60 s).

### Visual expectation

After the loading screen the playfield shows the map terrain (flat-shaded
ground textures with baked light/shadow, blended texture transitions) under the
original in-game UI/control bar. There must be **no** units, buildings, trees, water,
fog/shroud or particles. Terrain edges may appear as black. Acceptable at this
stage: no clouds, hard/blocky texture sampling if bilinear is off, missing
shorelines/roads. Not acceptable: scrambled UVs (texture tiles in the wrong place),
inside-out/missing triangles, depth artifacts (far terrain over near), black terrain
with a valid map, flicker, or a hang/crash.

### Machine PASS

`Test-NativeOrdinaryTerrainEvidence` requires everything X64.24 required plus
`terrain_optin` (cpu_backed_geometry=1, depth_buffer=1), exactly one
`terrain_first_frame` (one atlas upload, draws == 2 x uploaded tiles) after `game_start`,
`terrain_summary` with >= 30 frames, no residency leak (`live_residencies=0`), zero
dumps/shutdown bytes/timeouts. FAIL = any missing/duplicate row or failed phase; the
run is preserved and not re-run automatically.

## First run: FAIL (20261012T2230013281405Z) and fix1

Machine evidence: exit 1, 217 records, game started (players=5, alpine assault),
then `game_loop fail: Unsupported D3D11.1 renderer state/operation` before any terrain
frame (`terrain_summary frames=0`), no dumps, clean teardown rows absent.

Root cause (source-reviewed): the CPU atlas is deliberately truncated to the legacy
`MIP_LEVELS_3` (3 levels of 2048-wide), but `D3D11Backend::Create_Mipped_Rgba8_Residency`
required every chain to reach 1x1 and threw the generic unsupported error on the first
terrain frame. D3D11 immutable textures accept partial chains. Fix: the backend accepts a
validated partial chain (each level exactly halves), `WorldTest` now asserts a partial
chain is accepted and a wrong-size level is still rejected. `renderNative` additionally
reports invalid viewport / non-finite transforms with descriptive messages instead of the
generic backend error. The failed run is preserved under the original stage; a fresh
stage `x64-25-ordinary-terrain-fix1` replaces it.

## fix1 run: machine PASS, visual FAIL (20261012T2233246334227Z) and fix2

Facts kept separate. Machine: exit 0, 16 tiles uploaded, 32 draws first frame,
frames=3601 draws=115232, `live_residencies=0`. Visual (developer): the world viewport
stayed completely black while the original cursor/UI was alive.

Diagnosis (evidence + source): everything submitted (atlas 1 upload, 16 tile uploads,
2 draws per tile) but no pixels resulted. Legacy WW3D sets `D3DRS_CULLMODE = D3DCULL_CW`
(`shader.cpp` `_PolygonCullMode = D3DCULL_CW`, `dx8wrapper.cpp` default): clockwise
(on-screen) triangles are culled, counter-clockwise are front faces. The R1a world
rasterizer was written with the opposite convention (`FrontCounterClockwise = FALSE`,
documented as "D3DCULL_CCW"), so every terrain triangle was back-face culled. The R1a
tests encoded the same inversion (their quad was clockwise), which is why they passed.
The terrain cell pattern (0,2,3)(0,1,2) over (x,y),(x+1,y),(x+1,y+1),(x,y+1) is
counter-clockwise seen from +Z, i.e. a front face under the legacy default.

Fix: `FrontCounterClockwise = TRUE` for the world pipeline (same as Render2D). Tests:
the shared quad is now CCW; `LegacyBackFaceCullingAndInPlaceVertexUpdate` asserts
clockwise is culled; new `TerrainTileThroughLegacyCameraAndProjectionIsVisible` renders a
HeightMap-ordered tile through a Westwood right-handed camera and the
`Get_D3D_Projection_Matrix` form and requires lit pixels, centered on the look-at point.

New instrumentation (next run tests a concrete hypothesis): after the 30th terrain frame
the target is read back before the UI is drawn and `terrain_probe` records
`nonblack_pct`, the center pixel, the camera position and forward vector. The classifier
now fails a run whose terrain pass leaves < 5% non-black pixels, so a black world can no
longer be a machine PASS. If the probe is nonzero but the screen is black, the cause is
UI-layer covering; if zero, the camera/transform/texture values in the probe locate it.

## Acceptance: X64.25 first native D3D11 terrain frame - FORMALLY ACCEPTED

Reviewed run (immutable, never rerun):
`build/dev-runtimes/native/Generals/x64-25-ordinary-terrain-fix2/runs/20261013T0503179685543Z`,
stage `x64-25-ordinary-terrain-fix2` (EXE SHA256 `F373137CE9EE...6696C9`, re-hashed equal
to the staged binary; built from `b6920957c`).

Recorded separately:

- **Machine gate: PASS.** OS exit 0, no timeout/error, isolated paths match, 237 contiguous
  same-process records, zero `fail` rows, no dumps, `shutdown-exception.bin` empty,
  `live_residencies=0` before backend release. Classifier valid: map Alpine Assault with
  5 players, 1800 live logic frames, `FP_summary calls=14782`, ordinary `exitGame`;
  `terrain_first_frame draws=32 atlas_uploads=1 tile_uploads=16`; `terrain_summary
  frames=3600 draws=115200 atlas_uploads=1 tile_uploads=16 tile_updates=0`; 4845 presents.
- **Terrain pixel probe:** `terrain_probe nonblack_pct=80 center_rgb=546F30
  cam=(1351,1458,329) fwd=(-0.00,0.79,-0.61)` read back after the terrain pass and before
  the UI: a plausible grass-colored center pixel, camera pitched about 37 degrees down.
- **Developer visual gate: PASS.** Terrain is clearly visible; grass/ground rendered
  correctly under the original UI. Visible terrain is therefore confirmed on AMD64/D3D11.
  (The previous fix1 visual FAIL was the face-culling inversion documented above.)

### Bottom black strip: expected by the original viewport contract (not a defect)

Developer observation: a black horizontal area over roughly the bottom 10 percent of the
viewport. Source-backed explanation, no speculative change made:

- `GlobalData::m_viewportHeightScale` defaults to **0.80** (`GlobalData.cpp:847`, "Default
  value for the original Control Bar"). `ControlBar::setScaledViewportHeight()` sets the
  tactical view height to `displayHeight * 0.80` whenever the Control Bar is shown
  (`ControlBar.cpp:3060`, `ControlBarCallback.cpp`), `setFullViewportHeight()` restores 100
  percent. `W3DView::setHeight` narrows the camera viewport to that fraction.
- The native pass draws exactly the camera viewport: the probe's `nonblack_pct=80` equals
  the 480 of 600 rows the 80 percent viewport covers, i.e. the terrain fills the whole world
  viewport and the strip below it (rows 480-599, 20 percent) is by contract not world.
- In the original game that strip is covered by the opaque Control Bar art. The remaining
  black seen (about half of the strip) is, most likely and not verified, the part of the UI layer not painting
  opaque Control Bar art in this headless-world profile. That is a UI/Control Bar fidelity
  item (windows/images drawn by `winRepaint`), independent of the world pass; it was not
  changed here. The command-line `-fullViewport` would set scale 1.0 but is not part of the
  strict profile grammar.

Open follow-up (not blocking acceptance): confirm with a UI-only review which Control Bar
windows are drawn in-game under this profile (the world viewport itself is correct).

### Known gaps carried forward (unchanged from the scope section)

No cloud/noise pass, dynamic lights, shoreline/extra-blend, roads, bridges, props,
scorches, shroud/fog, meshes, water, shadows or particles. `tile_updates=0` over the run
means camera scrolling/terrain re-lighting was not exercised by this scripted run.
