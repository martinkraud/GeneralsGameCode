# Stage 4A.3: automated performance harness and minimal Developer Mode

**Closeout status (2026-10-09): PAUSED; not accepted representative automation.**
Neutral ownership and warmup goal relocation remain unresolved. Developer Mode
manual runtime validation has since passed. All launch/next-step instructions
below are historical; do not resume this scenario. Immediate next phase is X64.1
architecture/dependency audit and Win64 build foundation.

## Current priority — interactive Dev Mode / Stage4B.1 (2026-10-09)

Developer review explicitly PAUSES the Stage4A.3 relocation investigation and Fix6.
Do not rerun old scenarios or stage another diagnostic Fix6. All automation, forensic
snapshots, reference1–5, previous runtimes and working -skipIntro remain preserved.
The neutral-owner0 defect and goal_relocated_after_warmup remain unresolved harness
issues; neither is corrected or used as a prerequisite for current optimization.
Earlier "Stage4B locked" / "next diagnostic trial" instructions below are historical
and superseded by this priority decision.

Part A independent interactive Quick Game and actions are implemented and validated
without game launch. Explicit slot0 human / slot1 enemy ownership rejects neutral.
Part B implements one local surface-mask hoist in the accepted Internal line callback;
source-level exact-reference/order proof passes. No runtime speedup or gameplay success
is claimed. See [DEVELOPER_MODE.md](DEVELOPER_MODE.md) and
[STAGE4B1_PATH_POLICY_HOIST.md](STAGE4B1_PATH_POLICY_HOIST.md).

Next: developer runtime review of ONE fresh isolated stage4b1-devmode-pathfinding
candidate, first Quick Game/actions, then a bounded existing-profiler interactive
battle capture. No further five-AI selection capture required. No automation/Fix6
prerequisite. No launch, commit or push by agent. Future reproducible benchmark repair
remains a separate follow-up, after developer priority review.



## Current forensic review: Fix5 goal relocation; no Fix6 correction yet

Fix5 has been launched by the developer and failed deterministically at tick91.
Do NOT rerun Fix5 or use the historical launch commands below. Reference-5 is
preserved failure evidence, not a comparison baseline. Stage4B remains locked.
Root-cause evidence is incomplete, so no Fix6 correction/runtime or launch command
is prepared in this pass. The requested bounded getter-only diagnostic source is
implemented and validated for review; it is not present in the preserved Fix5
binary. Staging a diagnostic candidate is a separate next step after this review.

### Fix5 direct evidence and normalization proof

All three files were read directly from reference-5/trial-1: failure marker,
startup checkpoint and workload-failure-details. Matching launch receipt
807e85c2730b40b8b638bffa0a3878c2 and exit receipt show controlled code0; no new
dumps exist.153 candidates =96 accepted +3 validator/start-footprint +6 relocation
+48 spacing rejections.96 units,90 warmup ticks and76 orders preceded failure at
absolute tick91; capture never started. Unit293/index76/Redguard was stationary
at(600.5,1580.5); its prepared goal(600.5,3080.5) adjusted to(610.5,3080.5).
All96 recorded units remained at prepared starts. No performance reports or
fingerprints exist, so neither qualification nor Request/Internal/Closest chains
can be evaluated. Developer observation confirms **-skipIntro passed real runtime
validation**: startup movies skipped and gameplay map loaded normally. Intro code
is unchanged in this investigation. Earlier unverified statements below are history.

Exact harness route: orders(relative90) -> adjustDestination(obj, actual locomotors,
prepared goal) -> getRadiusAndCenter -> worldToCell -> center checkForAdjust ->
(if rejected) expanding spiral -> checkForAdjust on subsequent cells. Normal
successful commands then use AICommandInterface::aiMoveToPosition -> aiDoCommand
-> privateMoveToPosition -> state-machine AI_MOVE_TO -> AIInternalMoveToState::onEnter
-> adjustDestination/updateGoal -> requestPath -> normal queue/findPath/Internal
and normal fallback policy. Harness preflight fails before issuing unit293's command.

The output fractional part .5 proves the returned adjustment used center=false:
adjustCoordToCell emits(cell+0.05)*10 in that case; center=true would emit(cell+0.5)*10.
The prepared coordinate already maps to cell(60,308): floor((600.5+5)/10)=60 and
floor((3080.5+5)/10)=308. Successful center validation would reproduce the exact
prepared coordinate, not shift it. Therefore the center checkForAdjust failed.
The first spiral candidate increments x to61 with y308, whose normalized coordinate
is exactly(610.5,3080.5). This is a normal **one10-unit cell** adjustment, not floating
point drift or repeated-normalization accumulation. It does not prove the original
cell failed a reservation check: checkForAdjust also rejects cliff/bounds,
checkDestination failure, or adjusted-zone/connectivity failure. Within
checkDestination, causes include bounds, obstacle/ignored-obstacle rules,
impassability, allied goals and fixed-unit occupancy/crush policy. Fix5 did not
record those inputs, so the exact rejecting branch remains unknown.

Installed static map inspection: both original/replacement have clear stored5x5
cliff masks. MapsZH.big in Fix5 and the previously analyzed Fix2 runtime has identical
SHA25635cc8947f34f363d69f5045b9ac65f6ee164b8d5a382a1f7af213dbc2a744b96.
This rules out a different installed MapsZH archive and a stored-cliff explanation
there; it does NOT rule out runtime obstacles, water, changed classification or zones.

### Preparation versus ordering; sequencing proof limits

Preparation already calls the same normal adjustDestination, including its
checkDestination occupancy/reservation logic. It is NOT merely a static-connectivity
validator. It places each newly created object, validates/snaps its start and goal,
checks quick connectivity both ways, and enforces relocation<=10 and separation>=40.
The goal is stored but not reserved. Preparation occurs before the remaining army
is created and before90 ticks of world/AI updates. Order-time validation uses the
current object/locomotor/world state after warmup and demands goal idempotence within
0.1; it then checks forward connectivity. Thus the validation *primitive* is shared,
but time/world/geometry/reservation state and relocation assertion differ. No newly
invented dynamic-goal validator is missing from the preparation callback.

Orders0..75 can synchronously mutate reservations even without physical movement:
privateMoveToPosition enters the normal move state; onEnter adjusts destination,
calls updateGoal, then requests a path. updateGoal removes old goal cells and writes
new goal IDs over the unit footprint. It is not necessary to wait for the next
simulation tick for these writes. This proves sequencing is a possible mechanism,
not that it caused this failure. Fix5 lacks cell snapshots before orders or after
each order, so whether the goal was still acceptable before any order or specifically
after order75 cannot be established offline. Warmup-world changes remain possible.

Nearest earlier prepared goals to the failing point:

| Index / ID | Goal | Distance |
| --- | --- | --- |
|66 /283|(600.5,3040.5)|40.0000|
|67 /284|(645,3045)|56.9254|
|75 /292|(525,3085)|75.6340|

No earlier prepared goal equals the failing cell. These coordinates alone do not
establish actual reserved cells, geometry or which final goal normal state entry
used. Accepting the new10-unit destination or changing spacing would be speculative.
The0.1 tolerance,96-unit geometry,90/300 ticks, normal routing and qualification
thresholds remain unchanged.

An independent ownership issue is now proven: all96 units record owner0;
PlayerList::getNeutralPlayer returns m_players[0], whose init(nullptr) sets
PLAYER_COMPUTER. The existing 'first active computer' selection therefore selected
neutral. This contradicts the intended active skirmish-AI ownership description.
It is not proof of the specific relocation cause and is not changed in this pass;
it must be addressed before accepting the automated workload as the intended
benchmark. No claim of representativeness is made for either failed run.

### Minimum additional forensic observation implemented

Only scenario index76 is observed. A fixed100-snapshot buffer records its cells:
once immediately after its pair is accepted, once after all army preparation,
once before any movement orders, after each successful order and on preflight
rejection. A complete96-order run fits exactly100 snapshots; the known failing
sequence uses80. Each snapshot reads a fixed5x5 neighborhood around the original
goal, which covers the existing maximum radius2 footprint. It records tick/order
count, geometry radius, locomotor surfaces, layer/start zone, ignore ID, own goal
cell and owner/type; each cell records type/flags/raw zone, goal ID/owner/relationship,
obstacle ID/owner/bounded template name and ignored-obstacle membership.

No extra adjustDestination, quick-path query, reservation write, search, RNG draw,
wall-clock selection or command is performed by this observer. Public getCell,
worldToCell and cell getters were audited as lookup/read operations. The fixed
trace is written to goal-relocation-trace.json only during failure handling,
between measured outer frames after capture stop. All observations occur during
preparation/warmup/order preflight, before measured capture. Normal startup/devmode
never invokes them. Missing snapshots and dropped count are explicit.

This will distinguish a cell/reservation/geometry change during warmup from its
first transition after a specific issued order, and identify an owning unit or
building if present. It is not a replacement implementation of checkForAdjust and
does not manufacture an exact rejection code. If snapshots show no relevant change,
hidden effective-zone/checkForAdjust state would still need a narrower branch probe;
do not add that probe before the minimal evidence is reviewed.

### Review decision

**Underlying rejection cause remains unproven; no endpoint correction is justified.**
No tolerance increase, adjusted-goal acceptance, ownership change or Fix6 candidate
is included. The new observer and serialization/bound tests are available for
review. Do not launch the rebuilt executables directly or reuse reference-5.
A future explicitly reviewed diagnostic candidate must use a fresh runtime/output
root; an exact Fix6 trial command is withheld because the requested root-cause gate
for a corrected Fix6 candidate has not been satisfied. No game launched, commit,
push or Stage4B implementation.

### Forensic observer validation and Git state

Full x86 Release build of both games/tools passes (final exact-source incremental
rebuild included). CTest2/2, direct Google120/title, focused harness/profiler46/title
and Python11 pass. The new synthetic test verifies bounded100-snapshot retention,
overflow reporting, independent before/after-order metadata, fractional coordinates,
cell reservation serialization, output bound and failed-stream handling. It does
not claim to reproduce a live map or identify Fix5's unrecorded rejection branch.
Static/token audit passes: authoritative AIPathfind.cpp unchanged; ordinary engine/
logic/input/UI tokens preserved when gated hooks are removed; probe contains no
path search/adjustment/reservation/RNG call; intro source matches Fix5 exactly.
Diff check passes. All386 earlier snapshotted evidence/runtime/dump files and the
Fix5 JSON/launch/exit evidence retain hashes. Preserved Fix5 inventory passes369
paths; that binary intentionally does NOT contain the new observer source.
No Fix6 runtime exists, so new-runtime inventory/hashes and a Fix6 launch command
are not applicable. Accepted A.2 capture remains unchanged; Git index is empty.
Worktree remains19 modified plus9 untracked files (28 total), including earlier
uncommitted Stage4A.3 work. This pass edits only DeveloperHarness.h/.cpp,
DeveloperToolsRuntime.cpp, DeveloperHarnessTest.cpp and the three modernization
documents. Exact status is in ignored build/stage4a3-relocation-status.txt.
No generated capture/runtime/analysis files are staged.

