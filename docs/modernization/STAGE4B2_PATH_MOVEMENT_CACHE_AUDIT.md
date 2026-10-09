# Stage4B.2 movement-query reuse audit (2026-10-09)

## Verdict: reject production cache for this task; equivalence gate remains unmet

No Stage4B.2 gameplay implementation, new observer, runtime candidate or launch.
Stage4B.1 remains accepted for retention, without a measured speedup claim.
Stage4A.3 stays PAUSED. No commit, push, staging or Steam writes.

The audit finds a credible, substantial repetition opportunity and no ordinary
world update inside the Internal expansion loop. It does **not** demonstrate the
required optimized-versus-reference behavior on real path searches. This is a
failure to establish the implementation gate, not evidence that caching is inherently
unsafe, nor a finding that reuse is negligible. Do not convert this report into a
claim of a proven safe cache or a measured runtime improvement.

Two missing pieces matter: actual keyed-query sequences for a specified small bounded
cache (including layer transitions/collisions), and exact production-query/path
reference comparisons with occupancy, relationships and pool failures. Existing
aggregate counters and phase times cannot supply either. A mock dictionary test or a
copy of the movement algorithm would not establish those production semantics.

## Evidence and scope

Read actual shared AIPathfind.cpp, AIPathfind.h and Pathfinder headers, and the
Object, AIUpdate, StateMachine, Team, Player, GameLogic, containment, LocomotorSet
and name-key dependencies in the title trees. The route is:

internalFindPath -> examineNeighboringCells -> iterateCellsAlongLine ->
examineCellsCallback -> checkForMovement.

The accepted CSV was read directly from
build/performance/dev-20261009T082332Z-c286325d/
20261009T082505Z-29724-1-paths.csv. The summary was also read directly. All five
reports' SHA256 hashes match the saved accepted analysis. The earlier complete
report validation and category/frame correlations are recorded in
[STAGE4B1_INTERACTIVE_CAPTURE.md](STAGE4B1_INTERACTIVE_CAPTURE.md).

60.0094 seconds, 3587 outer frames, 1778 completed ticks, 30375 path records,
zero dropped/stack-clock/phase errors, cap120/interpolation1. All 15 logic frames
above30ms are path dominated. Seven Internal searches exceed40ms; their aggregate
sampled Line-self/Neighbor-self/insertion shares are 61.87/13.22/24.90%.
These are sampled shares, not exact full-search phase times. Historical A.2 has
31 severe Internal checking winners and 51.19/15.56/33.25% shares. Neither capture
is a matched before/after experiment.

## Inputs, outputs and lifetime classification

A = invariant inside one synchronous expansion loop under the audited ordinary
call graph; B = fixed for a particular callback but varies across expansions;
C = mutable search state; D = world/object state, mutable outside this loop;
E = pool/legacy state. Some inputs have more than one classification: D/A means
mutable between searches, with no reachable ordinary writer during this loop.

