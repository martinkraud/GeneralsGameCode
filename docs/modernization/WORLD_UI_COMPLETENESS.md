# X64.30 World/UI Completeness (Generals)

Status: offline batch complete, candidate `x64-30-world-ui-completeness` staged and preflighted (not executed).
Baselines preserved: X64.28 First Playable and X64.29 fix2 (accepted 2026-10-13). Evidence of the accepted baseline:
`docs/modernization/evidence/x64-29-gameplay-render-completeness-fix2/`.

The batch restores the remaining high-value original Generals battlefield presentation: radar/minimap first,
then terrain tracks, building bibs, waypoint/rally lines, the retail water pixel shaders and the sky box, plus
the shroud integration audit and a measured renderer completeness classification.

## 1. Dependency audit (what the radar and the other overlays actually needed)

| Finding | Evidence | Consequence |
| --- | --- | --- |
| The radar was a `RadarDummy` in the play profile | `GameEngine.cpp` creates it with `createRadar(m_headless)`; the play profile keeps `m_headless` TRUE | factory selection per profile (no `GameEngine.cpp` change; its contract pin stays exact) |
| `W3DRadar` could not have run on the native backend anyway | 3 x `TextureClass(w,h,fmt)` + `SurfaceClass` Lock/Clear/Draw_Pixel are DX8 storage; format choice asks `DX8Wrapper::Get_Current_Caps()` | shared CPU-backed dynamic UI texture residency (1.2) |
| `W3DRadar::setShroudLevel` returned early natively | it reads `W3DShroud::getCellWidth()`; the play profile has no `W3DShroud` | native cell size accessor (1.2) |
| The retail `.pso` for water is the *fixed* program set in `LegacyFixedPixelShader.h` | `TrapezoidWaterCode` / `RiverWaterCode` sources | exact combiner semantics for sparkles/noise/alpha-edge (4) |
| `Trees.vso` is not in retail `shaders.big`; `W3DTreeDraw` is not used by any vanilla Generals INI | INI.big scan (0 `W3DTreeDraw`), shaders.big listing | tree sway / tree buffer crush are Zero Hour features: not used by vanilla Generals |
| Bibs and the waypoint buffer are terrain-pass overlays | `HeightMapRenderObjClass::Render`, `W3DBibBuffer`, `W3DWaypointBuffer` headers | new ground layer drawn before objects with PASS_ALWAYS (3) |
| Retail uses `WaterType = 0`, `DrawSkyBox = Yes`, `MakeTrackMarks = Yes` | GameData.ini | trapezoid/river pixel shaders, sky box and tracks are the ordinary-gameplay paths |

## 2. Radar / minimap

### 2.1 Architecture and fidelity

The original `W3DRadar` is the radar. It keeps its three textures (terrain, object overlay, shroud), the
128x128 cell grid, `OVERLAY_REFRESH_RATE`, marker/stealth/event/view-box/hero-icon drawing and the whole `Radar`
base class (events with audio, `localPixelToRadar`, `objectUnderRadarPixel`, Control Bar click handling,
availability through `rts::localPlayerHasRadar`, forced/hidden state). No replacement minimap, no Control Bar
redesign. What changed is only how the three textures are stored and how profile selection happens.

| Layer | Change |
| --- | --- |
| Factory | `Win32GameEngine::createRadar`: `RadarDummy` unless the play profile (and the `radar` native feature; `GENERALS_NATIVE_DISABLE=radar` restores the dummy). Other profiles are unchanged. |
| Shared dynamic UI texture residency (reusable for video and Zero Hour) | `IRenderBackend::Supports_Dynamic_Render2D_Content()` (false on DX8). `TextureClass(w,h,fmt,MIP_1)` becomes CPU-backed content (`Texture2DCPUContent`, zero filled) and `Get_Surface_Level()` returns a CPU-backed `SurfaceClass` (Get_Description / Lock / Unlock / Clear, Draw_Pixel / Draw_H_Line). The content `Revision()` is bumped on every Unlock. `D3D11Backend::Create_Dynamic_Content_Residency` + `Update_Content_Residency` (DEFAULT usage, same conversion as the immutable route: A8R8G8B8, X8R8G8B8, R5G6B5, A4R4G4B4). `Apply_Render2D_Texture` re-uploads only when the revision changed. |
| Formats | `W3DRadar::findFormat` picks from the legacy preference lists skipping R8G8B8 (terrain X8R8G8B8, overlay/shroud A8R8G8B8): what the original selects on hardware without 24 bit textures. |
| Shroud | logic cell size from `NativeWorldEffects::shroudCellWorldSize()` (the partition cell size W3DShroud is initialized with) instead of `W3DShroud::getCellWidth()`. |
| Pure raster rules | `W3DRadarRaster.h`: `shadeForHeight`, `plotMarker`, `stealthAlpha`, `shroudAlpha`, `shroudCellRect`: the legacy formulas moved unchanged and used by `W3DRadar` (tests drive the same code). |

