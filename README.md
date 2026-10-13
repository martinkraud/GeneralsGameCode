# GeneralsGameCode — Modern Engine

> **Experimental modernization fork of [TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode).**
>
> This branch is focused on native AMD64 support, Direct3D 11 rendering,
> engine modernization and long-term performance improvements for
> **Command & Conquer: Generals** and **Command & Conquer: Generals – Zero Hour**.
>
> It is under active development and is **not yet intended as a replacement
> for the original game executables**.

---

# About This Fork

This repository builds on the community-driven GeneralsGameCode project
maintained by TheSuperHackers.

The upstream project modernizes, fixes and improves the released source code
for *Command & Conquer: Generals* and *Command & Conquer: Generals – Zero Hour*.

This fork explores a larger engine modernization while preserving the
original games as compatibility targets.

The main goals are:

- Native 64-bit Windows support
- A modern Direct3D 11 rendering backend
- Improved performance and frame pacing
- Separation of simulation and rendering rates
- Modern display and resolution support
- Improved stability and maintainability
- Preservation of original Generals and Zero Hour gameplay
- A foundation for future engine and gameplay expansion

The current work deliberately separates **engine modernization** from future
content changes.

The first objective is to make the original games work correctly on the
modernized engine.

---

# Current Status

## Native AMD64 — Working

Generals now builds and runs as a native AMD64 executable.

The current native path has successfully demonstrated:

- Native 64-bit application startup
- Original game asset loading
- Original INI and WND parsing
- Native Win32 application window
- Hardware Direct3D 11 device and presentation
- Original Render2D-based UI through D3D11
- Original fonts and UI textures
- Original mouse and keyboard input
- Original interactive Main Menu
- Solo Play / Skirmish setup
- Map loading
- Live GameLogic on the ordinary AMD64 engine loop
- AI/game simulation during a real Skirmish
- Clean bounded game shutdown

A real Generals Skirmish can currently progress through:

`Main Menu → Skirmish → Map Load → Live GameLogic`

on the native AMD64 engine.

---

## Direct3D 11 UI — Working

The original Generals user interface can now run through the native
Direct3D 11 backend.

The current path supports the functionality required for:

- Original UI textures
- Fonts and text
- Render2D geometry
- Alpha blending
- Additive rendering
- Legacy grayscale behaviour
- Original WND layouts
- Interactive buttons
- Original mouse cursors

The current objective is compatibility with the original interface before
any future UI redesign is considered.

---

## Direct3D 11 World Renderer — In Progress

The major current development task is replacing the remaining Direct3D 8
world-rendering path with native Direct3D 11 rendering.

The first terrain pipeline is integrated and currently includes:

- D3D11 depth-buffer support
- Backend-neutral geometry residency
- CPU-backed legacy vertex/index buffers on the AMD64 D3D11 path
- CPU generation of the original terrain texture atlas
- Terrain base and blend passes
- Original game camera integration
- World/view/projection transform conversion
- Terrain submission from a real loaded Generals map

The first real-map terrain gate now successfully reaches:

`Main Menu → Skirmish → Map Load → GameLogic → D3D11 Terrain Submission`

The terrain backend executes successfully and completes its draw calls and
cleanup.

**Visible terrain output is still under investigation.**

The current real-map test produces a black world viewport even though the
terrain pass is submitted and drawn by the backend.

The original in-game UI, cursor and game simulation remain active.

Camera transforms, clipping, culling, depth state and other world-render
state are being investigated before additional scene systems are migrated.

---

# Current Feature Status

| Area | Status |
| --- | --- |
| Native AMD64 executable | ✅ Working |
| Original asset loading | ✅ Working |
| D3D11 presentation | ✅ Working |
| Original Main Menu | ✅ Working |
| Mouse / keyboard input | ✅ Working |
| Original UI through D3D11 | ✅ Working |
| Skirmish setup | ✅ Working |
| Map loading | ✅ Working |
| AMD64 GameLogic | ✅ Working |
| AI / game simulation | ✅ Working |
| Terrain data / geometry submission | ✅ Working |
| D3D11 terrain draw pipeline | 🧪 Experimental |
| Visible D3D11 terrain | 🚧 In progress |
| Units / buildings | ⏳ Planned |
| Trees / props | ⏳ Planned |
| Roads / bridges | ⏳ Planned |
| Water / shorelines | ⏳ Planned |
| Fog / shroud | ⏳ Planned |
| Shadows | ⏳ Planned |
| Particles / effects | ⏳ Planned |
| Complete D3D11 world frame | ⏳ Planned |
| Full Zero Hour AMD64 validation | ⏳ Planned |

