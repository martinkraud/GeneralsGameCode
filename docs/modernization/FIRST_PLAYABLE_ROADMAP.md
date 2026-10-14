# FIRST PLAYABLE GENERALS - roadmap, dependency graph and batch strategy

Status: written 2026-10-14; CP1/First Playable closed 2026-10-13 (native run `20261013T1227301326226Z`); CP2 in progress.
This file is the planning source for the native AMD64/D3D11 path from the accepted terrain frame to a
first playable skirmish. `AGENT_HANDOFF.md` holds the current state; this file holds the plan.

## 1. Objective

FIRST PLAYABLE GENERALS (native AMD64, D3D11, original Generals, Win32/DX8 retained as the reference):

Main Menu -> Skirmish setup -> faction/map -> Start -> see the real base, buildings and units -> select units
-> issue orders -> use the original Control Bar -> build/produce -> move/attack -> the AI plays -> finish or
leave cleanly.

Full visual fidelity is **not** required. Non-blocking omissions go to the fidelity backlog (section 6) but
may never bypass rendering correctness, determinism (simulation stays authoritative at 30 TPS) or gameplay
contracts. Architecture constraints stay as before: native AMD64, D3D11 backend, `W3DDisplay`/`WW3D`
remain the shared owners, renderer-neutral lifecycle adapters, no generic D3D8 emulator, original Control
Bar (no replacement UI), original camera/zoom defaults.

## 2. Cadence change (developer decision, 2026-10-14)

Larger coherent offline batches; one staged native candidate per batch; fewer human native runs (target:
2-4 checkpoints to First Playable). Inside a batch the agent inspects, implements, refactors, builds, runs
every automated test (Win64/Win32/WARP/contract/workflow), diagnoses and fixes offline failures with
understood causes, and only then stages one candidate. Not changed: no native execution by the agent, no
visual acceptance by the agent, no weakened gates, immutable failed-run evidence, push only to
`origin/dev/modern-engine` (merge, never rebase/force).

| Checkpoint | Content | Status |
| --- | --- | --- |
| X64.27 | terrain completion | ACCEPTED (fix2 run `20261013T0638444013011Z`); bridges/shoreline unexercised |
| **CP1 = X64.28** | world objects + interactive world + play host | **ACCEPTED 2026-10-13 (fix2 run `20261013T1227301326226Z`) - FIRST PLAYABLE baseline** |
| CP2 = X64.29 | Gameplay + Rendering Completeness (see `GAMEPLAY_RENDERING_COMPLETENESS.md`): water, shroud/fog, ghosts, particles, lines, shadows, decals, scorches, material families | **ACCEPTED 2026-10-13 (fix2, run `20261013T2148410437663Z`, evidence `evidence/x64-29-gameplay-render-completeness-fix2/`)** - regression baseline with X64.28 |
| CP3 = X64.30 | World/UI Completeness (`WORLD_UI_COMPLETENESS.md`) | **ACCEPTED 2026-10-14 (run `20261014T0518489306297Z`, evidence `evidence/x64-30-world-ui-completeness/`)** |
| CP5 = X64.31 | Original Game Experience / Normal Launch (`NORMAL_LAUNCH.md`) | **ACCEPTED 2026-10-14 (x64-31-fix5, evidence `evidence/x64-31-accepted/`)** |
| CP6 = X64.32 | Original experience completion + frontend/QoL audit (`X64_32_ORIGINAL_EXPERIENCE_AUDIT.md`) | **ACCEPTED 2026-10-14 (x64-32-fix1: Save A, Load A, frontend A)** |
| CP7 = X64.33 | Modern Experience foundation: display/window/aspect/UI scale/Options/HUD (`X64_33_MODERN_DISPLAY.md`) | **ACCEPTED 2026-10-14 (x64-33-final2-options)** |
| CP8 = X64.34 | Smooth presentation completion + performance instrumentation (`X64_34_SMOOTH_PRESENTATION_PERFORMANCE.md`) | **ACCEPTED 2026-10-15 (x64-34-smooth-presentation)**; first performance baseline receipt = first X64.35 task |
| CP4 | Full skirmish acceptance (AI, clean end) | planned |

## 3. Dependency graph from accepted terrain to First Playable

Legend: **N** native and offline-validated in X64.28 | **L** legacy code reused unchanged under the profile |
**B** backlog (does not block play) | **X** blocking and not yet done.

