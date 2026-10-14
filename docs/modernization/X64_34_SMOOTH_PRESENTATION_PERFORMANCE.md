# X64.34 Smooth presentation completion + performance instrumentation

Baseline: `6dc40760d` (X64.33 accepted and closed). Branch `dev/modern-engine`. Status: **READY FOR NATIVE REVIEW,
not accepted.** No native run was performed by the agent.

## 1. Repository state reviewed
Clean tree at `6dc40760d`, remote `origin` only (no upstream push). `AGENT_HANDOFF.md` and the Stage 2-4 documents
exist and were treated as source of truth; no other agent's uncommitted work was present. This milestone extends the
existing systems; it does not recreate them.

## 2. Audit: what already existed (verified from code)

| System | Where | Status before X64.34 |
|---|---|---|
| Authoritative 30 TPS logic | `GameEngine::update` accumulator, `FramePacer` | COMPLETE (render independent of logic) |
| Presentation clock (previous/current generation, alpha, epoch, validity) | `PresentationClock.h`, `FramePacer` | COMPLETE (Stage 3C.1) |
| FPS presets 30/60/120/144/165/240 + uncapped, limiter | `FrameRateLimit`, `FramePacer`, Options | COMPLETE; Sleep + 2 ms spin, `timeBeginPeriod(1)` |
| Ground-unit translation + root heading interpolation | `GroundTranslationHistory`, `Drawable` | COMPLETE for ordinary ground units; opt-in (Unit Motion: Smooth) |
| Physics decoration (pitch/roll/Z/yaw wobble) | `Drawable::applyPhysicsXform` | COMPLETE (already phase-interpolated) |
| Skeletal animation | WW3D fractional time | COMPLETE (time based, raw-bone lerp/slerp) |
| Particles/effects | `ParticleSystemManager`, `hasUpdated` gating | COMPLETE (delta-of-phase integration) |
| User camera (keyboard/edge/RMB/zoom) | `LookAtXlat`, `View::userScrollBy` | PRESENTATION-BOUND, rate invariant (`fpsRatio = 30 x min(dt, 0.2)`); no 30 Hz stepping |
| Scripted/follow camera, shake | `W3DView` | PARTIAL (follow has per-update 0.1 smoothing; shake deliberately steps) - NEEDS NATIVE VISUAL VALIDATION |
| Aircraft root, turrets, projectiles | - | NOT IMPLEMENTED (draw current canonical transform at 30 Hz) |
| Water / shroud / scorches / tracks | native world layers | PRESENTATION-BOUND (drawn per frame from client state; water scroll uses WW sync time) - NEEDS NATIVE VISUAL VALIDATION |
| UI animation | `ProcessAnimateWindow`, transitions | PRESENTATION-BOUND (render-time scaled) |
| Audio | NativeMiles | independent of frame rate |
| Frame/phase profiler (frame, categories, percentiles, counters, path deep profile) | `PerformanceProfile`, `-performanceProfile` | COMPLETE for CPU categories (Stage 4A.x); no renderer/world counters |

## 3. What X64.34 changed

### 3.1 Smooth Unit Motion now covers the classes that visibly step at 30 Hz
* **Aircraft** (`KINDOF_AIRCRAFT`): root translation and shortest-path heading, own airborne policy (no ground
  locomotor/AI-state rules). Per-tick step limit 60 units; larger = relocation = snap.
* **Projectiles** (`KINDOF_PROJECTILE`): root translation + heading, step limit 120 units. Bespoke effect
  draw modules (projectile streams, lasers) are not modules that support root translation, so they stay canonical.
* **Turret yaw / barrel pitch**: `AnglePairHistory` per turret slot in `W3DModelDraw`, shortest-path yaw, linear
  pitch, between the previous/current completed samples. Per-tick implausibility limits (1.0 rad yaw, 0.6 rad pitch)
  show the canonical value instead of sweeping.
