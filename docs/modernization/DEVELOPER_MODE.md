# Interactive Developer Mode / Quick Game — Stage4B.1 runtime review

Status: initial developer manual runtime validation PASS (2026-10-09). Stage4A.3 automated workload
investigation is PAUSED. This adapter is interactive convenience, not a replacement
reproducible benchmark. All earlier automation and bounded forensic diagnostics remain.

## Startup and safety

Normal launch retains normal intro/gameplay. `-skipIntro` retains the existing tested
intro skip. `-devMode` enables the overlay/actions in a normal offline skirmish.
`-devMode -quickGame` also skips intro and disables the shell map after INI loading,
without bypassing normal intro completion/shell initialization. Flag order is irrelevant;
`-quickGame` alone has no effect. `-devPreset battle` optionally creates the initial
interactive battle after Quick Game readiness; other preset names are unsupported.

The new independent adapter waits for complete intro, no movie, no map/save loading
and no game-data clear. It refuses networking, replay playback and an existing live
match. It validates cached stock multiplayer Twilight Flame metadata and China faction,
allocates the ordinary engine-owned SkirmishGameInfo lazily, and configures:

- human slot0/start0, China, color0/team0, local IP0;
- enemy easy China AI slot1/start1, color1/team1;
- starting cash10000, fixed seed0x4b1001, stats off, runtime render cap120.

It uses ordinary `startGame`, new-game RNG initialization and `MSG_NEW_GAME` dispatch.
After map loading it requires Skirmish mode, active SkirmishGameInfo, a valid active
non-neutral human local player equal to slot0, and a distinct active non-neutral
computer from slot1 with ENEMIES relationship. It then hands over control. There is
no warmup controller, relocation assertion, automatic capture or automatic exit.
Failures report in the overlay/debug output and leave the application interactive.

All mutations require `-devMode`, no scenario, offline Skirmish, no replay/network,
no loading/clearing and a valid non-neutral local human. Enemy actions additionally
validate slot1 and enemy relationship. Neutral is index0 and is explicitly rejected.
No “first active computer” enumeration is used here. Developer state is bounded,
process-local and not serialized. Normal save formats and normal command processing
are unchanged. Saved developer-created units are ordinary game objects; loading a save
is not a reproducible benchmark continuation. Tracked IDs clear across loading/menu
transitions. Do not combine Quick Game with replay/campaign/network flags.

## Actions and overlay

Hold **Ctrl+Alt+Shift**, press/release the listed key (action occurs on release):

| Key | Action |
|---|---|
| F5 | Toggle overlay |
| F6 | Deposit100000 in local human player's ordinary money service |
| F7 | Existing instant-build toggle only where compiled; deferred in standard Release |
| F8 | Reveal map / remove developer reveal with ordinary reshroud service |
| F9 | Spawn12 human mixed Redguard/BattleMaster units |
| F10 | Spawn96 human mixed units |
| F11 | Start/stop configured performance capture between measured frames |
| F12 | Spawn48 human mixed units |
| O | Spawn96 opposing slot1 mixed units |
| M | Issue normal move orders to tracked armies toward the opposite deployment area |
| B | Issue normal attack-move orders; if no tracked army, create96 units per side first |

Spawn uses ThingFactory/default team, checks actual controlling owner, ground layer and
ordinary footprint legality; unsuccessful creations are destroyed through GameLogic.
At most384 creations are retained/tracked per game,512 placement candidates per action;
partial placement is reported. Repeated spawns reuse deployment areas and may be limited
by occupied terrain. Deployment anchors are map-relative at22%x/22%y and22%x/65%y, with
48-unit grid spacing. Camera looks at human deployment after spawning. Orders use normal
AICommandInterface with CMD_FROM_SCRIPT, no custom movement/destination policy or retry.
Surviving ownership is rechecked for each order. The preset uses these same actions once,
then leaves control with the developer. AI/pathfinding-specific additional presets are
follow-up, not implemented. Instant upgrades are deferred too.

F8 removal restores engine shroud processing, not a snapshot of previously explored fog.
Normal unit visibility remains. No full visibility snapshot system was added.

Overlay refreshes at2Hz: FPS/frame ms, measured elapsed TPS/tick, last recorded GameLogic
ms, object count (one existing linked-list count per refresh, never every render), last
recorded-frame dispatched queue paths/cells, profiler/path state, elapsed/max60s, cumulative
path record capacity/drops/errors and developer status. Timings/queue data are unavailable
or stale when not recording and labelled as recorded data; they are not live engine gauges.
Overlay can be off throughout capture. Search detail is represented by cumulative path
records, not a new live search instrument. No new profiler or per-render world scan.