Closeness to original: terrain colors, water/bridge colors, marker pixels (2x2, clipped per cell), team colors,
stealth pulse, shroud/fog alpha (255 / 127 / 0), event triangles, view box and hero reticle are the same code.
Differences are limited to the storage substrate.

### 2.2 Offline coverage

`NativeRadarTest` (7 tests, hermetic, WARP): CPU-backed texture and surface lock contract; a deterministic scene
(height-shaded terrain, two markers at known world positions, a shrouded and a fogged logic cell) rendered through
`TextureClass` -> `Render2DSubmission` -> D3D11 and read back at exact pixels; upload counting (none when
unchanged, one per changed texture, one per shroud batch); raster formulas; shroud rectangles vs
`Radar::worldToRadar` over four map shapes and two cell sizes; aspect-ratio letterboxing; and the **real
`W3DRadar`** (`init`, `buildTerrainTexture` with a terrain-logic/terrain-visual double, `begin/set/endSetShroudLevel`)
on the dynamic path with pixel assertions. `D3D11UITextureTest.DynamicContent...` covers the backend route for
all four formats and rejects updates of immutable residencies.

## 3. Terrain tracks

The legacy `TerrainTracksRenderObjClassSystem` stays the owner of the edge ring buffer, bind/unbind, anchoring,
capping on fog entry, airborne restart, fade timing and expiry (`update()` now also runs in the native display
update). Only the DX8 buffers, the scene registration and `flush()` are replaced: `forEachNativeTrack()` yields
exactly the vertices `flush()` writes (terrain light shade `ambient + diffuse/2`, fade alpha with
`MaxTankTrackOpaqueEdges`, the same strip index pattern), drawn by `NativeWorldEffects::CollectTracks` on the
decal layer with the alpha shader state (SRC_ALPHA, no depth write). The texture is the `TrackMarks` file of the
draw module. Because the decal layer is below the shroud overlay, tracks are darkened by the fog like the
original's framebuffer shroud pass. `GENERALS_NATIVE_DISABLE=tracks` is the fallback. Receipt: `world_overlays`.

## 4. Bibs and waypoint/rally lines (terrain-pass overlays)

* New **ground layer** (`EffectLayerGround`), drawn before opaque objects, with `ShaderClass::PASS_ALWAYS` and no
  depth write (`WorldSubmission::depth_always`, new D3D11 depth state): the legacy `detailAlphaShader` for bibs and
  the additive PASS_ALWAYS line shader of the waypoint buffer. Objects still occlude them (drawn after).
* **Bibs**: `W3DBibBuffer(TRUE)` is the CPU list only (add/replace by id/remove/highlight/clear unchanged);
  `NativeWorldEffects::CollectBibs` draws `TBBib.tga` / `TBRedBib.tga` (clamped, terrain light diffuse, normal then
  highlighted) exactly like `loadBibsInVertexAndIndexBuffers`.
* **Waypoints / rally points**: `W3DWaypointBuffer::drawWaypoints` runs unchanged (selected-unit goal paths in
  waypoint mode, rally point lines with the elbow logic); its `m_line->Render` and node `WW3D::Render` calls go to
  `renderLine`/`renderNode`, which in the native profile snapshot the line (`queueGroundLine`) and register a clone
  of the `SCMNode` "hockey puck" as a frame object. The buffer is created on the first native terrain frame (it needs
  the asset manager).

## 5. Water completeness

Retail water is `WaterType = 0` (translucent): standing water through the trapezoid pixel shader, rivers through
the river pixel shader, both PS 1.1 combiners from `LegacyFixedPixelShader.h`.

| Original behavior | Native |
| --- | --- |
| Standing: `r0 = v0 * t0; r0.rgb += t1 * t2` with t1 = `WaterSurfaceBubbles` (uv2 = x/50, (y+0.3x)/50), t2 = `Noise0000` through camera-space position x inverse view (world xy) x 1/16 + river origin | new `WorldMaterial::Water`, `PSWaterStanding`; vertex second UV set from `Geometry::standingWater`; noise offset = the water scroll phase |
| River: `rgb = v0.rgb*t0 + (t1*t2 + t3.rgb)*v0.a`, `a = t0.a * t3.a`, t3 = `TWAlphaEdge` across the strip | `PSWaterRiver`; exact 2-vertex-per-row layout (u2 = 1 inner / 0 outer); the alpha edge is sampled on the GPU (the previous CPU profile is only the missing-asset fallback) |
| Shroud on the water (`t3` multiply / per-vertex river shroud) | the native shroud overlay is drawn after water and covers it |
| Soft shoreline (dest alpha), additive option, scroll formulas | unchanged (X64.29) |

If any of the three textures is absent the accepted flat water is drawn instead (never a white sheet). Fallback:
`GENERALS_NATIVE_DISABLE=water`. Receipt fields `water_sparkles` / `water_rivers`.

