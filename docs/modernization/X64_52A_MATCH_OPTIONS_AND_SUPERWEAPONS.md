# X64.52A - Match Options UI completion and the Superweapons match rule (Generals + Zero Hour)

Status: **IMPLEMENTED + AUTOMATICALLY VALIDATED. Native human validation pending (checklist in section 12).** Start point: HEAD `ad15878ec` = `origin/dev/modern-engine` (verified; clean tree).
This milestone supersedes the X64.52 packages as the human acceptance candidates: it contains X64.52 Enhanced Dynamic Base & Defence **plus** the corrected Match Options panel, the AI Behaviour
selector, Starting Cash and the new Superweapons rule. Read with `X64_52_ENHANCED_AI_DYNAMIC_BASE.md`; X64.53 is not started.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 1. Match Options UI

### Findings (screenshots of the X64.52 candidates, 1920x1080)
* **Generals**: the AI Behaviour row sat directly on the Battle Honors statistics frame (the frame started where the second row ended) and left no room for a third option.
* **Zero Hour**: AI Behaviour had been put where the retired Game Speed slider was, **under the visible "Game Speed" caption** (the retail caption is hidden by a text comparison that fails
  on some localisations), its own caption was clipped to "AI Beha", and the retail "Limit Superweapons" check box and Starting Cash were laid out on the same band with different rhythms.

### Design (all geometry in `Common/MatchOptionsLayout.h`, pure and unit tested; nothing is an absolute pixel)
* **Generals** keeps its visual language (header "Match Options", caption + dropdown rows on the grid's 24-unit rhythm) with **three rows**: Starting Cash, AI Behaviour, Superweapons, flush under
  the header. The Battle Honors statistics block makes room by a vertical **squeeze** (factor ~0.73): it starts 6 units below the third row and ends where it always ended; its lines keep their order and left edges.
* **Zero Hour** keeps its band design (no "Match Options" header in the retail screen): the band between the player grid and the statistics holds **three caption + dropdown pairs of equal width**,
  left edge = the grid's left edge, right edge = the map preview's right edge, one row of the grid's height. The retail Starting Cash caption / dropdown are moved into the band (caption left aligned like the
  other two), the retail Game Speed caption is hidden **by name**, and the retail "Limit Superweapons" check box is replaced by the Superweapons dropdown (Classic keeps the retail check box).
* One shared control module (`GameClient/MatchOptionControls.*`) builds the two new dropdowns by copying the look of the screen's own dropdown / caption and places retail dropdowns correctly
  (entry field, drop-down button and list follow the combo). The entries are the short, plain words **Original / Enhanced** and **Disabled / Max 1 / Max 3 / Max 5 / Unlimited** so they fit at every width.
* The Modern Experience gates: `-classic` shows none of the new controls (AI Behaviour = Original, Superweapons = Unlimited, the retail ZH check box stays).

### Verification
* New developer flow `GENERALS_AUTOFLOW=matchoptions` (wrapper `scripts/native-runtime/Invoke-MatchOptionsAudit.ps1`, screenshots in `evidence/x64-52a/ui/`): per run it records the geometry of every control, **audits
  every pair of visible windows for partial overlap**, captures the closed screen, **opens each dropdown, captures it and probes that every row of the open list is reachable by the mouse path**
  (`findWindowUnderMouse`). The wrapper hashes the player's preference files before and after (a guard: nothing was written).
* **18 configurations, both titles, 0 defects, 0 overlaps, every row of every open dropdown reachable**: client sizes 1920x1080, 2560x1440, 3440x1440, 5120x1440 (Auto UI scale) and, at 1920x1080,
  UI scales 75 / 100 / 150 / 200 / 300 % (`GENERALS_FORCE_UI_SCALE`, a developer aid inert unless set).
