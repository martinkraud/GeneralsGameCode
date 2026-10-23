# X64.52 - Enhanced AI Dynamic Base & Defence (Generals + Zero Hour)

Status: **IMPLEMENTED + AUTOMATICALLY VALIDATED. Native human validation pending (checklist in section 14).** Start point: HEAD `60dd27844` = `origin/dev/modern-engine`
(repository, handoff and X64.51 report agreed; no other agent had pushed). Read with `X64_50_AI_ARCHAEOLOGY.md`, `X64_51_ENHANCED_AI_FOUNDATION.md` and `AGENT_HANDOFF.md`.

This milestone makes **Enhanced** AI bases more varied, better defended, economically more active and better at recovering - **through the existing EA mechanisms** (the build
list, the dozer construction path, `buildAIBaseDefense`, `findSupplyCenter`) and a small amount of new, bounded, deterministic policy. It is **not** a combat rewrite, **not** a
planner and changes **no balance**. `AI Behaviour = Original` is untouched (section 9); `-classic`, campaigns and General's Challenge stay Original.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 1. Phase A - what the source already offered (re-audited against the current tree)

| EA mechanism | What it really does (verified in source) | Used by X64.52 |
|---|---|---|
| `SkirmishBuildList` slots (`BuildListInfo`) | fixed world offsets from the Command Center, rotated by `adjustBuildList` according to the start position (Generals: 45 degree steps because `RotateSkirmishBases = Yes`; Zero Hour: one fixed angle, `= No`); building angles are *not* rotated (retail quirk) | **reused**: Enhanced adds one extra bounded rotation and small slot shifts at the same place (`AISkirmishPlayer::adjustBuildList`) |
| Priority orders | scripts (and `addToPriorityBuildList`) flag slots; `processBaseBuilding` builds the **first** priority slot in list order; the flag is never cleared | **reused** as the only way Enhanced orders structures (supply expansion, threat defence, fill-in, recovery all add / flag slots; the retail loop builds) |
| `buildAIBaseDefense` / `buildAIBaseDefenseStructure` | places defences on arcs around the base, facing a scripted waypoint path (`Center<n>`, `Flank<n>`, `Backdoor<n>`) or the enemy's structure bounds; bookkeeping of left / right angles | **reused and steered**: the arc centre becomes the observed attack direction (after the X64.51 AI-11 repair) |
| `findSupplyCenter` / `buildBySupplies` | chooses the warehouse nearest the base that has supplies, is not already served and is not closer to the enemy; places a supply center a few cells from the dock | **reused** for the choice and the placement rule (the placement *search* is replaced by a cheap probe, section 6) |
| `m_rebuildDelaySeconds`, `Rebuilds` | per-slot rebuild allowance, enforced for automatic slots only | **reused**; Enhanced adds a bounded grant for the Command Center / supply centers (section 7) |
| Fallback placement (`buildStructureWithDozer`) | when a slot is illegal, a square-ring search with up to 120 cells radius runs; it costs 100-350 ms when it finds nothing | **bounded** for Enhanced (section 6) |
| Hole bookkeeping, automatic path | repaired in X64.51 (AI-01, AI-02) | foundation |

Not found / not usable: no per-slot "importance", no threat memory, no expansion logic outside scripted calls, no dynamic layout. Those are the new parts.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 2. Architecture

