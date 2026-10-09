# Stage4B.1 — successful interactive validation and battle capture analysis

Stage4B.2 follow-up audit (2026-10-09): no production cache implemented because
exact reference equivalence is not established; see
[STAGE4B2_PATH_MOVEMENT_CACHE_AUDIT.md](STAGE4B2_PATH_MOVEMENT_CACHE_AUDIT.md).
The movement-reuse recommendation below is the historical proposal, not an
accepted implementation or proven cache-hit/performance claim.


Developer runtime review: **PASS for TEST1 Developer Mode and TEST2 battle preset**.
Analysis date2026-10-09. This report reads the exact battle run's receipts and every
row of all five reports directly. No source changes, game launch, build, commit or
push were performed for this analysis. Stage4A.3 debugging remains paused.

## Provenance and capture validity (items1–5)

Exact directory: `build/performance/dev-20261009T082332Z-c286325d`.
Exactly one report set, stem `20261009T082505Z-29724-1`:

- `20261009T082505Z-29724-1-summary.txt`
- `20261009T082505Z-29724-1-frames.csv`
- `20261009T082505Z-29724-1-slow.csv`
- `20261009T082505Z-29724-1-categories.csv`
- `20261009T082505Z-29724-1-paths.csv`

Matching isolated runtime receipt: `launch-0533331afb3d44dc9a38acf5d3724e26.json`;
matching `.json.exit.json` is normal exit0. Start08:23:47.0280264Z, exit08:25:17.3282439Z.
Arguments include -devMode -quickGame -groundInterpolation -pathProfile, the exact
performance output directory and -devPreset battle. Executable SHA256
`BDAA237D20078B50018C3862ED3849255A0A1B135118F52375714D186B49B5E4` matches the
prepared Stage4B.1 candidate; git0ff7f9c9b/dirty1 alone would not identify this binary.
Backup receipt Verified=true. No launch was performed by this analysis.

Capture started08:24:05Z, label developer-shortcut; report08:25:05Z. Duration60.0094s,
**mode=60_second_limit**, not developer_stop: the existing automatic bound stopped
this report before normal game exit. Developer confirmed capture controls worked;
recorded stop reason is authoritative. All3587 frames request/effectively cap120;
ground_interpolation=1. Actual throughput is59.774outer frames/s and29.629logic ticks/s;
a configured120cap does not imply120achieved FPS or changed30TPS policy.

Path detail enabled1,30375 records, dropped0; phase enabled1/stride64/errors0;
stack_or_clock_errors0.3587 outer rows,1778 completed logic ticks, tick310→2088,
one tick per completed-logic outer frame. No capacity truncation or incomplete final
frame requires exclusion. Final frame3586 starts59991.3ms and lasts18.0581ms, consistent
with bounded stop at the next outer boundary. All rows are retained.

Validation: contiguous unique path IDs1..30375; existing earlier parent in the same
frame; matching frame/tick links; parent/child and root/frame containment allowing
0.11ms CSV rounding; inclusive-minus-children equals self within duration rounding;
consecutive frame/tick progression; summary counts/counter totals and category totals
match CSVs. All520 slow rows (260each outer/completed-logic rankings) match frames.csv.
Exclusive category partition differs from outer inclusive by at most0.0003ms.
Selections=floor(own head pops/64), and own pops equal inclusive pops minus direct
children's pops for eligible kinds. Line/Neighbor span and sampled insertion counts
obey bounds; unsupported kinds have zero phases; phase self accounting is nonnegative.
No integrity failures remain. Offsets use six significant digits: apparent tiny
containment discrepancies at late offsets are formatting precision, not clock errors.

Capture starts in established gameplay, tick310. No map-loading/intro/device-sized
entry spike is apparent; path stalls recur across the full minute. Excluding the first
second still leaves60of64 Internal searches>10ms. Focus/device state is not explicitly
logged; unchanged caps and regular rendering are supporting evidence, not proof of
uninterrupted focus or absence of OS preemption. Timings are wall durations, not
hardware CPU cycles or GPU measurements.

## Logic stalls and individual searches (items6–12)

Percentiles use the profiler's nearest-rank convention. Logic statistics use1778
completed-logic frames, not all3587 outer frames with zero logic.

| Timing | Count | Total ms | Mean ms | p50 ms | p95 ms | p99 ms | Max ms |
|---|---:|---:|---:|---:|---:|---:|---:|
| GameLogic |1778|5899.4224|3.3180|2.0301|8.2369|27.8651|165.1720|
| Internal inclusive |770|1864.5432|2.4215|0.0104|16.9316|37.0285|156.2320|
| Internal self |770|1795.9186|2.3324|0.0065|16.6312|36.8359|155.8920|

