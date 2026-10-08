# Stage 4A.2: bounded sampled path phases

## Accepted tooling: pre-commit validation (2026-10-08)

The developer accepted Stage 4A.2 and authorized one measurement-tooling commit. Every tracked diff and both untracked source/document files were inspected against baseline `854b8294d`. Only observer implementation, tests/overhead benchmark and modernization documentation are included. The Stage 4A.1 follow-up link now reflects the final selection verdict. No Stage 4B, benchmark harness or Developer Mode is implemented.

Pre-commit validation actually reran: `cmake --build build/win32 --config Release` succeeded (incremental), `ctest --test-dir build/win32 -C Release --output-on-failure` passed 2/2, and the observer-removal token audit reproduced all accepted authoritative pathfinder tokens in order. All six source/test SHA-256 values match the saved validated candidate-source hashes. Read-only source and candidate runtime inventory checks each passed 369 files; the candidate EXE/PDB hashes match the identity table below. Final capture report/command hashes are unchanged. `git diff --check` passes. No game, replay or multiplayer test was launched, and no new benchmark timing trial is claimed. The build log and all capture/runtime/analysis artifacts remain ignored and excluded from the commit. Stop after the local commit for developer review; do not push.

## Final capture analysis and selection decision (2026-10-08)

**Stage 4B selection gate resolved: select goal-directed line/movement checking in the Internal search route as the first optimization target.** The next infrastructure task is the reproducible developer scenario/benchmark harness described below, before a long series of optimization iterations. No Stage 4B implementation, harness, game launch, commit or push was performed during this analysis. Another manually constructed five-AI selection capture or WPR trace is not required.

This analysis reads every row of all five reports directly from `build/performance/stage4a2-B-final-2/20261008T213720Z-19920-1-*`, plus `command.txt`. It does not substitute a copied summary for the CSV evidence. The candidate is the unchanged Stage 4A.2 observer build, based on accepted/pushed Stage 4A/4A.1 HEAD `854b8294d10ff499ffc39b5bc40fd72f5939c39b` with dirty observer sources. The implementation/capture instructions further below describe the preceding work and are retained as history; their formerly unresolved selection state is superseded here.

### Validity and exclusions

| Check | Observed result |
| --- | --- |
| Start / process / output stem | `capture_started_utc=20261008T213636Z`, PID 19920, `20261008T213720Z-19920-1` |
| Duration / actual stop reason | 43.9108 s, `capture_capacity`; detail capacity stopped recording before the scheduled manual stop |
| Command file | Final command `stop B-final-20261008T213627242`; successful background job does not imply `manual_stop` in the report |
| Detail | `path_detail_enabled=1`, 32,768 retained records, 15 dropped |
| Sampled phases | `path_phase_enabled=1`, stride 64, `path_phase_errors=0` in summary and every retained row |
| Clock/stack | `stack_or_clock_errors=0` |
| Settings | Interpolation 1; requested/effective cap 120 in summary and every frame |
| Window | 2,557 outer frames, 1,271 completed logic ticks; approximately 58.23 outer frames/s and 28.95 ticks/s, not 120 achieved FPS |
| Complete analysis population | Exclude all 50 retained detail rows in final outer frame 2556; retain 32,718 rows in the preceding 2,556 frames |

IDs are unique and contiguous; parents precede children, exist, and agree on outer/logic frame. All frame/category statistics, slow-row inclusive/self/call values and rankings, per-kind summary statistics, and complete-frame dispatch counts were cross-checked. Frame self partitions agree within 0.0002 ms. The only incomplete detailed self partitions are records 32719 and 32766, both in excluded final frame 2556. That final frame has only 5.1075 ms logic and 0.0902 ms aggregated search time: the excluded tail does not contain a severe sample.

For every eligible complete record, own loop iterations equal inclusive HeadPops minus direct children's inclusive HeadPops; selections equal floor(own iterations/64). Line calls do not exceed selections, Neighbor calls obey the two-per-selection Internal/Closest or one-per-selection Ground boundary, and sampled insertions do not exceed actual insertions. Unsupported kinds have zero phase fields. Phase spans fit their searches, and all phase self differences are nonnegative (minimum zero). Complete Internal/Ground/Closest populations are respectively 3,194/5/71 records, with 410/3/42 containing sampled phases and 24,531/258/5,321 selected iterations. This validates coverage, not statistical randomness.

Absolute CSV offsets are printed with six significant digits and eventually have 0.1 ms precision. Parent/child offset comparisons therefore allow 0.11 ms rounding tolerance (largest observed discrepancy 0.0997 ms); duration partitions use tighter duration precision. Do not interpret rounded offsets as sub-millisecond synchronization or as clock faults.

