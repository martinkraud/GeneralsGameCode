# Gameplay + Rendering Completeness (CP2 / X64.29)

Status: offline batch complete; candidate staged and preflighted (section 8), **not executed**.
Baseline: FIRST PLAYABLE (`x64-28-play-world-fix2`, run `20261013T1227301326226Z`, `X64_28_PLAY_WORLD.md`
section 15). Reference: the Win32/DX8 renderer and retail behavior/assets. Plan history:
`FIRST_PLAYABLE_ROADMAP.md`.

## 1. Measured starting point

| Source | Finding |
| --- | --- |
| Accepted run `world_objects_summary` | `unsupported_state=65589` of 1.40 M meshes drawn (4.7 %), 0 texture failures, 0 unsupported classes |
| Retail W3D sweep (37,350 prototypes, 66,726 meshes) with family counters added in this batch | multi-pass 4,726 meshes (7.1 %), texture mappers 2,922 materials (environment 2,736, linear offset 186), blend pairs 0, MODULATE2X 0, specular 0, post-detail 0 |
| W3D prototype classes | MESH 33,718, HLOD 3,195, AABOX 1, OBBOX 436 - no particle/line/sprite prototypes; those are created by code and INI |
| Missing by design in X64.28 | water, shroud/fog, ghost (fogged) objects, particles, tracers/lasers/ropes/streams, shadows, decals, scorches, tracks, radar, smudge, snow |
| Retail shadow types (Object INI) | `SHADOW_VOLUME` 706, `SHADOW_DECAL` 87 |
| Water | `WaterType = 0` (translucent flat polygons + river strips), `DrawSkyBox = Yes` |

So after the measured material families the visible gap is dominated by the *systems* around the meshes.

## 2. Audit and dependency graph

Legend: **N** native in this batch | **L** legacy code reused | **D** deferred (reason).

```
D3D11Backend TerrainOverlay material  (existing)  ──┐
   + WorldBlend DestAlpha/DestAlphaAdditive          │  new
   + stencil-once depth/stencil state                │  new
   + batch LAYERS (opaque, decal, translucent, shroud overlay, particles)   new, NativeWorldObjects
   + EffectBatch services (emit/texture/white/image) new, NativeWorldObjects
        │
        ├─ materials (N)   multi-pass drawing; texture mappers on the CPU: scale, linear offset, rotate,
        │                  sine/step/zig-zag, classic + environment texgen (Generals mapper.h Native_Evaluate)
        ├─ line objects (N) Line3D box (tracers, ropes) and segmented line ribbons (lasers, projectile streams)
        │                  registered through the same draw-module hooks as meshes
        ├─ particles (N)   real W3DParticleSystemManager under play (the headless dummy was the blocker);
        │                  billboard / ground-aligned / streak / volume layers; ADDITIVE/ALPHA/ALPHA_TEST/MULTIPLY
        ├─ decals + shadows (N) NativeShadowManager implements the legacy ProjectedShadowManager/Shadow
        │                  interface: radius decals, object decals, SHADOW_VOLUME as stencil-once planar
        │                  silhouettes projected along the sun onto the terrain
        ├─ scorches (N)    terrain-following decal from the 3x3 EXScorch01 atlas
        ├─ water (N)       standing polygons (lit, scrolling, soft edge through the destination alpha of the
        │                  X64.27 shoreline pass) and river strips (edge alpha profile)
        ├─ shroud (N)      W3DDisplay::setShroudLevel feeds a CPU level array; terrain-following multiply
        │                  overlay with bilinear cell-center filtering, shroud color, border level
        └─ ghost objects (N) real GhostObjectManager under play; snapshots live in the native registry
                           (fogged building remnants are drawn, then dimmed by the overlay)

Deferred (need infrastructure not shared with the above):
  radar minimap     W3DRadar builds DX8 texture surfaces; needs native raw-RGBA UI residency + CPU port
  terrain tracks    W3DTerrainTracks owns DX8 vertex buffers per vehicle; needs a native strip emitter
  smudge/heat       needs a framebuffer-copy pass (back-buffer sampling) in the backend
  snow/weather      W3DSnow (point sprites) - particle path is ready, system not bound
  tree sway/topple  W3DTreeBuffer animation state; static scenery today
  bibs, waypoints   low visibility; W3DBibBuffer / W3DWaypointBuffer
  water sparkles/bump, sky box, dynamic (point) lights, specular, dual texture stages
  render-to-texture projection shadows (soft blurred), shadows received by objects
  second texture stage / dual-stage materials (counted: `stateDualTexture`)
```

