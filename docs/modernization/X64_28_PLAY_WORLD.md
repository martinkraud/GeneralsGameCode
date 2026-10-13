# X64.28 - play world (CP1 of FIRST PLAYABLE)

Status: candidate `x64-28-play-world` was executed natively twice (section 9): playable, with two
crashes and a receipt-order defect. Fix candidate `x64-28-play-world-fix1` is staged and preflighted,
**not executed**. Original status: offline batch complete, candidate staged and preflighted,
**not executed**. Awaiting the developer's
native run. Plan and dependency graph: `FIRST_PLAYABLE_ROADMAP.md`. Current state: `AGENT_HANDOFF.md`.

## 1. Scope

Adds the fifth strict profile `-nativeOrdinaryPlay` (Generals AMD64 only; Win32 and Zero Hour reject it).
It implies `-nativeOrdinaryTerrain` (and Game/Menu/Host) and adds only:

1. native rendering of W3D world objects (`NativeWorldObjects`) through the D3D11 TerrainOverlay path,
2. native picking against the same registry,
3. the in-game UI draw (`drawNativeWorldOverlays` + `InGameUI::DRAW`) and the full `ControlBar::update`,
4. a user-driven loop (no scripted camera, no logic-frame bound, 60 minute wall-clock bound).

The terrain/game/menu/host tokens, Zero Hour and the Win32 reference keep their behavior
(`test_ordinary_play_contract.py`). `m_headless` stays TRUE; world selection is UI-without-world.

## 2. Renderer summary

- `WorldBlend` enum on the TerrainOverlay material; D3D11 blend states for SrcAlpha, Opaque, Additive,
  AdditiveAlpha, Multiply, InvMultiply, Premultiplied; alpha test reference 0x60.
- Registry fed by `W3DModelDraw`/`W3DDebrisDraw` (else-branch of the legacy scene add/remove; no-op outside play).
- Per frame: frustum cull, drawable hidden/ghost skip, HLod current LOD + additional models, animation step,
  CPU skin deformation, ALIGNED/ORIENTED meshes, CPU vertex lighting (ambient + up to 3 directional from the
  terrain-object lighting, tint, infantry scale), alpha override/stealth, tread UV scroll, team color
  (ZHCD palette / ZHCA hue / HOUSECOLOR), pooled geometry chunks (60k vertices), opaque then back-to-front
  translucent.
- Native static scenery: trees (foliage state) and props, replacing the absent legacy buffers.
- Selection rings: screen-space circle on terrain (green own / red enemy / white other).

## 3. Receipts (in `startup.log`, run directory)

`world_objects_first_frame`, `world_objects_texture_failure` (per name, bounded), `play_progress`
(15 s x3 then every 60 s: logic frame, CRC, money, selection, camera, objects drawn, input counters),
`play_summary`, `world_objects_summary`, `play_frame_N.bmp` (3 composed frames), plus all terrain receipts.
Log capacity is 2048 records and fails closed; play receipts are capped (`playReceipts < 120`).
Exit reasons: `game_ended`, `game_quit`, `boundary` (shell push, e.g. score screen), `time_bound`.

## 4. Validation evidence (commit `d9e595ce2`, rerun after the final distance-sanitize edit)

| Check | Result |
| --- | --- |
| Win64 `g_generals z_generals g_googletest d3d11_ui_test native_startup_gate_test` build | pass |
| Win64 ctest | 17/17 |
| Win32 full build + ctest (reference intact) | pass, 25/25 |
| `g_googletest` with offline asset env vars (169 tests, 6 disabled) | 169 pass |
| Real-model WARP render (tank, humvee, barracks, skinned ranger, foliage), team colors | reviewed images, 0 texture failures |
| Skinned animation advance through native path | 7184 differing bytes between frames |
| Registry ray cast: 4 objects hit, sky misses | pass |
| All 37,350 retail W3D prototypes | 36,861 rendered, 52 intentionally empty, 0 exceptions, 0 chunk overflow, 11 missing texture names (white fallback) |
| Texture census (Patch.big terrain) | DXT1/3/5 DDS + 24/32 bpp TGA only |
| Python contracts (7 files), PowerShell workflow tests (terrain/play/game/menu/host) | all pass |

Offline evidence is not visual acceptance. No game executable was launched by the agent.