The first three outer frames are 80.0599, 72.1091 and 69.6171 ms, accompanied by 75.2541, 66.7012 and 63.1055 ms logic work. They already contain expensive path searches, rather than a lone enormous client/device-entry event. Keep them in the raw data. Removing the first second still leaves 41 Internal searches above 10 ms with the same selection result. Focus/device state is not explicitly instrumented; wall-time preemption cannot be excluded for an individual row.

### Individual sampled phase split

The accounting formulas remain mandatory: Line self is Line inclusive minus its nested insertion; Neighbor self is Neighbor inclusive minus its own nested insertion; insertion is the sum of those two insertion fields. Neighbor already excludes its Line child. Percentages below divide these disjoint sampled durations by their sampled total. **No value is multiplied by 64 or represented as exact full-search phase time.** Full-search self is separately reported and excludes nested path records such as reconstruction/cleanup.

| Complete search cohort | Count | Full-search self sum, ms | Sampled Line self, ms (%) | Sampled Neighbor self, ms (%) | Sampled insertion, ms (%) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Internal inclusive >10 ms | 47 | 2,085.7620 | 18.8574 (50.99%) | 5.8631 (15.85%) | 12.2601 (33.15%) |
| Internal inclusive >40 ms | 31 | 1,655.5411 | 14.9974 (51.19%) | 4.5590 (15.56%) | 9.7433 (33.25%) |
| Closest inclusive >10 ms | 15 | 228.9069 | 0.2515 (5.81%) | 0.9653 (22.29%) | 3.1145 (71.91%) |
| Ground inclusive >10 ms | 1 | 40.1896 | 0.0140 (2.04%) | 0.3148 (45.82%) | 0.3582 (52.14%) |

These are overlapping threshold cohorts, not rows to add together. Internal >10 ms accounts for 92.86% of complete Internal search self time. Combined checking beats insertion in **47/47** of these expensive Internal searches, with checking shares 57.00–87.70% (median 66.62%). In **31/31** Internal searches over 40 ms, checking contributes 61.50–73.86%; even Line self alone exceeds insertion in all 31 raw samples. Across that severe cohort checking is 66.75% versus 33.25% insertion, approximately 2.01 times as much sampled time. This is a substantial, consistent winner for the recurrent severe Internal route, not a universal claim about every path kind.

| Internal representative: ID / object / outer frame | Inclusive / self, ms | Selected iterations | Sampled Line / Neighbor / insertion, ms | Outcome |
| --- | ---: | ---: | ---: | --- |
| 92 / 748 / 3 | 76.3856 / 76.1530 | 723 | 0.7362 / 0.1710 / 0.4008 | null_after_work |
| 4802 / 737 / 433 | 71.1406 / 70.8610 | 550 | 0.6790 / 0.1508 / 0.4478 | null_after_work |
| 6294 / 748 / 562 | 65.5024 / 64.6694 | 614 | 0.6245 / 0.1590 / 0.3740 | returned_path |
| 15006 / 906 / 1287 | 64.9885 / 64.6233 | 657 | 0.6095 / 0.1541 / 0.3402 | null_after_work |
| 10522 / 804 / 924 | 62.3066 / 61.6653 | 609 | 0.5973 / 0.1542 / 0.3478 | null_after_work |
| 27751 / 1115 / 2252 | 61.2123 / 60.5810 | 773 | 0.6609 / 0.1749 / 0.2958 | returned_path |

| Same Internal ID | Head pops | Insertions | Forward list hops | Info attempts / new / failures |
| --- | ---: | ---: | ---: | ---: |
| 92 | 46,311 | 50,723 | 3,064,909 | 1,192,817 / 16,969 / 420 |
| 4802 | 35,259 | 37,902 | 2,495,262 | 828,219 / 16,921 / 367 |
| 6294 | 39,332 | 43,783 | 2,465,174 | 964,093 / 12,884 / 0 |
| 15006 | 42,078 | 46,116 | 2,709,745 | 992,453 / 16,845 / 371 |
| 10522 | 39,027 | 41,666 | 2,418,685 | 910,296 / 16,964 / 364 |
| 27751 | 49,489 | 54,512 | 1,869,228 | 1,505,027 / 16,141 / 0 |

Millions of list hops remain real work, but their count does not establish dominant time. Info attempts greatly exceed new records: repeated line/cell checking revisits existing information. Head pops can include reopens and are not unique expanded cells; info attempts are not unique cell visits either. The measured cause is expensive repeated checking during large individual searches, with substantial insertion work alongside it, rather than merely a burst of many tiny requests.