* `Core/GameEngine/Include/Common/AIEnhancedBase.h` - **pure** rules (hash, layout variants, jitter, threat map, caps, failure memory). Includes only `<cmath>` / `<cstdint>`; unit tested.
* `Core/GameEngine/Include/Common/AIEnhancedBaseXfer.h` - save / load of the state (shared by the engine and the round-trip test).
* `Core/GameEngine/Source/GameLogic/AI/AIEnhancedBase.cpp` - the **single shared implementation** for both titles (compiled into each tree's engine; uses only API both trees have).
* Small seams in each tree: `AIPlayer.{h,cpp}` (state member, once-per-second `enhancedBaseUpdate`, placement probe + bounded fallback), `AISkirmishPlayer.{h,cpp}` (layout variation in
  `adjustBuildList`, threat steering and seed-dependent first side in `buildAIBaseDefenseStructure`, failed-slot skip in `processBaseBuilding`).
* Everything is behind `AIPlayer::m_enhanced` (X64.51). The Original branch of every edited function executes the original statements.

### Determinism
* Decisions are pure functions of `(match seed, player index, game state)`. The seed is `GameInfo::getSeed()` (in the replay header, saved with the game); the per-player seed is
  `playerSeed(seed, playerIndex)`. **No logic RNG draw, no clock, no OS entropy** (source contract `test_ai_enhanced_base_contract.py` enforces it).
* Threat memory is integer (sector weights, integer decay); lists are iterated in build-list / object-list order; the cadence is `(frame + 7 * playerIndex) % 30`.
* Cost control: one scan per AI per second over its own build-list objects; nothing runs per tick except an early exit.

### Save / load / network
* `AIPlayer` xfer **version 3**, written only by an Enhanced AI (Original keeps version 1): appends `AIEnhancedBase::State` (seed, chosen layout variant, counters, cooldown frames,
  the failed-slot memory and the 16 threat weights). A load restores every value that influences later decisions; the round trip is unit tested (`StateRoundTripsThroughSaveAndLoad`).
  Version 2 (an X64.51 Enhanced save) loads with a default state.
* All layout / expansion / defence choices are made identically on every peer (same seed, same inputs) - no local-only AI decision exists.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 3. Phase B - deterministic base variation

* **Layout rotation** (once, in `adjustBuildList`): five variants, extra rotation of 0, -11.25, +11.25, -22.5, +22.5 degrees about the Command Center. Each variant is scored by how many
  build slots are terrain-legal (`isLocationLegalToBuild`, `TERRAIN_RESTRICTIONS`; object overlap is deliberately not scored because the destroyed start Command Center is still in the
  world at that moment); the variant is picked from the seed among those within one slot of the best. The retail rotation (start-position dependent in Generals) is applied first, so
  Generals keeps its eight-direction orientation and Zero Hour its fixed orientation, each varied by at most 22.5 degrees.
* **Slot shifts**: non-critical slots (not the Command Center, not supply centers / cash generators) move by up to 2 cells, never more than a **quarter of the free gap** to the nearest
  other slot (two neighbours moving toward each other keep half their gap), and only if the move keeps the slot terrain-legal.
* **Defence side**: which side of the front arc is used first depends on the seed.
* **Order variation**: when Enhanced activates an unscripted slot (section 5) the choice among eligible slots depends on the seed and changes once per simulated minute.
* No arbitrary coordinates: everything is a small, bounded perturbation of the retail layout.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 4. Phase E - threat-responsive defence

* **Observation** (`enhancedScanThreats`, once per second): for each structure of the AI's own build list, a new damage since the last scan whose source is a living *enemy* object adds one
  observation to the sector (16 sectors of 22.5 degrees) of the source's direction from the base center. This uses the damage the base actually took (no map-wide scan, no fog bypass beyond
  what the damage report already contains).
* **Memory**: integer weights, decay ~0.4 % per second (half-life about three minutes), cap 60 observations per sector.
* **Steering**: when a sector is dominant (at least 6 smoothed observations), front defences face that direction instead of the scripted path; the arc bookkeeping restarts when the threat moves
  more than 45 degrees. The AI-11 arc limit still bounds the sector.
* **Autonomous reinforcement**: at 14 smoothed observations, at most one extra base defence per 75 simulated seconds, at most 6 per match, only with twice the defence's cost in the bank and
  no unbuilt defence order pending. It uses the faction's own defence structure through `buildAIBaseDefense`.
* Flank / backdoor defences keep their scripted paths.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 5. Phases C / D - completeness and expansion

* **Supply expansion** (`enhancedExpansion`): from minute 4, if the AI owns fewer supply centers than 2 (3 after minute 10), none is on order, and it has the cost + 300 spare, it asks the retail
  chooser (`findSupplyCenter`) for the best uncovered source and orders a supply center there with the retail placement rule. At most 3 expansions per match; a failed attempt
  (no source / no legal site) counts toward a cap of 3 failures, so it cannot loop. Trucks follow the existing `checkForSupplyCenter` path; no money is given to the AI.
* **Fill-in** (`enhancedFillIn`): from minute 5, when the treasury exceeds the cost plus half of the data's `Wealthy` level and the scripted queue has nothing pending, one unbuilt production /
  tech slot (Barracks, War Factory, Airfield, tech, strategy / internet centre, black market; **never** superweapons, base defences, power plants, fakes) is flagged as a priority order. The
  retail script remains in charge: in the measured games the queue is almost never empty, so this fires about once per game.