Logic>30ms:15/1778(0.844%); >33.333ms13, >40ms9, >50ms5, >100ms2.
All15 are pathfinding-dominated (>50% by non-overlapping roots); actual shares92.58–98.96%.
Queue/root/search values below are nested alternative views and MUST NOT be summed.

| Outer | Logic ms | Path queue ms | PathSearch category ms | All path-root share |
|---|---:|---:|---:|---:|
|39|47.2312|45.4548|45.4325|96.34%|
|43|81.1955|78.7880|78.7655|97.10%|
|183|67.9457|64.7887|64.7621|95.35%|
|251|104.4540|102.4030|102.3840|98.07%|
|318|32.2433|30.4584|30.4441|94.57%|
|659|67.0593|64.7897|0.0369|96.66%|
|970|43.5989|41.6199|20.2379|95.46%|
|1294|48.5934|46.6853|46.6665|96.15%|
|1718|39.4583|37.2042|37.1836|94.39%|
|1899|165.1720|163.4270|156.3300|98.96%|
|1906|43.1739|41.2362|41.1955|95.65%|
|2196|38.5712|35.6326|35.6099|92.58%|
|3439|34.3238|32.0958|32.0806|93.63%|
|3462|30.2231|28.3781|28.3172|94.02%|
|3530|35.4194|33.7635|33.4732|95.43%|

Internal>10ms64; >30ms11; >40ms7. The64 expensive records account for1614.6662ms
self (89.91%of all Internal self), across31objects; they recur29/13/22 times in the
first/middle/final20seconds. All64 follow a failed hierarchical route in the Request
parent (fallback counter is on the parent, not the Internal row).56are AI and8human,
all ground layer/radius1. This substantially exercises the optimized line route:
202Internal records have actual sampled Line spans,19803selected iterations across
1276704own head pops. This does not count every callback or infer exact cache hits.

| Internal ID / object / outer | Inclusive/self ms | Pops | Info attempts/new/failures | Insertions | Forward hops | Outcome |
|---|---:|---:|---:|---:|---:|---|
|12738/297/1899|156.2320/155.8920|40,130|1,663,539/25,759/461|44,007|8,148,529|null_after_work|
|1378/419/251|102.3590/101.2440|71,087|2,853,220/11,586/0|76,216|1,710,106|returned_path|
|249/417/43|78.6992/77.3934|60,955|2,171,429/9,084/0|65,216|1,169,982|returned_path|
|768/230/183|64.7285/64.1769|42,562|1,184,315/9,272/0|46,554|2,016,368|returned_path|
|7339/218/1294|46.6076/45.9227|27,752|750,224/8,405/0|30,691|1,774,869|returned_path|
|218/402/39|45.3780/44.4148|36,605|1,181,001/7,618/0|40,249|853,697|returned_path|
|12882/422/1906|41.1046/40.6615|22,482|762,335/5,117/0|24,844|724,276|returned_path|

Worst chain, outer1899/tick1249/approximately31.8s, human object297:
Queue12725(163.427ms, depth7→6) → Dispatch12726(163.425ms) → Request12733(156.261ms,
hierarchy fallback1) → Internal12738(156.232inclusive/155.892self). Its cleanup child
is0.3404ms. Dispatch siblings: Attack2.5713ms before Request, Closest4.5009ms after
Request, MoveAway0.0098ms. GameLogic165.172ms and outer180.615ms show the stall's
end-to-end effect.461info allocation failures accompany null_after_work; do not call
this proof of unreachable terrain. Requests can fail/limit and then use ordinary Closest.

Internal outcomes:504returned_path,265null_before_work,1null_after_work. Request
outcomes:504returned_path,265null_after_work,1null_before_work. A Request's hierarchical
child has already done work when its Internal child returns before expansion; these
labels intentionally describe different scopes. Early Internal rejection can include
destination/zone/preflight reasons; this instrumentation does not identify which branch
for each row. Do not infer the paused harness defect from these ordinary battle requests.