All 47 expensive Internal searches are AI requests on the ground layer following hierarchy fallback; 45 have radius 1, two radius 2, and both crusher contexts occur (21/26). There are 23 `null_after_work` outcomes with info failures and 24 successes. Failed and successful cohorts both favor checking: sampled Line/Neighbor/insertion sums are 11.2938/3.3791/7.5360 ms and 7.5636/2.4840/4.7241 ms respectively. A null outcome with allocation failures is not proof that the destination is unreachable.

All 23 failed expensive Internal searches are followed by Closest in the same dispatch. Fifteen Closest searches exceed 10 ms; insertion wins all 15, with 59.38–84.46% of sampled time. This is a credible later target, but their 228.9069 ms self total is about nine times smaller than expensive Internal self. All 71 complete Closest records total 360.5737 ms self. The one expensive Ground operation is near-even checking/insertion and cannot establish a repeatable Ground-specific winner.

Checking wins in every expensive Internal row in each time interval: 11 searches at 0–15 s, 29 at 15–30 s and seven thereafter, with combined checking shares approximately 66.84%, 66.71% and 67.51%. Twelve objects have repeated expensive Internal searches; the pattern is not confined to one object or one failed request. Excluding the first second leaves 41 expensive Internal records, 1,765.3829 ms self and 16.0418/4.9339/10.3660 ms sampled Line/Neighbor/insertion. These repetitions are within one battle, not independent matched trials. Earlier A/B and WPR evidence establishes workload scaling and genuine CPU stalls, but contains no directly comparable Stage 4A.2 phase percentages.

### Authoritative frame attribution and alternative bottlenecks

| Outer frame / approximate offset | Logic, ms | Non-overlapping path-root attribution, ms | Main explanation |
| --- | ---: | ---: | --- |
| 2252 / 38.7233 s | 109.0160 | 105.5458 | Ground 27738: 44.3143 ms; Internal 27751: 61.2123 ms, plus tiny hierarchy work |
| 3 / 0.221944 s | 101.2830 | 98.5655 | Queue/dispatch contains Internal 92: 76.3856 ms then Closest 94: 22.1458 ms |
| 433 / 7.56715 s | 92.9418 | 89.7342 | Internal 4802: 71.1406 ms then Closest 4804: 18.4444 ms; five dispatches total |
| 1287 / 22.4297 s | 83.2153 | 78.4510 | Internal 15006: 64.9885 ms then Closest 15008: 13.4344 ms |

Queue roots enclose dispatch/request/search descendants; do not add them to those descendants. The frame 2252 aggregate `path_search` total is 105.532 ms across four calls, whereas its queue contains only the 61.2 ms queued Internal branch. Internal and sequential Closest siblings can be added; an Internal and its inclusive Request cannot. All 43 complete logic frames over 30 ms have at least 80% of logic attributed to path roots. Overall logic mean/p95/p99/max are 6.2174/10.6196/71.6297/109.016 ms. The queue reaches depth 22; severe examples occur at depths 2, 3, 5, 8 and 17, so a deep queue is not required for an individual expensive search. Queue scheduling/budget is not selected for modification.

Complete root-deduplicated workload includes 2,268,042 head pops, 36,302,097 info attempts, 1,478,641 new records, 9,073 info failures, 2,550,461 insertions, 174,244,400 forward hops (zero reverse hops), 1,560,521 cleaned cells, 189,737 block-zone queries and 582 hierarchy fallbacks. Inclusive ancestor counters must not be added again.

Cleanup peaks at 0.6413 ms; reconstruction peaks at 3.8264 ms (Ground 27738). They do not explain the recurrent 40–76 ms Internal self costs. Zone maintenance has two approximately 22.48/23.76 ms events and remains an occasional secondary outlier. Object-loop, AI-player, weapon and GameLogic-self maxima are 5.996, 2.2198, 0.5172 and 4.8242 ms respectively; none displaces path checking as this capture's authoritative stall priority. Client mean/max is 13.4166/17.8116 ms; render-end mean/max 9.10723/11.855 ms. Rendering is a substantial baseline consumer and can include driver waits, but does not explain inclusive logic-path spikes. This capture alone cannot divide GPU/driver/CPU time.

Priority for this workload is therefore (1) Internal goal-directed checking, (2) substantial Internal insertion and insertion-heavy Closest retries, (3) occasional Ground/zone work, (4) object/AI/combat/logic residuals. Rendering merits a separate throughput investigation if that becomes the objective; it is not the present authoritative-stall gate.

### Observer cost and limitations

The recorded synthetic observer model below gives new-phase incremental costs of 22.809–35.445 ns per synthetic iteration versus legacy deep profiling. Applying that illustrative range to 1,953,142 complete eligible iterations gives 44.55–69.23 ms over the 43.91-second window. The earlier aggregate/detail/counter model contributes approximately 106.71 ms, giving about 151–176 ms combined (0.34–0.40% of window elapsed time). These are modeled costs, not measured runtime corrections or upper bounds: the synthetic insertion rate, branches and cache behavior differ from gameplay.

