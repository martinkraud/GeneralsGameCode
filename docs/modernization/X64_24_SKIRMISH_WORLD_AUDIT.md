# X64.24: Skirmish setup -> map load -> first world frame (audit + first batch)

Date: 2026-10-12. Baseline: accepted X64.23 (`365f42ffc`). Implementation commit
`bc55d3744`. Generals only. No title was executed by the implementer.

## Observed starting point

The accepted X64.23 run ended exactly where this audit begins: the developer's
Skirmish click was refused by the push guard
(`reason=shell_push Menus/SkirmishGameOptionsMenu.wnd`). The real window
manager, Shell, transitions, mouse and keyboard already work on the ordinary
AMD64/D3D11 lifetime.

## Path map (source-backed)

```text
MainMenu ButtonSkirmish (MainMenu.cpp:1327)  -> Shell::push("Menus/SkirmishGameOptionsMenu.wnd")
SkirmishGameOptionsMenu                          UI only: MapCache, player templates, map-preview
                                                 images (Image + Get_Texture), start-spot buttons,
                                                 optional SkirmishMapSelectMenu layout (manager, not Shell)
  Start -> MSG_NEW_GAME(GAME_SKIRMISH) on TheMessageStream
GameEngine::update -> GameClient::update          message stream -> CommandList
                    -> GameLogic::update           processCommandList -> startNewGame()
GameLogic::startNewGame                           loadMapINI, TerrainLogic::loadMap, TerrainVisual::load,
                                                 players/teams/objects (Drawables), scripts, AI, load screen
                                                 object (getLoadScreen: MultiPlayerLoadScreen),
                                                 Shell hide, ControlBar/InGameUI show
steady state                                      logic frames + GameClient::update + TheDisplay->DRAW
```

Facts that shape the plan:

1. **The headless world selection is a supported full-game mode.**
   `GameLogic::startNewGame`, the terrain visual (`W3DTerrainVisual::isXferEnabled`
   documents "only partially initialized in headless mode"), drawables
   (`W3DAssetManager` recolor shortcuts), ghost objects and the tactical view all
   have `m_headless` branches that let a complete skirmish run with no render
   device. `GameEngine::execute` with `-headless` is the upstream route.
2. `m_loadScreen` is created even when headless (`MultiPlayerLoadScreen`); only its
   `init()` is skipped, and its `update()` is null-safe for progress bars
   (LoadScreen.cpp:1448..1481). `updateLoadProgress` throws `QuitGameException`
   when the engine is quitting.
3. `MilesAudioManager` init failure leaves audio flagged on again after
   `GameEngine::init` re-applies the user switches, so a live loop would queue
   playback requests; the profile therefore sets `m_audioOn=FALSE` after the
   GameData INI load.
4. The shell map is the only thing that pushes MainMenu in the stock path; with
   shell map off the menu is pushed from `GameClient::update`'s intro-finished
   branch exactly as in the accepted menu profile.
5. Unaudited screens that must stay refused: OptionsMenu (`getDisplayModeCount` ->
   `WW3D::Get_Render_Device_Desc` -> DX8 enumeration), Credits/Replay/SaveLoad
   (shell map, `TheGameState`), LAN/online (network, GameSpy).
6. All 34 map preview TGAs in `maps.big` are raw 32-bit (supported by the UI
   texture contract); all MainMenu-reachable UI atlases were already censused.

## What this batch implements (profile `-nativeOrdinaryGame`)

Strict 11-token grammar, third distinct token, implies the menu and host
lifetimes (`ordinaryGame()` => `ordinaryMenu()` => `ordinaryHost()`):