* No slot is made `AutomaticallyBuild`; X64.51's AI-01 repair already lets the (few) automatic slots work.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 6. Two validation findings that shaped the design

**6.1 Placement cost.** The first performance run showed Enhanced single-tick spikes in `AIPlayer::update` of **350 ms** (and 100-140 ms several times). Cause: the retail fallback placement
search (`buildStructureWithDozer`, up to 120 cells radius, several thousand `isLocationLegalToBuild` calls) came up empty for a slot, repeatedly; `buildBySupplies` runs a similar 400 unit search.
Original has the same code but its slots are always placeable; Enhanced's rotated layout produced a few unplaceable ones. Enhanced-only fixes: (1) a **near probe** (at most 65 legality
checks, 8 rings of 8 neighbours) tried before the retail search - a hit relocates the slot so the retail path finds it legal at once (it hit 50-145 times per game); (2) the retail search
radius is **bounded to 50 cells**; (3) a slot whose search comes up empty is **remembered for 3 minutes** (8 entries, saved) and skipped, also by the retail priority selection; (4)
supply expansion uses the near probe instead of the 400 unit wiggle. Worst `AIPlayer::update` tick: 351 ms -> 23 ms (Original 22 ms).

**6.2 Head-of-line blocking (a retail property that Enhanced would have amplified).** `processBaseBuilding` starts only the **first** priority order of the list; every order behind it waits.
An order that cannot start (enemy units on the site, no free dozer, unreachable) therefore freezes the whole base build queue for as long as it stays first. In the threat experiment an
expansion supply center ordered next to enemy units made the Enhanced AI build **4** instead of **13** Gattling Cannons while 9 600 money sat unused. Fix (Enhanced only): an order that has been
unable to start for 45 simulated seconds **steps aside for 3 minutes** (`enhancedNoteStartFailure`); expansion also refuses unsafe sites. After the fix the same scenario builds 12-13
defences. The step-aside fires 8-58 times per 20-minute game (a measure of how often the retail queue is really stuck); it never touches Original.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 7. Phase F - rebuilding

Retail semantics are respected: `Rebuilds` is untouched; scripted priority slots rebuild as before. New, Enhanced-only: a destroyed **Command Center or supply center whose allowance is spent**
may receive **one more rebuild** while the treasury holds the structure's cost plus half the `Wealthy` level, at most one grant per 20 simulated seconds and **24 per match**. Other structures follow the
retail rules (repeated destruction tests in section 11).

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 8. Generals vs Zero Hour

Shared implementation, per-title seams. Differences kept: Generals rotates its layout per start position (retail `RotateSkirmishBases = Yes`), Zero Hour does not; Zero Hour has
`buildSpecificBuildingNearestTeam`, `calcClosestConstructionZoneLocation`, the larger faction set and the X64.51 AI-04 fix; Generals' retail defence offset uses the base radius only
(Zero Hour adds `SkirmishBaseDefenseExtraDistance`). Supply centers are recognised by the `SupplyCenterDockUpdate` module (the `FS_SUPPLY_CENTER` tag is not in every data set).

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 9. Original preservation

* **Golden gate** (`Invoke-AiBehaviorGate.ps1`, deterministic AI-versus-AI headless simulations, FNV-1a digest of GameLogic CRC + object count every 30 frames): the X64.51 package
  executables (`build/baseline-x64-51`, HEAD `60dd27844`) and the X64.52 executables, **Original mode, 2 runs each**, 4 scenarios per title. **All 8 digests identical** (and, because X64.51 was
  proven equal to X64.50, equal to the retail-compatible baseline). The gate was run again after the last code change.
* Source: every new branch is behind `m_enhanced`; Original executes the same statements as before (contract `test_ai_enhanced_base_contract.py`, `OriginalIsNotTouched`).
* `-classic`, campaigns and General's Challenge: unchanged (the rule resolves to Original there).

