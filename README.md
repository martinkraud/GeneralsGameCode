# GeneralsGameCode — MKRaudstein Modern Engine

> **Experimental modernization fork of TheSuperHackers/GeneralsGameCode.**
>
> A native 64-bit and DirectX 11 modernization of  
> **Command & Conquer: Generals** and **Command & Conquer: Generals – Zero Hour**.
>
> The project focuses on preserving the original games while modernizing the
> engine, rendering, display support, performance and optional user experience.
>
> **Status:** Active development. Generals and Zero Hour are both playable on the
> modernized native AMD64/D3D11 engine and have been stable on the project's tested
> desktop and laptop systems. This project is not yet intended as a finished
> end-user release. Current detail: [docs/modernization/PROJECT_STATUS.md](docs/modernization/PROJECT_STATUS.md).

---

## Project Overview

This repository builds on the community-driven
[TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode)
project.

The goal of this fork is not to turn Generals into a different game.

The goal is to make the original game work correctly on a modernized engine
first, while creating a foundation for optional modern presentation and future
expansion.

The project currently focuses on:

- Native AMD64 / 64-bit Windows support
- Direct3D 11 rendering
- Preservation of original Generals gameplay
- Authoritative original-style 30 TPS simulation
- Rendering independent of simulation rate
- Smooth high-refresh presentation
- Modern resolutions and ultrawide displays
- UI scaling and modern display handling
- Improved frame pacing and performance
- Modernized frontend and settings
- Optional Enhanced gameplay interface
- Improved stability and maintainability
- Continued compatibility work for Zero Hour

The modernization deliberately separates:

1. **Original game preservation**
2. **Engine modernization**
3. **Optional Modern Experience features**
4. **Future gameplay/content expansion**

---

## Documentation