| Piece | Behavior |
| --- | --- |
| Loop | `TheGameEngine->update()` (= `Win32GameEngine::update`: GameClient, message stream, GameLogic, OS pump) + `TheFramePacer->update()`: exactly what `GameEngine::execute` iterates, bounded. No private re-implementation of client/logic stepping. |
| Input/UI | Real window manager, mouse (Windows cursors), DirectInput keyboard, ordinary translators and Shell. Original Skirmish setup screens run unmodified. |
| Draw | `W3DDisplay::draw` profile branch: black color-only frame, `winRepaint()` and cursor. The 3D view, terrain, water, objects, particles are NOT drawn (renderer not migrated); every other state is an explicit `Unsupported`. |
| Shell | Push allow-list `Menus/MainMenu.wnd`, `Menus/SkirmishGameOptionsMenu.wnd`; every other push is refused and ends the run with a `shell_push` boundary receipt. |
| Audio | Explicitly off (see fact 3). |
| Bounds | 1800 logic frames after game start (~60 s) then ordinary `exitGame()` (MSG_CLEAR_GAME_DATA) and wait for `!isInGame()`; 15 min wall clock; player quit also ends the run normally. |
| Evidence | `game_start` (mode, map, player count), `game_progress` at logic frames 1/30/150/300/900/1800 with the cached CRC and presents, `game_exit`, `game_summary`. CRCs are recorded for later x86/x64 comparison; they are NOT yet compared. |
| Validation | Launcher classifier requires >= 2 players, >= 150 logic frames, no shell-push boundary, exit `frame_bound` or `game_quit`, plus all X64.22/23 invariants (hardware device, `d3d8=0`, `live_residencies=0`, window released last, no dumps). |

What it proves if accepted: AMD64 GameLogic/AI/script/object creation and map
loading inside the real application with the real UI on the D3D11 presentation;
the in-game UI windows; ordinary game exit. What it does not prove: any world
rendering, determinism across architectures, performance, save/load, replay.

## First world frame: renderer inventory and migration plan

Call-site census (DX8Wrapper/D3D8 state) of the code a world frame touches:

| Area | Evidence |
| --- | --- |
| Shader manager (terrain/water/scene shaders, ~chipset policy) | `W3DShaderManager.cpp` ~710 refs; `-noshaders` forces the fixed-function chipset |
| Water | `W3DWater.cpp` ~259 |
| Terrain texturing (atlas, blend tiles) | `TerrainTex.cpp` ~241 |
| Terrain mesh/tiles/roads/bridges/props/trees | `HeightMap.cpp` 55, `BaseHeightMap.cpp` 29, `FlatHeightMap.cpp` 23, `W3DTreeBuffer.cpp` 45, `W3DRoadBuffer`/`W3DBridgeBuffer`/`W3DBibBuffer` |
| Scene/object passes, shadows | `W3DScene.cpp` 99, `W3DVolumetricShadow.cpp` 84, `W3DProjectedShadow.cpp` 78 |
| WW3D mesh/material/sort pipeline | `dx8wrapper.cpp` 547, `dx8renderer.cpp`, `sortingrenderer.cpp`, `mapper.cpp`, `shader.cpp`, `meshmatdesc.cpp`, `dx8vertexbuffer/indexbuffer` (COM `IDirect3DVertexBuffer8`) |
| Call mix in the device layer | 689 `Set_DX8_Texture_Stage_State`, 207 `Set_DX8_Render_State`, 194 `_Get_D3D_Device8`, 93 `Set_Texture`, 58 `Set_Shader`, 36 `Draw_Triangles` |

Two things are decisive: (a) geometry lives in `DX8VertexBufferClass` /
`DX8IndexBufferClass` COM objects created through the device, and (b) the
fixed-function multi-stage combiner is configured per draw through hundreds of
`D3DTSS_*` calls. A frame therefore needs a neutral GPU-resource and a world
draw contract, not a factory change.

Staged plan (each stage independently testable, shared by both titles):

| Stage | Content | Offline proof |
| --- | --- | --- |
| R0 | Depth/stencil target, depth clear and depth-state variants in `D3D11Backend`, `PresentationLifecycle` depth begin (currently rejected) | WARP readback, lifecycle tests |
| R1 | Backend-neutral vertex/index buffer content + residency (same model as `Texture2DCPUContent`/residency) behind `DX8VertexBufferClass`/`DX8IndexBufferClass`; no COM aliasing | Dual-architecture content tests, WARP upload/readback |
| R2 | Terrain-only world submission: purpose-built D3D11 terrain pass (tile VB/IB, base+blend tile atlas, cloud/noise stages, vertex lighting) replacing `ST_TERRAIN_BASE*` for the profile; per-subsystem shader with pinned reference images from the legacy combiner math | Golden-image tests against a CPU reference of the legacy stage ops |
| R3 | Static/animated meshes through the DX8MeshRenderer sort/batch path with a bounded material set (opaque, alpha-tested, additive, team-color) | Material census over the Skirmish W3D assets + pinned references |
| R4 | Water, roads/bridges, shadows, particles, UI-in-world overlays | per-subsystem |
| R5 | Camera/viewport/shroud/fog and the legacy depth-clear Begin_Render in `W3DDisplay::draw` | integration |