* Unit tests: `MatchOptionsLayout` (3 rows on the rhythm, squeeze bounds and monotonicity at 8 UI scales; the band's equal pairs / gutters / bounds at 8 scales x 5 window sizes; combo parts) and the Python contract
  `test_skirmish_match_options_contract.py`, whose data-level half runs against the **real retail `Window.big`**: at every UI scale the three rows touch no retail control that stays visible.
* **Crash found and fixed during validation (reproducible, root cause)**: the first Zero Hour band code dereferenced `comboBoxPlayer[0]`; slot 0 of the Skirmish grid is the player's name entry field,
  not a dropdown (null) - the game died (0xC0000005) when the Skirmish screen opened. Fixed by taking the grid's edge and row height from slot 1 (contract `test_zero_hour_does_not_dereference_the_human_slot_dropdown`).

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 2. The Superweapons match rule

**Values:** Disabled, Max 1, Max 3, Max 5, **Unlimited (default = retail behaviour, nothing is written anywhere)**. A match rule for **humans and both AI Behaviour modes**; it is not an AI setting.

### Exact semantics
* The limit is on how many superweapon **structures a player owns at the same time**; it restricts **starting** one more. It is not a lifetime quota: destroying or selling a structure frees the capacity at once.
* "Owns" = completed structures, structures **under construction** (a construction site is an object from the moment the command is accepted, so two commands in the same frame cannot both pass), structures **captured** from an enemy,
  and standing **GLA rebuild holes** of a superweapon (the hole holds the capacity of the structure it will rebuild - the retail data comment says the same: "count all superweapons AND THE GLA SCUD STORM REBUILD HOLE as one type").
  Dead objects are not counted. There is no queue to count: structures are never produced by a factory queue; the check sits in front of the construction command.
* **Disabled:** no superweapon structure can be started. **Max N:** refused when the owner count is >= N.
* General Powers (A-10, Artillery Barrage, Fuel Air Bomb, Spectre ...) are not structures and are never affected.
* Campaign and General's Challenge: `GameLogic::getSuperweaponLimit()` reads `TheGameInfo`, which is null in a Campaign and is the Challenge info (never given a rule) in General's Challenge: they keep their authored rules.

### Identification (metadata, not object-name lists)
`ThingTemplate::isSuperweaponStructure()` (cached per template):
1. **Zero Hour data marks them**: `MaxSimultaneousOfType = DeterminedBySuperweaponRestriction` (+ `MaxSimultaneousLinkKey = Superweapon`). The audit (`scripts/ai-audit/superweapon_audit.py`, retail data of both titles, read-only,
   output in `evidence/x64-52a/superweapon-audit.txt`) finds **22 structures**: 20 tagged `FS_SUPERWEAPON` (USA: Particle Cannon, Air Force General, Laser General's Particle Cannon / Laser Cannon uplink / Laser Cannon, Super Weapon General, CINE and
   Boss variants; China: Nuclear Missile of China / Infantry / Nuke / Tank General and Boss; GLA: Scud Storm of GLA / Toxin / Demolition / Stealth, the GC_ copies and Boss) plus the two untagged Super Weapon General launchers
   (`SupW_AmericaNuclearMissileLauncher`, `SupW_AmericaCruiseMissileLauncher`), and the GLA Scud Storm rebuild holes. The data marker is used as it is.
2. **The original Generals data has no marker.** There a STRUCTURE is a superweapon when one of its special power modules (`SpecialPowerModuleData::isSpecialPowerModuleData()`, a new virtual on `ModuleData`) names
   `SPECIAL_PARTICLE_UPLINK_CANNON`, `SPECIAL_NEUTRON_MISSILE` or `SPECIAL_SCUD_STORM` (Zero Hour additionally the `SUPW_` / `LAZR_` / `NUKE_` copies, in `#if !RTS_GENERALS`). Holes are recognised through
   `RebuildHoleBehaviorInterface::getRebuildTemplate()`.

### Counting and enforcement (one authoritative path)
`SuperweaponRule::blocksStart(player, template)` (Core, shared) is **false immediately** for Unlimited (no counting, no cost) and for non-superweapon templates; otherwise `countOwned` walks the owner's objects (only while a limit is
active and only when a superweapon template is asked about - build button refresh, construct command, AI order selection; there is no per-frame scan).
* **Human**: `BuildAssistant::canMakeUnit` (Generals, before the prerequisite test, like Zero Hour's own "maxed out" order) and `Player::canBuildMoreOfType` (Zero Hour, which `canMakeUnit` and `canBuild` call) return
  `CANMAKE_MAXED_OUT_FOR_PLAYER`. `DozerAIUpdate::construct` / `WorkerAIUpdate` re-check it **on the simulation side** (they always did for humans), so a hidden or bypassed button cannot place a site.
  The build buttons use the normal Generals semantics for that code (the "maxed out" state and its description text).
* **AI (Original and Enhanced alike)**: the AI never asked `canMakeUnit` for scripted structures (retail: "just build it"). `AISkirmishPlayer::processBaseBuilding` now **skips** a superweapon order while the rule blocks it - it is
  neither attempted nor allowed to hold up the orders behind it (the retail loop starts only the first priority order) - and selects it again as soon as capacity is free; `AIPlayer::buildStructureWithDozer` refuses as defence in depth.
  With Unlimited both are no-ops: **Original + Unlimited is the preserved Original** (gate, section 8).
* **Capture**: nothing in the rule destroys, rejects or flags a captured structure; ownership may exceed the cap and construction stays refused until the owner count is below it.
* No money, no stat, no timing and no balance value is touched by the rule (contract-tested).

### Replay, save, network
* **Replay / lobby options string**: key `SW=` written **only** for a non-default value (`SW=1`, `3`, `5`, `D`). Unlimited writes nothing: every default replay header is byte-identical to before. The parser accepts only those values; an
  unknown value or an executable that does not know the key refuses the string (verified: the X64.51 build refuses an `SW=1` replay; this build refuses `SW=2` and `SW=7`, plays `SW=1` and `SW=D`).
  The ZH legacy `SR=` (the retail LAN check box) is untouched and independent.
* **Save / load**: `SkirmishGameInfo` xfer **version 6** (written only when the limit is not Unlimited or the game is Enhanced; Original + Unlimited saves keep the retail version 2 / 4), through `SuperweaponLimit::xferLimit`
  (round trip unit-tested; an invalid stored value reads as Unlimited). No count is saved: it is derived from the world, so loading counts what is there and destroyed structures free capacity as usual.
* **Network / LAN**: the value is part of the synchronized `GameInfo` / options string, and enforcement runs inside the deterministic simulation (a pure function of the world), so every peer enforces the same rule. The LAN
  lobby has no Modern Match Options control (unchanged scope): a LAN game carries the default.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 3. Dynamic evidence

All runs are headless deterministic simulations (`Invoke-AiBaseExperiments.ps1 -Sw <rule> -SwAid <script>`; aids `give` / `capture` / `kill` / `probe` / `build` of `AIDiagnostics.h`, applied to the human slot too).

### 3.1 Human enforcement, capture, destruction, simultaneous construction (`make_sw_aids.py`, `check_sw_events.py`)
For every rule value, in **both titles and both AI modes (20 runs)** the scripted human (an America player with a dozer and a Strategy Center) is checked: below the cap `canMakeUnit` allows and `DozerAIUpdate::construct`
creates a site (the owner count rises immediately, so a second command in the same step sees it); at the cap both refuse; a captured enemy superweapon (`Object::setTeam`) raises the count above the cap with nothing destroyed and construction
stays refused; destroying structures one by one allows construction again exactly when the count is below the cap and a site is created again; Unlimited builds on top of six owned ones; Disabled refuses at zero owned.
**Result: 0 failures in 20 runs** (14 checks per capped run, 3 per Unlimited / Disabled run; `evidence/x64-52a/swmatrix/`).

### 3.2 AI at the cap (Original and Enhanced)
Zero Hour Tournament City (5 AIs, 20 min), the Super Weapon General AI builds 5 Particle Cannons when Unlimited:

| rule | peak owned (Super Weapon General) | superweapons started (all AIs) | order skipped at the cap |
|---|---|---|---|
| Unlimited | 5 | 8 | 0 |
| Max 5 | 5 | 8 | 0 |
| Max 3 | **3** | 6 | 779-885 |
| Max 1 | **1** | 3-4 | 1 873-2 104 |
| Disabled | **0** | **0** | 4 694-4 762 |

(Original and Enhanced give the same pattern.) Generals Twilight Flame (7 AIs): the same, Disabled = 0 superweapons started in both modes. Freed capacity is used: with Max 1 and the AI's Particle Cannon destroyed twice, the AI **started a replacement each time**
(1 -> 3 started, peak owned stays 1, Original and Enhanced). The AI keeps building everything else at the cap - structures started other than superweapons, Unlimited -> Disabled: Zero Hour Original 184 -> 179, Enhanced 209 -> 178; Generals Original 101 -> 104, Enhanced 109 -> 110 (Max 1 / Max 3: 174-186 in Zero Hour, unchanged in Generals where each AI builds one anyway).

### 3.3 Starting Cash and AI Behaviour regression
* Starting Cash: the human receives exactly the chosen amount (5 000 / 10 000 default / 20 000 / 50 000 checked) and the AIs the amount plus their retail difficulty bonus, in Original and Enhanced. With `SC=20000` the X64.51 and X64.52A executables produce **identical
  fingerprints in both titles** (the option's simulation path is unchanged); the default setup is unchanged (gate). The control displays, opens and every row is reachable at all 18 UI configurations; its selection handler is untouched.
* AI Behaviour: Original is the default, `AI=E2` replays play, other revisions are refused (X64.52 evidence unchanged); the control is visible, aligned and every row reachable; `-classic` hides it and forces Original; Campaign / Challenge never get a rule.

---------------------------------------------------------------------------------------------------------------------------------------------------------------

## 4. Original preservation, performance, tests

### 4.1 Original AI stays the preserved Original
The golden gate (`Invoke-AiBehaviorGate.ps1`: FNV digest of the GameLogic CRC every 30 frames, X64.51 executables as the reference, two runs each for repeatability) was re-run on the **final** X64.52A executables
(`evidence/x64-52a/gates/`): **GATE PASS in Generals and Zero Hour** - the Original digest equals the X64.51 reference for every scenario (Unlimited, the default), and every run (Original and Enhanced) is repeatable.
Unlimited never reaches the counting code: `SuperweaponRule::blocksStart` reads the match value and returns before looking at the template.

### 4.2 Performance
* Unlimited (default): one integer read and compare per build query; Original path-search counts are identical to X64.51 (55 103 / 80 995 in the two benchmark scenarios) and the per-tick `AIPlayer::update` cost is equal within the noise of the machine.
* Limit active (worst case, Max 1, 20-minute AI-vs-AI matches with 5-7 AIs): a temporary timer around the count measured **3 540-4 252 counted queries and 5.9-6.9 ms in total per match** (about 1.5-2 microseconds each),
  i.e. well below 0.01 ms per simulation frame averaged. The count runs only when a superweapon template is queried, never per frame; nothing is cached and nothing is saved. (The timer was removed again; `evidence/x64-52a/superweapon-rule-cost.txt`.)
* The benchmark machine was loaded by other applications during the broader tick-time runs, so the absolute mean/p99 numbers in `evidence/x64-52a/gates/` are noisy (Enhanced worst ticks of 90-137 ms against 37-53 ms in quiet X64.52 reruns). They are reported, not used as evidence of a change.

### 4.3 Automated tests
* Google Test: Generals 458, Zero Hour 457, all passed (`SuperweaponLimitTest`, the new layout tests of `DisplaySettingsTest`).
* Python: `scripts/native-runtime` (including `test_superweapon_limit_contract.py`, the rewritten `test_skirmish_match_options_contract.py`, the X64.51 / X64.52 contracts), `scripts/performance`, `scripts/diagnostics`.
* Dynamic: switch matrix 20 runs / 0 failures, AI at the cap, Starting Cash, UI audit at 18 configurations (sections 1 and 3).

### 4.4 Classic, Campaign, Challenge, multiplayer
* `-classic`: the AI Behaviour and Superweapons controls are not built, the retail Zero Hour check box is kept, Original and Unlimited are forced.
* Campaign and Challenge: `GameLogic::getSuperweaponLimit()` reads the Skirmish game info (null in Campaign); the Challenge game info is never given a rule.
* LAN / online lobbies have no Modern control (unchanged scope). The value is serialized (`SW=`) and enforced inside the deterministic simulation, so a game that carries it is enforced identically on every peer.

### 4.5 Packages (code commit `8d1e2e18b`, `build\dist`)
Built by `New-MKRaudsteinDistribution.ps1` (both titles in one run, clean install + ZIP == staged candidate file-for-file, overlay test PASS for both). No PDB in either player ZIP; symbols are separate archives. The staged executables are byte-identical to the gated `build\win64` executables.

| file | size (B) | SHA-256 |
|---|---|---|
| `MKRaudstein-Generals-x64-Preview-8d1e2e18b.zip` | 4 050 103 | `13E32AF7FB95072B1446CC5DEFE8FA77F2E005726983270BB6AB15A82E291D7C` |
| `MKRaudstein-ZeroHour-x64-Preview-8d1e2e18b.zip` | 4 155 201 | `0B4A9F55A0C50BAE4843E409C561D23ADC7B0127F70516E687596049ACB41C88` |
| `MKRaudstein-Generals-x64-Preview-8d1e2e18b-Symbols.zip` | 22 137 626 | (see `MKRaudstein-Generals-x64-Preview-8d1e2e18b.manifest.json`) |
| `MKRaudstein-ZeroHour-x64-Preview-8d1e2e18b-Symbols.zip` | 22 496 788 | (see `MKRaudstein-ZeroHour-x64-Preview-8d1e2e18b.manifest.json`) |

Combined list: `MKRaudstein-x64-Preview-8d1e2e18b.SHA256SUMS`. Both contain X64.52 Enhanced AI (`AI=E2`), the corrected Match Options, AI Behaviour, Starting Cash and the Superweapons limit.

### 4.6 Human native test (one run of the whole current feature set)
Launch the candidate (not the X64.52 packages): `build\dev-runtimes\native\ZeroHour\dist-8d1e2e18b-zh\game\generalszh.exe`, or `pwsh scripts/native-runtime/Run-ZeroHourCandidate.ps1 -Name dist-8d1e2e18b-zh`
(Generals: `build\dev-runtimes\native\Generals\dist-8d1e2e18b-gen`), or install the two ZIPs over the Steam installations as in X64.48B.
1. Skirmish > Game Options at 1920x1080, then your widest resolution and one larger UI scale: Starting Cash, AI Behaviour and Superweapons sit in one tidy row/column, nothing overlaps, open each drop-down and pick an entry in the row below it.
2. Defaults: AI Behaviour Original, Superweapons Unlimited, Starting Cash unchanged. Start a match; superweapon buttons behave as retail.
3. Superweapons = Max 1: build one Particle Cannon / Nuclear Missile / Scud Storm; the second's button is greyed (maxed out). Destroy the first: the button comes back. Max 3 / Max 5 the same at 3 / 5. Disabled: no superweapon structure can be started. General Powers still work.
4. Capture an enemy superweapon (Max 1): it stays yours (not destroyed) and your own build button stays greyed.
5. Vs. AI with Max 1 / Disabled, Original and Enhanced: the AI never exceeds the cap and keeps building the rest of its base.
6. Save and load a Skirmish with a limit, and replay a recorded one: the limit holds. Campaign and General's Challenge: no Superweapons control, retail behaviour. `-classic`: no new controls.
7. Report anything that overlaps, mis-clicks or differs.

### 4.7 Known limits
* Zero Hour's own retail "Limit Superweapons" check box is hidden in Modern mode (the new control supersedes it); an old ticked preference migrates to Max 1 once and the legacy preference is not rewritten.
* Performance numbers above are from a loaded machine (see 4.2).
* Replays that carry `SW=` are refused by builds older than X64.52A (unknown key), by design.
