# Retail Game Speed / simulation cadence audit

Status: **audit only (X64.41 workstream B).** Nothing in the game was changed. Every statement below is traced to source or to the retail data files in the staged
installations; where a statement is an inference it says so. No Game Speed UI is implemented here.

## 1. What the retail game proves

### 1.1 Normal cadence: 30 logic frames per second, in both titles

| evidence | where |
|---|---|
| The logic cadence constant: `LOGICFRAMES_PER_SECOND = WWSyncPerSecond = 30`, `BaseFps = 30` ("The historic base frame rate for this game. This value must never change.") | `Core/GameEngine/Include/Common/GameCommon.h`, `Core/Libraries/Source/WWVegas/WWLib/WWCommon.h` |
| Retail default frame-rate cap in **both** titles: `UseFPSLimit = Yes`, `FramesPerSecondLimit = 30` | `Data\INI\GameData.ini` read from the staged **Generals** `INI.big` and the staged **Zero Hour** `INIZH.big` |
| The original loop runs **one logic update per main-loop iteration**: `GameEngine::update()` = client update, message stream, network update, then `TheGameLogic->UPDATE()` once when not paused (offline) or when the network frame data is ready; `execute()` repeats `update()` and then busy-waits until `1000/m_maxFPS - 1` ms have passed since the previous iteration when `m_useFpsLimit` is on | first commit `3d0ee53a0` of this repository, `Generals/Code/GameEngine/Source/Common/GameEngine.cpp` (`update`, `execute`); the Zero Hour copy is identical in structure |
| `m_maxFPS` is taken from the INI value in `GameEngine::init` (`setFramesPerSecondLimit(TheGlobalData->m_framesPerSecondLimit)`); `DEFAULT_MAX_FPS = 45` is only the fallback for a missing value | same files |

So in retail, **render frame rate, loop iteration rate and logic frame rate were one and the same number, and that number was the FPS cap**: Normal = 30 logic frames per second = 33.333 ms per frame
in Generals **and** in Zero Hour. The 30 TPS used by this project's benchmark is therefore the proven retail Normal, not an assumption.

All gameplay data is expressed in time units and converted once at load into logic frames/second constants:
`INI::parseDurationReal/UnsignedInt/UnsignedShort` -> `ConvertDurationFromMsecsToFrames`, `parseVelocityReal` -> `ConvertVelocityInSecsToFrames`,
`parseAccelerationReal`, `parseAngularVelocityReal` (`Core/GameEngine/Source/Common/INI/INI.cpp`), so the unit of simulation time is the logic frame (1/30 s).

### 1.2 The original "Game Speed" control = the FPS cap

* Retail **Skirmish** had a "Game Speed" slider (`SkirmishGameOptionsMenu.wnd:SliderGameSpeed`, captions "Slower" / "Faster", numeric read-out `StaticTextGameSpeed`).
  Original code (first commit, both titles): the slider position is an **FPS value 15..60**; positions above 60 mean "uncapped" (read-out "--", `maxFPS = 1000`); below 15 is clamped to 15;
  the value equal to the INI default (30) is drawn in the disabled colour; it is persisted as the `FPS` entry of `Skirmish.ini`.
* At game start the value is sent as the **fourth argument of `MSG_NEW_GAME`** (`msg->appendIntegerArgument(maxFPS)`), `GameLogic::onNewGame` (`GameLogicDispatch.cpp`) calls `TheFramePacer->setFramesPerSecondLimit(maxFPS)` and forces
  `m_useFpsLimit = true`, and the recorder stores it in the replay header (`maxFPS` field).
* Mechanism: **wall-clock pacing of the whole loop**. Because logic ran once per loop iteration, changing the cap changed the logic frames per second 1:1 (faster = more logic frames per real second). No time scaling, no change of the
  logic frame duration, no change of any game data.
* Campaign and the other single-player starts send only three arguments (`MainMenu.cpp`, `ScoreScreen.cpp`, `MapSelectMenu.cpp`: game mode, difficulty, rank points) - **no FPS argument**, so they run at the INI default (30). The Skirmish
  code states the intent: "just specified so we can add the game speed last". Multiplayer: no per-player slider (section 1.4).