Design stance (consistent with the retained architecture, not a new decision):
purpose-built per-subsystem D3D11 passes with pinned reference tests, **not** a
generic fixed-function emulator, fake `IDirect3DDevice8` or ModernDisplay. Each
stage keeps unsupported operations explicit.

**Open decision for review (not blocking this batch):** the *fidelity target* for
R2/R3. Pinned bit-exact legacy images (expensive, needs a CPU reference of the
D3D8 combiner for every state) versus visually-equivalent modern shaders
validated against tolerance and human review. This changes test cost by an order
of magnitude. Default if not answered: R2 terrain only with tolerance-based
golden images from a CPU reference of the exact stage math.

## First reviewed native run (Generals only)

Stage `x64-24-ordinary-game`, preflighted, never executed; do not rebuild or
restage (a changed build gets a new name):

- Debug GUID `c7323867-d82e-41f3-b190-20a5da4eae06`, age 69.
- EXE SHA256 `0C68A6EC46A8A9FB484BCE9C4EBD570BF6FC67FA4FA18697FE1F2FB8E9FF0E1C`.
- PDB SHA256 `4AC44F9997C87B8D1F2F4F5753AC97BDD6F1ECF96B0B13FBB1FD6E986E5F8034`.
- PE32+ AMD64, 30 imports, no delay/D3D8/D3DX imports, no app-local DLLs.

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeOrdinaryGame.ps1 -Title Generals -Name x64-24-ordinary-game
./scripts/native-runtime/Invoke-NativeOrdinaryGame.ps1 -Title Generals -Name x64-24-ordinary-game -RunReviewedOrdinaryGame
```

Run the second line only after the first prints PASS.

**What to do.** The window opens black; move the mouse to show the original
menu. Click `Solo Play`, then `Skirmish`: the original Skirmish setup screen
opens. Pick any map and keep at least one AI opponent (the defaults are
fine), then press `Start`. Expect no 3D world: the screen stays black with the
in-game UI windows (command bar frame) over it while the logic runs; do not
click in the playfield. After about 60 seconds of game time the run ends by
itself (`exitGame`) and the process exits. You may instead quit early through
the in-game quit menu. Do not use Options, Credits, Load, Online or Network.

**PASS requires all of:** exit code 0; launcher prints PASS, i.e. the original
MainMenu and Skirmish screens were reached, the game started with >= 2 players,
>= 150 logic frames ran, CRC milestones were recorded, exit reason
`frame_bound` or `game_quit`, `d3d8=0`, `live_residencies=0`, window released
after engine cleanup, empty `shutdown-exception.bin`, no dumps, final
`WinMain complete`; and your confirmation that the setup screens looked like the
original and nothing crashed or hung.

**FAIL:** non-zero exit, hang/timeout, crash, a `shell_push` boundary (a screen
outside the allow-list was requested), game never starting, or the launcher
message `Ordinary-game validation failed`. The run directory
`build/dev-runtimes/native/Generals/x64-24-ordinary-game/runs/<timestamp>` is
preserved untouched and nothing reruns automatically; report it with the
console output and `startup.log` (the `fail` row names the phase).

## Known unverified (the first run exercises them)

1. Real-manager in-game UI windows (ControlBar, InGameUI) never executed in this
   process; `ControlBar::update` is a no-op under the headless selection, so the
   command bar is a static frame.
2. `GameLogic` + AI + scripts on AMD64 for the first time natively (only the
   engine init was proven); `getCRC(CRC_CACHED)` milestones record the cached
   CRC and are not compared with Win32.
3. Blocking loops inside `startNewGame` (`LoadScreen`, transition wait) call
   `TheDisplay->draw()` through the UI-only frame.
4. Skirmish setup screens' own resources (map preview TGAs, start-spot images)
   through the ordinary global texture cache.
5. Game end with the real Shell: nothing pushes the menu again (headless
   selection), so the run ends by the bounds above.

## X64.24 first run: FAILED (validator-rejected), root cause found

Preserved run (immutable, never rerun):
`build/dev-runtimes/native/Generals/x64-24-ordinary-game/runs/20261012T2112467768379Z`.
X64.24 is **not accepted**.

### Evidence (independent review)

- Exit code 1, no timeout, no dumps, `shutdown-exception.bin` empty, paths match,
  PID 32948 / thread 25868 on every record.
- `startup.log` contains **exactly 2048 records** (the in-process capacity).
  Records 1..479 are the normal init, engine_init complete, `game_loop begin`,
  first Present `S_OK` visible, and `game_start complete
  mode=2 map=maps\alpine assault\alpine assault.map players=1 frame=0` at record
  479. Records 480..2048 are **1569 consecutive `FP complete` rows** and nothing else:
  no `game_progress`, `game_summary`, boundary, residency, detach, cleanup or
  `WinMain` rows.
- The launcher errors are all consequences: missing `game_summary`/teardown rows,
  "Fewer than three logic progress observations", "Game did not start with at
  least two players" (`players=1`), wrong boundary label.

### Root causes (two, both bounded evidence defects)

1. **Log capacity exhaustion.** `setFPMode()` (GameLogic.cpp) emitted one `FP`
   receipt per call and it is called every logic step and every load step. After
   the game started the 2048-record fail-closed log filled in about 1.5k calls;
   `StartupDiagnostics::checkpoint` then sets `healthy=false`, silently dropping
   all later receipts, and WinMain forces `exitcode=1` for an unhealthy log. The
   engine and the 60-second bounded game almost certainly ran to
   `frame_bound` (developer observed ~60 s then normal termination), but this
   run cannot prove it: the tail evidence was not recorded. No GameLogic
   correctness conclusion is drawn from this run.
2. **`game_start` fired one update early.** `MSG_NEW_GAME` sets the game mode in
   `processCommandList` and `GameLogic::update` runs `startNewGame` at the top of the
   *next* update (`m_startNewGame`). My check saw `isInGame()` with the neutral
   player only (`players=1`, `frame=0`). The classifier's "at least two players"
   rule correctly rejected it. Not an AMD64 logic fault.

### Fixes (commit `18f0be5ab`)

- `StartupDiagnostics::RepeatGate`; `setFPMode` still resets and verifies the FP
  state on **every** call (and still `RELEASE_CRASH`es on failure) but writes only
  the first 32 receipts plus any state change; the total is reported once
  (`FP_summary calls=N`).
- `game_start` now requires `!isLoadingMap()` and `PlayerList >= 2`.
- Launcher classifier names capacity exhaustion (`Records >= 2000`).
- Regression: gate test (1,000,000 calls admit exactly 32), Python contract
  (throttle precedes nothing that skips verification, start waits for load),
  PowerShell flood test (2100 repeated receipts -> named error) and pre-start
  player-list test.

### Fresh candidate (preflight only, never executed)

`x64-24-ordinary-game-fix1`: GUID `c7323867-d82e-41f3-b190-20a5da4eae06`, age 70,
EXE SHA256 `2C62BD2A8795EBF783DCCA03543D013FA3FC17433CAC873611BCD4F083E7D0A9`,
PDB SHA256 `EA4217480A49B5B4425E37CE358854A97FB65504449F5005AD4469BDEBDF656F`,
30 imports, no app-local DLLs, no `runs` directory.

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeOrdinaryGame.ps1 -Title Generals -Name x64-24-ordinary-game-fix1
./scripts/native-runtime/Invoke-NativeOrdinaryGame.ps1 -Title Generals -Name x64-24-ordinary-game-fix1 -RunReviewedOrdinaryGame
```