## 5. Candidate

| Item | Value |
| --- | --- |
| Name | `x64-28-play-world` |
| Path | `build/dev-runtimes/native/Generals/x64-28-play-world` |
| Source commit | `d9e595ce2bef2c9cb166c0f70c9637582cf762e8` (documentation commit follows; code identical) |
| `generalsv.exe` SHA-256 | `58F5EF9515EA3BAB7B702AED64445ECF46CB2F1D8C071763678D1CD0754ADE4C` (7,526,912 bytes) |
| `generalsv.pdb` SHA-256 | `B2B0553041858970ACE2DFADF3123FF598D709D8757EBFFF8588DAEA342B4145` |
| `manifest.json` SHA-256 | `CF8FD8E1534378AE861266BF8032949AFC8D7F0C94193361588228DA50D825CB` |
| `ordinaryplay.json` SHA-256 | `40B0748E1A5D508A1CA4F9A72D7C0716F01778DA48A0E5F644838EC9DA89CC24` |
| Stage/preflight | `Stage-NativeOrdinaryPlay.ps1` + `Test-NativeOrdinaryPlay.ps1` PASS (AMD64, no D3D8/D3DX imports) |

Run (developer only):

```powershell
pwsh -NoProfile -File scripts/native-runtime/Invoke-NativeOrdinaryPlay.ps1 -Title Generals -Name x64-28-play-world -RunReviewedOrdinaryPlay
```

Select **Lone Eagle** in Skirmish (bridge/shoreline coverage still owed from X64.27). Quit through the menu.

## 6. Expected

Terrain as X64.27; player base with buildings, team-colored units, trees; original Control Bar whose command
card changes with selection; health bars on selected/damaged units; selection rings; RMB scroll / arrow keys /
wheel zoom; box select; move/attack orders; building placement and production; AI base activity.

## 7. Known omissions (by design, fidelity backlog)

Water; shadows; particles and weapon/explosion effects; tracers, lasers, projectile streams, ropes
(invisible, guarded); shroud/fog and fogged ghosts; radar minimap (empty bezel); bibs, terrain tracks, scorch
marks; waypoints; tree sway/topple; specular, env map, multi-pass, MODULATE2X, dynamic lights; score screen
(shell push ends the run); audio, video.

## 8. Watch list

- Black/white/untextured objects (texture failures; see `world_objects_texture_failure`).
- Inside-out or missing faces, mirrored models; wrong team color.
- Frozen treads/animations; units not selectable or picking offset; rings misplaced.
- Control Bar context not updating or dead buttons; build/production not progressing.
- Camera unresponsive (compare `play_progress` cam/keys/left counters).
- Trees missing or z-fighting; translucent sorting artifacts, stealth fade.
- Frame hitches (CPU lighting/skinning); crash at first tracer/laser/police car.
- Run ending early with `boundary`/`menu_exit`; log capacity approaching 2048.

## 9. Native evidence (candidate `x64-28-play-world`, two developer runs)

Both runs are preserved under `build/dev-runtimes/native/Generals/x64-28-play-world/runs/`; failed-run
evidence is immutable. The developer launched the candidate twice by mistake; both are used.

| Run | Outcome | Last receipt | Crash |
| --- | --- | --- | --- |
| `20261013T1151501824715Z` | played ~3375 logic frames (~112 s), then Exit Game | `game_exit` complete (`logic not in game`) | `0xC0000005` after `game_exit`, before `play_summary`: `MessageStream::removeTranslator` -> `PlayTap` scalar deleting destructor -> `DynamicMemoryAllocator::freeBytes` (GameMemory.cpp:2347), called from `W3DDisplay::runOrdinaryGameHost` (W3DDisplayOrdinaryHost.cpp:946) |
| `20261013T1155367083324Z` | played ~830 logic frames, then reconnaissance drone | `play_progress` frame 451 | `0xC0000005` read of address 0: `RadiusDecalTemplate::createRadiusDecal` (RadiusDecal.cpp:80) <- `InGameUI::setRadiusCursor` (InGameUI.cpp:1487) <- `InGameUI::createCommandHint` (InGameUI.cpp:3060) <- `HintSpyTranslator` <- `MessageStream::propagateMessages` |