There are373Closest operations:245returned_closest,128returned_path; no null outcome.
265dispatches have direct Request→Closest children,107Attack→Closest and one
Attack→Request→Closest→MoveAway. Closest totals193.6757ms inclusive, max64.7512ms
(ID3977, outer659), following a0.0321ms failed Request with a0.0006ms early-null Internal.
This explains the severe67.0593ms logic frame despite PathSearch category only0.0369ms:
Closest has its own path-detail interval and is not covered by the Internal PathSearch
category. Outer970 combines a20.2023ms Internal and21.3450ms Closest from another
request; most other severe frames contain one dominant Internal. Do not add their
inclusive ancestors. Returned_path does not guarantee exact-goal rather than partial
path behavior. These distributions are plausible under busy movement/combat, with
manual confirmation of movement/fighting; they do not prove golden path equivalence.

## Workload counters (items13–17)

Root columns sum only parent_id0; Internal is its own non-overlapping cohort. Internal
rows are nested inside roots: NEVER add these columns. Inclusive work propagates up
ancestors, so summing all paths.csv rows would massively overcount.

| Counter | All path roots | Internal cohort |
|---|---:|---:|
|Open head pops|1,706,221|1,276,704|
|Info attempts|38,529,375|37,933,553|
|Info new records|790,173|397,287|
|Info failures|967|461|
|Sorted insertions|2,034,995|1,478,825|
|Forward traversal hops|84,168,766|49,776,138|
|Reverse hops|0|0|
|Cleaned cells|1,033,897|465,454|
|Block-zone queries|637,717|0|
|Hierarchy fallbacks|304|0|

Frame counters:1657queued dispatches;856894queue cells allocated;1192636update-module
invocations;738588objects visited by the disabled-object loop (visits, NOT unique world
objects);1491565drawable updates. Maximum queue depth35before/10after processing;
max queue_cells_allocated in one frame34927. Queue budget checks between dispatches do
not preempt a large search; no budget/pool/retry change is justified. Root cleanups
include immediate/nonqueued paths and are not the same measure as queue allocations.

Internal info attempts/new ratio95.48:1; repeated checking/reopens revisit existing
information. Representative large successes have239–246attempts per new record and
60–71thousand pops, versus only9–12thousand new records. This is measured revisit
amplification, not an exact count of repeated identical movement queries or cache hits.
It supports tackling repeated checking, not another constant-size getter tweak.

Non-overlapping path roots inside GameLogic total2352.0353ms (39.87%of GameLogic);
queue category2263.0369ms (38.36%). Root queue detail2262.1081ms differs slightly from
category due to observer scope placement. These are alternative views, not additive.
Internal1864.5432ms and Closest193.6757ms are nested within those roots. Reconstruction
78.782ms/cleanup13.0029ms are child timing totals, not separate extra search cost.

## Sampled phase evidence (item18)

line_self=Line inclusive−Line insertion; neighbor_self=Neighbor inclusive−Neighbor
insertion; insertion=sum of the two insertion fields. Neighbor excludes its Line child.
Denominator is the disjoint sampled total. Values below are raw measured sample sums;
NONE is multiplied by64 or represented as exact whole-search phase duration.

| Internal threshold | Count | Sampled line self ms (%) | Neighbor self ms (%) | Insertion ms (%) | Checking / line alone wins |
|---|---:|---:|---:|---:|---:|
|>10ms|64|18.7289 (64.52%)|4.3537 (15.00%)|5.9442 (20.48%)|64/64 ; 64/64|
|>30ms|11|7.4368 (63.55%)|1.5919 (13.60%)|2.6740 (22.85%)|11/11 ; 11/11|
|>40ms|7|5.7459 (61.87%)|1.2280 (13.22%)|2.3127 (24.90%)|7/7 ; 7/7|

| Severe Internal ID | Line-self % | Neighbor-self % | Insertion % |
|---|---:|---:|---:|
|12738|46.68|7.68|45.64|
|1378|74.21|13.45|12.34|
|249|74.08|14.62|11.30|
|768|54.54|21.80|23.66|
|7339|57.64|14.03|28.34|
|218|69.36|16.05|14.59|
|12882|73.49|12.15|14.36|

The worst Internal has only a narrow line-alone margin (46.68%vs45.64%insertion);
combined checking54.36%still exceeds insertion. The other severe successes have much
larger checking margins. All7remain checking winners under the deliberately unfavorable
clock-only sensitivity charging every phase probe at33ns to checking and none to
insertion: worst ID12738 has0.2325ms checking-minus-insertion versus0.1703ms such clock
cost. This is NOT a bound on cache/bookkeeping/preemption or a corrected measurement.