Interaction, expectations and PASS/FAIL are unchanged from the section above
(substitute the stage name). Additional PASS evidence now present: an
`FP_summary` row and `game_start` with `players >= 2`.

## Renderer progress (independent of the X64.24 gate)

Fidelity decision (developer): equivalent modern shaders, tolerance-based golden
images against CPU references of the exact legacy stage math, developer visual
review; purpose-built D3D11 subsystem paths, never a generic fixed-function/D3D8
emulator. Nothing below changes a staged candidate (they are source-only and
unused by the profiles until the terrain consumer lands).

| Stage | State | Evidence |
| --- | --- | --- |
| R0 depth | Done: opt-in D24S8 target, depth/stencil clear, resize-safe | `WorldTest.*` on WARP |
| R1a neutral geometry | Done: `GeometryCPUContent`/`GeometryResidency`, XYZDUV2, in-place vertex update, `Submit_World` with `TerrainBase`/`TerrainBlend`, explicit sampler/depth intent, legacy `D3DCULL_CCW` | depth order, depth-write off, uv1+alpha blend, culling, residency counting, explicit unsupported cases |
| R1b legacy buffers | Done (AMD64 only, `_WIN64`): `DX8VertexBufferClass`/`DX8IndexBufferClass` become CPU-backed when the native D3D11 backend owns `WW3D`; locks/versions work through the unchanged lock classes; no COM buffer is created. Win32 and legacy-DX8 behavior untouched (`Is_CPU_Backed()` is false there). | `NativeBufferTest` on hardware (real classes -> backend geometry residency) |
| R2a terrain atlas | Done as a pure function: `BuildTerrainAtlas` replicates `TerrainTextureClass::update` (inverted rows, A1R5G5B5 quantization, 4-texel class borders incl. corners) and the legacy box-filtered mip chain; `Create_Mipped_Rgba8_Residency` uploads the chain | `TerrainAtlasTest`, `WorldTest.MippedAtlas...` |