For Internal 92, new modeled increment is 1.06–1.64 ms versus 76.3856 ms inclusive; its earlier counter model is about 2.34 ms before other scope costs. Worst logic frame 2252 has 64,561 eligible iterations: new modeled cost 1.47–2.29 ms plus about 3.27 ms old observer cost, versus 109.016 ms logic. The model does not plausibly explain the severe stalls. Different maxima from older captures do not establish observer improvement or slowdown; the battles are not paired identical workloads.

Sampled timings include probe overhead. There are 244,752 phase clock queries in complete records; synthetic clock-query costs are approximately 24–27 ns in the final trials, about 33 ns in an earlier trial. A deliberately unfavorable sensitivity calculation charges *all* those phase clock costs at 33 ns to checking and none to insertion, record by record. Checking still wins in all **31/31 >40 ms Internal searches** and 45/47 >10 ms searches; the two shorter ambiguous cases are IDs 15054 and 15084 (23.5089/27.7175 ms). This is a clock-only sensitivity, not a bound on bookkeeping/cache interference. Narrow Line-alone margins can be probe-sensitive; the robust gate is combined checking versus insertion, with Line the largest measured checking component in severe searches.

Every-64th selection can alias periodic work; elapsed samples can contain preemption, and there is no exact full-search phase decomposition or individual helper attribution inside the Line span. Nevertheless, repeated severe searches, success/failure and time-bin agreement, the clock sensitivity, and the prior WPR evidence of real search CPU work are sufficient to choose the first route. More profiling merely to refine helper percentages is not required before the benchmark/correctness infrastructure task.

### Proposed first Stage 4B target and acceptance plan

**Exact target:** the goal-directed line in `internalFindPath -> examineNeighboringCells -> iterateCellsAlongLine(ICoord2D...) -> examineCellsCallback`, including repeated `validMovementPosition`, passable-zone and `checkForMovement` work and associated info lookup/setup on that route. Do not generalize a change automatically to Ground, Closest or every caller of the shared walker. Sorted insertion is explicitly deferred as the first target; do not enable `s_useFixedPathfinding` or replace the list with a heap.

Expected direction: reduce redundant pure lookup/context/setup work within an individual goal-directed line call, where source audit and differential tests prove equivalent inputs and immutable dependencies. This is a proposed direction, not proof that caching is legal. `examineCellsCallback` performs stateful allocation, cost/parent updates, reopen/reinsert and early termination; skipping callbacks or caching whole callback results across cells/searches is not justified. First establish which repeated movement/zone/unit reads can safely be reused without suppressing checks or changing their order. Do not promise a numerical full-search speedup from sampled percentages.

Required invariants include exact visited-cell/callback order (including endpoint/adjacent-cell behavior and early abort), movement/obstacle/unit/zone/layer/footprint checks, relationship/crush semantics, allocate-info attempts and capacity/failure behavior, path result and cost, FP operations/settings, parent/reopen behavior, expansion and equal-cost insertion ordering, 5000-hop legacy behavior, hierarchy fallback/retry and Closest policy, queue ordering/budget, RNG, 30 TPS and serialization/CRC state. An optimization must not alter requested destinations or paths simply to improve the benchmark. Authoritative route changes carry replay, save and multiplayer lockstep risk even if profiler wall time improves.

**Recommended next task, before optimization iterations:** implement the already proposed guarded, reproducible developer performance scenario/benchmark harness. Audit existing scenario/replay/test entry points first and use a deterministic supported route to create a representative heavy ground workload. Workflow: build -> isolated runtime -> deterministic scenario/seed/settings -> warm-up -> bounded measurement -> exit -> reference comparison. Retain an unchanged legacy reference, record binary/PDB/source identity and scenario parameters, and make repeated trials easy. Keep capacity large enough through a shorter bounded capture or bounded scenario, rather than changing production path pools/budgets. This task does not implement the harness or authorize a launch now.

Before accepting a later optimization, require:

- Differential legacy/candidate tests for exact paths, costs, expanded/callback/insert/reopen order and allocation/failure behavior, covering successes, no-path/pool exhaustion, equal costs, the 5000-hop edge, hierarchy fallback, Closest retries, blocked/moving/fixed units, crushers, footprints, zones/layers and line endpoints.
- Repeated identical heavy scenario runs against the unchanged reference, reporting individual-search and authoritative-frame distributions (median/p95/p99/worst), work counts, outcomes and phase sampling in matched opt-in runs. Use profiling-off paired runs to verify an end-to-end improvement rather than an observer-only effect. Require consistent improvement beyond run variability in the targeted expensive cohort without material regression elsewhere; agree numeric thresholds after measuring harness repeatability.
- Full x86 Release build/tests for both titles, unchanged deterministic state/RNG/CRC progression under equivalent command streams, replay equivalence, save/load continuation and multiplayer lockstep milestone validation. Synthetic path tests alone do not establish replay/save/network compatibility.