Method: crash dumps (`CrashMG`/`CrashFG`) symbolized against the staged PDB with a dbghelp tool (exception
record, thread context, stack walk plus raw return-address scan), cross-checked with the receipts and the
recorded replays (`Replays/00000000.rep`, parsed record by record).

Root causes (independent, no shared cause; both are "legacy lifecycle assumption the native profile broke"):

1. **Exit Game**: ownership. `MessageStream::TranslatorData::~TranslatorData` does `delete m_translator`; the play
   tap was a namespace-scope static. Removal at the end of the host loop freed a non-heap pointer. Not related
   to rendering, the shell transition itself, or the Control Bar.
2. **Radius cursor / drone**: the native play profile never creates `W3DShadowManager`, so
   `TheProjectedShadowManager` is null; `createRadiusDecal` used it without a check. The trigger is any special
   power / command that shows a radius cursor (the drone, artillery, paradrop, ...), not the drone object.
3. **Receipt order** (found while reading the classifier errors): `world_objects_first_frame` was emitted inside
   the object pass, i.e. one record before `terrain_first_frame`, which the evidence route requires first. The
   classifier was right; the emission order was wrong. (Both runs report `Order/code mismatch
   world_objects_first_frame` for this reason, plus the missing completions caused by the crashes.)

### 9.1 Contracts natively proven by the successful portions

Evidence: receipts (`world_objects_first_frame`, `play_progress`), composed frames, and replay command streams.

