# X64.51 - Enhanced AI foundation and proven bug fixes (Generals + Zero Hour)

Status: **IMPLEMENTED + AUTOMATICALLY VALIDATED. Native human validation pending (checklist in section 12).** Start point: HEAD `2e0ba1ba6` (X64.50) = `origin/dev/modern-engine`.
Read with `X64_50_AI_ARCHAEOLOGY.md` (the audit this milestone acts on) and `AGENT_HANDOFF.md`.

This milestone adds a **run-time, per-match rule "AI Behaviour: Original / Enhanced"** and uses it to ship the first, deliberately small set of *repairs of proven EA defects*.
It is **not** the smart / dynamic AI (Phase 2 of the X64.50 roadmap is untouched): no random building positions, no planners, no adaptive build orders, no threat response, no personalities,
no fog-aware AI, no difficulty redesign, no balance change. Brutal's money bonuses, attack waves, build times, prices and damage are unchanged in both modes.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 1. Runtime mode architecture

**One value, one place: `GameInfo::m_aiBehavior` (`AIBehaviorMode`, 0 = Original, 1 = Enhanced).** It is a *game rule* like Starting Cash, not a local toggle:

| Path | Handling |
|---|---|
| Skirmish setup | `SkirmishGameOptionsMenu` (both titles) reads/writes it in `TheSkirmishGameInfo`; stored in Skirmish preferences (`AIBehavior=Original|Enhanced`, only written while the Modern Experience feature is on) |
| Match options string (skirmish start, replay header, LAN) | key `AI=E`, written **only when Enhanced**. Original strings are byte-identical to X64.50, so every Original replay header is unchanged. Parsed strictly (`AIBehavior::parseOptionValue`: `E`/`O`, anything else rejects the string). |
| Replay recording / playback | the replay header is the options string, so the mode is recorded and restored. A retail / X64.50 executable *rejects* an `AI=` header (unknown key -> invalid game info), so an Enhanced replay can **never silently play as Original** in an old build (verified negatively, section 6). A new executable plays an Original replay as Original and an Enhanced replay as Enhanced. |
| Multiplayer / LAN | the value is part of the host's options string, which every client parses; all clients simulate the same rule. The setup UI is the Skirmish screen only; LAN lobbies carry the value if present (default Original). |
| Save / load | `SkirmishGameInfo` xfer version 5 appends the value **only for Enhanced** (Original saves keep the retail layout, versions 2/4); each `AIPlayer` xfer version 2 appends `m_enhanced` **only for an Enhanced AI** (Original saves keep version 1). Loading an old save yields Original. |
| AI instance | `AIPlayer::m_enhanced` is latched in the constructor from `TheGameInfo` (set by `GameLogic::startNewGame` for skirmish / replay / LAN; campaign and General's Challenge have no rule -> Original). Every Enhanced branch tests this member; the **Original branch executes the original statements** (same calls, same order, same RNG use). |
| Classic | `-classic` hides the selector (`ModernExperience::FEATURE_AI_BEHAVIOR`) and forces Original. |

Pure decision helpers live in `Core/GameEngine/Include/Common/AIBehavior.h`; each returns the retail answer for Original (unit tested). `Common/AIDiagnostics.h` provides **read-only counters/events**
(`GENERALS_AI_DIAG*` env, inert when unset) and *scenario aids* used by the experiments (scheduled kills, forced defence orders, build-list dumps) - applied identically in both modes.
The existing compile-time `RETAIL_COMPATIBLE_CRC` is unchanged (default 1); the run-time seam is separate and does not depend on it.

Why this is safe for determinism: the mode is the same on every machine (options string), no Enhanced branch reads wall time or unsynchronised state, no Enhanced branch consumes the RNG
(`GameLogicRandomValue` call sequence is unchanged; the new branches are pure control-flow/data repairs), and iteration order of lists is the existing build-list / object-list order.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 2. Original is unchanged - the proof

1. **Golden gate** (`scripts/native-runtime/Invoke-AiBehaviorGate.ps1`): deterministic self-playing AI-vs-AI headless simulations (an idle human slot + computer slots, `-headless -replay`),
   FNV-1a digest over (GameLogic CRC + object count) every 30 logic frames for 15-20 simulated minutes. The pre-X64.51 executables (`build/baseline-x64-50`, built from HEAD `2e0ba1ba6`) and the X64.51
   executables were run in Original mode, twice each: **every digest is identical for all 8 scenarios of both titles** (table in section 8; raw: `evidence/x64-51/gate-*.json`).
2. The gate was re-run after the last code change (UI included) - it is the *final* state, not an intermediate one.
3. Source: every Enhanced change sits behind `m_enhanced`; the Original path was kept statement-for-statement (diff reviewed; AI-08 deliberately keeps the two `isSkirmishAIPlayer()/getCurrentEnemy()` calls exactly as before in Original).
4. Unit tests (`Tests/Google/Core/GameEngine/Common/AIBehaviorTest.cpp`, 9 tests): every helper returns the retail answer for `enhanced == false`.
5. Options strings / replays / saves of an Original match have the pre-X64.51 bytes (the `AI=` key and the extra xfer fields exist only for Enhanced).

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 3. Findings reproduced dynamically (E1-E4 and the Phase-1 set)

Method: a disposable stage, the synthetic replay generator (`New-SimulationBenchmarkReplay.py --ai-mode O|E`), identical scenario aids in both modes, read-only counters.
Script: `scripts/native-runtime/Invoke-AiFindingExperiments.ps1`; results: `evidence/x64-51/experiments-Generals.json`, `experiments-ZeroHour.json`. No Steam directory or user data was touched.

| Finding | Experiment | Original (retail) | Enhanced |
|---|---|---|---|
| **E1 / AI-01** automatic build entries unreachable | China Command Center is `AutomaticallyBuild=Yes` (non-priority); synthetic America list with an unscripted automatic Barracks (`scripts/native-runtime/ai-experiments/`) | Generals: 685 of 688 automatic-entry examinations rejected by the dozer check although the entry's *own* plan passes (`entry_plan_would_pass` = 688); 0 selections. Zero Hour China: 785 of 802 rejected, 0 selections. Synthetic list: 443 `blocked_by_wrong_template` events. | selections happen (Generals CC 3, ZH CC 6, synthetic list 2) |
| **E2 / AI-02** GLA rebuild-hole bookkeeping | GLA structures destroyed on schedule | slot linked to its hole keeps a stale (null) pointer: Generals 25 `stale_pointer`, 3 structures started inside the hole pass; ZH 5 stale, 2 started | pointer refreshed: Generals 17 `slot_kept_as_built`, ZH 5; no second structure started in the hole pass |
| **E3 Command Center rebuild** | CC destroyed repeatedly | Generals China (`Rebuilds=3`): only the first kill lands in the run and the CC is **never** rebuilt (E1 is the reason); ZH China (`Rebuilds=-1`): never rebuilt | Generals: 4 kills, 3 rebuilds (cap reached, then none - the counter works); ZH China: 5 kills, 5 rebuilds (unlimited, as the data says); ZH Tank General (`Rebuilds=0`): no rebuild |
| **E4 / AI-04** ZH superweapon fine search | Z-superweapons (5 AI, Tournament City) | the 11x11 refinement visited the diagonal only: 158 fine searches, **134 (85 %)** had a strictly better cell off the diagonal; summed value left on the table 131 038 | the whole 2D region (unit test: 121 cells vs 11, and a superset of the retail cells); the Enhanced search takes the best cell of the whole region |
| **AI-08** retaliation preference | 7-AI Twilight Flame, 5-AI Tournament City | `retail_bonus_to_unrelated_candidate`: 50 (Generals) / 16 (ZH) per run - an AI favoured a candidate because *another* AI targeted it | `enhanced_bonus_to_attacker`: 6 / 4 - only attackers are favoured, once |
| **AI-11** base-defence arcs | 40 front + 40 flank defences ordered at once | Generals: 22 `arc_exhausted_break` (retail gives up without advancing the alternation), 2 defences placed **outside** the intended +-60 deg sector; ZH: 15 breaks, 1 outside | `arc_exhausted_continue` 56 / 32: both sides bounded, alternation advances, 0 outside the sector |

AI-05 (rally offset), AI-13 (10 frames vs 10 seconds), AI-14 (harvester docking at a hole) and AI-16 (null temporary state) are proven by source + unit tests (they have no cheap dynamic scenario);
AI-14 and AI-16 are guards, AI-13/AI-14 are behaviour only when Enhanced.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 4. Fixes shipped (Enhanced mode only unless stated)

| ID | Where | Change | Generals / Zero Hour |
|---|---|---|---|
| AI-01 | `AIPlayer::processBaseBuilding` | dozer capability is checked against the **entry being examined**, not the plan accumulated so far | both |
| AI-02 | same loop | after a rebuild hole is linked to a slot the building pointer is refreshed; the hole scan stops at the first match | both (GLA) |
| AI-04 | `AISkirmishPlayer::computeSuperweaponTarget` | refinement Y offset uses the y index (`fineSearchYIndex`) | **ZH only** - Generals' loop was always correct and its Original code does not call the helper (not "ported") |
| AI-05 | build-list rally offset | `gotOffset = applyRallyOffset(...)` (retail `gotOffset;` was a no-op) | both |
| AI-08 | `AIPlayer::acquireEnemy` | retaliation preference tests the candidate itself, once | both |
| AI-11 | `buildAIBaseDefenseStructure` | `|angle| > 60 deg` on both sides, alternation advances, bounded search | both |
| AI-13 | `isSupplySourceAttacked` | attack memory window 10 s (retail 10 frames) | both |
| AI-14 | supply-center choice | skips rebuild holes like its sibling branch | both |
| AI-16 | `AIStates` temporary-state exit | null guard (**both modes**; no behaviour change while the pointer is set; upstream #3438 crash) | both |

Not changed: everything else in the X64.50 inventory (AI-03, 06, 07, 09, 10, 12, 15, 17-20 ...), all data (`AIData.ini`, scripts), cheats, difficulty, build lists.

### Player-visible effects of Enhanced (repaired EA systems, not new behaviour)
* A destroyed Command Center is **rebuilt** (China in both titles as the data intends: Generals 3 times, ZH China unlimited; the Tank General not at all). In Original it never comes back.
* GLA sides stop doubling structures next to their rebuild holes and stop sending harvesters to dock at a hole.
* Base defences are no longer placed outside the +-60 deg sector and keep being placed after the first arc fills; a stalled defence builder works again.
* AI armies prefer attacking the player that actually attacks them; the Zero Hour AI aims superweapons at better spots; AI harvesters/teams remember a supply attack for 10 s.
* In undisturbed games on small maps (the gate's desert / wasteland runs) the two modes are indistinguishable (identical digests) because the repaired code paths only act after a structure is destroyed or under pressure.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 5. Generals vs Zero Hour

Shared: `AIBehavior.h`, `GameInfo` plumbing, `AIBehaviorOptions` UI module, diagnostics. Per tree (retail code differs): `AIPlayer.cpp`, `AISkirmishPlayer.cpp`, `AIStates.cpp` carry the same seams.
Differences kept: AI-04 is Zero Hour only; the Generals `RotateSkirmishBases` default, ZH superweapon/special-power AI and ZH-only factions (Tank, Infantry, Nuke, Laser, Superweapon, Stealth, Toxin, Demo, Boss) stay as retail. UI: Generals row 1 of the Match Options block; ZH the retired Game Speed slider's place.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 6. Replay / save / network handling and negative checks

* Options string: `AI=E` only when Enhanced; unit test `OptionValueRoundTripAndStrictParsing`. Old executable on an Enhanced replay: **rejected** ("invalid game info", exit without simulating) - the safe failure mode.
* Replay playback fingerprints: Enhanced runs are repeatable (same digest twice) in all 8 scenarios of both titles.
* Save: Enhanced-only extra fields behind xfer versions (section 1). An Original save is byte-compatible with X64.50 readers; an Enhanced save is not meant to be loaded by an older build (version byte > supported -> the existing "unsupported version" error).
* Not exercised in an automated way: an interactive native save/load of an Enhanced game, and a real two-PC LAN match -> section 12.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 7. UI

Skirmish "Match Options": **AI Behaviour [Original (retail)] [Enhanced (repaired)]**, default Original, persisted in Skirmish preferences, hidden by `-classic`. It describes behaviour, not strength:
difficulty remains the per-slot Easy / Medium / Brutal choice (Enhanced is not Brutal). Implemented once in `Core/GameEngine/Source/GameClient/GUI/AIBehaviorOptions.cpp`; the Generals menu places it on the reserved row 1,
the Zero Hour menu in the area of the retired Game Speed slider. **Compiled and wired, but not visually verified in this milestone (no uncontrolled native session was run): first item of the native checklist.**

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 8. Performance (measured; 30 TPS objective, 33.333 ms budget)

Gate matrix (digest per run; Original digests identical base vs new, Enhanced repeatable twice). Raw: `evidence/x64-51/gate-*.json`.

| Scenario | Original (base = new) | Enhanced |
|---|---|---|
| G-desert3H | 659C8E11AC36F1E2 | same as Original |
| G-twilight7H | 4215BF3745E5B9D8 | FE4ABD185423CCC9 |
| G-wasteland5M | 97DD40E4D0A0E73A | same |
| G-desert3E | EBE872BC013C750B | same |
| Z-desert3H-gla | 49491F01A022DD00 | same |
| Z-city5H-mix | 4C0928A48678A5DA | E978FE7FDD6A3AB7 |
| Z-alpine3M-generals | 6EE51D5F24BDFA45 | B3106A15E1CED7D3 |
| Z-desert3H-china | 6A37BA1F5BBF97AC | same |

Heavy scenarios, three runs, nothing else running (`evidence/x64-51/perf-*.json`; headless simulation only, 36 000 logic ticks = 20 simulated minutes):

| | tick mean | p99 | p99.9 | worst | >33.3 ms | AIPlayer::update total | ai_object_base_update | logic scripts | path searches |
|---|---|---|---|---|---|---|---|---|---|
| G-twilight7H base Original | 1.64 ms | 8.64 | 14.39 | 102.6 | 16 | 1.95 s | 24.2 s | 4.39 s | 55 103 |
| G-twilight7H X64.51 Original | 1.89 | 9.45 | 14.75 | 119.6 | 16 | 2.25 s | 28.6 s | 5.15 s | 55 103 |
| G-twilight7H X64.51 Enhanced | 1.65 | 8.84 | 15.93 | 92.8 | 11 | 2.13 s | 24.7 s | 4.40 s | 57 956 |
| Z-city5H base Original | 2.23 | 7.51 | 15.65 | 29.1 | 0 | 4.04 s | 30.1 s | 5.13 s | 80 995 |
| Z-city5H X64.51 Original | 2.28 | 7.64 | 15.73 | 33.9 | 1 | 4.04 s | 31.0 s | 5.20 s | 80 995 |
| Z-city5H X64.51 Enhanced | 2.40 | 8.27 | 16.33 | 34.0 | 1 | 4.60 s | 31.5 s | 5.89 s | 84 386 |

Reading: run-to-run noise on this machine is about +-10-15 % (the earlier gate run measured the same Original binaries at 1.57 / 1.65 / 2.01 ms), larger than any Original X64.51-vs-baseline difference; Original path counts are identical
(55 103 / 80 995 searches). Enhanced differs because the AI does more (rebuilt structures, more path requests: +5 % / +4 %) - ZH AIPlayer::update +14 % = 0.016 ms per tick. No tick backlog, no catch-up beyond the
single >33 ms outliers that also exist in the baseline (worst 102 ms in the 7-AI map is a pre-existing script / path spike, X64.42-44). **No regression attributable to X64.51; nothing was optimised speculatively.** Native frame pacing was not measured.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 9. Automated validation

| Suite | Result |
|---|---|
| `g_googletest` / `z_googletest` | 436 / 435 passed (new: 9 `AIBehavior` tests) |
| `test_ai_archaeology_contract.py` (source contract incl. all seams, versions, UI wiring) | 18 OK |
| `test_skirmish_match_options_contract.py`, `test_modern_extensions.py` | OK (2 native-only skips each) |
| Golden Original gate, both titles, 8 scenarios, 2 runs each | PASS (identical to X64.50 executables) |
| Enhanced repeatability, same matrix | PASS |
| win64 build `g_generals` + `z_generals` | OK |

## 10. Packaging

Built with `scripts/native-runtime/New-MKRaudsteinDistribution.ps1` after the code commit; names, sizes and SHA256 are in section 13.

## 11. Known limits

* Enhanced is *opt-in and only as strong as the data*: the retail build lists have `AutomaticallyBuild = No` for all but the Command Center, so AI-01 changes only that slot until data opts in (not done).
* The selector is Skirmish only; LAN lobby UI has no control (mode defaults to Original).
* No interactive native verification of the UI, an Enhanced save/load round trip, or LAN yet.

## 12. Native human test checklist

1. Skirmish screen (Generals and Zero Hour): "AI Behaviour" label + dropdown visible, aligned, not overlapping other controls; default Original; choice survives leaving and re-entering the screen; absent with `-classic`.
2. Play ~10 min Original vs Brutal: behaves like before (no rebuilt Command Center after you destroy it).
3. Same with Enhanced: destroy the AI Command Center -> it is rebuilt; GLA AI keeps its hole behaviour sane; no crash.
4. Record a replay in each mode, play it back in the same build: it plays in the mode it was recorded in. Try the Enhanced replay with the previous (X64.49) build: it must refuse.
5. Save during an Enhanced game, load it, continue 2+ minutes; same for Original.
6. (Optional) two PCs, LAN Skirmish with Enhanced in the options.
7. Frame pacing of a Twilight Flame 7-AI Enhanced game is no worse than Original.

## 13. Package names and hashes

Built from commit `a01703845` with `New-MKRaudsteinDistribution.ps1` (both titles validated, 26/26 checks each; `build/dist/`, not committed):

| File | Size | SHA-256 |
|---|---|---|
| `MKRaudstein-Generals-x64-Preview-a01703845.zip` | 4 027 468 B | `633B3F16C7F5D8CADFC7B0D5FB638C2A74926E9A9A030F5934DB5144DE7AE2C2` |
| `MKRaudstein-ZeroHour-x64-Preview-a01703845.zip` | 4 132 982 B | `F8DCC48C94456E7816B548B4B3294127BAA2D574B0492B775F5FFDFA7C38BA8C` |
| `...-Generals-...-Symbols.zip` / `...-ZeroHour-...-Symbols.zip` | 22 005 908 B / 22 508 261 B | (separate, in the manifests) |

Staged candidates `dist-a01703845-gen` / `dist-a01703845-zh`; the launcher default is `dist-a01703845-zh`. The packages contain every runtime change through X64.49 plus X64.51 (built from the HEAD tree, no allowlist).

## 14. Deferred (Phase 2 and beyond, not started)

Random / dynamic base layout, planners, adaptive build orders, threat-responsive defence, personalities, smarter composition, congestion-aware attacks, fog-aware AI, difficulty redesign,
opting more slots into `AutomaticallyBuild`, a LAN lobby control, and the remaining X64.50 findings. Human review is required before any of it starts.

### Launching the Zero Hour candidate (default of `Run-ZeroHourCandidate.cmd`)

`build\dev-runtimes\native\ZeroHour\dist-a01703845-zh\game\generalszh.exe`, or `pwsh scripts/native-runtime/Run-ZeroHourCandidate.ps1 -Name dist-a01703845-zh`
(Generals: `build\dev-runtimes\native\Generals\dist-a01703845-gen`).