* Both use the existing Smooth gate (`GroundTranslation::isEnabled()`); Original remains the reference. Default
  stays OFF until native acceptance.

### 3.2 Safety rules (all presentation-only, never read by simulation)
* Interpolates only between consecutive completed generations of one continuous epoch; alpha clamped to [0, 1]; no
  extrapolation.
* Snap (canonical shown): first sample, spawn, generation gap (undrawn/culled/paused), epoch change (load, map change,
  reset), invalid timing (pause, menu, stall), non-finite values, step above the class limit (teleport/relocation),
  contained/disabled/parachuting/hidden/dead, scripted camera.
* A stall that makes alpha huge cannot push past the current sample (clamped); pause/load return canonical.
* The authoritative Matrix3D, AI state and weapon/bone queries are unchanged (pristine bone queries stay canonical).

### 3.3 Camera, animation, effects, water/shroud
Audited, no code change: keyboard/edge/zoom camera are measured-delta integrators (smooth at any rate, equal speed);
animation and particles are time based; water/shroud/scorches/tracks are drawn per frame from client state. Items that
need a native visual judgement are listed in section 8.

### 3.4 Performance instrumentation (extends the existing Stage 4A recorder)
* New per-frame counters: `draw_calls`, `textures_created`, `buffers_created` (deltas of the native D3D11 backend's
  cumulative counters), `drawables_drawn`, and two gauges `particles_live_gauge`, `objects_live_gauge`
  (sampled once per outer frame; gauges are levels, not rates). Sampling is free when no capture is running.
* New **Generals Performance Receipt** block appended to the capture `-summary.txt`:

```
=== Generals Performance Receipt ===
duration_s: 60  frames: 8642  avg_fps: 144
frame_ms: avg 6.94 p95 7.31 p99 8.04 max 12.7
logic_ms (per frame with a tick): avg .. p95 .. p99 .. max ..
logic_ticks: 1800 catch_up_frames: 0 long_logic_frames(>33.3ms): 0
render: avg_draw_calls .. peak_draw_calls .. avg_drawables_drawn .. textures_created .. buffers_created ..
world: avg_objects .. peak_objects .. avg_particles .. peak_particles ..
interpolation(smooth): 1
=== end receipt ===
```
* CPU wall time including pacer waits; **GPU time is not measured**; counters are not "invented" (every number comes
  from an existing cumulative counter or a direct count).
* Usage (existing, bounded to 60 s per capture): create an absolute output directory, launch with
  `-performanceProfile <that directory>` (optionally `-groundInterpolation` is NOT needed: Unit Motion drives it),
  then write `start <label>` (and later `stop <label>`) into `<directory>\command.txt`. Reports: `*-summary.txt` (with
  the receipt), `*-frames.csv`, `*-categories.csv`, `*-slow.csv`.

## 4. Determinism evidence (and its limits)
Automated (`PresentationSmoothnessTest`): for render rates 30/60/120/144/165/240/1000 and Smooth ON/OFF, the
authoritative position sequence of 120 ticks is bit-identical; presented motion is monotonic, never ahead of the
authoritative position, at most one tick behind, and offers at least as many distinct positions at higher rates.
Source contracts: interpolation state lives in the Drawable/DrawModule (client), is not serialized or CRC'd, and no
authoritative setter was added or called. **Not proven**: multiplayer lockstep/replay with Smooth ON (needs replay/CRC
and LAN validation); visual correctness (native).

## 5. Original vs Smooth
Original: authoritative transforms drawn as before (the accepted retail-compatible presentation). Smooth: everything in
3.1 plus the existing ground translation/heading, physics decoration and measured-delta camera. Smooth stays opt-in
(`Options > Gameplay > Unit Motion`, key `SmoothUnitMotion`, or `-groundInterpolation`).

