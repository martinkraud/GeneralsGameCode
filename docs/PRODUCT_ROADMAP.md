# MKRaudstein Modern Engine - product roadmap

Release path: **Current → Developer Preview → Public Alpha → Beta → Release Candidate → 1.0.**
This page tracks product state only and mirrors the repository; engineering history lives in [modernization/PROJECT_STATUS.md](modernization/PROJECT_STATUS.md). Last reviewed: 2026-11-07 (after the Developer Preview Distribution milestone).

State vocabulary: **Done** (implemented and validated to the level stated) · **Needs validation** (implemented, a named validation is still open) · **Planned** (agreed, not started) · **Beta** (deliberately targeted at Beta) · **Post-1.0** (explicitly outside 1.0).

## Feature state

| Area | State | Evidence / what is open |
|---|---|---|
| Native AMD64 engine | Done | Generals and Zero Hour run as 64-bit executables; golden AI gates, replay and determinism digests unchanged since X64.45 |
| Direct3D 11 renderer | Done | world, UI, shadows, water, particles; flip-model swap chain. Rare off-CPU stalls are classified external ([PERFORMANCE_FINAL_AUDIT](modernization/PERFORMANCE_FINAL_AUDIT.md)) |
| High-refresh presentation | Done | authoritative 30 TPS simulation with independent, interpolated presentation; render FPS presets. X64.52B camera/edge-scroll: awaiting human re-test at 120 FPS |
| Generals | Done (single-player Skirmish) / Needs validation (multiplayer) | native desktop and laptop play; campaign progression incomplete (README) |
| Zero Hour | Done (single-player Skirmish) / Needs validation (multiplayer) | human-tested on desktop and laptop |
| Classic | Done | `-classic` keeps the retail frontend and settings; Classic isolation re-checked every UI milestone |
| Modern UI: shell, Options, Match Setup (Skirmish), Load / Replay, dialogs, Credits | Done (**owner accepted**: visual direction 2026-11-06, Settings / UX review 2026-11-07) | implemented, automatically validated and reviewed natively by the owner |
| Modern UI: LAN lobby / LAN game setup, Challenge general select, Map select, Online | Planned | still the retail screens (see [MODERN_UI_PRODUCT_COMPLETION](modernization/MODERN_UI_PRODUCT_COMPLETION.md)) |
| Modern UI: Settings / UX completion | Done (**owner accepted 2026-11-07**; the one remaining issue, the unscrollable Controls / Keyboard list, is fixed in the Developer Preview Distribution milestone and covered by a native audit) | help strip for every exposed setting, generated Network rows, read-only Keyboard reference, Accept / Cancel semantics made honest, Zero Hour controls restored ([MODERN_SETTINGS_ARCHITECTURE](modernization/MODERN_SETTINGS_ARCHITECTURE.md)); key **rebinding** does not exist in the retail engine and is a post-Preview candidate |
| In-match HUD | Done for Generals (Enhanced HUD, USA / China / GLA); **Zero Hour: implemented post-DP1 (14-slot grid, all sides), awaiting owner acceptance** ([ZERO_HOUR_MODERN_HUD](modernization/ZERO_HOUR_MODERN_HUD.md)) | native-accepted and frozen ([ENHANCED_HUD_FRAMEWORK_V1](modernization/ENHANCED_HUD_FRAMEWORK_V1.md)); pre-Preview gaps: known issue 6 (legacy "Command Menu" button) and first-use control-bar delay (known issue 5) |
| Enhanced AI | Done (E1/E2: foundation, dynamic base and defence) | opt-in per match; Original untouched; combat coordination (X64.53) paused by decision; known weaknesses in PROJECT_STATUS |
| Match Options | Done for Skirmish (**owner accepted 2026-11-07**) | Starting Cash, AI Behaviour, Superweapons, Game Speed as generated rules; contextual start-validation / rule-help status line; hidden in Classic. Not available in LAN setup (retail screen) |
| Game Speed | Done for Skirmish (**owner accepted 2026-11-07**) / Planned for multiplayer | 0.75 / 1.00 / 1.25 / 1.50x, pacing only, replay + save/load + Speed Up/Down ([GAME_SPEED](modernization/GAME_SPEED.md)); 2.0x rejected (heavy Zero Hour cannot hold the 16.7 ms budget); multiplayer: not implemented, not validated |
| LAN | Needs validation | lobby/game code unchanged retail; **real two-PC multiplayer validation of `2071ad765` remains pending** |
| Multiplayer version compatibility | Planned | no engine version / protocol check exists (Zero Hour accepts any two Modern builds); required before Public Alpha |
| Distribution: Developer Preview package (both titles), installer, uninstaller, separate symbols, SHA-256 | Done (developer-built, automatically validated) / Needs validation (real clean machine) | `MKRaudstein-<Title>-DeveloperPreview-<commit>.zip` with `Install.cmd` / `Uninstall.cmd`, backup + rollback, no EA asset (the Generals sidebar extension files are generated on the player PC), validated in three layers on the developer PC ([DISTRIBUTION](modernization/DISTRIBUTION.md)); unsigned; no auto-updater by decision |
| Player-package install / play / uninstall (owner manual test, 2026-11-08) | **Owner accepted** (Generals and Zero Hour; Zero Hour uninstall also tested) | the owner's real Steam installation; a manual test, not the automated Layer C |
| Modern Experience title themes | Done | Generals warm gold, Zero Hour cold blue; one shared design system; Classic untouched |
| Clean-install validation | Needs validation | package isolation validation is automated ("retail copy + installer == validated stage", real launches of the installed build, Classic and Modern); **a real clean-machine run of the package is pending** |
| Rollback / uninstall | Done (tested on isolated copies) / Needs validation (real machine) | hash-verified backups, rollback on a failed install, uninstaller for complete and interrupted installs, Steam *Verify integrity* as the documented fallback |
| Hardware QA | Planned | two development PCs only; wider GPU / driver matrix is an Alpha-to-Beta activity |
| Documentation | Done | README front page, [player guide](DEVELOPER_PREVIEW.md), `README-FIRST.txt` in the package, [distribution architecture](modernization/DISTRIBUTION.md), docs/ current |
| Repository CI | Added, not yet verified on GitHub (informational) | `Repository checks` workflow runs the repository-only Python suites; no game files, never builds or starts the game; native builds, determinism gates and UI audits stay on the developer machines |
| Release QA | Planned | RC activity |
| Movies (Bink) | Beta | skipped on AMD64 (graceful); FFmpeg player designed |
| Long-session endurance, HUNT failing-search chain, first-use hitch | Beta | measured; not blockers |