* The current modern Skirmish screen keeps the code but hides the slider (the layout reuses its slot for "Starting Cash"); Options owns a *render* frame-rate limit (`FrameRateLimit`, 30..240).

### 1.3 Other speed mechanisms in the source

| mechanism | effect | availability |
|---|---|---|
| Replay fast-forward ("TiVo", `m_TiVOFastMode`) | logic runs without waiting (`canUpdateRegularGameLogic` returns true); render pacing off | hotkey; **replays only** in release (`_ALLOW_DEBUG_CHEATS_IN_RELEASE` otherwise) |
| Script `setTimeMultiplier` / `isTimeFast` | the tactical view multiplies camera/animation time and the loop skips the FPS wait (cinematics, debug window) | map scripts / debug |
| `MSG_META_INCREASE/DECREASE_MAX_RENDER_FPS` | changes the **render** cap (message "Max Render FPS is N") | upstream TheSuperHackers feature, key-bindable |
| `MSG_META_INCREASE/DECREASE_LOGIC_TIME_SCALE` | changes the **logic** rate independently of render (`changeLogicTimeScale`); in release the minimum is 30 (`LogicTimeScaleFpsPreset::MinFpsValue = LOGICFRAMES_PER_SECOND`), debug allows 5; refused when a network game runs | upstream feature, key-bindable; not part of retail |

### 1.4 Multiplayer

The network layer owns the logic rate: `TheNetwork->getFrameRate()` (starts at 30). The packet router computes the **minimum average FPS of all players**, keeps it if within 10 % of the current rate,
clamps it to `[MIN_LOGIC_FRAMES = 5, TheGlobalData->m_framesPerSecondLimit]`, derives the run-ahead from latency x that rate, and broadcasts `NetRunAheadCommandMsg(runAhead, frameRate)` with an execution frame
(`ConnectionManager::updateRunAhead`). Logic advances only when `isFrameDataReady()`. Consequences (retail behaviour): a slow machine slows everybody ("slugging"); nobody can run faster than the cap; there is no
per-player or lobby speed slider. The decision is made on frame numbers, so replays and CRCs are unaffected.

## 2. Where this fork changed the concepts

* `FramePacer` (upstream `xezon` work) **decouples the logic time step from the render update**: `canUpdateRegularGameLogic` accumulates real time and issues a logic update only when `1/logicTimeScaleFps` has elapsed;
  when the logic time scale is disabled the logic rate falls back to the render rate "as per the original game behavior" (`getActualLogicTimeScaleFps`).
* This project (X64.34) sets `m_enableLogicTimeScale = TRUE` by default and `m_logicTimeScaleFPS = LOGICFRAMES_PER_SECOND`: **offline simulation stays at 30 TPS whatever the render cap is**
  ("Keep normal offline simulation at 30 ticks/s when the render cap changes"). Render interpolation (`getLogicFramePhase`) and the presentation clock consume the render cap; the network overrides the logic rate.
* Visible consequences relative to retail: (a) Options -> render FPS 30..240 no longer changes game speed (in retail raising the FPS cap *would have*); (b) the `maxFPS` argument of `MSG_NEW_GAME` and the replay header still
  set the *render* cap on start/playback - a replay recorded with a 120 FPS cap (as the user replay here: header `maxFPS = 120`) plays its logic at 30 TPS; the same file in retail would have been recorded **and** played at 120 logic
  frames per second; (c) the Skirmish slider is hidden. The retail Normal experience (30 TPS) is preserved by construction.

## 3. Timing classification (does a speed change keep gameplay ratios?)

A = simulation frames/ticks | B = simulation (game) time derived from frames | C = wall-clock | D = render frames | E = mixed