## Current review state: Fix5 diagnostics + opt-in intro skip; ONE trial after review

Fix4/reference-4 is a preserved controlled failure, not a comparison baseline.
Fix5 does **not** claim to fix the rejected endpoint: the old marker cannot
identify its failing check. No endpoint, spacing, warmup, order or qualification
policy was changed speculatively. Stage4B remains locked. Do not rerun Fix4.
The historical fix4 preparation/validation below predates the developer launch.

From PowerShell7 in `C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode`,
non-launching checks for the new candidate:

```powershell
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage4a3-pathfinding-workload-fix5-final
./scripts/performance/Invoke-ZHPerformanceScenario.ps1 -Name stage4a3-pathfinding-workload-fix5-final -OutputRoot 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\performance\stage4a3-reference-5' -Trials 1 -ValidateOnly
```

**Only after developer review**, ONE real diagnostic trial (not executed here):

```powershell
./scripts/performance/Invoke-ZHPerformanceScenario.ps1 -Name stage4a3-pathfinding-workload-fix5-final -OutputRoot 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\performance\stage4a3-reference-5' -Trials 1
```

No rebuild/restage is needed for this prepared candidate. The scenario now
implicitly skips startup movies; normal Intro completion and shell initialization
still run. Twilight Flame loads, 96 units are prepared on the western banks,
and 90 logic ticks of stationary warmup precede the one-shot movement orders.
The initial camera may show the human base; no camera position was recorded in
Fix4. Keep the game active; no interaction is required. Both controlled failure
and success exit automatically. This command has no trial2. If it fails again,
send the entire trial-1 directory (including both failure JSONs and startup JSON),
console output and matching Fix5 launch/exit receipts. Stop for review; preserve
all evidence. If successful, send all reports and benchmark JSON as well.

## Fix4 real Trial 1 forensic review (2026-10-09)

Read both complete JSON files directly; these are the only files in
`build/performance/stage4a3-reference-4/trial-1`. Matching receipt
`launch-31e10c92e594402abb264da0966ee6dd.json` identifies the Fix4 EXE SHA256
`423928A40B7CAF0B5FC21116AF151D605AEE277D48BFCDCAE7FBE3C38E39AF51`.
Launch was 2026-10-09T07:04:47Z (rounded here); exit receipt records
2026-10-09T07:06:04.9973904Z, code0. Only the old Fix1 PID3592 dumps exist.

| Question | Recorded fact / justified limit |
| --- | --- |
| Shutdown | Controlled harness failure: fail clears pending capture events; beginOuter writes marker/checkpoint and calls setQuitting(TRUE). Exit0 agrees; no new crash dump. |
| Exact code | `workload_goal_changed_or_unreachable` |
| State/tick | `capturing`, absolute tick91; controller entered that state before issuing orders, **not** proof of active capture |
| Map/slots/players | map_requested/map_loaded/slots_ready/players_ready=1; normal offline Skirmish and active GameInfo readiness passed. Fixed runtime requests Twilight Flame. No map CRC persisted in this failed run. |
| Selection | 96 accepted pairs implied by 96 completed spawns; generated count96..192, rejected count0..96. Exact count/reasons/adjusted endpoints were not recorded. Insufficient pairs did NOT cause this failure. |
| Army | 96/96 recorded; source alternates48 Redguard/48 BattleMaster, owned by first active computer/default team. Actual IDs/player index/coordinates/camera were not persisted. |
| Orders | 76/96 recorded at tick91; first rejection is index76, the77th unit (Redguard by creation order). AI/alive checks for it passed before goal validation. |
| Warmup | 90/90 recorded; apparent idle is consistent with intentional warmup and an offscreen army, but camera/visual location cannot be reconstructed. |
| Capture | 0/300 captured ticks; start/end tick0 and profiler_running_at_failure=0. Pending Start was cleared on failure. |
| Profiler lifecycle | Configuration/detail prerequisites passed setup; capture never started, no measured stop/finalization. Runtime failure handling calls stopCapture on the inactive profiler. |
| Reports/fingerprints | None: no summary/categories/frames/slow/paths/benchmark JSON, CRC or RNG/path digest output. |
| Qualification | Never reached. Counts/shares/errors/drops cannot be inferred as measured zeros. Gate remains >=96 Internal, >=3 with20k pops AND500k attempts, >=3 of those sampling both phases, >=75% Internal+Closest pop share. |
| Performance comparison/chains | Unavailable for Fix4. No evidence that it improved or worsened reference-3's Closest dominance; no Request -> Internal -> Closest chain can be reconstructed. |

The first failing transition is warmup -> order issue, before capture. The
unchanged code rejects the first of: adjustDestination failure; squared adjusted
versus prepared goal distance >0.01; quick forward connectivity failure from the
unit's current location. Old evidence cannot distinguish these three branches
or the underlying occupancy/geometry/locomotor condition. In particular it does
not prove bad map geometry, insufficient pairs, a qualification failure or a
pathfinder defect. The76 aiMoveToPosition calls use normal CMD_FROM_SCRIPT routing;
there is no report proving when those units moved or requests entered the queue.
No full gameplay tick after the failed order batch is guaranteed by this evidence.

Reference-3 remains invalid for strict comparison (lossy original coordinates).
Its earlier directly analyzed307 Internal/max18.6461ms/zero>40ms versus223 Closest,
93 Closest>40ms/max73.8039ms remains descriptive evidence only. Accepted manual
A.2 example65.5024ms/39,332 pops/964,093 attempts returned a path; Fix4 has no
comparable path rows. Nothing here reopens the accepted Stage4B selection gate.

### Fix5: diagnostics first; no speculative scenario correction

The proven tooling defect is insufficient failure attribution and the validator
hiding the recorded reason. Split the existing short-circuit expression into
identical ordered checks, retaining all thresholds/calls/orders. Record first
failure_check, unit index, current/prepared/adjusted coordinates. Failure marker
also includes orders, warmup, profiler configuration and bounded selection totals.
`workload-failure-details.json` stores at most96 unit IDs/templates/owners,
prepared starts/goals/current positions/order flags, plus7 validator rejection
counters and first rejected coordinate examples. Selection totals separately
count validator rejection, excessive/nonfinite relocation and spacing, conserving
candidates. Bounds are the existing192 candidates/96 units. No additional
pathfinder call is used for diagnosis. Start footprint, start adjustment, goal
adjustment, layer, interface/radius, forward and reverse connectivity each have
an explicit first-rejection counter. Map-bound or obstacle ownership is not a
new independent check and is not falsely reported as a known causal counter.

All diagnostic disk writes occur between measured outer frames, after any capture
stop. The Python validator continues rejecting every failure marker; its terminal
JSON now includes the recorded reason/check/state/tick/orders/unit index, escaping
control characters. The wrapper already displays this validator output and stops.
No correctness/representativeness gate, coordinate precision/float parsing or
strict integer/fingerprint requirement was weakened. Qualification metrics remain
available from reports only when capture reaches finalization; failure before
capture does not synthesize them.

### Safe opt-in -skipIntro

Shared startup CommandLine registers `-skipIntro`; its handler sets only existing
m_playIntro=false AND m_playSizzle=false. A valid -performanceScenario invokes the
same handler and still disables its shell map. A process-local request bit
reapplies those flags in engine-init parsing after GameData INI loading; tests
simulate that overwrite and verify the request survives it. Explicit -skipIntro alone or with
-devMode leaves normal shell-map policy intact. No flags leaves default playback
unchanged. This is startup/developer workflow, not a persisted user preference.

Audit: Intro constructor uses these flags solely to select logo/custom screen,
Sizzle and associated waits. With neither flag, the ordinary update advances
Start -> Done, calls doPostIntro (m_breakTheMovie=true), and both titles' unchanged
GameClient update deletes Intro, shows shell map/shell, and services queued
save/replay. No constructor, GameEngine subsystem initialization, message processing,
menu initialization, map-load path or destructor is bypassed. Intro movie stage
selection is skipped, not arbitrary engine startup. GameInfo remains initialized
by normal MSG_NEW_GAME processing; scenario-only lazy SkirmishGameInfo allocation
and ownership remain intact. Existing full-Intro/movie/loading-map/loading-save/
clearing/offline/active-GameInfo guards remain unchanged. Therefore the Fix1
Sizzle/deferred-map/null-GameInfo route is still defensively gated. This occurs
before measurement;90 warmup/300 capture ticks, seeding and order semantics remain.
Real -skipIntro shell/scenario runtime behavior remains unverified until developer
validation; unit tests alone do not prove it.

### Fix5 validation

Full x86 Release build passes all59 initial incremental targets (both titles/tools),
then27 targets on the final INI-safe command-line revision.
CTest2/2 and direct Google tests119/title pass;45 focused tests/title pass.
New tests cover bounded conserved selection counts, metadata for the synthetic
76-order/tick91 failure boundary, and the actual startup command-line flags.
The fixture names a hypothetical relocation failure; it is NOT a claim that
Fix4 failed that branch. Separate processes for each title pass -skipIntro,
-devMode -skipIntro, and -performanceScenario pathfinding-heavy; the default test
checks unchanged movie defaults. Existing whole-Intro/readiness guards remain
covered.11 Python tests pass, including real CLI failure reason propagation,
terminal control escaping, strict schema/fingerprint and qualification cases.
No game was launched. No Stage4B implementation, commit or push.

### Final candidate identity and preservation

Prepared final runtime (never launched):
`C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\dev-runtimes\zh\stage4a3-pathfinding-workload-fix5-final\game`.
The earlier preliminary `stage4a3-pathfinding-workload-fix5` copy is retained,
never launched and superseded; use only the final candidate above.

| Artifact | SHA256 |
| --- | --- |
| generalszh.exe | `25E8C56CB5BE295A2211E54894C647FBB00E496719BA5E75480303320F3239D7` |
| generalszh.pdb | `8DFB7302AFB2294A601EC8D522A98DDDB5CB3B7504B2E94D8EC781C6E2AAAFFF` |

