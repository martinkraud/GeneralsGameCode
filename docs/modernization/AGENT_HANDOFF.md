# GeneralsGameCode: current cross-agent handoff

Updated 2026-10-14 after the formal X64.27 acceptance and the X64.28 First Playable batch (the
"Current primary objective" section below carries the running log; plan and dependency graph:
`FIRST_PLAYABLE_ROADMAP.md`). This file is the
current project summary, not a chronological transcript. Historical reports may
contain superseded candidate instructions; use their latest acceptance sections.
Developer task direction overrides any inferred roadmap task.

## Repository and Git

- Workspace: `C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode`.
- Branch: `dev/modern-engine`.
- Implementation HEAD at handoff preparation:
  `f6e8c1aeec7f1cda20b3dce58348a8c4366c181c`, synchronized with origin.
- This handoff/acceptance is a documentation-only commit on that baseline. Its own
  hash cannot be embedded in itself; obtain final current HEAD with `git rev-parse HEAD`
  and `git log -1`. The developer-facing handoff reports the final hash.
- Worktree was clean before this task; only four modernization documents change.
  Intended final state is clean after the documentation commit/push. Always verify.
- origin: `https://github.com/martinkraud/GeneralsGameCode.git`, developer fork.
- upstream: `https://github.com/TheSuperHackers/GeneralsGameCode.git`, reference;
  its push URL is `DISABLED`. Never push upstream.
- Authorized destination only: `git push origin HEAD:dev/modern-engine`.
  No force push, rebase/history rewrite, blind `git add .` or generated artifacts.
  Explicitly stage intended paths and inspect the staged diff.

Recent relevant commits:

| Commit | Meaning |
| --- | --- |
| f6e8c1aee | AMD64 WindowMsgData pointer-width repair; exact Win32 type retained |
| 7c3aaeb8c | Original MainMenu archive path correction |
| 02f8cb00c | Static original MainMenu diagnostic composition gate |
| 2caf42983 | Disabled texturing honors shader state despite retained texture |
| 34fdd323f | Original UI sentence/font producer gate |
| ac38a1c5b | CPU texture-content pool registration and UI-load diagnostics |
| 53f08fbdc | AMD64 ObjectPool block-header/payload layout repair |
| 09b2983af | Final external D3DX resource helpers replaced |

## Latest accepted milestone: Generals X64.21

**FORMALLY ACCEPTED** from direct machine evidence and developer visual confirmation.
Exact reviewed run:
`build/dev-runtimes/native/Generals/x64-21-fix2/runs/20261012T1852064397888Z`.

- OS exit0, no timeout/launcher error, isolated paths match, PID24536/thread3980.
- 346 contiguous same-process checkpoints; independent strict UIMenu classifier PASS.
- Hardware Intel(R) Graphics vendor8086/device7D45, feature0xB000 (11.0),800x600.
- Original `Window\Menus\MainMenu.wnd` parsed63 windows; selected11, six buttons,
  five mapped images/two original atlases. Original fonts/localized labels resolved.
- Original ruler, panel/inner panel, logo and six normal-state textured/labeled
  buttons render via real manager/widget/DisplayString/Render2D producers.
- 235 visible, non-minimized, unoccluded hardware frames in5015ms; first Present S_OK.
- W3DDisplay -> WW3D -> PresentationLifecycle Begin/End/Present route completes.
- UI windows/DisplayStrings/private images/atlases released before backend/device,
  then HWND/class release; engine cleanup, MiniDumper shutdown, late observer armed,
  final WinMain completion. shutdown-exception.bin is empty, no trailer, no dumps.
- Ordinary execute/update/replay/simulation/GameLogic updates remained skipped by the bounded mode.
- Developer confirms full expected static composition, correct artwork/layout,
  all six textured buttons/readable labels, no obvious corruption, normal exit.

Actual staged EXE/PDB independently verified against launch/build identity:
GUID `c7323867-d82e-41f3-b190-20a5da4eae06`, age63.
EXE SHA256 `C9C508406C0A4B032139E2D9E7778B4C976B76F3EFF4AE35BB4D07CAAFCA2B74`.
PDB SHA256 `34843207311994D3865E5DCFABDC9C248299E2A29612D87D4FC2DDBBA3B0C3A6`.

This proves a controlled native AMD64 D3D11 **static original main-dropdown
composition**, using retained game-facing owners after headless initialization.
It does NOT prove ordinary graphical startup, interactive input/menu callbacks,
hover/pressed/disabled states, all screens/transitions, world/shell-map rendering,
ordinary engine updates, gameplay, save/load, replay, multiplayer, performance or
x86/x64 determinism. Zero Hour native headless and basic graphical smoke milestones
are accepted historically; Generals UI acceptance does not establish Zero Hour UI parity.
Both titles link AMD64. External D3DX helper count is zero; this does not remove DX8.

