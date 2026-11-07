# MKRaudstein Modern Engine - project status

One-page, evidence-linked status. Updated at every milestone closeout together with `README.md` and `AGENT_HANDOFF.md` (rule since UI.1: a milestone that changes what is true updates this file in the same milestone).
Last updated: 2026-11-07 (Modern Match Options + Game Speed, after the Modern UI product-completion audit). Product-level plan: [../PRODUCT_ROADMAP.md](../PRODUCT_ROADMAP.md). Detail per milestone: [FIRST_PLAYABLE_ROADMAP.md](FIRST_PLAYABLE_ROADMAP.md), [AGENT_HANDOFF.md](AGENT_HANDOFF.md).

## Current direction
The broad performance phase is **closed** ([PERFORMANCE_FINAL_AUDIT.md](PERFORMANCE_FINAL_AUDIT.md): no remaining exact low-risk optimisation, no performance release blocker) and Enhanced AI development is **paused** (gates stay in force). Active phase: **Modern Experience UI** ([MODERN_UI_ARCHITECTURE.md](MODERN_UI_ARCHITECTURE.md)).
Do not start X64.53 (AI combat coordination) before the owner decides to resume AI work.

## Completed (evidence in the linked documents)
| Area | State |
|---|---|
| Native AMD64 + Direct3D 11 engine, normal launch, original asset loading | working, Generals and Zero Hour (X64.1 - X64.45) |
| World rendering: terrain, objects, water, shroud, shadows, particles, radar | working in both titles |
| Generals and Zero Hour Skirmish play through victory/exit, save/load, audio | working; Zero Hour human-tested on desktop and laptop (stable, noticeably faster than retail) |
| Authoritative 30 TPS simulation, independent presentation, smooth motion, FPS presets | delivered, determinism and Original digests gated |
| Exact serial CPU optimizations | delivered ([PERFORMANCE.md](../PERFORMANCE.md)); further optimization paused |
| Modern Display (native resolution, borderless, UI scale, anchors, Hor+, ultrawide fixes) | working (X64.33, X64.48A) |
| Modern Options (categories), Modern Settings | working |
| Enhanced HUD framework v1 + USA / China / GLA themes | native-accepted, frozen ([ENHANCED_HUD_FRAMEWORK_V1.md](ENHANCED_HUD_FRAMEWORK_V1.md)) |
| Camera scrolling in presentation time (X64.49) + screen-edge fix and real-input gate (X64.52B) | fixed; **awaiting human verification** at 120 FPS |
| Overlay player packages for both titles (ZIP over the legitimate install, separate symbols, SHA-256, automatic retention cleanup) | working |
| Skirmish match rules: Starting Cash, AI Behaviour (Original / Enhanced), Superweapons limit (Disabled / 1 / 3 / 5 / Unlimited) | delivered (X64.51 - X64.52A), `-classic` hides them |
| Enhanced AI foundation + dynamic base and defence (`AI=E2`) | delivered, opt-in per match, Original untouched and gate-protected (X64.51, X64.52) |
| AI archaeology and bug audit | delivered (X64.50) |
| UI archaeology + responsive layout foundation + Classic/Modern contract test (UI.1) | delivered |
| Modern main menu / navigation shell for Generals and Zero Hour: left-docked panel over the shell map, pure focus/navigation model, design-system tokens (UI.2) | implemented and automatically validated (layout matrix, real-window audit, Classic isolation, golden gates); **native visual acceptance pending** ([MODERN_UI_2_MAIN_MENU.md](MODERN_UI_2_MAIN_MENU.md)) |
| Modern Frontend system (UI.3): procedural design system, redrawn main menu, Options (rail, generated rows, dirty state), Skirmish Match Setup (Map / Players / Rules from specs), Load Game / Replays, dialogs, Credits - both titles, 5 surfaces x 5 UI scales | implemented and automatically validated (gtest, Python, native audit matrix, Classic isolation, golden gates); **native visual acceptance pending** ([MODERN_UI_3_FRONTEND.md](MODERN_UI_3_FRONTEND.md)) |
| Multiplayer smoothness audit (LAN): presentation interpolation in network games, render cap honoured in LAN, logic-rate ceiling = 30, frame-delivery telemetry | implemented and automatically validated (gtest incl. lock-step model, golden gates both titles, A/B on the laptop); **real two-PC re-test pending** ([MULTIPLAYER_SMOOTHNESS_AUDIT.md](MULTIPLAYER_SMOOTHNESS_AUDIT.md)) |
| Engine completion audit: AMD64 pointer scan + upstream ports, D3D11 flip-model swap chain, render-hitch attribution, CPU tail / long-session measurements | implemented and automatically validated (gtest, golden gates both titles, Python suites); hitches attributed (simulation tail, terrain full update, first-use assets), not all fixed ([ENGINE_COMPLETION_AUDIT.md](ENGINE_COMPLETION_AUDIT.md)); **real two-PC validation of 2071ad765 still pending** |
| Pathfinding performance (X64.pf) and engine scalability audit (X64.sc): exact A* line-phase optimisations (-17 % path-search CPU), unit-multiplier ladder, `getClosestObjects` characterisation, graphical endurance, multithreading decision (not justified), upstream pathfinding triage | measured and documented ([PATHFINDING_PERFORMANCE_AUDIT.md](PATHFINDING_PERFORMANCE_AUDIT.md), [ENGINE_SCALABILITY_AUDIT.md](ENGINE_SCALABILITY_AUDIT.md)); digests and golden gates unchanged |
| AI query / object update audit, seeded graphical benchmark (fingerprint-verified against headless), exact staged HUNT scan, bounded disk use (X64.aq, X64.dh) | measured and documented ([AI_QUERY_AUDIT.md](AI_QUERY_AUDIT.md), [DISK_RETENTION.md](DISK_RETENTION.md)); digests and golden gates unchanged |
| Final performance audit and baseline (X64.performance-final): failing-search root cause (shared cell-info pool, not congestion), HUNT chain, Present-stall probe, first-use hitch attribution, reference baseline for both titles, phase exit decision | measured and documented ([PERFORMANCE_FINAL_AUDIT.md](PERFORMANCE_FINAL_AUDIT.md)); diagnostics only, digests and gates unchanged |
| Modern UI product-completion audit: 4:3 / 16:10 added to the validated aspect matrix, Options Network-block and 4:3 footer fixes, ranked remaining UI work, product roadmap | measured and documented ([MODERN_UI_PRODUCT_COMPLETION.md](MODERN_UI_PRODUCT_COMPLETION.md)); UI-only, digests and golden gates unchanged |
| Game Speed match rule (Skirmish, both titles): 0.75 / 1.00 / 1.25 / 1.50x as pacing of the existing logic-rate seam, one GameInfo value (`GS=` in the options string / replay header, save version 7), runtime Speed Up / Down, Skirmish.ini persistence; 2.0x rejected | implemented and automatically validated (unit tests, source contract, headless digests identical at every level, graphical pace and fingerprints at 60 / 120 / 240 FPS, heavy Zero Hour 99 % of the target rate, recorded replays, save / load); **native human review pending; multiplayer not implemented / not validated** ([GAME_SPEED.md](GAME_SPEED.md)) |
| Match Rules UX: Game Speed as the fourth generated rule, start-validation / rule-help status line, de-emphasised career block; ultrawide Match Setup composition, 4 % scale-fit steps, 11 pt text floor | implemented and automatically validated (native audits 4:3 - 32:9, Classic smoke); owner review pending |