## 3. What the batch implements

Infrastructure (`NativeWorldObjects`): batch layers and an effect-emission service API; shader-state mapping
shared by meshes and effects; per-family unsupported counters. Backend (`D3D11Backend`): `DestAlpha` and
`DestAlphaAdditive` blends, stencil-once depth-stencil state. Materials: every pass of a mesh is drawn (shader,
texture, material and UV set per pass), texture mappers are evaluated per material block
(`TextureMapperClass::Native_Evaluate` in Generals' `mapper.*`; Zero Hour keeps the simple subset and compiles).

Effects (`NativeWorldEffects`): see the graph. Lifecycle: the decal manager is installed when the play loop
starts and removed at display detach; shadows released late are safe; the particle and ghost managers are the
retail ones. All play-only code in shared sources is inside `#if defined(_WIN64)` or gated by
`StartupDiagnostics::ordinaryPlay()` (contract tests extended; Win32 behavior byte-identical).

Receipts added to `startup.log` (observed, classifier route unchanged): `world_objects_families` (state family
counters and mapper kinds) and `world_effects_summary` (lines, ribbons, particles, batches, decals, shadows,
water patches, shroud cells, scorches, dropped).

Crash isolation fallback: environment variable `GENERALS_NATIVE_DISABLE` (comma list of `particles`, `ghosts`,
`shadows`, `water`, `shroud`, `scorches`, `lines`, `mappers`) switches single features off without a rebuild;
default is everything on.

## 4. Regression and asset coverage

| Check | Result |
| --- | --- |
| Retail W3D sweep (37,350 prototypes, 36,861 drawn, 49 intentionally empty): exceptions / chunk overflow | 0 / 0 |
| ... `unsupported_state` / `unsupported_class` after this batch | **0 / 0** (was 7,648 / 0); `mapper_applied` 2,922, multi-pass meshes drawn 4,726; locked by `EXPECT_EQ` in the sweep test |
| Material gallery (environment mapped + multi-pass models: ABARFRCCMD family, AVCOMANCHE, AVPALADIN, AVLEOPARD ...) | WARP images reviewed |
| Line3D / segmented line objects through the native path | WARP pixel test |
| Decal manager (alpha decal, enable/invisible/release), uninstall-then-release | WARP pixel tests |
| Volume shadow of a retail tank projected and blended once per pixel (second shadow identical) | WARP image reviewed + pixel test |
| Water standing/river geometry, shroud overlay (levels, fog, tint, filter, terrain follow), billboard quads | geometry unit tests |
| `DestAlpha` / `DestAlphaAdditive` blends and stencil-once on the backend | D3D11 WARP tests |
| Lifecycle / null-manager: `createRadiusDecal` with absent manager, translator ownership (X64.28 fix tests) | gtest |
| Win64 build + ctest, Win32 full build + ctest, z_generals/z_googletest compile | see section 7 |
| Python contracts (7 files, extended) and PowerShell workflows (5) | pass |

Not covered offline (honest gaps): a particle system driven through the full engine update (needs GameClient,
partition manager and FX lists), the shroud refresh from the live partition manager, ghost snapshots with a
live logic, and the real WaterSettings/PolygonTrigger data of a retail map. These are the first things the
native review exercises.

## 5. Remaining unsupported families (measured)

`unsupported_state` over the retail prototypes is now **0**. What the counters do **not** cover (silently
approximated, listed so the review can tell "missing by design" from "broken"):

- dual texture stages / detail color (`stateDualTexture`, `stateDetail`: 0 in retail W3D)
- specular/secondary gradient and MODULATE2X (0 in retail W3D)
- water sparkles, bump/noise stage, sky box, reflection; river shroud per vertex
- soft (render-to-texture) projection shadows; shadows are planar silhouettes and decals only
- terrain tracks, smudge/heat distortion, snow, tree sway, bibs, waypoints, radar minimap
- volume particle depth is capped to 8 shifted layers; streak ribbons approximate the legacy streak UVs

## 6. Deferred (explicitly out of scope: UI redesign, Modern Experience, extended camera, ultrawide, DLSS,
multithreading, audio/video, future content)

The deferred list above is the next integration phase's input.

## 7. Validation (final source state of the batch)

