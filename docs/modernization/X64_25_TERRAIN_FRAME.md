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