**Sky box** (`DrawSkyBox = Yes`, `SkyBoxPositionZ`, `SkyBoxScale`, per-map `Skybox*` texture overrides): the
legacy water render object owned it and does not exist in the play profile. `NativeWorldEffects::addFrameObjects`
creates `new_skybox` lazily, centers it on the camera and hands it to the object pass; `replaceSkyboxTextures`
is mirrored (`NativeWorldEffects::replaceSkyboxTexture`). Fallback `GENERALS_NATIVE_DISABLE=skybox`.

## 6. Shroud / fog integration audit

| Family | Result |
| --- | --- |
| Terrain, roads, scorches, decals, tracks, bibs, waypoint lines | covered by the ground-level overlay (layers below it) |
| Bridges | fixed in X64.29 fix2 (per-vertex level) |
| Water, rivers | drawn before the overlay, which darkens them (the legacy PS multiplies the shroud itself) |
| **Trees and props (non-drawable scenery)** | **fixed here**: the legacy tree shader / prop material pass apply the shroud; native scenery objects now use the per-vertex shroud level (`!info -> shrouded`) |
| Fogged buildings (ghosts), partially clear objects | X64.29 |
| Particles, light/effects | not shrouded in the original either (drawn after the terrain shroud pass) |
| Radar | its own shroud texture (2) |
| Sky box | not shrouded (original) |
| **Known deviation** | bibs and waypoint/rally lines are darkened by the native shroud overlay; the legacy order draws them after the shroud pass (undarkened). Rally lines into fog show darker natively. Recorded in 8. |

## 7. Renderer completeness classification (measured against retail usage)

| Family | Class | Evidence |
| --- | --- | --- |
| Radar/minimap, tracks, bibs, waypoints/rally, water shaders, sky box | **required, done** | sections 2-5 |
| Tree sway | **not used by vanilla Generals** | `Trees.vso` not shipped; `W3DTreeDraw` has 0 uses in INI.big |
| Tree crush / push aside (tree buffer) | **not used by vanilla Generals** (Zero Hour feature: `notifyTerrainObjectMoved` exists only in GeneralsMD); object topple is `ToppleUpdate` on normal drawables | GeneralsMD-only call site |
| Snow / weather | **not used by vanilla Generals** | no `Weather.ini`, no map `WeatherSetting` |
| Smudge / heat distortion | **not used by vanilla Generals** | 0 SMUDGE particle systems in INI.big; `smudge_skipped=0` in the accepted run |
| Soft/projected shadows | **later fidelity** | 717 `SHADOW_VOLUME` + 87 `SHADOW_DECAL`; volumes are native planar silhouettes (flat), not true stencil volumes onto objects/slopes |
| Dynamic light pulses (`LightPulse` FX nuggets) | **later fidelity** | 57 FX nuggets; `W3DDisplay::createLightPulse` has no scene in the play profile (terrain/object light boost absent) |
| Water mesh grid (`enableWaterGrid`), wakes/waves (`WaterTracks`/`WaveGuide`) | **later fidelity** | `WaveGuide` in 5 INI entries (OCL/system), water grid is map-script specific |
| Reflection/sea shaders (`WATER_TYPE_1/2/3`) | **not used by vanilla Generals** | `WaterType = 0` |
| Video textures | **deferred (out of scope)** | `W3DVideoBuffer` |
| Material/texture stage behavior | **complete for retail** | retail sweep: 37,350 prototypes, 36,863 rendered, `unsupported_state=0`, `unsupported_class=0`; gallery `mapper_applied=22 multipass=28 unsupported_state=0` |
| Missing source textures | **data defect, same as original** | 11 referenced TGAs absent from the archives (`19_muzzflsh_5`, `cbtconvst2_*`, `housecolor2_s`, `pmbarroadstr`, `ztrefinery01_s`, particle `exfthrower01`) |

## 8. Deliberately deferred fidelity gaps

1. Undarkened ground overlays (bibs, rally lines) in fog: needs the overlay before objects plus per-vertex water
   shroud (documented trade-off).
2. True volumetric stencil shadows (current planar silhouettes).
3. Dynamic light pulses on terrain/objects.
4. Water mesh grid and water wakes/waves.
5. Zero Hour: bib/road/bridge/ghost buffers, `W3DTreeDraw` crush/push-aside (explicitly postponed).

## 9. Offline coverage added

`NativeRadarTest` (7), `NativeWorldOverlaysTest` (4: track bookkeeping + geometry + decal-layer frame; bib list
semantics; queued ground lines snapshot/draw/consume), `D3D11UITextureTest` (+3: dynamic residency, PASS_ALWAYS
overlay without depth write, standing and river water pixel-shader math), `NativeWorldEffectsTest` (+2: legacy
river layout, sparkle UV set), contracts (`test_x64_30_world_ui_completeness_is_pinned`), workflow fixture
(`radar=real`). Existing suites unchanged and passing.

## 10. Native review checklist

See the candidate section at the end of the final report in `AGENT_HANDOFF.md` (X64.30).