Final369-path inventory passes. Guarded one-trial ValidateOnly passes and creates
no reference-5 output, backup, launch receipt or user-data change. All386 files
in the pre-edit reference1..4/Fix4-runtime/old-dump snapshot retain their hashes;
prior Fix1 receipt/dump hashes and accepted A.2 capture hashes also pass.
Authoritative AIPathfind.cpp matches accepted baseline. Removing gated adapters
reproduces accepted engine/logic/input/UI tokens. Existing RNG seeding call count,
ID/storage bounds, startup guards and check ordering remain intact. Diff whitespace
check passes. Git index remains empty; generated artifacts stay ignored. The
current source/status snapshots are ignored under build/stage4a3-fix5-final-*.
There is no new runtime-performance or representativeness claim. Next review
must decide whether to authorize the one diagnostic trial, not Stage4B work.

## Architecture decision before implementation

Accepted starting point: `0ff7f9c9bf8dfe188015af36c7db8d5388756eda`, synchronized `dev/modern-engine`. Stage 4B remains unimplemented. Its selected route is Internal goal-directed line/movement checking; the accepted phase evidence does not require another manual five-AI selection battle.

Investigation found existing facilities suitable for reuse:

- `GameEngine::init` supports initial `.map` loading via `MSG_NEW_GAME`, but `-file` is debug-only and starts a single-player scripted map. The skirmish menu's `reallyDoStart` instead prepares `TheSkirmishGameInfo`, initializes its seed, and sends `MSG_NEW_GAME(GAME_SKIRMISH, ..., maxFPS)`. The harness will reuse that skirmish path with fixed slots, not repurpose campaign loading or synthesize a parallel game world.
- `GameInfo` owns slots/map/seed/cash; `MapCache` identifies installed map CRC, extent and start spots. `PlayerTemplateStore` resolves factions by name. Existing player/default-team creation remains in normal new-game processing.
- Script spawning uses `ThingFactory::findTemplate/newObject`, `setOrientation` and `setPosition`. Script orders use `AIUpdateInterface::aiMoveToPosition(..., CMD_FROM_SCRIPT)`. The harness will use these facilities, bounded IDs, fixed grids and tick-indexed orders, without changing pathfinding algorithms or budgets.
- `PerformanceProfileRuntime` already owns bounded capture buffers/report I/O and manual `command.txt` polling. Add explicit internal start/stop and report-observer interfaces, retaining manual mode; transitions must occur outside an active measured frame.
- Completed ticks are identified at `GameLogic::update`'s existing frame increment. A gated hook there schedules workload actions; it does not alter pacing or tick advancement. Warm-up/capture decisions use tick counts, never elapsed wall time or random draws. A wall-time guard may abort a stalled setup/run, never choose workload actions.
- `GetGameLogicRandomSeed/CRC` expose seed identity; `GameLogic::getCRC(CRC_RECALC)` exposes existing state CRC. Boundary checks will run outside measured frames. Existing path rows provide request/order/outcome/work fingerprints; no new per-node correctness instrumentation is justified here. CRC/path summaries cannot prove replay/save/network compatibility.
- `Keyboard::createStreamMessages` can consume a gated modifier chord before normal hotkeys. `DisplayStringManager`, `GameFont` and W3D UI drawing provide a small removable overlay. Existing `Player::toggleInstantBuild`, `Money::deposit` and `PartitionManager::revealMapForPlayer` avoid new cheat semantics.
- `GameEngine::setQuitting(TRUE)` is the normal clean loop-exit mechanism; profiler reports must finish before requesting it. Existing isolated runtime scripts stage and validate without launching.

Implementation boundaries: all behavior-changing actions require an explicit scenario/dev command-line gate and offline skirmish state; reject network and recorder playback. Automatic official captures hide the overlay and reject manual dev actions. No arbitrary spawning console, new serialized state, RNG control draws, game worker threads or Stage 4B changes. A pure controller and comparison tool will test state/validity independently from the real engine. Automatic setup and workload coverage remain unproven until guarded developer runtime validation; missing map/templates/setup or capacity/errors must fail clearly rather than report a valid speedup.

The implementation and validation below follow this decision. No game launch, commit or push is authorized in this task.

## Implemented interface and scenario version 1

`-performanceScenario pathfinding-heavy -performanceProfile <existing-absolute-directory> -pathProfile -groundInterpolation` requests the official scenario. `-devMode` separately enables manual tools. Both are default OFF. An unknown scenario fails closed; missing profiler configuration/details, unavailable map/templates, wrong game mode, playback/network state, a reversed logic clock, capacity/error stop or setup exception fails the run. Official runs suppress the shell map/intro, manual profiler polling and all dev shortcuts/overlay. Manual profiling retains `command.txt` and its existing limits.

The automatic match is an offline skirmish on `Maps\Twilight Flame\Twilight Flame.map`: China human slot 0/start 0 and allied easy China AI slot 1/start 1, colors 0/1, team 0, starting cash 10,000, fixed ordinary game seed `0x4a3001` (4,861,953), runtime render cap 120. It does not enable stats. Alliance prevents immediate combat/victory from dominating the short benchmark; the ordinary AI economy still runs. The actual game seed, map CRC and boundary RNG state are recorded. Interpolation comes from the explicit flag and is recorded. No preference is persisted by the harness.

At the first completed skirmish tick, spawn 96 AI-owned ground units, alternating 48 `ChinaInfantryRedguard` and 48 `ChinaTankBattleMaster`, through the normal factory/default team. The grid starts at (22%,22%) of terrain extent; twelve columns, 24 world units apart, eight rows. Height comes from terrain. IDs are retained in a fixed 96-entry array. Missing templates, inactive owner, missing AI interfaces or extent smaller than 1,000 units fails setup. No retry/random placement occurs; this preserves repeatability but does not guarantee every grid location is navigable.

Issue ordinary script movement orders at relative tick 0, tick 90 and every positive multiple of 120 thereafter. Destinations alternate between (78%,22%) and (22%,78%) of extent, with per-unit offsets of 8 units in the same grid. Orders use `CMD_FROM_SCRIPT` on AI-owned units, so they exercise nonhuman production pathfinding. The map supplies terrain, obstacles and chokepoints. This creates long paths, simultaneous requests and repeated crossings, with live object/AI load. Pool exhaustion, null-after-work and Closest retries are observed if the production search encounters them; the harness does not force failures or alter policy. It does not reproduce the entire five-AI battle.

The pure `Controller` transitions Setup -> Warmup (90 completed ticks) -> Capturing (300 completed ticks) -> Finalizing -> Complete/Failed. Orders execute at the existing completed-tick hook. Start/stop, CRC calculation and report finalization are deferred to the next outer-frame boundary, outside measured scopes. The capture contains exactly 300 completed simulation ticks (nominally ten seconds at 30 TPS), rather than ten wall-clock seconds. Capacity/60-second profiler guards remain unchanged; they invalidate the official benchmark rather than changing production capacity. A 180-second wall guard only aborts active-loop setup/run failure; it does not choose workload decisions and cannot interrupt a blocked load/driver call. Completion/failure requests normal `setQuitting(TRUE)`; no forced process kill is added. A failure may exit with the engine's ordinary zero process code, so report validation is mandatory.

Automatic match creation, spawning, warm-up, orders, capture, report finalization and exit are implemented. No runtime game was launched to prove them. The developer still must review the candidate, launch the guarded trial script, keep the application active and avoid input/pause/camera/settings changes. The installed asset/configuration identity must stay fixed. Real five-AI matches remain milestone checks; another manually assembled five-AI selection battle is not requested.

## Outputs, correctness and comparison

Each successful reporting attempt retains all existing five profiling reports and appends `<stem>-benchmark.json` (schema 1). It identifies source/build/title, scenario/version/configuration, map/CRC, requested/actual seed, warm-up/capture ticks, frame/tick counts, runtime requested/effective FPS, interpolation/detail/phase/overlay modes, unit/end-object counts, elapsed time, dropped/errors, completion/reason, Internal/severe counts and queue maximum. Files remain under the explicit directory; no output goes to the Steam installation. Fresh trial directories prevent mixing or overwriting results. `benchmark-failure.json` marks setup/timeout/early-stop failure where the configured output directory remains available. Missing/unwritable output is a failed run, never an implicit valid result.

Start/end `GameLogic::getCRC(CRC_RECALC)` and logic RNG CRC are computed outside measured frames. A bounded 64-bit FNV-1a digest encodes each retained path record in order: ID/parent, kind/request, relative logic tick, object ID, outcome, endpoints and existing work counters. Encoding uses explicit eight-byte words, not pointers/struct padding/clocks. The comparison tool recomputes this digest from the CSV. Inclusive work counters appear in parent and child rows intentionally for identity, never summed as performance time. This checks request/dispatch/result relationships and workload identity but **does not contain exact result path nodes, costs, parent chains or expansion sequence**. Hash collisions and unobserved state remain possible. CRC/hash agreement is a useful rejection gate, not proof of retail replay, save, multiplayer compatibility or global determinism. Future Stage 4B still needs the previously specified differential path tests and milestone replay/save/lockstep validation.

`scripts/performance/compare.py` uses only Python's standard library. `--validate <trial>` reads the metadata plus every report, checks bounded sizes/row counts, capture completion, tick/cap/configuration identity, zero errors/drops, IDs/parents/frames, phase selection/nonnegative self and CSV fingerprint. It derives logic mean/p95/p99/max, Internal self p95, expensive-search/pop/failure coverage and sampled phase totals. It does not multiply phase samples by 64. This validator is intentionally stricter than the exploratory Stage 4A.2 partial-tail analysis: an official automated benchmark must complete without dropped records.

`--reference <trial> --candidate <trial>` accepts repeated arguments for multiple independent fresh trials. Source/build identities may differ; scenario configuration, caps, map/seed, start/end ticks, boundary CRC/RNG and path digest must match **across all reference and candidate trials**. A mismatch reports `not_comparable`, not a speedup. Performance uses medians of trial metrics; the default descriptive threshold is 5%. A >5% increase in any reported metric is `regression`; otherwise >5% reduction in logic p95 is `improvement`; remaining changes are `unchanged_within_noise`. This is a conservative descriptive rule, not significance estimation. Single runs are explicitly not statistically significant. Exit codes are 0 valid/improvement/unchanged, 1 regression, 2 not comparable, 3 invalid. Threshold can be explicitly adjusted after observing repeatability; do not relax correctness gates to make a result pass.

