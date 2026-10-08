# Stage 3C.1 — scheduler-aligned presentation clock

2026-10-08. Started clean on `dev/modern-engine` at `bc87b4954 Document visual
interpolation architecture`. Stage 3A/3B were reviewed, committed and pushed by
the developer. This stage implements timing only; no world interpolation or
controls. No game launch, Install, commit or push.

## 1. Reverified scheduler ordering

Both title GameEngine.cpp variants have the same relevant ordering:

```text
GameEngine::execute
  GameEngine::update
    VERIFY_CRC block: Radar, Audio, GameClient (including render), messages, Network
    canUpdateGameLogic(IgnoreFrozenTime)
      default presentation observation = unavailable (period 0)
      GameLogic::preUpdate clears hasUpdated; updates pause state
      refresh FramePacer frozen/halted flags
      network: existing frame-readiness decision, no offline accumulation
      offline:
        rate <= 0: no acceptance
        TiVO/immediate branch: existing acceptance, no accumulation
        accumulator branch:
          T = 1 / actual logic rate
          accumulator += min(previous measured delta, T)
          if accumulator >= T: subtract T once; accept one tick
          observe actual post-decision accumulator and T (both accept/wait)
    if accepted:
      GameLogic::update
        scripts can run and return while frozen, without completing world state
        completed world update increments m_frame and sets hasUpdated
      GameClient::step unless frozen
  FramePacer::update
    existing limiter wait captures next measured delta
    existing legacy phase reset/advance (unchanged)
    new observer combines scheduler observation, completion, delta and eligibility
  next GameClient/render pass can read the published PresentationTiming
```

Read-only copies are added beside the existing scheduler subtraction; none of
its comparisons, arithmetic, branches, return values or readiness calls change.
Neither world update nor client step moves. The normal scheduler can accept at
most one tick per outer iteration and retains its existing delta clamp.

## 2. Explicit timing API

`Core/GameEngine/Include/Common/PresentationClock.h` contains a deterministic,
header-only observer and the small `PresentationTiming` structure:

| Field | Meaning |
|---|---|
| previousGeneration | Previous completed GameLogic frame in a valid pair |
| generation | Current completed GameLogic frame; invalid timing reports the latest observed frame |
| epoch | Presentation continuity identifier; compare for equality, not elapsed simulation time |
| alpha | Normalized previous/current sample weight, clamped to [0,1] |
| valid | A consecutive completed pair and trustworthy normal offline accumulator timing exist |

FramePacer owns the observer. `getPresentationTiming()` returns a const reference;
consumers must copy it if retaining it beyond the current client pass.
`observePresentationScheduler(remainder, period)` is the scheduler-facing input,
not a consumer-controlled timer. `resetPresentationTiming()` is the lifecycle
invalidation API. There are no production consumers of the new alpha yet.

`period=0` explicitly marks an unsupported scheduler branch. Each observation is
consumed once by the subsequent FramePacer update. Missing observations fail
closed rather than reusing an old scheduler remainder.

## 3. Alpha semantics and latency

For a valid pair `(previousGeneration, generation)`:

- alpha 0 means the previous completed sample.
- alpha 1 means the current completed sample.
- intermediate alpha means interpolate only between those two samples.

For normal offline accumulator timing:

```text
R = actual accumulator after the last scheduler decision/subtraction
T = observed logic period
D = subsequent measured FramePacer update duration
alpha = min(1, (R + D) / T)
```

This ordering matters: the upcoming client draw occurs before the next scheduler
decision consumes D. R already contains the earlier consumed deltas; adding D
once observes the look-ahead needed by that client pass. The observer does not
accumulate its own elapsed time, consume R or subtract T itself.

Accepted completion advances the sample IDs and retains R, rather than resetting
alpha to D/T. If the next acceptance is due but has not happened yet, alpha
saturates at 1; there is no extrapolation into an unavailable state.

Previous/current interpolation normally presents approximately one logic tick
behind simulation time (about 33.3 ms at 30 TPS), with possible additional hold at
saturation. This is a deliberate future presentation tradeoff, not a latency fix.

**Invalid timing means render canonical current state.** Alpha is 1 in that
case; consumers must not apply it to an old cached pair. Startup and recovery need
two fresh consecutive completed observations before a valid pair is published.

## 4. Completed generation semantics

GameLogic's existing frame number plus `hasUpdated()` identifies a completed
world update, not merely permission to invoke update. Frozen script-only calls
do not prime a pair. The initial sample is invalid for interpolation; the next
consecutive completion supplies its predecessor.

Repeated completion of the same frame, skipped/nonconsecutive completion,
unannounced frame changes, rewind and unsigned frame wrap invalidate continuity.
A completed jump may prime the new first sample, but cannot publish a valid pair.
Future caches must compare both IDs and epoch; drawing multiple views cannot
advance sample generation.

## 5. Epoch and reset semantics

The observer begins at epoch 1. Explicit reset increments epoch and clears its
pair and pending observation. Loss of a continuous timing interval increments
epoch once; continued invalid/paused frames do not churn it. Period changes also
invalidate continuity. Epoch is a presentation-only unsigned counter, not a
globally unique persistent ID, save field, CRC input or network frame number.

