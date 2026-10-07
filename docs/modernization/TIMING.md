# Timing and frame-rate architecture

Stage 3A update (2026-10-08): the developer has now manually verified the Stage 2
fix: Ctrl+numpad + reaches 60 render FPS without accelerating infantry/vehicles
or gameplay, and camera motion is visibly smoother. Approximately 30 TPS is
inferred from normal speed, not measured. Release `-fps 60` is ineffective because
its parser-table entry is RTS_DEBUG-only. The Stage 3A candidate adds default 60
and persistent Options render choices 30/60/120/144/165/240, keeping Stage 2's
30-TPS policy intact. See [STAGE3A_FPS_OPTIONS.md](STAGE3A_FPS_OPTIONS.md) for the
complete legacy Game Speed slider audit, tests and manual acceptance. Stage 3A
runtime/UI/persistence remains UNVERIFIED. Earlier status below is historical.

Stage 2 update (2026-10-08): the developer manually verified normal baseline
startup/menus/skirmish/exit and verified that **raising the render/frame-rate cap
using Ctrl + Numpad + caused offline gameplay/simulation to speed up**.
`FramePacer` now enables its existing 30-tick logic scale by default. The offline
accumulator, tick units, main-loop order and network readiness path are unchanged.
See [STAGE2_TIMING.md](STAGE2_TIMING.md) for tests, manual acceptance and risks.
The candidate's 60-FPS gameplay behavior is still UNVERIFIED.

Stage 1 follow-up (2026-10-08): the source-verified default keys are Ctrl+numpad
+/- for render cap and Ctrl+Shift+numpad +/- for offline logic scale. See
[STAGE1_WORKFLOW.md](STAGE1_WORKFLOW.md) for the controlled matrix, source anchors,
press-count expectations and limitations for the original baseline. The developer
has now tested the render-cap increase; logic-scale keys remain runtime-unverified.
At a 30 render cap, enabled 30-rate scaling is unavailable because the control
enables scaling only below the render cap. Stage 2 starts with scaling already
enabled at 30, including at a 30 render cap; explicitly using the old scale
shortcut can still disable it. Stage 1 itself changed no engine code.

Scope: revision `adac468d7`, 2026-10-07. This code already contains upstream
decoupling and interpolation work; do not design from the assumption of retail
code with one FPS constant.

## Code map

Paths below are repository-relative. Function names are the durable anchors.

| Concern | File / symbol |
|---|---|
| Historic base | `Core/GameEngine/Include/Common/GameCommon.h:70`: BaseFps = 30; must never change |
| Logic units | Same header: LOGICFRAMES_PER_SECOND = WWSyncPerSecond; seconds/ms/velocity/acceleration conversions |
| W3D tick constant | `Core/Libraries/Source/WWVegas/WWLib/WWCommon.h:37`: WWSyncPerSecond = 30 |
| Main loop | Title `GameEngine/Source/Common/GameEngine.cpp::execute/update` (ZH 896/937) |
| Logic scheduler | Same file `canUpdateGameLogic`, `canUpdateNetworkGameLogic`, `canUpdateRegularGameLogic` (ZH 816/834/851; Generals equivalents 653 onward) |
| Frame timing | `Core/GameEngine/Source/Common/FramePacer.cpp` and header |
| Limiter | `Core/GameEngine/Source/Common/FrameRateLimit.cpp::wait/reset` |
| Render-cap input | `CommandLine.cpp::parseFPSLimit` (`-fps`), `-noFPSLimit`; GlobalData FramesPerSecondLimit INI field |
| Speed commands | `Core/GameEngine/Source/GameClient/MessageStream/CommandXlat.cpp::changeLogicTimeScale`, render-FPS commands |
| Logic frame / pause | ZH `GameLogic/System/GameLogic.cpp::update/preUpdate/setGamePausedInFrame` |
| Input / client | ZH `GameClient/GameClient.cpp::update`, Keyboard/Mouse update/createStreamMessages |
| Client draw/time | `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp::update/draw/step` |
| Camera | Core `W3DView.cpp::updateCameraMovements`, waypoint motion and view update |
| Drawable visuals | ZH `Drawable.cpp::applyPhysicsXform`, updateDrawable; Core W3DModelDraw |
| Animation sync | `WW3D2/ww3d.cpp::Update_Logic_Frame_Time/Sync`; W3D animation classes |
| Particles | ZH GameClient update gate; Core `GameClient/System/ParticleSys.cpp` phase interpolation |
| Network pacing | Core `GameNetwork/Network.cpp::update/timeForNewFrame/isFrameDataReady` |

## Current execution model

One main engine thread executes client/render work and simulation work in a
single outer loop. Scheduling is **partly decoupled**, not a separate render
thread or a universal fixed-timestep catch-up loop.

Each iteration updates radar, audio and client (including drawing), propagates
messages, updates networking, calls the logic-readiness check, then optionally
executes one logic update and client step. `FramePacer::update` waits and captures
elapsed wall time at the end. It updates the interpolation phase for the next
iteration. Preserve this ordering when evaluating visual latency and stale state.

`GameLogic::preUpdate` clears hasUpdated during the readiness check; a completed
logic tick increments m_frame and sets it true. The next client update uses that
flag for W3D sync and particle stepping. This is intentional state carried across
iterations, not a flag to clear arbitrarily before rendering.

### Offline scheduling and game speed