The final analysis changed documentation only. All six source/test hashes match the saved previously validated candidate-source hashes; all five report hashes and command.txt match the hashes taken for this analysis. The complete report-validation script was rerun successfully, and `git diff --check` passes. No new build, CTest or runtime validation is claimed for this documentation pass. Capture reports and command file are preserved byte-for-byte. Ignored local analysis scripts/JSON/logs are reproducibility aids, not tooling source or staged capture data. **Stop for developer review; select the Internal checking route, commission the benchmark harness next, and do not request another five-AI selection capture.**

## Scope and implementation

Started from clean, synchronized `dev/modern-engine` at accepted/pushed tooling baseline `854b8294d10ff499ffc39b5bc40fd72f5939c39b`. This is measurement infrastructure only. The Stage 4A.1 WPR trace showed real CPU search stalls but truncated x86 caller chains could not distinguish insertion from line/movement checking. This implements only that report's approved minimum fallback. The final capture analysis above now selects the first Stage 4B route; no optimization is implemented.

`PerformanceProfile.h/.cpp` adds profiler-owned `PathPhases`, `PathIteration`, `PathPhase` and `PathInsertion`. `AIPathfind.cpp` adds iteration scopes to `internalFindPath`, `findGroundPath` and `findClosestPath`; line scopes surround the existing `iterateCellsAlongLine` calls with `examineCellsCallback` and `groundCellsCallback`. The common `PathfindCell::putOnSortedOpenList` entry records insertion inside selected phases, including retail-compatible traversal and reinsertion through the existing dispatch. No fixed-pathfinding setting or sorting behavior changes.

The existing `-performanceProfile <directory> -pathProfile` opt-in automatically enables sampled phases. Neither flag means no clocks, capture allocation or I/O from the observer. Aggregate-only profiling has no phase clocks or records. Unsampled deep iterations have only observer-owned ordinal/pointer checks; insertion's disabled check is inline. No new runtime flag, key, game command, authoritative field, serialized data, worker or subsystem is added. The test/benchmark-only Recorder constructor parameter can suppress phase sampling to compare old deep behavior; no new runtime configuration surface is needed.

### Sampling and boundaries

- Each retained Internal, Ground or Closest record starts with ordinal zero. Immediately after its existing head-pop counter, `PathIteration` increments its own ordinal and selects 64, 128, 192, etc. It does not use game RNG or a pathfinding variable. Terminal/early-skipped selected iterations can have zero phase calls; selected is not a count of expanded nodes.
- Internal and closest searches time layer changes in a short Neighbor scope and the entire `examineNeighboringCells` function in another Neighbor scope. The Line child surrounds only its existing goal-directed line call. Preparation and the remaining neighbor checks stay in Neighbor. Counts can therefore include two Neighbor spans per selected expansion.
- Ground search times the remainder of a selected expansion from `putOnClosedList` through layer changes, line preparation and the eight-neighbor loop. Its Line child again surrounds only the existing goal-directed line call. Head removal, goal tests, successful reconstruction/cleanup and closest-goal screening outside these boundaries are not measured as expansion phases.
- An enclosing Neighbor span subtracts the entire separately recorded Line child duration. Its resulting `neighbor_sample_inclusive_ms` includes its own nested insertions, but excludes the Line phase and Line's insertions. Thus Line and Neighbor totals are disjoint.
- Every insertion executed within a selected phase is timed, including insertions inside callbacks. It is charged only to that immediate phase. Initial open-list setup outside an expansion is excluded. No per-node or per-insertion rows are emitted.
- Nested path records, including dropped ones, suspend and restore phase/iteration context. This avoids charging child insertion probes or head ordinals to the enclosing search's phase accumulator. RAII also resets context on early returns, continue/break and exception unwind. Storage is reserved before capture; record addresses remain stable.

All added statements operate only on profiler state. The token audit removes the observer declarations/wrappers and reproduces every accepted pathfinder code token in its original order. Calls, arguments, callback sequence, equal-cost ties, 5000-hop retail behavior, parents/reopens, movement/unit/obstacle checks, pool capacity, hierarchy fallback/retries, queue order/budget, RNG, FP calculations/settings, 30 TPS, save/replay/network/CRC state remain unchanged. Observer wall time can affect pacing; source equivalence and synthetic tests are not a runtime replay/multiplayer proof.

### Report fields and non-overlapping accounting

Summary adds `path_phase_enabled`, `path_phase_stride=64` and `path_phase_errors`. Existing headers, rows, IDs, contexts, counters and parent relations are preserved. `paths.csv` appends eleven columns to each existing record:

| Fields | Meaning |
| --- | --- |
| phase_iterations / phase_selected | Eligible head-pop ordinal count / every-64th selections |
| phase_errors | Regressed phase/insertion clocks or inconsistent timing partitions |
| line_sample_calls / neighbor_sample_calls | Timed phase spans in that record |
| line_sample_inclusive_ms / neighbor_sample_inclusive_ms | Selected phase durations; Neighbor excludes its Line child |
| line_sample_insertion_ms / neighbor_sample_insertion_ms | Insertion time nested in each selected phase |
| line_sample_inserts / neighbor_sample_inserts | Timed insertion invocation counts in each phase |

For each eligible individual row, calculate:

```text
line_self     = line_sample_inclusive_ms - line_sample_insertion_ms
neighbor_self = neighbor_sample_inclusive_ms - neighbor_sample_insertion_ms
insertion     = line_sample_insertion_ms + neighbor_sample_insertion_ms
sampled_total = line_self + neighbor_self + insertion
              = line_sample_inclusive_ms + neighbor_sample_inclusive_ms
```

Require zero phase errors. Accounting clamps invalid subtractions and reports errors rather than wrapping unsigned time; reject erroneous phase evidence. Tiny differences at CSV rounding precision are possible. Phase fields are local to their search record and are NOT propagated into request/queue ancestors; ancestor work counters retain their existing inclusive semantics. Never add sampled phase totals to full search inclusive/self durations, or full search totals to inclusive ancestors.

`phase_iterations` counts only that record's own loop pops. Existing HeadPops counts are inclusive: Ground/Closest can include their hierarchical child search. Compare iterations against HeadPops minus the direct children's inclusive HeadPops, not blindly against the parent's raw inclusive counter. This preserves the accepted counters without changing their meaning.

The 32,768-record limit, frame/depth limits and 60-second guard are unchanged. On x86, PathSample grows from 208 to 304 bytes: reserved detail storage grows from 6,815,744 to 9,961,472 bytes (3 MiB extra). No additional rows are created. Eleven mostly zero-valued columns add approximately 0.69 MiB at full detail capacity before the header and nonzero numeric formatting. Capture overflow retains the existing dropped-detail behavior; exclude the final incomplete frame when dropped >0.

For scale, the prior synchronized capture's 32,768-row paths.csv is 4,745,643 bytes. Appending eleven zero-valued fields to each existing row would add 720,896 bytes (15.19%), plus the short header; nonzero timing text adds further bytes. This is a format-size estimate from the original file, not a new runtime capture. No input capture was rewritten.

## Validation actually run

Full existing x86 Release build succeeded for both titles and tools. CTest passes 2/2. Direct Generals and Zero Hour Google binaries pass 99 tests each (198 total). Focused `PathPhaseProfiler.*:PathProfiler.*:PerformanceProfiler.*` passes 25 tests per title. All three explicitly enabled profiler benchmark tests pass in three sequential trials per title. No game was launched.

Seven new fake-clock tests cover every-64th selection including 128, per-search reset for internal/ground/closest kinds, disabled/shallow/legacy-deep gating, nested Line/Neighbor/insertion subtraction, dropped records and stable capacity, nested-search suspension/restoration, capture reset, unsupported kinds, unsampled selection isolation, early return/exception unwind, regressed clock/error reporting and unsigned-underflow prevention. Actual engine loop/callback placement is statically audited; synthetic tests do not execute a live map/pathfinder.

The initial build and CTest passed. A final full build after the completed observer-state/disabled-check edits, final CTest/direct/focused tests and benchmarks were rerun; only that final source is staged. The warning audit finds zero warnings on new/changed lines; 140 legacy warning occurrences across the two build logs remain recorded. `git diff --check` passes. The observer-removal token audit verifies all original pathfinder tokens, exactly three loop hooks/two line hooks/one common insertion hook, and no sort/pool/budget/policy/FP/RNG/serialization changes. Prior deep capture report hashes are checked unchanged. No tests of runtime gameplay, save/replay or multiplayer equivalence are claimed.

Ignored reproducibility artifacts: `build/stage4a2-build.log`, `stage4a2-build-final.log`, `stage4a2-ctest-final.log`, `stage4a2-g/z-direct-final.log`, `stage4a2-g/z-focused-final.log`, `stage4a2-g/z-benchmark-final-0/1/2.log`, `stage4a2-validation-final.log`, `stage4a2-overhead.json`, `stage4a2-audit.py/log`. The helpers are not production/runtime functionality and are not staged for commit.

## Observer overhead