| Input/dependency | Class | Actual use and writer/lifetime boundary |
|---|---|---|
| Source Object pointer, ID, AI interface and ignored-obstacle ID | D/A | Null source immediately succeeds after clearing outputs. Otherwise source ID and ignore ID skip occupants; ignore ID is read again for fixed/moving checks. AIUpdate constructor/setters/reset/load can change it, not expansion. Object creation/destruction changes identity outside this route. |
| Footprint radius and centerInCell | A | getRadiusAndCenter runs before search. Loop visits x then y, lower=coordinate-radius, upper exclusive=coordinate+radius+(center?1:0). Geometry changes are outside this route. |
| Query x/y and movement layer | B | Coordinates are the walker arguments, layer is **from->getLayer()**, not to->getLayer(). Must key the query layer; source object's starting layer alone is insufficient. |
| considerTransient | A here | Explicit false in Internal line callback. Other callers can pass true; must not share their results. |
| acceptableSurfaces | A, external | Assigned in TCheckMovementInfo, but **not read by checkForMovement**. Used by preceding validMovementPosition and separate movement checks. Stage4B.1 hoists only the synchronous callback mask read. |
| Source/target infantry kind | D/A | Compile-time INFANTRY_MOVES_THROUGH_INFANTRY enables the skip. Template/kind changes outside this route. |
| Cell existence, grid/layer buffers and extents | D/A | getCell checks extents and layer storage. Missing/impassable non-ground cell falls back to ground. Map/layer allocation/classification/destruction occurs outside expansion. |
| Cell unit flags | D/A | NO_UNITS skips occupant lookup; GOAL and GOAL_OTHER_MOVING set allyGoal regardless of owner. Written by setPosUnit/setGoalUnit/reset; expansion does not call these. Flags are PathfindCell fields. |
| Occupant ID | E + D/A | getPosUnit reads m_info->m_posUnitID, or INVALID_ID if no info. Physical storage is shared with costs/parents/list links. Allocation/release must be audited, not assumed independent. |
| Object lookup | D/A | GameLogic::findObjectByID uses a hash find in Generals and a bounds-checked object-vector lookup in Zero Hour; neither inserts. Add/remove/load changes the registry outside expansion. Missing object is a distinct case. |
| Directional relationship | D/A | Object checks source/target undetected-defector flags, then source Team overrides, Player overrides and controlling-player relationship maps; default NEUTRAL. It is not symmetric. Team/player setters and ownership/defector changes are outside expansion. |
| Ally AI/idle policy | D/A | Missing ally AI rejects. **Generals retail** additionally calls virtual isIdle; Zero Hour does not use that idle rejection here. Base checks state-machine current state's isIdle; idle state is true, default state false. Overrides inspect pending/outside command, flight and containment state. Chinook containment query reads enter/exit-map emptiness. No update/command dispatch in these readers. |
| Enemy crush/squish policy | D/A, hidden init | canCrushOrSquish checks source DISABLED_UNMANNED, crusher level, directional relationship, target SquishCollide module and crushable level. Template/module/team/disabled writers are outside expansion. See first-use name-key caveat below. |
| Human/crusher, zones, terrain/type, pinched cells, logical bounds, obstacle ID | D/A, external | These affect preceding callback/neighbor decisions. checkForMovement itself does not read human, zones, pinched, terrain surfaces or cell obstacle ID. Source ignored-obstacle ID is a separate input. Never cache these larger decisions as part of a footprint result. |
| Goal/destination reservations | D/A + E | Unit goal flags influence allyGoal; helper does not read goalUnitID or goalAircraftID directly. Destination screening occurs before expansion. updateGoal/removeGoal and position classification write reservations outside expansion. |
| from/to costs, parents, open/closed flags, blockedByAlly, tunneling | C/E | Expansion mutates them. They control callback result, reinsertion and future expansion and are excluded from any proposed cache. |
| Return bool and four outputs | query result | Always initialize allyFixedCount=0, allyMoving=false, allyGoal=false, enemyFixed=false. Early false returns can retain partially accumulated outputs. Full result must be retained, not just bool. |

The ally de-duplication array holds **five** IDs. Later distinct IDs increment
allyFixedCount without entering that array, so repeats of those IDs can increment
again. Do not replace this with general set semantics. Preserve footprint traversal
and early exits exactly.

## Reachable writes, ordering and hidden effects

The walker can invoke two callbacks in an iteration, including its initial callback
with from=null. The initial null-from callback performs no footprint query. Preserve
every call, coordinate, intermediate step and early-stop decision.

The line callback performs terrain/surface, zone-passable, pinched, human-bound checks
before the helper. After the helper it rejects enemy/ally-fixed occupancy, computes
cost, rejects cliff, calls allocateInfo **on every surviving callback**, then writes
blockedByAlly/cost/parent and removes/reinserts list nodes. Those operations cannot
be skipped on a cache hit, even when the cell already has info or an equal/better
cost. The neighbor loop also writes tunneling, costs and blockedByAlly and has its own
allocation and movement policy; it must remain uncached in this proposed scope.

