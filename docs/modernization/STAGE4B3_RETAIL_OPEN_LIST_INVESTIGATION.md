# Stage4B.3 retail open-list investigation (2026-10-09)

**Closeout status (2026-10-09): investigation/tooling complete; production
insertion unchanged. Immediate next phase is X64.1 architecture/dependency audit
and Win64 build foundation. Prefix/rank hints are future Pathfinding 2.0 backlog.**
Original no-commit/push statements below describe the investigation task only.

## Outcome B: retain oracle/benchmark; no production optimization selected

Actual production-primitive tests and a reproducible opt-in microbenchmark are now
implemented. No open-list algorithm, representation, allocation, queue policy or
fixed-pathfinding behavior changed. Three header friend declarations provide access
only to test fixtures; they add no fields, executable code or serialization state.

Two bounded constant-work candidates were evaluated. A cost snapshot and a two-step
loop preserve the original algorithm, but neither establishes a meaningful broad,
repeatable benefit. More aggressive shortcuts have unproven or demonstrably different
cutoff/repair behavior. Therefore no production Stage4B.3 optimization or
stage4b3-open-list runtime is prepared. Do not claim a gameplay speedup.

Stage4B.2 movement caching remains rejected for implementation, its audit unchanged.
Stage4B.1 remains accepted for retention. Stage4A.3 remains PAUSED. No game launch,
Steam install/write, commit, push or staging occurred.

## Exact production algorithm and compatibility contract

Shared implementation: Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp,
PathfindCell::forwardInsertionSortRetailCompatible, called by putOnSortedOpenList
when RETAIL_COMPATIBLE_PATHFINDING is compiled and s_useFixedPathfinding is false.
The static defaults to false. Legacy failure cleanup can later switch it to true;
this task never enables it. The alternative implementation maintains tail and may
reverse-sort; enabling it is not a compatible performance shortcut.

Representation: PathfindCell holds m_info. PathfindCellInfo holds next/prev-open
links to other **info records**, m_cell back to the cell, parent, open/closed bits
and unsigned-short total/so-far costs. PathfindCellList has head/tail **cell pointers**.
The shared pool has 30000 info records. Insertion does not allocate or release any.
Stored total cost is 16-bit; a wider caller value is truncated before comparison.
Do not index or compare the untruncated caller value instead.

Exact steps:

1. Assert the inserted cell has info and is neither open nor closed (diagnostic
   assertions); mark its info open=true and closed=false.
2. Empty head: set head=this, clear inserted prev/next, return. **Tail is untouched.**
3. Start at the head every time, previous=null, count=0. Continue while current
   exists, count<5000, and current info totalCost<=inserted info totalCost, in that
   short-circuit order.
4. On each visited qualifying cell, inspect next info. If next exists and its owner
   cell has null m_info, set **next info's m_cell=null** and **current info's next=null**.
   This is a mutation, not a redundant validity check. The branch itself assumes
   next->m_cell is non-null; arbitrary null-owner input can crash and is not claimed
   as repaired/supported. Then increment count, save previous=current and advance
   through current->getNextOpen(), which performs info->next->m_cell.
5. Add the exact count to the ForwardHops profiler counter, once after traversal.
6. If current exists, insert before it. Update previous-info's next or list head,
   inserted prev, current prev and inserted next. Otherwise append after previous,
   update previous next and inserted prev/next. **Do not update list tail.**

putOnSortedOpenList additionally counts OpenInserts and creates the sampled insertion
observer. The direct primitive benchmark reports its own invocation count and verifies
that the primitive itself does not increment OpenInserts. No RNG, floating arithmetic,
world query, GameLogic tick, parent, cost or mode mutation occurs inside this primitive.

### Ties, cutoff, repair and later expansion

For a clean sorted list shorter than the cutoff, <= traverses all equal-cost entries,
so a new equal node follows older equals. Decreasing keys prepend; increasing/equal
keys scan the entire prefix. There is no de-duplication: reinsertion requires the
caller to remove an already-open entry first. Inserting the same live node twice
without removal violates the function's precondition; it is not a supported workload.

The 5000-hop limit is an **insertion stop**, not a request/queue limit. At count5000,
cost and repair checks on the next current node do not execute. The new node is
inserted before that current node even if it has lower/equal cost. This can make
the remainder unsorted and alter FIFO tie order. Example: with5001 existing cost10
nodes, inserting cost20 puts the new node at index5000 before a remaining cost10.
A tail append, global lower_bound, heap or ordinary reverse-sort is not equivalent.

