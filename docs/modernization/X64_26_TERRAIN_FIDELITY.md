# X64.26: terrain fidelity (cloud/noise) and dynamic terrain exercise

Status: **candidate staged, not executed.** Builds on the accepted X64.25 terrain frame.
Profile token and grammar are unchanged (`-nativeOrdinaryTerrain`); the X64.25 gates are
retained and extended, never weakened.

## 1. Cloud / noise multiply: legacy behavior and the native form

### What the legacy renderer actually runs

`HeightMapRenderObjClass::Render` selects `ST_TERRAIN_BASE`, `_NOISE1` (cloud), `_NOISE2`
(macro noise/lightmap) or `_NOISE12` (both) from `useCloud()` (`m_useCloudMap` and not
night) and `m_useLightMap`; both default to **Yes** in `GameData.ini` (`INI.big`). Three
implementations exist and are equivalent in intent:

1. `TerrainShader2Stage` (any card): pass 0 base, pass 1 alpha blend, pass 2 draws the
   same tiles with `SRC=DESTCOLOR, DEST=ZERO`: stage 0 cloud (`TSCloudMed`), optionally
   stage 1 noise modulate, both with camera-space-position texgen.
2. `TerrainShader8Stage` (single-pass NVIDIA path) reuses pass 2 of (1).
3. `TerrainShaderPixelShader`: the retail path on ps_1_1 hardware. The three shaders in
   `shaders.big` were decoded token by token (staged copy, read only):
   - `terrain.pso`: `tex t0; tex t1; lrp r0, v0.a, t1, t0; mul r0, r0, v0`
   - `terrainnoise.pso`: the above, then `mul r0, r0, t2`
   - `terrainnoise2.pso`: the above, then `mul r0, r0, t3` (t2 = cloud, t3 = noise)

   i.e. `rgb = lerp(atlas(uv0), atlas(uv1), diffuse.a) * diffuse.rgb [* cloud] [* noise]`,
   no 2x factor, no clamping other than the render target. This equals pass 0 + pass 1 + the
   multiply pass of (1) (the atlas alpha is 1 for filled texels, so the blend factor is the
   diffuse alpha).

### Texture coordinates (camera-space texgen)

`D3DTSS_TCI_CAMERASPACEPOSITION` with texture matrix `inverse(View) * Scale(s,s,1) *
Translate(ox,oy)` (`updateNoise1/2`) maps `vertex*World*View*inverse(View)` = the vertex
transformed by the terrain world matrix, so `uv = worldXY * s + offset`, `s = 1/(63 *
MAP_XY_FACTOR / 2)`. The cloud offset scrolls at (-0.02, -0.03) per second of logic frame
time and wraps at +/-1 (`updateCloud`); noise has no offset. Wrap addressing; cloud is
bilinear, noise min=point/mag=linear (stage states in `TerrainShaderPixelShader::set`),
no mip filter (D3D8 default `D3DTEXF_NONE`: base level). Macro texture is `TSNoiseUrb.tga`
unless a map/worldbuilder macro name is set; cloud is `TSCloudMed.tga` (both ship as
DXT1 `.dds` in `Textures.big`, 256x256 / 128x128).

### Native implementation

- `WorldMaterial::TerrainCombined` + `WorldSubmission::{cloud, noise, cloud_offset,
  texgen_scale}`; one HLSL pixel shader implements exactly the retail math above. Cloud
  and noise are optional (null = factor 1 = `terrain.pso`).
- `HeightMapRenderObjClass::renderNative`: single draw per tile (was two passes in X64.25);
  loads `TSCloudMed`/macro noise once through the engine file factory with the legacy
  dds-first rule; missing/unsupported textures are an explicit failure, never skipped;
  scrolls the cloud with `WW3D::Get_Logic_Frame_Time_Seconds()`, which the terrain profile
  now feeds from `W3DDisplay::update` exactly like the non-headless update.
- Same selection predicate as `Render()` (`useCloud()`, `m_useLightMap`); no global is
  modified.

### Offline validation (reference-based)

- `WorldTest.CombinedTerrainMatchesRetailPixelShaderMath`: combiner == hand-computed
  `lrp`*diffuse and == the legacy two-pass result (pass 0 opaque + pass 1 SRCALPHA).
- `...CloudAndNoiseMultiplyAndAbsentFactorsAreOne`: base, cloud, cloud*noise, noise-only.
- `...TexgenUsesWorldXYScaleAndCloudOffsetWraps`: world XY mapping, wrap by whole texture,
  half-texture scroll swaps the halves.
- `WorldTransformTest.LegacyCameraSpaceTexgenEqualsWorldXYScaleAndOffset`: the legacy
  matrix chain (rigid view, `inverse(View)*Scale*Translate`) evaluated on an arbitrary
  camera equals `worldXY*s + offset`.
