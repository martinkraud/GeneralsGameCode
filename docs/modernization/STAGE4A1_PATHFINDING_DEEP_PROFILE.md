# Stage 4A.1: capture analysis and individual pathfinding profile

## Accepted tooling baseline - pre-commit validation, 2026-10-08

The developer accepted Stage 4A + Stage 4A.1 as the performance-tooling baseline and authorized one tooling commit. The synchronized WPR findings below are included. Stage 4B selection remains unresolved; the sampled phase accumulator is a proposal only and has not been implemented.

Pre-commit checks actually run: full existing win32 Release build (successful, no compilation/link work needed), CTest 2/2, direct Generals and Zero Hour Google tests (92 each, 184 total), and both explicitly enabled PerformanceProfileBenchmark tests in each title. The benchmark executions were smoke checks, run concurrently, and supply no new controlled overhead estimate. The pathfinder observer-removal token audit passes. Current Release generalszh.exe/PDB SHA-256 hashes exactly match the captured stage4a1-pathfinding-deep-final candidate. No source changes, game launch, runtime staging or push occurred. Validation logs remain ignored under build/stage4a-tooling-commit-*. Earlier no-commit/no-build statements below describe their historical analysis tasks.

## Synchronized WPR selection analysis - 2026-10-08

**Verdict B: the ETL confirms real on-CPU pathfinding stalls but does not resolve the strict Stage 4B selection gate. No optimization target is selected yet.** The matching game symbols resolve correctly and ETW reports zero lost events/buffers. The technical limitation is truncated game caller chains in almost all insertion and movement samples, combined with comparable insertion and movement/checking leaf costs that vary across bursts. Neither the claim that insertion dominates nor the claim that line checking dominates is established. Use only the previously documented minimum fallback: bounded sampled per-search phase accumulators separating line/neighbor work from insertion, with nested insertion subtracted from the enclosing phase. Review that observer proposal before implementing it; no probe or gameplay code was changed here.

**Stage 4A + Stage 4A.1 should be accepted and committed as the performance-tooling baseline after developer review.** Its acceptance does not require guessing the optimization. No staging, commit, push, game launch, runtime staging or build/test run occurred in this analysis. Only this report and ignored analysis artifacts under build/stage4b-* were written. The previously passing full x86 Release build, 184 direct tests and CTest 2/2 apply to the unchanged instrumentation, not a new validation run. Earlier sections below are retained as history; this section supersedes the request to obtain another WPR trace.

### Synchronized capture validation

All five 20261008T203334Z-18676-1 reports and command.txt were read directly, along with the actual 2,469,396,480-byte B-cpu.etl. No manually copied summary was used. Categories/frames/slow rows, all individual parent/frame/logic relationships, timing/counter partitions and summary per-kind statistics were independently cross-checked with zero discrepancies beyond CSV rounding. Category self partitions differ from outer time by at most 0.0005 ms.

| Check | Observed | Interpretation |
| --- | --- | --- |
| path_detail_enabled / records | 1 / 32768 | Deep recording active and bounded |
| path_detail_dropped | 4 | Final outer 3189 is incomplete; excluded from individual attribution |
| stack_or_clock_errors | 0 | No recorded profiler stack/clock errors |
| Duration / mode | 55.6232 s / capture_capacity | Expected bounded early stop, not a full 60-second window |
| Outer frames / completed ticks | 3190 / 1556 | ~57.350 outer Hz and 27.974 completed ticks/s |
| Interpolation | 1 | ON |
| Requested / effective cap | 120 / 120 in every frame | Cap remained constant; not achieved FPS |
| Logic mean / p95 / p99 / max ms | 5.6399 / 4.9166 / 88.3666 / 156.3450 | Rare severe tails |
| Logic frames >30 ms | 61 | Path roots >=80% of logic in all 61 |
| Internal searches >10 / >30 ms | 61 / 60 | Pathological individual work reproduced |
| Internal null_after_work | 47; all with info failures | Resource-limited failures, not proof of topological no-path |

The candidate build string matches the prior captures (MSVC 195136260, Oct 8 2026 18:20:48; git 1de7e65d173104ad2f288ac0a379e102a5057ebc dirty 1). The first outer is 154.195 ms, of which client 138.359, display-draw 133.338 and scene 84.4554 ms; logic is only 2.7674 ms. The next outer is 26.4448 ms. Retain both raw rows, exclude indices 0 and 1 from steady rendering/outer comparisons, and do not interpret that entry/focus/device candidate as a pathfinding stall. Later severe logic frames occur around32-50 capture seconds and are retained. The four dropped detail records affect only outer 3189 (logic2.0652 ms, search aggregate 0.0244 ms), not the severe samples or their attribution.

Raw complete-window path totals include the retained portion of the final frame; selection uses complete earlier intervals. There are1938 queued valid dispatches,1863 findPath/internal calls,1997 hierarchical calls,5 ground,67 closest,62 attack and 3390 move-away intervals. No safe/patch searches occur. Queue released cells total 1,629,902, max queue depth 14. Root counters:2,501,782 head pops;58,057,609 info attempts;1,646,037 new records;15,039 info failures;2,783,250 insertions;252,855,185 forward hops; zero reverse hops;1,709,866 cleaned cells;57,538 block-zone queries;209 hierarchy fallbacks. Inclusive ancestor counters are not added together.

The 61 internal calls >10 ms account for 3997.639 ms self of 4024.716 ms total internal self (~99.3%). Every one has a hierarchy-fallback request parent; all are AI-controlled ground-layer searches,60 with radius 1 and one radius 2. Expensive repeated IDs 956/ 1020 occur13 times each,865 twelve times and 1083 eleven times. All47 failed internal calls have crusher 1, radius 1, ground layer 1, hierarchy fallback and info failures. The same pathological mechanism seen in B2/B3 is reproduced, although this is another battle/window rather than an identical deterministic workload.

| Outer / logic before | Offset s | Logic ms | Search aggregate ms / calls | Queue ms / dispatches / released cells |
| --- | --- | --- | --- | --- |
| 1903 / 10844 | 31.995 | 156.3450 | 154.2220 / 8 | 80.7542 / 3 / 19902 |
| 1904 / 10845 | 32.168 | 118.1710 | 93.7121 / 3 | 115.3800 / 1 / 31790 |
| 1907 / 10848 | 32.416 | 112.1390 | 108.3160 / 2 | 108.3530 / 1 / 17360 |
| 2303 / 11036 | 39.758 | 97.6676 | 79.7370 / 8 | 93.1948 / 3 / 31503 |
| 1922 / 10856 | 32.927 | 97.1132 | 75.9353 / 3 | 93.7192 / 1 / 30881 |
| 2100 / 10941 | 36.076 | 96.9249 | 80.9582 / 7 | 94.5170 / 3 / 29742 |
| 2030 / 10908 | 34.828 | 96.6261 | 77.7397 / 4 | 92.3963 / 2 / 32196 |
| 2132 / 10956 | 36.660 | 95.7359 | 80.0670 / 7 | 93.3355 / 3 / 29788 |
| 1934 / 10861 | 33.177 | 94.7836 | 75.6626 / 3 | 90.8720 / 1 / 30621 |
| 2179 / 10978 | 37.531 | 93.6872 | 81.6592 / 3 | 91.3684 / 1 / 28696 |

| Individual ID / parent | Kind / object | Inclusive / self ms | Pops / inserts | Forward hops | Info attempts / new / failed | Outcome |
| --- | --- | --- | --- | --- | --- | --- |
| 18660 / 18655 | internal / 679 | 108.3130 / 103.2760 | 45203 / 49873 | 3000168 | 1254771 / 17089 / 0 | returned_path |
| 18600 / 18595 | internal / 1019 | 93.6887 / 93.3442 | 54541 / 59277 | 3505896 | 1467495 / 20233 / 388 | null_after_work |
| 21555 / 21550 | internal / 865 | 81.6425 / 80.9183 | 40279 / 44459 | 3721699 | 1091625 / 20217 / 319 | null_after_work |
| 20661 / 20656 | internal / 1083 | 80.8981 / 80.1111 | 39899 / 44442 | 3728897 | 1101672 / 20202 / 318 | null_after_work |
| 21002 / 20997 | internal / 956 | 79.9908 / 79.7009 | 38696 / 43185 | 3933847 | 1083734 / 20208 / 322 | null_after_work |
| 18696 / 18691 | internal / 1207 | 82.0485 / 79.2539 | 48080 / 52785 | 3133877 | 1247988 / 19945 / 0 | returned_path |
| 23205 / 23200 | internal / 956 | 79.6840 / 79.2306 | 35568 / 39556 | 3702428 | 1014627 / 20201 / 293 | null_after_work |
| 18582 / 18577 | internal / 1082 | 80.7132 / 79.1481 | 49455 / 54198 | 3201666 | 1228087 / 19562 / 0 | returned_path |

Frame 1903 is a compound authoritative stall: ground 73.4882 ms plus internal 80.7132 ms produce search aggregate 154.222 ms inside logic156.345 ms; queue 80.7542 ms overlaps the internal route and is not added. Frame 1907 contains the largest individual internal interval 108.313 ms (self103.276), returned_path for object 679; reconstruction 3.7019 and cleanup 1.3346 ms account for its measured children. Frame 1904 contains internal 18600=93.6887 ms (self93.3442), failed for object 1019 with 388 info failures, followed by additional request/closest work inside queue 115.379 ms. The internal loop dominates, not cleanup. Remaining worst detailed calls repeat80 ms failures. Source 7243 on queued internal calls includes Logic+AIGlobal+PathQueue+PathRequest+PathSearch, while ground root source 4107 identifies authoritative work outside the queue. No client interval is added to logic path totals.

### ETL integrity, process and symbol/stack quality

The ETL was processed directly with the installed Microsoft xperf reader: SampledProfile (the underlying CPU Usage (Sampled) data), CSwitch and ReadyThread scheduling data, process/image rundown, trace statistics and decoded stack records. Read-only offline processing was used instead of launching the game or relying on UI screenshots. Derived raw/event/symbol exports are retained under build/stage4b-*. One initial stack filter used SampledProfile instead of the actual event name "Sampled Profile" and returned no rows; the corrected filter and direct raw sample dump below are the results used. An unsupported cswitch -util interval command was discarded; scheduling results use actual CSwitch/ReadyThread events.

| ETL check | Result |
| --- | --- |
| Start / end UTC | 2026-10-08 20:32:18.4956669 / 20:35:57.1635797 |
| Duration | 218.6679128 seconds; much longer than the CSV window |
| Lost events / buffers | 0 / 0 |
| Decoded event total | 21,173,422 |
| Sampled Profile events / with stacks | 581624 / 489498 (trace-wide) |
| CSwitch / ReadyThread events | 5831347 / 2971372 |
| Sampling period | 1000 microseconds throughout |
| Actual game PID / main TID | generalszh.exe 18676 / 18788 (WinMainCRTStartup) |
| Loaded game image | base 0x00e00000, size 0x006ba000, symbols resolved by RVA |
| Matched PDB | generalszh.pdb GUID 5706bc8d-51e9-49fc-9096-a636d0fd56be, age 19 |
| Staged EXE/PDB SHA-256 | Match the previously validated candidate hashes below |

Game symbols are usable for leaf addresses: the main-thread dump contains no unresolved generalszh.exe!0x... instruction-pointer labels. The generated game symcache key ends in 13, the hexadecimal representation of PDB age 19. EXE/PDB hashes remain 48463C4D86AF91BDA7BEC92EB35EFC23532E41FB3667BFD43BC2FA3B2AFB7193 and 6C93C1CD618CD130ABFF204B526465CF1ACE53C5E7C15B20A40BCE0486AB3CBA. OS/driver public symbols were not downloaded; their module/IP labels remain unresolved, limiting deeper rendering/kernel analysis. This does not explain the missing game callers: the game PDB matches and its leaf functions resolve. Fetching unrelated OS symbols would not recreate game return addresses absent from these recorded chains.

Nominal UTC anchor: CSV capture_started_utc20:32:38 minus ETL start gives 19.5043331 ETL seconds; adding 55.6232 gives 75.1275331. These whole-second CSV timestamps have up to approximately 1 second of start uncertainty. The primary export uses 19.504333-75.127533 seconds and checks the interior 21-74.5 seconds for boundary sensitivity. The CSV first-display recovery is excluded from rendering comparisons. Burst patterns suggest an offset near 20.1 seconds (within the UTC uncertainty), but this is a coarse pattern alignment, not a measured ETW marker or sub-millisecond synchronization. Do not assign one sampled instruction to one CSV call ID from timestamp proximity alone.

The nominal window has 20488 main-thread SampledProfile events;20153 have a recorded stack (335 absent). Of 1941 insertion-leaf samples,1931 contain only one game frame and 10 have none; zero have an internalFindPath caller. Of 1012 checkForMovement samples,1008 contain only one game frame and 4 have none; zero have an internal caller. examineCellsCallback similarly has 382 single-game-frame stacks plus2 absent; zero internal callers. iterateCellsAlongLine has 116 single-frame and 3 two-frame stacks. Occasional longer stacks in other functions do not repair coverage of the two main competing hotspots. This is observed truncation, not an assertion about a specific compiler setting or an ETW event-loss diagnosis. The xperf inline-name annotation lists functions inlined into putOnSortedOpenList, including the retail and fixed variants; it is not separate exclusive timing of each variant.

### Whole-window sampled CPU evidence

Counts below are exclusive sampled instruction-pointer hits on game main TID 18788, nominally ~1 ms/sample, not instrumented durations or inclusive call-tree weights. Denominator is 20488 main-thread samples, not xperf's all-process/all-processor usage percentage. Game-image leaves total 13351; other 7137 samples are kernel, graphics/audio/runtime modules or other external code. No overlapping parent/child inclusive times are summed.