| Suite | Result |
| --- | --- |
| Win64 build `g_generals z_generals g_googletest z_googletest d3d11_ui_test native_startup_gate_test` | pass |
| Win64 ctest | 17/17 |
| Win32 full build (reference renderer) + ctest | pass, 25/25 |
| `g_googletest` with offline asset env vars (retail archive sweep, WARP model/gallery/shadow tests) | 180 pass, 6 disabled |
| `d3d11_ui_test` / `native_startup_gate_test` | 32 / 31 pass |
| Python contracts (7 files, extended for the new play-only hooks) | pass |
| PowerShell workflow classifiers (terrain, play, game, menu, host) | pass; classifier and acceptance contracts unchanged |
| Stage + preflight | PASS (AMD64, D3D11 only, no D3D8/D3DX imports) |

Contract adjustments (strictness kept, not weakened): the game/menu contracts that pinned `GameEngine.cpp`
byte-identical now pin it to the accepted source with exactly the one particle-manager selection change; the
play contract's shadow-guard checks were updated for the native manager; new checks pin the feature switches,
the play gating of the shared-source hooks, and the layer order.

## 8. Candidate and review checklist

| Item | Value |
| --- | --- |
| Name / path | `x64-29-gameplay-render-completeness`, `build/dev-runtimes/native/Generals/x64-29-gameplay-render-completeness` |
| `generalsv.exe` SHA-256 | `3A004DFFEA42CDB699EC7DB98EAC5AF0E4D2ECB2FC2878DEA2BAA62EB704A566` (7,596,032 bytes) |
| `generalsv.pdb` SHA-256 | `5FCD25F93F8E2F2A5FBFC2958BAE9EB73685AEEBD40BC03672410DBDEB8144CD` |
| `manifest.json` SHA-256 | `57D617B85746726EC556D7FB4350FC30925A7ECC4BBB82DF1739CDE6D1F3C89D` |
| `ordinaryplay.json` SHA-256 | `D4028B8F16154EDD36C54CE57A708CF1646B08B381AC345AB0DB13D9A26CA190` |

```powershell
pwsh -NoProfile -File scripts/native-runtime/Invoke-NativeOrdinaryPlay.ps1 -Title Generals -Name x64-29-gameplay-render-completeness -RunReviewedOrdinaryPlay
```

If a feature misbehaves (or a crash needs isolating) relaunch with, for example,
`$env:GENERALS_NATIVE_DISABLE = 'particles,ghosts'` set in the shell first; names: `particles`, `ghosts`,
`shadows`, `water`, `shroud`, `scorches`, `lines`, `mappers`.

Visual / gameplay checklist for the review (Lone Eagle again; it has water, bridges and shoreline):

1. Shroud: unexplored map is black at the start and uncovered as units move; explored-but-unseen terrain is
   dimmed (fog); no bright seams at cell borders; terrain edge beyond the map stays dark.
2. Water: lakes/rivers are drawn as a textured, lit, scrolling surface with a soft shoreline (no hard line or
   missing water); rivers fade at their edges.
3. Shadows: units and buildings have a flat dark shadow on the ground following the sun (no double-dark
   overlaps, no shadow floating on slopes); infantry have a round decal shadow.
4. Effects: muzzle flashes, smoke, fire, dust, explosions appear (additive/alpha); tracers and laser beams are
   visible; rope/supply lines and projectile trails draw; nothing flickers at the screen origin.
5. Scorch marks remain after explosions; radius decals (superweapon/recon drone placement) show their circle.
6. Fogged buildings stay visible as dimmed remnants of what was last seen; they disappear when the building is
   destroyed and the area re-seen.
7. Vehicles: glossy/environment-mapped parts and additive glow passes (headlights, engines) look right.
8. Regression: everything the First Playable run did still works (select, orders, build, produce, Control Bar,
   exit cleanly with `PASS`). Frame rate should remain interactive; note any hitch with many units.
9. Known absent by design: minimap contents, tank/tire tracks, heat distortion, snow, tree sway, bibs,
   water sparkle/bump.

## 9. Native review of `x64-29-gameplay-render-completeness` and fix1

Evidence: run `20261013T2039557801674Z` (valid wrapper PASS, 11,903 logic frames) and the two shorter runs.
Receipts and the composed frames were correlated with each observation.