Closest>10ms:6operations, combined sampled shares5.18%Line self,20.11%Neighbor self,
74.71%insertion; insertion wins each. Sorted insertion is a credible later target,
particularly Closest, but Internal total is approximately9.6times Closest total in this
window and repeatable checking dominates expensive Internal across accepted evidence.
Ground6operations/max2.6844ms supplies no new Ground-specific severe bottleneck.

Total eligible iterations1429057. Stage4A.2 synthetic incremental phase model22.8–35.4ns
per iteration corresponds to32.6–50.6ms across the minute, and0.915–1.421ms for the
40130-pop worst Internal. This is an illustrative sensitivity, NOT measured game overhead
or an upper bound. Synthetic iteration mixes differ, the old aggregate/deep observer is
additional, and this run has no matched shallow control. The model does not explain
156ms or repeated40–102ms searches. Some perturbation and individual preemption remain
possible; no new trace/profiler is needed to retain the accepted checking priority.

## Alternatives and useful workload (items19–20)

| Bottleneck / observation | Evidence in this run | Priority interpretation |
|---|---|---|
| Internal goal-directed checking |64>10ms;7>40ms; sampled checking75.10%of severe cohort; all15severe logic frames path-dominated|First meaningful optimization target|
| Sorted-open-list insertion / Closest |Severe Internal insertion24.90%; Closest>10 insertion74.71%, Closest193.68ms total|Second pathfinding direction; preserve retail order/cap exactly|
| Rendering/draw/present |DisplayDraw mean13.9663/p99 18.4445/max20.5963ms; SceneView mean3.7530/max8.3971ms; RenderEnd mean6.5813/max11.0811ms|Largest sustained outer-frame wall cost, separate render-FPS investigation; GPU/wait vs CPU unresolved|
| Object/AI object updates |Object loops2691.10ms total, mean1.5136/p99 2.5734/max8.1204ms; AI object mean1.1492/max7.5376ms|Meaningful sustained simulation cost; no>30ms culprit here, inclusive overlap must be respected|
| Zone rebuild |One17.1449ms Zones child in hierarchy (outer2052), logic18.8933ms|Real isolated spike, lower repeated-stall priority|
| AI player strategy |28.84ms total, p99 0.0920/max9.8032ms, frame1449logic13.972ms|Earlier historical strategy outlier remains context; not dominant here|
| Weapon firing |93.84ms total, p99 0.451/max6.2827ms, frame1280logic8.2982ms|Combat visible, not severe-tail driver; overlaps object/AI update|
| Logic residual |Exclusive865.54ms total, mean0.4868/p99 1.0154/max4.1959ms|Unclassified logic is not the severe bottleneck|

Zone flags5737rows/22.2084ms/max0.111ms; obstacle6rows/0.1623ms/max0.0407ms.
Reconstruction/cleanup maxima1.112/0.516ms do not explain the long searches.
Render/present wall time must not be called pure CPU rendering or GPU saturation;
present/flush can wait on synchronization/driver/GPU. Actual~60FPS despite120cap is
observable, but neither VSync nor any specific limiting mechanism is proven by CSVs.
No>30ms client/draw/present category spike appears; stalls180msouter align with logic.

The battle preset IS a useful repeatable setup for interactive performance development:
fast entry, substantial moving/fighting armies, both human/AI searches, hierarchy
fallback, successful and failed Internal paths, Closest retries and real severe stalls.
It is not the old developer+5AI large-map match and does not replace its broad AI/object
coverage or constitute a deterministic repeatable benchmark: capture timing, player
commands, camera, casualties and AI evolution vary. Record these for future trials.

## Stage4B.1 retention decision

**Retain the bounded surface-mask hoist.** The optimized route was exercised substantially;
manual gameplay and exit pass, capture integrity/workload/result distributions are
plausible, and no regression is evident. This is evidence of operational health,
not proof of exact old/new path sequences, replay/save continuation or multiplayer
lockstep. Keep the existing source-equivalence proof and require stronger paired
correctness checks for any more substantial change.

This is NOT matched before/after. No percentage speedup, slowdown or reduced failure
rate versus Stage4A/A.1/A.2 is claimed. Their accepted findings are historical context:
A control was light, busy windows exposed pathological individual searches, WPR did
not resolve leaf-cost attribution, and A.2's31>40ms Internal cohort established
51.19/15.56/33.25%Line/Neighbor/insertion. The present61.87/13.22/24.90%split is a
DIFFERENT workload, not a Stage4B.1 improvement. The getter is inline and the compiler
may already eliminate redundant loads; likely effect is too small to distinguish
from uncontrolled run-to-run variation. Do not spend further manual trials estimating
this tiny isolated change before more meaningful work.

