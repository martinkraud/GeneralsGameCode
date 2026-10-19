# X64.49 - Camera scrolling in presentation time (judder while holding the arrow keys)

## Human validation record (before this milestone)

The complete-runtime Generals / Zero Hour packages (`MKRaudstein-*-x64-Preview-83d6236b1.zip`, X64.48B) passed the human second-PC retest:
complete Generals runtime features including the extended power shortcut bars, Fog of War, 5120x1440 world rendering, all eight Skirmish slots and the packaged runtime
functionality. **Distribution / runtime parity: HUMAN PASS.**

New observation: simulation felt smooth, the reported render rate was about 240 FPS on a high-refresh ultrawide, but holding the arrow keys gave visibly jerky camera motion.

## Trace (how keyboard scrolling reaches the screen)

`WM key message -> MSG_RAW_KEY_DOWN/UP -> LookAtTranslator::scrollDir[] + setScrolling(SCROLL_KEY)` (state, not key repeat: repeat messages are irrelevant)
`-> MSG_FRAME_TICK (appended by GameClient::update, handled in GameEngine::update -> TheMessageStream->propagateMessages(), AFTER that frame was drawn)`
`-> offset = speedFactor * fpsRatio * SCROLL_AMT * keyboardScrollFactor` with `fpsRatio = BaseFps(30) / FramePacer::getUpdateFps()`, i.e. 30 x the interval of the PREVIOUS loop iteration
`-> View::userScrollBy -> W3DView::scrollBy -> setPosition2D (view pivot m_pos)`
`-> next W3DView::update -> updateCameraTransform -> m_3DCamera -> terrain / world / D3D11 matrices -> W3DDisplay::draw -> Present`.

* Cadence: **once per rendered frame** (every `GameEngine::update` loop iteration) - NOT once per 30 TPS logic tick, not a timer, not key repeat. The measured number of scroll steps equals the number of drawn frames at every cap (e.g. 548 at 240, 91 at 30), while only 91 logic ticks occur in 3 s.
* Arrow keys and screen-edge scrolling use the identical code path (`SCROLL_KEY` / `SCROLL_SCREENEDGE` branches of the same switch); RMB scrolling too (same `fpsRatio`).
* The step for the frame displayed NEXT is sized by the interval that ENDED before the frame that was just drawn: a one-frame lag between the interval a step is computed from and the interval it is displayed for.

## Measurement (probe `GENERALS_CAMERA_PROBE`, `Invoke-CameraProbe.ps1`, `analyze_camera_probe.py`)

The probe drives the REAL scroll state (the state the arrow keys / the screen edge set) for a fixed 3 s hold at 30 / 60 / 120 / 144 / 240 FPS caps through the normal
frame pacer, records every scroll step, every drawn frame (view pivot and the render camera position used for the matrices) and the moment each frame is handed to the swap chain,
and writes camera-probe.csv + a summary. Both titles, staged candidates, skirmish map, 5-cap sweep, "before" = the former step (`GENERALS_CAMERA_PROBE_LEGACY=1`, same executable) and "after".
Full tables: `evidence/x64-49/camera-cadence-before-after.txt`, per-run summaries `evidence/x64-49/summary-*.txt`.

Camera speed the viewer gets between consecutive drawn frames (units/s; the commanded speed is 700 Generals / 1076 Zero Hour), key hold:

| cap | G before mean / cv | G after mean / cv | ZH before mean / cv | ZH after mean / cv |
|---:|---|---|---|---|
| 30 | 699.5 / 0.006 | 699.5 / 0.005 | 1075.8 / 0.013 | 1075.8 / 0.012 |
| 60 | 699.5 / 0.008 | 699.5 / 0.007 | 1077.5 / 0.044 | 1076.0 / 0.019 |
| 120 | 705.5 / 0.026 | 705.1 / 0.033 | 1081.5 / 0.099 | 1075.8 / 0.029 |
| 144 | 707.1 / 0.089 | 705.8 / 0.021 | 1078.2 / 0.028 | 1082.2 / 0.032 |
| 240 | **765.6 / 0.390** | **707.3 / 0.031** | **1157.8 / 0.359** | **1084.2 / 0.040** |