Before treating a reference as relevant to the selected optimization, the tool conservatively requires at least three Internal searches over 40 ms and one Internal search with at least 10,000 head pops in each reference trial. This is a workload qualification heuristic, not a new target-selection rule or required timing for an optimized candidate. Validation reports coverage independently, so a fast candidate is not rejected merely because it eliminated >40 ms stalls. **This first scenario has not yet met that runtime qualification: its actual coverage, CRC repeatability and absence of capacity overflow must be checked by the developer.** If it is too light or spawns into unsuitable terrain, adjust only the scenario's bounded layout/order schedule after inspecting its reports and increment its version. Do not tune pathfinding policy or ask for another manually built battle.

## Minimal Developer Mode

`-devMode` enables a small existing-DisplayString overlay and reserves **Ctrl+Alt+Shift+F5 through F11** before normal hotkey translation. Existing function keys and gameplay/UI bindings remain unchanged without the gate/chord. This triple-modifier combination avoids ordinary meta/UI hotkeys; user-custom bindings may conflict while Developer Mode is explicitly enabled. Key-down is armed and key-up consumed even if modifiers were released first, avoiding a leaked normal key-up. Autorepeat does not repeat the action. All state-changing actions are queued to a completed offline skirmish tick; capture controls run between outer frames. Network, replay, campaign, shell and inactive-player actions are rejected. Scenario runs reject every shortcut.

| Chord suffix | Action |
| --- | --- |
| F5 | Toggle overlay (available without a live skirmish) |
| F6 | Deposit 100,000 into the current active local player's money, without income tracking/sound |
| F7 | Existing instant-build toggle only where already compiled; unavailable in the tested Release build, with a debugger diagnostic |
| F8 | Existing temporary map reveal for the current local player; normal shroud behavior may return |
| F9 | Spawn predefined 12-unit mixed group |
| F10 | Spawn predefined 96-unit mixed army |
| F11 | Start/stop the configured manual profiler internally; no command-file editing |

Manual spawning uses the same fixed map-relative grid and local default team, with a cumulative maximum of 192 units per process. It can overlap previous spawns or terrain; no arbitrary object console/editor is added. Start a fresh process for another bounded batch. Instant build was deliberately not broadened into Release: its methods and production consumers are under existing compile-time cheat gates. Predefined spawning provides the needed fast manual army setup. Scenario reset/restart is a fresh guarded process, avoiding partial reset of transient state. No new state is serialized; manual spawned objects are ordinary objects and existing game save/replay side effects still apply. These shortcuts are not recorded as replay commands, so do not claim replay compatibility for dev-cheated sessions.

The overlay shows existing average FPS and frame-time estimate, observed tick/TPS, last captured logic time, profiler OFF/READY/RECORDING/COMPLETE, path mode, elapsed/guard duration, record capacity/drops/scope errors and shortcut hints. Last logic timing is zero/unavailable before a profiled frame and remains the last captured value after completion. Text/statistics refresh at most twice per second; drawing uses the existing UI batch. No per-frame object scan or percentile calculation. Official scenario state is recorded in JSON and the overlay stays hidden throughout official captures. Visible overlay cost has not been measured in a game.

## Disabled and enabled overhead

With both gates OFF, adapters only read mode booleans/return; they perform no clocks, file I/O, allocations, CRC calculation, spawning or RNG calls. The accepted pathfinder/measurement implementation is unchanged. The disabled adapter bundle microbenchmark (two gate queries plus hidden overlay return) measured medians 2.641 ns/iteration Generals and 2.543 ns Zero Hour, against 0.358/0.374 ns volatile-loop references: illustrative incremental differences 2.283/2.169 ns. Enabled pure tick control measured 2.676/2.410 ns/iteration. These are seven-repetition within-binary synthetic medians, not a measured game FPS delta or rigorous upper bound. Actual engine hooks also include small gate checks at logic/input boundaries. Their few calls per frame/tick do not plausibly constitute meaningful normal-game overhead under this model.

Official enabled runs additionally poll the guard/state at outer boundaries and run a constant-space controller per tick. Workload creation/orders intentionally create real game work. CRC, bounded digest and metadata/report writing occur only at capture boundaries outside measured frames. Existing detailed/phase profiling costs remain: regression benchmark sampled-phase medians were 116.257/118.989 ns per synthetic iteration for Generals/Zero Hour, comparable in scale to Stage 4A.2 and subject to its documented limitations. Visible overlay GPU/text cost and automatic workload runtime overhead are **unmeasured** until developer validation; hidden overlay executes its early gate only.

## Validation actually run

Full x86 Release build for both titles and tools passed. The first build attempt lacked Visual Studio's INCLUDE environment; rerunning in the installed x86 VsDevCmd environment exposed and fixed the title-specific faction lookup and compile-time instant-build differences. Final build passed. CTest passed 2/2; direct Google tests passed 109 per title (218 total); focused developer/profiler suites passed 35 per title; all four explicitly enabled profiler/overhead benchmark tests passed per title. Six Python comparison tests passed, and all new PowerShell scripts parsed successfully. No gameplay, replay/save or network run is claimed.

Ten new Google tests cover default/exact CLI gates, disabled/rejected dev actions, tick transitions/repeated-render independence, failure/reset/timeout, spawn capacity/overflow bounds, real profiler start/stop/report integration and active-frame rejection, metadata generation/escaping/stream failure, and chord release handling. Tests isolate controller/metadata behavior; no live map is started. Python fixtures cover complete input loading, missing/oversized files, failure/digest/parent/phase rejection, configuration/CRC/coverage mismatch, repeated-trial descriptive verdicts and regression safeguards.

Static token audit reproduces all accepted engine/logic/input/UI tokens after removing gated adapter statements. `AIPathfind.cpp` and `PerformanceProfile.cpp` have no diff from accepted `0ff7f9c9b`; no sorting, pool, hierarchy, retry, queue, simulation/RNG/FP algorithm or serialized gameplay fields changed. Pure control uses no RNG/wall clock. The only `InitRandom` is inside gated ordinary new-game setup. Accepted final Stage 4A.2 reports/command hashes are unchanged; index remains empty. Build/test/audit logs and temporary helpers live under ignored `build/stage4a3-*`.

## Guarded candidate and exact developer procedure

Fresh candidate: `C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\dev-runtimes\zh\stage4a3-performance-harness\game`. Earlier runtimes remain intact. Full source, new candidate and previous Stage 4A.2 inventories each pass 369 file paths/lengths/SHA-256 values. Candidate identity:

| Field | Value |
| --- | --- |
| Build / source | x86 Release; accepted `0ff7f9c9bf8dfe188015af36c7db8d5388756eda` plus dirty Stage 4A.3 worktree |
| PDB GUID / age | `5706bc8d-51e9-49fc-9096-a636d0fd56be` / 25 |
| EXE SHA-256 | `FD579BA614EE3FB4B109C84A62AA4439E7A4E7CA4220B18067494AEE4FA32BE0` |
| PDB SHA-256 | `1EF39F784DD55E0473DF8635F0E2A691FCB3FB460F3C96BF44A01946A11913F1` |

The new wrapper was exercised for two trials **with `-ValidateOnly`**. It validated guarded launch argv and backup resolution without creating the proposed output root, backups, launch receipts or processes. PowerShell child-process adapters preserve array arguments and prevent the existing launch script's `exit` from ending the multi-trial loop. The wrapper bounds trials to 1â€“10, requires a fresh child directory under repository `build/performance`, rejects links, reuses guarded user-data backup on each actual launch, waits for clean exit and validates each completed trial. `-Build` optionally invokes the existing x86 Release build (run from an x86 Visual Studio developer shell); `-StageSource <Steam-path>` optionally stages a new distinct named runtime. Validate-only skips build/copy/write/launch. The agent has not executed either actual launch command below.

From PowerShell 7 in `C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode`, first perform read-only checks:

```powershell
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage4a3-performance-harness-fix2 -CheckSource
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage4a3-performance-harness-fix2
./scripts/performance/Invoke-ZHPerformanceScenario.ps1 -Name stage4a3-performance-harness-fix2 -OutputRoot 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\performance\stage4a3-reference-3' -Trials 3 -ValidateOnly
```

After developer review, run the same command without `-ValidateOnly` to launch three guarded automatic trials (a dedicated test Windows account is preferable because the existing engine uses shared user data; the wrapper makes verified fresh backups):

```powershell
./scripts/performance/Invoke-ZHPerformanceScenario.ps1 -Name stage4a3-performance-harness-fix2 -OutputRoot 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\performance\stage4a3-reference-3' -Trials 3
```

Use a new output suffix if it exists. Keep the application active and hands off, with identical resolution/settings/camera and assets; no Alt+Tab capture control is needed. Expected flow: ordinary startup Intro completes -> automatic map -> immediate army -> three-second nominal warm-up -> ten-second nominal logic capture -> reports -> exit -> next trial. Review each `--validate` output's coverage. Compare unchanged trials first:

```powershell
python -B scripts/performance/compare.py --reference build/performance/stage4a3-reference-3/trial-1 --candidate build/performance/stage4a3-reference-3/trial-2 --candidate build/performance/stage4a3-reference-3/trial-3
```

Require complete reports, zero drops/errors, exact 300 capture ticks, stable caps/settings and matching CRC/RNG/path fingerprints. Require qualification/coverage of expensive Internal work before freezing this as the frequent Stage 4B reference. This is validation of an automatic scenario, **not another manual five-AI target-selection capture**. If automatic setup fails, preserve all output and report the specific failure; do not continue to optimize against an unqualified workload. A blocked loader/driver may still require manual close because the active-loop timeout cannot run while blocked.

For later reference/candidate comparisons, preserve the unchanged reference runtime and its trial directories. Stage each changed candidate under a fresh name using the guarded workflow, run the same versioned scenario into a fresh root, then pass all three reference paths and all three candidate paths with repeated `--reference`/`--candidate`. Do not compare different scenario versions, assets, commands, caps or fingerprints. The tool reports descriptive medians and does not establish significance. A reference/candidate correctness mismatch requires investigation, not threshold adjustment.

For separate manual dev checks, create a fresh profiling directory and launch through the existing guard with `-BackupUserData -GameArguments @('-devMode','-groundInterpolation','-performanceProfile',<absolute-directory>,'-pathProfile')`. Start an ordinary offline skirmish, verify F5 toggles the overlay, F6 money, F8 reveal, F9/F10 bounded spawning and F11 start/stop. F7 is intentionally unavailable in this Release build. Confirm no shortcut activates without `-devMode`, and state-changing shortcuts are rejected in network/replay/campaign/shell contexts. Do not use a cheated session as replay/save/lockstep evidence. No reset shortcut is provided; restart the process safely for another scenario/batch.