```
accepted terrain (X64.25-27: tiles, cloud/noise, extra-blend, roads, bridges, shoreline, Control Bar frame)
 |
 +-- W3D data model without DX8 (L)  MeshClass/MeshModelClass/HLod/HTree/HAnim load headless (replays proved it)
 |     +-- mesh geometry, normals, UV, DCG/DIG, per-poly shader/texture/material            N  NativeWorldObjects
 |     +-- transforms: rigid mesh = bone/container world transform; skin = HTree deformed    N
 |     +-- animation advance (Animatable3DObjClass::Render base step, logic time)           N  AnimatedSkin test
 |     +-- materials: fixed-function vertex lighting (ambient+3 directional, emissive/DCG)  N  CPU, per vertex
 |     +-- shader state: blend pairs, alpha test 0x60, depth mask, cull, gradient            N  WorldBlend
 |     +-- textures: DXT1/3/5 + 24/32-bit TGA (census: all of Textures/Terrain/Patch.big)    N  WorldTextureLoad
 |     +-- team color: ZHCD palette row, ZHCA hue shift, HOUSECOLOR vertex material          N  legacy Recolor math
 |     +-- alpha override / stealth / tint / selection tint                                  N
 |     +-- tread UV scroll (Material_Override)                                               N
 |     +-- multi-pass, specular, env map, dual stage, MODULATE2X, point/dynamic lights       B  counted (unsupported_state)
 |
 +-- drawable lifecycle: W3DModelDraw/W3DDebrisDraw register the render object without a scene   N  registry
 |     +-- Drawable::draw() re-enabled (animation, bones, turret, recoil, muzzle hide)       N  W3DView::update
 |     +-- tracer/laser/projectile-stream/police-car lights are scene-only                    N  guarded (invisible)  B
 |     +-- trees/props (tree/prop buffers do not exist headless)                             N  static native scenery (no sway/topple)
 |
 +-- selection and visuals
 |     +-- picking: ray vs registry (RTS3DScene::castRay equivalent), cursor hints           N  castRay + pickDrawable
 |     +-- box select / double click: screen projection of drawable centers                   L  unchanged
 |     +-- health bars, veterancy, ammo, names: Drawable::drawIconUI via Render2D            L  drawNativeWorldOverlays
 |     +-- selection ring (legacy: projected terrain decal)                                  N  screen-space circle
 |     +-- move hints / placement anchor+arrow render objects                                N  registry
 |     +-- drag-select rectangle, messages, superweapon timers (InGameUI::DRAW)              L
 |
 +-- input and command path (translators attached by GameClient::init)
 |     +-- mouse/keyboard -> WindowTranslator, hotkeys, Selection/LookAt/Command translators L  real Mouse/Keyboard
 |     +-- camera: scroll/rotate/zoom (normal camera input, original defaults)               L  W3DView::update runs
 |     +-- extended zoom, free camera                                                         B  (Developer Mode / Modern Experience)
 |
 +-- Control Bar (original windows)
 |     +-- context UI, command buttons, build/production queue, power, science             L  ControlBar::update full under play
 |     +-- radar minimap render                                                              B  RadarDummy (empty bezel)
 |
 +-- world presentation not required for play
       water (W3DWater), shadows (decals/volumes), particles, scorches, shroud/fog texture,
       fogged ghost objects, bibs, waypoints, terrain tracks, lens flare, video, audio       B
```

Everything on the critical path to the loop above is **N** or **L** after X64.28. What is deliberately absent
is listed in section 6 so the first playable review can tell "missing by design" from "broken".

## 4. X64.28 batch (CP1) - what was built

Profile: `-nativeOrdinaryPlay` (Generals AMD64 only; implies `-nativeOrdinaryTerrain` and below; the terrain,
game, menu and host tokens keep their accepted behavior byte for byte at the source level - see
`test_ordinary_play_contract.py`).

### 4.1 Renderer (Core/GameEngineDevice `NativeWorldObjects`, Core/Libraries WW3D2 backend)

- `WorldBlend` (SrcAlpha/Opaque/Additive/AdditiveAlpha/Multiply/InvMultiply/Premultiplied) on the
  TerrainOverlay material; D3D11 alpha-channel factors use the alpha equivalents of `*_COLOR` factors.
- Registry of render objects fed by `W3DModelDraw`/`W3DDebrisDraw` (else-branch of the legacy
  `m_3DScene` add/remove; no-op outside the play profile, so replays and Win32 never track objects).
- Per frame: frustum cull by bounding sphere, drawable hidden/obscured/ghost skip, sub-object hidden bits,
  HLod current LOD + additional models, CPU skin deformation (HTree), alpha-override, ALIGNED/ORIENTED
  camera-facing meshes, per-vertex lighting from `m_terrainObjectsLighting` (infantry light scale, tint),
  pooled dynamic geometry chunks (60k vertices), opaque in registration order then translucent back to front.
- Team color exactly as `W3DAssetManager::Remap_Palette` (ZHCD/ZHCA) on the decoded RGBA texture and
  `Recolor_Vertex_Material` for HOUSECOLOR meshes; recolored textures cached per color.