checkChangeLayers allocates at most one linked cell and changes costs/parents/lists.
No ordinary expansion callee calls GameLogic update, sends orders, moves/deletes
objects, changes teams, updates occupancy/reservations, changes geometry or advances
RNG/time. Team/Player relationship lookups, object lookup and module lookup are reads. Cross-title token comparisons confirm identical crush/squish, Object relationship, module lookup, Team/Player relationship and containment-query bodies; object lookup differs as recorded above.
This is a call-graph finding, not a universal guarantee for new virtual overrides,
corrupted structures, a future reentrant callback or arbitrary debug instrumentation.

canCrushOrSquish has a **static NAMEKEY("SquishCollide") initializer**. The name-key
lookup can allocate/register a name on first use. A cache could not prepopulate a
result or bypass the first original query; first miss must execute it at the same
point. It is not accurate to describe every operation in this call chain as having
no side effects. No collision, RNG or damage operation is called by this test.

Retail insertion also repairs dangling next-open links. Reconstruction/prependCells
can trigger forceCleanCells and switch fixed mode; cleanup can trigger failover too.
forceCleanCells changes pool/list and orphan-obstacle state and emits UI/audio.
**End any proposed reuse at the end of the expansion loop, before reconstruction
and cleanup**, rather than casually freezing the entire Internal function. Shared
callbacks and reconstruction line-passability queries must not inherit a cache.

## PathfindCellInfo and pool audit

The shared pool has 30000 records. allocateInfo always increments InfoAttempts. It
only obtains a new record when m_info is null and counts new/failure accordingly.
getACellInfo initializes costs/links/parents and clears occupant/goal/aircraft IDs.
Therefore a cached allocation result, an early hasInfo skip or moving allocation
before an original check is forbidden.

releaseInfo retains obstacle cells, cells with unit flags and aircraft goals; normal
empty-cell records can be freed. Allocation of a well-formed NO_UNITS cell changes
its info pointer/IDs but not what the helper sees: NO_UNITS already skips occupant
reads. Occupied cells retain their info and are not overwritten by normal search
allocation. There is no info release in the expansion loop; releases are on entry
failure or exit/reconstruction/cleanup, after further Internal line queries cease.

This makes value reuse plausible on well-formed live occupancy. It does not justify
caching pointers, assuming orphan/failover behavior irrelevant, or claiming tested
near-exhaustion equivalence. The worst captured Internal has 461 failures, so that
case is important rather than theoretical.

## Reuse bound from actual allocation sites

This is a derived bound, **not** a recorded cache-hit rate. It is stronger than the
95:1 aggregate info-attempt:new ratio and states its assumptions explicitly.

For one Internal, let A=InfoAttempts, P=head pops, F=InfoFailed. At most two calls
allocate start/goal, at most one allocation per pop comes from checkChangeLayers,
and at most eight per pop from neighbors. Other Internal allocation attempts are
at the line callback site. Reconstruction and cleanup introduce no additional
allocateInfo calls on this route. Consequently:

L = max(0, A - 2 - 9*P - F)

is a lower bound on **successful allocating line-callback queries** (subtracting
all failures conservatively). Each has passed checkForMovement. It excludes blocked
queries and does not count all footprint calls. No release occurs during expansion,
so successful queries touch at most 30000 admitted cells in well-formed pool state.

However, the footprint key uses the FROM layer. getCell(layer,x,y) may return a
ground cell outside a non-ground layer. Multiple query layers can address the same
admitted cell and have different surrounding footprints. **Do not claim30000 unique
keys from pool size alone.** There are 15 valid layers (1..15); a conservative upper
bound is 450000 successful (layer,x,y) keys for this fixed source/radius/center/policy.
The captured source layer being ground does not prove every expansion stayed ground.