| Contract | Evidence | Status |
| --- | --- | --- |
| Native world objects drawn | 694 registered; objects_drawn 6 -> 30 over the run, 124 meshes/150 batches/2144 triangles first frame, 0 texture failures, 0 unsupported states | proven (visual quality is the developer's observation) |
| Buildings and units | developer: base, constructed structures; replay: 5 `DOZER_CONSTRUCT`, 17 `QUEUE_UNIT_CREATE` | proven |
| Animation | 3 skinned objects first frame; developer played with units | counters only; developer observation |
| Selection / picking | 13 `CREATE_SELECTED_GROUP` (run 1), 3 (run 2); `select` counter 12 | proven |
| Camera and input | `keys` 415, `left` 16, `right` 2, camera moved (470,1569)->(462,1468) | proven |
| Original Control Bar | developer used it; build/produce/science/rally commands issued through it (`PURCHASE_SCIENCE`, `SET_RALLY_POINT`) | proven |
| Building / production | money 10000 -> 600; 17 queued units; 5 structures | proven |
| Movement / attack orders | **no `DO_MOVETO`/attack messages in either replay** | NOT proven (not exercised) |
| AI activity | AI commands are not in the replay and no per-AI receipt exists; logic CRC advanced | NOT proven |
| Clean exit | crashed in teardown | NOT proven (defect 1) |
| Radius cursor / special powers | crashed | NOT proven (defect 2) |

## 10. Fixes (fix1)

1. `W3DDisplayOrdinaryHost.cpp`: the tap is created with `MSGNEW("PlayTap") PlayTap` and owned by the stream
   (static instance removed).
2. `RadiusDecal.cpp`: after setting the logic-visible `m_empty = false`, return when
   `TheProjectedShadowManager` is null (the decal is simply not drawn; simulation state identical).
3. Same family, found by audit of every legacy shadow/scene singleton that is null in the headless world
   selection (`TheW3DShadowManager`, `TheProjectedShadowManager`, `TheTerrainTracksRenderObjClassSystem`,
   `TheWaterRenderObj`, `TheSmudgeManager`, `W3DDisplay::m_3DScene`, `m_3DInterfaceScene`, tree/prop/bib/waypoint
   buffers, shroud): one more unguarded use, `W3DDebrisDraw::setModelName` (`TheW3DShadowManager->addShadow` for
   every spawned debris with a shadow type, i.e. the first destroyed unit) is now guarded. All other uses were
   already guarded (shadow/track/water/smudge in the draw modules, scene in laser/rope/tracer/projectile/
   police-car, shroud/bib/waypoint buffers); `W3DDefaultDraw` and `W3DMouse` W3D-cursor paths are debug-only or
   not selected by `-winCursors`.
4. `world_objects_first_frame` is published right after `terrain_first_frame` (pending receipt), keeping the
   classifier route unchanged.

Regression coverage: `Tests/Google/Core/GameEngine/Common/PlayWorldLifecycleTest.cpp` (stream ownership
contract; `createRadiusDecal` with a null manager is non-empty and safe; runs in `g_googletest`/`z_googletest`,
Win32 and Win64), and `test_ordinary_play_contract.py` (heap tap, no `attachTranslator(&`, guard order in
RadiusDecal, all shadow-manager dereferences guarded, receipt order). The classifier and acceptance contracts are
unchanged.

Not covered natively: teardown after `game_exit` (summaries, `releaseAll`, display detach) has only run in the
offline workflow tests, never in a native play run; fix1's first run is its first real exercise.

## 11. Candidate `x64-28-play-world-fix1`

| Item | Value |
| --- | --- |
| Path | `build/dev-runtimes/native/Generals/x64-28-play-world-fix1` |
| `generalsv.exe` SHA-256 | `7EACE4771B302A8C7E1B622C8AA8006731B3D543C6F1710E2629D68685B4326B` (7,526,912 bytes) |
| `generalsv.pdb` SHA-256 | `93BABC359F39C6331C5F884D4B2786FE8DF611DDE8915E94C6FA1C6E6861B3DC` |
| `manifest.json` SHA-256 | `667FC8AE150CF1BC6025E2CA17DBD2D7E0FCCBA056BC502C0FB9C9E805914B3B` |
| `ordinaryplay.json` SHA-256 | `CB86F0DC908EBFF93B2664799F20887D4F78826391326FEF61A4831231AAFDE3` |

```powershell
pwsh -NoProfile -File scripts/native-runtime/Invoke-NativeOrdinaryPlay.ps1 -Title Generals -Name x64-28-play-world-fix1 -RunReviewedOrdinaryPlay
```

Validation: Win64 build + ctest 17/17, Win32 build + ctest 25/25, `g_googletest` 171 pass with offline-asset env
vars, 7 Python contract files, 5 PowerShell workflow tests, stage + preflight PASS.

Please test in fix1: a reconnaissance drone (and one other radius-cursor power if available); Exit Game from the
in-game menu (expect `play_summary`..`WinMain` receipts and a PASS from the wrapper); issue explicit move and
attack orders (none were exercised); watch AI base activity; let a unit die (debris path); then quit.

## 12. Native evidence (candidate `x64-28-play-world-fix1`, run `20261013T1216116100741Z`)

Outcome: **clean process exit (code 0), no crash dump, no shutdown exception**; wrapper result FAILED with one
classifier error: `Fewer than 150 logic frames in game`.

Receipt sequence (all present, in order, code 0): `game_exit` (`match ended by the player or the rules`) ->
`play_summary` -> `FP_summary` -> `terrain_summary` -> `world_objects_summary` (`registered=0`: every drawable had already been
destroyed and unregistered) -> `game_summary` (`exit=game_ended`) -> `ordinary_host_boundary` ->
`engine_cleanup` begin -> `display_detach` begin -> `ui_residency_release` (live_residencies=0) ->
`display_detach` complete -> `engine_cleanup` complete -> `app_window_release` -> `minidumper_shutdown` ->
`native_shutdown armed` -> `WinMain exit`. The previous PlayTap crash (before `play_summary`) did not recur:
the first thing that failed in run 1 of the candidate is now the first thing that works.

Root cause (new, distinct from the PlayTap ownership bug): `game_summary logic_frames=0`. In the host loop the
block `if (inGame && !exiting)` updated `lastFrame = TheGameLogic->getFrame()` every iteration. When the
player leaves the match, the same iteration that detects it (`!TheGameLogic->isInGame()` -> `game_exit`) runs
that block after clear-game-data has reset the frame counter, so `lastFrame` became the start frame and
`lastFrame - startFrame` was 0. Verified: the real log with only `logic_frames=0` replaced by the last in-game
value classifies Valid with the unchanged classifier (`Test-NativeOrdinaryPlayEvidence`). The terrain/game
profiles are unaffected because they exit through the `exiting` flag, which skips that block.

What this run natively proves (receipts, replay command stream, developer observation):

| Contract | Evidence | Status |
| --- | --- | --- |
| Interactive play loop, ~6291 logic frames (~210 s), 13,583 presents, no crash | play_summary, replay, exit 0 | proven |
| World objects | 694 registered, up to 21 drawn, 1.5M meshes over the run, 0 texture failures, 0 unsupported classes, `unsupported_state=30488` (counted, fallback), `chunk_overflow=0` | proven |
| Selection / box select / camera / wheel | 36 selections, 882 keys, 6 wheel, 52 left + 23 right clicks | proven |
| Move orders | replay: 13 `DO_MOVETO`; receipt move=13 | proven |
| Attack order | replay: 1 `DO_ATTACK_OBJECT`; receipt attack=1 | proven (single order) |
| Build / production / cancel | 5 `DOZER_CONSTRUCT`, 28 `QUEUE_UNIT_CREATE`, 3 `CANCEL_UNIT_CREATE`, science + rally | proven |
| Supply docking | 3 `DOCK` | proven |
| Enemy AI active combat, pathing, attacking the player's base | developer observation of enemy units reaching and attacking the base | proven (visual) |
| AI base construction / production | not in receipts or replay (AI commands are not recorded; no AI receipt exists) | NOT claimed |
| Clean exit teardown | all teardown receipts, exit code 0 | proven natively (gate failed only on the metric above) |
| Radius cursor / special power | not exercised in this run (fix1 radius guard stays unproven natively) | NOT proven |

## 13. Fix (fix2) and adjacent audit

1. `W3DDisplayOrdinaryHost.cpp`: the per-iteration in-game block is guarded with `TheGameLogic->isInGame()`, so
   `lastFrame` always holds the last frame observed while in the match. Classifier and contracts unchanged.
2. Adjacent audit of the same pattern ("a value sampled from engine state in a loop whose last iteration runs
   after the engine reset it"): `startFrame`/`lastFrame` are the only host values that cross the exit
   transition; `elapsed`/`updates` are host counters; the play-receipt block and milestone loop are inside the
   guarded block (previously they could not fire only because the reset frame happened to be smaller than the
   next threshold - the unsigned `frame - startFrame` would have underflowed); `DriveTerrainExercise` and the
   `frame_bound` exit are non-play and reached only while `!exiting`.
3. Ownership audit (second pass, motivated by the earlier teardown bug): no other static object is handed to
   an engine-owned container in the host (`&g_` passed to engine APIs: none); the shell push guards are plain
   function pointers cleared before return; `NativeWorldObjects` registry removal after `releaseAll` is a
   no-op for unknown objects.

Regression coverage: `test_ordinary_play_contract.py::test_last_logic_frame_is_not_overwritten_after_leaving_the_match`
(single `lastFrame` assignment, guarded by `isInGame()`, executes before the exit detection, the 150-frame
classifier contract untouched). Not covered by a host unit test: the host loop is Win64 native-only; the
classifier outcome on the real log was checked offline as described above.

## 14. Candidate `x64-28-play-world-fix2`

| Item | Value |
| --- | --- |
| Path | `build/dev-runtimes/native/Generals/x64-28-play-world-fix2` |
| `generalsv.exe` SHA-256 | `5A9FA5BC6AB782FDA6F6463513528616D8393968312094FDEC860467379C19A1` (7,526,912 bytes) |
| `generalsv.pdb` SHA-256 | `4E1093BF887C1260479A033FACFFC207292233C868C11CC7DDD4D18AC84D5D4D` |
| `manifest.json` SHA-256 | `28B51C4B52DDC8E0D78B1A0E974A48E461D99BDB4435C65209090CE24ED2FD32` |
| `ordinaryplay.json` SHA-256 | `9213FD4D042D8AFC1DBF5A0043C797FC320739A26D3B597E2FEB804C8D23B440` |

```powershell
pwsh -NoProfile -File scripts/native-runtime/Invoke-NativeOrdinaryPlay.ps1 -Title Generals -Name x64-28-play-world-fix2 -RunReviewedOrdinaryPlay
```

Validation: Win64 build + ctest 17/17, Win32 build + ctest 25/25, `g_googletest` 171 pass with offline-asset env
vars, 7 Python contract files, 5 PowerShell workflow tests, stage + preflight PASS.

Please test in fix2: play at least a minute, then Exit Game (expect wrapper PASS with `logic_frames` equal to the
match length). If you can, also use a radius-cursor power (reconnaissance drone) once, since fix1's guard has
not been exercised natively. Do not rerun fix1 or the original candidate.