Frame pacing while reproducing (interval between frames handed to the swap chain, ms, key hold, mean / p95 / p99 / worst / stddev), "after":

| cap | Generals | Zero Hour |
|---:|---|---|
| 30 | 33.33 / 34.0 / 34.4 / 34.5 / 0.40 | 33.31 / 33.8 / 33.9 / 34.3 / 0.27 |
| 60 | 16.67 / 17.1 / 17.3 / 17.6 / 0.30 | 16.66 / 17.3 / 17.4 / 18.6 / 0.40 |
| 120 | 8.53 / 8.8 / 9.2 / 77.3 (one hitch) / 3.77 | 8.33 / 9.4 / 9.7 / 10.0 / 0.83 |
| 144 | 6.94 / 7.9 / 8.3 / 8.8 / 0.57 | 6.94 / 7.8 / 8.3 / 9.1 / 0.57 |
| 240 | **6.95 / 12.5 / 13.4 / 20.9 / 3.56** | **5.92 / 11.3 / 12.7 / 17.6 / 3.04** |

A 240 FPS cap is **not** 240 smooth frames: in this scene the engine hands frames over every 4 - 13 ms (mean 5.3 - 7 ms), alternating about 1.6 ms and 8 ms in the render +
Present phase (a 4.17 ms budget). This is the render-frame-pacing component (B). It is also why the problem is invisible at 30 - 120 FPS and appears at the top rates.

## Root cause

Combination (D) of **A** and **B** - not logic-tick quantisation (the camera is not tied to 30 TPS) and not keyboard input sampling (C: the key state is level based and sampled every frame):

* A. The camera step of a frame was sized by the *previous* loop interval and applied after the frame was drawn. With a steady frame time that is exact; with the irregular frame times of a 240 FPS cap the step and the interval it is displayed for are anti-correlated (4.17 ms interval gets the step of a 7.3 ms one and vice versa; measured step 2.9 / 5.1 alternating at the same time) so the camera speed swings by a factor of 2 - 3 and its average is also biased high (766 vs 700 units/s).
* B. The frame cadence itself is irregular at 240 FPS, which turns A into visible stutter.

## Fix (narrow, presentation only)

Camera scrolling is integrated **directly in presentation time, not interpolated**: a held scroll is a velocity, the camera pivot is a single user-controlled state (not a simulated object that has a previous and a next logic state), so unit interpolation (`GroundTranslation`) is not applicable.

* `LookAtTranslator` (MSG_FRAME_TICK) still computes the same offset (so the cursor/scroll-amount UI and every speed constant are untouched) but hands it over as a velocity: `View::userScrollByPresented(&offset, fpsRatio)` - same locked-control / scripted-camera checks as `userScrollBy`, stores `offset / fpsRatio`.
* `W3DView::update` calls `View::applyPendingUserScroll()` first, i.e. right before this frame's camera transform is built: it moves the camera by `velocity x 30 x (wall-clock seconds since the previous update)` (`PresentationScroll.h`; QueryPerformanceCounter; first call / stall / counter going backwards never produce a jump; a stall is capped at 0.2 s, the frame pacer's 5 fps floor).
* Release or a forced stop clears the pending velocity (`clearPendingUserScroll`), so nothing is applied after release: no overshoot. The velocity is consumed once per update: no drift, no accumulated state.
* Edge scrolling, RMB scrolling and keyboard scrolling all go through the same call, so all three get the same behaviour; the scroll speed per second is unchanged.

Not changed: the 30 TPS logic accumulator, GameLogic, RNG, replay, pathfinding, AI, balance, drawable interpolation, unit smoothing, frame limiter, swap chain. A short experiment with `IDXGIDevice1::SetMaximumFrameLatency(1/3)` showed no reliable effect on Present cadence (run-to-run variation was larger than the effect) and was removed; the render-pacing component (B) is **not** claimed fixed - see "Remaining".

## Validation

