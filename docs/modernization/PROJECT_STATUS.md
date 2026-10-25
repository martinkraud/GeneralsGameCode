# MKRaudstein Modern Engine - project status

One-page, evidence-linked status. Updated at every milestone closeout together with `README.md` and `AGENT_HANDOFF.md` (rule since UI.1: a milestone that changes what is true updates this file in the same milestone).
Last updated: 2026-10-25 (UI.3). Detail per milestone: [FIRST_PLAYABLE_ROADMAP.md](FIRST_PLAYABLE_ROADMAP.md), [AGENT_HANDOFF.md](AGENT_HANDOFF.md).

## Current direction
Performance optimization and Enhanced AI development are **paused** (gates stay in force). Active phase: **Modern Experience UI** ([MODERN_UI_ARCHITECTURE.md](MODERN_UI_ARCHITECTURE.md)).
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

## In progress
* Modern Experience UI phase: UI.2 (shell) and UI.3 (design system + Options, Match Setup, Load / Replay, dialogs, Credits) are implemented and **waiting for the owner's native visual review**. If the look is disliked the next step is a visual revision, not HUD work.
* Native human verification of X64.52B (edge scrolling, camera smoothness at 120 FPS) - the real-input camera gate on the final clean executables is still to be run on an unlocked desktop.

## Planned
UI.2 shell / main menu (done) -> UI.3 frontend system, Options, Match Setup, browsers (done) -> owner review -> UI.4 remaining retail screens (LAN lobby, Online, Challenge / Map select) -> UI.5 Modern HUD -> UI.6 polish/acceptance; parallel UI.V (movie playback via FFmpeg). After the UI phase: a new performance
baseline, then deeper performance / endurance work; later Enhanced AI phase 2 (combat coordination) and the long-term extended-game ideas listed in the README roadmap.

## Known issues and limitations
Classification: **C** = correctness defect (belongs to a regression gate), **F** = feature/quality gap, **U** = unverified observation.

| # | Item | Class | Notes / next step |
|---|---|---|---|
| 1 | Movies (logos, intro, briefings, Challenge video backgrounds) are skipped on AMD64 | F | Bink SDK is 32-bit; plan = FFmpeg player ([architecture section 7](MODERN_UI_ARCHITECTURE.md)); skip is logged and graceful |
| 2 | Enemy structures sometimes show the local player's colour briefly before the correct enemy colour | **C (suspected)** | Native-test observation. Lead (unverified): per-object team colour resolution in `NativeWorldObjects.cpp` (`ObjectTexture` keyed by `hexColor`, `teamColored`) and fogged/ghost snapshots using the local colour. Needs a reproduction harness (headless frame capture of a newly revealed enemy structure). Not a UI-phase task; keep as a blocker candidate for the next renderer-correctness pass. |
| 3 | Allied (Enhanced) AI attacked China defensive towers in one observed case | U | Cannot be classified without a reproduction: first check whether the same occurs with Original AI and the same seed (then it is retail behaviour), then whether the target was a real ally (team/alliance lookup). Enhanced AI development is paused; record only. |
| 4 | Zero Hour Enhanced GLA base development weak; allied USA Enhanced AI built many more production buildings; Generals Enhanced AI better but incomplete | F | AI backlog; see X64_52 document; paused |
| 5 | Command/build UI takes about 1 - 2 s to appear at the start of a match (not recurring) | F | Likely first-use asset loads of the control bar scheme / command images; candidate for prefetch at match load in UI.5/6; measure first |
| 6 | Modern USA HUD still shows a legacy-looking "Command Menu" button | F | Control not yet identified (candidates: the four 30x22 procedural plates); UI.5 |
| 7 | Possible camera judder at ~120 FPS reported on X64.52A | U | Not reproduced with real input at 60/120/240 FPS; camera integration unchanged since X64.49; render/present pacing item open. See [X64_52B](X64_52B_CAMERA_INPUT_REGRESSION.md). |
| 8 | Multiplayer / LAN and long-session endurance not fully validated; the LAN lobby and Online screens are still the retail screens (no Modern match-option controls) | F | Planned validation; UI.4 |
| 9 | Native real-input gates need an unlocked desktop | tooling | Exit code 3 = inconclusive |
| 10 | Campaign progression, cross-version replays, some renderer/shadow fidelity cases | F | README "Known Incomplete Areas" |

## Disk and artifact hygiene
`build/` is regenerable; retention: the newest two validated distributions (`New-MKRaudsteinDistribution.ps1` cleans automatically), native candidates by `Cleanup-NativeCandidates.ps1` (dry run, reviewed plan hash),
Zero Hour stages by `Cleanup-ZHStages.ps1`. The 2026-10-25 cleanup removed 7 superseded candidates (15.1 GB; free space 23 GB -> 38 GB).