## Architecture and compatibility invariants

- Retain **W3DDisplay/WW3D** as shared game-facing owners. Do not introduce a new
  ModernDisplay frontend without a separately justified material decision.
- Reuse renderer-neutral Render2DSubmission, Texture2D CPU content/residency/sampler
  intent and PresentationLifecycle contracts. Shared modern architecture serves
  both titles; title-specific startup/UI semantics still require separate review.
- D3D11Backend owns real device/context/swap-chain/backbuffer/RTV and embedded modern
  shaders/states. WW3D owns the presentation lifecycle and borrows its active backend.
  This is an implemented UI/presentation slice, not a complete game renderer.
- Retain Win32/DX8 as the reference path. Preserve pinned math, resource conversion,
  filtering/compression and rendering contracts/tests. No generic resampler/DXT
  substitution for characterized legacy behavior; read X64.9/10 only when relevant.
- Preserve original assets, UVs,800x600 layout and UI design. Original Generals font
  alias uses Arial with the legacy narrow GDI width policy; do not replace arbitrarily.
- Unsupported renderer operations/states fail explicitly. No silent no-ops,
  fake IDirect3DDevice8, generic D3D8 emulation, COM casts between APIs or replacement DLLs.
- Diagnostic UI clones five mapped images and owns two attachment-scoped texture
  atlases; it does not validate ordinary global asset/texture-cache ownership.
- Destroy UI/residency before backend/device and destroy backend before HWND.
  Preserve normal engine/MiniDumper/late observer/process teardown and mandatory exit0.

## Significant AMD64 findings to retain

1. ObjectPoolClass block header is pointer-width. Old uint32* +1 started objects
   at byte4 on AMD64 and corrupted the upper half of an8-byte link. X64.14 fixes
   payload offset/alignment while preserving Win32 byte4 layout, capacity/reuse/order.
   Dual-architecture tests and both reviewed native init-only passes support it.
2. WindowMsgData carries pointers, not just integers. f6e8c1aee uses uintptr_t on
   _WIN64 and retains exact UnsignedInt on Win32. Fix1 dump proved label pointer
   0x842879d758 truncated to0x2879d758 in GadgetButtonSetText -> GadgetPushButtonSystem
   -> UnicodeString copy. Do not narrow UI payloads or revert the callback ABI.
   The dual-architecture real producer/dispatch/copy regression is UIProducerTest.
3. MainMenu widget IDs use `MainMenu.wnd:` but file loading needs
   `Window\Menus\MainMenu.wnd`. A bare name prefixes only Window\ and failed the
   first X64.21 candidate. Do not confuse widget names and archive keys.
4. Texture2DCPUContent pools must be registered before native texture preparation
   (X64.18). Retained texture ownership does not imply texturing is enabled;
   D3D11 must honor the shader texturing bit (X64.20).
5. AMD64 FP startup explicitly establishes round-to-nearest/masked exceptions and
   MXCSR0x1f80 (FTZ/DAZ off); Win32 x87 PC24 policy is preserved. This does not prove
   simulation equivalence. Accepted FP/arithmetic/resource tests remain mandatory.
6. Native link/import success is narrower than runtime readiness. Ordinary DX8
   startup dynamically loads D3D8; absence from PE imports is not evidence it is safe.
   Optional media/audio/browser/network paths are not all accepted as native runtime.
7. Further pointer-to-Int/UnsignedInt casts remain in legacy source; warnings alone
   are not proof of a reachable defect. Audit the actual next path and establish
   root cause before changing it. Do not broaden this into generic x64 cleanup.

## Current primary objective and next task