| For | Start here |
|---|---|
| What changed, by area | [CHANGELOG.md](CHANGELOG.md) |
| Performance methodology and results | [docs/PERFORMANCE.md](docs/PERFORMANCE.md) |
| Engineering findings (what was wrong, where, how it was verified) | [docs/findings/README.md](docs/findings/README.md) - [performance](docs/findings/PERFORMANCE_FINDINGS.md), [AMD64 porting](docs/findings/AMD64_PORTING_FINDINGS.md), [replay / determinism](docs/findings/REPLAY_DETERMINISM_FINDINGS.md) |
| Current status: completed / in progress / planned / known issues | [docs/modernization/PROJECT_STATUS.md](docs/modernization/PROJECT_STATUS.md) |
| Modern Experience UI: archaeology of the retail UI and the target architecture | [archaeology](docs/modernization/MODERN_UI_ARCHAEOLOGY.md), [architecture and phases](docs/modernization/MODERN_UI_ARCHITECTURE.md) |
| Roadmap, milestone records and handoff | [docs/modernization/](docs/modernization/) - [roadmap](docs/modernization/FIRST_PLAYABLE_ROADMAP.md), [handoff](docs/modernization/AGENT_HANDOFF.md) |
| Original Game Speed facts and the (unimplemented) Game Speed design | [audit](docs/modernization/RETAIL_GAME_SPEED_AUDIT.md), [design](docs/modernization/GAME_SPEED_DESIGN.md) |
| Building | [Building](#building) below |
| Known limitations | [Known Incomplete Areas](#known-incomplete-areas) below |

---

# Current Status

## Generals — Native AMD64 / DirectX 11

**Command & Conquer: Generals is now playable through the modernized native
AMD64 and Direct3D 11 engine path.**

The native path has progressed far beyond the initial rendering prototype.

Currently demonstrated functionality includes:

- Native 64-bit Windows executable
- Normal application startup
- Original game asset loading
- Original INI and WND parsing
- Native Direct3D 11 presentation
- Original frontend and Main Menu
- Solo Play / Skirmish setup
- Map loading
- Live GameLogic
- AI players
- Unit selection and commands
- Buildings and production
- Ground units
- Aircraft
- Projectiles
- Terrain
- Scenery
- Trees and props
- Roads and bridges
- Water and shorelines
- Shroud
- Shadows
- Particles and effects
- Radar
- Command bar
- General powers
- Audio
- Edge scrolling
- Save / Load
- Skirmish victory and return to frontend
- Repeated match startup during native testing

The modern renderer is therefore no longer limited to UI or terrain
experimentation.

A normal Generals gameplay path can run through:

`Startup → Main Menu → Skirmish → Map Load → Gameplay → Victory / Exit`

on the native AMD64/D3D11 engine.

---

# Original Experience

Preserving the original game is a core requirement.

The modernization is not intended to silently alter game balance, simulation
rules or the identity of Generals.

The original gameplay simulation remains authoritative.

Examples of preservation priorities include:

- Original unit behaviour
- Original economy
- Original combat rules
- Original build times
- Original damage values
- Original faction behaviour
- Original General powers
- Original campaign behaviour
- Original simulation timing

Unexpected or unusual retail values are not automatically treated as bugs.

Where optional modern features are introduced, the original behaviour remains
the reference and compatibility target.

---

# Modern Experience

Alongside preservation, the project provides an optional **Modern Experience**
for improvements that do not need to redefine the underlying game.

Current Modern Experience work includes:

- Modern display handling
- Widescreen support
- Ultrawide support
- UI scaling
- Modernized Options interface
- Improved frontend layout
- Skirmish match options: Starting Cash, AI Behaviour (Original / Enhanced) and a Superweapons limit
- Smooth unit presentation
- Optional Enhanced gameplay interface (USA, China, GLA)

The guiding design principle is:

> **Modernize Generals, not replace its identity.**

### Where the Modern UI is going

The retail interface is built on 800x600-era windows. Modern Experience keeps
that content and atmosphere but replaces the fixed layout with a responsive,
extensible one (reusable layout components, a real Match Setup screen, a
Modern HUD) so new options never again have to be squeezed into unused pixels.
Original / `-classic` remains the preservation path. The investigation and the
phased plan are in
[MODERN_UI_ARCHAEOLOGY.md](docs/modernization/MODERN_UI_ARCHAEOLOGY.md) and
[MODERN_UI_ARCHITECTURE.md](docs/modernization/MODERN_UI_ARCHITECTURE.md).

The project asks what Generals might look and feel like if its original design
team were adapting it for modern hardware and displays today.

---

# Modern Display Support

The modernized engine supports display geometries far beyond the original
4:3-era assumptions.

Development and layout validation includes resolutions and aspect ratios such
as:

- 800×600
- 1920×1080
- 1920×1200
- 2560×1440
- 3440×1440
- 5120×1440

This includes support for:

- 16:9
- 16:10
- 21:9
- 32:9 ultrawide
- unusual intermediate display sizes
- scalable UI geometry

Modern UI elements are designed to remain bounded rather than stretching
proportionally across extremely wide displays.

Additional display work remains planned, including further fullscreen,
refresh-rate and runtime display handling improvements.

---

# Simulation and Smooth Presentation

Generals gameplay simulation remains authoritative at its intended simulation
rate.

Rendering is treated separately.

This allows modern displays to render substantially more frequently without
making the game simulation run faster.

The engine now includes an optional **Smooth** presentation mode.

Current interpolation work includes:

- Ground-unit movement
- Aircraft movement
- Projectile movement
- Heading interpolation
- Turret yaw
- Barrel pitch

The simulation itself remains authoritative.

Interpolation is presentation-only and contains safeguards for events such as:

- Large relocations
- Teleports
- Load transitions
- Invalid history
- Simulation gaps
- Epoch changes

The objective is smooth visual motion on modern high-refresh displays without
changing the underlying gameplay.

---

# Performance

The performance work is **not** about maximising render FPS. The original game simulates **30 logic ticks per second** at Normal speed (a 33.333 ms budget per tick, proven for both
Generals and Zero Hour in the [Game Speed audit](docs/modernization/RETAIL_GAME_SPEED_AUDIT.md)); rendering is paced separately in this fork. The CPU work targets a **stable simulation in
large battles**: the slowest ticks (p99 / worst), ticks that exceed the 33.333 ms budget, derived deadline misses and accumulated backlog - not the average, which is already small.

All optimizations are exact (bit-identical simulation): each keeps the original algorithm selectable, was checked against it, and left the deterministic benchmark fingerprint unchanged.

**Benchmark:** 72 000 authoritative ticks (40 game minutes at 30 TPS), Twilight Flame, one idle human + 7 Brutal AI, headless and deterministic, one development machine
(Intel Core Ultra 5 125U). Same executable, original code paths vs current defaults, alternating runs, 4 each:

| whole simulation tick | original paths | current |
|---|---|---|
| p99 | 22.8 ms | 5.9 ms |
| p99.9 | 49.7 ms | 10.4 ms |
| ticks over the 33.3 ms budget (of 72 000) | 204 | 4.5 |
| derived missed deadlines | 414 | 8 |
| derived maximum backlog | 3.0 s | 52 ms |
| median tick | 0.71 ms | 0.71 ms |

The median did not change: the work removed the rare expensive ticks (pathfinding zone recalculation, the A* open list and line walk, and a synchronous diagnostic disk flush on the logic thread), not the
typical one. The deadline and backlog figures are derived from measured tick durations, not from a paced run. These numbers describe this workload on this machine and should not be generalised to other
PCs, maps or match types. Remaining known tail: rare exhaustive failing path searches (~80 ms).

Methodology, the per-milestone A/B tables, the current profile and the limits of these results: **[docs/PERFORMANCE.md](docs/PERFORMANCE.md)**. Reusable root-cause write-ups for other engine developers:
**[docs/findings/](docs/findings/README.md)**.

---

# Enhanced Gameplay UI

An optional **Enhanced** gameplay interface is available for the three base
Generals factions: USA, China and GLA.

This is separate from the Original presentation.

The architecture follows:

- **Original** — preservation of the classic Generals interface
- **Enhanced** — modernized presentation using the same gameplay systems

The Enhanced interface does not duplicate command or simulation logic.

Instead, it presents existing gameplay functionality through a modernized
layout shared by every faction:

- A centered bottom HUD island
- Open battlefield space on both sides
- Modern responsive geometry
- Improved radar presentation
- Improved unit information
- Clear command/build grouping
- Dedicated cash and power presentation
- General rank and XP presentation
- A General Powers console built from the game's real power data
- Ultrawide-safe layout
- UI-scale support

Each faction has its own construction language (USA precision-machined,
China heavy industrial, GLA improvised field equipment) on top of the shared
functional structure. Further faction-specific visual polish is planned for a
later pass.

---

# Rendering Strategy

This project does **not** implement a generic Direct3D 8 emulation device.

Instead, original rendering semantics are migrated into backend-neutral or
purpose-built Direct3D 11 systems.

The fidelity target is:

> Preserve the original visual intent, assets and rendering behaviour while
> implementing equivalent modern rendering systems.

The project does not require bit-identical Direct3D 8 raster output.

Visual equivalence, gameplay readability and preservation of the original
artistic intent are the priorities.

The original renderer and original game remain important references during
modernization.

---

# Current Feature Status

| Area | Status |
| --- | --- |
| Native AMD64 executable | ✅ Working |
| Normal Generals startup | ✅ Working |
| Original asset loading | ✅ Working |
| Direct3D 11 presentation | ✅ Working |
| Direct3D 11 world rendering | ✅ Working |
| Original Main Menu | ✅ Working |
| Mouse / keyboard input | ✅ Working |
| Skirmish setup | ✅ Working |
| Map loading | ✅ Working |
| Live GameLogic | ✅ Working |
| AI gameplay | ✅ Working |
| Terrain | ✅ Working |
| Units / buildings | ✅ Working |
| Aircraft / projectiles | ✅ Working |
| Trees / scenery | ✅ Working |
| Roads / bridges | ✅ Working |
| Water / shorelines | ✅ Working |
| Fog / shroud | ✅ Working |
| Shadows | ✅ Working |
| Particles / effects | ✅ Working |
| Radar / command interface | ✅ Working |
| Audio | ✅ Working |
| Save / Load | ✅ Working |
| Modern resolutions | ✅ Working |
| Ultrawide support | ✅ Working |
| UI scaling foundation | ✅ Working |
| Smooth presentation | ✅ Working, optional |
| Performance instrumentation | ✅ Working |
| Simulation CPU optimization (exact, serial) | ✅ Delivered, see [Performance](docs/PERFORMANCE.md) |
| Enhanced HUD (USA, China, GLA) | ✅ Working, optional |
| Movies / cutscenes | 🚧 Incomplete |
| Replay record / re-simulate (Generals Skirmish) | ✅ Verified for a recorded match; wider validation needed |
| Multiplayer / determinism | ⏳ Further validation needed |
| Zero Hour native gameplay | ✅ Working, human-tested on desktop and laptop; broader (multiplayer, campaign, endurance) validation continues |
| Skirmish match rules (Starting Cash, AI Behaviour, Superweapons) | ✅ Working, optional (Original/defaults preserved; `-classic` hides them) |
| Enhanced AI (opt-in per match) | 🚧 Experimental; development paused while the Modern UI is built, Original AI is the default and is gate-protected |
| Overlay player packages (Generals + Zero Hour) | ✅ Working (ZIP over the legitimate installation; no game assets are redistributed) |
| Modern Experience UI (responsive frontend, Match Setup, Modern HUD) | 🚧 Architecture phase (see [Modern UI](docs/modernization/MODERN_UI_ARCHITECTURE.md)) |

---

# Generals and Zero Hour

The modernized engine is shared by both games.

## Command & Conquer: Generals

Generals is currently the primary native validation target.

Most major AMD64, renderer and Modern Experience milestones are therefore
validated in Generals first.

## Command & Conquer: Generals – Zero Hour

Zero Hour now runs on the same native AMD64/D3D11 engine and has been played
through Skirmish on the project's tested desktop and laptop systems, with the
Enhanced HUD for all three base factions.

Zero Hour keeps its own validation track: its content, shell, Challenge mode
and generals' powers differ from Generals. Multiplayer, campaign progression,
movies and long-session endurance are not yet fully validated. See
[PROJECT_STATUS.md](docs/modernization/PROJECT_STATUS.md).

---

# Roadmap

## Phase 1 — Native Engine Foundation

- [x] Native AMD64 builds
- [x] Win64 startup
- [x] Direct3D 11 backend foundation
- [x] Backend-neutral rendering groundwork
- [x] Simulation/render timing separation

## Phase 2 — Original UI and Frontend

- [x] Direct3D 11 presentation
- [x] Render2D
- [x] Original UI textures
- [x] Original fonts
- [x] Original Main Menu
- [x] Mouse and keyboard input
- [x] Interactive Skirmish setup

## Phase 3 — Native Game Integration

- [x] Map loading
- [x] Live GameLogic
- [x] AI simulation
- [x] Selection and commands
- [x] Production
- [x] General powers
- [x] Save / Load
- [x] Normal startup path
- [x] Normal gameplay lifecycle

## Phase 4 — Direct3D 11 World Rendering

- [x] Terrain
- [x] Static meshes
- [x] Units and buildings
- [x] Aircraft
- [x] Projectiles
- [x] Trees and props
- [x] Roads and bridges
- [x] Water and shorelines
- [x] Shroud
- [x] Shadows
- [x] Particles and effects
- [x] Radar
- [x] Complete playable world frame

Further fidelity and correctness improvements remain ongoing.

## Phase 5 — Modern Display and Presentation

- [x] Modern resolution handling
- [x] Widescreen layouts
- [x] Ultrawide layouts
- [x] UI scaling foundation
- [x] Modern Options architecture
- [x] Simulation/render separation
- [x] Optional Smooth unit presentation
- [ ] Further fullscreen handling
- [ ] Refresh-rate controls
- [ ] Key rebinding

## Phase 6 — Performance

- [x] Performance instrumentation
- [x] Frame-time attribution
- [x] Renderer profiling
- [x] Evidence-driven optimization foundation
- [x] Large-game simulation profiling (deterministic AI-vs-AI benchmark)
- [x] Exact serial CPU optimizations (ZoneCalc, A* open list and line walk, launch-log flush)
- [ ] Heavy real-combat profiling with a human player
- [ ] Further CPU optimization (exhaustive failing path searches, ZoneCalc coincidence)
- [ ] Further frame-pacing improvements

## Phase 7 — Enhanced Experience

- [x] Original / Enhanced interface architecture
- [x] USA Enhanced HUD foundation
- [x] USA HUD visual refinement
- [x] China Enhanced HUD
- [x] GLA Enhanced HUD
- [ ] Cross-faction Enhanced HUD polish and final artwork
- [ ] Further graphics enhancements
- [ ] Higher-quality optional visual assets

## Phase 7b — Modern Experience UI (in progress)

- [x] UI archaeology and responsive layout foundation (UI.1)
- [ ] Main frontend / navigation shell (UI.2)
- [ ] Modern Match Setup (UI.3)
- [ ] Options and secondary frontend screens (UI.4)
- [ ] Modern in-game HUD for USA / China / GLA (UI.5)
- [ ] Resolution / scaling matrix, polish and native acceptance (UI.6)

Active performance optimization and Enhanced AI development are paused while
this phase runs; their regression gates stay in force.

## Phase 8 — Compatibility Completion

- [ ] Movies / cutscenes
- [ ] Broader replay validation
- [ ] Multiplayer validation
- [ ] Determinism validation
- [ ] Full Zero Hour native gameplay validation
- [ ] Remaining renderer fidelity issues

## Phase 9 — Extended Game

Only after the original games are sufficiently stable on the modernized
engine may the project provide a foundation for larger optional expansion.

Possible future work includes:

- New factions
- New units and buildings
- New maps
- New campaigns
- Expanded AI
- Larger battles
- New gameplay systems
- Additional graphics features

Potential future factions include:

- Europe
- Russia

These are long-term concepts and are deliberately separated from the current
Generals and Zero Hour preservation work.

---

# Original vs Enhanced

The project deliberately avoids forcing every modernization feature onto the
original presentation.

## Original

The preservation path.

Its purpose is to retain the recognizable original Generals experience as
closely as practical on the modernized engine.

## Enhanced

An optional modern presentation layer.

Enhanced may provide:

- Modernized HUD layouts
- Improved information hierarchy
- Higher-quality UI assets
- Improved graphics options
- Additional visual improvements

Enhanced should still unmistakably look and feel like **Command & Conquer:
Generals**.

Gameplay changes are not automatically part of Enhanced presentation.

---

# AI-Assisted Development

This modernization project makes extensive use of **AI coding agents** as part
of the development workflow.

AI agents are used for tasks including:

- Source-code and engine analysis
- C++ implementation
- Refactoring
- Renderer migration
- Automated testing
- Build diagnostics
- Runtime diagnostics
- Regression investigation
- Performance analysis
- Technical documentation

The project is directed and reviewed by **Martin Raudstein / MKRaudstein**.

Project goals, compatibility requirements, architecture priorities and
acceptance criteria are human-directed.

AI-generated code is **not considered correct solely because an agent produced
it**.

Changes are validated through combinations of:

- Automated tests
- AMD64 builds
- Win32 builds
- Generals and Zero Hour build validation
- Reference-behaviour comparisons
- Runtime diagnostics
- Preserved evidence
- Performance measurements
- Regression checks
- Manual native-game testing
- Human visual and behavioural acceptance

AI agents may autonomously investigate and implement bounded technical work
within established project constraints.

Native game execution, major architectural decisions and acceptance of
significant runtime or visual milestones remain human-controlled.

The project therefore follows a:

> **Human-directed, AI-assisted development model.**

---

# Development Approach

Modernization is performed incrementally.

Original behaviour is first understood and preserved before or while the
underlying implementation is replaced.

Major milestones typically separate:

1. Source and architecture analysis
2. Implementation
3. Automated regression testing
4. AMD64 validation
5. Win32 preservation validation
6. Isolated native candidate staging
7. Manual native-game testing
8. Evidence preservation
9. Human acceptance

A successful build or automated rendering test is not automatically considered
a successful native milestone.

Machine validation and human visual/behavioural validation are deliberately
treated as separate acceptance criteria where appropriate.

Detailed engineering notes, audits, profiling data, runtime evidence and
milestone documentation are maintained under:

`docs/modernization/`

The root README is intended to describe the public state and direction of the
project rather than act as a detailed engineering changelog.

---

# Branches

## `main`

The `main` branch serves as the stable/upstream-compatible baseline.

Experimental Modern Engine features should not be assumed to exist there.

## `dev/modern-engine`

This is the active modernization branch.

It contains the current:

- AMD64 engine work
- Direct3D 11 renderer
- Modern Experience
- performance work
- Modern Display / Settings completion

Major modernization work remains here while systems are still being developed
and validated.

---

# Building

The upstream project supports multiple build configurations.

This modernization branch currently performs its primary native engine work
and runtime validation on Windows.

## Standard Win32 Build

```bash
cmake --preset win32
cmake --build --preset win32
```

The repository uses a vcpkg manifest (`vcpkg.json`) for dependency management.

The experimental AMD64/D3D11 runtime uses its own staging and validation
workflow.

Do not assume that the normal Win32 installation workflow represents the
current AMD64 development runtime.

Detailed modernization build, staging and runtime information is maintained in
the repository documentation and scripts.

For upstream build information, see the
[GeneralsGameCode build guides](https://github.com/TheSuperHackers/GeneralsGameCode/wiki/build_guides).

---

# Running the Game

Original **Command & Conquer: Generals** and **Zero Hour** game data is still
required.

This repository does not provide the proprietary game assets required to run
the games.

Users must obtain the original game data from a legitimate copy.

The current Modern Engine branch is still a development project and is **not
yet intended as a finished end-user release**.

Development runtime workflows are deliberately isolated so modernization
testing does not require replacing the user's installed original game
executables.

---

# Known Incomplete Areas

Although Generals is now playable on the modernized engine, development is not
complete.

Important remaining areas include:

- Movie / cutscene playback
- Full replay validation (a recorded Generals Skirmish re-simulates; Zero Hour, campaign and cross-version replays are not validated)
- Multiplayer validation
- Determinism validation
- Broader Zero Hour validation (multiplayer, campaign, endurance)
- Additional renderer fidelity work
- Some shadow correctness/fidelity cases
- Further display controls
- Key rebinding
- Cross-faction Enhanced HUD polish
- Heavy-combat performance validation
- Additional Modern Experience polish

The presence of a feature in the native engine should not automatically be
interpreted as final release-quality implementation.

---

# Upstream Project

This work is based on:

**TheSuperHackers / GeneralsGameCode**

https://github.com/TheSuperHackers/GeneralsGameCode

The upstream project is a community-driven effort to fix and improve
*Command & Conquer: Generals* and *Command & Conquer: Generals – Zero Hour*.

The complementary upstream game-data project is:

**TheSuperHackers / GeneralsGamePatch**

https://github.com/TheSuperHackers/GeneralsGamePatch

It focuses on game data and assets including INI scripts, GUI, AI, maps,
models, textures, audio and localization.

This modernization fork should **not** be interpreted as the official
development direction, status or release plan of TheSuperHackers or the
upstream GeneralsGameCode project.

---

# Contributing

For information about the upstream project, see:

- [CONTRIBUTING.md](CONTRIBUTING.md)
- [GeneralsGameCode Wiki](https://github.com/TheSuperHackers/GeneralsGameCode/wiki)
- [TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode)

Modernization-specific engineering documentation is available under:

`docs/modernization/`

Because the Modern Engine branch includes substantial architectural changes,
contributors should review the current modernization documentation and
implementation before making large renderer, platform or simulation changes.

---

# Project Maintenance

`README.md` describes the public state of the modernization project.

It should be reviewed at major milestone closeout so that completed public
capabilities, current development work and major remaining limitations remain
accurate.

Detailed implementation history, profiling results and engineering evidence
belong under `docs/modernization/` rather than turning this README into a
commit-by-commit changelog.

---

# License & Legal Disclaimer

EA has not endorsed and does not support this project.

**Command & Conquer**, **Command & Conquer: Generals**, **Zero Hour** and
related trademarks and proprietary game assets belong to their respective
owners.

This project is based on the released GeneralsGameCode source and the
community-maintained upstream project.

The source code is licensed under the **GPL-3.0 License**.

See [LICENSE.md](LICENSE.md) for details.

The source-code license does **not** grant permission to redistribute
proprietary Command & Conquer game assets.

Original game data must be obtained from a legitimate copy of the game.

---

# Credits

## Modern Engine fork

**MKRaudstein / Martin Raudstein**

Modern Engine direction, project priorities, native validation and human
acceptance.

Development is performed through a human-directed, AI-assisted workflow.

## Upstream

This project would not exist without the work of:

- Electronic Arts and the original Command & Conquer: Generals development team
- TheSuperHackers
- GeneralsGameCode contributors
- The wider Command & Conquer community

For the full upstream contribution history, see the original repository and
Git history.

---

**MKRaudstein Modern Engine**

*Preserve the game. Modernize the engine.*