Both titles call resetPresentationTiming at GameLogic::reset and
GameLogic::loadPostProcess. This catches load-to-the-same-frame as well as rewind.
FramePacer::reset also invalidates the new clock while preserving all its old
limiter/delta/legacy-phase operations. Startup/reset can occur inside a logic
update; clearing the pending observation then makes that interval invalid.

**The engine accumulator is not reset by this work.** It was separately retained
in the existing engine. After a map/load transition, new pairs observe whatever
remainder the unchanged scheduler actually uses. Do not discard that remainder
in a presentation helper or change scheduling to make tests simpler.

Object lifetimes are outside this clock: Stage 3C.2 still needs per-object identity,
creation/destruction/teleport/containment invalidation even within one timing epoch.

## 6. Offline behavior

Valid timing is limited to normal 30-TPS, unpaused, unfrozen, unhalted offline
play using the actual accumulator branch, normal script speed and tactical time
multiplier 1. Requested render FPS is not an alpha input.

At caps above 30 the existing branch supplies the actual remainder. Uncapped
rendering can also qualify if it uses that branch and achieved deltas remain
trustworthy. Explicit altered logic speeds/legacy scaling are conservatively
unsupported. No FPS Options or speed-control behavior changes.

At a 30 cap the existing scheduler normally accepts immediately and does not use
the accumulator. Stage 3C.1 explicitly publishes invalid/snap timing there:
alpha 1, latest generation, no intermediate presentation. This preserves the
30-FPS reference instead of inventing a nonexistent scheduler remainder. A
subsequent switch to accumulator pacing must re-prime its pair.

## 7. Network behavior

Network scheduling continues to use isFrameDataReady and its existing deadlines.
No network remainder is invented. Whether a frame was accepted, is still waiting,
or arrived in a burst, the new clock is invalid/snap with the latest completed
frame. Network arrival cadence does not advance alpha or extrapolate Objects.
Leaving a valid offline interval invalidates its epoch. Returning offline requires
a new completed pair. Network interpolation is deliberately unavailable here.

## 8. Pause and freeze

Pause, time freeze and halt make timing invalid/snap. Script-only updates while
frozen do not count as world samples. Resume preserves existing scheduling and
requires two new completed samples; it never reuses the pre-pause pair. The
FramePacer eligibility check also reads current GameLogic pause state after the
world update, so a pause set during that update cannot leave valid timing.

## 9. Stalls, low FPS and unavailable rendering

A measured delta >= T invalidates timing rather than extrapolating through a
long stall or achieved FPS <=30. Negative/NaN/nonfinite deltas and malformed
remainder/period observations cannot publish valid timing. Re-prime on recovery.
The existing scheduler still clamps its own input delta and accepts at most one
tick; no catch-up loop or changed low-FPS simulation behavior is introduced.

Minimize, headless and device-unavailable paths can skip rendering while engine
updates continue. The clock has no renderer dependency and continues observing
actual world completions/deltas; a draw call is never required. A stall/reset
invalidates as above. Merely losing/restoring the device does not change the
scheduler's time domain, so no new renderer/device hooks are added.

Stage 3C.2 must capture samples independent of visibility, or reject a missed
generation pair on restore. Valid timing alone cannot certify a cache that skipped
sampling while minimized or after device-resource recreation.

## 10. Fast-mode behavior

TiVO immediate acceptance supplies period 0. Script-fast, altered logic rates,
legacy scaling and tactical time multipliers !=1 fail eligibility. Timing is
invalid/snap through these paths, regardless of cap bypass or skipped display
updates. Returning to normal requires fresh completed samples. The observer never
changes the limiter's existing fast-mode checks or network/replay command timing.
Normal offline replay can use the same clock; replay loading resets its epoch.

## 11. Legacy phase preservation

getLogicFramePhase's getter, existing reset value and update/reset arithmetic are
unchanged. Particle and physics-decoration call sites are untouched. The new
observer runs after that existing phase block; it has separate state and API.
The added FramePacer test verifies observer/reset calls leave the legacy phase,
elapsed delta, target rate and scaling enablement unchanged. It does not claim
a visual regression test of particles or skeletons.

## 12. Deterministic test design

Tests were written and the first 15 clock tests passed before engine integration.
PresentationClockTest uses synthetic float scheduler inputs with the existing
one-subtraction/clamped-delta policy; production still reads the real engine
accumulator. No Sleep/QPC dependence or engine startup is needed for the helper.

For every valid sample, tests compare represented presentation time with an
independent double elapsed-time oracle, capped at the latest completed state:

```text
represented = (previousGeneration + alpha) * T
expected = min(totalConsumedTime + nextDelta - T, generation * T)
```

This checks sample IDs, remainder alignment and accumulated time, not only alpha
bounds. Stable runs last 120 synthetic seconds at every cap. Jitter uses a
repeating unequal-delta sequence at all five higher caps. Tests cover explicit
144/165 residuals, reset/same-frame restart, pause/freeze/halt/fast eligibility,
network accept/wait/arrival, >=T stalls, recovery, missing observations, generation
jumps, repeated completions, frame wrap, skipped drawing, saturation and malformed
numeric inputs. They do not execute an actual multiplayer session or full map load.