**X64.31 (2026-10-14) — ORIGINAL GAME EXPERIENCE / NORMAL LAUNCH: offline batch complete, candidate staged,
awaiting human run.** Authoritative report: `NORMAL_LAUNCH.md`. X64.30 is accepted (evidence preserved under
`evidence/x64-30-world-ui-completeness/`). Summary: `generalsv.exe` with no diagnostic token is a normal launch
(`StartupDiagnostics::normalLaunch()`): ordinary host/menu/game/terrain/play predicates all true, original
`GameEngine::execute()` loop, lightweight `generals64.log`; original audio via `Dependencies/Miles/NativeMiles`
(software mixer + waveOut + Media Foundation MP3 + IMA ADPCM) bound behind `MilesLoader`; load/score screens,
return-to-shell and movie entry points no longer gated by headless; edge scrolling default includes windowed on
AMD64; video (Bink) is genuinely unavailable on AMD64 and is not faked. Launch with
`scripts/native-runtime/Invoke-NativeNormalLaunch.ps1 -Title Generals -Name <stage> -RunNormalLaunch`.
Do not run it automatically; frontend/audio/Skirmish/return-to-frontend review is the developer's.
**fix2 native review (2026-10-14): normal launch, full Skirmish win, second Skirmish in-process, USA campaign intro/mission all worked.** fix3 (`NORMAL_LAUNCH.md` section 11): root cause of long-session audio loss was 64 bit completion handles truncated to 32 bits in `MilesAudioManager` (`UnsignedPtr` now); NativeMiles max-distance/callback-thread/lock fixes and `audio_stats` diagnostics; Starting Cash extension for Generals Skirmish (Zero Hour choices, retail default, `startingCashExtensionEnabled()` gate). Selection indicator still open (hypothesis corrected: effect textures are already mipped).

**fix3 native review: audio lifecycle fixed (audio_stats: starved=0, completions==delivered), Starting Cash works.** fix4 (`NORMAL_LAUNCH.md` section 12): mirrored stereo (listener right = up x forward), Starting Cash layout repaired (parent/coordinate-space bug), Daisy Cutter delivery instrumented (`special_power`/`delivery` checkpoints; root cause not provable offline - read `generals64.log` first).

**fix4 native review: stereo fixed; Daisy Cutter root cause = special-power shortcut source picked an in-flight cargo plane (`KINDOF_IGNORED_IN_GUI`) instead of the Command Center; fix5 filters it (`NORMAL_LAUNCH.md` section 13). Starting Cash remnants hidden geometrically, layout aligned to the setup-grid header rule.**

The sections below describe the earlier reviewed-profile history.

**NORMAL GENERALS AMD64/D3D11 STARTUP -> ORIGINAL INTERACTIVE MAIN MENU.**

Static composition is now demonstrated. Do not indefinitely expand synthetic
-nativeUIMenuSmoke composition, immediately switch backend factories, rewrite UI,
flip headless midway, or enable ordinary execute just to show more graphics.

**Ordinary-startup audit COMPLETE (documentation only, developer review pending):**
`X64_22_ORDINARY_STARTUP_AUDIT.md` holds the source-backed A/B/C inventory. Key
current truths: with ShellMap on, MainMenu is pushed by GameLogic after the shell
map loads (the world path); only `shellMapOn=FALSE` plus no intro pushes it from
the client; `GameEngine::update` cannot be the menu loop because
`GameLogic::update` is the door to `MSG_NEW_GAME`; `m_headless` is one flag for
~30 behaviors, so a separate UI-without-world profile is the main architecture
decision; ordinary `W3DDisplay::draw` throws on the presentation lifecycle (depth
clear, world render) and D3D11 Render2D rejects grayscale/additive. Recommended
first slice: an ordinary-lifetime presentation host (strict profile, checked
ApplicationHWnd, profile-selected native-presentation `W3DDisplay::init`, bounded
UI-only frames). The class table below is the original question list; the audit
answers it.

**Status (2026-10-12): X64.22 ordinary-lifetime presentation host FORMALLY
ACCEPTED** from run `build/dev-runtimes/native/Generals/x64-22-ordinary-host/runs/20261012T2015215339111Z`
(exit 0, 292 checkpoints, zero fail rows, hardware Intel 8086/7D45 feature 0xB000,
239 visible S_OK frames, `live_residencies=0` before backend, HWND destroyed last,
no dumps; EXE `A0BDFA5C...3E66`, age 65; developer visual PASS). Implementation
commit `66d23d7d1`; profile `-nativeOrdinaryHost` (Generals AMD64 only) with the
A9 UI-without-world selection (`m_headless` stays TRUE; only the application
window and `W3DDisplay` lifetime are real). Details and limits: "Acceptance"
section of `X64_22_ORDINARY_STARTUP_AUDIT.md`. Next: the larger offline batch
real WindowManager -> Shell -> original MainMenu -> transitions -> cursor/input
-> bounded menu-only event loop (see the X64.23 section there). Class C stays
closed; do not expand synthetic `-nativeUIMenuSmoke`.