| Exclusive leaf | Samples | % main-thread samples |
| --- | --- | --- |
| PathfindCell::putOnSortedOpenList | 1941 | 9.474% |
| Pathfinder::checkForMovement | 1012 | 4.939% |
| Pathfinder::examineCellsCallback | 384 | 1.874% |
| Pathfinder::validMovementPosition | 252 | 1.230% |
| Pathfinder::examineNeighboringCells | 151 | 0.737% |
| PathfindCell::allocateInfo | 126 | 0.615% |
| Pathfinder::iterateCellsAlongLine | 119 | 0.581% |
| PathfindZoneManager::isPassable | 98 | 0.478% |
| PathfindCellInfo::getACellInfo | 55 | 0.268% |
| Pathfinder::groundCellsCallback | 1 | 0.005% |
| Explicit PerformanceProfile functions | 58 | 0.283% |

Insertion is the largest single resolved game leaf:1941 samples. The seven selected movement/line/neighbor/allocation leaves total 2045 samples: checkForMovement 1012, examineCellsCallback 384, validMovementPosition252, examineNeighboringCells151, allocateInfo126, iterateCellsAlongLine119 and groundCellsCallback1. Of 4544 pathfinder-named leaf samples, insertion is 42.7% and that selected group 45.0%. The group omits additional zone, terrain, cost and object helpers and does not isolate line scans from ordinary neighbor/closest/attack checks. Hence it is neither a complete line-walk inclusive total nor proof that movement dominates. Interior21-74.5 seconds still has 1889 insertion,993 checkForMovement and 377 examineCellsCallback samples; the comparison is not created by the approximate capture boundary.

The full 218.668-second ETL has 94.704 seconds of sampled process weight across all game threads, including setup and substantial time after the bounded CSV ended. Across that entire ETL, exclusive process weights are3.250 s insertion,2.067 s checkForMovement,0.658 s examineCellsCallback,0.300 s examineNeighboringCells,0.205 s iterateCellsAlongLine and 0.160 s allocateInfo. These are supplementary context, not synchronized-search percentages. In the nominal CSV window process-wide sampled weight is 29.088 s and CSwitch process runtime 29.374 s, with main-thread CSwitch runtime 19.972 s. Background graphics/audio threads contribute substantially; their work is not charged to authoritative path searches.

Other main-thread leaves include PartitionManager::getClosestObjects 494 samples, ParticleSystem::draw 277, GameLogic::rebalanceChildSleepyUpdate 259 and W3DParticleSystemManager::doParticles 204. They do not displace the demonstrated search tails; without reliable callers, the partition-query cost is not arbitrarily reassigned to search. CPU sampling does not show another specific sub-operation clearly outranking both competing search groups. Reopen/reinsert work is confirmed by the source/counters, but no leaf label distinguishes an initial insertion from reinsertion; no separate reopen percentage can be claimed. allocateInfo samples include its inlined profiler check and reuse/allocation logic, not only fresh pool allocation. Fewer groundCellsCallback samples can also reflect inlining and rare ground calls; it is not evidence that ground search is free.

### Repeated severe bursts and scheduling

To avoid claiming exact CSV/ETW event matching, group pathfinder-leaf samples separated by at most8 ms and retain groups spanning>40 ms. These are ETL path-heavy clusters, not reconstructed single-call intervals; adjacent searches/frames can merge. Report selected clusters alongside scheduling state derived directly from main-thread CSwitch and ReadyThread events. "Running" means scheduled time, including any interrupt/DPC time while dispatched; it is not a cycle-perfect user CPU measurement. Ready time is runnable off-CPU time. Waiting uses the observed thread state; wait-reason labels do not identify the owning game subsystem without callers.

| ETL start-end s | Span ms | Path / insertion / selected movement leaves | Scheduled / ready / waiting ms |
| --- | --- | --- | --- |
| 52.523-52.893 | 369.574 | 322 / 114 / 178 | 365.194 / 2.285 / 2.095 |
| 52.111-52.265 | 154.647 | 145 / 69 / 44 | 153.912 / 0.735 / 0.000 |
| 50.959-51.081 | 122.047 | 104 / 24 / 68 | 119.546 / 0.919 / 1.582 |
| 52.275-52.389 | 114.649 | 107 / 46 / 46 | 114.294 / 0.355 / 0.000 |
| 56.195-56.296 | 100.994 | 86 / 37 / 40 | 99.523 / 0.758 / 0.713 |
| 59.879-59.971 | 92.558 | 86 / 44 / 37 | 92.166 / 0.392 / 0.000 |
| 53.047-53.139 | 92.518 | 86 / 42 / 40 | 92.004 / 0.514 / 0.000 |
| 56.778-56.870 | 92.241 | 86 / 48 / 33 | 91.792 / 0.449 / 0.000 |
| 58.325-58.417 | 91.594 | 84 / 36 / 37 | 91.343 / 0.251 / 0.000 |
| 62.593-62.684 | 91.329 | 78 / 38 / 37 | 89.157 / 0.852 / 1.320 |

The 52.111-52.265 s cluster (154.647 ms) is consistent at coarse offset~20.1 s with the 156.345 ms logic frame1903 and its73.4882 ms ground plus80.7132 ms internal searches. It is 153.912 ms scheduled with 0.735 ms ready and no measured wait: the long interval is computation, not a 150 ms preemption. The 52.523-52.893 s cluster spans multiple nearby expensive requests, consistent with the CSV1907-1909 region, so it is not labeled a single 369 ms search. Later90-101 ms clusters recur with mixed insertion/checking weights. For example one cluster has 69 insertion versus44 selected movement samples, another 114 versus178, and others46/ 46 or 37/ 40. There is no consistent, clearly dominant winner across the pathological bursts.

Over ETL 50-66 seconds the main thread is scheduled 7133.863 ms, ready 183.271 ms, waiting 8682.846 ms, with 0.020 ms initial state unknown. Waiting reasons include UserRequest 6646.077 ms, WrAlertByThreadId 1595.533 ms, DelayExecution 270.450 ms and WrResource 165.809 ms; these span whole frames including rendering/pacing. At ETL 52-53 seconds, scheduled 858.867 ms/ready 7.678 ms/waiting 133.455 ms coincides with 677 pathfinder-leaf samples. Global frame waiting is expected and must not be confused with off-CPU time inside the path-heavy intervals. On these observed bursts, OS scheduling does not explain the severe search wall times; individual CPU frequency/cache/interrupt effects remain unmeasured.

### Observer overhead and other alternatives

Explicit profiler function leaves total 58 samples (~0.283% of main-thread samples), including31 pathCount hits, but inlined counter instructions are charged to their containing functions and QPC/runtime work can have external labels. Thus 58 is not the full observer cost. The synthetic scope/counter model estimates 142.488 ms for aggregate+detail+selected counter work over the 55.623 s capture, ~2.44 ms of counter work for the largest individual search and~2.93 ms for the worst logic frame. These estimates are not measured corrections or rigorous upper bounds. They cannot explain the repeated80-108 ms search intervals under that model, and ETL samples show substantial real insertion/checking work. No paired shallow/deep deterministic benchmark was supplied, so several-percent perturbation remains possible.

Independent non-pathfinding maxima in the CSV: player strategy 0.9477 ms, weapon fire 0.7413 ms, object loops 16.6208 ms, AI-object 4.0277 ms, GameLogic self 2.4242 ms. Zones rebuild twice for 20.0746/ 21.4984 ms. Cleanup max 1.3346 and reconstruction 3.7019 ms are visible but far smaller than search self tails. Initial rendering has the entry artifact described above; steady scene/draw/present work and graphics-driver samples consume baseline budget but do not cause time already attributed to the authoritative search loop. None now deserves higher priority than resolving the search sub-operation gate.

### Exact remaining limitation and minimum fallback

The failed part of the measurement is caller coverage, not WPR completion, event retention or the matching game PDB. For the two leading hotspots the ETL has a named leaf but no enclosing internalFindPath/line-callback/neighbor chain. Consequently it cannot separate checkForMovement called from repeated goal-directed lines versus neighboring-cell checks, nor count insertion cost exclusively outside/inside each of those phases, nor distinguish initial versus reopened-node insertion time. Insertion is a strong and substantial candidate, but a largest-single-function ranking is insufficient to claim dominance over a comparable multi-function checking computation under the user's selection rule. Repeating the same WPR settings or downloading more symbols is not the minimum remedy for missing recorded game frames.

Propose only the already documented bounded sampled per-search phase accumulator, to be reviewed before any implementation:

1. Keep the existing default-OFF developer deep-profile gate, fixed capture bounds and linked search record. Add no per-cell/per-insertion CSV records or unbounded storage. Store phase sample counts/ticks on the retained search record (or equivalent bounded profiler-owned data).
2. Select a fixed sparse subset of open-head iterations per search using a profiler-owned ordinal, initially every 64th iteration; do not use game RNG or branch search decisions. Within selected iterations time the goal-directed line phase and remaining neighbor/layer work, and time insertion calls nested in those selected phases. Report enclosing phase inclusive ticks and nested insertion ticks separately; subtract nested insertion before comparing line/neighbor self with insertion. Insertion invokes counters too, so retain an explicit observer estimate and sample coverage.
3. Cover internal and ground loops and the closest retry path sharing the same insertion route. Retain existing outcomes/work counters/context; no new request classification, cache, pool change, state-machine change or subsystem refactor. Keep all phase state outside authoritative objects and serialized data. No need for finer per-obstacle timing, uniqueness counters, another full WPR trace or new control suite to answer this specific gate.
4. Validate fake-clock nested subtraction, sample selection/reset, disabled gating and capacity/drop handling before any new candidate. Calibrate observer cost and sampling coverage; sampled phase totals are estimates, not full-search exact times. Repeat one bounded busy window once pathological activity is present, stopping before detail capacity (a delayed 45-second stop is suitable for this workload). Inspect both failure/retry and long successful ground/internal samples. The accumulated line-versus-insertion split must repeat across expensive operations before choosing exactly one optimization.

This is a proposal only. No fallback counters, clocks, tests or runtime candidate were implemented in this task. If approved later, perform the existing full x86 Release validation/guarded staging workflow for that observer. Existing tooling should be committed separately before this focused addition or Stage 4B work, after developer review.

### Authoritative behavior and future acceptance gates

No Stage 4B implementation approach is selected or authorized by this report. If later evidence selects insertion, the narrow code route is PathfindCell::putOnSortedOpenList/forwardInsertionSortRetailCompatible; any acceleration must reproduce exact current ordering, equal-cost <= ties,5000-hop legacy truncation, pointer-removal/reinsert behavior and dangling-link/fixed-fallback guards. Do not enable `s_useFixedPathfinding` or substitute a generic heap as a shortcut. If line/checking is selected, preserve callback order, early aborts, movement/unit/obstacle reads, FP costs, parent updates and reopens; the line callback mutates search state and is not a disposable visibility query.

For either future target, pool capacity, hierarchy/retry policy, queue order/budget, RNG/FP behavior,30 TPS logic and serialized/replay state must remain unchanged. Benchmark identical scripted searches against the reference, including heavy failures/closest retries, successful long paths, equal costs, reopened nodes,5000-hop boundaries and pool exhaustion. Require identical popped-node sequence where required, costs/parents, path nodes/outcomes, dispatch/tick order and end-state CRCs. Replay playback, save/load continuation and multiplayer lockstep are explicit acceptance checks; single-player traces do not prove them. Compare both shallow and deep instrumentation modes and multiple representative busy windows before claiming a speedup.

### Reproducibility and unchanged inputs

Derived evidence: stage4b-cpu-analysis.json/log, stage4b-cpu-extra.json/log, stage4b-tracestats.txt, stage4b-eventstats.txt, stage4b-process.txt, stage4b-profile-whole.txt, stage4b-profile-window.txt, stage4b-sample-frequency.txt, stage4b-sampled-stacks-window.html/json, stage4b-window-samples.csv/analysis.json, stage4b-selection-evidence.json/log and stage4b-scheduling-analysis.json/log under build. xperf output uses nominal UTC anchor range 19504333-75127533 microseconds; stack filtering uses PID 18676/TID 18788/event "Sampled Profile"; scheduling event export uses 50-66 seconds. Raw export/analysis helpers are ignored artifacts, not added runtime functionality. Failed exploratory exports were not used in the findings. Final checks rehash original captures/ETL and verify document whitespace; no test/build claim is based on those document checks.

| Input | SHA-256 |
| --- | --- |
| 20261008T203334Z-18676-1-frames.csv | 9f86fc1161bd97ceec9e11d766346221636b3a238d1802aec471212e1513bff0 |
| 20261008T203334Z-18676-1-paths.csv | e6bfbe36cde2e35f57bec8facc0ca72064fe013f35792b7f626dc70fcdd49c3b |
| 20261008T203334Z-18676-1-categories.csv | 021383d11d17f5a225cf529c194682ce47364328a20fec6019504e4805c88629 |
| 20261008T203334Z-18676-1-slow.csv | bbcb045325b58ec54b9f8b639a68f6ac59be9a4cf61c40ee38510ae070763780 |
| 20261008T203334Z-18676-1-summary.txt | 08afc0c36b06db92f84577e512e6795ec8cc71d794964ebad5cc636193cece75 |
| B-cpu.etl | 1724D7FB7269B7AC791FAFDD2D6A885C73BEF9CA53AE323AA43841AFD6BEDCC7 |

**Stopped for developer review: accept/commit the unchanged performance tooling baseline; review the minimal sampled phase observer before any further implementation. No commit or push was made.**
## Final Stage 4A / 4A.1 analysis - developer review, 2026-10-08