Original baseline FramePacer defaults: max FPS 30; logic-time-scale value 30;
**logic-time-scale enabled = false**. Disabled means offline logic follows the render/update loop
using an uncapped sentinel. Therefore **raising `-fps` alone can still speed up
offline simulation**. A stored scale value of 30 is insufficient unless enabled.
The Stage 2 candidate changes only the initial enabled state to true. With no
explicit speed override, 30 remains selected as the render cap changes. At cap
30 the existing immediate-tick branch remains in use; above 30 the existing
wall-clock accumulator is selected automatically. Shared Core applies this
default to Generals as well as Zero Hour; Generals gameplay is not validated.

When a finite logic scale is enabled and below the render cap, the scheduler
accumulates `min(lastUpdateTime, 1/logicFPS)` and executes a tick when the
accumulator crosses the target period. There is at most one logic update per
outer iteration. It deliberately clamps stall accumulation; it does not run a
while-loop to catch up all elapsed ticks. When desired logic rate is at least
the render cap, or fast replay mode applies, logic advances each iteration.
Consequently slow rendering can slow the effective simulation rate.

`CommandXlat::changeLogicTimeScale` manipulates the stored scale and enabled
state relative to the render cap. Pause/resume also remembers and restores this
state. Script fast mode, tactical time multipliers and replay fast-forward affect
actual limiting. Do not confuse game-speed controls with rendering-only controls.

### Multiplayer

Network readiness replaces offline accumulation. `Network::update` collects
commands, checks all participants' commands, checks the next frame deadline,
relays that frame's commands and sets readiness. `timeForNewFrame` uses QPC and
the network frame rate (initially 30), adjusts delay for run-ahead cushion and
resets timing when too far behind. FramePacer queries the network rate for
client scaling. This is frame-based lockstep with pacing/stall handling, not
wall-clock dt integrated into all simulation objects.

### Rendering, deltas and interpolation already implemented

`FrameRateLimit::wait` uses QueryPerformanceCounter, Sleep until about 2 ms before
the target, then busy-waits. FramePacer requests 1 ms Windows timer resolution
for its lifetime. The actual frame delta includes work/Present and waiting;
there is no proof that the cap equals the monitor refresh or achieved FPS.
Render cap presets already include 120, 144, 240 and 480. Preset availability
does not demonstrate smooth motion or constant game speed.

FramePacer exposes elapsed update time, FPS, a base/update FPS ratio (flooring
FPS at 5 for input scaling), actual logic/update ratio capped at 1, logic-scaled
seconds/ms and a phase clamped to [0,1]. The phase resets after a logic tick and
advances on intervening render frames.

- Drawable updates receive the logic/update ratio. `applyPhysicsXform` stores
  previous/current visual pitch/roll/yaw/Z and interpolates by the phase. This
  is demonstrated physics decoration, **not proof of general world-position
  interpolation for every unit/projectile**.
- W3DDisplay supplies logic-scaled ms, then `WW3D::Sync(hasUpdated)`. WW3D
  accumulates fractional milliseconds and advances integral sync time on steps.
  Raw/legacy animation update paths need separate auditing; history includes
  `26c5a9b7b` for raw-animation interpolation/sync coupling.
- Particle simulation is gated by !freezeTime and hasUpdated; rendering uses
  phase interpolation in ParticleSys. Bone attachments make update order matter.
- W3DView camera controls, LookAtXlat and InGameUI use elapsed-time ratios;
  waypoint camera movement uses logic-scaled ms with frozen-time exceptions.
- GUI transitions/window animation use the base/update ratio. This does not
  establish that every GUI timer is independent of render cadence.
- Keyboard/mouse poll and generate messages at client frequency. Simulation
  commands must retain execution-frame semantics as input sampling increases.
- Audio updates each engine iteration; Miles playback/callbacks and video
  decode clocks require independent verification for pauses/fast-forward.
- AI, module sleep scheduling and pathfinding are reached through logic ticks.
  Increasing their update frequency changes gameplay and CPU load.

## Candidate next architecture work

First validate the Stage 2 candidate at 30 and 60 without manually enabling
logic scaling, then characterize 120/144/240 and network pacing. Measure tick counts, rendered frame
times, CRCs and visual motion. Document the exact control sequence from a real
runtime; this investigation does not claim a tested launch flag for fixed logic.

Extend the present architecture only where evidence shows gaps:

1. Make normal-speed simulation policy explicit and persistent across offline,
   replay, pause, loading and speed changes; preserve legacy behavior where
   compatibility requires it. Never change BaseFps/WWSyncPerSecond to raise FPS.
2. Define stall/catch-up policy before changing the one-tick-per-iteration rule.
3. Inventory all visual consumers and logic-to-client callbacks. Keep previous
   and current *visual* transforms for missing interpolation, handling spawn,
   teleport, destroy, visibility and save/load discontinuities.
4. Ensure rendering cannot mutate deterministic state or consume logic RNG.
   Preserve command ordering and tick assignments.
5. Test non-integer render/logic ratios, stalls, minimize/device loss, network
   waits, script freezing, speed changes and replay seeking/fast-forward.

A 30 Hz simulation / 144 FPS presentation is conceptually supported by parts
of the current design. End-to-end correctness and smoothness remain UNKNOWN.
Do not rewrite the loop or add threading until the existing implementation's
behavior has a measured baseline.
