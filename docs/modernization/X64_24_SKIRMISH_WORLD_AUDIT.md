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