| Internal ID | Inclusive ms | Pops | Attempts | New | Fail | L lower bound | Repeated keys lower bound, U<=450000 | Repetition rate lower bound among successful line queries |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
|12738|156.2320|40130|1663539|25759|461|1301906|851906|65.44%|
|1378|102.3590|71087|2853220|11586|0|2213435|1763435|79.67%|
|249|78.6992|60955|2171429|9084|0|1622832|1172832|72.27%|
|768|64.7285|42562|1184315|9272|0|801255|351255|43.84%|
|7339|46.6076|27752|750224|8405|0|500454|50454|10.08%|
|218|45.3780|36605|1181001|7618|0|851554|401554|47.16%|
|12882|41.1046|22482|762335|5117|0|559995|109995|19.64%|

Across those 7 operations, L>=7851431 and repeated keys>=4701431 (59.88% conservative
aggregate lower bound). If every query were proven ground-only, repeats>=7641431,
a 97.33% lower bound. The ground-only figure is **conditional**, not a measured hit rate.
For all 64 Internal >10ms, L>=25105130 and repeats>=5525994 using the conservative
15-layer bound; for 11>30ms, L>=10376050 and repeats>=5426050.

Actual calls/unique keys/distribution, bounded-table hit rate, collision pressure,
failed/blocked-query reuse and helper-only CPU time are **unmeasured**. A large fully
retaining table could cover the successful-key upper bound, but that does not prove
that its allocation/clear/lookup/memory cost is worthwhile. A smaller bounded cache
cannot claim the ideal repetition count as its own hits. Phase Line self includes
more than occupancy queries: walker, surface/zone checks, allocation/probes, costs,
parents and list bookkeeping outside insertion also contribute. Do not assign61.87%
of severe Internal time to checkForMovement or promise that reduction.

## Conditional cache design, not implemented or proven sufficient

Owner/context: one Internal expansion-loop invocation, one live source pointer/ID,
radius, center, considerTransient=false, exact engine/title policies and stable
world/occupancy epoch. No persistence into reconstruction, Closest, requests or ticks.
Entries: exact query (x,y,from-layer), complete returned bool and output count/flags.
No path-info pointers or allocation/path-cost/list decisions. Explicit callback
context pointer only for this Internal route; all other callers remain reference.

Coordinates alone are insufficient. Radius/center/source/ignore and all object/world
inputs cannot be omitted unless the per-loop owner/context proof and tests establish
their stability. A hash collision must check the full key; saturation/memory failure
must run the original helper. Deterministic bounded lookup with no authoritative
iteration is required. Restricting admission to queries whose subsequent original
allocateInfo succeeds would support the pool-derived bound, but must preserve the
helper and allocation order and test failures. No positive/negative cached allocation.

This describes necessary conditions; no cache implementation or complete key
sufficiency/equivalence claim is made. In particular malformed pool/list states and
both-title virtual-object policies have not been exercised with cache hits.

## Reference/equivalence gate and validation actually run

Existing Google tests initialize memory, command-line startup and ClientInstance;
they do not initialize a terrain map, object/team registry, AI modules and pathfinder
world. There is no existing full path-result/expansion reference fixture. Existing
PathProfiler tests validate profiling records, not the engine's pathfinding decisions.
Stage4B.1's exact inverse token audit proves its sole surface-getter transformation;
it cannot prove memoization, which deliberately changes helper invocation count.

Required before a production cache: a narrow fixture executing the **actual helper**
against occupied/off-map/moving/fixed/goal cells, directional relationship overrides,
ignored IDs, infantry/crush/unmanned/squish, Generals retail virtual idle, surfaces and
layer fallback cases. Compare bool and every output, including partial false outputs
and >5 ally behavior. Then run the actual uncached/cached expansion on reset-equivalent
world/pool state; compare path/null and node order, callback/pops/insert order, parents,
costs, info attempts/new/failures, hops, hierarchy/Closest decisions and end-state
fingerprints. Include reopens, equal cost, 5000-hop boundary, pool exhaustion and
legacy repair. A fake-world algorithm copy or final-path-only comparison is insufficient.

