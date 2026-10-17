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
| CP9 = X64.35 | Performance profiling + evidence-driven optimization (`X64_35_PERFORMANCE_OPTIMIZATION.md`, `X64_35_BASELINE_ANALYSIS_AND_DISK_AUDIT.md`, `NATIVE_CANDIDATE_RETENTION.md`) | **ACCEPTED AND CLOSED 2026-10-16 (x64-35-decal-reuse)**: instrumentation retained; exact decal-geometry reuse kept (-0.59 ms/frame, clean-window frame -11.7 %, native visual check accepted); shadow-volume cache rejected and reverted; per-object / shroud / ZoneCalc / helicopter shadow / late Present step remain documented backlog |
| CP10 = X64.36 | Enhanced gameplay UI: USA, China, GLA on Shared Framework v1 (`X64_36_CP10_CLOSEOUT.md`) | **ACCEPTED AND CLOSED 2026-10-17** (USA `201b126b1`, China v2 `dac818a0d`, GLA `c1cde26c1`): Options > Gameplay > Interface Original/Enhanced for all three base Generals factions; General Powers built from the real science graph; Framework v1 frozen; Original preserved; cross-faction polish deferred (`ENHANCED_HUD_POLISH_BACKLOG.md`) |
| CP11 = X64.37 | Modern Display / Settings completion (`X64_37_MODERN_SETTINGS.md`, `SETTINGS_COMPLETION_AUDIT.md`) | **ACCEPTED AND CLOSED 2026-10-16 (x64-37-modern-settings-v4, `8d4f6838d`)**: real resolution / display-mode / monitor selection with transactional Apply-Keep-Revert; Render Scale (world-only intermediate target) natively verified; Interface selection, compact Global Powers dock and live Original/Enhanced switching accepted; audit of every Modern Options control. Settings are not reopened unless a regression appears |
| CP12 = X64.38 | CPU scalability / multithreading foundation (`X64_38_CPU_SCALABILITY.md`) | **FOUNDATION DELIVERED, no parallel code shipped**: tick-distribution stats, determinism fingerprint, worker-count policy, phase attribution, headless heavy AI-vs-AI benchmark (serial baseline reproducible, digest `74F2CB528BED42ED`); evidence: the tail is path search (51 %) and ZoneCalc (30 %), ZoneCalc is an O(zones) scan per union (algorithmic, not a threading problem); next = serial ZoneCalc union-find with verify mode (X64.39) |
| CP13 = X64.39 | ZoneCalc algorithmic optimization (`X64_39_ZONECALC_OPTIMIZATION.md`) | **DELIVERED (serial, bit-identical)**: min-root union-find replaces the O(zones) scan per merge; legacy kept as the oracle; differential tests + `GENERALS_ZONE_VERIFY` (388 recalculations, 0 mismatches); heavy-benchmark digest `74F2CB528BED42ED` unchanged; ZoneCalc 23.5 -> 5.5 ms per call, tick p99 23.4 -> 8.4 ms, ticks > 16.67 ms 836 -> 450; severe tail (> 33 ms, worst 239 ms, deadline misses) unchanged = path search (73 % of the slowest 1 %). Next: serial path-search investigation (X64.40) |
| CP14 = X64.40 | Path-search deep profiling + serial optimization (`X64_40_PATH_SEARCH_INVESTIGATION.md`) | **DELIVERED (serial, exact)**: the retail open-list insertion walk (76-96 % of A* time in the expensive searches) is answered by an exact index (`OpenListIndex`); open-list evolution digest identical to the original walk, 23.5 M insertions verified in-engine (+33.9 M on 13 more maps), heavy-benchmark digest `74F2CB528BED42ED` unchanged; worst tick 265 -> 160 ms, ticks > 33.3 ms 188 -> 45, ticks > 100 ms 32 -> 2, missed deadlines 389 -> 75, max backlog 2.9 s -> 127 ms. R1: the native client diverges at frame 1600 too (record vs re-simulation). Next: serial line-phase work (X64.41) |
| CP15 = X64.41 | Path line-phase optimization + retail Game Speed audit + R1 tooling (`X64_41_PATH_LINE_PHASE_AND_R1.md`, `RETAIL_GAME_SPEED_AUDIT.md`) | **DELIVERED (serial, exact; R1 CLOSED: replay header lacked Skirmish starting cash, fixed and natively verified)**: per-search passability memo for the line walk (72 M hits re-verified in-engine + 50.6 M on 16 maps, decision digest and fingerprint unchanged); worst path search 167 -> 75 ms, ticks > 33.3 ms 45 -> 34, > 66.7 ms 15 -> 10, missed deadlines 79 -> 48, max backlog 141 -> 91 ms. Retail Normal PROVEN = 30 logic frames/s in Generals and Zero Hour; the retail Skirmish Game Speed slider was the FPS cap (audit only, nothing implemented). R1: per-frame CRC trace + comparison tooling + staged diagnostic exe. Stage4B.1 guard redesigned to a mutation-tested function-level contract. Next: periodic script tick / update-module spikes (X64.42) |
| CP16 = X64.42 | Simulation tail reduction (`X64_42_SIMULATION_TAIL_OPTIMIZATION.md`, `GAME_SPEED_DESIGN.md`) | **DELIVERED (serial, exact)**: the 70-110 ms "script tick" every 300 frames and the 55-84 ms update-module spikes were one cause - a synchronous `FlushFileBuffers` per launch-log record on the logic thread (special power / payload delivery checkpoints); normal launch no longer flushes per record (`GENERALS_NORMAL_LOG_FLUSH=1` restores). Heavy benchmark A/B x4: ticks > 33.3 ms 34 -> 4.8, > 66.7 ms 8.8 -> 2.0, missed deadlines 45 -> 8, p99.9 17.8 -> 9.1 ms, script worst 85 -> 3.9 ms; fingerprint `74F2CB528BED42ED`, 16-map digest + log equality. Next tail = exhaustive failing path searches (X64.43). Game Speed: design document only |
| CP17 = X64.43 | Exhaustive failing path searches (`X64_43_FAILING_PATH_INVESTIGATION.md`) | **DELIVERED (investigation; decision C: no justified optimization)**: off-by-default read-only diagnostic `GENERALS_SIM_PATHFAIL`; the remaining expensive A* failures are pockets sealed by stationary non-idle allied / uncrushable enemy units (terrain and zones say reachable, 109 of 112), requested by AI skirmish scripts (GUARD state); repeats are few and the pathfinder state changes ~96 000 times between repeats, so no exact reuse is provable. Serial CPU phase at diminishing returns; next: real-game scalability benchmark (measurement), not another optimization |
| CP18 = X64.44 | Real-game scalability / chokepoint endurance benchmark (`X64_44_ENDURANCE_BENCHMARK.md`) | **DELIVERED AND CLOSED** (measurement only): native 51.5-minute Twilight Flame match (human + 5 Brutal AI): peak 1 227 objects / 469 units, tick mean 4.15 ms (12 % of budget), p99 10.97 ms, 6 ticks over 33.333 ms, 29.998 TPS, lag 0.35 s - no knee, no sustained degradation; the match re-simulates headless with zero CRC mismatches (first long real-game determinism validation). The historical huge-army chokepoint workload was NOT reproduced. Decision A: no multithreading before Zero Hour; serial CPU phase stays closed. Next: Zero Hour native validation |
| CP19 = X64.45 | Zero Hour native audit and bring-up (`X64_45_ZERO_HOUR_NATIVE_BRINGUP.md`) | **CLOSED 2026-10-17, HUMAN NATIVE VALIDATED (candidate v3)**: Zero Hour is playable natively; the black terrain strips (stale native terrain atlas, fixed) are gone; clean exit, audio healthy; the v3 replay re-simulates deterministically on Win64 (0 mismatches, 8 741 frames) but not on Win32; brief minor lag observed, undiagnosed (no telemetry). Backlog in section 12.4; recommended next: X64.46 Zero Hour game-mode completeness (Campaign, General's Challenge, Save/Load, movie-skip policy) |
| CP20 = X64.46 | Zero Hour game-mode completeness (`X64_46_ZERO_HOUR_GAME_MODE_COMPLETENESS.md`) | **CLOSED 2026-10-17, HUMAN NATIVE VALIDATED (candidate `x64-46-zh-modes-v1`)**: Campaign, General's Challenge and Save/Load work natively (human: no crash, black screen or stuck flow; loaded game correct). One blocker fixed (General's Challenge crashed on a null movie stream); movie-skip policy consistent and logged (`movie/skipped`); Save/Load keeps 470/470 objects bit-identical. NOT validated: mission victory/defeat progression (deferred), movies (absent: no Bink on AMD64). R-6 (cross-architecture replay) still open. |
| CP20a = X64.47A | **Zero Hour Steam overlay ZIP feasibility (PREVIEW)** (`X64_47A_ZERO_HOUR_STEAM_OVERLAY_ZIP.md`) | **OPEN, package built + automated disposable-overlay test passed (15/15); real Steam test on a second PC pending (human)**. Finding: Steam's Play starts `Generals.exe`, so the AMD64 runtime ships under that name (one replaced file). Not a feature milestone; X64.47 below remains next |
| CP21 = X64.47 | **NEXT (recommended, not started)**: Zero Hour visual and options parity | the 3 hidden ZH-only Options controls, animated UV mappers (Rotate, SineLinearOffset, StepLinearOffset, ZigZagLinearOffset), a decision on a still-image policy for the missing movies, mission victory/progression coverage as a human checklist |
| CP22 = X64.48 | Enhanced HUD for Zero Hour | 12 sides, 14-slot command bar, General's powers, per-side theme review |
| CP23 = X64.49 | Audio by ear, endurance, performance, replay follow-up | ZH endurance run (includes the unverified brief-lag input), audio review, R-3/R-5/R-6 cross-architecture replay question; performance work only here, after the above |
| CP24 = X64.50 | **Release engineering design (FUTURE, not started)** | design only: layout of the ZIP, asset discovery/verification, launcher, update/uninstall story, licence/asset boundary; see section 8 |
| CP25 = X64.51 | **First distributable ZIP and second-PC validation (FUTURE, not started)** | build the ZIP, validate ZIP -> extract -> Play on at least a second PC before any public release |
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