Repair is conditional on visiting a qualifying node before the cutoff. It is not
performed on a node whose cost already stops traversal or beyond the cap. Truncating
a dangling link can detach a suffix; the orphan record's m_cell mutation must also
match. Insertion itself does not signal mode fallback or call forceCleanCells.
Later releaseOpenList/releaseClosedList/cleanup can signal s_useFixedPathfinding and
s_forceCleanCells, reset lists and clear orphan state. Preserving insertion's exact
link/owner mutations preserves input to those later mechanisms; the standalone
fixture does not claim a full world/UI/audio failover integration test.

Tail may be null or stale in this retail path. removeFromOpenList can update it, but
retail insertion leaves it untouched. The fixtures explicitly preserve that behavior.
The retail Internal route can seed head with reset(parentCell); insertion must not
silently change the existing head's open flag. Actual later expansion removes head,
checks goal, expands and may reopen/reinsert nodes, making identity order, not just
cost ordering, authoritative. Full paths, RNG/FP, lockstep and save/replay policies
are unchanged because production insertion remains byte-for-byte unchanged.

A noncanonical-owner fixture also matters: next-info can point to a non-null cell
whose m_info is a different record. The old walk then follows that cell's info.
With head cost10, next record cost100 referring to a different cell with cost20,
inserting cost30 visits two nodes and produces identity order[0,2,3]. Directly
following info links would inspect100 and produce a different order. No null pointer
or undefined invalid memory access is needed for this counterexample. Do not collapse
the cell/info round trip based only on an assumed owner invariant.

Compatibility-sensitive: exact visited sequence, <= comparison and stored width,
5000 stop/short-circuiting, ties, seeded-head flags, all link/head/tail/owner mutations,
repair order and hop accounting. Potential implementation details: local temporaries
and loop grouping, **only** if they preserve those observations and show real value.

## Actual production fixture and exact reference oracle

Tests/Google/Core/GameEngine/Common/RetailOpenListTest.cpp invokes the actual linked
production member, actual pool allocation, actual cells/lists and actual removal.
No toy list is substituted for baseline timing. Narrow friend declarations in
PathfindCell.h, PathfindCellInfo.h and PathfindCellList.h allow construction and
inspection of otherwise inaccessible repair states. No private/public macro or
layout casts are used. Fixtures allocate before measurement and restore deliberately
detached info pointers before destruction; all pool records are reclaimed.

RetailOpenListReference.inc freezes the accepted production body. Only member context
is adapted: self=&cell, m_info=self->m_info and this becomes self. Python token audits
compare that body to accepted git0ff7f9c9bf8dfe188015af36c7db8d5388756eda and the unchanged
current member. It is an independent executable reference copy of the real body,
not a separately invented sorting algorithm.

Oracle captures complete identity order, head/tail identity and every allocated
record's next/prev/parent/owner identities, costs, open/closed/free flags and coordinates,
including disconnected repair records. It also compares exposed hop counters. An
independent identity-vector specification inserts after the qualifying prefix limited
to5000 for valid-list cases; that model is correctness-only, never baseline timing.

All intermediate states are compared for small/generated and churn workloads and
remove/reinsert sequences; large static lists compare full final state/order plus
per-insertion hop counts. Tests cover empty/one, seeded head, unsigned-short wrap,
increasing/decreasing/random/clustered/equal/alternating/small-range keys,6002-node
lists,128 fixed LCG seeds,512 remove/reinsert events,64/384 live windows,4999/5000/5001,
repair before/beyond cutoff and noncanonical owner round trips. Production, frozen
reference and test-only two-step candidate have exact equality. No fuzzy comparison.
These are primitive-level proofs/tests, not full-map replay or multiplayer results.

## Microbenchmark design and results

Disabled-by-default Google benchmark RetailOpenListBenchmark.DISABLED_ProductionAndTwoStep
runs nine fixed workloads of6002 insertions, seven repetitions per implementation,
rotating implementation order each repetition. Correctness/counters/complete final
identity-order output, pool allocation/destruction and inspection are outside timing.
Timed production calls use the real member with profiling disabled. Live-window
cases include actual FIFO removal plus insertion; their times are **churn cycle**
cost, not insertion-only cost. Other cases time insertion only. Timing uses steady
wall clock, so scheduling/cache/frequency noise is included; no CPU pinning is claimed.

Baseline, min/median/max and full identity orders are in ignored local logs:

- build/stage4b3-unrolled-g.log
- build/stage4b3-unrolled-z.log
- build/stage4b3-benchmark-g.log
- build/stage4b3-benchmark-z.log
- build/stage4b3-benchmark-g-repeat.log