| Scenario | Original, X64.51 = X64.52 | Original equal | Enhanced X64.52 (2 runs, identical) |
|---|---|---|---|
| G-desert3H | `659C8E11AC36F1E2` | equal | `412741810A5D8B66` |
| G-twilight7H | `4215BF3745E5B9D8` | equal | `7CB3C4A94266AACB` |
| G-wasteland5M | `97DD40E4D0A0E73A` | equal | `84B5683033140DBA` |
| G-desert3E | `EBE872BC013C750B` | equal | `0AC11BCBB8ACEA46` |
| Z-desert3H-gla | `49491F01A022DD00` | equal | `BDB99201CC07258C` |
| Z-city5H-mix | `4C0928A48678A5DA` | equal | `627FC0735895E862` |
| Z-alpine3M-generals | `6EE51D5F24BDFA45` | equal | `E60DCCF39E3FFFCF` |
| Z-desert3H-china | `6A37BA1F5BBF97AC` | equal | `8146397B57DA931D` |

Every Enhanced scenario now differs from Original (in X64.51 several undisturbed small-map scenarios were identical): the layout variation is active from the first tick.

## 10. Replay revision, save / load, network

* **Revision in the options string.** The Enhanced value is `AI=E2` (revision 2). X64.51 wrote `AI=E` (revision 1). A build accepts only its own revision: an X64.51 build refuses `E2`, X64.52
  refuses `E`, `E1`, `E3`... ("Cannot open replay"; verified with the real executables and header-patched replays). Without this, an X64.51 Enhanced replay would play with different
  decisions in X64.52. Original (`AI=O`/absent) is unchanged and plays in every build.
* **Same seed -> same decisions, new seed -> new decisions** (section 11.1): 5 seeds give different layouts per title; the gate plays each Enhanced scenario twice with identical digests.
* **Save / load**: `AIPlayer` xfer v3 (Enhanced only). Unit test `StateRoundTripsThroughSaveAndLoad` writes the state with `XferSave`, reads it with `XferLoad` and compares every field and the
  dominant-threat decision. An end-to-end native save/load of an Enhanced game is on the human checklist; headless automation cannot save mid-game.
* **Network**: no peer-local decision exists; seed, state and inputs are identical on every peer. A mixed-revision LAN game is refused at the options string.

## 11. Dynamic evidence (isolated, disposable stages; raw summaries in `evidence/x64-52/evidence-summary.json`)

All runs are headless AI-versus-AI simulations with the diagnostics of `Common/AIDiagnostics.h`; scenario aids are applied identically to Original and Enhanced. Commands: `build/x64-52-evidence.sh`
pattern = `Invoke-AiBaseExperiments.ps1` (see the script header), reports by `scripts/ai-audit/x52_census_report.py`, `x52_tick_spikes.py`, `x52_collect_evidence.py`.

### 11.1 Base variation
Generals Tournament Desert, China AI (player 3), 14 build slots, Enhanced, five match seeds. Shift of the build list against the Original layout (world units; one cell = 10):

| seed | variant | mean shift | max shift |
|---|---|---|---|
| 51002 | 0 deg (slot shifts only) | 3.2 | 8.0 |
| 51003 | -11.25 deg | 40.9 | 83.7 |
| 51004 | 0 deg (slot shifts only) | 2.9 | 8.1 |
| 51005 | -11.25 deg | 40.7 | 83.7 |
| 51006 | -22.5 deg | 82.4 | 166.1 |

Zero Hour Tournament Desert, China (18 slots): 5 seeds -> variants +22.5, +11.25, -22.5, +22.5, +11.25 degrees (mean shifts 92.8 / 48.3 / 94.1 / 93.8 / 46.3). Original: identical layout in every run. The
visible difference between two matches is a base rotated by up to half of a Generals' 45 degree step plus slightly shifted secondary buildings; what is where relative to the Command Center and to
each other is preserved.

### 11.2 Resource expansion
Zero Hour Tournament City (5 AIs, 4 seeds, 20 minutes) and Generals Twilight Flame (7 AIs, 4 seeds): Enhanced orders 3-7 expansions per game (cap 3 per AI) at sources chosen by the retail
`findSupplyCenter`. Supply centers alive at the last census: Zero Hour seed 51102 **8 vs 6** (Original), 51103 **12 vs 7**, 51104 **9 vs 4**, 51105 6 vs 5; Generals 51002 **7 vs 6**, 51003 6 vs 6,
51004 6 vs 10 (the Original AI of that game hoarded 10; the games diverge), 51005 6 vs 7. Unsafe sites are refused (`expand.unsafe_site` 1-6 per game) and failures are capped (3), so there is no
expansion loop. Sites are deterministic: the same seed reproduces the same coordinates (`x52.expand.ordered ... at=x,y source=x,y` events).