**X64.23 (2026-10-12): ordinary interactive MainMenu FORMALLY ACCEPTED** from run
`build/dev-runtimes/native/Generals/x64-23-ordinary-menu/runs/20261012T2044447975464Z`
(exit 0, 295 checkpoints, zero fail rows, 555 visible S_OK frames, 279 moves,
2 clicks, Skirmish click stopped by the push guard at
`Menus/SkirmishGameOptionsMenu.wnd`, `live_residencies=0`, no dumps; EXE
`B8C0F45A...112DF`, age 67; developer visual PASS). Implementation `eacf37eae`; doc
`X64_23_ORDINARY_MENU.md`. Profile `-nativeOrdinaryMenu` (Generals AMD64): real
window manager/transitions, `W3DMouse` Windows cursors, DirectInput keyboard,
original MainMenu through the ordinary Shell with a push guard, input via
`WindowTranslator` only, bounded menu-only loop, D3D11 Render2D additive/opaque/
flat/grayscale. Next: Skirmish setup -> map load -> first world frame; see
`X64_24_SKIRMISH_WORLD_AUDIT.md`. Class C (GameLogic/world) is opened only through
the bounded first-world-frame boundary defined there.

**X64.24 (2026-10-12): first run FAILED validation; NOT accepted.** Run
`build/dev-runtimes/native/Generals/x64-24-ordinary-game/runs/20261012T2112467768379Z`
(immutable): the 2048-record log filled with unthrottled `FP` receipts (1569 rows
after `game_start`), so tail evidence was lost and exit was forced to 1; plus
`game_start` fired one update before `startNewGame` (`players=1`). Developer saw the
menu, Skirmish setup, match start and ~60 s of run. Both are evidence defects, no
GameLogic fault is concluded. Fixed in `18f0be5ab` (bounded receipts, start after
load, capacity named in the classifier). Fresh candidate
`x64-24-ordinary-game-fix1` (EXE SHA256 `2C62BD2A...D0A9`, PDB `EA421748...656F`,
age 70) preflighted, never executed. Doc: `X64_24_SKIRMISH_WORLD_AUDIT.md`.

**X64.24 FORMALLY ACCEPTED (2026-10-13)** from fix2 run
`build/dev-runtimes/native/Generals/x64-24-ordinary-game-fix2/runs/20261012T2200386937314Z`
(exit 0, 233 records, zero fail rows, original menu -> Skirmish -> map load ->
1800 live AMD64 logic frames with CRC milestones, `FP_summary calls=14500`, ordinary
`exitGame`, `live_residencies=0`, no dumps; EXE `40A35EEF...D5199`, age 72; developer
visual PASS). The first run's failure was an evidence-capacity defect (see doc).
Proven: AMD64 GameLogic/AI/scripts/map load inside the real application. Not proven:
world rendering, cross-arch determinism, performance.

**X64.25 first native D3D11 terrain frame FORMALLY ACCEPTED (2026-10-13).** Machine PASS and developer visual PASS recorded separately from fix2 run `build/dev-runtimes/native/Generals/x64-25-ordinary-terrain-fix2/runs/20261013T0503179685543Z` (exit 0, 237 records, 1800 logic frames, 16 terrain tiles, 3600 terrain frames, pixel probe nonblack 80%, `live_residencies=0`; EXE `F373137C...6696C9`). Terrain visible and correct. History: run 1 failed on a partial-mip-chain check; fix1 machine PASS but visually black (world rasterizer culled the legacy front face: legacy default is D3DCULL_CW); fix2 corrected it and added the probe. Bottom black strip = outside the 0.80 ViewportHeightScale world viewport (original Control Bar contract), not a terrain defect. Doc: `X64_25_TERRAIN_FRAME.md`. Not yet rendered: clouds/noise, dynamic lights, shoreline/extra-blend, roads/bridges/props, shroud, meshes, water, shadows, particles.

**X64.26 terrain fidelity + dynamic terrain exercise FORMALLY ACCEPTED (2026-10-13).** Machine PASS and developer visual PASS recorded separately from run `build/dev-runtimes/native/Generals/x64-26-terrain-fidelity/runs/20261013T0533511829002Z` (exit 0, 255 records, 1800 logic frames, 3600 terrain frames with retail cloud+noise on every frame, six scripted camera moves, `tile_updates` 16->84 strictly increasing, `live_residencies=0`; EXE `09EB69BC...378D57`). Retail `terrain*.pso` decoded; single-pass combiner and world-XY texgen verified. Control Bar review: bar is parked at y=1216 because `ControlBar::update()` returns under headless so the slide-in animation never runs (radar is RadarDummy); bounded follow-up documented. Doc: `X64_26_TERRAIN_FIDELITY.md`. Next: shoreline/extra-blend, roads/bridges, R3 meshes; small Control Bar animation fix as a separate UI step.