| system | class | evidence |
|---|---|---|
| movement, locomotors, turning, acceleration | A (INI per-second values converted to per-frame at load) | `INI.cpp` parseVelocity/Acceleration/AngularVelocity conversions |
| weapons: reload, delay between shots, clip, burst | A (INI durations -> frames at load) | `parseDuration*` conversions (weapon INI fields are durations) |
| projectiles, missiles, flight time | A | per-frame velocities (`parseVelocityReal`), update modules run from the logic update heap |
| build/production times, upgrades, sciences, special-power cooldowns | A (INI durations -> frames at load; inferred from the shared parsers, not traced module by module) | `INI.cpp` duration parsers |
| economy: income per second, harvesting | A/B (`Money` income buckets `frame / LOGICFRAMES_PER_SECOND`; supply cycles are update-module sleeps) | `Money.cpp` |
| AI (skirmish AI, build orders, attack waves), AI think delays | A | update sleeps / frame counters |
| scripts: timers, counters, "wait n seconds" | B (`LOGICFRAMES_PER_SECOND * seconds`) | `ScriptActions.cpp` (`Int frames = LOGICFRAMES_PER_SECOND * timeInSeconds`) |
| save / load, replay commands, CRC cadence, network execution frames | A (frame-indexed) | `Recorder`, `GameLogic::getFrame`, `ConnectionManager` |
| camera movement, scrolling, UI animation, window transitions | D in retail (per render frame, which equalled logic frames); in this fork scaled by `getBaseOverUpdateFpsRatio()` (30 / update fps) | `W3DView`, `AnimateWindowManager`, `GameWindowTransitions`, `InGameUI` use the ratio |
| particles, decals, ground-effect decoration, drawable interpolation | D/E (client update per render frame; fork adds `getLogicFramePhase`) | `ParticleSys`, `Drawable` |
| audio (Miles) | C (real time) with event triggers from logic (inference: the audio device is a real-time mixer; not tested at other speeds) | `MilesAudioManager` |
| wall-clock reads in the logic | none relevant to behaviour (profiling `QueryPerformanceCounter`, a load-progress timer in `GameLogic.cpp` for the loading UI, a debug-only `RAILROAD_DESYNC_TEST`) | grep of `timeGetTime/GetTickCount/QueryPerformanceCounter` in `GameLogic` |

Conclusion: **everything that decides gameplay is a function of logic frame counts (A/B)**. Changing the logic rate only rescales wall-clock duration and keeps all gameplay ratios (unit speed vs weapon
reload vs build time vs income). The things that do *not* scale are audio (real time) and client-side presentation, which the fork already drives by explicit ratios.

## 4. Product contract (proposal; not implemented)

1. **Normal = 30 TPS exactly** (33.333 ms), the retail Normal of Generals and Zero Hour, and the only value for: campaign and scripted missions, saved games' default, multiplayer default, and every performance claim.
2. **Game Speed is simulation cadence, nothing else.** It must be a separate setting from Render FPS Limit, Render Scale, VSync, Smooth presentation and the Enhanced HUD, and must never be derived from them.
   A machine that renders faster never plays faster.
3. **Levels** (names are UI; values are TPS and are not final until measured on target hardware): *Slower* (e.g. 15-25), **Normal (30, default)**, *Faster* (e.g. 36, 45, 60). The retail slider's 15..60 range and "uncapped" are the
   historical bounds; "uncapped" is a replay/benchmark facility, not a gameplay level.
4. **Where it is set**:
   * *Skirmish -> Match Options -> Game Speed* (per match, host/local authority; it was the retail location). Stored in the match setup, in `Skirmish.ini` as before (`FPS` entry semantics), in the **replay header**
     (the `maxFPS` field already exists) and in saved games, so playback and loading reproduce the pace.
   * *Options -> Gameplay -> Game Speed* only as the **default for newly created skirmish matches** (a personal preference), shown with the Normal marker; it must not change a running match or the campaign.
   * **Campaign and scripted missions: Normal only** (scripts, cinematics and map timers are tuned at 30; keep the control disabled with the reason).
   * **Replays**: play at the recorded speed; the existing fast-forward stays a view-side facility.
   * **Multiplayer**: not user-selectable per player. If ever offered it is a host-chosen *ceiling* in the lobby, broadcast with the match setup, folded into the existing minimum-FPS negotiation (a slow machine still lowers the
     rate for everyone); never a client-local setting. Out of scope until the replay determinism issue (R1) is closed and the lockstep has been validated at non-Normal rates.