## Gates by release stage

### Developer Preview (a technically interested owner of a legitimate install can use it without developer tools)
1. ~~Modern UI: owner native review of the Options pages (help strip, Network, Keyboard)~~ **owner accepted 2026-11-07**; the Controls / Keyboard scrolling defect found in that review is fixed.
2. ~~Modern Match Options + Game Speed (Skirmish)~~ **owner accepted 2026-11-07**; ~~Settings / UX completion~~ implemented and automatically validated.
3. Decision: does the Preview ship with the retail LAN screens (current plan) or wait for the Modern LAN setup? Either way multiplayer Game Speed stays out of the Preview.
4. **Real two-PC multiplayer validation of the current candidate** (still pending for `2071ad765`).
5. ~~Package built by script and validated~~ done (Developer Preview package, installer, uninstaller, three validation layers); **installed on a real clean machine without Visual Studio, CMake, Python or the repository: pending (manual)**.
6. ~~Player-facing install / uninstall / known-limitations note~~ done ([DEVELOPER_PREVIEW.md](DEVELOPER_PREVIEW.md), `README-FIRST.txt`).

### Public Alpha
Multiplayer version/build compatibility check, wider hardware reports, crash-report path, campaign progression decision.

### Beta and feature freeze
External compatibility and multiplayer QA, Beta targets above (HUNT chain, first-use hitch, movies), final polish of retail-screen leftovers, localisation pass on Modern strings.

### Release Candidate → 1.0
Release QA pass on both titles, Classic regression sweep, documentation freeze, signed-off known-issues list.

## Post-1.0 (not started, not required for 1.0)
Vulkan · native Linux · broad simulation multithreading · a new pathfinding architecture · 60 TPS simulation conversion · total graphics remake · new factions, units or campaigns · internet matchmaking backend · workshop / mod platform. A release blocker found during QA may pull a specific item forward; nothing here starts otherwise.