### What the terrain pass actually has to do (from `HeightMapRenderObjClass::Render`)

`ST_TERRAIN_BASE` is two fixed-function passes over every vertex-buffer tile:
pass 0 `rgb = atlas(uv0).rgb * diffuse.rgb`, opaque, clamp; pass 1
`rgb = atlas(uv1).rgb * diffuse.rgb`, `a = atlas.a * diffuse.a`, SRCALPHA/INVSRCALPHA.
Lighting is baked into the vertex diffuse (prelit material). The cloud/noise
pass (`D3DBLEND_DESTCOLOR, ZERO` with camera-space texture generation) and the
shoreline/extra-blend/roads/bridges/props/scorch sub-passes are separate,
later increments. `TerrainBase`/`TerrainBlend` are exactly passes 0/1.

### Remaining R2 integration (each needs its own offline test; none is a decision)

1. Un-gate terrain allocation under the profile: `BaseHeightMap` returns before
   creating trees/props/bibs when headless and `HeightMapRenderObjClass::initHeightData`
   only allocates when `m_treeBuffer != nullptr`; the native profile must allocate
   the index/vertex tiles (now CPU-backed) without the DX8-only tree/prop/bib buffers.
2. Replace `WorldHeightMap::getTerrainTexture`/`AlphaTerrainTexture` (D3D8 surfaces
   + `LegacyD3DX::FilterTexture`) with the CPU atlas build + residency under the profile.
3. Camera: `W3DView::get3DCamera()` view/projection/viewport into `WorldSubmission`
   (D3D row-vector convention is already the contract).
4. Frame: depth-capable `PresentationLifecycle::Begin` for world frames and a
   terrain-only world pass ahead of the UI in `drawOrdinaryMenuFrame` (profile
   `ordinaryGame`), with unsupported scene objects remaining explicit.
5. Cloud/noise multiply pass, then shoreline/extra-blend tiles.
6. R3 meshes (DX8MeshRenderer sort/batch -> bounded material set), R4 water/shadows/
   particles, R5 full `RTS3DScene` camera/fog/shroud integration.