No such optimized/reference comparison was run; therefore the explicit user gate
"If exact equivalence cannot be demonstrated, reject the optimization" applies.
No full build was run: engine/test/script source is unchanged this task, verified
against 29 saved hashes, and no new candidate needs compilation.

Actually run in this task:

- Existing Python script/exact-reference suite: 13/13 PASS.
- Existing Release g_googletest focused profiler/path-phase/developer suites: 47/47 PASS.
- Existing Release z_googletest same focused suites: 47/47 PASS.
- Preservation/token audit: PASS; 391 earlier runtime/evidence files and accepted
  A.2 capture hashes unchanged, gated adapters restore accepted non-dev token order.
- All 5 latest accepted capture report hashes unchanged; index empty.
- Final git diff --check and 29-source/test/script preservation checks: PASS.

These are tests of retained code, **not cache equivalence tests**. Full x86 rebuild,
CTest/full direct suites, new equivalence tests and runtime inventory are not claimed.
The full implementation-validation procedure remains required if a future task
actually changes source.

## Alternatives and recommended next task

Do not substitute an unsafe cache, early allocateInfo elimination, whole-callback
memo, cross-tick/object memo, generic heap, fixed-mode enable, pool enlargement or
policy/budget change. None is authorized by this audit. Query repeats do not permit
skipping callback or path-state work.

The next highest-value **separately testable optimization investigation** is the
existing retail open-list traversal in
PathfindCell::forwardInsertionSortRetailCompatible, through putOnSortedOpenList.
This recommendation is not simply "second largest":

- Actual workload has 49776138 Internal forward hops and 84168766 root-work forward
  hops. The worst Internal alone has 8148529. Expensive Closest searches add a
  distinct insertion-heavy tail (six>10ms, aggregate sampled insertion 74.71%).
- Severe Internal sampled insertion 24.90% (historical A.2 severe 33.25%) provides a
  material measured ceiling; it is smaller than checking and not a full-path speedup.
- The production list primitive can be exercised with allocated PathfindCell records,
  explicit total costs, links and remove/reinsert sequences without a terrain/object/
  relationship world. Its exact oracle is substantially narrower than the proposed
  movement cache's missing world fixture.
- Zone rebuild17.145ms is isolated; objects8.120ms, AI strategy9.803ms and weapon6.283ms
  do not dominate these repeated>30ms authoritative stalls. Rendering/present remains
  a separate unresolved GPU/wait issue; it does not explain the logic tail.

Start with an actual-retail-list reference/equivalence microbenchmark and inspect
whether reducing traversal memory/indirection cost can give a meaningful gain while
retaining **the same visited-node sequence, <= tie order,5000-hop stop, dangling-link
repair side effects, parents/reopen and counters**. Do not silently skip traversal
checks or change ordering. A pure hoist with noise-sized benefit should be rejected;
no specific acceleration is accepted until its proof and benchmark establish value.
No heap/reverse-sort/auxiliary-index implementation is proposed as already safe.

The current checking route remains the larger opportunity, so this is not a finding
that list work is the dominant original cause. If developer review prefers continuing
movement reuse, the minimum prerequisite is the production reference fixture above
and a bounded, default-OFF shadow-table hit/mismatch measurement for a specified key/
capacity, attached to existing Internal records. Always execute the original helper;
compare full outputs on candidate hits. It would require separate review and one
interactive battle run only if fixtures cannot establish representative reuse. No
new profiler framework, WPR trace, five-AI selection capture or Stage4A.3 repair is
needed. None of that measurement instrumentation is added here.

## Runtime, preservation and review handoff

No stage4b2-path-movement-cache candidate exists from this task; no hashes/inventory
or new runtime test command are supplied for an optimization that was rejected.
The retained, manually validated Stage4B.1 runtime and all captures remain untouched.
Only this audit and the current-state documentation pointers are changed by this
task. Existing uncommitted Stage4A.3, DevMode and Stage4B.1 work is preserved, index
empty. No launch/build/install/commit/push. Stop for developer review.
