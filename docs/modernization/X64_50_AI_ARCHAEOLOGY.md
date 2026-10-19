# X64.50 - Generals / Zero Hour AI archaeology and bug audit

Status: **AUDIT / RESEARCH. No AI behaviour, balance, camera, frame pacing, HUD or packaging change was made.** The only code added is read-only audit tooling
(`scripts/ai-audit/`) and a source-text contract (`scripts/native-runtime/test_ai_archaeology_contract.py`). No player ZIP was rebuilt (no runtime code changed).
Start point: HEAD `f07cfceda` = `origin/dev/modern-engine` (no other agent had pushed anything: the repository equals the handoff).

Read this document with `AGENT_HANDOFF.md` (section X64.50) and `X64_43_FAILING_PATH_INVESTIGATION.md` (the measured path-failure side of section 7).

How facts below were established: **source of truth is this repository** (both trees read line by line), plus the retail *data* of the user's Steam installations read **read-only**
(`AIData.ini` from the BIG archives, `Data/Scripts/SkirmishScripts.scb`, decoded with `scripts/ai-audit/scb.py` from the DataChunk code of this tree). External projects were used
only as *research references*; each of their claims was re-proved or disproved against our source (section 6). Retail scripts / team data are EA content and are **not** committed: only
aggregates are (`evidence/x64-50/ai-statistics.json`).

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 0. Executive summary (the answers)

1. **Why retail bases look predetermined.** Three stacked layers, none of which uses the random number generator: (a) the *positions*: every side has one fixed `SkirmishBuildList`
   in `AIData.ini` - 11 - 14 (Generals) / 14 - 26 (Zero Hour) structure slots with hard-wired coordinates relative to the Command Center; (b) the *order and timing*: the skirmish
   scripts (`SkirmishScripts.scb`, ~320 scripts per side in Zero Hour) are a deterministic state machine - "1st power plant, then supply center, then barracks, then war factory, defense
   after escalation counter 10..." - each step *marks the next not-yet-built slot of that template as priority* (`SKIRMISH_BUILD_BUILDING`); (c) the engine's "AutomaticallyBuild" path,
   which would let the AI pick structures itself, is **dead code** (AI-01) and the retail data has `AutomaticallyBuild = No` for every slot except the Command Center anyway.
   Fallback placement (when a slot is blocked) is a deterministic square-ring search, so even failures repeat identically. The only randomness in the whole AI is team selection
   among equal priority, the science skill-set pick, the superweapon grid start corner, a few random script timers and AI scan stagger.
2. **Does more dynamic EA base-building code exist?** **Yes.** The engine already contains: an automatic build-from-list path (unreachable because of AI-01), supply-center *expansion*
   near supply sources (`buildBySupplies` / `AI_PLAYER_BUILD_SUPPLY_CENTER`, used 164x in Zero Hour scripts), construction near the army (`buildSpecificBuildingNearestTeam`,
   Zero Hour only), geometry-driven base defences on an arc facing the enemy approach path (`buildAIBaseDefenseStructure`, front/flank/backdoor), optional rotation of the whole layout
   by map position (`RotateSkirmishBases`, **on in Generals, off in Zero Hour**), power-driven additional power plants and adaptive team priorities. Retail data/scripts use them
   sparingly; AI-01 (and AI-11) bugs and data choices (`AutomaticallyBuild = No`, one defence type per side, `StructureSeconds = 0`) hide the rest.
3. **Bugs.** 20 findings in section 3 (3 proven + 7 likely original bugs in our tree that are in AI code, 1 Zero Hour-only regression of a correct Generals loop, plus design limits).
   The important ones: AI-01 (wrong template passed to `canMakeUnit`: automatic builds never selected; explains "AI won't rebuild its Command Center", incl. China in Zero Hour which is
   configured for *unlimited automatic* Command Center rebuilds), AI-02 (GLA rebuild-hole bookkeeping bug that interacts with AI-01), AI-04 (Zero Hour superweapon fine-search typo
   `(x-5)` for y: only a diagonal is searched; Generals is correct), AI-11 (front defence placement stalls after the left arc is exhausted).
4. **Brutal / Hard "cheats".** In Skirmish there are **no** health or weapon bonuses (those apply only to `GAME_SINGLE_PLAYER` campaigns). The Hard (= Brutal) AI instead receives
   **scripted money**: Zero Hour `PLAYER_GIVE_MONEY` appears 155 times; nominal Hard-only totals are ~71 000 - 99 000 per side (+10 000 / +10 000 / +15 000 / +20 000 on the
   `_ESCALATION` counter, plus supply-drop / uplink / expansion gifts); Normal gets ~3 500 - 4 000 in expansion gifts; *every* difficulty gets 200 - 2 350 "to cover bug".
   Knowledge advantages (not removable by data): hunting teams scan the **whole map** for targets without fog/LOS (`findClosestEnemy(9999.9)`), idle AI units do not apply the
   `UNFOGGED` filter humans do, `SKIRMISH_VALUE_IN_AREA` / superweapon targeting / supply-expansion read true enemy state, AI paths may use the shrouded border region.
