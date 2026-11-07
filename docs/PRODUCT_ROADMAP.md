# MKRaudstein Modern Engine - product roadmap

Release path: **Current → Developer Preview → Public Alpha → Beta → Release Candidate → 1.0.**
This page tracks product state only and mirrors the repository; engineering history lives in [modernization/PROJECT_STATUS.md](modernization/PROJECT_STATUS.md). Last reviewed: 2026-11-07 (after the Modern Match Options + Game Speed milestone).

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
| Modern UI: shell, Options, Match Setup (Skirmish), Load / Replay, dialogs, Credits | Needs validation (visual direction approved by the owner 2026-11-06) | implemented and automatically validated; ultrawide Match Setup scale and minimum text readability polished |
| Modern UI: LAN lobby / LAN game setup, Challenge general select, Map select, Online | Planned | still the retail screens (see [MODERN_UI_PRODUCT_COMPLETION](modernization/MODERN_UI_PRODUCT_COMPLETION.md)) |
| Modern UI: Settings / UX completion (per-setting help, FPS limit and render scale rows audit) | Planned | listed in [SETTINGS_COMPLETION_AUDIT](modernization/SETTINGS_COMPLETION_AUDIT.md) |
| In-match HUD | Done (Enhanced HUD, USA / China / GLA) | native-accepted and frozen ([ENHANCED_HUD_FRAMEWORK_V1](modernization/ENHANCED_HUD_FRAMEWORK_V1.md)); pre-Preview gaps: known issue 6 (legacy "Command Menu" button) and first-use control-bar delay (known issue 5) |
| Enhanced AI | Done (E1/E2: foundation, dynamic base and defence) | opt-in per match; Original untouched; combat coordination (X64.53) paused by decision; known weaknesses in PROJECT_STATUS |
| Match Options | Done for Skirmish (native review pending) | Starting Cash, AI Behaviour, Superweapons, Game Speed as generated rules; contextual start-validation / rule-help status line; hidden in Classic. Not available in LAN setup (retail screen) |
| Game Speed | Done for Skirmish (native review pending) / Planned for multiplayer | 0.75 / 1.00 / 1.25 / 1.50x, pacing only, replay + save/load + Speed Up/Down ([GAME_SPEED](modernization/GAME_SPEED.md)); 2.0x rejected (heavy Zero Hour cannot hold the 16.7 ms budget); multiplayer: not implemented, not validated |
| LAN | Needs validation | lobby/game code unchanged retail; **real two-PC multiplayer validation of `2071ad765` remains pending** |
| Multiplayer version compatibility | Planned | no engine version / protocol check exists (Zero Hour accepts any two Modern builds); required before Public Alpha |
| Distribution (overlay ZIP + separate symbols) | Done (developer-built) / Needs validation | package scripts and validator exist (26/26 checks); no installer; needs a non-developer machine run |
| Clean-install validation | Needs validation | "clean install + ZIP == stage" is proven by script; a human clean-machine pass of the current package is open |
| Rollback / uninstall | Planned | overlay only adds files; documented manual removal is not yet a supported, tested procedure |
| Hardware QA | Planned | two development PCs only; wider GPU / driver matrix is an Alpha-to-Beta activity |
| Documentation | Done for developers / Planned for players | README and docs/ current; a player-facing quick start and troubleshooting page is not written |
| Release QA | Planned | RC activity |
| Movies (Bink) | Beta | skipped on AMD64 (graceful); FFmpeg player designed |
| Long-session endurance, HUNT failing-search chain, first-use hitch | Beta | measured; not blockers |

## Gates by release stage

### Developer Preview (a technically interested owner of a legitimate install can use it without developer tools)
1. Modern UI: owner visual acceptance, remaining retail-screen decisions (UI.4) recorded.
2. ~~Modern Match Options + Game Speed (Skirmish)~~ implemented; native human review of the selector and the feel at 1.25x / 1.5x.
3. Settings / UX completion for the options the Preview exposes.
4. **Real two-PC multiplayer validation of the current candidate** (still pending for `2071ad765`).
5. Package built by script, validated, installed on a clean machine without Visual Studio, CMake, Python or the repository.
6. Player-facing install / uninstall / known-limitations note.

### Public Alpha
Multiplayer version/build compatibility check, rollback/uninstall procedure, wider hardware reports, crash-report path, campaign progression decision.

### Beta and feature freeze
External compatibility and multiplayer QA, Beta targets above (HUNT chain, first-use hitch, movies), final polish of retail-screen leftovers, localisation pass on Modern strings.

### Release Candidate → 1.0
Release QA pass on both titles, Classic regression sweep, documentation freeze, signed-off known-issues list.

## Post-1.0 (not started, not required for 1.0)
Vulkan · native Linux · broad simulation multithreading · a new pathfinding architecture · 60 TPS simulation conversion · total graphics remake · new factions, units or campaigns · internet matchmaking backend · workshop / mod platform. A release blocker found during QA may pull a specific item forward; nothing here starts otherwise.