**X64.27 terrain completion + map infrastructure (2026-10-13): candidate staged, NOT executed.** Control Bar slide-in via a profile-only `ControlBar::updateNativePresentation()` (presentation subset of `update()`); native TerrainOverlay family (extra-blend 3-way tiles, roads, bridges, destination-alpha shoreline) decoded from `roadnoise2.pso`/`detailAlphaShader`; dynamic geometry + dds/TGA world texture decode; classifier requires each overlay family exactly when the loaded map reports it, plus Control Bar visibility. Acceptance map: `Lone Eagle` (all four families). Doc: `X64_27_TERRAIN_COMPLETION.md`. Out of scope: meshes, water, shadows, particles, radar, extended camera (later Modern Experience phase).

**X64.27 FORMALLY ACCEPTED (2026-10-14)** from fix2 run
`build/dev-runtimes/native/Generals/x64-27-terrain-completion-fix2/runs/20261013T0638444013011Z` (exit 0, 263
records, 1800 logic frames, 3600 terrain frames, 4 overlay families counted, `live_residencies=0`; developer
visual PASS: terrain, roads, moving cloud shadows, original Control Bar). The run used Alpine Assault, so
bridges and shoreline were not exercised natively (WARP contracts only). Doc: `X64_27_TERRAIN_COMPLETION.md`.

**NEW PRIMARY OBJECTIVE (2026-10-14): FIRST PLAYABLE GENERALS** with larger offline batches and one staged
candidate per batch (target 2-4 native checkpoints). Plan, dependency graph, fidelity backlog and the
record-only Developer Mode note: `FIRST_PLAYABLE_ROADMAP.md`.

