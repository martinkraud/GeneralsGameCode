# Incremental modernization roadmap

Stage 2 update (2026-10-08): the developer's guarded baseline passes startup,
menus, ordinary skirmish and clean exit, with 369-file source/runtime hash
checks before/after. Raising the render cap manually accelerates offline play.
The first narrow correction enables the existing 30-TPS logic scale by default;
no scheduler rewrite is involved. [STAGE2_TIMING.md](STAGE2_TIMING.md) tracks the
candidate and required manual 30-versus-60 acceptance. Higher caps, replay/LAN,
full Stage 1 scenario coverage and performance characterization remain open.

Stage 1 update, 2026-10-08: guarded staging/launch/backup/integrity helpers and
synthetic infrastructure checks are implemented under `scripts/zh-runtime`.
[STAGE1_WORKFLOW.md](STAGE1_WORKFLOW.md) documents exact manual launch, baseline,
profiling and Stage 2 preparation. This historical update preceded the developer's
manual runtime session and Stage 2 candidate above. Stage 1 measurement work is
not complete; the broader scenario/profile/compatibility tests remain open.

Revision `adac468d7`, 2026-10-07. Research order follows current source evidence:
existing render/logic separation must be evaluated before a new decoupling
prototype. Stage descriptions below are the plan; current status is recorded above.

## Stage 0 — Freeze the evidence baseline

- **Objective:** preserve source revision, Git safety, compiler/cache settings and
  known successful x86 build; distinguish developer reports from runtime tests.
- **Benefit:** future results can be reproduced and correctly attributed.
- **Risk:** mistaking a build for retail compatibility or a badge for x64 support.
- **Compatibility:** no behavior change; preserve retail guards.
- **Tests:** verify branch/remotes, clean initial tree, binaries/compiler metadata;
  establish optimized VC6 reference separately when retail checks are needed.
- **Rollback:** retain this revision and original artifacts; no source changes yet.

## Stage 1 — Disposable runtime and reproducible profiling baseline

- **Objective:** implement an opt-in staging/launch workflow using a full asset
  copy outside Steam, explicit CWD and a manifest. Isolate user data with a test
  account or first design a reversible path override in a separate change.
- **Benefit:** safe debugging and meaningful measurements without modifying Steam.
- **Risk:** archive/DLL provenance, legacy registry key, shared user data and
  existing app-local D3D wrapper can contaminate results.
- **Compatibility:** no engine/data-format change for staging; keep baseline
  assets/settings and explicitly record registry dependencies.
- **Tests:** destination guard/no links to Steam; file hashes; DLL import inventory;
  launch, skirmish/campaign, audio/video, save/load, mod load; modern same-build
  replay checks and separate retail VC6 replay checks. Capture Tracy Release
  profile plus uninstrumented reference on identical scenarios, including frame
  distributions and ticks/second. Include user-data backup/restore verification.
- **Rollback:** discard only the verified disposable runtime; restore backed-up
  test user data. Keep Steam and existing build cache untouched.

**Implemented infrastructure:** a small PowerShell staging/launch
helper with a destination guard, file manifest and explicit CWD, plus a repeatable
scenario checklist. It must not invoke the cached install target, edit registry,
copy over Steam or change timing. Decide user-data isolation before launching.

## Stage 2 — Characterize existing FPS and display behavior

- **Objective:** correct the verified offline speed defect by enabling the
  existing 30-TPS scale by default, then exercise FramePacer, network
  pacing, interpolation, presentation interval, resolutions and window modes.
- **Benefit:** identifies actual gaps and avoids duplicating upstream work.
- **Risk:** raising FPS alone speeds offline simulation; profiler/wrapper effects
  and Present waits can obscure findings.
- **Compatibility:** no tick-unit/order/network-path changes; offline wall-clock
  pacing changes above 30. Same-build replay/live input need separate validation.
- **Tests:** 30/60/120/144/240 rendering with explicit 30-tick policy; non-integer
  ratios; pause, load, fast-forward, scripted time freeze, minimized/device-loss
  cases; fixed camera/replays; CRCs and tick counts; Alt-Tab and DPI tests.
- **Rollback:** return test settings/runtime to baseline; disable diagnostics.

## Stage 3 — Close measured scheduling/interpolation gaps

- **Objective:** define explicit normal-speed policy and fix one demonstrated
  visual/timing gap per change using existing FramePacer/phase infrastructure.
- **Benefit:** smoother high-refresh presentation with stable gameplay speed.
- **Risk:** command-to-frame assignment, update ordering, logic callbacks,
  spawn/teleport transitions and catch-up policy can affect simulation.
- **Compatibility:** preserve fixed logic units and default/legacy behavior;
  any policy change requires documented replay/multiplayer analysis first.
- **Tests:** frame-indexed CRC equivalence across caps; live input tests;
  transforms/animation/particles/attachments/camera; speed/pause/load/stall cases;
  long LAN sessions with mixed caps and existing replay corpus.
- **Rollback:** opt-in feature flag and focused revert; retain legacy scheduler
  and unmodified serialized state. No render threading in the first prototype.