Final state: source/tests/docs/scripts are uncommitted and unstaged; generated artifacts remain ignored. No game was launched, no Steam files were overwritten, and no previous runtime/capture was removed. **Next: developer review and three automatic unchanged-reference trials; then verify coverage/repeatability before the first Stage 4B implementation.**


## First real trial failure and fix1 (2026-10-09)

This section supersedes the original unlaunched-candidate next-step statement above.
The developer launched the original candidate once. Preserve
`build/performance/stage4a3-reference-1/trial-1` unchanged; the next root is
`build/performance/stage4a3-reference-2`. Trials 2/3 never started. No game was
launched during this investigation.

### Evidence and progress

A direct recursive inventory found exactly one generated file: the 85-byte
`benchmark-failure.json`, containing schema 1, failed status and reason
`scenario_map_or_offline_setup_unavailable`. Its SHA-256 is
`03674421970977549052679276D483E35CEB37D90DEB1E0649DDEE1A0F1CBDD3`.
There is no benchmark result, summary, categories, frames, paths, slow report or
other diagnostic file in this trial. No partial performance evidence exists.

Matching receipts are `launch-f8b49b9011584aa4b887659b62cc6ed7.json` and its
`.exit.json` under the original runtime. They identify the original EXE hash in
the table above, the expected scenario/interpolation/profile/path argv, and a
verified user-data backup. Start was `2026-10-08T22:34:10.3914254Z`; normal exit
code 0 was recorded at `2026-10-08T22:34:16.7694440Z` (6.378 seconds later).

The failure route was `GameEngine::execute -> DeveloperTools::beginOuter ->
startScenarioGame -> false -> fail -> stopCapture("scenario_failed") -> failure
marker -> setQuitting(TRUE)`. It occurred on the first scenario outer boundary
in **Setup**, before any `MSG_NEW_GAME` request or scenario tick. The marker did
not record the absolute GameLogic frame; that numeric value cannot be recovered
and must not be reported as tick 0. No scenario players/slots were configured;
zero of 96 scenario units were created; no scenario movement orders were issued;
warm-up and capture never began. Profiler configuration was accepted, but no
capture started; the failure stop was an inactive no-op. Twilight Flame was not
requested or loaded as the scenario. Startup map-cache parsing or a shell map is
not proof of a successful scenario match.

### Exact cause and correction

`startScenarioGame` incorrectly treated a null `TheSkirmishGameInfo` as an
unavailable startup subsystem. This singleton starts null and is allocated
lazily by `SkirmishGameOptionsMenuInit` in both titles (or by saved-game loading).
The automatic scenario bypasses the menu, so the original predicate rejected
ordinary fresh startup before even checking the map/faction metadata. This is a
harness initialization/ownership assumption, not evidence of an invalid Twilight
Flame map, missing unit template or a pathfinder/profiler failure.

The scenario now allocates `NEW SkirmishGameInfo` when absent, after validating
offline/startup/map/faction requirements and clearing any shell game. It then
uses the existing init/closed slots/local IP/enterGame setup. Existing instances
are reused. Ownership remains with GameEngine, which already deletes the
singleton at shutdown. Allocation requires the exact valid scenario gate,
GlobalData and absence of Network; normal gameplay and Developer Mode do not
allocate it through this helper. No sleep, retry, map/player/workload change,
validation relaxation or production pathfinding change is introduced.

Startup rejection reasons now distinguish network/replay/live match, missing
startup subsystems, uncached map, unavailable multiplayer map and missing China
template. Bounded failure metadata adds the state before failure, absolute logic
tick, spawned-unit count, game-requested flag, profiler-running flag and capture
boundary ticks. This is diagnostic state only and is never used for workload or
simulation decisions. A failed marker stream emits a debugger diagnostic; missing
reports still fail strict validation.

The normal code 0 is intentional clean failure handling: GameMain initializes
`exitcode=0`, returns from execute after quitting and performs normal destruction.
Benchmark failure does not change that process exit code. The marker/report
validator is authoritative. Its rejection stopped the wrapper before trial 2,
exactly as intended. The short bounded receipt interval and normal exit provide
no evidence of a hang or crash; no visual success is inferred from code 0.

### Regression and validation

`AutomaticScenarioAllocatesMenuOwnedSkirmishInfo` exercises the real singleton
allocation with the test runner's initialized GlobalData: default/invalid gates
cannot allocate, a valid scenario allocates from null, the object supports its
normal init/enter/slot operations, and repeated ensure preserves both pointer
and slot setup. It cleans up through normal deletion. It deliberately avoids
localized UI calls and does not start a map. `FailureMarkerRecordsStateTickAndProgress`
checks the diagnostic state/tick/progress fields, JSON quoting and stream failure.
The existing Python failure-marker rejection test remains unchanged.

Initial test-fixture attempts incorrectly assumed GlobalData was absent and then
called an unloaded UI localization service; these were corrected in the test,
not by changing production services. Only final passing validation establishes
acceptance. Runtime setup, workload coverage and repeatability still require
fresh developer trials; unit tests alone do not establish them.

Final validation results and the new candidate identity are recorded below.

The final incremental full x86 Release build (both titles and tools) passed.
CTest passed 2/2; direct Google tests passed 111 per title, focused
DeveloperHarness/PerformanceProfiler/PathProfiler/PathPhaseProfiler tests passed
37 per title (including all 12 DeveloperHarness tests), and all four explicitly
enabled synthetic profiler/overhead benchmarks passed per title. Six Python
comparison tests passed. Static/token audits confirm the accepted pathfinder,
observer measurement algorithms and ordinary engine/logic/input/UI tokens remain
unchanged after removing gated adapters. The original failed trial still returns
invalid/exit 3 because its marker is present. No runtime success is claimed.

Fresh guarded candidate (never launched):
`C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\dev-runtimes\zh\stage4a3-performance-harness-fix1\game`.
The original candidate remains preserved separately. Candidate identity:

| Field | Value |
| --- | --- |
| Build/source | x86 Release; `0ff7f9c9bf8dfe188015af36c7db8d5388756eda` plus dirty Stage 4A.3/fix1 worktree |
| PDB GUID / age | `5706bc8d-51e9-49fc-9096-a636d0fd56be` / 27 |
| EXE SHA-256 | `C223F62FCAC6C0F8A0983999DA48F63D9B08B224EF3DE14511C37A9E81E1E031` |
| PDB SHA-256 | `86FF3968B1CBBB0EDD8D634936DC56D9BDB037173960ED4AB7A98E2FB3CA6077` |

Use the exact commands in the developer procedure above, now updated to
`stage4a3-performance-harness-fix1` and **fresh** `stage4a3-reference-2`.
The one `-Trials 3` command launches sequentially, validates each trial and stops
on the first failure; successful trials exit automatically before the next starts.
Do not rerun into reference-1. Keep all reports and send back the output path,
validation/comparison terminal output and visual observations. Actual scenario
setup, qualification and CRC/RNG/path repeatability remain pending developer
review and the next manual authorization/run. No Stage 4B was implemented.

Post-stage verification also passed: installed source, fix1 runtime and preserved
original runtime each match all 369 inventory paths/lengths/SHA-256 hashes.
The guarded three-trial wrapper passed `-ValidateOnly` for fix1/reference-2
without creating output directories, receipts, backups or a game process.
Final `git diff --check` passed; the index is empty. The failed marker hash and
sole-file inventory are unchanged. Runtime/build/audit artifacts remain ignored.


## fix1 runtime crash and fix2 (2026-10-09)

This section supersedes fix1's next-step recommendation. Do not rerun fix1 or
reuse reference-1/reference-2. Preserve both runtimes and all evidence. No game
was launched during this forensic investigation or fix2 preparation.

### Exact run, dump identity and useful stack

The matching receipt is `launch-8bef4ce28b8345978ec9a5e7faea86f2.json` under
`build/dev-runtimes/zh/stage4a3-performance-harness-fix1`, selected by its actual
EXE path/hash, scenario argv and reference-2/trial-1 output path. It records a
verified user-data backup and launch `2026-10-08T22:48:26.3872766Z`. Its exit
receipt records `-1073741819` at `2026-10-08T22:48:35.8241579Z`. This was an
unhandled access violation, not the previous controlled code-0 failure.
The trial directory is empty: no marker/result or partial profiler report.
There are no new runtime log files associated with the crash; the runtime's
old `game.dat-DESKTOP-RT3SH8S.log` predates this run.

Matching user-data CrashDumps:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `CrashMZ-20261009-004831-0ff7f9c9b-pid3592.dmp` | 323468 | `651C54A93F7F08D3705D350AE7DCED0CD8EE97C71F8831B81BAE4E08CBD2632D` |
| `CrashFZ-20261009-004831-0ff7f9c9b-pid3592.dmp` | 508091437 | `D044D19A07B6E6ABC2A927CF91C12255371727367A3883409BE875F034DF6553` |

The minidump's module points to the exact fix1 staged EXE, base `0x00f90000`,
image size `0x006c0000`. Its RSDS identity is GUID
`5706bc8d-51e9-49fc-9096-a636d0fd56be`, age 27. The preserved EXE/PDB hashes match
fix1's table. The local DbgHelp reader loaded that module/PDB, recovered x86
context and unwound the useful stack from full-dump memory. No current rebuilt
PDB is substituted. Original dumps are read-only; analysis helpers/logs live
under ignored `build/stage4a3-fix2-*`.

Exception: `0xC0000005`, **read address `0x00000008`**, thread **22644**, PID
**3592**, instruction `0x0107c271` (`generalszh+0xec271`). Captured ECX is zero;
fault bytes `f7 71 08` are `div dword ptr [ecx+8]`, the CRC interval access.

| Frame | Address | Symbol / source line in crashing build |
| --- | --- | --- |
| 0 | `0x0107c271` | `GameLogic::update+0x1a1`, GeneralsMD GameLogic.cpp:3757 |
| 1 | `0x0106bd6d` | `GameEngine::update+0x18d`, GeneralsMD GameEngine.cpp:933 |
| 2 | `0x00fb2c39` | `Win32GameEngine::update+0x9`, Core Win32GameEngine.cpp:94 |
| 3 | `0x01069d8d` | `GameEngine::execute+0x9d`, GeneralsMD GameEngine.cpp:1000 |
| 4 | `0x0105c4b3` | `GameMain+0xa3`, GeneralsMD GameMain.cpp:59 |
| 5 | `0x00fb1523` | `WinMain+0x313`, GeneralsMD WinMain.cpp:921 |
| 6 | `0x014413b0` | `__scrt_common_main_seh+0x140`, CRT exe_common.inl:288 |
| 7–9 | `0x751a5d49`, `0x76fec75b`, `0x76fec6e1` | OS addresses; no local OS PDB names claimed |