**Verdict B: one additional measurement is required before selecting the exact Stage 4B optimization. ACCEPT Stage 4A/4A.1 as the performance-baseline tooling, subject to developer review; it is ready to commit as tooling, but no commit was made.** The new captures establish expensive individual detailed searches, hierarchy fallback and subsequent closest-path retries as the principal severe-stall mechanism. They do not establish whether sorted-list insertion or repeated goal-directed line/movement checking dominates the remaining search-loop CPU time. One synchronized CPU sampling/scheduling trace using the existing candidate is the minimum next step. No new game instrumentation is proposed or implemented in this task.

This section supersedes the earlier capture decision below. Only this document was edited among repository deliverables. Gameplay/profiler/test code was not changed; no build, game launch, runtime staging, commit or push was performed. The earlier x86 Release validation remains the validation of the unchanged candidate, not a new test run. Analysis scripts and derived JSON/logs are ignored under build/stage4a1-final-*.

### Inputs and capture validation

Read all 20 generated files directly: every summary.txt, categories.csv, frames.csv, slow.csv and paths.csv across one A and three B windows. Also read the two command.txt control files; they retain only the last start command, not a window history. The eight original Stage 4A files were also reread and their frame/category/slow cross-checks rerun without errors (build/stage4a1-old-capture-recheck.log), so the historical comparisons below are checked against disk too. B1/B2/B3 mean chronological windows, identified below. All three B files report process 8164 and successive capture sequence IDs: these are separate windows of one process/session, not three independently restarted games. A reports process 29824. This is repeatability across workload windows, not independent statistical trials or a controlled AI-count experiment.

| Set | Capture basename | Frames / completed ticks | Detail records / dropped / errors | Seconds / mode |
| --- | --- | --- | --- | --- |
| A | 20261008T182324Z-29824-1 | 3602 / 1800 | 6112 / 0 / 0 | 60.0119 / 60_second_limit |
| B1 | 20261008T180518Z-8164-1 | 3602 / 1798 | 26154 / 0 / 0 | 60.0091 / 60_second_limit |
| B2 | 20261008T181332Z-8164-2 | 3556 / 1676 | 28818 / 0 / 0 | 60.0084 / 60_second_limit |
| B3 | 20261008T181539Z-8164-3 | 3564 / 1690 | 12180 / 0 / 0 | 60.0187 / 60_second_limit |

Every summary has path_detail_enabled=1 and ground_interpolation=1. Every frame has requested_fps=120 and effective_fps=120; no cap changes occur. Build metadata is identical (MSVC 195136260, Oct 8 2026 18:20:48), git=1de7e65d173104ad2f288ac0a379e102a5057ebc, dirty=1, title=Zero Hour. These metadata do not independently certify the loaded EXE hash or all graphics/mod settings; retain the staged manifest with future captures. The requested/effective values describe the cap, not achieved render FPS.

| Set | Observed outer Hz | Completed ticks/s | Logic mean / p95 / p99 / max ms | Logic frames >30 ms / path roots >=80% |
| --- | --- | --- | --- | --- |
| A | 60.021 | 29.994 | 1.0795 / 1.5962 / 2.1284 / 6.3008 | 0 / 0 |
| B1 | 60.024 | 29.962 | 2.2569 / 3.2208 / 11.7853 / 44.2501 | 1 / 1 |
| B2 | 59.258 | 27.929 | 8.2275 / 45.5982 / 54.3523 / 147.6280 | 132 / 132 |
| B3 | 59.381 | 28.158 | 8.0782 / 45.2753 / 53.9417 / 94.8253 | 122 / 121 |

Unlike the old Stage 4A captures, none of these windows has a multi-second first display frame or an obvious device/focus recovery pair. First three outer durations are A 9.412/19.756/13.490 ms; B1 9.530/15.053/18.508; B2 8.836/8.333/8.333; B3 9.062/8.334/8.333. Retain all frames in the analysis; do not mechanically discard the first two based on the old captures. Capture-entry partial cadence is harmless here. Later 22-25 ms zone rebuilds, including early ones, have real measured maintenance work and are not discarded as startup artifacts. No capture stopped for capacity and there is no incomplete final-frame attribution.

Independent cross-checks: all category active counts, invocation counts and totals match all frame rows; every slow row matches its frame inclusive/exclusive duration, calls and logic IDs. All per-kind summary sample counts, totals, means, p50/p95/p99 and maxima match paths.csv. Every path frame/logic ID and parent link is valid, inclusive minus immediate child durations matches self within CSV precision, and all parent counters cover immediate child counters. No cross-check errors. Maximum category-self partition discrepancy versus outer time is A <0.0001, B1 <0.0001, B2 0.0003, B3 0.0005 ms. Counters are inclusive: root totals or parent-minus-immediate-child work are used to avoid double counting. No sum of enclosing category/path inclusive values is used as a total.

### Workload scaling and individual versus aggregate costs

| Metric | A | B1 | B2 | B3 |
| --- | --- | --- | --- | --- |
| Queued valid AI dispatches | 517 | 2829 | 2844 | 1182 |
| Request entries (findPath) | 431 | 2579 | 2668 | 798 |
| Internal detailed calls | 431 | 2579 | 2668 | 798 |
| Ground / hierarchical calls | 3 / 484 | 15 / 2660 | 6 / 3080 | 12 / 1237 |
| Closest / attack calls | 6 / 46 | 26 / 80 | 155 / 259 | 176 / 276 |
| Queue released-cell count | 14,765 | 179,954 | 3,442,192 | 3,206,235 |
| Released cells / dispatch | 28.56 | 63.61 | 1210.33 | 2712.55 |
| All-root head pops | 11,943 | 212,730 | 4,327,248 | 4,316,886 |
| All-root info attempts / new / failed | 51,068 / 13,266 / 0 | 3,190,325 / 169,080 / 0 | 80,778,964 / 3,357,518 / 33,270 | 77,137,616 / 3,128,315 / 30,783 |
| All-root insertions / forward hops | 19,732 / 466,590 | 308,477 / 13,230,431 | 4,743,690 / 372,233,907 | 4,634,224 / 335,679,759 |
| Hops / insertion (all roots) | 23.65 | 42.89 | 78.47 | 72.43 |
| All-root block-zone queries / hierarchy fallbacks | 7,456 / 68 | 108,955 / 598 | 183,570 / 1,446 | 73,694 / 374 |
| Queue depth max | 10 | 14 | 20 | 17 |
| Internal self total ms | 3.9375 | 177.3140 | 5405.7997 | 4926.2122 |
| Internal calls >10 / >30 ms | 0 / 0 | 8 / 0 | 133 / 129 | 129 / 119 |
| Ground self total ms | 2.9512 | 52.5969 | 41.1881 | 7.6425 |
| Closest self total ms | 0.7694 | 9.4034 | 363.1709 | 495.4677 |
| Zones count / total ms | 4 / 13.7138 | 5 / 113.3569 | 5 / 122.0474 | 4 / 95.7441 |
| Update modules / object visits / drawable updates | 519,468 / 571,805 / 1,158,726 | 1,280,070 / 877,808 / 1,760,691 | 1,945,100 / 1,207,199 / 2,579,659 | 2,070,353 / 1,236,540 / 2,619,352 |

The queue cell metric is released open/closed-list entries, not unique examined nodes or all allocations. Head pops may revisit reopened nodes; info_attempts includes reuse of already attached info. Newly obtained records are not a unique world-cell count across the capture. There is no trustworthy unique-expansion counter or search-result cache hit/miss metric. All reverse-hop counters are zero. Retail-compatible pathfinding is enabled by default in GameDefines.h: putOnSortedOpenList selects forwardInsertionSortRetailCompatible while s_useFixedPathfinding is false; that function reuses its pre-existing cellCount. Zero reverse work is consistent with this retail route, but the captures do not explicitly log the fallback-mode flag and should not be described as a measured mode trace.

Relative to A, B2/B3 dispatch counts rise only 5.50x/2.29x, whereas queue released cells rise 233.13x/217.15x and internal self time rises 1372.90x/1251.10x (ratios reflect these different workloads, not an AI-count law). B3 has fewer internal calls than B1 (798 versus 2579), yet ~27.8x its internal self time. This directly rejects a simple explanation of only more uniformly cheap requests. B1 has eight internal calls >10 ms, all returning a path, plus two ground calls >10 ms. B2 has 133 internal calls >10 ms, and B3 has 129. These occupy 5311.729/4863.066 ms self, respectively, about 98.3%/98.7% of internal self time. There are also bursts and repeated requests, but each severe dispatch can already be expensive on its own.

All internal calls >10 ms in B1/B2/B3 have human=0, layer=1 (ground), surfaces=1 and radius=1. All have a request parent with one hierarchy fallback. B2/B3 have 126/115 such calls with info failures; internal outcomes overall are B2 2516 returned_path, 126 null_after_work, 26 null_before_work; B3 668 returned_path, 115 null_after_work, 15 null_before_work. A/B1 have no info failures anywhere and no internal null_after_work. The 30,000-record global cell-info pool is finite and getACellInfo returns null when its free list is empty. Failure counts demonstrate pool pressure during these searches, but do not measure how much of the pool was retained by obstacles/units or establish why the destination was not reached. Do not relabel all null_after_work as a proven topological no-path result; resource exhaustion can prune useful work.

Repeated expensive objects are particularly clear: IDs 1759 and 1629 each appear 27 times among B2 internal >10 ms, and IDs 1759, 2213 and 1629 each appear 28 times in B3 (2012 appears 27 times). This is repeated costly activity in one session, not evidence of a safe negative-result cache key. Destination/obstacle/occupancy state can change between requests. Altering retry cadence, pool size, hierarchy fallback or queue budget changes search results/tick behavior and is not an observer-only optimization.

### Individual operation ranking and frame correlation

**A: all path operations, largest self intervals (including maintenance)**

| ID / parent | Kind | Inclusive / self ms | Outer | Outcome |
| --- | --- | --- | --- | --- |
| 1080 / 1079 | zones | 3.4983 / 3.4983 | 693 | observed |
| 28 / 27 | zones | 3.4839 / 3.4839 | 17 | observed |
| 3133 / 3132 | zones | 3.3924 / 3.3924 | 2092 | observed |
| 5109 / 5108 | zones | 3.3392 / 3.3392 | 2998 | observed |
| 1427 / 0 | ground | 3.1950 / 2.7597 | 930 | returned_path |

**B1: all path operations, largest self intervals (including maintenance)**

| ID / parent | Kind | Inclusive / self ms | Outer | Outcome |
| --- | --- | --- | --- | --- |
| 3217 / 0 | ground | 42.1357 / 40.5998 | 421 | returned_path |
| 9015 / 9014 | zones | 23.9077 / 23.9077 | 1311 | observed |
| 12732 / 12727 | internal | 23.6010 / 23.1221 | 1873 | returned_path |
| 25627 / 25626 | zones | 22.9781 / 22.9781 | 3454 | observed |
| 1759 / 1758 | zones | 22.6648 / 22.6648 | 238 | observed |

**B2: all path operations, largest self intervals (including maintenance)**

| ID / parent | Kind | Inclusive / self ms | Outer | Outcome |
| --- | --- | --- | --- | --- |
| 23580 / 23575 | internal | 109.3780 / 108.8400 | 2908 | null_after_work |
| 18834 / 18829 | internal | 65.8649 / 65.3599 | 2411 | returned_path |
| 5545 / 5540 | internal | 61.7380 / 60.2081 | 963 | returned_path |
| 5952 / 5947 | internal | 54.8887 / 54.4626 | 1013 | null_after_work |
| 5535 / 5530 | internal | 54.8168 / 53.4054 | 962 | returned_path |

**B3: all path operations, largest self intervals (including maintenance)**

| ID / parent | Kind | Inclusive / self ms | Outer | Outcome |
| --- | --- | --- | --- | --- |
| 7778 / 7773 | internal | 64.0723 / 63.8590 | 2395 | null_after_work |
| 7750 / 7745 | internal | 63.5185 / 63.2646 | 2394 | null_after_work |
| 8143 / 8138 | internal | 61.0953 / 60.7179 | 2458 | null_after_work |
| 7794 / 7789 | internal | 58.0831 / 57.8733 | 2396 | null_after_work |
| 6622 / 6617 | internal | 45.5177 / 45.2381 | 1952 | null_after_work |

Inclusive rankings contain enclosing queue/dispatch/request intervals and must not be interpreted as additional work. IDs are local to each capture. The following tables rank the largest individual operations by inclusive duration, then individual search functions by exclusive duration. Self time includes uninstrumented subroutines, CPU preemption and observer work; it is not a timer exclusively around open-list insertion.

**A: largest individual operations (inclusive ranking)**

| ID / parent | Kind | Inclusive ms | Self ms | Outer | Outcome |
| --- | --- | --- | --- | --- | --- |
| 1079 / 0 | queue | 3.5002 | 0.0019 | 693 | observed |
| 1080 / 1079 | zones | 3.4983 | 3.4983 | 693 | observed |
| 27 / 0 | queue | 3.4870 | 0.0031 | 17 | observed |
| 28 / 27 | zones | 3.4839 | 3.4839 | 17 | observed |
| 3132 / 0 | queue | 3.3941 | 0.0017 | 2092 | observed |
| 3133 / 3132 | zones | 3.3924 | 3.3924 | 2092 | observed |

**A: individual searches (self ranking)**