Sequential Release microbenchmarks: 262,144 synthetic head iterations, seven repetitions per mode, three trials per title; table shows the median of trial medians in ns/iteration. Every iteration performs two small expansion helpers with eight insertion probes each (16 total), batched info-attempt counters and a head-pop counter. New-hook modes include iteration/phase/insertion branches; phase mode times both spans and all their insertions only every 64th iteration. Scope/record setup and output are outside the timed loop. This deliberately measures observer/harness costs, not real line traversal, linked-list traversal or a representative game search duration.

| Mode | Generals ns/iteration | Zero Hour ns/iteration |
| --- | --- | --- |
| Profiling disabled, new hooks present | 83.129 | 83.793 |
| Stage 4A aggregate, new hooks present | 85.289 | 84.346 |
| Stage 4A.1 deep, no phase hooks | 81.866 | 93.123 |
| Deep with new hooks, phase clocks suppressed | 101.579 | 103.703 |
| Stage 4A.2 deep + every-64th phases | 117.311 | 115.932 |
| Disabled reference without phase hooks | 74.804 | 75.527 |
| Aggregate reference without phase hooks | 76.019 | 75.116 |

Sampled-mode trial medians range 115.274-118.639 ns (Generals), 115.685-122.110 ns (ZH). New sampled-minus-old-deep median differences are 35.445/22.809 ns per synthetic iteration. The disabled-hook difference is about 8.3 ns per iteration; the inline inactive insertion check avoids a function call on each disabled insertion. These are within-binary synthetic controls, not paired old/new game binaries or a same-workload runtime overhead measurement. Different code layout, CPU frequency/cache/preemption, batched counters and the artificial 16 insertions/iteration limit interpretation. Negative/small differences between adjacent modes are noise, not a speedup claim.

Applying the 22.8-35.4 ns difference to the prior 45,203-pop, 108.313 ms search gives a sensitivity estimate of 1.03-1.60 ms; applying it to all 2,501,782 prior root pops gives 57-89 ms across 55.623 seconds. Not all those pops are eligible, and real insertion distributions differ, so these are illustrative models, not corrections or rigorous upper bounds. The new probe is suitable for the bounded busy capture and cannot explain 80-108 ms stalls under this model. Several-percent observer perturbation, fixed-stride sampling bias and unmeasured memory/cache effects remain possible. Old aggregate/deep overhead is additional, not replaced by these figures. Existing aggregate/deep benchmark smoke checks were also rerun; they do not establish a new end-to-end overhead correction.

## Limitations and selection plan

This is systematic sampling of head ordinals, not random sampling or an unbiased estimator. Short searches (<64 pops) have no phase sample. Selected goal/skip iterations can have no expansion spans. A phase clock observes wall time including preemption, as before; the preceding WPR evidence establishes that repeated severe bursts were primarily on-CPU but does not guarantee every new sample is. Timed insertion includes existing counters and clock-probe overhead; very short spans are especially sensitive. The measuring code does not remove its overhead from recorded values.

The split distinguishes repeated goal-directed checks from insertion without requiring caller stacks. It does not separately time each movement/zone helper, initial versus reopened insertion, every layer/cost operation or cache effects. Ground/internal boundaries differ slightly as documented. Never treat a missing or tiny sampled group as proof that its full-search cost is zero. Do not multiply by 64 and claim exact full-search durations or sub-millisecond ETW synchronization.

Context suspension prevents attribution of nested child probes, but does not pause an enclosing phase's wall clock. If an unexpected nested search is found inside a timed expansion, its enclosing wall span can overlap the child search and must not be summed across those records. The current targeted expansion routes do not deliberately start another search; hierarchy/cleanup/reconstruction have their existing separate boundaries.

Analyze the single final window by individual expensive search record, kind/outcome and surrounding logic frame. Check coverage, errors, selected insertion counts and both success/failure/closest routes. Compare repeated severe searches' line_self, neighbor_self and insertion proportions, together with existing pops/inserts/hops/info failures. Separate child reconstruction/cleanup and hierarchy work. Require a consistent substantial winner across repeated pathological operations before choosing exactly ONE first Stage 4B target. The other hotspot can become a later optimization. If the split remains comparable, report that evidence honestly; more instrumentation or another ETL is not an automatic next step.

## Guarded candidate and single final manual capture

Candidate: `stage4a2-path-phase-sampling`, a new isolated full runtime copy; previous runtimes are retained. Final integrity results and EXE/PDB identity are recorded below after staging. No game launch, user-data backup/write, Steam installation change, commit or push was performed by the agent.

Staging completed using the existing guarded `Stage-ZHRuntime.ps1` workflow. Full source/new candidate/previous Stage 4A.1 inventories pass with 369 files each. Guarded launch validation, including backup validation, passes without creating a backup, launch receipt or game process. The validation-only output directory is `build/performance/stage4a2-validation`; the final manual capture directory below is distinct and has not been created by this task.