## In progress
* Modern Experience UI phase: UI.2 (shell) and UI.3 (design system + Options, Match Setup, Load / Replay, dialogs, Credits) are implemented and **waiting for the owner's native visual review**. If the look is disliked the next step is a visual revision, not HUD work.
* Native human verification of X64.52B (edge scrolling, camera smoothness at 120 FPS) - the real-input camera gate on the final clean executables is still to be run on an unlocked desktop.

## Planned
UI.2 shell / main menu (done) -> UI.3 frontend system, Options, Match Setup, browsers (done) -> owner review -> UI.4 remaining retail screens (LAN lobby, Online, Challenge / Map select) -> UI.5 Modern HUD -> UI.6 polish/acceptance; parallel UI.V (movie playback via FFmpeg). After the UI phase: endurance work against the [performance baseline](PERFORMANCE_FINAL_AUDIT.md) (the HUNT failing-search chain and the first-use render hitch are Beta targets); later Enhanced AI phase 2 (combat coordination) and the long-term extended-game ideas listed in the README roadmap.

## Known issues and limitations
Classification: **C** = correctness defect (belongs to a regression gate), **F** = feature/quality gap, **U** = unverified observation.

| # | Item | Class | Notes / next step |
|---|---|---|---|
| 1 | Movies (logos, intro, briefings, Challenge video backgrounds) are skipped on AMD64 | F | Bink SDK is 32-bit; plan = FFmpeg player ([architecture section 7](MODERN_UI_ARCHITECTURE.md)); skip is logged and graceful |
| 2 | Enemy structures sometimes show the local player's colour briefly before the correct enemy colour | **C (suspected)** | Native-test observation. Lead (unverified): per-object team colour resolution in `NativeWorldObjects.cpp` (`ObjectTexture` keyed by `hexColor`, `teamColored`) and fogged/ghost snapshots using the local colour. Needs a reproduction harness (headless frame capture of a newly revealed enemy structure). Not a UI-phase task; keep as a blocker candidate for the next renderer-correctness pass. |
| 3 | Allied (Enhanced) AI attacked China defensive towers in one observed case | U | Cannot be classified without a reproduction: first check whether the same occurs with Original AI and the same seed (then it is retail behaviour), then whether the target was a real ally (team/alliance lookup). Enhanced AI development is paused; record only. |
| 4 | Zero Hour Enhanced GLA base development weak; allied USA Enhanced AI built many more production buildings; Generals Enhanced AI better but incomplete | F | AI backlog; see X64_52 document; paused |
| 5 | Command/build UI takes about 1 - 2 s to appear at the start of a match (not recurring) | F | Investigated (Game Speed milestone): no deliberate delay exists in the control bar code (decorative scheme animations, a 500 ms General Powers shortcut slide, a 0.5 s observer refresh); the only measured cost is the one-time ~125 ms first-use texture / buffer creation hitch (PERFORMANCE_FINAL_AUDIT). Not reproduced as a 1 - 2 s wait; prefetch at match load stays a Beta candidate; nothing changed |
| 6 | Modern USA HUD still shows a legacy-looking "Command Menu" button | F | Control not yet identified (candidates: the four 30x22 procedural plates); UI.5 |
| 7 | Possible camera judder at ~120 FPS reported on X64.52A | U | Not reproduced with real input at 60/120/240 FPS; camera integration unchanged since X64.49; render/present pacing item open. See [X64_52B](X64_52B_CAMERA_INPUT_REGRESSION.md). |
| 8 | Multiplayer / LAN and long-session endurance not fully validated; the LAN lobby and Online screens are still the retail screens (no Modern match-option controls) | F | Planned validation; UI.4 |
| 9 | Native real-input gates need an unlocked desktop | tooling | Exit code 3 = inconclusive |
| 10 | Campaign progression, cross-version replays, some renderer/shadow fidelity cases | F | README "Known Incomplete Areas" |
| 11 | Zero Hour HUNT state: ~1 400 exhaustive failing path searches per match (11 ms each, a quarter of the 1x simulation time; ticks > 33.3 ms: 9-26 of 23 400) | F (Beta target) | Existing engine pool budget, not congestion; no exact fix, needs a behaviour decision ([PERFORMANCE_FINAL_AUDIT.md](PERFORMANCE_FINAL_AUDIT.md)) |
| 12 | Rare 100-2 200 ms process stalls (previously seen as 450-510 ms inside Present) and one ~130 ms first-use render hitch per match | U / F | Stalls are off-CPU and not Present-specific here; need an elevated ETW trace on a native session; probe `GENERALS_PRESENT_STALL_LOG` |

## Disk and artifact hygiene
`build/` is regenerable; retention: the newest two validated distributions (`New-MKRaudsteinDistribution.ps1` cleans automatically), native candidates by `Cleanup-NativeCandidates.ps1` (dry run, reviewed plan hash),
Zero Hour stages by `Cleanup-ZHStages.ps1`. The 2026-10-25 cleanup removed 7 superseded candidates (15.1 GB; free space 23 GB -> 38 GB).