Final suite adds 16 clock cases and one FramePacer isolation case to the previous
11 tests per title: **28 per title, 56 total**. Both CTest executables pass.

## 13. Results at all six render rates

These are synthetic cadence results, not measured game FPS/TPS. Both title suites
run the same assertions. Google Test XML records the stable cadence metrics.

| Render FPS | Iterations (120 s) | Completed frames | Acceptance gap (iterations) | Presentation result | Maximum elapsed-oracle error |
|---|---|---|---|---|---|
| 30 | 3,600 | 3,600 | 1 | PASS: explicit invalid/snap, alpha 1 | Not applicable |
| 60 | 7,200 | 3,600 | 2 | PASS: aligned completed pairs | <1 ns reported |
| 120 | 14,400 | 3,600 | 4 | PASS: aligned completed pairs | <1 ns reported |
| 144 | 17,280 | 3,599 | 4–5 | PASS: residual retained | 8.042 microseconds |
| 165 | 19,800 | 3,599 | 5–6 | PASS: residual retained | 3.348 microseconds |
| 240 | 28,800 | 3,600 | 8 | PASS: aligned completed pairs | 1 ns reported |

Nanosecond metrics truncate to integer nanoseconds. At 144/165 the finite test
endpoint lands just before the last acceptance under existing float arithmetic;
the retained remainder remains for the next decision. Tests allow the existing
one-tick boundary difference, not a changed scheduler policy. Time alignment error
stays below the asserted 100-microsecond tolerance over two minutes; this is not a
claim of mathematically zero rounding error or unlimited-duration drift immunity.

Commands used in x86 Visual Studio Developer PowerShell:

```powershell
cmake --preset win32
cmake --build --preset win32
ctest --preset win32 --output-on-failure
git diff --check
```

Configure and Release build pass (MSVC 19.51.36260.0, X86), both titles/tools.
Final CTest: 2/2 executables, 56/56 Google cases. The initial integrated build
reports 82 warnings in unchanged lines: C4018 (33), C4267 (16), C4722 (1), C4996
(2), C5055 (30). Reviewed examples include signedness/conversion/deprecated enum
arithmetic, obsolete MFC Enable3dControls and WorldBuilder's nonreturning
destructor. No warning points to new timing code; no unrelated warning fixes or
LF/CRLF policy changes. Final incremental test rebuild has no compiler warnings.
Configure also reports the known missing Generals registry/install discovery and
modern-MSVC retail CRC limitation; no registry edit or Install was attempted.
Build/test logs and XML remain ignored under build.

## 14. Non-integer-ratio findings

No production code special-cases 144 or 165. Acceptance spacing emerges from the
unchanged float accumulator; the observer copies its residual after subtraction.
The dedicated completed-pair check verifies R is included on the very next draw:
after five 144-FPS deltas, alpha is approximately 6/24 instead of a reset to 5/24;
after six 165-FPS deltas, approximately 3/11 instead of 2/11. Pair warmup is
separate from that residual calculation. Jitter is handled by supplied deltas,
without any assumed 4/5 or 5/6 pattern or modulo counter.

## 15. Limitations and safety review

No new world-transform caches, Object/Drawable writes, draw-module changes,
controls, AI/physics/pathfinding/RNG changes, network messages or save/replay
format changes. FramePacer has no snapshot/CRC traversal for the new member;
GameLogic reset/load hooks only invalidate presentation state. Simulation remains
targeted at 30 TPS under the existing policy and completion ordering.

The new API has no visual consumer or permanent logging. A new runtime candidate
was not staged: an unused timing API offers no visual acceptance result, and all
existing development runtimes are retained. No game or user-data session ran.
Final read-only Test-ZHRuntime checks PASS for baseline, stage2-timing and
stage3a-fps-options: each matches all 369 inventory paths, lengths and SHA256
values. Those checks establish file integrity only, not runtime acceptance.
Runtime FPS/TPS, CRC/save/replay/LAN behavior, legacy particle visuals and actual
pause/device transitions remain unverified. Modern-MSVC retail compatibility
remains unestablished as described in COMPATIBILITY.md.

## 16. Stage 3C.2 prerequisites

Review the clock's invalid/snap policy, one-tick latency and conservative 30-cap/
network exclusions first. Then scope a gated translation-only prototype for a
small eligible uncontained ground-unit subset. Capture final canonical samples
once per completed generation before view/bone work, including off-screen units;
require matching epoch, generation pair and object lifetime. Define spawn,
destroy, load, teleport and containment snaps before rendering interpolation.

Keep canonical Drawable/Object transforms intact; compose only a transient local
matrix. Audit rendered picking, pristine bone/weapon queries, markers and attached
effects. Preserve legacy particle/decorative clocks. Build/focused tests plus
guarded developer-only FPS/TPS/visual/CRC/save/replay/LAN acceptance are required
before enabling any prototype. Network smoothing, rotation/turrets, aircraft,
projectiles and WASD/rebinding remain separate work. Stop here for review.