First trustworthy project-owned/faulting line:
`Bool generateForMP = (isMPGameOrReplay && (m_frame % TheGameInfo->getCRCInterval()) == 0);`
No CRC guard or gameplay line was modified to conceal the fault.

### Dump-derived startup progress and exact cause

PDB type/member layouts applied to full-dump memory establish:

- Developer controller is **Setup**; `startedGame=1`, `unitCount=0`, all capture
  boundary/CRC/RNG values remain zero; failure string is empty.
- GameLogic frame is **0**, mode **GAME_SKIRMISH (2)**,
  `m_loadingMap=1`, `m_loadingSave=0`, `m_startNewGame=1`.
- `TheSkirmishGameInfo` is nonnull (`0x07191838`), inGame/inProgress are true.
  Slots 0/1 are accepted human/easy-AI, colors/starts 0/1, faction index 3,
  team 0; slots 2–7 are closed. Map is Twilight Flame, seed `0x4a3001`,
  CRC `0xf0f7e3eb`. Thus fix1 allocation and slot configuration completed.
- `TheGameInfo` is **null**. PlayerList slot-to-player indices are all -1;
  scenario player construction had not completed.
- Display has both video stream and buffer, and its captured movie name is
  **Sizzle**. This exactly satisfies `Display::isMoviePlaying()`.
- Profiler enabled/details/automatic are true, but its recorder pointer and
  serial are zero. Recording never began; there is nothing to finalize.

Twilight Flame **was requested**, but actual scenario loading did **not**
complete. `prepareNewGame` sets GAME_SKIRMISH; the first `tryStartNewGame` sets
loadingMap, resets frame to zero, sets startNewGame and returns intentionally.
The next logic update completes loading only when no movie plays. Because the
harness submitted MSG_NEW_GAME before Intro completed, Sizzle was still active:
the loader was skipped and TheGameInfo was never selected. The same update then
reached multiplayer-style CRC generation and dereferenced that null pointer.
RecorderClass::isMultiplayer classifies a recording skirmish as multiplayer for
this CRC path; this does not mean an actual network session was created.

Zero scenario units and **zero scenario orders** were issued: dump unitCount
and Setup state, together with the only order call sites after successful
spawning, establish this. Warm-up, capture and profiler recording never began.
There is no benchmark result because the unhandled AV occurred before a
completed scenario tick or report-finalization boundary. The wrapper correctly
rejected missing results and stopped before trials 2/3.

### Narrow correction and surrounding-dependency audit

Fix2 waits in Setup before any match mutation/request until all conditions hold:
GameClient's full Intro sequence is gone, Display has no movie, and GameLogic is
neither loading a map/save nor clearing data. A read-only **nonvirtual**
`isStartupIntroComplete()` accessor in both title headers observes the existing
`m_intro` pointer; it adds no field, vtable entry, normal-game call or scheduling
change. Waiting for absence of a movie alone is insufficient because Intro has
movie-free waits/logo screens and can start Sizzle on a later client update.
GameClient deletes Intro only after its final stage, then performs the normal
shell-map/menu handoff. The outer-boundary guard observes that completed state.
The existing 180-second active-loop abort remains; no arbitrary sleeps, forced
movie skipping, RNG reseeding during waits or runtime retry is added.

The allocation/ownership fix1 path remains. The audit covered Intro's lifecycle
and shell handoff, new-game readiness checks, shell clear/reset, slot assignment,
map pending-file selection, seed/CRC metadata, deferred startNewGame, active
GameInfo selection, PlayerList construction, completed-tick spawn readiness,
profiler boundaries and shutdown ownership. The completed-tick harness also
requires no load/clear in progress and the normal selected GameInfo equal to its
skirmish singleton before spawning. It does not assign TheGameInfo itself or
bypass normal loading. No production CRC, pathfinding, pacing, pool, RNG or
normal menu behavior is changed; no menu initialization is broadly duplicated.

`scenario-startup.json` is a bounded diagnostic checkpoint, not a benchmark
result or a replacement correctness gate. It includes startup substage,
state/tick, intro/movie/readiness, map request/load, slots/players, units/orders,
warm-up progress and profiler state. Memory progress advances at meaningful
startup/spawn/order/capture boundaries; unit/order counters are bounded by the
existing scenario. Tick callbacks only update memory and defer file writes to
an unmeasured outer boundary. There is no per-tick file I/O or new normal-game
work. A hard crash may leave the last persisted boundary; full dumps retain
more recent in-memory progress. Missing diagnostics never imply success.

Regression `StartupWaitsForWholeIntroAndAllLoadingTransitions` rejects the exact
Sizzle condition, movie-free inter-stage gaps, movie-after-intro, and map/save/
clear transitions, then accepts readiness; repeated render checks cannot make
an unready state ready. `StartupCheckpointRecordsBoundedProgressAndEscapes`
checks bounded JSON, progress fields, escaping and stream failure. Existing
allocation/default-off/failure/capture tests remain. These isolated contracts
are not proof of a successful game run.

Final validation and fresh fix2 identity follow below. Developer review and a
fresh `stage4a3-reference-3` run remain required; no Stage 4B is implemented.


Final fix2 automated validation: incremental full x86 Release build (both titles
and tools) passed, followed by a final consistency build after deferring tick
breadcrumb writes. CTest passed 2/2; direct Google tests passed **113 per title**;
focused DeveloperHarness/PerformanceProfiler/PathProfiler/PathPhaseProfiler tests
passed **39 per title**, including all 14 harness tests. Six Python comparison
tests passed unchanged. The static audit confirms both new accessors are the
only GameClient header token differences and do not change layout/vtables;
accepted pathfinding/profiler algorithms and ordinary engine hooks remain
unchanged. `git diff --check` passed. Both dump hashes, launch/exit receipts,
reference-1 marker and empty reference-2/trial-1 inventory remain unchanged.

A separate exact-symbol-quality check explicitly reports loaded PDB path under
`stage4a3-performance-harness-fix1/game`, matching GUID/age 27, SymType=PDB,
PdbUnmatched=0, LineNumbers=1 and TypeInfo=1. This confirms symbol trust after
the build directory acquired newer binaries; it does not use those for fix1.
No successful runtime/setup/coverage/repeatability is claimed by these tests.


Fresh guarded candidate (never launched):
`C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\dev-runtimes\zh\stage4a3-performance-harness-fix2\game`.

| Field | Value |
| --- | --- |
| Build/source | x86 Release; accepted `0ff7f9c9bf8dfe188015af36c7db8d5388756eda` plus dirty Stage 4A.3/fix2 worktree |
| PDB GUID / age | `5706bc8d-51e9-49fc-9096-a636d0fd56be` / 29 |
| EXE SHA-256 | `0FFADA9331538D84B3A01AAFBFF6B8DA4B7BEDBAE7BE0E387FE1AE5FDA1CA575` |
| PDB SHA-256 | `28FF7A03745442E758D9EFCEB935A1426F38582B52E5003CF076B96C7E052EFE` |

The exact PowerShell procedure above now targets fix2 and fresh reference-3.
It performs source/runtime/three-trial validation-only first; after developer
review the same wrapper without ValidateOnly runs three sequential guarded
trials, stopping on the first invalid result. Successful trials exit normally
and the next starts automatically. Expect the normal Intro to finish before the
map/army sequence; no manual skipping/input is required. Keep the application
active. Preserve `scenario-startup.json` alongside every generated report and
send back terminal output and observed progress if anything fails. No runtime
success is established yet. No Stage 4B, commit or push.

Post-stage checks passed for the installed source, fix2, fix1 and original
runtime: all four full inventories match 369 file paths/lengths/SHA-256 values.
The guarded fix2/reference-3 wrapper passed all three ValidateOnly trials;
reference-3 remains nonexistent, fix2 has no launch receipts, and no user-data
backup was written by these checks. Final evidence audit and diff whitespace
check pass, with an empty Git index. Changed/untracked file inventory is retained
under ignored `build/stage4a3-fix2-final-status.txt`; no generated evidence is staged.


## Fix2 real trial: completed runtime, invalid comparison evidence (2026-10-09)

Analyzed every file directly in `build/performance/stage4a3-reference-3/trial-1`:
benchmark JSON, startup checkpoint, summary and all 52 category, 620 frame,
7,275 path and 520 slow rows. No game/benchmark was rerun; reference-1/2/3
remain unchanged. The wrapper stopped correctly after post-run validation.

**The visible closure was intentional automatic shutdown, not a new crash.**
The matching receipt `launch-03f6c1aa299940a5b09c151fb041afc6.json` starts at
23:04:43.5112071 UTC; its exit receipt is code 0 at 23:06:15.0814027 UTC.
Report PID is 11712. Both final records say `complete`, with no failure reason.
Startup confirms requested/loaded Twilight Flame, slots/players ready, 96 units,
480 orders, 90 warmup ticks, final logic tick 391, intro complete/no movie,
logic ready and profiler stopped. Benchmark start/end ticks are 91/391: 300
captured ticks. The Finalizing report callback finishes the controller; the
outer handler checkpoints Complete and calls the existing setQuitting(TRUE).
All five reports plus benchmark JSON/fingerprints and startup JSON exist.
Only the two old PID3592 dumps from 22:48 UTC remain in shared user data;
the fix2 receipt's pre-launch backup already inventories these exact hashes.
They are the fix1 crash, not evidence of a fix2 crash.

### Capture validity and workload

| Measurement | Trial 1 |
| --- | ---: |
| Duration / mode | 13.4483 s / scenario_ticks |
| Logic ticks / outer frames | 300 / 620 |
| Path records / dropped / stack-clock errors / phase errors | 7,275 / 0 / 0 / 0 |
| Interpolation / requested-effective FPS cap | 1 / 120-120 |
| Phase observer / stride | enabled / 64 |
| GameLogic mean / p95 / p99 / max | 22.8668 / 71.4274 / 76.3769 / 82.7886 ms |
| Queue dispatches / allocated cells | 447 / 3,062,821 |
| Request / Internal / Hierarchical / Closest records | 310 / 307 / 522 / 223 |
| Ground records | 0 |
| Internal >10 / >30 / >40 ms | 16 / 0 / 0 |
| Worst Internal inclusive / self | 18.6461 / 17.6551 ms |
| Maximum Internal head pops | 13,120 |
| Closest >40 ms / worst inclusive-self | 93 / 73.8039-73.1765 ms |
| Observed queue depth maximum | 96 |