* Gtest `PresentationScroll.*` (6 tests): equal distance at 30/60/120/144/240 FPS steady and jittered (velocity x elapsed, 1e-4); seen speed cv < 0.001 for the new model on a jittered 240 FPS trace versus > 0.15 for the former model; first update / stall / backwards counter never jump; release does not overshoot. Full `g_googletest`: 427 passed.
* `test_camera_presentation_contract.py` (7 tests): the hand-over, the classic speed formula (8 occurrences + RMB + edge size) untouched, the application before `updateCameraTransform`, the pending clear on release, simulation files and the 30 TPS accumulator untouched, the probe inert unless requested. Python suite: 243 tests OK.
* Real game, both titles, 5 caps: the logic tick count is 90 - 91 per 3 s hold at every cap (30 TPS unchanged); distance covered by the same hold: Generals key 2020 - 2041 units at every cap (2093 - 2098 edge), Zero Hour 2223 - 2254 (3196 - 3230 edge): independent of the render rate. Win32 reference build and Win64 build compile.
* Before/after cadence above: at 240 FPS the camera speed ripple falls from cv 0.39 / 0.36 to 0.031 / 0.040 and the mean speed bias (+9% / +8%) disappears; 144: 0.089 -> 0.021 (Generals).

## Packages for the human retest (one `New-MKRaudsteinDistribution.ps1` invocation, source commit `c74890a7e`, 26/26 validator checks per title)

| Package | Files | Bytes | SHA-256 |
|---|---:|---:|---|
| `MKRaudstein-Generals-x64-Preview-c74890a7e.zip` | 14 | 4,019,966 | `75EAD59F9A6407E07A568E4079D44C383397F0945E32B40A07E812F2616F569C` |
| `MKRaudstein-ZeroHour-x64-Preview-c74890a7e.zip` | 8 | 4,124,631 | `26F0550938BDDAE3D314C10CF33F1B20CE1897A40D581132E4D98401D656C8A7` |
| Generals Symbols ZIP | 1 PDB | - | `2525F36DAFDD56ADFF58345C365015953933B3093599458D25959395EF79A0D0` |
| Zero Hour Symbols ZIP | 1 PDB | - | `E442473AC4E96CBAEA4C48F8FF2D07D7B4D56471CBF57ADBBF1E839FBE167E8D` |

Staged candidates for a developer-side run: Generals `build\dev-runtimes
ative\Generals\dist-c74890a7e-gen\game\generalsv.exe`, Zero Hour `build\dev-runtimes
ative\ZeroHour\dist-c74890a7e-zh\game\generalszh.exe` (launcher default: `Run-ZeroHourCandidate.cmd`, equivalent `-Name dist-c74890a7e-zh`). Evidence: `evidence/x64-49/distribution/`.

## Remaining (not fixed here, not claimed fixed)

* Render/Present pacing at the top rates (B): frames are handed over irregularly (p95 11 - 12.5 ms against a 4.17 ms budget) so the eye still sees uneven frame spacing even though the camera now moves exactly by the time between frames. The seen speed measured by *present* intervals is still uneven at 240 FPS. A pacing / frame-latency milestone (swap-chain model, waitable swap chain, GPU-bound cap) is the next lever; it is a renderer change and out of this milestone's scope.
* The probe cannot observe the monitor's real scan-out; human retest remains the final judge.

## Human retest procedure

Install the new Preview ZIP(s) over Steam Generals / Zero Hour as before (they are produced by `New-MKRaudsteinDistribution.ps1`, hashes in the final report). In a Skirmish at your usual 240 FPS setting: (1) hold the Left/Right/Up/Down arrow keys for 3 - 5 s across open ground and watch for stutter; (2) repeat with the mouse at the screen edge; (3) repeat at 144 and 60 FPS in Modern Settings; (4) release the key: the camera must stop at once (no creep); (5) verify the usual game speed and that units are not slower. Report: smoother / same / worse at 240, 144, 60 for keys and for the edge, plus anything that overshoots or jumps.

## Reproduce

```
pwsh scripts/native-runtime/Stage-NativeOrdinaryPlay.ps1 -Title Generals -Name <name> -AssetSource <ZH_Generals>   # stage the current build
pwsh scripts/native-runtime/Invoke-CameraProbe.ps1 -Title Generals -Candidate <name> -Label after                    # -Legacy -Label before for the former step
python scripts/native-runtime/analyze_camera_probe.py build/diag
```