## Guarded developer review (PowerShell7, repository root)

The helper validates the isolated inventory and absence of game processes first. On a
real launch it makes a verified user-data backup through the existing guarded launcher,
creates a fresh ignored `build/performance/dev-<UTC>-<id>` output directory, prints that
path, and launches only the staged binary. Validation-only creates no directory/backup,
receipt or game process. Never use CMake Install into Steam.

Validation without launch:

```powershell
pwsh -NoProfile -File .\scripts\performance\Invoke-ZHDeveloperGame.ps1 -ValidateOnly
```

**TEST1 — one launch command:**

```powershell
pwsh -NoProfile -File .\scripts\performance\Invoke-ZHDeveloperGame.ps1
```

Expect intro skipped, normal loading and direct Twilight Flame gameplay; no automatic
shutdown. Confirm READY human slot0/enemy slot1 and normal interaction. Test F5 overlay,
F6 money, F8 reveal/reshroud, F9 small group and selection/player color, F10 army, O enemy,
M movement/B battle (all with Ctrl+Alt+Shift). Test F11 to start, wait several seconds,
F11 to stop. Expect all five profiler reports in the printed directory. Exit normally
through the game menu. Send reports and launch/exit receipts, plus observations or errors.
Spawn ownership is enforced in code; selectable human units/opposing colors are the visual
review. The developer has now confirmed initial TEST1 and TEST2 runtime success; later changes still require review.

**TEST2 — fast heavy normal battle/pathfinding workload:**

```powershell
pwsh -NoProfile -File .\scripts\performance\Invoke-ZHDeveloperGame.ps1 -Preset battle
```

The preset creates96 units per side and orders an attack move once after readiness;
no economy, menus or production preparation. Confirm visible movement/battle. Toggle F5
off, F11 start; observe without pausing, then F11 stop after20–40seconds (automatic60s or
capacity ceiling remains). If necessary issue M/B again manually to stress surviving units;
record those actions and timing. No automatic exit/next trial. Exit normally. Send ALL
`*-summary.txt`, `*-categories.csv`, `*-frames.csv`, `*-slow.csv`, `*-paths.csv` from the
printed directory, exact launch argv/receipts and brief workload/action observations.
Do not overwrite accepted captures. If capacity stopped capture, don't press F11 to
“stop” again: it starts another capture. Check the overlay state first.

Analysis must validate drops/errors/caps/interpolation/phase stride and exclude incomplete
final attribution. Report GameLogic mean/p95/p99/max, Internal inclusive/self/max/severe
operations, non-overlapping root contribution, head pops/info attempts/forward hops/work,
and sampled line-self/neighbor-self/insertion shares without multiplying by64 as exact
full-search time. Compare to accepted Stage4A evidence as context, not a controlled speedup:
this is a different interactive workload. A measured improvement requires comparable
reference/optimized runs with the same setup/actions and matching deterministic work.
The paused automated scenario is not required for either review.

## Implementation / validation boundaries

Shared Core DeveloperInteractive.cpp owns the independent startup/action adapter; existing
DeveloperToolsRuntime dispatches it, handles chords/capture and draws the overlay. Existing
engine/logic/input/UI hooks remain gated. Public pure ownership predicate is regression
covered for neutral/same/inactive/wrong-type owners. Readiness and all action gates retain
focused tests. Dedicated command-line tests verify both flag orders, default and skipIntro,
and reapplication after simulated INI loading. Source checks are not a game launch or
replay/save/lockstep runtime test. See STAGE4B1_PATH_POLICY_HOIST.md for optimization proof.

## Successful first manual runtime review (2026-10-09)

Developer confirms TEST1 PASS: intro skipped, direct Twilight Flame entry, interactive
gameplay, overlay/money/reveal/spawn/capture controls and normal exit. TEST2 PASS:
battle preset spawned96human/96opposing units, moved/fought normally, remained
interactive, captured profiling and exited normally. Instant build unavailable in
Release matches the documented deferred action and is not a blocker. Receipts
13fe37c403534aaa958cb3a98c892bdb (TEST1) and0533331afb3d44dc9a38acf5d3724e26
(TEST2) both show the staged EXE hash and normal exit0.

The exact battle report stopped at60_second_limit, not developer_stop; capture
controls were manually confirmed, while the report's actual stop reason remains
automatic. See [STAGE4B1_INTERACTIVE_CAPTURE.md](STAGE4B1_INTERACTIVE_CAPTURE.md)
for validated measurements and the proposed next meaningful optimization.