## Recommended next task — meaningful Internal checking reuse (proposal only)

Recommend ONE next optimization group: **bounded, single-Internal-search memoization
of the pure footprint/movement-query result used by examineCellsCallback**, after a
specific read/write invariance audit. Route remains internalFindPath →
examineNeighboringCells → iterateCellsAlongLine → examineCellsCallback → checkForMovement.
This is a substantive attempt to avoid redoing whole footprint occupancy/relationship/
crush queries when goal-directed lines revisit the same cell, not another getter hoist.

Scope the first experiment ONLY to the Internal line callback. Preserve the walker,
every callback invocation and its order, all external terrain/zone/pinched/bounds checks,
allocateInfo attempts/failures, costs, parents/reopens and sorted insertions exactly.
Cache only checkForMovement return and complete observable output flags/counts, not
path-cell info or an entire callback result. Its footprint loops repeatedly read cells,
flags, object IDs, relationships, ignored obstacles, infantry/idle/crush policies.
Path info/open/closed/parents mutate during search and MUST NEVER be memoized.

Use a fixed bounded per-search cache with exact keys (cell coordinates/layer plus the
full query policy/object context); reset on each invocation and destroy/discard before
return. No cross-request/tick persistence, heap growth, saved field, RNG or FP arithmetic.
Miss/eviction/collision must call the unchanged reference helper; capacity can influence
performance only. Shared callbacks also serve Closest/other searches, so the cache must
be explicitly supplied only by Internal, never implicitly enabled everywhere.

Before implementing, prove that occupancy/goals, ignored obstacles, relationship/idle/
crush policies and all query inputs cannot change between reused calls in this exact
synchronous scope, including layer changes, reentrancy and both title variants. Current
checkForMovement reads occupancy fields while the callback changes cost/list/parent
fields. IMPORTANT: occupancy IDs and search fields share PathfindCellInfo storage;
allocation, release, orphan handling and retail failover must therefore be audited
explicitly. Do not assume physical storage separation or cache an m_info pointer.
This is a credible candidate with a required invariance gate, not an already proven cache. If a getter
or nested route can mutate relevant state, narrow to a proven scope or reject caching;
do not weaken authoritative semantics or add a broad invalidation architecture.

The38million info attempts and high attempts/new ratios establish repeated downstream
cell work, NOT exact memo hit rate or that checkForMovement alone dominates Line self.
Line self also contains movement terrain/zone checks and info/cost/parent work. Existing
phase evidence identifies the route and makes this a justified bounded reference
experiment; it does not warrant a numeric improvement promise or a new profiling
framework. Aim for a material reduction in repeated helper execution, not token-count
reduction; reject if actual reuse/phase/tail benefit is negligible.

Acceptance plan: compare reference and cached helpers' complete return/output for exact
query sequences on empty/occupied/off-map, ally/enemy/ignored/infantry/crusher and
Generals retail idle cases. Then compare ordered cell pops/callbacks, costs/parents,
returned/null paths and path nodes, work counters (pops/info/new/fail/inserts/hops), queue
order and deterministic end state with cache hits/misses/collisions. Test layer transitions,
reopens/equal costs, pool failures and nested/reentrant exclusion. These reference tests
are part of the next implementation, not claimed as run now. Preserve5000-hop behavior,
hierarchy/retry policy, pool/queue budgets,30TPS, RNG/FP, save/replay/lockstep state.

Measure multiple comparable reference/candidate interactive runs with the same setup,
capture timing/actions/camera/profiling/caps and verify workload equivalence first.
Report severe Internal self, sampled checking shares and GameLogic p95/p99/max, including
non-path regressions. Replay/save-load/multiplayer correctness remain acceptance gates
before broad adoption. No Stage4A.3 debug prerequisite or new five-AI selection capture
is needed. Developer approval is required to begin the proposed Stage4B.2; none was
implemented here. Rendering/present is a separate sustained-FPS concern, not evidence
to displace the proven authoritative pathfinding tail target with an unmeasured GPU fix.

## Preservation and scope

All original capture files/receipts are preserved. Analysis helpers/JSON are ignored
under build and are not staged. No source/script/test edits, rebuild, launch, commit or
push. Documentation alone records manual success, measured results and the proposed
next task. Stage4A.3 relocation/neutral-owner defects and its forensic diagnostics remain
unchanged and paused. Release instant build is deferred as documented, not a blocker.