IDs are contiguous; parents precede children with matching frame/logic; frame
indices are valid. Sample selection equals iterations//64; all phase self times
and operation self times are nonnegative. Completed-frame tick counts sum to
300; no capacity truncation or incomplete tail frame is present. All slow rows
reference captured frames. Configured 30 TPS is unchanged; 300 ticks took
13.4483 wall seconds under load (this does not demonstrate sustained 30 TPS).

**Structural runtime completion is proven; correctness validation is not.**
CRC boundaries 632045365/1397580133, RNG CRCs 1172401302/1852883062 and producer
path fingerprint `1922340151998500896` were written, but this single capture
cannot establish repeatability or global determinism. After correcting coordinate
parsing, reconstructed digest is `14642841358572741620`, so validation still
rejects it. Do not label this a validated reference or bypass that gate.

The worst Internal (ID1567, frame75/tick132) has 13,120 pops, 15,045 inserts,
320,758 forward hops, no allocation failures/fallbacks and returns a path.
Its frame Logic/Queue/Search totals are 29.3622/28.3399/28.2883 ms.
The 16 Internal >10ms records have sampled line self 2.7889ms, neighbor self
0.6222ms and insertion 0.5324ms: **70.72% / 15.78% / 13.50%**.
These are sampled observations, not full-search estimates multiplied by 64.
There are no severe Internal records to compare to the 31 manual Stage4A.2
>40ms searches (51.19% / 15.56% / 33.25%).

Instead, 93 dispatches contain a failed Request followed by Closest; all 93
Request `null_after_work` outcomes feed this pattern. Internal outcomes are
214 returned_path and 93 null_before_work; Closest has 221 returned_closest
and two returned_path. Closest totals 6,042.9159ms, versus Internal 475.8363ms.
The 93 severe Closest samples split **3.14% line self / 12.42% neighbor self /
84.44% insertion** (3.0293/11.9684/81.3930 sampled ms). Across these searches,
maxima include 28,751 pops, 29,220 inserts, 9,435,219 forward hops, 719 info
failures, 27,079 cleaned cells and 990 block-zone queries. The Closest record's
own hierarchy-fallback counter is zero; this does not imply its preceding
request/hierarchy did not fail. ID3078 has 27,174 pops, 27,318 inserts,
9,435,219 hops and 455 info failures. It occupies 73.8039ms of frame232's
74.6327ms Logic / 73.8585ms Queue; Search category is only 0.0496ms there.
Closest work is largely in PathRequest category self, not PathSearch: the
category residual alone must not be interpreted as unexplained engine time.

Queue total 6,533.68ms is ~95.24% of Logic's 6,860.04ms. Closest alone is
~92.49% of queue time. These are separate comparisons, not overlapping sums.
Reconstruction total is 166.2526ms (max1.5941); cleanup45.4569ms (max0.6321);
zone_flags4.8403ms (max0.0320), all including nested records where applicable.
AI strategy total1.5509ms, object loops189.923ms; combat firing is absent.
DisplayDraw totals5,199.84ms across620 render frames, max16.7165ms; it does
not explain authoritative path retry stalls. Worst Logic frame63 is82.7886ms,
Queue81.0334ms and Search14.9319ms. Do not sum inclusive category totals.

**Workload gate fails.** It exercises the selected Internal route moderately,
but severe tails are Closest retry/sorted-list work. It is not an adequate
primary benchmark for the accepted Internal line/checking optimization.
The accepted Stage4A.2 selection remains unchanged. Do not redirect Stage4B to
insertion based on this different workload, or run two more unchanged trials
as though they would solve representativeness. Next infrastructure task is a
narrow scenario-v1 workload review (spawn/destination reachability, failed
request/Closest trigger, army placement), then a separately reviewed scenario
revision if warranted. No scenario, retry policy or gameplay changes here.

### Proven report schema defects and narrow repair

The first `1090.5` is `from_x` in paths.csv ID3 (CSV line4), inherited by ID4
and other records. PathSample declares four **float32 world-coordinate** fields.
The producer correctly emits fractional coordinates; semantic_hash incorrectly
calls int() on them. Coordinates now parse as finite float32 values. ONLY the
fingerprint encoder applies truncation toward zero, reproducing the existing
C++ hashWord(uint64_t) conversion for its defined nonnegative range. Coordinate
storage/interpretation is not changed to an integer counter. Invalid conversion
ranges fail closed. Counter, index, flag and ID fields remain strict decimal
integers; JSON integers cannot be floats/bools. Timings/elapsed values accept
finite fractions. Python parsing is locale-independent; comma decimals fail.

A second producer defect prevents safe reuse: paths.csv used default six-digit
precision for float coordinates while the digest converted the in-memory float.
E.g. float32 1023.99994 serializes as 1024 but hashes as1023. The old reports do
not retain enough information to reconstruct original values at such boundaries;
the mismatching digest cannot be repaired from those rounded values with
confidence. No exact offending in-memory coordinate is claimed recovered.
Report-only repair uses float max_digits10 (nine significant digits) for the
four coordinates, restores timing precision and uses the classic locale for
path CSV. No probe/search/startup/gameplay/state-machine logic changes.
The existing digest algorithm, CRC/RNG gates and comparison qualification stay.

Schema audit also found `queue_max=18446744073709551615`: the existing diagnostic
aggregator converts unknown queueBefore=-1 to uint64 before max. This field is
not a correctness/comparison gate; it is unusable as a measured depth. Direct
queue records show maximum96. This independent diagnostic defect is documented,
not silently treated as a real queue or used to weaken fingerprint validation;
its producer is unchanged in this narrowly scoped coordinate repair.
CSV/report precision also limits numerical timing comparisons to printed digits.

Eight Python tests cover 1090.5 coordinates, near-integer float32 round trips,
fractional timings, strict adjacent counters/metadata, nonfinite/locale rejection,
and lossy-coordinate fingerprint failure, plus existing config/CRC/RNG/path gates.
The C++ regression round-trips fractional/near-boundary coordinates through the
actual report writer and preserves the digest integer word. Full incremental
x86 Release build passed (both titles/tools), CTest2/2 and focused40 tests per
title passed. No claim of a successful new runtime trial is made.

Trial1 remains byte-for-byte preserved as completed-runtime/workload evidence;
it **cannot be reused unchanged as a validated comparison trial**. Fix2 remains
preserved but has the old serialization defect. A fresh report-schema candidate
is required; no startup fix3 or Stage4B is implemented. Review this report before
any launch. Do not start trial2/3 in reference-3 or overwrite any earlier output.


Fresh **reporting-only**, never-launched candidate:
`build/dev-runtimes/zh/stage4a3-report-schema-fix3/game`.
EXE SHA256 `6CF87960DDB5BA5E3056BDEAF42D640199EA0508A04A6439700B56B04F266B43`;
PDB SHA256 `514B296492B21C17886B35EFF52E614230388E7DE304FFBBEC97C7E76F9F6760`.
Both fresh and preserved fix2 runtime inventories pass all369 path/length/hash
checks. The fresh candidate's one-trial wrapper ValidateOnly passes; it creates
no reference-4 directory, backup or launch receipt. Original trial/report,
old dump and receipt hash checks pass; index is empty and git diff --check passes.
Ignored analysis helpers/logs are under build only.

Next developer commands are **validation only**, from PowerShell7 in the repo:

```powershell
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage4a3-report-schema-fix3
./scripts/performance/Invoke-ZHPerformanceScenario.ps1 -Name stage4a3-report-schema-fix3 -OutputRoot 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\performance\stage4a3-reference-4' -Trials 1 -ValidateOnly
```

Keep ValidateOnly. Do not launch the unchanged workload yet; review these schema
findings and the proposed focused scenario-workload investigation first. Any
subsequent binary/scenario revision requires a fresh candidate/output root and
its own reviewed procedure; there are no remaining valid reference-3 trials to
resume. This task did not implement a scenario revision or Stage4B.


## Workload-v1 investigation and bounded v2 correction (2026-10-09)

Read all path rows directly from automated reference-3/trial-1, all three
stage4a1-B captures and final stage4a2-B-final-2. Existing frames/categories/slow
correlations are reported in the preceding real-trial analysis. Final phase
capture excludes its incomplete final frame, retaining32,718 rows. Historical
manual CSVs do not encode map identity or exact destination rejection cell/owner;
coordinates are route clues, not proof of identical maps or a recovered live
occupancy map. This investigation does not invent either missing observation.

### Proven failure route, representative chains and limits

`Pathfinder::findPath` runs quick connectivity, clears zone passability, attempts
hierarchical routing, optionally sets all zones passable, then invokes Internal.
Internal checks goal cell existence and `checkDestination` BEFORE allocating
search info, computing effective zones or checking full movement validity.
`checkDestination` rejects off-map/impassable footprint cells and conflicting
allied goal reservations (with existing obstacle/ignore and enemy/crush rules).
AIUpdate::computePath calls Closest when the requested path is null and no old
path remains, with the normal pathCostFactor0 and m_retryPath=true. Closest
searches for a legal near-goal result and may traverse a large region; its
weighted-near-goal selection and sorted open list are unchanged.

Representative automated chain at frame1/tick91, object217:

| Record | Parent | Operation | Outcome | Inclusive ms / work |
| --- | --- | --- | --- | --- |
| 2 | 1 | Dispatch | observed | shared request owner |
| 3 | 2 | Request | null_after_work | 0.0441; hierarchy_fallbacks1 |
| 5 | 3 | Hierarchical | null_after_work | 0.0324 |
| 8 | 3 | Internal | null_before_work | 0.0009; zero pops/info attempts |
| 9 | 2 | Closest | returned_closest | 67.8499 |
| 11 | 9 | Hierarchical | returned_closest | 0.0201 |

Request coordinates are (1090.5,1030.28)->(3870.5,1100.5).
A second repeated pattern is Internal1249 under Request1244, tick122,
object310, (1286.92,1256.99)->(1145,3715): Internal0.0032ms, zero info attempts,
no zone rejection, crusher=-1 (not yet evaluated); parent hierarchy fallback1.
All93 automated null-before-work Internal records have zero info attempts,
zero zone rejection, layer1/ground and AI human0. Their parent Requests have
null_after_work; all93 Dispatches then invoke severe Closest. Thus the parent
outcome does not mean Internal exhausted a large search. Different nested
outcomes measure different intervals/work; do not add their inclusive costs.