## 8. FUTURE: user-facing MKRaudstein distribution (RECORD ONLY - not started, nothing of it is implemented)
Recorded 2026-10-17 as the end goal after the remaining Zero Hour completeness and Modern Experience work (CP21-CP23). Do **not** start it before those are done and the developer asks.
**Objective:** a user-facing "Modern Experience by MKRaudstein" Generals / Zero Hour distribution comparable in usability to established community releases:
- one downloadable ZIP; target experience ZIP -> extract -> Play;
- no Visual Studio, CMake or source checkout, and no developer staging scripts, required by the player;
- works with a legitimate, existing Generals / Zero Hour installation; **no proprietary EA game assets are redistributed** (no BIGs, no Bink, no codecs bundled);
- automatically locates a legitimate Steam / EA installation, or lets the player pick one; verifies the required original game data;
- keeps the original installation unmodified where practical (the stage-by-reference approach of the developer tooling is the starting point; the Steam directory stays read-only);
- launches the AMD64 modernized Generals or Zero Hour runtime directly; ideally one simple MKRaudstein launcher for both titles;
- validated on at least a second PC (clean user, different GPU/driver, no developer tools) before any public release.
**Known inputs for that milestone:** current staging copies assets into `build/dev-runtimes/...` (developer layout); the 64-bit registry view is empty for the game (defaults apply, WOW6432Node holds the keys; base-game assets for Zero Hour are found by a recursive `*.big` scan of the working directory); user data lives in the Documents data folders and is backed up by `Run-ZeroHourCandidate`; the missing Bink decoder is a product decision (still-image policy or a licensed decoder) for CP21 or CP24; the licence/asset boundary needs a written policy.

## 9. Short ordered roadmap from here to the first distributable ZIP
1. **X64.47** Zero Hour visual and options parity (3 Options controls, animated UV, movie still-image decision, victory/progression human checklist).
2. **X64.48** Enhanced HUD for Zero Hour.
3. **X64.49** Audio, endurance, performance (incl. the unverified brief-lag input) and the replay / R-6 follow-up.
4. **X64.50** Release engineering design (ZIP layout, asset discovery and verification, launcher, licence boundary).
5. **X64.51** Build the first distributable ZIP and validate it on a second PC; only then consider a public release.