---

# Rendering Strategy

This project does **not** aim to implement a generic Direct3D 8 emulation
device.

Instead, original rendering semantics are being migrated into
backend-neutral or purpose-built Direct3D 11 paths.

The fidelity target is:

> Preserve the original visual intent, assets and rendering behaviour while
> implementing equivalent modern rendering systems.

CPU reference implementations and automated tests are used where useful to
validate legacy texture-stage, blending and combiner behaviour.

The goal is faithful visual equivalence rather than bit-identical Direct3D 8
raster output.

The Win32/original renderer remains available as an important reference
during migration.

---

# Simulation and Rendering

The modernization separates gameplay simulation concerns from rendering.

The original gameplay simulation remains authoritative while the renderer is
being modernized independently.

This creates a foundation for:

- Higher rendering frame rates without increasing game speed
- Better frame pacing
- Rendering interpolation
- Future performance improvements
- Modern display support

Simulation behaviour and determinism remain compatibility-sensitive areas and
are not intentionally changed simply to increase rendering performance.

---

# Generals and Zero Hour

The engine is shared by both games.

## Command & Conquer: Generals

Generals is currently the primary native validation target for the AMD64 and
Direct3D 11 migration.

Most recent native developer gates are therefore performed in Generals first.

## Command & Conquer: Generals – Zero Hour

Zero Hour remains a target of the same modernized engine.

Shared engine work is built and tested against Zero Hour where appropriate,
but a milestone validated only through a Generals native developer run should
not be interpreted as equivalent Zero Hour runtime validation.

---

# Roadmap

## Phase 1 — Engine Foundation

- [x] Modern build system inherited and extended from upstream
- [x] Native AMD64 builds
- [x] Win64 startup
- [x] Simulation/render timing groundwork
- [x] D3D11 backend foundation

## Phase 2 — Original UI on D3D11

- [x] D3D11 presentation
- [x] Render2D
- [x] Original UI textures
- [x] Original fonts
- [x] Original Main Menu
- [x] Mouse and keyboard input
- [x] Interactive Skirmish setup

## Phase 3 — Native Game Loop

- [x] Map loading on AMD64
- [x] Live GameLogic
- [x] Skirmish simulation
- [x] AI/game logic during bounded native runs
- [x] Clean native shutdown

## Phase 4 — D3D11 World Rendering

- [x] Depth-buffer foundation
- [x] Backend-neutral geometry
- [x] CPU terrain atlas
- [x] Terrain geometry submission
- [x] Terrain draw pipeline integration
- [ ] Visible terrain
- [ ] Static meshes
- [ ] Units and buildings
- [ ] Trees and props
- [ ] Roads and bridges
- [ ] Water and shorelines
- [ ] Fog and shroud
- [ ] Shadows
- [ ] Particles and effects
- [ ] Complete world frame

## Phase 5 — Full Game Integration

- [ ] Complete Generals gameplay validation
- [ ] Complete Zero Hour validation
- [ ] Save/load validation
- [ ] Replay validation
- [ ] Multiplayer/determinism validation
- [ ] Performance profiling and optimization

## Phase 6 — Modern Experience

- [ ] Modern resolution handling
- [ ] Ultrawide support
- [ ] UI scaling
- [ ] Modern graphics settings
- [ ] Modern controls and key rebinding
- [ ] Further CPU/gameplay performance work

## Phase 7 — Extended Game

After the original games are stable on the modernized engine, the engine may
also provide the foundation for a separate extended version.

Possible future work includes:

- New factions
- New units and buildings
- New campaigns
- New maps
- Expanded AI
- New gameplay systems
- Larger battles
- Additional graphics features
- Modernized UI

These features are deliberately separated from the compatibility work on
Generals and Zero Hour.

---

# AI-Assisted Development

This modernization project makes extensive use of **AI coding agents** as part
of the development workflow.

AI agents are used for tasks including:

- Source-code and engine analysis
- C++ implementation
- Refactoring
- Renderer migration work
- Automated testing
- Build and runtime diagnostics
- Regression investigation
- Technical documentation

The project is directed and reviewed by **Martin Raudstein**.

Project goals, compatibility requirements and development priorities are
human-directed.

