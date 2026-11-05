# Multiplayer Smoothness & Frame Pacing Audit

Status: audit + fix delivered, automatically validated; **a real two-PC re-test with the telemetry is pending** (procedure at the end).
Scope: presentation timing and frame pacing only. The authoritative simulation (30 ticks/s, commands, RNG, ordering, AI, pathfinding) is untouched.

## 1. Question

Two real LAN matches (laptop host, desktop client) finished with no mismatch, but both players felt multiplayer was less smooth than singleplayer. Treated as two separate concerns: the
two matches proved correctness; this audit is about what the player *sees*.

## 2. Method - measure first

`GENERALS_FRAMEDELIVERY=<dir>` (opt-in, inert when unset; `Common/FrameDeliveryTelemetry`, pure statistics in `Common/FrameDeliveryStats.h`) records, per render iteration and per 5 s window:

* render interval p50 / p95 / p99 / max and "hitches" (> 1.5 x median) - **not** average FPS,
* completed logic ticks per iteration, tick spacing (p50 / p95 / p99 / max) and tick duration,
* presentation clock: share of iterations that interpolate (`valid`), frozen iterations (alpha pinned / invalid), history resets (`epoch`), alpha regressions,
* network: stalled iterations, negotiated logic frame rate, run-ahead, cushion, self-slug / schedule-reset / release counters, and the schedule remainder / period.

`scripts/native-runtime/Invoke-FrameDeliveryRun.ps1` drives the real screens into a match and quits: `-Mode Skirmish` (offline) or `-Mode Lan` (autoflow `lan`: Network -> Host -> AI in the slots -> Start).
`GlobalData::m_netMinPlayers = 1` ("sandbox") allows a LAN game with one human, so **the real `TheNetwork` lock-step path runs on one PC** (loopback: no latency, so cushion / stall behaviour needs the
real two-PC run). `scripts/performance/Summarize-FrameDelivery.py` prints a run (or compares runs). Developer-only overrides for measurement: `GENERALS_FORCE_VSYNC=0|1`, `GENERALS_FORCE_CLIENT_SIZE=WxH` (session only, never written back).

## 3. Timing architecture (as found)

```
 network / commands -> Network::update() (QPC schedule, m_frameDataReady) -> GameLogic::UPDATE (authoritative 30 TPS) -> GameClient::step
                                                                               |
 render loop:  GameEngine::update -> GameClient::UPDATE (draw, uses PresentationTiming of the previous iteration) -> ... -> FramePacer::update (limiter wait, presentation clock)
```