**X64.28 (CP1) world objects + interactive world + play profile - offline batch complete, candidate staged,
NOT executed.** Profile `-nativeOrdinaryPlay` (implies terrain). Native object renderer
(`Core/GameEngineDevice/.../NativeWorldObjects.*`: registry, CPU skin/lighting, shader-state mapping, team
color, animation, trees/props), native picking, in-game UI/overlay draw, full Control Bar update, user-driven
loop with message-tap receipts. Offline evidence: real models rendered through WARP, every W3D.big prototype
rendered fault-free, classifier/contracts. Doc: `FIRST_PLAYABLE_ROADMAP.md` section 4 and
`X64_28_PLAY_WORLD.md`. Lessons: bash heredocs in this environment halve backslashes (use the Write/Edit
tools for C++/Python containing `\`); a global text replace once made a helper call itself - the static
contract test caught it.

**X64.28 native result (runs `20261013T1151501824715Z` and `20261013T1155367083324Z`, candidate
`x64-28-play-world`): real interactive gameplay was reached, the acceptance wrapper correctly reported
FAILED.** Two independent defects (both preserved, evidence immutable): (1) Exit Game -> teardown access
violation: the play tap was a static object handed to `attachTranslator`; `MessageStream` owns and deletes
translators, so `removeTranslator` freed a non-heap pointer (`DynamicMemoryAllocator::freeBytes`); (2)
radius-cursor (reconnaissance drone special power) access violation: `RadiusDecalTemplate::createRadiusDecal`
dereferenced the null `TheProjectedShadowManager` of the scene-less profile. A third, classifier-visible
defect: `world_objects_first_frame` was logged one record before `terrain_first_frame`. Fixes, regression
tests and the `x64-28-play-world-fix1` candidate: `X64_28_PLAY_WORLD.md` sections 9-11. Next: developer runs
fix1 once; do not rerun `x64-28-play-world`.

**X64.28 fix1 native result (run `20261013T1216116100741Z`, candidate `x64-28-play-world-fix1`):** both
earlier crashes are gone. Process exit code 0, no dump, complete teardown receipts (`game_exit` ..
`WinMain`); the wrapper still said FAILED with exactly one classifier error, "Fewer than 150 logic frames in
game", because the host printed `logic_frames=0`: the loop iteration that observes the player leaving the match
ran after clear-game-data reset the frame counter and overwrote `lastFrame`. A host measurement defect, not a
crash and not the PlayTap bug (the classifier contract was correct and is unchanged; the real log classifies
Valid once `logic_frames` carries the last in-game frame). Fix + candidate `x64-28-play-world-fix2`:
`X64_28_PLAY_WORLD.md` sections 12-14. Natively proven in this run: AI enemy units attacked the base (developer
observation), 13 move + 1 attack orders, 28 queued units, 5 constructions, 36 selections, 6291 logic frames.
Do not claim AI base construction: neither receipts nor the replay show it.

**X64.28 / FIRST PLAYABLE - ACCEPTED (2026-10-13).** Candidate `x64-28-play-world-fix2`, native run
`20261013T1227301326226Z`: wrapper PASS, exit 0, 7248 logic frames, AI active and constructing (developer),
combat/orders/production/Control Bar/special powers/helicopters exercised, clean Exit Game. This candidate and
run are the **First Playable reference baseline** (immutable; evidence copy in
`evidence/x64-28-first-playable/`; closeout `X64_28_PLAY_WORLD.md` section 15). Measured remaining fallback:
`unsupported_state=65589` over the run. **Current phase: Gameplay + Rendering Completeness
(`GAMEPLAY_RENDERING_COMPLETENESS.md`)** - large offline batches, one native candidate per batch, First
Playable acceptance contracts and classifier unchanged.

**X64.29 (CP2) Gameplay + Rendering Completeness - offline batch complete, candidate
`x64-29-gameplay-render-completeness` staged and preflighted, NOT executed.** Details, dependency graph,
validation and the review checklist: `GAMEPLAY_RENDERING_COMPLETENESS.md`. Implemented: multi-pass + texture
mappers (retail sweep `unsupported_state` 7,648 -> 0), line objects, real particle manager + native particles,
decal/planar-shadow manager, scorches, water, shroud overlay, ghost objects, layers/stencil-once/dest-alpha in the
backend. Deferred: radar minimap, terrain tracks, smudge, snow, tree sway, bibs/waypoints, water sparkle/bump,
soft projection shadows, dual-stage materials. Crash isolation: `GENERALS_NATIVE_DISABLE` (comma list of
particles, ghosts, shadows, water, shroud, scorches, lines, mappers). The First Playable baseline
(`x64-28-play-world-fix2`) stays the regression reference; do not rerun the earlier candidates.
**X64.29 review stabilization:** root causes and fixes in `GAMEPLAY_RENDERING_COMPLETENESS.md` section 9
(particle layer never submitted; overlay tessellation vs terrain; multiply shadows; shroud pass for ghosts;
review window scale). Candidate `x64-29-gameplay-render-completeness-fix1` staged and preflighted, not executed.
Env: `GENERALS_NATIVE_REVIEW_SCALE` (percent or factor, default 150), `GENERALS_NATIVE_DISABLE`.

**Renderer (fidelity decision from the developer: equivalent modern shaders,
tolerance-based golden images vs CPU references of the legacy stage math, developer
visual review; purpose-built D3D11 subsystem paths, no generic emulator).** R0/R1a
done offline in `18f0be5ab`: opt-in D24S8 depth + depth clear in `D3D11Backend`,
neutral `GeometryCPUContent`/`GeometryResidency` (XYZDUV2) with in-place update,
`Submit_World` with the purpose-built terrain base/blend combiner (`WorldSubmission.h`),
WARP-tested. Next: R1b backend-neutral CPU-backed vertex/index buffers behind
`DX8VertexBufferClass`/`DX8IndexBufferClass`, mutable multi-mip texture content for
the terrain atlas (`TerrainTextureClass` currently locks a D3D8 A1R5G5B5 surface and
`LegacyD3DX::FilterTexture`), then the HeightMap consumer (R2).

| Class | Known source facts and audit questions |
| --- | --- |
| A: required for interactive menu | Real ApplicationHWnd creation/ownership, checked creation/focus/message handling, truthful modern device/configuration/size lifecycle, real WindowManager/layout callbacks, menu event routing, cursor/keyboard/IME, resource ownership and graceful partial-init/exit. Determine exact minimum from reachable source; not every legacy feature is automatically required. |
| B: safely deferrable candidates | Other screens, fullscreen/MSAA/resize/device recovery variants, optional browser/media/online features and later UI states may be deferrable for a bounded first gate. Audit reachability and fail explicitly; do not assume they are optional or silently stub them. |
| C: world/gameplay/simulation remains gated | Shell-map/world drawing, GameLogic, ordinary engine execute/update, replay, save/load and multiplayer. Audit shared menu dependencies and RNG/network side effects without activating them prematurely. |

### Actual known gap: sources to inspect first

- `Generals/Code/Main/WinMain.cpp`: non-headless initializeAppWindows uses the
  application WndProc/ApplicationHWnd; diagnostic HWND and message pump are separate.
  WndProc has focus/audio/input/quit/splash/device-resize interactions. Their safe
  modern equivalents and partial-init behavior require audit, not assumptions.
- `Generals/Code/GameEngine/Source/Common/GameMain.cpp`: normal path enters replay
  or GameEngine::execute; diagnostic branches run after init and before those paths.
- `Generals/Code/GameEngine/Source/GameClient/GameClient.cpp`: non-headless creates
  real keyboard/mouse/window manager. Headless chooses dummy mouse/window manager;
  fonts, mapped images, display and Shell metadata can still initialize.
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp`: non-headless
  init calls WW3D::Init(ApplicationHWnd), Set_Render_Device, scenes/shader/browser;
  normal draw contains DX8 cooperative-level/world/depth-clear dependencies.
- `Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Backend.cpp`: ordinary
  Create_Render_Backend still selects DX8Backend. `ww3d.cpp` Set_Render_Device
  still delegates to DX8Wrapper; ordinary Begin/End include legacy loader/buffer/
  sorting work. D3D11 has no equivalent complete world/depth pipeline here.
  Modern presentation adapters exist; merely changing the factory is unsound.
- `W3DDisplayPresentation.cpp` and `NativeMenuComposition.h` in the shared W3DDevice
  GameClient directories: current accepted diagnostic retains headless selection,
  installs a private real W3D manager temporarily, nulls/restores transition owner,
  suppresses application input/system/layout callbacks and selects11 of63 windows.
  Therefore success does not validate real manager init/update/transition ownership.
- `Core/GameEngine/Source/GameClient/GUI/Shell/Shell.cpp`, `WindowLayout.cpp`,
  `GameWindowManager.cpp` and `GameWindowManagerScript.cpp`: trace original Shell
  push/layout init/shutdown, repaint/transitions and real callback dispatch.
- `Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/MainMenu.cpp`:
  MainMenuInit shows shell-map/mouse, initializes menu state and touches GameSpy;
  nearby routes touch RNG/transitions. Shared W3DDevice/GameClient/GUI/GUICallbacks/
  W3DMainMenu.cpp::W3DMainMenuInit currently delegates to MainMenuInit (its older
  custom draw setup is commented out). Determine which
  effects are required, which can be intentionally gated, and which enter class C.
- `Core/GameEngineDevice/Source/Win32Device/Common/Win32GameEngine.cpp`: update calls
  GameEngine::update before serviceWindowsOS. Do not use execute/update as a harmless
  message pump. Audit a real menu-only event/update boundary rather than bypassing it.

Known source facts above are not a completed normal-startup feasibility audit.
Focus/input, transition timing, optional subsystem reachability, additional assets/
render states and ordinary cache/device lifetimes remain to be classified/tested.

## Runtime and evidence rules

No native title execution by the coding agent. Every new Generals/Zero Hour run
requires separate developer review and visual confirmation where applicable.
No automatic reruns. On failure preserve the exact run, inspect machine evidence
and matching symbols first; fix only concrete bounded causes offline. Stop for
uncertain root cause, material architecture choice or destructive action.

Never modify the installed Steam game, replace graphics DLLs, reuse/overwrite an
old candidate, or mutate accepted/failed evidence. Stage fresh uniquely named
isolated runtimes only after final validation; inventory original assets and
actual AMD64 EXE/PDB SHA256/GUID/age, graphics dependencies/imports and paths.
Use existing scripts/native-runtime Stage/Test/Invoke guard contracts; Test does
not launch, Invoke does. Do not invoke a game-launch script yourself. Isolate
userdata/dumps per run. Hardware means explicit hardware, no silent WARP fallback.
After staging, do not rebuild/restage that candidate; a changed build gets a new name.

Accepted/failed evidence is under ignored build/dev-runtimes/native/**/runs.
Do not stage dumps/ETLs/captures/runtime copies/build logs/analysis helpers/JSON.
Latest failed runs: x64-21-final/20261012T1822476915817Z (wrong file path,exit1)
and x64-21-fix1/20261012T1838100556946Z (payload AV,two dumps). Preserve both.
This handoff reverified60 recorded evidence hashes and32 prior staged EXE/PDB hashes,
plus latest candidate identity. Local build/x64-*-*preserved.json are useful ignored
inventories, not portable tracked test dependencies. Runtime evidence is local;
another host without it cannot independently claim to have reviewed it.

## Validation and shortest reading order

Read this handoff, then:
1. `X64_21_STATIC_MENU_COMPOSITION.md` latest acceptance/failure sections and
   current source listed above; `Tests/Google/Core/Libraries/UIProducerTest.cpp`.
2. `X64_17_DISPLAY_PRESENTATION_LIFECYCLE.md` for retained-owner contracts,
   `X64_15_GRAPHICAL_ROUTE_FEASIBILITY.md` for original startup dependencies;
   reconcile historical line numbers/claims with current source.
3. `RENDERER.md`, `X64.md`, `ROADMAP.md` current sections. Consult earlier resource,
   FP/pool/startup reports only when a reachable dependency requires their contract.

Last full source validation was run for f6e8c1aee before staging fix2:
Win32 Release/CTest25,direct156/focused82 each title (six pre-existing disabled);
AMD64 native links/independent CTest15/texture-UI17 each title; sentence5/render2d15;
Python rendering30/native17/performance16; PowerShell graphics81/native6/10script parses.
AMD64 imports30/title,no delay/D3D8/D3DX imports;31 graphics dependencies/title PASS.
This documentation-only handoff did not rebuild or rerun those suites. It independently
reclassified the complete run, verified hashes/symbol identity/dumps/ordering and
runs git diff --check. Do not conflate inherited validation with a new run.

Build from repo root in the matching VS2026 Developer shell (existing configured
build/win32 and build/win64 caches; CMake/Ninja, Release). VsDevCmd location:
`C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat`;
use -arch=x86 or -arch=x64, -host_arch=x64. Do not rely on ignored helper CMD files.

```powershell
# Win32 developer shell
cmake --build build/win32 --config Release --parallel 4
ctest --test-dir build/win32 -C Release --output-on-failure
./build/win32/Tests/Google/Release/g_googletest.exe
./build/win32/Tests/Google/Release/z_googletest.exe
# AMD64 developer shell: established bounded native targets
cmake --build --preset win64 --parallel 4 --target g_generals z_generals g_googletest z_googletest native_graphics_smoke_test object_pool_test native_startup_fp_test native_startup_gate_test legacy_resource_comparison legacy_error_string_test legacy_fixed_shader_test legacy_texture_creation_test legacy_fvf_stride_test legacy_d3dx_math_test d3d11_ui_test d3d11_sentence_test sentence_content_test render2d_submission_test windows_diagnostics_test
ctest --test-dir build/win64 -C Release -E '^[zg]_googletest$' --output-on-failure
./build/win64/Tests/Google/Release/g_googletest.exe '--gtest_filter=Texture2DTest.*:UIProducerTest.*'
./build/win64/Tests/Google/Release/z_googletest.exe '--gtest_filter=Texture2DTest.*:UIProducerTest.*'
# PowerShell7, offline workflow/reference checks
./scripts/native-runtime/Test-NativeGraphicsWorkflow.ps1
./scripts/native-runtime/Test-NativeWorkflow.ps1
python -B -m unittest discover -s scripts/rendering
python -B -m unittest discover -s scripts/native-runtime
python -B -m unittest discover -s scripts/performance
git diff --check
```

Python locally used: `C:/Users/buskr/openvino-npu/ovms/python/python.exe`.
These commands execute tests/tools, not game processes. Never execute built title
binaries. Rebuild only when source work warrants it; no rebuild is needed to read
this handoff or audit sources. For exact additional focused regression filters,
see the X64.21 validation report and prior implementation validation receipts.

## Handoff protocol

Before modifying anything, inspect git status/diff/log/remotes; reconcile this
handoff against actual repository/tests/evidence. Treat those as source of truth,
not a prior chat or copied summary. Preserve accepted numerical/behavioral contracts;
do not re-derive closed gates without contradictory evidence. Preserve architecture
unless a source-backed need justifies a reviewed change. Continue authorized safe
offline work, but stop before native execution/visual validation or unresolved
material choices. Update this handoff before transferring to another agent.

This task stops after documentation. Do not begin X64.22 or normal-startup
implementation as part of this handoff. The next task is the ordinary-startup
blocker audit described above, under the next developer instruction.

**X64.29 fix2:** bridges now follow local shroud (per-vertex shroud multiplier at upload); Zero Hour shared/specific audit and parallel-candidate recommendation in `GAMEPLAY_RENDERING_COMPLETENESS.md` sections 11-12.

**X64.29 CLOSED:** `x64-29-gameplay-render-completeness-fix2` accepted by developer visual review (2026-10-13); evidence preserved in `docs/modernization/evidence/x64-29-gameplay-render-completeness-fix2/`. Zero Hour X64.30 parameterization is deliberately postponed (audit kept in `GAMEPLAY_RENDERING_COMPLETENESS.md` sections 11-12). Next batch: World/UI Completeness (`WORLD_UI_COMPLETENESS.md`).

**X64.30 World/UI Completeness (offline batch complete, candidate `x64-30-world-ui-completeness` staged, NOT executed):** radar/minimap through the original `W3DRadar` on a shared CPU-backed dynamic UI texture residency (D3D11 backend, revision-driven upload), terrain tracks, building bibs and waypoint/rally lines (new ground layer, PASS_ALWAYS), retail water pixel shaders (sparkles/noise, river alpha edge), sky box, scenery shroud. Doc: `WORLD_UI_COMPLETENESS.md`. Env fallbacks: `GENERALS_NATIVE_DISABLE=radar,tracks,bibs,waypoints,skybox,water`. New receipt `world_overlays`. Zero Hour untouched (hooks are `RTS_GENERALS`-guarded).