The first two use the final two-step fixture; the latter three preserve the earlier
cost-snapshot experiment. All five produce identical full baseline identity orders
for every workload. Analysis JSON is ignored build/stage4b3-benchmark-analysis.json.
No timing threshold is used as a correctness test.

Median wall milliseconds below. Hops/counts are exact for these deterministic inputs;
timing differences are not gameplay speedups or precise causal effect estimates.

| Workload | Hops | Mean hops | Max hops | Generals production / two-step ms | ZH production / two-step ms |
|---|---:|---:|---:|---:|---:|
| increasing | 17507500 | 2916.9444 | 5000 | 67.9883 / 71.9772 | 69.0137 / 70.9024 |
| decreasing | 0 | 0.0000 | 0 | 0.0280 / 0.0224 | 0.0272 / 0.0233 |
| random | 8998062 | 1499.1773 | 5000 | 78.0335 / 175.2390 | 79.9388 / 192.6727 |
| equal | 17507500 | 2916.9444 | 5000 | 114.4335 / 108.7004 | 67.5192 / 67.6121 |
| clustered | 10059465 | 1676.0188 | 5000 | 67.8689 / 68.6100 | 43.0642 / 41.2015 |
| alternating | 13256500 | 2208.6804 | 5000 | 85.5818 / 81.4517 | 76.0427 / 71.3296 |
| small_range | 9065848 | 1510.4712 | 5000 | 218.6539 / 222.0382 | 232.2803 / 235.8874 |
| live64 | 187905 | 31.3071 | 63 | 0.7853 / 0.7496 | 1.4526 / 1.3630 |
| live384 | 1112748 | 185.3962 | 383 | 4.0481 / 3.7035 | 7.2450 / 6.9004 |

Increasing/equal totals follow sum(min(j,5000),j=0..6001)=17507500; decreasing needs0.
This quantifies the always-head/<= mechanism, not just Big-O. Equal and increasing
workloads have identical hops but different timings, illustrating dependence on
layout/cache/branch/scheduling beyond a hop count. Random keys reach the cap too;
ties are not necessary for pathological traversal. The small live-window cases
reproduce observed average traversal intensity, not actual recorded key sequences.

### Why the accepted capture reaches49.8 million Internal hops

Direct CSV calculation gives1478825 insertions *33.659248 mean hops=49776138 hops.
The worst Internal gives44007 *185.164383=8148529. Across7 Internal>40ms the mean
is50.027388 over327777 insertions;64 Internal>10ms average35.158689. Closest totals
158694 insertions/19535367 hops=123.100854; the six>10ms average170.126650.
Thus high invocation count multiplied by repeated prefix walking explains the total;
it does not require every insertion to traverse thousands of entries.

Accepted phases attribute24.90% of sampled severe Internal time to insertion and
74.71% in expensive Closest operations. No overlapping inclusive times are added.
Capture CSV does not store insertion keys, list lengths, per-insertion maximum,
cutoff-hit counts or repair frequency. It cannot establish how much of those real
hops came from ties versus increasing/clustered keys, poor key locality, long lists
or repairs. Benchmark mechanisms are demonstrated; actual distributions are not
invented. No extra profiler or manual capture was added just to fill those gaps.

## Candidates evaluated and selection gate

| Candidate | Exactness/value finding | Decision |
|---|---|---|
| Snapshot inserted ushort cost | Read-only cost stays unchanged in traversal; frozen-source adaptation and initial reference tests pass. No hop reduction. Earlier seven-repeat, three-session timing shows inconsistent common-workload gains (live384 candidate6.5151 vs7.2220ms,6.6785 vs7.6745ms, then6.6642 vs4.5931ms). | Reject production change: no broad repeatable meaningful benefit. Prototype discarded; logs retained. |
| Two original steps per loop | Same condition/short-circuit check and repair between steps, same count and post-loop insertion. Exact-token construction audit and all final primitive tests pass. No skipped nodes or repairs, no cache invalidation needed. | Reject production change: increasing slightly slower in both titles, equal has inconsistent benefit, random has severe wall-time variability, calibrated churn gains are modest. No robust broad value established. Keep test-only for reproducibility. |
| Finger/cursor | Sorted prefix alone is insufficient:5000 cutoff can leave an unsorted suffix; skipped prefix repairs and logical rank matter. Reset/reopen/remove/cost changes/pool lifecycle invalidate assumptions. | No production implementation; no invariant/repair-preserving bounded hint proven. |
| Reverse traversal/tail append | Retail tail may be stale/null, ties and cutoff differ, original forward repairs would be skipped. | Reject generic shortcut; do not enable fixed mode. |
| Auxiliary index | Must preserve first qualifying-stop identity, exact rank cutoff and every relevant repair side effect; needs complete mutation/alias validity proof and fallback. | Potential future investigation, not proven safe or implemented here. |
| Follow info links directly/locality rewrite | Noncanonical-owner fixture demonstrates a different cost/link route without a null owner. Pool/layout changes would broaden allocation/compatibility scope. | Reject assumed-invariant rewrite; no representation/pool change. |
| Heap/priority_queue | Neither approved nor equivalent to cutoff/tie/repair semantics. | Not implemented. |