| ID / parent | Kind / object | Inclusive / self ms | Pops / inserts | Forward hops | Info attempts / new / failed | Cleaned cells | Outcome |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1427 / 0 | ground / 0 | 3.1950 / 2.7597 | 1,618 / 3,029 | 180,194 | 12,559 / 1,836 / 0 | 1,848 | returned_path |
| 1440 / 1435 | internal / 595 | 1.1649 / 1.0832 | 1,809 / 2,429 | 59,121 | 20,518 / 1,253 / 0 | 1,259 | returned_path |
| 4297 / 4296 | attack / 252 | 0.4936 / 0.4789 | 846 / 912 | 39,086 | 845 / 839 / 0 | 915 | returned_path |
| 4257 / 4256 | attack / 247 | 0.4718 / 0.4541 | 503 / 568 | 18,423 | 513 / 508 / 0 | 570 | returned_path |
| 4879 / 4878 | closest / 697 | 0.4783 / 0.4383 | 514 / 561 | 13,702 | 1,923 / 386 / 0 | 458 | returned_closest |
| 4198 / 4197 | attack / 247 | 0.2744 / 0.2646 | 392 / 456 | 15,392 | 397 / 392 / 0 | 458 | returned_path |

**A: ten worst completed authoritative frames**

| Outer / logic before | Offset s | Logic / self ms | Search aggregate ms / calls | Queue ms / dispatches / cells | Nonoverlapping logic path roots ms |
| --- | --- | --- | --- | --- | --- |
| 1058 / 6185 | 17.621 | 6.3008 / 0.2341 | 0.0000 / 0 | 0.0046 / 1 / 0 | 0.0043 |
| 930 / 6121 | 15.488 | 5.9132 / 0.5033 | 4.3644 / 4 | 1.1745 / 1 / 1262 | 4.3694 |
| 693 / 6003 | 11.540 | 4.5804 / 0.2953 | 0.0000 / 0 | 3.5006 / 0 / 0 | 3.5002 |
| 17 / 5665 | 0.276 | 4.5657 / 0.3467 | 0.0000 / 0 | 3.4875 / 0 / 0 | 3.4870 |
| 2092 / 6702 | 34.849 | 4.3594 / 0.1509 | 0.0000 / 0 | 3.3943 / 0 / 0 | 3.3941 |
| 2998 / 7155 | 49.944 | 4.3443 / 0.2157 | 0.0000 / 0 | 3.3413 / 0 / 0 | 3.3408 |
| 427 / 5870 | 7.107 | 3.7982 / 2.8324 | 0.0000 / 0 | 0.0028 / 0 / 0 | 0.0021 |
| 999 / 6156 | 16.639 | 3.6985 / 2.7213 | 0.0000 / 0 | 0.0032 / 0 / 0 | 0.0021 |
| 93 / 5703 | 1.542 | 3.6268 / 2.2113 | 0.0234 / 2 | 0.0345 / 1 / 32 | 0.0566 |
| 3546 / 7429 | 59.076 | 3.4624 / 0.3648 | 0.0178 / 2 | 0.0297 / 1 / 18 | 0.0559 |

**B1: largest individual operations (inclusive ranking)**

| ID / parent | Kind | Inclusive ms | Self ms | Outer | Outcome |
| --- | --- | --- | --- | --- | --- |
| 3217 / 0 | ground | 42.1357 | 40.5998 | 421 | returned_path |
| 9014 / 0 | queue | 23.9118 | 0.0041 | 1311 | observed |
| 9015 / 9014 | zones | 23.9077 | 23.9077 | 1311 | observed |
| 12725 / 0 | queue | 23.6195 | 0.0014 | 1873 | observed |
| 12726 / 12725 | dispatch | 23.6181 | 0.0045 | 1873 | observed |
| 12727 / 12726 | request | 23.6136 | 0.0014 | 1873 | returned_path |

**B1: individual searches (self ranking)**

| ID / parent | Kind / object | Inclusive / self ms | Pops / inserts | Forward hops | Info attempts / new / failed | Cleaned cells | Outcome |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 3217 / 0 | ground / 0 | 42.1357 / 40.5998 | 15,327 / 29,630 | 3,376,808 | 120,508 / 14,134 / 0 | 14,309 | returned_path |
| 12732 / 12727 | internal / 632 | 23.6010 / 23.1221 | 15,084 / 18,114 | 1,177,636 | 343,194 / 8,192 / 0 | 8,294 | returned_path |
| 21995 / 21990 | internal / 980 | 21.8724 / 21.5164 | 18,604 / 20,792 | 666,463 | 541,578 / 6,206 / 0 | 6,347 | returned_path |
| 12722 / 12717 | internal / 755 | 15.1444 / 14.8561 | 11,702 / 13,969 | 761,998 | 205,608 / 5,814 / 0 | 5,890 | returned_path |
| 23812 / 23807 | internal / 1118 | 14.9442 / 14.6650 | 12,314 / 14,310 | 399,547 | 388,250 / 4,974 / 0 | 5,120 | returned_path |
| 22014 / 22009 | internal / 979 | 13.8522 / 13.6501 | 13,747 / 15,614 | 369,910 | 400,567 / 4,653 / 0 | 4,767 | returned_path |

**B1: ten worst completed authoritative frames**

| Outer / logic before | Offset s | Logic / self ms | Search aggregate ms / calls | Queue ms / dispatches / cells | Nonoverlapping logic path roots ms |
| --- | --- | --- | --- | --- | --- |
| 421 / 8101 | 7.005 | 44.2501 / 0.4980 | 42.2816 / 12 | 0.1773 / 5 / 304 | 42.3128 |
| 1668 / 8723 | 27.783 | 26.5356 / 0.4854 | 0.0174 / 4 | 0.0429 / 2 / 8 | 0.1017 |
| 1873 / 8826 | 31.217 | 26.1964 / 0.4564 | 23.6055 / 2 | 23.6197 / 1 / 8297 | 23.6195 |
| 1311 / 8545 | 21.835 | 25.9049 / 0.3815 | 0.0000 / 0 | 23.9121 / 0 / 0 | 23.9118 |
| 238 / 8010 | 3.956 | 25.2317 / 0.4710 | 0.0000 / 0 | 22.6746 / 0 / 0 | 22.6744 |
| 3454 / 9615 | 57.541 | 24.7051 / 0.4119 | 0.0000 / 0 | 22.9812 / 0 / 0 | 22.9808 |
| 1994 / 8886 | 33.217 | 24.3103 / 0.4212 | 0.0000 / 0 | 22.0067 / 0 / 0 | 22.0063 |
| 2733 / 9255 | 45.528 | 24.0950 / 0.4494 | 0.0000 / 0 | 21.8063 / 0 / 0 | 21.8060 |
| 2772 / 9274 | 46.177 | 23.9370 / 0.4382 | 21.9041 / 8 | 21.9353 / 4 / 6370 | 21.9349 |
| 1872 / 8825 | 31.181 | 19.1292 / 0.6032 | 17.0141 / 8 | 17.0440 / 4 / 7975 | 17.0436 |

**B2: largest individual operations (inclusive ranking)**

| ID / parent | Kind | Inclusive ms | Self ms | Outer | Outcome |
| --- | --- | --- | --- | --- | --- |
| 23567 / 0 | queue | 143.2390 | 0.0017 | 2908 | observed |
| 23568 / 23567 | dispatch | 143.2370 | 0.0168 | 2908 | observed |
| 23575 / 23568 | request | 109.3850 | 0.0022 | 2908 | null_after_work |
| 23580 / 23575 | internal | 109.3780 | 108.8400 | 2908 | null_after_work |
| 18813 / 0 | queue | 69.8724 | 0.0019 | 2411 | observed |
| 18823 / 18813 | dispatch | 69.5339 | 0.0277 | 2411 | observed |

**B2: individual searches (self ranking)**

| ID / parent | Kind / object | Inclusive / self ms | Pops / inserts | Forward hops | Info attempts / new / failed | Cleaned cells | Outcome |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 23580 / 23575 | internal / 4594 | 109.3780 / 108.8400 | 54,375 / 60,917 | 3,791,022 | 1,897,437 / 16,543 / 439 | 16,714 | null_after_work |
| 18834 / 18829 | internal / 4286 | 65.8649 / 65.3599 | 23,579 / 29,177 | 2,723,774 | 1,389,177 / 13,352 / 0 | 13,380 | returned_path |
| 5545 / 5540 | internal / 5888 | 61.7380 / 60.2081 | 39,319 / 43,470 | 1,855,079 | 1,124,246 / 14,049 / 0 | 14,215 | returned_path |
| 5952 / 5947 | internal / 4114 | 54.8887 / 54.4626 | 30,097 / 33,302 | 2,640,634 | 748,333 / 16,413 / 230 | 16,605 | null_after_work |
| 5535 / 5530 | internal / 5358 | 54.8168 / 53.4054 | 33,150 / 36,894 | 1,755,092 | 986,115 / 13,306 / 0 | 13,474 | returned_path |
| 5908 / 5903 | internal / 5081 | 51.7441 / 51.5223 | 30,112 / 33,327 | 2,562,194 | 774,093 / 16,431 / 252 | 16,602 | null_after_work |

**B2: ten worst completed authoritative frames**

| Outer / logic before | Offset s | Logic / self ms | Search aggregate ms / calls | Queue ms / dispatches / cells | Nonoverlapping logic path roots ms |
| --- | --- | --- | --- | --- | --- |
| 2908 / 22561 | 49.111 | 147.6280 / 0.5193 | 109.3980 / 4 | 143.2400 / 1 / 35778 | 143.2390 |
| 962 / 21645 | 16.073 | 99.1408 / 0.8571 | 96.1744 / 4 | 54.8422 / 1 / 13479 | 96.1927 |
| 2411 / 22329 | 40.726 | 75.5198 / 0.6094 | 66.1931 / 5 | 69.8727 / 2 / 16266 | 69.8724 |
| 1013 / 21669 | 17.133 | 67.2482 / 0.5091 | 54.9575 / 8 | 63.4263 / 3 / 25597 | 63.4261 |
| 963 / 21646 | 16.177 | 66.3100 / 0.5855 | 61.7537 / 2 | 61.7676 / 1 / 14220 | 61.7674 |
| 1012 / 21668 | 17.054 | 65.0829 / 0.6129 | 51.7670 / 4 | 60.0731 / 1 / 25057 | 60.0727 |
| 982 / 21655 | 16.545 | 65.0607 / 1.0629 | 54.7519 / 12 | 58.0244 / 2 / 24072 | 61.3290 |
| 1070 / 21696 | 18.133 | 61.4395 / 0.5182 | 49.6808 / 4 | 57.0301 / 1 / 23834 | 57.0298 |
| 1014 / 21670 | 17.207 | 58.8257 / 0.5133 | 48.2192 / 6 | 56.2719 / 2 / 24091 | 56.2717 |
| 1169 / 21742 | 19.830 | 58.0386 / 0.6409 | 46.1295 / 4 | 53.8062 / 2 / 26179 | 53.8059 |

**B3: largest individual operations (inclusive ranking)**

| ID / parent | Kind | Inclusive ms | Self ms | Outer | Outcome |
| --- | --- | --- | --- | --- | --- |
| 7771 / 0 | queue | 89.8539 | 0.0016 | 2395 | observed |
| 7772 / 7771 | dispatch | 89.8523 | 0.0063 | 2395 | observed |
| 7743 / 0 | queue | 89.3075 | 0.0013 | 2394 | observed |
| 7744 / 7743 | dispatch | 89.3062 | 0.0535 | 2394 | observed |
| 8127 / 0 | queue | 86.3973 | 0.0019 | 2458 | observed |
| 8137 / 8127 | dispatch | 86.3515 | 0.0770 | 2458 | observed |

**B3: individual searches (self ranking)**

| ID / parent | Kind / object | Inclusive / self ms | Pops / inserts | Forward hops | Info attempts / new / failed | Cleaned cells | Outcome |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 7778 / 7773 | internal / 7781 | 64.0723 / 63.8590 | 45,151 / 49,430 | 2,253,631 | 1,132,668 / 16,299 / 384 | 16,468 | null_after_work |
| 7750 / 7745 | internal / 8209 | 63.5185 / 63.2646 | 47,089 / 51,338 | 2,282,718 | 1,142,837 / 16,321 / 404 | 16,463 | null_after_work |
| 8143 / 8138 | internal / 8209 | 61.0953 / 60.7179 | 40,836 / 45,147 | 2,334,617 | 1,020,381 / 16,458 / 394 | 16,619 | null_after_work |
| 7794 / 7789 | internal / 7394 | 58.0831 / 57.8733 | 37,926 / 41,811 | 2,276,529 | 961,322 / 16,301 / 381 | 16,478 | null_after_work |
| 6622 / 6617 | internal / 2213 | 45.5177 / 45.2381 | 27,547 / 29,419 | 1,874,858 | 620,955 / 16,399 / 271 | 16,655 | null_after_work |
| 1368 / 1363 | internal / 2213 | 45.4709 / 45.0439 | 28,436 / 30,390 | 1,931,403 | 640,937 / 16,760 / 320 | 16,990 | null_after_work |

**B3: ten worst completed authoritative frames**