### 11.3 Threat-responsive defence
Scripted harassment (diagnostics aid): from frame 4 000 an enemy unit shoots the AI base 60 times from a chosen compass direction, then 6 front and 6 flank defences are ordered at frame 6 000.
Defences within 60 degrees of the attack direction (of all base defences at minute 8):

| attack from | Generals Original | Generals Enhanced | Zero Hour Original | Zero Hour Enhanced |
|---|---|---|---|---|
| 0 deg | 4 of 14 | **10 of 13** | 6 of 19 | **12 of 20** |
| 90 deg | 0 of 14 | **4 of 11** | 3 of 19 | **8 of 20** |
| 180 deg | 3 of 13 | **10 of 15** | 4 of 18 | **12 of 20** |
| 270 deg | 7 of 12 | 8 of 13 | 8 of 18 | 9 of 19 |

Original places defences on the scripted approach regardless of where the damage comes from; Enhanced re-centres the front arc on the observed direction (270 degrees coincides with the
scripted approach on that map). The total number of defences is comparable (no spam).

### 11.4 Rebuilding under repeated destruction
Scripted kills of one China AI: 5-6 Command Centers, 5 Barracks, 5 Supply Centers, 3 War Factories (+ Airfields in Zero Hour) over 25 minutes.

| | Generals O | Generals E | Zero Hour O | Zero Hour E |
|---|---|---|---|---|
| kills that found a target | 14 of 18 | 17 of 18 | 12 of 21 | 21 of 21 |
| Command Center rebuilt | **0** | **4** (retail allowance 3 + 1 recovery grant) | **0** | **6** (retail `Rebuilds = -1`) |
| Barracks started | 6 | 6 | 3 | 6 |
| Supply Center started | 6 | 6 | 3 | 9 |
| War Factory started | 4 | 4 | 3 | 5 |

In Original the first Command Center loss ends the AI's ability to rebuild it (X64.50 finding, repaired in Enhanced by AI-01); without it later kills have nothing to hit. The recovery grant
fired once in Generals (the retail allowance of 3 was spent); `Rebuilds` itself is never rewritten and no structure besides Command Centers and supply centers gets a grant.

### 11.5 Long runs (runaway / loop check)
8 games per title (4 seeds x Original / Enhanced, 20 simulated minutes, 5-7 AIs). Largest number of any single structure type per player: Generals Original 7 / Enhanced 5-6; Zero Hour Original
10-16 / Enhanced 13-20 (the Superweapon General's retail script builds 16 power plants and 14 Patriot batteries in Original; Enhanced reached 20 and 19 in a game where that AI stayed solvent -
`money 275` at the end instead of 13 550 unspent - that retail script behaviour is not touched). No assertion, crash or hang in any run; no construction loop (every counter that could loop is
capped); no player starved by the new code (fill-in needs half the `Wealthy` reserve left, expansion needs cost + 300).

## 12. Performance (30 TPS objective, 33.333 ms budget; 36 000 logic ticks = 20 simulated minutes each, 3 runs per cell, nothing else running)

| | tick mean | p99 | p99.9 | worst | >33.3 ms | AIPlayer::update total | path searches |
|---|---|---|---|---|---|---|---|
| G-twilight7H X64.51 Original | 1.37 ms | 7.48 | 12.31 | 91.1 | 11 | 1.70 s | 55 103 |
| G-twilight7H X64.52 Original | 1.42 | 7.69 | 11.97 | 90.1 | 13 | 1.74 s | 55 103 |
| G-twilight7H X64.52 Enhanced | 1.43 | 8.41 | 11.79 | 79.5 | 8 | 2.25 s | 53 245 |
| Z-city5H X64.51 Original | 2.11 | 7.06 | 15.65 | 29.2 | 0 | 3.89 s | 80 995 |
| Z-city5H X64.52 Original | 2.14 | 7.22 | 16.15 | 27.8 | 0 | 3.97 s | 80 995 |
| Z-city5H X64.52 Enhanced | 2.28 | 8.21 | 16.77 | 43.7 | 3 | 4.62 s | 97 015 |