5. **Classic vs Modern Experience**: Classic keeps the retail slider semantics (a Skirmish-only 15-60 FPS-style control, default 30). Modern Experience may present named levels, but the same TPS values and the same
   storage. Neither changes defaults: a new install plays at 30.
6. **Safety**: gameplay ratios are preserved (section 3). Risks: scripts/cinematics (campaign) assume 30; audio does not scale; AI think cadence and network latency tolerances are frame-based (run-ahead in frames means the
   same *frames* of latency tolerance but a different *time*); physics-like decoration (rubble, particles) is client-side and interpolated; any code that reads real time in the logic would break determinism (none found, section 3).

## 5. CPU budget implications

The per-tick deadline is `1000 / TPS` ms: Slower 20 TPS = 50 ms; **Normal 30 = 33.333 ms**; 36 = 27.8 ms; 45 = 22.2 ms; 60 = 16.67 ms. The simulation cost per tick does not change with the speed, so a faster level needs proportionally
more CPU and any tick longer than the shorter deadline becomes a visible stall. Normal remains the optimization target. The post-X64.40 heavy benchmark (72 000 ticks, AI vs AI, i7-class desktop) in terms of other deadlines:
see section 6.

## 6. Measured deadline exposure of the heavy benchmark at other speeds

The same 72 000 measured tick durations (X64.40 + X64.41 build, 4 runs, Twilight Flame, 7 brutal AI, mean tick 0.947 ms) evaluated against other per-tick deadlines. This is a **derived** model
(ticks run back to back as they become due); it assumes the cost of a tick does not depend on the speed (true for the simulation: only the wall-clock spacing changes). Means over the 4 runs:

| speed | deadline | ticks over the deadline (of 72 000) | share | derived missed deadlines | max backlog |
|---|---|---|---|---|---|
| Slower 20 TPS | 50.00 ms | 24 | 0.033 % | 25 | 74 ms |
| Slower 25 TPS | 40.00 ms | 30 | 0.041 % | 36 | 84 ms |
| **Normal 30 TPS** | **33.33 ms** | **34** | **0.048 %** | **48** | **91 ms** |
| Faster 36 TPS | 27.78 ms | 47 | 0.065 % | 73 | 96 ms |
| Faster 45 TPS | 22.22 ms | 54 | 0.075 % | 105 | 102 ms |
| Faster 60 TPS | 16.67 ms | 82 | 0.114 % | 176 | 110 ms |

Average CPU use of the simulation thread is mean tick x TPS: 2.8 % at 30 TPS, 5.7 % at 60 TPS for this heavy scenario; the limit is the **tail**, not the average: at 60 TPS a 33 ms tick costs two deadlines, and the benchmark's 34 ticks above 33 ms
(including the periodic 70-110 ms script tick every 300 frames) would then be visible stalls. Faster levels therefore need the tail work (scripts, object updates, the remaining searches) done first; they must not be offered as a way to compensate for slow hardware.

## 7. Implementation risks / open items for a later, separately authorized milestone

* Replay compatibility: today playback pins the logic at 30 TPS (`m_enableLogicTimeScale = TRUE`); an implemented Game Speed must route the recorded `maxFPS` into the **logic** time scale, not the render cap, and treat old
  replays (render cap values 30-240 in the header) as Normal.
* The Options render FPS list (30..240) and the logic time-scale presets (step 5) already exist and are separate; the new setting must reuse `FramePacer::setLogicTimeScaleFps`, never `setFramesPerSecondLimit`.
* Network: `changeLogicTimeScale` is refused in network games and the run-ahead code clamps with `m_framesPerSecondLimit`; a lobby speed would need a protocol decision.
* Time-sensitive systems to re-verify at non-Normal speeds: audio sync, scripted timers, cinematics, the sleepy-update heap (frame based, no issue), UI auto-repeat and double-click timing (real time).