| Outer / logic before | Offset s | Logic / self ms | Search aggregate ms / calls | Queue ms / dispatches / cells | Nonoverlapping logic path roots ms |
| --- | --- | --- | --- | --- | --- |
| 2394 / 24918 | 40.053 | 94.8253 / 0.9189 | 63.5372 / 3 | 89.3078 / 1 / 33067 | 89.3075 |
| 2395 / 24919 | 40.164 | 93.2796 / 0.6580 | 64.0858 / 3 | 89.8542 / 1 / 32937 | 89.8539 |
| 2458 / 24949 | 41.336 | 92.2330 / 0.9202 | 61.1406 / 5 | 86.3975 / 2 / 33295 | 86.3973 |
| 2396 / 24920 | 40.264 | 87.6656 / 0.6792 | 58.1030 / 3 | 84.1986 / 1 / 32957 | 84.1983 |
| 1952 / 24709 | 32.684 | 59.2269 / 0.7347 | 45.5346 / 4 | 52.8744 / 1 / 25113 | 52.8740 |
| 1184 / 24343 | 19.771 | 56.8572 / 0.5972 | 44.7430 / 6 | 50.9417 / 2 / 25133 | 50.9415 |
| 3364 / 25380 | 56.683 | 55.9341 / 1.2333 | 45.0721 / 4 | 51.0796 / 1 / 25193 | 51.0794 |
| 152 / 23855 | 2.486 | 55.4820 / 0.7590 | 44.9494 / 4 | 50.9947 / 1 / 25354 | 50.9944 |
| 411 / 23977 | 6.830 | 55.3854 / 0.6511 | 45.4882 / 4 | 51.8036 / 1 / 25448 | 51.8028 |
| 2854 / 25136 | 48.012 | 55.0297 / 0.5952 | 44.2752 / 4 | 50.5138 / 1 / 25151 | 50.5134 |

A frame 1058 (logic 6.3008 ms) is dominated by object loops, not paths. A frame 930 has a 3.1950 ms ground search plus 1.1649 ms internal search. Its ground root ID1427 has no object/request identity, diameter=6, crusher=0, raw coordinates (1309.61,1764.18)->(1997.89,1456.88), source mask4107 (Outer+Engine+Logic+PathSearch). Ground root metadata layer/human=-1 means unavailable, not wall/AI status. The external caller audit identifies AIGroup::friend_computeGroundPath. Children are flags 0.0039, hierarchy null_before_work 0.0036, flags 0.0005, reconstruction 0.4153 and cleanup 0.0120 ms; the ground interval self is 2.7597 ms. Thus detailed work, not hierarchy timing, dominates this control search.

B1 frame421, logic 44.2501 ms: root ground ID3217 costs 42.1357 ms inclusive / 40.5998 self. Diameter=6, crusher=0, raw coordinates (518.183,3397.9)->(958.683,1266.55); source4107 again attributes it to authoritative logic outside the queue. The hierarchy child ID3219 returns null_after_work in 0.0096 ms; fallback sets all passable. Reconstruction is 1.0140 ms and cleanup 0.5045 ms; flags total 0.0078 ms. The ground operation contributes nearly all the stall. Queue time in this frame is only 0.1773 ms despite five cheap dispatches. B1 internal ID12732 separately spends 23.6010 ms (23.1221 self) in frame1873 under queue12725->dispatch12726->request12727. Expensive ground and queued-object routes both occur; a queue-only fix misses the first.

B2 frame2908, logic 147.6280 ms: queue23567=143.239 ms, depth1->0, contains one dispatch23568=143.237 ms. Its sequential children are attack23569=2.8225 ms (null_after_work), request23575=109.385 ms (null_after_work), and closest23582=31.0126 ms (returned_closest), plus dispatch self 0.0168 ms. These siblings may be added; the request and its internal child may not. Internal23580=109.378 ms /108.840 self; its only detail child is cleanup23581=0.5379 ms. Object4594, human0, ground layer1, surfaces1, radius1, crusher1; raw coordinates (1367.01,4274.63)->(355,2355), zone_rejected0. Source7243 includes Logic+AIGlobal+PathQueue+PathRequest+PathSearch (plus outer/engine), not Client. Request hierarchy23577 takes 0.0019 ms, returns null_after_work and triggers one fallback. Closest reconstruction takes 0.1660 ms and its cleanup 0.2986 ms. This is one compound expensive dispatch, not 143 ms of queue bookkeeping. The queue releases 35778 cells in this frame, overshooting 5000 because the budget is checked between dispatches. Frame962 additionally combines a 54.8168 ms internal call with a 41.3502 ms ground call; path_search aggregate 96.1744 ms is not either individual duration.

B3 frame2394, logic 94.8253 ms: queue7743=89.3075 ms, depth5->4; dispatch7744=89.3062 ms contains request7745=63.5376 ms, internal7750=63.5185 ms /63.2646 self (null_after_work), then closest7752=25.6597 ms /25.2456 self (returned_closest), plus small move-away siblings. Object8209, raw coordinates (518.183,3397.9)->(795,1545), ground/surfaces1/radius1/human0/crusher1. Internal cleanup is 0.2539 ms. Request hierarchy fallback is followed by the full detailed search. This frame releases 33067 queue cells. Next frame2395, logic 93.2796 ms, queue7771=89.8539 ms, depth4->3: request7773=64.0877 ms contains internal7778=64.0723 /63.8590 ms, followed by closest7780=25.7583 ms. Object7781, raw coordinates (554.538,3394.83)->(825,1545); same context and source7243, zone_rejected0. Cleanup is 0.2133 ms; no reconstruction occurs in the failed internal call. Closest reconstruction is 0.1382 ms and cleanup 0.2382 ms. This pattern repeats in frame2458 and many ~40 ms failures. Queue depths <=20 across all windows rule out a full 512-slot queue as the measured cause of these tails; they do not measure order latency.

### What is established, and what is not

Current source anchors: Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp:1679 (retail insertion), :1841 (insertion dispatch), :6017 (queue), :6153 (line callback), :6243 (neighbor/line expansion), :6523 (internal search), :7115 (ground search), :9295 (line walker); GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate.cpp:397 (queued AI dispatch). These refer to the current uncommitted source and may move in future edits.

The code route is processPathfindQueue -> AIUpdateInterface::doPathfind -> findPath -> internalFindPath, commonly followed by findClosestPath; attack attempts can precede it. findGroundPath is a separate group route. In internalFindPath each open-head pop calls examineNeighboringCells, which can call iterateCellsAlongLine toward the goal using examineCellsCallback before examining eight local neighbors. The callback performs movement/zone/unit checks, calls allocateInfo even when info already exists, and can update/reopen/reinsert cells. It is not a read-only visibility test. findGroundPath similarly performs a goal-directed groundCellsCallback walk for each popped node. Therefore huge info-attempt counts can reflect repeated line scans of existing cells, not millions of new node allocations. B2 ID23580 has 1,897,437 attempts but only 16,543 new records and 54,375 head pops; B3 ID7778 has 1,132,668 attempts but 16,299 new records and 45,151 pops. Reopening is also possible, so pops can exceed cleaned-cell counts.

Simultaneously, sorted-list traversal is substantial and repeatable: B1 ground ID3217 has 3,376,808 forward hops /29,630 insertions; B2 internal23580 has 3,791,022 /60,917; B3 internal7778 has 2,253,631 /49,430. This is an excellent optimization candidate, but counts are not elapsed-time attribution. Movement checks, repeated line walks, reopen work and insertion execute inside the same self interval. No dedicated insertion or line-walk time exists. B1 and B2 each have a ~41-42 ms ground call, but B3 ground max is only 3.4304 ms; ground alone is not the repeatable severe B2/B3 mechanism. Internal expansion/line/list work repeats in all B windows; the pooled-failure/retry tail is especially repeatable in B2/B3.

Hierarchy preparation itself is tiny (individual maxima A 0.0658/B1 0.0971/B2 0.1357/B3 0.1110 ms). Fallback can broaden detailed search, but it also occurs for cheap successful searches and for all three A ground calls. These records do not show that disabling fallback or changing hierarchy answers is safe. Zone rebuilding is a separate repeatable 22-25 ms B operation, not hidden inside the 109 ms failed search. Zone flags max<=0.1443 ms, obstacle maintenance<=0.1201 ms, cleanup<=0.6691 ms and reconstruction<=2.0754 ms; none explains the principal individual tails. There is no measured result-cache behavior to optimize.

Search counts and work correlate strongly enough to reject pure OS/preemption noise as the general explanation: hundreds of expensive calls repeat with millions of operations, and paths account for >=80% of all 132 B2 logic frames >30 ms and 121/122 B3 such frames. OS scheduling can still enlarge any particular wall duration; QPC gives no on-CPU split. The remaining B3 frame is a compound path/object workload rather than proof of a different primary cause. Work counts alone cannot determine which loop consumes most cycles, whether missing the goal is primarily blocked topology versus pool exhaustion, or what percentage a list change would save.

### Ranked bottlenecks and alternatives

| Priority for authoritative stutter | Measured interval (max ms) | A | B1 | B2 | B3 | Interpretation |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Detailed/ground search loop self (mixed line, neighbors, list work) | 2.7597 | 40.5998 | 108.8400 | 63.8590 | Dominant severe tails; choose sub-operation only after CPU attribution. |
| 2 | Zone rebuild individual self | 3.4983 | 23.9077 | 24.7296 | 24.8624 | 4/5/5/4 events; real occasional maintenance, secondary to repeated 40-100 ms searches. |
| 3 | Object-update aggregate inclusive | 5.5579 | 4.9780 | 19.7495 | 8.0606 | Largest ordinary logic budget; B2 mean 3.448/B3 3.660 ms. Includes AI/weapon children. |
| 4 | AI player strategy inclusive | 0.9473 | 24.3874 | 4.1413 | 2.0063 | Independent 24.387 ms B1 outlier; old 31 ms finding remains real, less repeated than paths. |
| 5 | Render-end CPU wait/present inclusive | 18.2375 | 16.0305 | 12.2652 | 12.1480 | Baseline render cadence near 60 Hz; cannot cause time already measured inside GameLogic. |
| 6 | Weapon firing inclusive | 1.6128 | 6.7158 | 3.3277 | 1.2592 | Independent combat tail, old 20.889 ms not repeated here; A now has active combat too. |
| 7 | GameLogic exclusive residual | 2.8324 | 4.7815 | 4.6227 | 6.3534 | Unclassified work/preemption; does not dominate largest B frames. |
| 8 | Reconstruction individual self | 0.4153 | 2.0413 | 1.3480 | 2.0754 | Small relative to search loops; not the first target. |
| 9 | Cleanup individual self | 0.0120 | 0.5045 | 0.6691 | 0.6463 | Not the stall source despite large released-cell counts. |

These rows are alternative priorities, not disjoint totals. Object updates include AI-object work; AI-global includes queue work. AI-object maxima are 1.295/2.280/5.024/6.105 ms. Scene-view maxima 9.783/8.870/14.747/9.568 ms; drawable updates 0.381/0.497/0.729/1.090; particles 0.544/0.388/1.483/0.937. Rendering deserves a separate presentation-budget investigation if desired, but none of these supersedes path search for the demonstrated authoritative stalls. No network activity was instrumented in any capture, so multiplayer overhead is not assessed.

### Observer overhead and limitations

Use the prior non-inline synthetic medians conservatively: 79.596 ns/detail scope, 1.724 ns/counter probe, 58.875 ns/aggregate scope. Count root/self work, not inclusive ancestor repetitions; info attempts/new/fail, pops, insertions, block queries and fallbacks approximate leaf probe events. Add up to one hop-total probe per insertion and one cleaned-cell-total probe per cleanup. Retail traversal reuses its existing loop counter; there is no timed probe per list hop. Some paths have fewer probe calls than this model. Report/start I/O is outside measured outer frames.

| Set | Approx modeled path counters + detail + aggregate cost / 60s | Worst search counter estimate | Worst logic frame model |
| --- | --- | --- | --- |
| A | 12.031 ms | 0.038 ms | 0.006 ms |
| B1 | 33.027 ms | 0.361 ms | 0.383 ms |
| B2 | 204.377 ms | 3.604 ms | 3.843 ms |
| B3 | 197.749 ms | 2.230 ms | 2.437 ms |

This model excludes some generic frame-counter/bookkeeping work and is not a rigorous upper bound. It includes aggregate scopes in total/frame estimates, so it is not all incremental deep-profile overhead. Deep counters can materially add a few milliseconds in the million-attempt tails (B2 worst search estimate 3.604 ms of 109.378; B3 2.230 of 64.072). They cannot plausibly create all of the observed 40-109 ms searches under the synthetic model, but may inflate reported times by several percent and perturb instruction/cache behavior. Do not claim zero impact or use these estimates as corrected timings. B1 also shows many more detail records than B3 while being much cheaper: record count itself is not the tail cause. The original shallow Stage 4A had 172 ms logic/163 ms search aggregate without per-node deep counters, supporting a pre-existing problem, but that was a different battle/time and is not a paired overhead experiment. No matching shallow/deep workload or CPU trace was supplied. Therefore this task cannot prove in-game overhead quantitatively; the next trace should expose profiler instructions alongside game work.

### Stage 4B selection gate and minimum next measurement

Do not select a heap replacement, enable s_useFixedPathfinding globally, increase the pool, reduce retries, change hierarchy fallback, split searches or adjust the 5000-cell budget from these CSVs. Retail forward insertion stops at 5000 traversed cells and uses <= tie handling plus dangling-link guards; enabling the existing reverse-sort/fixed mode changes more than speed and is not demonstrated equivalent. A generic heap can change equal-cost expansion order and, with the traversal cap, even the actual legacy list order. These changes can alter paths, movement, combat and subsequent lockstep state. Negative-result caching/retry suppression can become stale with changing occupancy and resources. Save/replay/multiplayer compatibility is untested by these single-player captures; no save-format or authoritative-state change is justified.

**Minimum: one additional B-busy window on the unchanged candidate with synchronized WPR CPU sampling and scheduling stacks. No new A capture, new game-code probe or replacement instrumentation framework is required.** WPR and WPA are installed locally; read-only `wpr -profiles`, `wpr -help start` and `wpr -profiledetails CPU` confirm CPU.Verbose includes SampledProfile, CSwitch, ReadyThread and associated stacks. Those help queries were the only WPR commands executed in this task; no recording was started. Use the existing complete guarded launch procedure below with candidate stage4a1-pathfinding-deep-final, both profiling flags and a new output directory build/performance/stage4a1-B-cpu. Retain the matching generalszh.pdb/EXE and staging manifest. In an elevated PowerShell terminal, once the busy battle is ready:

```powershell
$profileDir = 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\performance\stage4a1-B-cpu'
# Create this directory before the guarded launch and pass it to -performanceProfile.
# Inspect existing recording state; do not cancel another recording.
wpr.exe -status
wpr.exe -start CPU -filemode
# After successful start, schedule the existing deep capture and refocus within 8 seconds.
Start-Job -ArgumentList $profileDir -ScriptBlock {
    param($dir)
    Start-Sleep -Seconds 8
    Set-Content -LiteralPath (Join-Path $dir 'command.txt') -Value ('start B-cpu-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))
} | Out-Null
# Remain in the battle about 70 seconds; let the CSV window auto-stop.
# After returning to the terminal, stop only the recording started above.
wpr.exe -stop (Join-Path $profileDir 'B-cpu.etl')
```

Preserve the ETL plus all five CSV/TXT outputs, manifest and notes about orders/focus/settings. The WPR trace covers the same 60-second CSV window with a lead-in/out; exclude setup/focus time using capture_started_utc and capture offsets as approximate anchors, then locate the repeated long search stacks on the game thread. QPC offsets are not exported as an absolute ETW timestamp, so do not claim sub-millisecond event matching from UTC alone. Analyze both whole-window hotspot weights and repeated 40+ ms path bursts. In WPA load symbols from build/dev-runtimes/zh/stage4a1-pathfinding-deep-final/game (generalszh.pdb; GUID 5706bc8d-51e9-49fc-9096-a636d0fd56be age 19) and filter the actual generalszh.exe game process/thread. Use CPU Usage (Sampled) stacks to separate insertion from iterateCellsAlongLine/examineCellsCallback/groundCellsCallback/checkForMovement and other search work, including profiler probes; use scheduling data to distinguish on-CPU work from ready/wait/preemption intervals. Inline functions may appear under callers; unresolved or unusable x86 stacks are not evidence that a function is cheap. Inspect trace event-loss/stack quality before deciding.

Selection rule: choose exactly one behavior-preserving sub-operation only if repeatable costly on-CPU stacks support it. If sorted insertion dominates, the prospective target is forwardInsertionSortRetailCompatible/putOnSortedOpenList, preserving exact order, equal-cost ties, 5000-hop truncation, reinsertion/removal and crash/fallback guards; do not merely enable the fixed pathfinder. If repeated line/movement checking dominates, target that computation while preserving callback sequence, early-abort behavior, pool failures, costs, parents/reopens and obstacle/unit reads. The CSVs cannot pick between these directions yet. If WPR stacks are unusable, the fallback is only a proposed bounded sampled per-search phase accumulator distinguishing line/neighbor work from insertion, not per-cell records; review that probe separately instead of implementing it now.

For the selected future change, benchmark identical deterministic scripted/map/search scenarios and original versus candidate with the same profiling mode, compiler/FP settings and warmup. Include heavy failed/closest searches, long successful ground/internal paths, equal costs, reopened nodes, insertion-cap boundaries and pool exhaustion. Require identical ordered popped cells, costs, parent links, returned path nodes/outcomes, dispatch/tick order and end-state CRCs; compare old retail/fixed-fallback behavior explicitly. Then compare multiple manual busy windows (individual self tails, authoritative p95/p99/max, work counts, non-path regressions) with shallow profiling as well as deep. Replay, save/load continuation and multiplayer lockstep checks are acceptance gates, not assumed from unit tests. Keep logic rate, queue budget/order, RNG/FP arithmetic, retry/fallback policy, movement commands, path costs/ties, pool capacity and serialized state unchanged. No numeric speedup is promised before timing the sub-operation.

### Tooling acceptance and delivery verification

ACCEPT the existing Stage 4A/4A.1 bounded/default-OFF observer as baseline tooling: all four real windows are internally consistent, complete, capacity-safe and diagnostically useful; earlier x86 Release 184 direct tests/CTest 2/2 and runtime validation apply to this unchanged code. The tooling does not need to explain every downstream optimization before it can be accepted. Its known limits (wall versus CPU time, mixed search self, outcome/resource ambiguity, inclusive counters, metadata and lack of paired overhead capture) remain explicit. Retain the shallow gate and disabled path. Do not claim final Stage 4B readiness or remove the profiler because the top-level category is known. A subsequent CPU trace is an analysis input, not a prerequisite architecture refactor. Developer review may commit the tooling separately; this task does not stage or commit anything.

Reproducible read-only analyses: build/stage4a1-final-analysis.py/json/log and build/stage4a1-final-extra.py/json/log. Report tables were generated from those independently recomputed rows, not manually copied observations. Final verification rechecks all 20 capture fingerprints and whitespace. No source/test/runtime file was edited in this task. Full build/tests were not rerun for a documentation-only analysis.

Input SHA-256 fingerprints:

| Set / file | Rows (summary: lines) | SHA-256 |
| --- | --- | --- |
| A/20261008T182324Z-29824-1-frames.csv | 3602 | 503c25fb931633ba054b9d43b211fe196cd2a9e9c804f154df9312e4f3dcbbaf |
| A/20261008T182324Z-29824-1-paths.csv | 6112 | ce2c70d1684068863a4d938a8471e93c8d6963cf3d2b9710359a9e4e7f7cee6b |
| A/20261008T182324Z-29824-1-categories.csv | 52 | 694096a644c36edddf31cf7ef7581c048ab44192824825eaf641691a3e1941ca |
| A/20261008T182324Z-29824-1-slow.csv | 520 | 3211d70c4a277e09710f5e57aec5c66082f8c9fe4db8032f593e3c46fb4daccc |
| A/20261008T182324Z-29824-1-summary.txt | 77 | 0dc7b1cbb6c900b77e10ab406b5565d902091e46f3d0f3916e9f3dba7719a764 |
| B1/20261008T180518Z-8164-1-frames.csv | 3602 | f61af3bbe837c0e9be7007af862c94f8755031e227e9f3aba88c6febba65815d |
| B1/20261008T180518Z-8164-1-paths.csv | 26154 | 23da5e4eca36617ee6f6c7fe852f23edc67661005b852052ff1e835f082d61a5 |
| B1/20261008T180518Z-8164-1-categories.csv | 52 | 8301225d45b56018901a138120467420289e0c547a1c9cfe9e0fb9fdb5a38734 |
| B1/20261008T180518Z-8164-1-slow.csv | 520 | b5788ed3d19fe5106b1bb44e3f2998935014e1589e99c6e80276974d7d04e2f5 |
| B1/20261008T180518Z-8164-1-summary.txt | 77 | 652f0dc32f852f2a2a52a735c9508785d3ccc45c7540bd11a48d3b6c6ba297a4 |
| B2/20261008T181332Z-8164-2-frames.csv | 3556 | be6a565abc6303b7f9dd9ffd2aa0b02998ceaff413fd11378f9ba9350523f2a4 |
| B2/20261008T181332Z-8164-2-paths.csv | 28818 | 69eff672238759bac2ebc804b4ca4afc7167c523e5f6701c432766f8a6f7c973 |
| B2/20261008T181332Z-8164-2-categories.csv | 52 | 4670bc764ebae8cbefd3513cbd7af237da5478c5a8184759b90a4303de555322 |
| B2/20261008T181332Z-8164-2-slow.csv | 520 | 394f67af74a7774d6b086581438490fd2fc3fc6d5ac49cbb73c0fb26bdd41a08 |
| B2/20261008T181332Z-8164-2-summary.txt | 77 | f1720ade576b0a6f06340c2e0e3bb4ab40c25989f85f2eb7671680fa80f0e5a0 |
| B3/20261008T181539Z-8164-3-frames.csv | 3564 | e1aa924e9abf8fa7935a230dc43bdd6b85175737844f17d80f3aa17ef2a24b22 |
| B3/20261008T181539Z-8164-3-paths.csv | 12180 | ee7e71206d98e8e1db114e4a0a6f092b7a878d794fd65c58041dac8cfaf6d046 |
| B3/20261008T181539Z-8164-3-categories.csv | 52 | 161a3979e08dac375397cd298b80778b9f3c6506bfdeda473043d53a237934fc |
| B3/20261008T181539Z-8164-3-slow.csv | 520 | c32c8ce384075cb351bc5ba515c27aa16e02e139165766ea25ab8b3ed5078b3a |
| B3/20261008T181539Z-8164-3-summary.txt | 77 | 4a9baf9ee5740471d88931b9f737dacee1a05f7d6f7d9728562629cb291d6f5f |

**Stopped for developer review. Verdict B: collect the single synchronized CPU-stack/scheduling window above; no Stage 4B implementation is authorized by this analysis.**

## Historical implementation record and original Stage 4A analysis
Decision B: Stage 4A.1 is necessary. Existing captures identify pathfinding as the dominant measured contributor to several severe B logic stalls, but do not select a specific safe Stage 4B optimization. No optimization, gameplay change, threading, game launch, commit or push is part of this work.

Started on dev/modern-engine at 1de7e65d173104ad2f288ac0a379e102a5057ebc with the preceding Stage 4A changes still uncommitted. Those changes were preserved. The user supplied actual developer captures in build/performance/stage4a-A and stage4a-B. Both summaries, every frame/category row and every slow-sample row were read directly. No manually copied summary was used. Analysis helpers/results and validation logs are ignored under build.

## Direct capture evidence

| Metric (raw capture) | A: 1 AI | B: 5 AI | B/A |
| --- | ---: | ---: | ---: |
| Outer frames | 3483 | 3471 | 0.997 |
| Completed ticks | 1720 | 1711 | 0.995 |
| Update modules | 324702 | 1450463 | 4.467 |
| Object visits | 454969 | 951357 | 2.091 |
| Drawable updates | 910173 | 1909102 | 2.098 |
| Queued dispatches | 375 | 2436 | 6.496 |
| Queue released cells | 7906 | 275832 | 34.889 |
| Request entry calls | 358 | 2318 | 6.475 |
| Search entry calls | 712 | 4432 | 6.225 |
| Queue total ms | 23.2677 | 646.161 | 27.771 |
| Search total ms | 5.3535 | 594.731 | 111.092 |
| Instrumented actual-path reconstruction total ms | 1.0128 | 14.6556 | 14.470 |
| Released cells / queued dispatch | 21.0827 | 113.232 | 5.371 |

Worst ten completed logic frames in A (milliseconds):

| Outer | Logic before | Offset s | Logic | Search aggregate | Search calls | Queue | Queue cells | Logic minus search* |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 886 | 1765 | 16.736 | 26.1528 | 0.0482 | 2 | 0.0539 | 125 | 26.1046 |
| 1048 | 1846 | 19.439 | 13.4594 | 0.0251 | 2 | 0.0316 | 65 | 13.4343 |
| 2831 | 2737 | 49.144 | 13.0887 | 0.0000 | 0 | 0.0006 | 0 | 13.0887 |
| 3197 | 2920 | 55.241 | 7.1480 | 0.0000 | 0 | 0.0005 | 0 | 7.1480 |
| 2832 | 2738 | 49.173 | 5.9216 | 0.0666 | 2 | 0.0726 | 172 | 5.8550 |
| 2161 | 2402 | 37.980 | 4.9404 | 0.0000 | 0 | 0.0006 | 0 | 4.9404 |
| 606 | 1625 | 12.071 | 4.7742 | 0.0000 | 0 | 0.0007 | 0 | 4.7742 |
| 2815 | 2729 | 48.878 | 4.5527 | 0.0000 | 0 | 0.0004 | 0 | 4.5527 |
| 3431 | 3037 | 59.141 | 4.3265 | 0.0000 | 0 | 3.1476 | 0 | 4.3265 |
| 2487 | 2565 | 43.412 | 3.9047 | 0.0000 | 0 | 0.0476 | 20 | 3.9047 |

Worst ten completed logic frames in B (milliseconds):

| Outer | Logic before | Offset s | Logic | Search aggregate | Search calls | Queue | Queue cells | Logic minus search* |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 285 | 8158 | 6.698 | 172.4360 | 163.2870 | 5 | 94.1932 | 28217 | 9.1490 |
| 1029 | 8529 | 19.229 | 83.1705 | 72.0478 | 3 | 81.3220 | 29910 | 11.1227 |
| 2769 | 9396 | 48.286 | 64.0364 | 58.7142 | 3 | 61.6692 | 24591 | 5.3222 |
| 2221 | 9123 | 39.164 | 45.5737 | 43.0668 | 2 | 43.0829 | 9857 | 2.5069 |
| 2220 | 9122 | 39.105 | 41.9903 | 38.7824 | 2 | 38.7962 | 9405 | 3.2079 |
| 212 | 8122 | 5.483 | 39.3443 | 37.7616 | 12 | 0.1853 | 501 | 1.5827 |
| 1381 | 8704 | 25.127 | 33.7026 | 0.0124 | 4 | 0.0329 | 6 | 33.6902 |
| 3213 | 9617 | 55.700 | 31.5777 | 26.8056 | 8 | 26.8897 | 8693 | 4.7721 |
| 1977 | 9001 | 35.061 | 26.8363 | 23.9727 | 2 | 23.9910 | 7267 | 2.8636 |
| 1602 | 8814 | 28.809 | 25.8902 | 23.1372 | 6 | 23.1690 | 7145 | 2.7530 |

*Logic minus search is a contextual difference, not GameLogic exclusive time; search can also be called from client code. In these worst B frames its source attribution must be assessed alongside the enclosing categories. The old records do not retain individual call parentage.