Two-step proof is induction on original iterations: each copied step has the exact
original guard/body; after the first step the identical guard is re-evaluated before
the second. Stopping at null,>cost,5000 or repair-truncated end matches original.
Python verifies construction from the frozen body, including duplicated repair body;
identity/counter tests cover adversarial states. **Passing equivalence alone is not
permission to add a change with no demonstrated worthwhile performance benefit.**
No one bounded production optimization passes both gates in this task.

## Validation and changed files

Actually run on final C++ source:

- Full cmake --build build/win32 --config Release under x86 VS environment: PASS,
  both titles/tools (initial header rebuild and final test rebuilds; logs preserved).
- CTest Release:2/2 PASS.
- Direct g_googletest and z_googletest:132/132 PASS each; six disabled tests each.
- Focused RetailOpenList/profiler/path-phase/DeveloperHarness:58/58 PASS each.
- Explicit opt-in final benchmark:1/1 PASS per title; earlier cost-snapshot runs
  three sessions PASS. Timing was not run concurrently with builds/tests.
- Python/script/exact-reference suite:16/16 PASS.
- PowerShell AST syntax:9 scripts PASS.
- Preservation/token audit:391 earlier runtime/evidence files and accepted A.2
  capture hashes unchanged; old non-dev order audit passes; index empty.
- Final diff whitespace, source preservation and latest capture hashes: PASS.

Production algorithm .cpp is unchanged from task entry (retains only accepted B.1
surface hoist relative to HEAD). Production headers add only the named access friend.
Tests/Google/Core/CMakeLists.txt registers the new test. New test source, frozen body,
test-only unrolled body and scripts/performance/test_stage4b3_reference.py implement
the fixture/oracle/benchmark. Documentation adds this report and current-state notes
in ROADMAP.md and AGENT_HANDOFF.md. Stage4B.2 audit and prior work are preserved.

Reproduce tests/benchmark from repository root, no game launch:

```powershell
& .\build\win32\Tests\Google\Release\g_googletest.exe '--gtest_filter=RetailOpenList.*'
& .\build\win32\Tests\Google\Release\z_googletest.exe '--gtest_filter=RetailOpenList.*'
& .\build\win32\Tests\Google\Release\g_googletest.exe --gtest_also_run_disabled_tests '--gtest_filter=RetailOpenListBenchmark.DISABLED_ProductionAndTwoStep'
& .\build\win32\Tests\Google\Release\z_googletest.exe --gtest_also_run_disabled_tests '--gtest_filter=RetailOpenListBenchmark.DISABLED_ProductionAndTwoStep'
```

## Future Pathfinding 2.0 backlog and developer review

The next potentially meaningful **list** target is reducing repeated head-prefix
scans, rather than another setup hoist/unroll. Recommend a test-only bounded prefix/
rank-hint feasibility prototype using this actual oracle, with an explicit audit of
all reset/insert/remove/reopen/cost/pool/owner/repair writers. It must prove skipped
prefix checks cannot have effects, retain logical hop rank and cutoff and fall back
on unsorted/noncanonical/dangling state. Do not implement a production finger/index
until that proof and calibrated microbenchmarks show substantial benefit. This is a
conditional future backlog investigation, not a selected safe acceleration or an instruction
to redesign the list. No new capture/WPR/benchmark framework is needed for it.

If that narrow validity proof requires broad state/version machinery, stop this list
route rather than expanding architecture. The larger measured Internal line/checking
route remains the alternative; its production helper/path equivalence fixture is
still a prerequisite to reconsidering Stage4B.2. No movement cache is authorized by
this report. Zone/object/AI/render alternatives have no new evidence displacing the
repeated authoritative path tail.

No stage4b3-open-list candidate, hashes/inventory or developer game-launch command:
there is no selected production optimization to validate. Retain the already validated
Stage4B.1 interactive runtime unchanged. Generated logs/analysis stay ignored under
build; no stage/commit/push. Stop for developer review.