| Candidate identity | Value |
| --- | --- |
| Native image | x86 PE32; Generals and ZH Release builds passed |
| EXE/PDB GUID / age | 5706bc8d-51e9-49fc-9096-a636d0fd56be / 21 |
| generalszh.exe SHA-256 | B851F8A9F5CDDFD37CD54CCA29DAE30DEEFDBA79F43459E7BC90DD25F0CD730E |
| generalszh.pdb SHA-256 | 7401783ECB0A37E597CCA45E18A1EC8DB42F3317E75833CE51F32D83D361598B |
| Stage / integrity / launch logs | build/stage4a2-stage.log, stage4a2-integrity-source.log, stage4a2-integrity-candidate.log, stage4a2-integrity-previous.log, stage4a2-launch-validation.log |

The PDB GUID can remain constant during incremental linking; age 21 and hashes distinguish this candidate from Stage 4A.1 (age 19). The manifest records accepted HEAD plus the dirty observer worktree, not a new commit.

Use PowerShell 7 from `C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode` after developer review. Preserve every earlier capture. The fresh directory below must not already contain prior profiling reports; choose a new suffix if it does. Validate the runtime before the guarded developer launch:

```powershell
$profileDir = 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\performance\stage4a2-B-final'
New-Item -ItemType Directory -Path $profileDir -ErrorAction Stop | Out-Null
Set-Content -LiteralPath (Join-Path $profileDir 'command.txt') -Value ('idle ' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage4a2-path-phase-sampling -CheckSource
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage4a2-path-phase-sampling
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage4a2-path-phase-sampling -BackupUserData -GameArguments @('-groundInterpolation','-performanceProfile',$profileDir,'-pathProfile') -ValidateOnly
# Developer launch after reviewing validation:
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage4a2-path-phase-sampling -BackupUserData -GameArguments @('-groundInterpolation','-performanceProfile',$profileDir,'-pathProfile')
```

Keep interpolation ON, requested/effective cap 120, the same representative large map, developer + 5 AI, settings/factions/difficulties and ground-heavy fighting/movement. Record those settings, match time, approximate armies/camera, pauses/focus changes and visible stutters beside the capture. Wait until the battle is already busy. No new A control or WPR/ETL trace is requested. In a second PowerShell 7 terminal set the same `$profileDir`, then schedule BOTH start and stop before returning focus to the game:

```powershell
$profileDir = 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\performance\stage4a2-B-final'
$captureNonce = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff')
Start-Job -ArgumentList $profileDir,$captureNonce -ScriptBlock {
    param($dir,$nonce)
    Start-Sleep -Seconds 8
    Set-Content -LiteralPath (Join-Path $dir 'command.txt') -Value ('start B-final-' + $nonce)
    Start-Sleep -Seconds 45
    Set-Content -LiteralPath (Join-Path $dir 'command.txt') -Value ('stop B-final-' + $nonce)
} | Out-Null
```

Return focus within 8 seconds and remain in the active battle for about 60 seconds before inspecting files; keep that second terminal alive. Polling introduces up to one second of start/stop uncertainty, so expect roughly 44-46 seconds and `mode=manual_stop`. Capacity remains a valid earlier safety stop. The delayed stop avoids a focus transition to terminate profiling. Do not deliberately wait for detail capacity.

Preserve all five generated report files plus command.txt and scenario notes. Expect `path_detail_enabled=1`, records >0, `path_phase_enabled=1`, stride 64, `path_phase_errors=0`, `stack_or_clock_errors=0`, interpolation 1 and caps 120. Check phase_iterations equals the record's own HeadPops after direct-child subtraction, selected=floor(iterations/64), positive phase spans/insertion counts across repeated severe individual searches, valid parent/frame links and nonnegative phase self. Prefer zero dropped records; if capacity ends the window early, exclude incomplete final-frame attribution and first analyze the complete severe samples instead of automatically rebuilding the manual battle. Entry/device/focus artifacts stay in raw data but are not steady-work evidence. This is intended to be the last manually constructed large-AI selection workload if its samples settle the gate.

## Agreed future automation direction (documentation only)

After selecting the first Stage 4B target, prioritize a reproducible developer performance scenario/benchmark harness before a long optimization series. Desired workflow: build -> stage isolated runtime -> launch deterministic scenario -> warm up -> generate representative heavy pathfinding/object/AI load -> start bounded profiling -> capture -> exit -> compare with a reference baseline. This should reduce repeated manual construction of five-AI battles. Real developer+AI matches remain milestone validation; the automated scenario becomes the frequent development benchmark. No automated scenario or launch orchestration was implemented here.

Stopped for developer review. Stage 4B is not implemented; no commit or push.