* Offline: accumulator in `GameEngine::canUpdateRegularGameLogic`, at most one tick per iteration, remainder published to `PresentationClock`.
* Network: `Network::timeForNewFrame()` releases a tick when the QPC time reaches `m_nextFrameTime` (and the other players' commands are in), advances by `frameDelay` (+10 % when the cushion is low), re-anchors when
  more than two periods behind. Rendering is already decoupled from that (the render loop never blocks on the network; `Network::update` only polls).

## 4. Findings

| # | Finding | Evidence | Effect on the player |
|---|---|---|---|
| 1 | **Presentation interpolation was switched off in every network game.** `FramePacer::update` required `TheNetwork == nullptr` and `canUpdateGameLogic` published `observePresentationScheduler(0,0)` for networks. | 120 Hz render, same laptop, same flow: offline 99.8 % of iterations interpolate, LAN **0.0 %**. | Units/vehicles/turrets move in 30 Hz steps (move - hold - move) while the camera moves at the render rate: exactly "not as smooth as singleplayer", most visible on a high-refresh desktop. |
| 2 | **LAN matches lifted the render FPS limit** (`LANAPI::OnGameStart`: `m_useFpsLimit = false`, a retail leftover from when logic followed render). | LAN, VSync off: frame interval p50 2.3 ms (440 fps) vs 8.33 ms offline at the same cap; irregular intervals, CPU/GPU burn. | Ignores the player's FPS cap; uncapped delivery without VSync; with VSync the pacing depended only on the present call. |
| 3 | **The negotiated network logic rate was clamped by the *render* cap** (`ConnectionManager::updateRunAhead`: `clamp(MIN_LOGIC_FRAMES, minFps, m_framesPerSecondLimit)`; the render cap is 30..240 now). The slowest player is held at 30 but the others could be asked for up to the cap. | Code reading + unit test; the single-human sandbox cannot show it (the router excludes itself). | A faster machine asked for e.g. 60 ticks/s while the slowest is held at 30 gets bursts of ticks followed by stalls. Needs the real two-PC log to see whether it occurred (`net_framerate`). |
| 4 | A first version of finding 1's fix predicted the next alpha from the previous interval. | Telemetry showed alpha stepping **backwards** by up to 0.5 tick after a slow frame (13 events per minute). | Unit would jump back. Fixed by reading the schedule after the limiter wait (below). |
| 5 | Weak laptop (1920x1200, VSync, 60 Hz panel): render-bound. First measurements showed 34 ms frames offline **and** LAN (same), later runs 16.7 ms in both: machine state (thermal / background load), not a multiplayer effect. | Repeated runs; the A/B below was taken back to back. | Where render >= 33 ms the 30 TPS cap is also the render rate; nothing to interpolate. The net logic rate follows the slowest *display* fps (MIN_LOGIC_FRAMES 5): a laptop that really averages < 27 fps lowers the match speed for everybody (retail design, left unchanged; it is logged as `net_framerate`). |
| 6 | Rare 550 ms frames late in a LAN run on the laptop (4 in 12 s, tick duration 1 ms, no stall, no network involvement). | per-frame CSV: ticks 1, `stalled` 0, `tickdur` p99 1.2 ms. | Render-side first-use cost (new effect / shader), not simulation or network. Not MP specific; documented, not changed. |

Not found: blocking receives, barrier waits, sleeps or timers in the render path; render starvation during command processing; per-frame allocations/logging on the network hot path. `Network::update` costs
microseconds per iteration (see tick/iteration numbers); the lock-step wait is a boolean (`isFrameDataReady`), the loop keeps drawing.

## 5. Fix (smallest set; simulation untouched)

1. `FramePacer::update`: network games now feed the presentation clock too. `Network::getTimingDiagnostics` (read-only) offers the schedule remainder (time since the *ideal* start of the current tick interval) and
   its length. The remainder is read **after** the limiter wait (when the next frame is drawn), clamped just below one period while the next tick is still waiting for the next iteration. Stalling (commands late), an
   unusable schedule, or a renderer not faster than the tick rate keeps the canonical (invalid) state. Because the anchor is the *ideal* schedule time, a tick that is released a few ms late does not disturb the displayed timeline.
2. `PresentationClock`: a period change of up to 15 % (the 10 % self-slug) no longer resets the history.
3. `LANAPI::OnGameStart`: the render FPS limit is no longer lifted (the player's cap and VSync now apply in LAN exactly as offline). The GameSpy/online start paths are unchanged (service defunct, untestable here).
4. `NetworkPacing::negotiatedLogicRate` (pure): the ceiling of the negotiated logic rate is `min(render cap, 30)`; the 10 % hysteresis is unchanged. Pacing parameter only - it is applied at the same execution frame on
   every machine and changes when ticks are *released*, never what a tick computes.

Interpolation reads the finished simulation state (previous and current generation) and never writes it; `PresentationClock` has no timers, RNG or Object access.

## 6. Results (ZH, laptop, same session; ticks are 30/s in all rows)

| Run | render interval p50 / p99 | interpolating iterations | alpha regressions | notes |
|---|---|---|---|---|
| 120 Hz render, offline | 8.33 / 8.34 ms | 99.8 % | 0 | reference |
| 120 Hz render, LAN **before** | 2.28 / 3.23 ms (uncapped) | **0.0 %** | - | finding 1 + 2 |
| 120 Hz render, LAN after (predicted alpha) | 8.33 / 8.34 ms | 98.1 % | 7 / 13 per run | finding 4 |
| 120 Hz render, LAN **after (final)** | 8.33 / 8.33 ms | **100 % of windows** (97.4 % incl. load) | **0** | |
| 60 Hz VSync (default settings), LAN legacy behaviour (A/B, 2 runs) | 16.66 / 17.6 ms | **0.0 %** | - | same machine state, back to back |
| 60 Hz VSync, LAN fixed (A/B, 2 runs) | 16.65 / 17.7 ms | **98.7 %** | 0-1 (before the final fix) | |
| Generals, 240 Hz class render, LAN after | 4.2 / 6.2 ms | 95.4 % (offline 94.8 %) | 1 (before the final fix) | parity with offline |

Visual updates per second of a unit in LAN at 120 Hz render: 30 before -> 120 after (offline: 120).

### Stress / attribution

* Simulation cost (headless golden-gate replays, no render, final binaries): 7 AIs on Twilight Flame - tick mean 0.78 ms, p99 5.2 ms, p99.9 7.9 ms against the 33.3 ms budget (Zero Hour 5 AIs on Tournament City: mean 0.71, p99 4.3, p99.9 11.6 ms). Simulation is not the bottleneck anywhere in the matrix.
* Network cost: `Network::update` is a poll; with the lock-step wait a boolean the render loop never blocks (tick spacing p95 <= 34 ms at a 33.3 ms period in all LAN runs, zero stalled iterations in the sandbox).
* Render / present: the laptop at 1920x1200 with VSync is render-bound (16.7 - 34 ms frames depending on machine state); at a 1280x800 client it holds 120 Hz with 8.33 ms p99. The 550 ms frames are render-side first-use costs.
* Not measurable on one PC: real-link latency, cushion, self-slug, run-ahead growth (needs the two-PC run). A large-army LAN scenario could not be automated (the default map has two start positions; a 3-AI start is refused), so the many-unit case is covered by the headless simulation figures above and by the offline developer-mode battle presets from X64.45.

## 7. Determinism and regression evidence

* Golden AI-behaviour gate (`Invoke-AiBehaviorGate.ps1`, Original mode, quick): **ZH 4/4 and Generals 4/4 scenarios have digests identical to the X64.51 baseline executables**, runs repeatable.
* gtest: 556 (ZH) / 557 (Generals) pass, including the new `NetworkPresentationTest` (lock-step schedule model at 60-240 Hz: 30 ticks/s kept, > 99 % interpolating, no alpha regression with jitter and 28 ms hitches,
  self-slug keeps history, a stall invalidates and recovers, a 30 fps renderer never interpolates, pacing ceiling / hysteresis / floor, frame-delivery statistics).
* No file of the simulation (GameLogic, AI, pathfinding, RNG, command execution) was edited.

## 8. Camera (audit)

`W3DView` / `LookAtXlat` scroll, zoom and rotation integrate `FramePacer::getBaseOverUpdateFpsRatio()` (measured render interval); there is no network branch. The earlier camera work (X64.52B) therefore applies unchanged
to multiplayer. In multiplayer the perceived camera/unit mismatch was finding 1 (units at 30 Hz, camera at the render rate), and finding 2's irregular intervals. Unchanged: edge-scroll / judder native question from X64.52B.

## 9. Not done / open

* Real two-PC measurement (the sandbox has no latency): negotiated rate, cushion, stalls, self-slugs on a real link - run the procedure below and send the `framedelivery` folder.
* High-end desktop numbers: this machine is the weak laptop; the 120 Hz rows use a smaller client (1280x800) and VSync off to reproduce the desktop-class render rate. Desktop confirmation comes with the real run.
* Logic rate dropping with a slow display (retail design), the 550 ms first-use render hitches, and the GameSpy start path are documented, not changed.

## 10. Real-match procedure (both PCs)

```
powershell -ExecutionPolicy Bypass -File scripts\native-runtime\Start-GameWithFrameDelivery.ps1 -GameDir "<game folder>" -OutDir C:\lan-test\fd [-PerFrame]
... play the usual match ...
powershell -ExecutionPolicy Bypass -File Collect-LanTestEvidence.ps1 -GameDir "<game folder>" -Role Host|Client -Tag M3 -OutDir C:\lan-test -FrameDeliveryDir C:\lan-test\fd
python scripts\performance\Summarize-FrameDelivery.py C:\lan-test\fd
```

Read: `interpolating` (should be ~100 % while the render rate is above 30 fps), `tick rate` (30), `logic frame rate` (30; anything else means a slow display dragged the match), `stalled iterations`, `self-slugs`.