A steady outer mean/p50/p95/p99/max ms (excluding indices 0,1): 16.6498, 16.6556, 17.9293, 19.8285, 44.1390.
A completed logic mean/p50/p95/p99/max ms: 0.6543, 0.5615, 0.9330, 1.9861, 26.1528.

A worst logic frame contains 62 aggregate scope calls and 697 module/object/drawable counter increments. At a conservative 62 ns/scope and 2.1 ns/counter, their synthetic observer estimate is 0.005308 ms for the whole outer frame, excluding timer/loop bookkeeping not modeled by that estimate.

B steady outer mean/p50/p95/p99/max ms (excluding indices 0,1): 16.7141, 16.4855, 21.0036, 22.9133, 190.1540.
B completed logic mean/p50/p95/p99/max ms: 3.1117, 2.4817, 4.5934, 13.4448, 172.4360.

B worst logic frame contains 235 aggregate scope calls and 1864 module/object/drawable counter increments. At a conservative 62 ns/scope and 2.1 ns/counter, their synthetic observer estimate is 0.018484 ms for the whole outer frame, excluding timer/loop bookkeeping not modeled by that estimate.

Other B outliers: outer 1381 logic 33.7026 ms has search 0.0124 ms and player strategy 31.0507 ms; combat firing reaches 20.8892 ms in a different frame. Worst object-update aggregate is 5.3576 ms; AI-base 4.0101 ms; missile 0.1100 ms; particles 0.5938 ms. B frame 285 has client 16.7066 ms, object updates 1.0769 ms and player strategy 0.4204 ms. In A frame 886, logic self/residual is 25.7890 ms of 26.1528 ms; A frame 2831 instead spends 12.0593 ms in player strategy. None of these alternatives is silently reassigned to pathfinding.

Category sample/invocation counts and totals recomputed from frames match categories.csv; every slow-row inclusive value matches its referenced frame. Maximum discrepancy between summed category self times and outer time is 0.0004 ms in A and 0.0056 ms in B, consistent with CSV rounding. Both summaries report zero stack/clock errors, approximately 60 seconds, interpolation ON and cap 120. Only one A-combat and one B-combat window were present, not the complete idle/movement/precombat suite; no controlled matching workload or full hardware/settings log was supplied.

Input fingerprints (SHA-256; files were only read):

| File | SHA-256 |
| --- | --- |
| A/20261008T154809Z-19792-1-categories.csv | 00af251a17496ec17e2f81a74da77e61f2741052826ec1bededbc911d0875742 |
| A/20261008T154809Z-19792-1-frames.csv | 48add60946211bfdba5900fea11992bfe39439040d13803123256f3f000eac23 |
| A/20261008T154809Z-19792-1-slow.csv | cf2a050624cfe27c591da17299ba94bfa49a4b1abf9371fac00f6ddd228d9101 |
| A/20261008T154809Z-19792-1-summary.txt | bdd0e721eb3479b8df35a8da3c21b20a8621a4566afc6143dc217c704a788031 |
| B/20261008T160616Z-23436-1-categories.csv | aa67a17ef55a01811010eafc0b8270451c9ef571bf8df70f596d3e06b2a23c03 |
| B/20261008T160616Z-23436-1-frames.csv | 1617f1c87a4b02eb1b878e6271c7466e188e32b4c434683e5dbb5ffabf684ccb |
| B/20261008T160616Z-23436-1-slow.csv | f332eadd17f4e340b3452f824415bac5885f9659611f5778db99d61cdd0b6078 |
| B/20261008T160616Z-23436-1-summary.txt | d63dc3404e3d97f75912f63ffeb28bc223e3d92113c6310b9de759bd35d20893 |

The 163.287 ms path_search value is a per-outer-frame aggregate of five instrumented calls, with same-category nesting deduplicated inclusively. It is not an individual-search duration. In B frame 285 the queue is 94.1932 ms while all path_search intervals total 163.287 ms; searches also occur outside that queue interval. Do not add these overlapping times. The remaining logic time can include request/setup and other work as well as search cost. A's worst logic frame is mostly GameLogic self/residual time, so pathfinding does not explain every stall in either capture.

Five calls summing to 163.287 ms after inclusive deduplication give a conservative
lower bound of 32.6574 ms on at least one measured call; fewer outermost calls
would strengthen that bound. Thus the data already rule out only thousands of
uniformly cheap searches as the explanation of this particular frame. They still
cannot reveal which call/stage was slow or why. No claim of one 163 ms search is made.

Even the old path_search exclusive time is not pure node-loop execution: ground
and hierarchical reconstruction, cleanup and zone preparation were not separately
timed by Stage 4A. Its reconstruction category covered buildActualPath only.
Stage 4A.1 therefore isolates those phases rather than assuming every search
millisecond belongs to expansion or open-list insertion.

Queue cells are the existing count returned by releasing open/closed lists and added in cleanOpenAndClosedLists, despite the old exported name queue_cells_allocated. They are not universal unique allocations or expansions. Queue request counts count AI doPathfind dispatches, including dispatches that fail. Request/search entry counts can include hierarchical/detail stages and nested calls; division of aggregate milliseconds by calls cannot establish the maximum individual search.

A is labeled A-combat but records zero weapon-fire and missile-update calls in
these selected categories. B records active combat. This is a light-versus-busy
workload comparison, not a controlled experiment changing only AI-player count.
Map size, army mix, visibility and active orders can all affect the ratios.

Both captures begin with an approximately 1.9-second outer frame, entirely dominated by display-draw self time, zero scene-view/render-end calls and zero completed logic ticks, followed by a roughly 0.1-second render recovery frame. These are capture-entry/focus/device-transition candidates, not authoritative pathfinding stalls. Exact cause was not instrumented; retain the raw data, exclude indices 0 and 1 from steady gameplay outer/render comparisons, and do not extrapolate their cause to later stalls. Frame 285 and the other expensive B logic frames occur well after them. Removing these frames does not remove the pathfinding evidence. Requested/effective cap is 120, but observed steady outer cadence is around 60 Hz with substantial render-end wait; 120 is a cap, not achieved FPS or proof of a particular VSync mode.

Object-update and AI-base costs scale up with the larger battle, but are far below the worst search tails. A B logic frame with negligible search instead has a large player-strategy interval, and combat also has an independent firing outlier. These alternatives are retained in the evidence table rather than attributing all B stutter to paths. Client/display/scene/render-end work explains baseline frame budget consumption and some presentation tails; it does not account for the 172 ms GameLogic interval. CPU wall timings include scheduler preemption; no GPU execution times or CPU cycle accounting were collected.

Stage 4A observer cost cannot plausibly explain a 163 ms search aggregate with five category calls. A scope measured roughly 57-58 ns synthetically; even allowing all nested category calls in the entire worst frame gives a much smaller estimate, and Stage 4A had no per-node timing probes. Existing queue cells were copied once per queue update. Integer module/drawable/object counters add observer work, but their measured counts also do not approach the millions of timed scopes needed to synthesize that stall. This is an estimate, not a controlled in-game overhead measurement or proof against OS preemption. Report writing/start polling occur outside the outer interval; the first display stall cannot be labeled report-writing time from these data.

## Pathfinder audit before additional probes

The implementation is shared in Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp. findPath first performs a quick reachability test, runs a hierarchical prepass, then internalFindPath. findGroundPath also performs a hierarchical prepass. If hierarchy returns null, these callers set all zones passable and try the detailed search. Existing path_search covered internalFindPath, findGroundPath and internal_findHierarchicalPath, but not all closest/attack/safe/patch/move-away search loops. findClosestPath and other entry functions can do substantial work under request or other residuals.

The audited external findGroundPath caller in each title is
AIGroup::friend_computeGroundPath, which builds the group's shared ground path.
Thus a root ground detail row identifies that group-path route even without an
Object ID or request-context entry. Its hierarchy child is linked by parent ID.
The old capture cannot establish whether this route explains the search time
outside the queue; the new records test that hypothesis rather than assume it.

The queue is a 512-slot ring. PATHFIND_CELLS_PER_FRAME is 5000, checked between dispatches; it does not preempt a single search. Its budget is accumulated during cleanup. A single dispatch can therefore overshoot it, as shown by 24,591-29,910 released cells in the largest B frames. This is an observation about current behavior, not permission to change the budget or split/reorder simulation work.

Searches maintain intrusive sorted open and closed lists. putOnSortedOpenList selects existing retail forward insertion or the existing forward/reverse insertion implementation. Traversal can scale with list shape as well as node count. The source already contains reverse-insertion improvements; do not assume another list replacement is the correct fix. PathfindCell::allocateInfo may reuse an attached info record or request a pooled record; failures and retained obstacle/unit info are meaningful. These are cell-info reuse semantics, not a result cache.

Hierarchical traversal uses precomputed zone/block lookups, bridge/layer checks and processHierarchicalCell; the detailed algorithms examine movement/obstacle/unit constraints. calculateZones rebuilds zones when dirty, and the queue can return early after doing this maintenance. clearPassableFlags/setAllPassable traverse zone flags. Footprint/fence classification maintains obstacles. Reconstruction includes prepend/smoothing/line-of-sight work, and cleanup releases search lists. No general search-result cache with a trustworthy hit/miss boundary was found in these audited paths; no fake cache-hit counter is added. Block-zone query counts and cell-info reuse are explicitly labeled for what they measure.

## Minimum additional observer

Use both -performanceProfile <existing-absolute-directory> and -pathProfile. Without the second flag, the original aggregate profiler remains available in the same binary. With neither flag, there are no clocks, allocations or I/O from the added probes. -pathProfile alone does not activate recording. No key binding, game command, object field, serialized state or gameplay worker is added.

Each capture can retain at most 32,768 linked path intervals in a reserved vector. The existing 16,384 outer-frame and 60-second bounds still apply; reaching either record capacity stops capture at the next outer boundary with mode=capture_capacity. A partial final frame can drop additional path records; path_detail_dropped makes that explicit. Discard that last frame's detail for complete attribution if drops occurred. Capturing stops rather than letting an unbounded log grow. Recorder storage is released before engine allocator teardown, including error cleanup.

Individual kinds: request, internal, ground, hierarchical, closest, attack, safe, patch, move_away, queue, dispatch, reconstruction, cleanup, zones, zone_flags and obstacle. The three original path_search entry boundaries remain unchanged. New detail covers all nine selected request/search functions, all three reconstruction functions and the maintenance boundaries above; it is not a universal timer for every path query or collision helper.

Every detail row retains ID/parent ID, outer index, logic-before ID, capture offset, kind, request context, original aggregate source-category bitmask, object ID/layer where safely available, surface mask, human/crusher/closest policy where known, shape value, optional raw input coordinates, queue depth before/after where available, return outcome, zone rejection, inclusive and exclusive milliseconds. Unknown integer context is -1; object 0 means unavailable. The radius column is the ground path diameter for ground calls and the already-computed radius for detailed object calls; it is -1 when not captured. Coordinates are inputs, not a claim about the clipped/adjusted destination or canonical Object transforms. Metadata reads are guarded, including nullable input pointers.

Source-category bits use the existing Category enum: GameLogic=8, GameClient=16,
ObjectUpdates=32, AIGlobal=64, AIPlayer=128, AIObject=256, Movement=512 and
PathQueue=1024. Other bits remain defined in PerformanceProfile.h. A mask can
contain several enclosing categories. Queue depths are ring snapshots; dispatch
children count valid AI dispatches, not expired slots or unique movement orders.
An early queue return can leave queue_after=-1 (unobserved), rather than a stale
or invented value.

Cheap counters track open-list head pops (not necessarily expanded neighbors), info attempts/new records/failures, sorted-list insertions, forward/reverse list hops, cleaned cells, block-zone queries and hierarchy fallbacks. No QPC or output occurs in node loops. Existing retail sort counts are reused. Fixed sort traversal uses a local hop counter guarded by a per-call recording flag and adds the total once per insertion. Some leaf counters use a null TLS check; all counters affect profiler-owned integers only.

Counters propagate to parents and are explicitly inclusive. Do not sum parent and child counters; derive parent self counts by subtracting immediate children if needed. Durations are individual function intervals, with child detail durations subtracted from exclusive/self time. A parent ground/request interval may include a hierarchy child; use IDs and self time to distinguish the stages. These detail exclusive times and the original aggregate exclusive times use different sets of boundaries and should not be interchanged.

Return outcomes distinguish returned_path, returned_closest where an explicit closest-return branch exists, null_before_work and null_after_work; phase/queue rows use observed. Returned paths may be partial/fallback or attack/escape paths, so returned_path does not promise exact-goal success. Null after work includes exhaustion, work-limit exits and other failures; info_failed and zone_rejected help separate causes. No gameplay return expression or decision is replaced, and no extra search is executed for diagnostics. The observer does not pretend every failure is unreachable terrain. If next captures expose ambiguity at a particular exit, add one focused reason marker before choosing an optimization.

The fifth output, -paths.csv, contains all retained individual intervals. The existing four outputs remain, and summary.txt adds path_detail_enabled/records/dropped plus per-kind individual duration count/total/mean/p50/p95/p99/max. Use paths.csv for individual ranking and frames/slow CSVs for the authoritative-frame relationship. No string formatting or event-buffer growth occurs in searches. No new authoritative cache or result reuse is introduced.

## Validation and candidate