- All 25 `d3d11_ui_test` tests on WARP, 161 Win64 / 157 Win32 `g_googletest` tests pass.

## 2. Bounded scripted camera exercise (dynamic terrain)

Inside the ordinary game loop, relative to `game_start`: six `TheTacticalView->lookAt`
moves at logic frames 120, 330, 540, 750, 960, 1170, +700/0, +700/+700, 0/+700, -700/+700,
-700/0, 0/-700 world units from the start pivot (clamped 300 units inside the map extent).
700 units (70 cells) exceeds half the 129-cell draw window, so `updateCenter` re-centers
the window and `updateBlock` rewrites vertex-buffer tiles, which the native pass picks up
through `Update_Geometry_Vertices` (tile version counters). No input is injected. Each
move writes `terrain_move`; 90 logic frames later the next terrain frame reads the target
back before the UI and writes `terrain_motion` (pivot, non-black %, cumulative
`tile_updates`, cloud/noise frame counters).

## 3. Control Bar painting review (bounded, documented; no world dependency)

Source findings (static, no change made):

- The 0.80 `ViewportHeightScale` contract is verified (X64.25): the bottom 20 % is HUD.
- `ControlBar::update()` returns immediately when `m_headless` (`ControlBar.cpp:1402`), so in
  this profile the scheme manager update, animate-window manager and video manager never
  advance. Background art is drawn by `W3DCommandBarBackgroundDraw` ->
  `ControlBarSchemeManager::drawBackground` when a scheme is current.
- The radar is `RadarDummy` under the headless selection (`Win32GameEngine::createRadar`),
  so `W3DLeftHUDDraw` paints nothing in the radar bezel: a black square at the HUD's left
  is expected until a radar renderer exists (the likely source of the black area the
  developer saw).
- No terrain code reads or depends on any of this. The terrain pass is not affected.

New receipts (informational, the gate never fails on UI content): `ui_probe`
(composed frame after UI: whole non-black %, bottom-strip left/mid/right thirds),
`ui_probe_window` (ControlBarParent/BackgroundMarker/LeftHUD/RightHUD hidden/pos/size),
`ui_probe_facts` (radar kind, viewport scale and heights) and two evidence images in the
run directory: `terrain_world_frame.bmp` (terrain before UI, terrain frame 30) and
`terrain_ui_frame.bmp` (composed frame, terrain frame 60). The review result will be
recorded from these at acceptance; any HUD fix is a separate milestone.