The map was decoded offline from the preserved fix2 installed MapsZH.big using
the repository BIG/RefPack/DataChunk layouts; no game or map editor was run.
Twilight Flame has630x630 height vertices, border70 and logical4860x4690 bounds.
All93 rejected goals have clear static cliff flags in their3x3 neighborhoods.
The first failed goal's nearest static object is a tree about103 units away;
the second example's nearest tree is about191 away. Static cliff/unreachable
center explanations are unsupported for these examples. Quick connectivity
already passed for each Request, and there is no measured zone-rejection path.
The immediate source route is goal-footprint validation; crowded allied goal
reservations are the leading cause, with dynamic obstacles/occupancy possible.
No capture records the rejecting cell/owner, so attributing all93 to a specific
unit/building or claiming full physical reachability is not justified.

V1 spawned a12x8 army on a24-unit grid (~264x168 span) without legality checks,
then used an8-unit destination grid (~88x56 span), with no endpoint/connected
footprint preflight. Raw96 destinations were distinct, but they occupy too few
10-unit path cells with overlapping20/30-unit reservation footprints. Normal
snapping/adjustment changes recorded coordinates and cannot reserve96 independent
legal goals in that small footprint. The installed Redguard geometry is cylinder
radius7 (20-unit path footprint after legacy diameter rounding); BattleMaster is
box13x9, bounding radius~15.81 (30-unit path footprint). V1's24-unit spawn rows
can overlap tank path footprints, and the8-unit goals overlap heavily.
Commands at relative0,90,120,240,360 repeatedly redirect long cross-map trips:
90 and120 repeat the same leg only30ticks apart, while120/240/360 switch before
most movement resolves. Request source positions remain near the original bank.
This amplifies deferred goal contention and retry work; it is not proof that
all goals were terrain-disconnected. No human/crusher/surface policy bug was
found: severe manual and automated Internal scopes share source mask7243,
request context Request, ground layer1 and AI human0. Surfaces1/17 correspond
to the installed tank/infantry sets. Crusher=-1 in rejected automated Internal
is an unvisited-policy sentinel, not a request to disable crushing.

The map also contains four KandaharBridge objects around x2376-2510/y1865-2914;
these are central, not the proposed endpoint banks. Cliff flags, water corner
checks, dynamic objects and bridge layers are separate classifications in the
engine. Height alone cannot establish water/bridge reachability. The actual
runtime preflight below checks normal ground-layer/zone/footprint legality;
it does not infer reachability from a picture or require direct line passage.

### Difference from accepted severe Internal evidence

| Capture | Internal count | Internal >40ms | Internal self share of Internal+Closest | Internal head-pop share |
| --- | ---: | ---: | ---: | ---: |
| A.1 B1 | 2579 | 0 | 94.96% | 93.27% |
| A.1 B2 | 2668 | 72 | 93.70% | 86.18% |
| A.1 B3 | 798 | 55 | 90.86% | 82.87% |
| A.2 final complete frames | 3194 | 31 | 86.17% | 82.33% |
| Automated v1 | 307 | 0 | 5.96% | 10.87% |

Example accepted A.2 Internal6294 under Request6289, frame562/tick6873,
object748: (1095.05,1295.1)->(965,3135), returned_path,65.5024ms,39,332 pops,
964,093 info attempts,12,884 new records, no info failure,2,465,174 forward hops.
The parent has hierarchy_fallbacks1. Example Internal92 under Request87,
frame3/tick6596, object748: (814.619,1505.7)->(965,3135), null_after_work after
76.3856ms/46,311 pops/1,192,817 info attempts/420 failures. Both enter the
full goal-directed loop; valid initial goals can still encounter path/pool
limits under live obstacles and congestion. Reproducing failure is not the goal.
B2 Internal18834 returns a path after65.8649ms/23,579 pops/1,389,177 attempts;
B3 Internal7778 fails only after64.0723ms/45,151 pops/1,132,668 attempts.
Minimum severe Internal work: A.2 27,722 pops/643,559 info attempts;
B2 19,950/528,774; B3 26,295/579,871. The existing accepted target is the
Internal line/movement/neighbor checking route, not Closest insertion.

### Exact v2 scenario correction

Only opt-in scenario geometry/order control changes. Same map, seed, factions,
AI ownership/policy,48 BattleMasters+48 Redguards, interpolation120cap,
90warmup/300capture ticks, observer flags/capacities and normal shutdown remain.
Schema stays1; scenario version changes1->2, configuration/label changes and
order_period=0 declares one-shot orders. Strict configuration equality prevents
v1/v2 comparisons. No old report is rewritten.

A fixed192-pair candidate stream uses16columns x12rows,40-unit spacing:
start x480..1080/y1300..1740, destination same x/y2800..3240. Candidate order
is row-major; accepted starts AND goals must remain at least40 units apart.
Both banks are mostly flat at installed height byte159:184/192 start candidates
and192/192 goals have flat5x5 neighborhoods;187/192 starts and192/192 goals
have clear3x3 cliff footprints. The direct western line crosses a substantial
ravine near y2200-2600, so legal endpoints still require terrain detours rather
than a manufactured direct call to Internal. Manual coordinate bands helped
choose these areas, but identical historical map identity is not asserted.

For each ordinary newly created scenario unit, bounded selection uses existing
validMovementPosition, adjustDestination and bidirectional
clientSafeQuickDoesPathExist checks for its real locomotor and ground endpoints.
Normal cell snapping may move each endpoint at most10 units; larger relocation,
nonfinite positions, footprint radius>18 or separation<40 is rejected. The
shared cursor bounds the whole army to192 candidates; insufficient legal pairs
abort before capture. No full path is computed by the planner, no pool/queue
bypass occurs, and no engine validation API is changed. These normal fast checks
are conservative preflight, not a mathematical guarantee under future movement.

The army settles for90ticks with no harness movement orders. At capture start,
prepared goals are rechecked against current legality/connectivity; a changed
or unreachable goal fails clearly rather than forcing a distant Closest result.
Exactly one CMD_FROM_SCRIPT aiMoveToPosition per unit is then issued in creation order.
No periodic redirection occurs. Normal AI request queue -> computePath -> findPath
-> Internal and normal fallback/retry behavior remain fully enabled. The300tick
window measures searches plus real movement; completing the1500-unit journey is
not required. Natural AI behavior remains, and live goal changes can still fail.
This is a source-supported candidate, not a claim that runtime qualification
already passes. No pathfinder optimization or additional profiler was added.

### Separate evidence-derived representativeness gate

Strict load/--validate still requires complete metadata/ticks, all reports,
zero errors/drops, phase stride64, valid parents/frames/self accounting and exact
reconstructed path fingerprint. Fractional float32 parsing, nine-digit coordinate
serialization, strict integer counters and CRC/RNG equality are retained.
After that, --qualify requires:

| Gate | Rationale |
| --- | --- |
| At least96 Internal records | One normal army's worth; scope/activity, not an exact search-count fingerprint |
| At least3 large Internal records, each >=20,000 head pops and >=500,000 info attempts | Existing minimum-repeat count3 retained; conservative rounded work below the accepted severe cohorts (B2's19,950 minimum is a near-boundary exception, not a threshold claim about every search) |
| At least3 such large Internal records with both line and neighbor sampled calls | Ensures repeated work reaches the actual checking phases, not only large administrative counters |
| Internal >=75% of Internal+Closest inclusive head pops | Conservative margin below observed82.33/86.18/82.87%; v1 is10.87%. Avoid a workload whose node work is mostly Closest |

Head-pop counts are summed only across non-overlapping Internal/Closest
operations; parent Queue/Dispatch/Request and their inclusive work are excluded.
Self-time shares and >10/>30/>40ms counts remain diagnostics. **No duration,
exact performance fingerprint or minimum self-time threshold qualifies a run.**
A legitimate future speedup or OS preemption must not fail a structural workload
gate solely by changing wall time. Configuration/CRC/RNG/path fingerprints remain
strict separate correctness/comparison gates. Matching fingerprints do not prove
exact full path nodes, replay, save or multiplayer lockstep globally.

Applied directly as workload-only evidence: final A.2 complete frames passes,
with37 large sampled Internal searches and82.33% pop share. V1 fails with zero
large qualifying Internal records and10.87% share, despite307 total Internal.
Its strict load still independently fails the old rounded-coordinate fingerprint;
this analytical classification does not relabel it valid. The wrapper runs
--validate then --qualify; either failure stops subsequent trials. Comparisons
require qualified references. Thresholds are initial workload-class guards,
not a statistical speedup claim or proof that v2 is representative before launch.

### Focused validation and preserved evidence

New C++ tests verify deterministic blocked/disconnected-candidate filtering,
bounded exhaustion/relocation rejection, nonshared spaced destinations and one
order event exactly at capture start. Synthetic terrain acceptance tests the
selection contract; actual installed-map legality is additionally checked at
runtime, not claimed proven by a mock. Existing startup/state/default-off and
fractional near-integer report round-trip tests remain.
Ten Python tests pass: representative Internal-heavy acceptance, old307/223
Closest-dominated rejection with measured aggregate pop/time shares, missing
large work/sample rejection, timing-independent qualification, separate CLI
correctness0/qualification2 exits, fractional coordinates/strict counters,
lossy-coordinate fingerprint rejection and existing config/CRC/RNG gates.
Full incremental x86 Release build (both games/tools) and CTest2/2 pass;
focused suites pass42/title including16 harness tests. Static gated-hook audit
and authoritative AIPathfind.cpp identity check pass; accepted A.2 capture
hashes and empty index are preserved. Only the opt-in developer adapter/controller is changed; no normal-game/pathfinder
algorithm, retry/pool/queue/RNG/FP/30TPS/serialized-state policy changes are made.


Prepared guarded runtime (no launch):
`C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\dev-runtimes\zh\stage4a3-pathfinding-workload-fix4\game`.

| Identity | SHA256 |
| --- | --- |
| generalszh.exe | `423928A40B7CAF0B5FC21116AF151D605AEE277D48BFCDCAE7FBE3C38E39AF51` |
| generalszh.pdb | `0ABD6EC567E3F3482D42AD7CC74BE4DF77A738B824B1126B20301D80AAC9E0B6` |

Full runtime inventory verification passes369 paths/lengths/hashes; one-trial
ValidateOnly passes. Final checks verify old fix2/fix3 EXE/PDB identities,
reference-1 marker, empty reference-2/trial-1, every reference-3/trial-1 file,
old crash/receipt hashes and accepted A.2 capture hashes unchanged. Reference-4
still does not exist, fix4 has no launch receipts and Git index is empty.
Generated map-analysis helpers, logs and identity snapshots stay ignored under
build; no captures, runtimes or generated analysis are staged. Git status is
recorded in ignored `build/stage4a3-fix4-final-status.txt`.