Steps 1-4 produce the first D3D11 terrain frame; they require a
developer-run candidate to be judged visually, and are sequenced after the X64.24
fix1 result confirms that logic runs on AMD64.

## fix2 candidate (supersedes fix1; same X64.24 contract)

The fix1 preflight was blocked ("Build changed; stage a fresh runtime") because the
independent renderer work rebuilt the shared outputs after fix1 was staged. A fresh
candidate was staged from clean HEAD `00fb1ce2c`. To keep the validation equal to
fix1 the CPU-backed legacy geometry (R1b), which keyed only on the D3D11 backend
owning WW3D (true in this profile), is now an explicit opt-in
(`D3D11Backend::Enable_CPU_Backed_Geometry`, default off; only tests and the future
terrain renderer enable it). The depth buffer is also opt-in and not enabled. The
profile's runtime behavior is therefore unchanged: same loop, push allow-list,
audio-off, bounded FP receipts and start-after-load evidence as `18f0be5ab`.

`x64-24-ordinary-game-fix2`: GUID `c7323867-d82e-41f3-b190-20a5da4eae06`, age 72,
EXE SHA256 `40A35EEFA001E98BF93B570D5B39A289C6BD4D2F40C1E065D9E7005C29BD5199`,
PDB SHA256 `1D07582141251A0EF671A03231244883C08F64C3EFCDFAE088604F6CF96B2BF9`,
30 imports, no app-local DLLs, no `runs` directory, preflighted, never executed.

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeOrdinaryGame.ps1 -Title Generals -Name x64-24-ordinary-game-fix2
./scripts/native-runtime/Invoke-NativeOrdinaryGame.ps1 -Title Generals -Name x64-24-ordinary-game-fix2 -RunReviewedOrdinaryGame
```

Interaction, expectations and PASS/FAIL are those of the original X64.24 section.
`fix1` must not be used (its stage no longer matches the build).

## Acceptance: X64.24 Skirmish -> map load -> live AMD64 GameLogic - FORMALLY ACCEPTED

Reviewed run (never rerun, immutable):
`build/dev-runtimes/native/Generals/x64-24-ordinary-game-fix2/runs/20261012T2200386937314Z`.
(The developer's cancelled second Invoke attempt created no run directory:
`runs/` contains exactly this one directory.)

Independent machine review:

- OS exit 0, no timeout/error, PID 3444 / thread 32608 on all 233 contiguous
  records (far below the 2048 capacity), zero `fail` rows, no
  `native_shutdown exception`, `shutdown-exception.bin` empty, no dumps, isolated
  paths match. Staged EXE/PDB re-hashed equal to the receipt (`40A35EEF...D5199` /
  `1D075821...B2BF9`, age 72); strict argv `-nativeOrdinaryGame`.
- Original menu and Skirmish setup ran (no `menu_boundary`, no `shell_push`).
- `game_start mode=2 map=maps\alpine assault\alpine assault.map players=5 frame=1`
  (after load, as fixed).
- `game_progress` at logic frames 2/31/151/301/901/1801 with cached CRCs
  `DF77AB89, DF77AB89, 7B3962C4, A23F32E6, 299FA3CE, CEFCD66F`; 4193 presents,
  4114 engine updates, 69.7 s, 1800 logic frames, `exit=frame_bound`.
- `FP_summary calls=14500`: every call verified, receipts bounded (32 + changes).
- Ordinary `exitGame` processed (`game_exit`), then `ui_residency_release
  live_residencies=0` before backend release, backend before engine cleanup,
  window released after engine cleanup, MiniDumper shutdown, final `WinMain complete`.
- Developer visual: original menu/setup, match started, black world (expected).

Proves: AMD64 GameLogic, AI, scripts, map/terrain-logic loading, object creation
and a 1800-frame skirmish run inside the real application with the original UI on
the D3D11 presentation, with orderly game exit and teardown. No crash, no hang,
no FP-state change observed over 14,500 verified calls.

Does NOT prove: any world rendering, cross-architecture determinism (the CRCs
are recorded but not compared with Win32; two early milestones are equal by the
cached-CRC cadence), performance, save/load, replay, multiplayer, interactive
in-game play.