## 4. Candidate gate

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeOrdinaryTerrain.ps1 -Title Generals -Name x64-26-terrain-fidelity
./scripts/native-runtime/Invoke-NativeOrdinaryTerrain.ps1 -Title Generals -Name x64-26-terrain-fidelity -RunReviewedOrdinaryTerrain
```

Interact as before (MainMenu -> Skirmish -> Start with a daytime map, e.g. the same Alpine
Assault). Do not touch the mouse/keyboard camera controls during the ~60 s run; the camera
moves itself.

### Machine PASS (in addition to every X64.25 requirement)

- `terrain_first_frame`: one atlas upload, one combined draw per uploaded tile,
  `use_cloud`/`use_noise` recorded; `terrain_summary` draws == frames x tiles.
- `cloud_frames == frames` when cloud enabled (else 0), `noise_frames == frames` when noise
  enabled (else 0), `macro_loads == use_cloud + use_noise`.
- `tile_updates > 0`; exactly six `terrain_move` and six `terrain_motion` rows; every
  motion probe non-black >= 5 %; cumulative updates increase in >= 3 steps; >= 4 distinct
  pivots; `terrain_probe` non-black >= 5 %; `ui_probe` present.
- `live_residencies=0` before backend release (cloud/noise/atlas/tiles all freed).

### Visual expectation

Terrain as in X64.25 but with soft cloud shadows drifting slowly across it (one cloud
texture repeat spans 315 world units; the drift is 0.02 repeats per second in x and 0.03 in
y, about 6 and 9 world units per second) and subtle macro noise that breaks tiling
repetition (the terrain should look slightly darker and more varied than the flat X64.25
result, like the retail game). Six automatic camera jumps: after each, terrain fills the
viewport again (beyond the map edge may be black), without frozen/stale patches, seams
between 32x32 tiles, flicker or UV scrambling.

Not in scope: shoreline, roads, bridges, props, scorches, shroud, meshes, water,
shadows, particles, radar, HUD fixes.

## Acceptance: X64.26 terrain fidelity and dynamic terrain exercise - FORMALLY ACCEPTED

Reviewed run (immutable, never rerun):
`build/dev-runtimes/native/Generals/x64-26-terrain-fidelity/runs/20261013T0533511829002Z`
(stage `x64-26-terrain-fidelity`, built from `e23b1dbb2`, EXE SHA256 `09EB69BC...378D57`).
Machine PASS and developer visual PASS are recorded separately.

### Machine gate: PASS (independently re-read)

OS exit 0, no timeout/error, isolated paths match, 255 contiguous same-process records,
zero `fail` rows, no dumps, `shutdown-exception.bin` empty, `live_residencies=0` before
backend release (atlas, 16 tiles, cloud and noise all released). Alpine Assault, 5 players,
1800 logic frames, `FP_summary calls=13654`, ordinary `exitGame`.

- Terrain: `terrain_first_frame draws=16 atlas_uploads=1 tile_uploads=16 use_cloud=1
  use_noise=1`; `terrain_summary frames=3600 draws=57600 (= 3600 x 16) atlas_uploads=1
  tile_uploads=16 tile_updates=84 cloud_frames=3600 noise_frames=3600 macro_loads=2`.
  Cloud and noise ran on every terrain frame; both macro textures loaded exactly once.
- `terrain_probe nonblack_pct=80 center_rgb=374A21` (pre-UI, same camera as X64.25).

### Scripted camera exercise: behaved as designed

Six `terrain_move` rows at logic frames 120/330/540/750/960/1170 and six `terrain_motion`
probes 90 frames later, six distinct pivots, every probe 80 % non-black (the full 0.80
viewport):

| step | target pivot | cumulative `tile_updates` |
| --- | --- | --- |
| 1 | (2051,1862) | 16 |
| 2 | (2051,2290) | 28 |
| 3 | (1351,2290) | 44 |
| 4 | (651,2290) | 60 |
| 5 | (651,1862) | 68 |
| 6 | (1351,1162) | 84 |

`tile_updates` strictly increases on every step (X64.25 had 0), so
`Update_Geometry_Vertices` was exercised through the ordinary `updateCenter`/`updateBlock`
path with correct content afterwards. Steps 2-4 targets were clamped to 300 units inside
the map extent (y = 2290) by design. The developer's observation that manual camera
controls did not respond is consistent with this profile (no gameplay input translators are
attached to the camera); the jumps seen were these six scripted moves.

### Image evidence

`terrain_world_frame.bmp` / `terrain_ui_frame.bmp` were decoded and inspected: the world
viewport (rows 0-479) is 100 % lit, shows plausible grass/ground with blended texture
transitions, soft darker cloud-shadow regions and macro noise detail, no seams or
scrambled UVs. Rows 480-599 are 0 % lit in both images.

### Developer visual gate: PASS

Terrain visible and correct throughout; abrupt scripted camera jumps; no stale patches,
tile seams, flicker or UV scrambling.

### Control Bar painting review: result (bounded investigation, no code change)

Root cause of the black bottom strip, from the receipts and source:

- `ui_probe_window`: `ControlBar.wnd:ControlBarParent hidden=0 pos=0,1216 size=800x184`;
  `LeftHUD pos=7,1243`, `RightHUD pos=621,1283`, `BackgroundMarker pos=8,1395`. The whole
  bar is positioned at y = 1216 on a 600-pixel display: it is **not hidden, it is parked
  below the screen**. `ui_probe` strip non-black = 0 % in all three thirds.
- `ShowControlBar(FALSE)` (`ControlBarCallback.cpp:496`, called at game start from
  `GameLogic.cpp:2107`) registers `WIN_ANIMATION_SLIDE_BOTTOM` (500 ms) with
  `m_animateWindowManager`; the window starts off-screen and is brought in by
  `m_animateWindowManager->update()`, which only runs from `ControlBar::update()`. That
  function returns at once under the headless world selection (`ControlBar.cpp:1402`), so
  the slide-in never advances and the bar stays parked. The same early return also keeps the
  scheme manager and video manager idle.
- The viewport is correctly scaled to 80 % (`view_height=480 display_height=600
  viewport_scale=0.80`) because `ShowControlBar` calls `setScaledViewportHeight()`, which
  is why a 20 % band exists but nothing paints into it.
- `radar=dummy` is also recorded: even with the bar on screen the radar bezel would stay
  empty until a radar renderer exists.

Conclusion: the strip is a UI-layer consequence of the headless `ControlBar::update`
guard, independent of the terrain pass (whole-viewport terrain is verified). Recommended
bounded follow-up (not done here, one profile-local change): call
`TheControlBar->m_animateWindowManager->update()` (or `ShowControlBar(TRUE)` immediate)
under `ordinaryTerrain()` so the bar slides in, then review scheme art and radar separately.

### Carried forward

Not rendered: shoreline/extra-blend tiles, roads, bridges, props, scorches, shroud/fog,
dynamic lights, meshes, water, shadows, particles, radar/HUD content. Static terrain
re-light on time-of-day change was not exercised.