Architectural decisions and significant changes are reviewed before being
accepted, while native game execution, visual verification and acceptance of
major runtime milestones require human validation.

AI-generated code is **not considered correct solely because an agent produced
it**.

Changes are validated through combinations of:

- Automated tests
- AMD64 and Win32 builds
- Reference-behaviour comparisons
- Runtime diagnostics and preserved evidence
- Regression checks
- Manual native-game testing
- Human visual acceptance

AI agents may autonomously investigate and implement bounded technical work
within established project constraints, but significant architectural
decisions and native acceptance gates remain under human control.

The project therefore follows a **human-directed, AI-assisted development
model**.

---

# Development Approach

Modernization is performed incrementally.

Original behaviour is first understood and preserved before the underlying
implementation is replaced.

Native runtime milestones use isolated test profiles and evidence-based
acceptance gates so that platform, renderer and game-logic changes can be
validated independently.

A typical modernization milestone separates:

1. Offline source analysis
2. Implementation
3. Automated regression testing
4. AMD64 and Win32 build validation
5. Isolated native runtime validation
6. Preserved runtime evidence
7. Human visual or behavioural acceptance where required

A machine-successful rendering test is not automatically considered visually
accepted. Machine validation and developer visual validation are treated as
separate acceptance criteria where appropriate.

Detailed engineering notes, audits, runtime evidence and modernization
milestones are kept under:

`docs/modernization/`

The root README intentionally describes the public project status rather than
serving as a detailed engineering log.

---

# Branches

## `main`

The `main` branch currently serves as the stable/upstream-compatible baseline.

Experimental Modern Engine features should not be assumed to exist there.

## `dev/modern-engine`

This branch contains the active AMD64/D3D11 modernization work documented by
this README.

Development takes place here while major engine systems are still being
migrated and validated.

The long-term intention is for completed and sufficiently validated
modernization work to become the primary development line rather than remain
permanently isolated on an experimental branch.

---

# Building

The upstream project supports Windows and Linux builds.

This modernization branch currently performs its primary native engine work
and runtime validation on Windows.

## Standard Win32 Build

```bash
cmake --preset win32
cmake --build --preset win32
```

The repository uses a vcpkg manifest (`vcpkg.json`) for dependency management.

> **Important:** the experimental AMD64/D3D11 development runtime uses its own
> staging and validation workflow.
>
> Do not assume the normal Win32 installation process represents the current
> AMD64 development path.

Detailed modernization build, staging and runtime information is maintained
in the repository's development documentation and scripts.

For upstream build information, see the
[GeneralsGameCode build guides](https://github.com/TheSuperHackers/GeneralsGameCode/wiki/build_guides).

---

# Running the Original / Upstream Project

The original *Command & Conquer: Generals* and *Zero Hour* game data is still
required.

The games are available as part of *Command & Conquer The Ultimate Collection*
on Steam and through other legitimate releases.

For instructions relating to the upstream GeneralsGameCode project and its
released builds, see:

- [TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode)
- [Upstream releases](https://github.com/TheSuperHackers/GeneralsGameCode/releases)
- [Upstream Wiki](https://github.com/TheSuperHackers/GeneralsGameCode/wiki)

The experimental AMD64/D3D11 branch documented here is **not currently
intended as an end-user release**.

Development runtime workflows are deliberately isolated so that modernization
testing does not require replacing the user's installed original game
executables.

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
development direction, status or release plan of the upstream
GeneralsGameCode project.

---

# Contributing

For information about the upstream project, see:

- [CONTRIBUTING.md](CONTRIBUTING.md)
- [GeneralsGameCode Wiki](https://github.com/TheSuperHackers/GeneralsGameCode/wiki)
- [TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode)

Modernization-specific engineering documentation is available under:

`docs/modernization/`

Because the Modern Engine branch is undergoing substantial architectural
migration, contributors should review the current modernization documentation
and existing implementation before making large renderer or platform changes.

---

# License & Legal Disclaimer

EA has not endorsed and does not support this project.

All trademarks are the property of their respective owners.

The source code is licensed under the
[GPL-3.0 License](https://www.gnu.org/licenses/gpl-3.0.html).

See [LICENSE.md](LICENSE.md) for details.

The source-code license does not grant permission to redistribute proprietary
*Command & Conquer* game assets.

Original game data must be obtained from a legitimate copy of the game.