## Stage 4 — High-refresh presentation and frame pacing

- **Objective:** tune only demonstrated limiter/Present issues and complete
  render-rate-independent visual updates for 120/144/165/240 Hz scenarios.
- **Benefit:** lower jitter/latency and consistent animation/input feel.
- **Risk:** sleep/spin tradeoffs, GPU/CPU saturation, clock handling and wrapper
  behavior; average FPS can hide poor tail latency.
- **Compatibility:** presentation-only changes must not change logic state/rate.
- **Tests:** p95/p99 pacing, CPU power/utilization, input latency proxy, VSync modes,
  low-FPS fallback, different refresh displays, CRC tests from Stage 3.
- **Rollback:** independent cap/pacing options with known baseline defaults.

## Stage 5 — Ultrawide, borderless and UI scaling

- **Objective:** extend current arbitrary-aspect support with tested projection,
  layout, scaling and desktop-window behavior; preserve mod UI compatibility.
- **Benefit:** usable 21:9/32:9/5120x1440 and high-DPI displays.
- **Risk:** picking/cursor/clipping, FOV/map bounds, stretched assets, device resets.
- **Compatibility:** no asset-format/INI semantic changes; optional layout policy.
- **Tests:** 4:3/16:9/21:9/32:9, multiple DPI scales, menu/HUD/tooltips/text,
  mod WND layouts, selection/placement, Alt-Tab/multi-monitor/minimize/recovery.
- **Rollback:** switch to legacy window/layout/FOV settings; separate commits.

## Stage 6 — Measured CPU improvements

- **Objective:** optimize only top measured costs, one subsystem at a time;
  consider safe client-side concurrency only after ownership analysis.
- **Benefit:** better CPU frame time and practical scene/unit capacity.
- **Risk:** ordering/precision changes, synchronization and memory ownership.
- **Compatibility:** deterministic output/order preserved; simulation threading
  requires a separate design and stronger tests.
- **Tests:** repeated identical traces/microbenchmarks, full replay CRCs, long
  matches, allocation/lifetime checks and non-profiled throughput confirmation.
- **Rollback:** focused revert or feature switch with reference implementation.

## Stage 7 — Memory and asset limits; separate x64 feasibility

- **Objective:** measure address-space/allocator/asset constraints before raising
  limits; separately scope native x64 graphics/audio/video and ABI work.
- **Benefit:** stability and larger practical content where evidence supports it.
- **Risk:** overflow, file/wire widths, CRC ordering, platform dependencies.
- **Compatibility:** changing architecture does not grant format compatibility;
  preserve field sizes or explicitly version changes before implementation.
- **Tests:** representative large mods/maps, memory pressure/leaks, save/load,
  old assets, x86 reference comparisons; x64 CI/runtime only if a viable design
  is implemented later.
- **Rollback:** preserve x86 runtime and original limits; independent branch and
  versioned feature choices. Do not start an x64 port to explain the badge.

## Stage 8 — AI/pathfinding scalability if profiling justifies it

- **Objective:** reduce demonstrated AI/path/spatial cost while preserving
  decisions, ordering, budgets and movement behavior.
- **Benefit:** larger practical battles/maps without simulation slowdown.
- **Risk:** very high determinism/gameplay sensitivity; even allocation-order
  changes can matter (existing retail guards acknowledge this).
- **Compatibility:** prioritize equivalent algorithms; document any deliberate
  behavioral change as a separate compatibility mode.
- **Tests:** adversarial terrain/crowds, path outcomes and frame-indexed CRCs,
  seeds/mods, long multiplayer, failure cases and benchmark confidence intervals.
- **Rollback:** retain baseline algorithm behind independent selection or revert.

## Stage 9 — Renderer modernization feasibility

- **Objective:** inventory IRenderBackend coverage and remaining DX8/D3DX types,
  fixed-function assumptions, effects/assets and tools before selecting a path.
- **Benefit:** informed options for graphics/API portability and future features.
- **Risk:** enormous visual/tool/content surface and performance regressions.
- **Compatibility:** no replacement until asset/mod fidelity and fallbacks have
  explicit acceptance criteria.
- **Tests:** image comparisons, shaders/materials/water/shadows/particles,
  mods/tools, device recovery and performance on representative hardware.
- **Rollback:** retain DX8 backend and old runtime; isolate experimental backend.

## Stage 10 — Multiplayer infrastructure research

- **Objective:** map lobby/services/transport and their current availability,
  then isolate service work from deterministic command protocol changes.
- **Benefit:** maintainable connectivity and modern service operations.
- **Risk:** security, latency, peer compatibility, replay semantics and external
  service dependencies; present service availability remains UNKNOWN.
- **Compatibility:** keep existing simulation/protocol where feasible; version
  any new peer protocol and explicitly separate incompatible sessions.
- **Tests:** interoperability matrix, packet loss/latency/reordering, reconnect,
  command/seed/order validation, long matches and replay reproducibility.
- **Rollback:** original transport/protocol/service configuration retained.

Tools may use other languages later, but the engine remains C++. No C# conversion,
speculative optimization or broad rewrite is part of this roadmap's first work.