Windows x86 Release, win32 Ninja Multi-Config preset, MSVC 14.51.36231, /W3,
legacy profiling/Tracy options OFF. Configure and full build pass. Final CTest
passes 2/2, both Google binaries directly pass 92 tests each (184 total), and
both aggregate/deep-profile benchmarks pass for both titles. Eight new fake-clock
tests per title cover disabled/shallow gating, individual nested intervals and
exclusive accounting, bounded storage/drop handling, context inheritance,
return outcomes, counters/phase propagation, exception unwinding, reset safety,
CSV/empty statistics and source-category correlation. Tests do not require real
timing thresholds. A token audit strips observer additions and reproduces every
pre-Stage4A.1 pathfinder code token in order. All eight original A/B input files
remain byte-identical to their recorded SHA-256 fingerprints.

Changed/new-line warning audit: zero, with 76 existing warning occurrences across
the initial and corrected successful builds. The final benchmark-only full build
also passes without warnings. Git diff --check and separate untracked-file checks
cover new files; CRLF conversion notices are not whitespace defects. Logs under
build: stage4a1-configure.log, stage4a1-build.log, stage4a1-build-final.log,
stage4a1-build-benchmark-final.log, stage4a1-ctest.log,
stage4a1-ctest-final.log, stage4a1-ctest-benchmark-final.log,
stage4a1-g/z-direct.log, stage4a1-g/z-benchmark.log,
stage4a1-verification-final.log, stage4a1-warning-audit.log and
stage4a1-token-audit.log. Capture analysis is reproducible from
stage4a1-analyze.py / stage4a1-capture-analysis.json/log under build.

Final synthetic overhead, median of five repeats, approximate ns/operation:

| Operation | Generals | Zero Hour |
| --- | ---: | ---: |
| Disabled detail scope | 3.680 | 3.607 |
| Enabled detail scope including record initialization/two QPC reads | 79.596 | 77.319 |
| Disabled node-counter probe | 0.579 | 0.579 |
| Enabled node-counter probe | 1.724 | 1.708 |
| Existing aggregate enabled scope | 58.875 | 57.812 |

Scope probes use 16,384 iterations per repeat; counters and aggregate probes use
500,000. Non-inline harnesses prevent folding away probe calls and include their
call overhead. Initial tightly inlined counter measurements were 0.47-0.84 ns;
the final counter harness was made non-inline to reduce optimizer ambiguity.
These measurements do not benchmark real linked-list traversal/cache behavior;
enabled hop-count branches can add cost in very long scans and should be assessed
against the shallow gate if observers are material. Each detail sample is 208
bytes; 32,768 reserved records use 6,815,744 bytes (6.5 MiB), in addition to the
existing 11,403,264-byte frame buffer. No Object/Drawable or pathfinder authoritative
fields grow. Reporting uses temporary allocations after capture; no worker or
asynchronous output ordering is introduced.

Final fresh candidate: build/dev-runtimes/zh/stage4a1-pathfinding-deep-final/game.
Full read-only SHA-256 inventories PASS for the Steam source and final candidate (369 files each).
The previous stage4a-performance-baseline inventory also PASSes (369 files). All previous candidates and original captures are preserved.
Launch validation with both profiling flags and BackupUserData PASSes in ValidateOnly mode; no backup, receipt or game launch occurred.

| Identity | Value |
| --- | --- |
| EXE SHA-256 | 48463C4D86AF91BDA7BEC92EB35EFC23532E41FB3667BFD43BC2FA3B2AFB7193 |
| PDB SHA-256 | 6C93C1CD618CD130ABFF204B526465CF1ACE53C5E7C15B20A40BCE0486AB3CBA |
| Matching EXE/PDB GUID | 5706bc8d-51e9-49fc-9096-a636d0fd56be |
| Matching age | 19 |

Integrity/validation logs: build/stage4a1-integrity-source-complete.log, stage4a1-integrity-candidate-complete.log, stage4a1-integrity-previous.log and stage4a1-launch-validation-complete.log.
The manifest records source revision/dirty worktree, compiler/build identity and complete inventories. Runtime performance and the new path-detail file frontend still require the developer capture below. No Install, Steam/user-data writes, execution-policy changes, commit or push occurred.

The first configure/full build succeeded, but CTest failed one new test per title (91/92 passed): a substring test for ',inf' matched the legitimate info_attempts header. The test now checks whole CSV fields. Other new test cases passed initially. This intermediate failure is retained in build/stage4a1-ctest.log; no report value was altered to conceal it. No production runtime failure or game launch was involved.

The final whitespace audit found one trailing space inherited from a return line
that became instrumented. An initial patch matched a different repeated return
line, so that check failed again even though build/CTest/direct tests passed.
The exact flagged line 10436 was then corrected and git diff --check passed before
the final full build, CTest and direct tests. The token audit still passes.
Final logs: stage4a1-whitespace-fixed.log, stage4a1-build-complete.log,
stage4a1-ctest-complete.log, stage4a1-g-direct-complete.log,
stage4a1-z-direct-complete.log and stage4a1-stage-complete.log.
Earlier whitespace-attempt logs remain under their *-whitespace-final/*-final names.
Candidates stage4a1-pathfinding-profile and stage4a1-pathfinding-profile-final
remain intact as unlaunched intermediates. stage4a1-pathfinding-deep-final is freshly
staged only after the final whitespace/build/test checks pass. The benchmark
results above use the same code tokens before that whitespace change. No previous
runtime was overwritten, moved or deleted in this task.

Determinism review covers unchanged RNG, gameplay argument evaluation, search decisions, list ordering, queue budget/dispatch order, loops, transforms, command/network/CRC/save/replay state and floating-point settings. Added metadata/count branches only touch profiler state; normal paths and failure exits retain their original calls and return values. Wall-clock pacing can still be perturbed by observers and synchronous report output. Synthetic testing and token/source audit are not a runtime determinism proof. No optimization or architecture refactor was made.

## Exact next developer captures

From the repository root in PowerShell 7, after review. These commands are provided for the developer and were not used to launch a game by the agent. Preserve the existing A/B directories unchanged. Keep interpolation ON, a finite 120 cap if stable, and the same map/factions/difficulty/resolution/detail/VSync/wrapper/power settings used for comparison. Record settings and approximate armies/camera/match time alongside the reports. No requirement for perfectly identical gameplay is implied.

```powershell
$profileDir = 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\performance\stage4a1-B'
New-Item -ItemType Directory -Force -Path $profileDir | Out-Null
Set-Content -LiteralPath (Join-Path $profileDir 'command.txt') -Value ('idle ' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage4a1-pathfinding-deep-final -CheckSource
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage4a1-pathfinding-deep-final
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage4a1-pathfinding-deep-final -BackupUserData -GameArguments @('-groundInterpolation','-performanceProfile',$profileDir,'-pathProfile') -ValidateOnly
# Developer launch after reviewing validation:
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage4a1-pathfinding-deep-final -BackupUserData -GameArguments @('-groundInterpolation','-performanceProfile',$profileDir,'-pathProfile')
```

In a second terminal set the same absolute $profileDir, then set up the developer + 5 AI large-map battle. Schedule the command below, return focus to the game within the 8-second delay, and stay in the game until the capture ends. This avoids intentionally beginning while still switching focus. The background job writes only the explicitly named profiling command file, not user data. File polling adds up to one second of delay. Leave ample time (about 70 seconds) before returning to inspect files; capture may finish earlier at capacity.

```powershell
Start-Job -ArgumentList $profileDir,'B-busy' -ScriptBlock {
    param($dir,$label)
    Start-Sleep -Seconds 8
    Set-Content -LiteralPath (Join-Path $dir 'command.txt') -Value ('start ' + $label + '-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))
} | Out-Null
# After returning from gameplay:
Get-ChildItem -LiteralPath $profileDir
```

Capture at least two separate B-busy windows with fresh command nonces; preserve all five outputs per window. Also take a light 1-AI sanity window using the same commands with profileDir ending stage4a1-A and label A-light (exit/relaunch for the different output directory). Optional manual early stop uses the command below; do not confuse its focus-transition/report-writing stall with a gameplay stall:

```powershell
Set-Content -LiteralPath (Join-Path $profileDir 'command.txt') -Value ('stop manual-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))
```

Check path_detail_enabled=1, path_detail_records>0, stack_or_clock_errors=0, capture label, interpolation=1 and finite cap. path_detail_dropped should normally be 0; if nonzero, ignore incomplete last-frame detail and repeat a shorter window. mode=capture_capacity is a valid bounded early stop, not proof of a new stall. Exclude any obvious focus/device/capture-entry recovery frames from gameplay comparisons while retaining the raw outputs.

If deep profiling visibly changes behavior/performance, repeat a representative B window in the same candidate without -pathProfile (retain -performanceProfile and a separate directory). Compare aggregate tails and the synthetic observer estimates while acknowledging workload variability. Do not repeatedly re-test interpolation OFF; the accepted feature remains the primary configuration.

For the next analysis, rank individual internal/ground/hierarchical/closest/attack/safe/patch/move_away rows by inclusive and self duration; follow parent IDs to the request/dispatch/queue. Compare node counts and info failures, list hops per insertion/head pop, cleanup/reconstruction/zone children, hierarchy failures/fallbacks and queue depths. Distinguish bursts of cheap searches from one expensive search, and many expanded cells from disproportionate time per cell or list hop. If counts are ordinary but wall time is extreme, investigate preemption before claiming algorithmic work. Choose one Stage 4B change only after repeatable, complete samples show its specific cause; queue splitting, new budgets, caching and threading are not authorized by these data.

## Final worktree record

Stage 4A.1 edits are limited to PerformanceProfile.h/.cpp/Runtime.cpp, CommandLine.cpp, AIPathfind.cpp, test registration/benchmark/new PathProfileTest.cpp and these performance documents. Other listed game/client/AI instrumentation changes were already present from Stage 4A and were retained.

Exact status (26 tracked modified files plus eight untracked files, including inherited Stage 4A work):

```text
 M Core/GameEngine/CMakeLists.txt
 M Core/GameEngine/Source/Common/CommandLine.cpp
 M Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp
 M Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp
 M Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp
 M Generals/Code/GameEngine/Source/Common/GameEngine.cpp
 M Generals/Code/GameEngine/Source/GameClient/GameClient.cpp
 M Generals/Code/GameEngine/Source/GameLogic/AI/AI.cpp
 M Generals/Code/GameEngine/Source/GameLogic/AI/AIPlayer.cpp
 M Generals/Code/GameEngine/Source/GameLogic/Object/Locomotor.cpp
 M Generals/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate.cpp
 M Generals/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/MissileAIUpdate.cpp
 M Generals/Code/GameEngine/Source/GameLogic/Object/Weapon.cpp
 M Generals/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp
 M GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp
 M GeneralsMD/Code/GameEngine/Source/GameClient/GameClient.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AI.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AIPlayer.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Locomotor.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/MissileAIUpdate.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Weapon.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp
 M Tests/Google/Core/CMakeLists.txt
 M docs/modernization/AGENT_HANDOFF.md
 M docs/modernization/ROADMAP.md
?? Core/GameEngine/Include/Common/PerformanceProfile.h
?? Core/GameEngine/Source/Common/PerformanceProfile.cpp
?? Core/GameEngine/Source/Common/PerformanceProfileRuntime.cpp
?? Tests/Google/Core/GameEngine/Common/PathProfileTest.cpp
?? Tests/Google/Core/GameEngine/Common/PerformanceProfileBenchmark.cpp
?? Tests/Google/Core/GameEngine/Common/PerformanceProfileTest.cpp
?? docs/modernization/STAGE4A1_PATHFINDING_DEEP_PROFILE.md
?? docs/modernization/STAGE4A_PERFORMANCE_BASELINE.md
```

Tracked diff statistics (exclude untracked additions):

```text
 Core/GameEngine/CMakeLists.txt                     |   3 +
 Core/GameEngine/Source/Common/CommandLine.cpp      |  18 ++
 Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp | 267 ++++++++++++++-------
 .../Source/W3DDevice/GameClient/W3DDisplay.cpp     |   9 +-
 .../Source/W3DDevice/GameClient/W3DView.cpp        |   2 +
 .../Code/GameEngine/Source/Common/GameEngine.cpp   |  14 +-
 .../GameEngine/Source/GameClient/GameClient.cpp    |   8 +-
 .../Code/GameEngine/Source/GameLogic/AI/AI.cpp     |   2 +
 .../GameEngine/Source/GameLogic/AI/AIPlayer.cpp    |   2 +
 .../Source/GameLogic/Object/Locomotor.cpp          |   2 +
 .../Source/GameLogic/Object/Update/AIUpdate.cpp    |   2 +
 .../Object/Update/AIUpdate/MissileAIUpdate.cpp     |   2 +
 .../GameEngine/Source/GameLogic/Object/Weapon.cpp  |   3 +
 .../Source/GameLogic/System/GameLogic.cpp          |   8 +
 .../Code/GameEngine/Source/Common/GameEngine.cpp   |  14 +-
 .../GameEngine/Source/GameClient/GameClient.cpp    |   8 +-
 .../Code/GameEngine/Source/GameLogic/AI/AI.cpp     |   2 +
 .../GameEngine/Source/GameLogic/AI/AIPlayer.cpp    |   2 +
 .../Source/GameLogic/Object/Locomotor.cpp          |   2 +
 .../Source/GameLogic/Object/Update/AIUpdate.cpp    |   2 +
 .../Object/Update/AIUpdate/MissileAIUpdate.cpp     |   2 +
 .../GameEngine/Source/GameLogic/Object/Weapon.cpp  |   3 +
 .../Source/GameLogic/System/GameLogic.cpp          |   8 +
 Tests/Google/Core/CMakeLists.txt                   |   3 +
 docs/modernization/AGENT_HANDOFF.md                |  36 +++
 docs/modernization/ROADMAP.md                      |  34 +++
 26 files changed, 363 insertions(+), 95 deletions(-)
```

Complete current review patch, including untracked additions: build/stage4a1-review.patch.