## 6. Benchmark methodology (for X64.35)
Scenarios (same map/seed/build, Smooth fixed, capture 60 s each, 3 repeats, compare medians):
A LIGHT: early Skirmish, ~20 units. B MEDIUM: developed bases, active combat. C HEAVY: large battle with many
units/effects. D EXTREME: diagnostic stress (optional). Record cap/VSync/resolution/UI scale and the receipt. Existing
pathfinding scenario (`-performanceScenario pathfinding-heavy`) remains the deterministic CPU workload. No benchmark
number is claimed in this document; native numbers belong to the human run.

## 7. Tests
`AnglePairHistory.*` (interpolation, wrap, snap, refusal, clamp), `GroundTranslationHistory.AirborneStepLimits...`,
`PresentationSmoothness.*` (render-rate independence, equivalence, stall/pause), `PerformanceReceipt.*` (format,
counters, catch-up/spike detection, unique names). Existing `GroundTranslation*`, `PerformanceProfiler.*`, FramePacer
and X64.33 display/Options/HUD tests remain.

## 8. Native-review requirements / known limitations
Smooth ON vs OFF on: camera (edge/keys/diagonal/start-stop/pause), infantry and vehicles (turns, groups), tank turrets
tracking while the body turns, fixed-wing and helicopters (banking, attack runs, takeoff/landing), missiles/shells,
large battle, game-speed sanity at 60/120/144+, pause/unpause and save/load (no catch-up flight), plus one receipt.
Known: an interpolated projectile/aircraft root lags the authoritative position by up to one tick (by design);
attached particle/laser starts follow rendered bones; helicopter rotors and decorative banking were already
time-based; follow/shake camera stays partial.

## 9. Deferred (X64.35 candidates, evidence first)
Renderer state-change/texture-bind counters (needs per-draw hooks), GPU timing queries, shadow-pass counters, culled
object counts, instancing/batching, pathfinding hot paths (existing Stage 4B data), allocation counters.

## 10. Candidate
`build/dev-runtimes/native/Generals/x64-34-smooth-presentation` (staged and preflighted, not run), built from `99078f585`:
* `generalsv.exe` SHA256 `64EB4D7250B42F0723DB8E24F60906AEE62B0B00A0C794B27EF91366CBD0C297`
* `generalsv.pdb` SHA256 `C2BCEA54DBB3F2D7845622843C6878C59CD39A4A9A0AD36CEF3492B057B28E20`
Steam untouched. X64.34 is NOT accepted before the human native review.

## 11. X64.34 ACCEPTED AND CLOSED (2026-10-15)

Native review of `x64-34-smooth-presentation` (generalsv.exe SHA256
`64EB4D7250B42F0723DB8E24F60906AEE62B0B00A0C794B27EF91366CBD0C297`): Smooth is visibly much smoother, the presentation
looks good, and no notable visual or gameplay-speed regressions were found.

**Accepted:** Smooth presentation (ground units, turrets/barrels, aircraft, projectiles; camera unchanged) as an
opt-in Modern Experience feature; presentation-only interpolation with the documented snap/reset rules.

**Performance instrumentation:** implemented, unit-tested and native-ready (counters + Generals Performance Receipt).
The first *baseline receipt* needs a native run, which is a human gate the agent may not perform; it is therefore
recorded as **the first X64.35 task** (scenarios A-C of section 6, 60 s each, 3 repeats, Smooth fixed, recorded with
cap/VSync/resolution/UI scale). No benchmark number is claimed here.

**Not yet proven:** multiplayer/replay determinism with Smooth enabled (needs replay/CRC and LAN validation).
Smooth remains opt-in; promoting it to a default is a separate decision.

### Future Modern Experience design direction (recorded, NOT implemented)
The centred HUD-island architecture (X64.33) is retained. A later Modern HUD pass may rebuild/redesign the control
panel for modern displays while preserving the recognizable Generals visual identity. Classic remains the
preservation path. The HUD must not be stretched across ultrawide displays simply to fill horizontal space.