* Original X64.52 vs X64.51: identical path-search counts; differences are inside the +-10 % run-to-run noise of this machine.
* Enhanced adds 0.014-0.018 ms/tick to `AIPlayer::update` (once-per-second scans, probes) and, because the Enhanced AI does more (more supply centers, more units, more path requests: +20 % in the
  Zero Hour game), up to +0.04 ms/tick of path search. Worst ticks are the same pre-existing path-search spikes (X64.42-44); in four extra Zero Hour reruns the Enhanced worst tick was 37-53 ms against 29-47 ms
  for Original (0-2 ticks over 33.3 ms per 36 000 for Enhanced, 0-1 for Original); no backlog beyond 10 ms.
* Two isolated stalls (673 ms and 240 ms, each in one of three runs of a perf cell) were attributed to **no** simulation phase (or to an AI phase at a frame that costs 14 ms in every rerun of the identical
  replay): operating-system stalls of this machine, not reproducible, not counted above.
* No speculative optimisation was done. The two fixes that were needed (section 6) were caused by this milestone and measured before and after.

## 13. Automated validation

| Suite | Result |
|---|---|
| `g_googletest` / `z_googletest` | 448 / 447 passed (new: `AIEnhancedBaseTest` incl. the save/load round trip; `AIBehaviorTest` revision parsing) |
| `test_ai_enhanced_base_contract.py` (new) | OK |
| `test_ai_archaeology_contract.py` (X64.50 / X64.51 contracts) | 18 OK (updated: AIPlayer save version 3 for Enhanced, Original still 1) |
| all other `scripts/native-runtime`, `scripts/performance`, `scripts/diagnostics` Python suites | OK |
| golden Original gate, 8 scenarios, vs the X64.51 executables | PASS (identical) |
| Enhanced repeatability, 8 scenarios | PASS |
| negative replay checks (E3 refused, old build refuses E2, legacy `E` refused) | PASS |
| win64 Release build `g_generals` + `z_generals` | OK |

## 14. Human native test checklist

Central question: does Enhanced AI now build and defend visibly more competently, varied and responsively, while Original still behaves exactly like the preserved game?

1. Skirmish, **Original**, Brutal AI, any map: plays as before; destroy the AI Command Center - it does **not** come back (retail).
2. Same map, **Enhanced**: the AI base is slightly rotated / spaced differently from Original and **differs between two matches** (restart the match: new seed -> different arrangement); it is still a tidy base.
3. Enhanced: destroy the AI Command Center and a supply center repeatedly - they are rebuilt (several times); GLA holes behave.
4. Enhanced: attack the AI base repeatedly from one side (the same lane for 3-4 minutes) - new defensive structures appear on **that side**, not only on the scripted approach; no wall of defences.
5. Enhanced, 10+ minutes, a map with several supply sources: the AI builds a second / third supply center at a nearby source and keeps harvesting.
6. Enhanced, 7-AI Twilight Flame for 15 minutes: no freeze or hitch (a "pause" of a tenth of a second would be a regression: report the time), no crash. **If a crash happens do not retry - keep the
   Windows Error Reporting dump and the log.**
7. Record an Enhanced replay, watch it in the same build: identical. An Original replay from an older build still plays. (An X64.51 Enhanced replay is refused with an error - expected.)
8. Save an Enhanced game mid-match, load it, continue 2 minutes; same for Original.
9. `-classic`: no AI Behaviour selector; Original.

## 15. Known limitations / deferred

* Variation is deliberately small (at most 22.5 degrees and a few cells): bases are recognisably the same design.
* Fill-in of unscripted slots fires about once per game because the scripted queue is rarely empty; no slot is made `AutomaticallyBuild`.
* The retail script over-builds power plants / defences for some Zero Hour generals (Original too); not touched.
* The threat memory sees the damage the base takes, not approaching armies; there is no scouting or fog-aware planning (X64.53+).
* Layout legality is scored on terrain only; a rotated slot that turns out to be blocked by objects is handled by the placement probe / fallback, not by re-planning the layout.
* No attack-wave, combat-group, composition, congestion, retreat or pathfinder changes (X64.53 scope). Difficulty and balance are untouched.
* Native visual / save-load / LAN acceptance pending (checklist above).

## 16. Packages

(filled in after the build - `evidence/x64-52/packages.txt`)