| Observation | Root cause | Fix |
| --- | --- | --- |
| No smoke, explosions or flames | **Particle layer never submitted.** The effect receipts were healthy (1.2 M quads, 248 k batches emitted, `dropped=0`), so the chain was intact up to the batch list. When the layers were renumbered (overlay before particles) the submission loop was left at `layer <= LayerOverlay`; layer 4 was emitted, counted and never drawn. | Loop by `LayerCount` with a `static_assert`; a layer-by-layer pixel test; `particle_pipeline` receipt (requested / null template / created / removed / particles added / alive / peaks / manager updates and draws / quads / batches / submitted per layer); `ParticleSystemManager::PipelineStats`; render-rate `DRAW()` integration restored (particles moved only in 30 Hz steps); retail `ParticleSystem.ini` test: 806 templates parsed, 60 created, 1,394 particles alive, 48,611 quads, 5,612 particle-layer submissions, visible pixels |
| Square/block pattern on slopes | **Overlay tessellation did not match the terrain.** The shroud overlay was built on its own sub-grid with interpolated `getGroundHeight` heights and a fixed diagonal; on steep terrain parts of its triangles fell under the terrain surface and the unshaded terrain showed through as squares. The same pattern applied to scorches and decals. | Shroud overlay and scorches are built on the terrain lattice with the terrain's own heights and cell diagonals (`getFlipState`), lifted by a constant; decals use a 2.5 unit lattice with a slope-dependent lift |
| Shadows grey / wrong tone | The legacy `renderStencilShadows` blends **SRC = DESTCOLOR, DST = ZERO** (the framebuffer is multiplied by the shadow color; its alpha is unused). Native used SrcAlpha with the shadow alpha, which lightens bright ground toward the color. | `WorldBlend::Multiply` with the map shadow color, stencil-once kept |
| Revealed buildings stay visible in fog | **Original behavior is correct; the dimming was missing.** In Generals a previously seen building stays as a ghost object (`RTS3DScene::renderOneObject`: a drawable info without a drawable is always fogged; objects with status past CLEAR are rendered through the shroud material pass, which darkens them to the fog level). Native drew ghosts at full brightness because the overlay only covers the ground. The live building was correctly hidden (`setFullyObscuredByShroud`) and replaced by its ghost. | `ObjectInput::shrouded` (ghosts, fogged, partial clear incl. the 2 s grace) multiplies lit vertex colors by the shroud level at the vertex position |
| Window too small | 800x600 client | Review window: the logical display, UI and input stay 800x600; the window client is scaled (default 150 % = 1200x900, clamped to the work area, `GENERALS_NATIVE_REVIEW_SCALE` overrides) and DXGI stretches the swap chain; mouse coordinates are mapped back. The accepted `app_window` receipt is unchanged; a `review_window` receipt reports the scale |

Adjacent findings: the retail sweep and effect tests did not cover the submission loop (now covered); particle
textures with a missing file are counted (`exfthrower01.tga` is absent from the retail archives); the
`g_googletest` binary shares process globals, so the particle test creates and restores its own engine
globals; the `ParticleSystemManagerDummy` hid the real-manager path until X64.29 (now exercised in a test).

## 10. Candidate `x64-29-gameplay-render-completeness-fix1`

| Item | Value |
| --- | --- |
| Path | `build/dev-runtimes/native/Generals/x64-29-gameplay-render-completeness-fix1` |
| `generalsv.exe` SHA-256 | `E8368F402D09910A5EFCF8B4C40F26483A8C8DB202A65414155D1528F3325103` (7,602,688 bytes) |
| `generalsv.pdb` SHA-256 | `D65622127E757B7FA95DD76D70F47E3F3B01AFE84D039B1E33651599949023AC` |
| `manifest.json` SHA-256 | `78AA3A28DE506112E63A1315C528ACF653B9C715530716183A9338005834B636` |
| `ordinaryplay.json` SHA-256 | `604FD7E08470C34BE763211A5B66A596983C421905272489A87E7149031CBC01` |

```powershell
pwsh -NoProfile -File scripts/native-runtime/Invoke-NativeOrdinaryPlay.ps1 -Title Generals -Name x64-29-gameplay-render-completeness-fix1 -RunReviewedOrdinaryPlay
```

Validation: Win64 build + ctest 17/17, Win32 full build + ctest 25/25, `g_googletest` 183 pass (retail sweep
`unsupported_state=0`, particle pipeline, layers, shroud dimming), `d3d11_ui_test` 32, `native_startup_gate_test`
32, 7 Python contract files, 5 PowerShell workflows, stage + preflight PASS.

Inspect in the next review: particles during combat, destruction and building fire/smoke (and
`particle_pipeline` in `startup.log`); shadow tone (a bluish/dark multiply, no lightening) and no overlay
squares on mountain slopes; fogged ghost buildings dimmed to the fog level and visible live buildings bright;
the 1200x900 window (mouse position and edge scrolling still exact at the corners); and everything from the
first checklist.