5. **Original vs Enhanced.** Original must stay byte-identical (the project's `RETAIL_COMPATIBLE_CRC = 1` contract). The recommended seam is a small runtime `AI behaviour mode`
   (synchronised game option, recorded in replays) consulted at the few decision points listed in section 8; shipping Enhanced as *data* (an alternative script / build-list pack) is
   possible for strategy, but engine fixes (AI-01/02/04/...) must be mode-gated code.
6. **Recommended first step after review:** Phase 0 (mode plumbing + Original golden-replay gate, no behaviour change), then Phase 1 (AI-01 + AI-02 *together*, AI-04, AI-05, AI-08, AI-11).

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 1. Architecture map (control flow, not just classes)

### 1.1 Where the AI lives

```
GameLogic::update()                                    [30 TPS, deterministic lockstep; every client runs every AI]
  |-- ScriptEngine::update()                           evaluates EVERY active script of EVERY player each tick
  |     |  per-player script lists: Data/Scripts/SkirmishScripts.scb  (one list per skirmish side + civilian)
  |     |  conditions: counters / flags / timers / PLAYER_HAS_* / SKIRMISH_* / BUILT_BY_PLAYER ...      (scripts are cheap state machines)
  |     `  actions that call the AI object:  SKIRMISH_BUILD_BUILDING -> AISkirmishPlayer::buildSpecificAIBuilding   (marks a build-list slot "priority")
  |                                           AI_PLAYER_BUILD_SUPPLY_CENTER -> AIPlayer::buildBySupplies             (dynamic placement near supplies)
  |                                           AI_PLAYER_BUILD_TYPE_NEAREST_TEAM -> buildSpecificBuildingNearestTeam   (ZH; dynamic placement near an army)
  |                                           SKIRMISH_BUILD_BASE_DEFENSE_FRONT/FLANK, SKIRMISH_BUILD_STRUCTURE_FRONT/FLANK -> buildAIBaseDefense[Structure]
  |                                           BUILD_TEAM / RECRUIT_TEAM -> buildSpecificAITeam / recruitSpecificAITeam
  |                                           AI_PLAYER_BUILD_UPGRADE, TEAM_* orders (guard / hunt / attack area / follow approach path), PLAYER_GIVE_MONEY, ...
  |-- Player::update() -> AIPlayer::update()  (AISkirmishPlayer overrides the building / team timers)
  |     |-- doBaseBuilding()      timer StructureSeconds (retail data: 0), poll every 2 s  -> processBaseBuilding()      [section 2]
  |     |-- checkReadyTeams() / checkQueuedTeams()   teams finished producing -> "active" (OnCreate script runs) or disbanded after timeout
  |     |-- doTeamBuilding()      TeamSeconds (data 10 s, **hard capped to 3 s** in skirmish, AI-09), poll every 2 s -> selectTeamToBuild() -> queueUnits()
  |     |-- doUpgradesAndSkills() spend science points from AIData SideInfo SkillSet1..5 (random skill set per game)
  |     `-- updateBridgeRepair()
  `-- Object updates: AIUpdateInterface state machines (AIStates.cpp) carry out the orders; Pathfinder serves every move
```

Skirmish AI = `AISkirmishPlayer` (derives `AIPlayer`; the solo/campaign AI is the base class). `ForceSkirmishAI = No` in data (a developer switch).

### 1.2 The loops in detail

* **Base building** - `AIPlayer::doBaseBuilding` / `AISkirmishPlayer::doBaseBuilding` count down `m_structureTimer`; `processBaseBuilding` walks the player's `BuildListInfo` list (copied from
  `AIData.ini` `SkirmishBuildList <side>` at `newMap`, positions transformed by `adjustBuildList`). Per entry: hole / destroyed / captured bookkeeping, dozer supervision (re-assign a dead dozer),
  30 s rebuild delay (`RebuildDelayTimeSeconds`), `isLocationSafe` (no enemy within `SupplyCenterSafeRadius` 300), **priority** entries first, **power plant** forcing when under-powered,
  then (non-priority) the automatic branch. The chosen plan is built with `buildStructureWithDozer`: dozer lookup, money check, legality check, deterministic square-ring "wiggle" search
  (up to 120 cells in skirmish) when the slot is blocked, teleport the dozer if no path, `buildObjectNow`. One structure per pass; `m_structureTimer` (0 s) re-arms.
* **Team building** - `selectTeamToBuild`: every team prototype whose production-condition *script* is true, which is not capped (`teamMaxInstances`) / already queued / affordable
  (`isPossibleToBuildTeam`: factories exist, `TeamResourcesToStart` 10 % of cost), the highest `teamProductionPriority` set wins; **random pick among the top-priority set**
  (`GameLogicRandomValue`); auto-reinforcement of existing teams is considered first. `queueUnits` fills each work order: *recruit* idle units from other teams within `MaxRecruitRadius`
  (500) else `startTraining` at an idle `findFactory`. `queueSupplyTruck` keeps harvesters at `ResourceGatherers` (+1 freebie) per supply building.
* **Team behaviour** is script data: a finished team becomes active and its `OnCreate` / generic-hook scripts issue `TEAM_GUARD_AREA` (671 in ZH scripts), `TEAM_HUNT`, `TEAM_ATTACK_AREA`,
  `SKIRMISH_MOVE_TO_APPROACH_PATH` + `SKIRMISH_FOLLOW_APPROACH_PATH` (Center / Flank / Backdoor), command-button abilities (Hunt with a command button, garrison, load transports),
  with attack *waves* gated by the `_ESCALATION` counter and a global `_LAUNCH_ATTACK` flag, and per-team **attack-priority sets** (`SET_ATTACK_PRIORITY_THING`, 1008 uses in ZH).
* **Approach paths**: `Center<N>`, `Flank<N>`, `Backdoor<N>` are *map waypoint paths* labelled with the **enemy's** start index N; the closest waypoint to the team centroid is taken. If the map
  lacks the path (or no enemy exists) the order silently does nothing (`ScriptActions.cpp doTeamFollowSkirmishApproachPath`).
* **Enemy choice**: `acquireEnemy` picks the *single* closest enemy by structure-bounds centre (penalty +500^2 if another AI already targets him, -25^2 "gentle retaliation"), re-evaluated
  every 5 s only when the current enemy is "in bad shape" (upstream issue #514: AI recognises only the closest enemy).
* **Superweapons**: `computeSuperweaponTarget` = coarse grid (<=10x10 over the enemy structure bounds, random corner start) then a fine 11x11 refinement, value = enemy object costs weighted by distance;
  sneak-attack mode punishes defended areas; Command Center / superweapons de-prioritised. Fired by `SKIRMISH_FIRE_SPECIAL_POWER_AT_MOST_COST` scripts.
* **Unit level**: `AIHuntState` rescans each `ENEMY_SCAN_RATE` (30 frames) with `findClosestEnemy(owner, 9999.9, CAN_ATTACK, attackPriorityInfo)`; guard states (`AIGuard`, `AITNGuard`,
  `AIGuardRetaliate` in ZH) scan at `GuardEnemyScanRate` (500 ms) with AI radius modifiers 1.1 / 1.333 (humans 1.8 / 2.2). All of it feeds `Pathfinder::findPath` through the pathfind queue.

### 1.3 Which behaviour comes from where

| Behaviour | A engine C++ | B INI | C map data | D scripts | E hard-coded assumption | F difficulty |
|---|---|---|---|---|---|---|
| Structure **positions** (slots) | placement search, legality, wiggle ring | `SkirmishBuildList` per side (coordinates, angle, rebuilds, automatic flag) | start position (command centre object) anchors the layout | - | layout always rotated by 3pi/4 (+grid rotation if `RotateSkirmishBases`); building angles never rotated (AI-03) | - |
| Structure **order / timing** | priority marking, power forcing, dozer supervision | `StructureSeconds` 0, `RebuildDelayTimeSeconds` 30 | - | the base-building script groups (per difficulty E/N/H) with escalation / credit conditions | 1 structure per pass, 2 s polling, `isLocationSafe` radius | script variants per difficulty |
| Supply expansion / forward bases | `buildBySupplies`, `findSupplyCenter` (60/40 enemy-proximity rule), `buildSpecificBuildingNearestTeam` | `ResourceGatherers`, `SupplyCenterSafeRadius` | supply sources on the map | `AI_PLAYER_BUILD_SUPPLY_CENTER` (164) with a minimum-cash threshold | - | gatherer counts (identical per difficulty in ZH) |
| Base defences | `buildAIBaseDefenseStructure` arc placement | `BaseDefenseStructure1` (one type per side), `SkirmishBaseDefenseExtraDistance` 150 (ZH) | waypoint paths `Center/Flank/Backdoor<N>` | `SKIRMISH_BUILD_BASE_DEFENSE_*` | +-60 degree arc (AI-11), 4x radius spacing | count of defences by difficulty script |
| Unit production | `selectTeamToBuild`, work orders, recruit, `findFactory` | `TeamSeconds`, poor/wealthy rates, `TeamResourcesToStart`, `MaxRecruitRadius` | - | team templates (1340 ZH / 368 Gen): composition, priority, conditions, max instances | 3 s cap (AI-09), random among equals | production conditions per difficulty |
| Attack waves / routes | hunt / guard / attack-area states, group moves | group-move thresholds (`MinInfantryForGroup` 3 ...) | approach waypoint paths, `[Skirmish]` perimeter / `CombatZone` trigger areas | wave flags, `_ESCALATION`, team scripts, attack-priority sets | closest-target-first, single enemy | wave count and timing per difficulty |
| Special powers / superweapons | grid search + value function | skill sets (`SideInfo`) | - | `SKIRMISH_FIRE_SPECIAL_POWER_AT_MOST_COST` | random corner, cost based | - |
| Money advantages | `PLAYER_GIVE_MONEY` execution | - | - | the "AI Bonus ..." / "cover bug" scripts | - | Hard >> Normal > Easy |
| Knowledge advantages | hunt / idle acquisition, superweapon value, supply expansion, pathing border | - | - | `SKIRMISH_VALUE_IN_AREA`, `PLAYER_HAS_*` read true state | no shroud filtering | - |

### 1.4 Generals vs Zero Hour

| | Generals | Zero Hour |
|---|---|---|
| AI sides / script lists | America, China, GLA (+civilian): 834 scripts | 12 generals + civilian: 3 957 scripts (323 per side) |
| Team templates | 368 (mean 5.9 units by max counts, 346 singleton) | 1 340 (mean 3.2 units, **1 221 singleton**; 596 attack-common-target; 469 recruitable) |
| `SkirmishBuildList` slots | 11 - 14 | 14 - 26 |
| `RotateSkirmishBases` | **Yes** (layout rotated by map-position grid) | **No** (fixed 135 degree rotation only) |
| `ResourceGatherers` Easy/Normal/Hard | America 1/1/1, China 1/2/2, GLA 1/3/6 | America/China 2/2/2, GLA 5/5/5 (no difficulty difference) |
| Command Center slot | `Rebuilds = 3`, `AutomaticallyBuild = Yes` (comment: "So it will automatically rebuild") | America/GLA/generals `Rebuilds = 0`; **China `Rebuilds = -1` (unlimited)** |
| Engine additions | - | `buildSpecificBuildingNearestTeam`, `calcClosestConstructionZoneLocation`, `SkirmishBaseDefenseExtraDistance`, `AIGuardRetaliate`, captured-dozer handling, superweapon `conservative` bounds / `includeMilitaryUnits`, `Bool computeSuperweaponTarget` |
| Superweapon fine search | correct `(i-5)/(j-5)` | **typo `(x-5)` for y (AI-04)** |

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 2. Base-building archaeology

### 2.1 The mechanism, step by step

1. `AISkirmishPlayer::newMap()` copies the side's `SkirmishBuildList` (`AISideBuildList`, first matching side name wins) and calls `adjustBuildList`, which finds the start Command Center,
   **destroys it**, and re-creates it as the list's own Command Center slot (marked `InitiallyBuilt`); all other slots are translated/rotated around it (rotation 3pi/4 + map-grid angle when
   `RotateSkirmishBases`). Slots that are not initially built get `Rebuilds + 1` ("the initial build consumes a rebuild").
2. Retail data for **every** slot except the Command Center: `AutomaticallyBuild = No`, `Rebuilds = 0` (so 1 after step 1). `BuildListInfo::m_automaticallyBuild` is documented as "If true, the ai will
   build. If false, **script has to enable this**."
3. A script runs `SKIRMISH_BUILD_BUILDING(Template)`; `AISkirmishPlayer::buildSpecificAIBuilding` finds the **first not-yet-built, not-yet-priority slot of that template in list order** and
   calls `markPriorityBuild()` (`m_priorityBuild` is **never cleared** by any code). The next `processBaseBuilding` pass selects priority entries first (`bldgPlan = curPlan`), checks safety,
   dozer availability and **builds exactly at the slot coordinates**; a blocked slot is resolved by a deterministic ring search, a lost building is rebuilt (after 30 s) *forever* because the priority flag stays.
4. The scripts decide *when*: e.g. (Zero Hour America, Normal) 1st power plant at once -> supply center after the power plant (dynamic, near supplies) -> barracks -> war factory after the supply center ->
   first defence at `_ESCALATION >= 10` (+credits) -> airfield -> strategy center at escalation 30 -> second power plant 35 or 200 -> supply drop zones after the strategy center -> uplink at 100 ...
   `_ESCALATION` is a game-time counter (+1 per 10 s Normal/Hard, per 20 s Easy; +30/+50 on enemy tech centre / superweapon): the same timeline every game.
5. Power is the only dynamic: a power-plant slot is chosen automatically when the player is under-powered (`isUnderPowered || isAutomaticBuild`), independent of scripts (and not subject to `isBuildable`).

So: **positions = INI, order = scripts, trigger times = counters, tie-breaks = list order, failures = deterministic search**. Nothing consumes randomness. (Quantified: 12 - 37 `SKIRMISH_BUILD_BUILDING`
actions per side in ZH scripts, one per structure step and difficulty variant.)

### 2.2 The suspected `canMakeUnit` / wrong-template issue - **PROVEN** (AI-01)

`AISkirmishPlayer::processBaseBuilding` (ZH `AISkirmishPlayer.cpp:226`, Generals `:218`):

```cpp
if (TheBuildAssistant->canMakeUnit(dozer, bldgPlan)!=CANMAKE_OK) { ... continue; }   // bldgPlan = the plan accumulated so far, not curPlan
...
if (info->isBuildable()) { if (bldgPlan == nullptr) { bldgPlan = curPlan; bldgInfo = info; } }
```

* `bldgPlan` is `nullptr` at loop start and is assigned only by the priority branch (line 203) or the post-loop power override; `BuildAssistant::canMakeUnit` returns `CANMAKE_NO_PREREQ`
  for a null template (`BuildAssistant.cpp:1312`). Therefore the "pick the first buildable automatic entry" assignment (line 238) is **unreachable**: for every non-priority entry
  `canMakeUnit(dozer, nullptr)` fails and the loop `continue`s. When a priority plan exists the check validates *that* plan for every other entry (harmless but meaningless), and its failure message
  ("money or technology missing") blames the wrong cause.
* Proven by source (both titles) and pinned by `test_ai_archaeology_contract.py::test_ai01_*`. Not yet reproduced dynamically (section 9, experiment E1).
* Consequences: `AutomaticallyBuild = Yes` is inert. The only `Yes` slot in retail data is the Command Center: Generals (`Rebuilds = 3`, comment "So it will automatically rebuild") and **Zero Hour China
  (`Rebuilds = -1` = unlimited)** can never be rebuilt automatically - this matches upstream issue #2679 "AI won't recreate command center if it is destroyed" (label ZH). For ZH America / GLA / generals the
  data says `Rebuilds = 0`, i.e. the designers do not rebuild it - possibly *because* it never worked. Modded lists using `AutomaticallyBuild` (the default is **true** when the key is omitted) are dead too.
* Upstream (research reference): issue **#2407** describes the same defect plus the GLA hole problem (AI-02) and advises fixing them together; PRs #2837 / #2936 were closed unmerged.

### 2.3 GLA rebuild holes - **LIKELY BUG** (AI-02, interacts with AI-01)

When a GLA structure dies it becomes a `KINDOF_REBUILD_HOLE` that later re-spawns the building. In `processBaseBuilding` the loop declares an outer `bldg` (assigned *before* the hole logic,
line 119) and a **shadowing inner `bldg`** inside the "used to have a building" branch (line 123). After the scan links the slot to the hole object (`info->setObjectID(hole)`, no `break`, outer
`bldg` not refreshed) the outer pointer is still null, so the entry passes `if (bldg) continue;` and the 30 s rebuild-delay guard (the object id is no longer INVALID). For a priority-marked slot
the AI then proceeds to build **immediately**: the slot is occupied by the hole, `buildStructureWithDozer` takes the "unable to place - adjust position" ring search and builds a *second* structure
beside the hole (up to 120 cells away in skirmish), while `RebuildHoleBehavior` later re-creates the original. Expected player-visible effect: duplicated / displaced GLA buildings and loss of the
rebuild delay. Confidence: high on the control flow (read twice, contract test), medium on the in-game severity (not yet observed natively, experiment E2). Fixing AI-01 alone would expose it
on the automatic path too (upstream agrees).

### 2.3.1 Smaller base-building defects found on the way

AI-03 `adjustBuildList` second loop reads `list->getTemplateName()` (the *head*), not `cur`, and re-tests `KINDOF_COMMANDCENTER` inside the transform: it works only because retail lists start with the Command
Center; building angles are never rotated (`cur->setAngle(cur->getAngle())`) although positions are; `buildPos` is uninitialised if a list has no Command Center. AI-05 `gotOffset;` is an expression statement with no
effect (rally offsets of build-list entries are never applied; 4 sites). AI-11 / AI-14 below.

### 2.4 Could the original engine already support more dynamic bases? What exists, what hides it

| Capability in the engine | Status | Why you do not see it |
|---|---|---|
| Automatic build from the list (`AutomaticallyBuild`) | **broken** (AI-01) + data `No` | dead code |
| Rebuild counts, unlimited rebuilds (`Rebuilds = -1`) | works for *priority* slots (flag never cleared) | Command Center (the only unlimited / automatic one in ZH China) is blocked by AI-01 |
| Supply expansion near warehouses (`findSupplyCenter` + `buildBySupplies`) | works | scripts gate it by cash thresholds / `SUPPLY_SOURCE_SAFE`; 1 template per call |
| Forward building near the army (`buildSpecificBuildingNearestTeam`) | works (ZH only) | 4 uses in GLA scripts, 19 overall |
| Base-defence arc towards enemy approach (`buildAIBaseDefenseStructure`) | works until the left arc is exhausted (AI-11) | one defence type per side, +-60 degree sector, needs the map's approach waypoint paths |
| Layout rotation by map position (`RotateSkirmishBases`) | works for positions only (AI-03) | **Zero Hour data sets `No`** (Generals `Yes`) |
| Power-driven plant placement | works | list order |
| Structure pacing modifiers (`StructureSeconds`, poor/wealthy rates) | inert | `StructureSeconds = 0`; the skirmish override caps timers at 3 s |
| Team priority adaptation (`SuccessIncrease` / `FailureDecrease`) | works | 88 / 439 (ZH) teams use it |
| Rally offsets | **broken** (AI-05) | no-op statement |
| Per-difficulty gatherer counts | works | Zero Hour data uses identical values for all difficulties |

Answer: the engine can already do clearly more than retail shows - dynamic expansion, forward bases, geometry-aware defences, layout rotation, automatic rebuilds - but the *base itself* is, by design and by AI-01, a scripted playback of a fixed
template.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 3. Bug inventory

Classes: A proven original bug, B likely original bug, C dormant / broken feature, D data / script limitation, E map-specific, F design limitation, G performance, H balance (not a preservation fix), I new Enhanced idea,
J determinism / multiplayer risk. "Classic impact" = effect on Original mode if changed. Title: G Generals, Z Zero Hour. **No finding was fixed.** Every fix changes simulation => J (needs the mode seam of section 8).

| ID | Class | Title | Location | Finding (current behaviour) | Evidence | Sev. | Conf. | Classic impact | Enhanced relevance | Action |
|---|---|---|---|---|---|---|---|---|---|---|
| AI-01 | A (C) | G Z | `AISkirmishPlayer.cpp` Z:226 G:218 | `canMakeUnit(dozer, bldgPlan)` with the accumulated (usually null) plan: automatic-build selection unreachable; wrong "money/tech" message | `BuildAssistant.cpp:1312` null => NO_PREREQ; struct comment; upstream #2407; data; contract test | minor-medium | high | changing it makes Generals rebuild its Command Center (Rebuilds 3) and ZH China unlimitedly: Original behaviour change | high (enables automatic bases) | Enhanced only; fix together with AI-02 |
| AI-02 | B | G Z | `AISkirmishPlayer.cpp:119-182` | shadowed `bldg` + hole scan without break: GLA slot linked to the hole is treated as unbuilt, rebuild delay bypassed, duplicate buildings | code flow; upstream #2407 (Bug 2) | minor-medium | high (flow) / medium (impact) | alters GLA rebuild timing/duplication | high | Enhanced only (E2 first) |
| AI-03 | B | G Z | `adjustBuildList` Z:1048 | head template used instead of `cur`; building angles not rotated; uninitialised `buildPos` without CC | code | minor | high | none in retail data | medium (needed for layout variety) | Enhanced; or leave |
| AI-04 | A | **Z only** | `AIPlayer.cpp:1292` | `pos.y = bestPos.y + (x-5)*...`: 11x11 refinement degenerates to 11 diagonal points; Generals `:1220` uses `(j-5)` | line comparison across titles | minor | high | changes ZH superweapon aim (gameplay) | medium | Enhanced only |
| AI-05 | A | G Z | `AIPlayer.cpp` Z:500,680 G:493,673 | `gotOffset;` no-op: build-list `RallyPointOffset` never applied | code | trivial | high | none in skirmish data (offsets 0) | low | fix with Phase 1 |
| AI-06 | B | G Z | `getPlayerStructureBounds` Z:3892 | "no structures -> use unit bounds" is dead (`objBounds` only holds structures) => zero rectangle (0,0) for structure-less players | code | minor | high | affects target choice for structure-less enemies (superweapon degenerate path covers it) | medium | Enhanced |
| AI-07 | B | G Z | `getMyEnemyPlayerIndex` | returns `playerCount` (out of range) when there is no human (AI-only / observer games); callers guard some paths | code | minor | medium | none | low | shared hardening |
| AI-08 | B | G Z | `acquireEnemy` Z:521 | retaliation bonus tests `somePlayer->getCurrentEnemy()==m_player` (any other AI) instead of the candidate | code; upstream #2413 | minor | high | target choice | medium | Enhanced |
| AI-09 | B/F/H | G Z | `doBaseBuilding` Z:879, `doTeamBuilding` Z:932 | `AISkirmishPlayer` caps `m_structureTimer` / `m_teamTimer` at 3 s: `TeamSeconds = 10` and the wealthy/poor modifiers are effectively ignored; poll comment says 5 s, code 2 s | code; upstream #2409 | n/a (it IS the retail pace) | high | **removing it triples AI team cadence: balance change** | high (pacing is a difficulty lever) | **leave in Original**; Enhanced decides |
| AI-10 | B | G Z | `AISkirmishPlayer::xfer` v1 | `m_currentEnemy`, `m_frameToCheckEnemy` not saved: AI re-acquires its enemy after load | code; upstream #2420 | minor | high | save-format change (`RETAIL_COMPATIBLE_XFER_SAVE`) | low | gate by xfer version |
| AI-11 | B | G Z | `buildAIBaseDefenseStructure` Z:675 | `angle > PI/3` bounds only the left/positive arc and `break`s *before* the counter advances: when the left arc is full the selector never alternates and later defence calls build nothing; the right arc is unbounded | code; upstream #2398 | minor | high | caps retail AI defence count | medium | Enhanced |
| AI-12 | A | G Z | `AIGroup.cpp` `groupMoveToPosition` | `Int dy = (max.x-min.x)/CELL` (x for y): tighten-area size wrong. **Player command path only** (`CMD_FROM_PLAYER`), not the AI | code; upstream #2462 (#2948 closed) | minor | high | changes human group-move tighten decisions (sim) | none | leave (not AI) |
| AI-13 | A | G Z | `isSupplySourceAttacked` | scan window 10 frames instead of 10 s (upstream fixed, gated by `!RETAIL_COMPATIBLE_CRC`) - **already in our tree**, inactive in the CRC-compatible build | upstream #3380 / #2386, source comment | minor | high | none while the macro is 1 | medium | take via the mode seam |
| AI-14 | B | G Z | `queueSupplyTruck` | loose-harvester branch lacks the `KINDOF_REBUILD_HOLE` check the sibling branch has: harvesters may be sent to dock at a hole | code; upstream #2388 | minor | high | GLA supply behaviour | medium | Enhanced |
| AI-15 | A | G Z | `AIPlayer::onUnitProduced` | uninitialised `supplyTruck` (upstream fixed; retail-compatible init preserved for VC6) | upstream comment | minor | high | already handled | - | none |
| AI-16 | A | G Z | `AIStates.cpp:887` `AIStateMachine::update` | temporary-state `onExit` after `update()` without null check (`clear()` inside own update): crash only with `!RETAIL_COMPATIBLE_CRC`; upstream fix #3438 merged 2026-10-08, **after our baseline** | code vs upstream PR | major (crash) when reachable | high | latent (macro = 1) | needed before any Enhanced behaviour change | take upstream when Enhanced work starts |
| AI-17 | F | G | `AISkirmishPlayer::computeSuperweaponTarget` | `void` (no failure signal), no military-unit / conservative-bounds logic: Zero Hour improved it | diff | - | high | none | low | none |
| AI-18 | B | G Z | `guardSupplyCenter` (Z:918), `buildBySupplies` (Z:1916) | `getSkirmishEnemyPlayer()->getPlayerIndex()` without null check (enemy gone / none) | code | minor (crash only without enemy) | medium | none | low | shared hardening |
| AI-19 | G | G Z | `Player::addToPriorityBuildList` | every dynamic build appends a `BuildListInfo` that is never removed; `processBaseBuilding` walks the whole list each pass | code | trivial | high | none | low | Enhanced cleanup |
| AI-20 | F | G Z | `AIPlayer::selectTeamToBuild` | closest-target / single-enemy design (`#514`), one defence type per side, team sizes mean 3.2 (ZH) | data | - | high | none | high (design space) | Phase 3 / 4 |

Not bugs, recorded to avoid re-litigation: the 3 s cap (AI-09) is the retail pace; money gifts (section 5) are intended design; `Rebuilds = 0` slots are rebuilt forever once a script marked them priority (flag never cleared) - that is how retail AI rebuilds
its base, even though it looks like "rebuild count 0".

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 4. Dormant, unused or broken EA functionality

| Id | Feature | State | Needed to activate | Original or Enhanced |
|---|---|---|---|---|
| D-1 | automatic base building from the list | dead (AI-01) | code fix + data (`AutomaticallyBuild = Yes`, `Rebuilds`) | Enhanced |
| D-2 | Command Center auto-rebuild (Generals `3`, ZH China `-1`) | designed, blocked by AI-01 | code fix | Enhanced |
| D-3 | layout rotation `RotateSkirmishBases` | on in Generals, **off in Zero Hour**; positions only | data + AI-03 fix | Enhanced (ZH), data |
| D-4 | structure pacing (`StructureSeconds`, poor/wealthy rates) | inert (0 s, 3 s cap) | data + remove cap | Enhanced |
| D-5 | rally offsets of build-list entries | broken (AI-05) | one-token fix | Enhanced |
| D-6 | per-difficulty gatherer counts | Generals uses them (1/1/1 .. 1/3/6); ZH data uniform | data | Enhanced data |
| D-7 | multiple base-defence types | single `BaseDefenseStructure1` per side | code (list) + data | Enhanced |
| D-8 | team auto-reinforcement | 9 (G) / 26 (ZH) teams only | scripts | optional |
| D-9 | `calcClosestConstructionZoneLocation` | unused outside `buildSpecificBuildingNearestTeam` helpers | scripts | optional |
| D-10 | non-priority AI building via dozer "is able" check, power forcing | partially live | - | - |
| D-11 | `USE_DOZER` off path (instant build) | compile-time alternative | - | no |

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 5. What the AI "cheats" (nothing removed - documented)

**Resources.** There is **no** engine-level resource multiplier, build-time/cost handicap or income bonus for skirmish AI (`Handicap` comes from the player template, identical for AI and human). The advantage is **scripted money**
(`PLAYER_GIVE_MONEY`, aggregates from `evidence/x64-50/ai-statistics.json`, nominal maxima: each script fires once when its condition becomes true):

| | all difficulties | Normal only | Hard (Brutal) only |
|---|---|---|---|
| Generals (14 actions) | 200 - 1 300 "give AI money to cover bug" | - | 20 000 - 25 000 per side (Defcon-style bonuses 5 000 / 10 000) |
| Zero Hour (155 actions) | 200 - 2 350 per side ("cover bug") | ~3 500 - 4 000 (base-expansion gifts) | **71 000 - 99 000 per side**: AI Bonus Early +10 000 (escalation >= 5..10), Mid +10 000 (20..25), Late +15 000 (40..50), Really Late +20 000 (75), plus supply-drop +2 500 x4, uplink +5 000, expansions +4 000, ... |

`_ESCALATION` runs +1 per 10 s (Normal/Hard) / 20 s (Easy) and +30 / +50 when the enemy builds a tech centre / superweapon (Hard) - so the money schedule is a deterministic game clock plus enemy tech triggers. Easy gets no expansions and slower waves.

**Production / speed.** None in the engine for skirmish. The campaign-only modifiers (`HumanSoloPlayerHealthBonus` 150/100/80 %, `SOLO_AI_*` rate of fire 80/100/120 %) are gated by `GameLogic::isInSinglePlayerGame()` = `GAME_SINGLE_PLAYER` only (skirmish is `GAME_SKIRMISH`): **not applied in Skirmish**.
Brutal = `SLOT_BRUTAL_AI` -> `DIFFICULTY_HARD` (`GameLogic.cpp` skirmish setup); Medium -> Normal; there is no fourth level.

**Difficulty differences that exist:** script variants flagged Easy / Normal / Hard (Zero Hour America as the example; wave count and spacing: Hard 5 waves starting at escalation 1 every 30 - 45 s, Normal 4 waves 60 - 90 s, Easy 3 waves 210 s; build-order scripts per difficulty;
expansion only Normal/Hard; Hard-only attack-priority sets targeting supply lines and power), `ResourceGatherers` (Generals only), common-target attacking off for Easy (`Team::setTeamTargetObject`).

**Knowledge the AI has and a human does not (unchanged, to be decided later):**
1. *Hunting* (`AIHuntState`): `findClosestEnemy(owner, 9999.9f, CAN_ATTACK, ...)` - whole map, **no fog / line-of-sight filter** (the comment says "we want to find everything"); stealth filter still applies.
2. *Idle / guard acquisition*: for **human** units `AI::UNFOGGED | WITHIN_ATTACK_RANGE` are added (`AIUpdate.cpp:4677`); AI units do not get them.
3. *Scripts*: `SKIRMISH_VALUE_IN_AREA`, `PLAYER_HAS_*`, `SKIRMISH_UNOWNED_FACTION_UNIT_EXISTS`, `SKIRMISH_TECH_BUILDING_WITHIN_DISTANCE` read true object state (no shroud check) - used for invasion alerts, wave decisions, tech capture.
4. *Superweapons*: `getPlayerStructureBounds` / `getPlayerSuperweaponValue` iterate all enemy objects (sees the whole base; ignores stealth).
5. *Economy*: `findSupplyCenter` scans every supply source (all warehouses) with a 60/40 enemy-proximity rule.
6. *Pathfinding*: for computer players `isHuman = false` ("computer gets to cheat", 7 sites): they may path through the shrouded border outside the logical map extent and get adjusted destinations (`adjustDestination`, `clampToMap`).
7. *Reaction*: scripts evaluate every tick (cheap) with `delayEvaluationSeconds` staggering; no human reaction delay.
Absent: no unit-vision hacks, no free units, no production-speed bonus.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 6. Historical bug audit - upstream, forks, other references

External material is **research only**; each row says whether it was re-proved against our source.

### 6.1 TheSuperHackers/GeneralsGameCode (baseline `adac468d7` = 2026-10-02; read via the GitHub API)

| Reference | Claim | Verified in our tree? | Status here |
|---|---|---|---|
| #2407 (issue; PRs #2837, #2936 closed unmerged) | `processBaseBuilding` template bug + GLA hole shadowing | **yes, both** (AI-01, AI-02) | open upstream |
| #2679 | AI won't recreate Command Center (ZH) | **explained** by AI-01 + data (China `Rebuilds=-1`, others 0) | open |
| #2398 | asymmetric defence angle limit | **yes** (AI-11) | open |
| #2409 | hard-coded 3 s caps override `AIData.ini` | **yes** (AI-09) - but it is the retail pace | open |
| #2413 | retaliation bonus tests the wrong player | **yes** (AI-08) | open |
| #2420 | `m_currentEnemy` / `m_frameToCheckEnemy` not saved | **yes** (AI-10) | open |
| #2388 | loose-harvester branch lacks rebuild-hole check | **yes** (AI-14) | open |
| #2386 / PR #3380 | supply-attack window 10 frames | **fixed in our tree**, gated `!RETAIL_COMPATIBLE_CRC` (AI-13) | merged |
| #2462 / PR #2948 | `groupMoveToPosition` rectangle (`dy` uses x) | **yes** - but player commands only (AI-12) | open / closed |
| #3438 | temp-state null `onExit` crash | **applies** (AI-16), not in our baseline (merged 2026-10-08) | merged upstream |
| #1768 / #1343 | AI scaffold health preserved | in tree (gated `RETAIL_COMPATIBLE_CRC`) | merged |
| #1435, #1664, #2095 | crashes (sneak attack location, nuke under construction, invalid supply centre name) | crash fixes; not re-audited in depth | merged |
| #514 | AI recognises only the closest enemy | design (AI-20) | open |
| #100 / #1969 | lag when AI units look for waypoints on large maps / 8-AI stutter | matches X64.43 (guard / path failures); closed "not planned" for #1969 | open / closed |
| #49 | uni-directional map waypoints can mismatch AI | map data (class E) | open |
| #3328 | bot / agent API for Zero Hour | idea (I) | open |
| #1392 | `DEMO_TOGGLE_SPECIAL_POWER_DELAYS` applies to AI | debug only | open |

Note how the project treats such fixes: they live behind `RETAIL_COMPATIBLE_CRC` / `RETAIL_COMPATIBLE_XFER_SAVE` (our default **1**), i.e. upstream already uses a *compile-time* Original/Enhanced split.

### 6.2 Zero Hour Reforged (`asaprockky/generals-reforged`)

A data mod plus an engine patch against upstream. Its `retail-bug-audit.md` (74 data fixes) contains **no** skirmish-AI, AIData, build-list, dozer or team findings; it sets `RETAIL_COMPATIBLE_CRC = 0` (adopting ~30 upstream non-retail fixes), fixes unit auto-acquire data
(`NotWhileAttacking`, Stinger auto-attack of buildings), hole victory counting (BUG-012: destroyed GLA holes keep a player "alive") and Scud / Nuke exploits. **None applies to the AI archaeology findings**; BUG-012 and the auto-acquire data are *data-level*
candidates for a future Enhanced data pack. Its "70+ fixes" claim was not accepted as such: nothing was copied.

### 6.3 Other

Community write-ups (PPM forums: editing the ZH AI is hard) and AI mods for other games gave no applicable engine finding. No other source-available Generals AI fork with relevant fixes was found.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 7. Attack, congestion and performance contribution

Evidence base: X64.43 (580 failing path searches in a 72 000-tick 7-Brutal-AI benchmark: requester state GUARD 385, FOLLOW_PATH 92, HUNT 42, DOCK 42; command source SCRIPT 100 of 112 large ones; movers = computer ground units; 0 human), X64.44 (51 min, human + 5 Brutal, peak 1 227 objects,
no knee), plus the script/team data above.

| Observation | Cause class | Mechanism | Relevance |
|---|---|---|---|
| Idle / guard orders into sealed pockets (`TEAM_GUARD_AREA` = 671 uses in ZH) | map scripts + movement | each unit of the team asks A* for the guard area; stationary allied units seal the chokepoint (`checkForMovement`), search explores ~22 000 cells and fails | the largest measured tail (X64.43) |
| Closest-target hunting, whole-map scan every 30 frames per unit | AI decision + performance | Euclidean closest, no path or congestion awareness; every hunting unit re-queries the whole partition each second; targets across unreachable ground generate failing paths | CPU and routing |
| Trickle arrival | AI decision (team design) | ZH teams are small (mean 3.2 units, 1 221 singletons) launched by one global `_LAUNCH_ATTACK` flag; each follows `Center/Flank/Backdoor` by *team template*, not by congestion | piecemeal attacks, chokepoint funnelling |
| Waves are time-driven, not strength-driven | map/script design | Hard: first wave at escalation 1; waves keep coming regardless of losses | idle accumulation on defeat |
| Approach paths come from map waypoint paths named by enemy start index | map data | missing path = silent no-op; closest waypoint to centroid may skip the route's start | map-specific stalls (class E) |
| Units hovering at depleted resources | AI decision | harvester gatherer counts fixed per building (`ResourceGatherers` + 1), re-docking logic in `queueSupplyTruck` | minor |
| Repeated retries | movement / AI | no failure memory (X64.43 proved failure reuse unsafe at the pathfinder level: a caller-side cooldown is a behaviour change) | Phase 3 |
| Group move / formation | movement | `AIGroup` tighten and group paths for >= 3 units (`MinInfantryForGroup`...), random group offsets per unit (`GameLogicRandomValue` in `AIStates`) | spread at chokepoints |

Ownership of each item for later work: **AI decision** (hunt target choice, wave policy, team size), **map scripts** (guard areas, perimeters), **pathfinding** (sealed-pocket search cost - already studied), **movement** (group formation), **performance infrastructure** (partition scans, per-script evaluation).
Nothing here was optimised.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 8. Dynamic bases, determinism, and Original vs Enhanced

### 8.1 Feasibility of less predetermined bases (not implemented)

| Mechanism | Needs | Engine support today | Risk |
|---|---|---|---|
| choose between several complete base templates per side | code: pick a variant at `newMap` (`m_sideBuildLists` takes the **first** match only) + data variants | none | low; deterministic if the pick uses one `GameLogicRandomValue` draw at start |
| mirror / rotate / jitter the layout, rotate building angles | `RotateSkirmishBases` (exists) + AI-03 fix | partial | low |
| automatic placement of extra structures relative to base geometry | AI-01 fix + new planner | ring search / defence arc code is reusable | medium |
| defences oriented to actual threats | reuse `buildAIBaseDefenseStructure` with the *attacker's* approach direction instead of the map path; fix AI-11 | partial | medium |
| rebuild infrastructure intelligently (CC, supply, power) | AI-01 + AI-02 + data (`Rebuilds`) | partial | medium |
| more supply expansion | existing `buildBySupplies`, new script conditions / planner | yes | low |
| several valid build priorities | alternative script packs or a priority table | scripts are data | medium (authoring tool: WorldBuilder does not build for AMD64) |

### 8.2 Determinism rules (all candidate changes)

* AI executes inside `GameLogic::update` on every client; lockstep CRC covers it. Allowed randomness: `GameLogicRandomValue` (synchronised). Forbidden: OS randomness, time, pointer-ordered containers, unordered iteration, floating-point ordering differences between builds.
* Every added random draw shifts the shared stream for everything after it: acceptable **only** in Enhanced mode. Cleaner pattern for strategy variation: **one** draw per AI player at `newMap` seeds a *private* deterministic generator (xorshift on the draw) used by the planner - the global stream is disturbed once, not per decision.
* The current AI never reads wall-clock time (`rand()`, `timeGetTime`, `GetTickCount` absent in `AIPlayer/AISkirmishPlayer/AIStates`, pinned by the contract test). Replays: the starting-cash lesson of X64.41 applies - the AI mode must be part of the game options **and the replay header**.
* `RETAIL_COMPATIBLE_CRC` is a **compile-time** macro. A runtime mode is required for a product that ships both: Original = every changed site falls back to the original statements (compare with how upstream gates fixes); the Original path must be proven byte-identical by golden-replay CRC comparison (existing `DeterminismFingerprint` tooling, X64.38/44 replays).
* Multiplayer: both modes must agree between clients; the mode is a lobby rule, never local.

### 8.3 Proposed architecture (design only)

`AIBehaviorMode { Original, Enhanced }` as a synchronised game option (default Original), stored in `GameInfo` / replay header, read by the few seams: `processBaseBuilding` (AI-01/02), `adjustBuildList` / list variant, `buildAIBaseDefenseStructure`,
`computeSuperweaponTarget`, `acquireEnemy`, `selectTeamToBuild` timers, plus a data root switch (`Data/INI/Enhanced/...`, alternative script pack). Original = existing code untouched. Enhanced reuses the existing pipeline (script state machine, team templates, factories, dozers); new logic is added as small policy functions, never as a second AI.

**Where each audited item belongs**

| Bucket | Items |
|---|---|
| 1 shared engine correctness (no simulation effect) | AI-16 (crash, take upstream), AI-07 / AI-18 null / range hardening, AI-12 left alone (player command, sim-visible) |
| 2 Original preservation fix | **none recommended** (anything that changes behaviour would break the retail-compatible contract; only crash-class fixes with proven identical behaviour) |
| 3 Enhanced only | AI-01+AI-02 (together), AI-03, AI-04, AI-05, AI-06, AI-08, AI-10 (save version), AI-11, AI-13, AI-14, AI-19 |
| 4 map / data fix | base template variants, `RotateSkirmishBases` for ZH, per-difficulty gatherers in ZH, hole victory counts (Reforged BUG-012), approach-path coverage of maps |
| 5 optional future AI feature | fair-knowledge hunting / script conditions, dynamic base planner, congestion-aware routing, wave-by-strength, multi-defence types, bot API (#3328) |
| 6 reject / leave unchanged | AI-09 3 s cap in Original (retail pace), scripted money in Original, Hard 5-wave schedule in Original, AI-15, AI-17 |

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 9. Future Enhanced-AI roadmap (phases; each independently validated; none started)

| Phase | Content | Player-visible gain | Impl. risk | Determinism risk | CPU cost | Automated tests | Human tests |
|---|---|---|---|---|---|---|---|
| 0 | mode plumbing (lobby / options / replay header), seam functions, **Original golden replays** (G + ZH, 3 sides each) | none (safety net) | low | low (no behaviour change) | none | replay CRC identity Original vs current build; option round-trip; MP mismatch test | normal Skirmish + replay |
| 1 | engine correctness in Enhanced: AI-01+AI-02 together, AI-04, AI-05, AI-08, AI-11, AI-13/14, AI-16 (take upstream) | CC rebuilt, GLA holes handled, ZH superweapons aim better, defences keep building | medium (AI-01+02 interact) | medium: every fix changes sim, mode-gated | ~0 | unit fixtures for `BuildListInfo` / bounds / arc; headless AI-vs-AI replays Original unchanged, Enhanced reproducible (CRC stable across 3 runs) | GLA base destroyed / rebuilt; CC kill |
| 2 | dynamic base construction: template variants, angle rotation, expansion planner, threat-oriented defences, intelligent rebuild | different bases per match, bases follow terrain/threat | medium-high | medium (private seeded generator) | low (planner every few seconds) | layout legality tests (no overlap, buildable, deterministic per seed); replay determinism | several maps, compare base shapes |
| 3 | attack grouping / routing / congestion awareness: staging, wave-by-strength, route choice with failure memory, guard-area validity | fewer trickle attacks, fewer stuck armies, lower tail CPU | high | medium-high | reduces CPU | scripted congestion scenario (X64.43 workload); no-regression path stats | Twilight Flame-like 6-player map, long match |
| 4 | strategic variation / faction personality (seeded) | recognisably different openings | medium | medium | low | distribution tests over seeds | play 10 matches each faction |
| 5 | difficulty redesign: fair-knowledge (fog-aware hunting / scripts), replace money gifts by better macro | Brutal beats by skill not bonuses | high | medium | low-medium | knowledge-filter unit tests | blind playtests |

Each phase is gated by: Original identity (golden replays), 30 TPS tail (X64.44 telemetry), and a human game against the unchanged Original.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 10. Tooling, tests, reproduction

* `scripts/ai-audit/scb.py` - DataChunk / BIG reader (scripts, teams). `scripts/ai-audit/ai_audit_stats.py` - writes `docs/modernization/evidence/x64-50/ai-statistics.json` from the user's own Steam installation (read-only; aggregates only; retail scripts and team data are **not** committed).
* `scripts/native-runtime/test_ai_archaeology_contract.py` - 15 source-text tests pinning AI-01 .. AI-20 and the cheat / determinism facts (read-only; it fails when a future milestone changes one without updating this audit).
* Reproduce: `python scripts/ai-audit/ai_audit_stats.py --zh "<Steam Zero Hour folder>"` then read the JSON; the source citations above are `file:line` in this tree.

## 11. Unresolved questions (need a native / human or dynamic experiment; none run)

* **E1** AI-01 dynamic proof: in a *disposable stage*, override `Data/INI/AIData.ini` so a power-plant / barracks slot is `AutomaticallyBuild = Yes`, `Rebuilds = 1`, with no script marking it: expect the AI never builds it (and builds it after a temporary local patch). Predicted: confirmed.
* **E2** AI-02 in play: GLA AI vs human, destroy a GLA structure and watch for a duplicate / displaced rebuild and the missing 30 s delay (replay of the endurance workload can show it).
* **E3** Zero Hour China: destroy the Command Center and check it is never rebuilt (#2679).
* **E4** Effect of AI-04 on ZH superweapon placement (fire count, distance to optimum) in headless AI-vs-AI.
* **E5** Does a loose `Data/Scripts/SkirmishScripts.scb` / `Data/INI/AIData.ini` override take effect in the native build (it should; data pack feasibility for Enhanced)?
* **E6** Balance effect of the 3 s cap vs 10 s (AI-09) - needed only if Enhanced wants pacing as a lever.
* **E7** `[Skirmish]MyOuterPerimeter` / `CombatZone` trigger and `Center/Flank/Backdoor<N>` waypoint coverage across the shipped Skirmish maps (map scan from the MapsZH.big archives).
* **E8** Value of `UNFOGGED` omission for AI idle acquisition (matters only beyond own vision range).
* **E9** CPU cost split of whole-map hunt scans in a 1 000-object game (X64.43 profiler hooks).