- Animation: `Animatable3DObjClass::Render` base step (no DX8 mesh renderer involved) is called for each
  HLod before its sub-objects are read; `WW3D::Sync` is bound to the logic update in the play profile.
- Static scenery: trees (alpha-tested double-sided foliage state of `W3DTreeBuffer`, atlas texture name
  from the module data, Art/Terrain then Art/Textures) and props become native render objects.

### 4.2 Interaction

- `W3DView::update` runs `Drawable::draw()` for the play profile (everything else keeps returning early).
- `W3DView::pickDrawable` casts against the registry; `iterateDrawablesInRegion`, box selection and the
  command translators are unchanged legacy code.
- `W3DView::drawNativeWorldOverlays` (icon UI pass + selection rings) then `InGameUI::DRAW` (which repaints
  every window, so the original Control Bar is drawn by the legacy path).
- `ControlBar::update()` runs the complete update under play (context UI, buttons, production).
- User-driven loop: no scripted camera, no logic-frame bound, 60 minute wall-clock bound, exit on
  match end/quit/boundary. A message tap counts clicks, keys, selections, moves, attacks, builds, production.

### 4.3 Offline validation (all green at commit time)

| Check | Result |
| --- | --- |
| Real model render (tank, humvee, barracks, skinned ranger; team colors) via WARP | image reviewed, 39 meshes, 0 texture failures |
| Skinned animation advances with logic time through the native path | 7184 differing bytes between frames |
| Foliage (pine/oak) alpha-tested with atlas texture | image reviewed, 0 translucent batches |
| Registry ray cast hits all four objects, misses the sky | pass |
| Every renderable prototype in W3D.big (37 350 prototypes, 36 861 rendered, 52 intentionally empty hidden FX) | 0 exceptions, 0 chunk overflow, 11 missing texture names (counted, white fallback) |
| Texture census Textures/Terrain/Patch.big | only DXT1/3/5 DDS and 24/32 bpp bottom-up TGA (all decodable) |
| Blend-pair WARP tests, startup gate tests, workflow classifier (45 checks), static contracts | pass |
| Win64 `g_googletest` 169 tests, Win64 `z_generals` link, Win32 full build | pass |

## 5. Native run: what to expect (X64.28 candidate)

Stage + preflight (no game launched): `Stage-NativeOrdinaryPlay.ps1`, `Test-NativeOrdinaryPlay.ps1`.
Run (developer only): `Invoke-NativeOrdinaryPlay.ps1 ... -RunReviewedOrdinaryPlay`. Select **Lone Eagle**
(bridges/shoreline coverage still owed from X64.27) and play as long as you like; leave through the quit menu.

Expected: terrain as X64.27, the player's base with buildings, workers/vehicles with team color, trees,
original Control Bar with the command card changing with the selection, health bars on selected/damaged
units, green selection rings, RMB-scroll / arrow keys / wheel zoom, box selection, move/attack orders,
building placement and production, AI base activity. Missing by design: see section 6.

Machine receipts written next to `startup.log`: `world_objects_first_frame`, `play_progress` (every
15 s x3 then 60 s: logic frame, CRC, money, selection, camera, objects drawn, input counters),
`play_frame_N.bmp` (3 composed frames), `play_summary`, `world_objects_summary`, existing terrain receipts.

## 6. Fidelity backlog (no gameplay contract is bypassed by these)

Water; shadows (projected decals, volumes); particles and weapon/explosion effects; tracers, lasers,
projectile streams, ropes (Line3D/SegmentedLine objects are not drawn); shroud/fog-of-war texture and fogged
ghost objects; radar minimap render (RadarDummy); bibs and terrain tracks/scorches; waypoints; tree sway and
topple; dynamic/point lights, specular, environment maps, multi-pass materials, MODULATE2X; LOD predictive
optimizer; lens flare/dazzle; video, audio. Each is counted where it is detectable
(`unsupported_class`, `unsupported_state`, `texture_failures`).

## 7. Developer Mode (RECORD ONLY - not implemented in this batch)

A future, explicitly enabled native Developer Mode (for example `-devmode`), strictly isolated from the
normal game and from multiplayer (never active in network/replay-compatible simulation without an explicit
opt-in that taints the match). Candidate content: free camera and extended zoom; FPS and frame-time overlays;
object IDs and selection debug; collision/bounds visualization; pathfinding and AI debug views; reveal
map/shroud controls; instant build; unlimited resources; unit spawn; game-speed control; renderer
diagnostics (draw/triangle/texture counters - `NativeWorldObjects::Stats` is the seed); stress scenarios.
The existing Stage4B.1 interactive adapter (`DEVELOPER_MODE.md`, `-devMode`) is the legacy-renderer
precedent and its safety rules (offline skirmish only, no networking/replay) carry over.
