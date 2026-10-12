# Welcome to the Generals Game Code Project

> ## Modern Engine Development
>
> This repository also contains an experimental modernization of
> **Command & Conquer: Generals** and **Command & Conquer: Generals – Zero Hour**
> focused on native AMD64 support, Direct3D 11 and broader engine modernization.
>
> Active development currently takes place on the
> [`dev/modern-engine`](../../tree/dev/modern-engine) branch.
>
> **The modernization features described on that branch should not be assumed
> to exist on `main`.**
>
> The `main` branch currently serves as the stable/upstream-compatible baseline.
> If the modernization project reaches the required level of completeness and
> validation, the intention is for the modernized engine to eventually become
> the main development line.
>
> See the
> [`dev/modern-engine` README](../../tree/dev/modern-engine#readme)
> for its current status, roadmap and technical direction.

---

GeneralsGameCode is a community-driven project aimed at fixing and improving
the classic RTS game *Command & Conquer: Generals* and its expansion
*Command & Conquer: Generals – Zero Hour*.

This repository is forked from
[TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode).

The upstream project contains the source code for both games and focuses on
fixing, maintaining and improving the released game source.

There is also a complementary upstream project for fixing and improving game
data and assets such as INI scripts, GUI, AI, maps, models, textures, audio
and localization:

[TheSuperHackers/GeneralsGamePatch](https://github.com/TheSuperHackers/GeneralsGamePatch)

---

# Project Overview

The game was originally developed using Visual Studio 6 and C++98.

The upstream GeneralsGameCode project has modernized the codebase to support
modern development environments including Visual Studio 2022 and C++20.

Its initial goals include fixing critical bugs and implementing improvements
while maintaining compatibility with the original:

- *Command & Conquer: Generals* version 1.08
- *Command & Conquer: Generals – Zero Hour* version 1.04

This fork additionally contains the separate `dev/modern-engine` development
line for more extensive engine modernization.

---

# Modern Engine Branch

The experimental modernization work is intentionally isolated from `main`
while major engine systems are being changed.

The work currently includes areas such as:

- Native AMD64 / 64-bit Windows support
- Direct3D 11 rendering
- Renderer modernization
- Simulation/render timing separation
- Modern display support
- Performance and frame-pacing work
- Engine maintainability
- Preservation of original Generals and Zero Hour behaviour

The modernization branch is under active development and is **not currently
intended as a replacement for the original game executables**.

For current progress and technical status, see:

**[`dev/modern-engine`](../../tree/dev/modern-engine)**

---

# Running the Upstream-Compatible Game

To run *Generals* or *Zero Hour* using the normal upstream-compatible project,
you need the original *Command & Conquer: Generals* and *Zero Hour* game data.

The easiest current way to obtain the games is through
*Command & Conquer The Ultimate Collection* on Steam.

For upstream releases and installation instructions, see:

- [GeneralsGameCode Releases](https://github.com/TheSuperHackers/GeneralsGameCode/releases)
- [GeneralsGameCode Wiki](https://github.com/TheSuperHackers/GeneralsGameCode/wiki)

> The experimental AMD64/D3D11 development branch uses a separate development
> and validation workflow. The normal upstream installation instructions
> should not be assumed to apply to that branch.

---

# Building the Game Yourself

The upstream project supports building on Windows and Linux.

For detailed upstream build instructions, see the
[GeneralsGameCode build guides](https://github.com/TheSuperHackers/GeneralsGameCode/wiki/build_guides).

## Quick Start

### Windows

```bash
cmake --preset win32
cmake --build build/win32 --config Release
```

### Linux via Docker

```bash
./scripts/docker-build.sh
./scripts/docker-install.sh --detect
```

---

# Dependency Management

The repository uses a vcpkg manifest (`vcpkg.json`).

Dependency versions come from the `builtin-baseline` commit recorded there,
with per-port `overrides` when a specific version is required.

GitHub Actions can consume these ports through a vcpkg binary cache backed by
a NuGet feed on GitHub Packages.

---

# Profiling

Tracy profiling is supported by the upstream project's `win32-profile`
CMake preset.

See the upstream documentation for current profiling instructions and
supported Tracy versions.

---

# Contributing

For the original/upstream-compatible project, see:

- [CONTRIBUTING.md](CONTRIBUTING.md)
- [GeneralsGameCode Wiki](https://github.com/TheSuperHackers/GeneralsGameCode/wiki)
- [TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode)

The experimental Modern Engine work has additional engineering documentation
on the `dev/modern-engine` branch under:

`docs/modernization/`

---

# Upstream Project

This repository is based on:

**TheSuperHackers / GeneralsGameCode**

https://github.com/TheSuperHackers/GeneralsGameCode

The complementary game-data project is:

**TheSuperHackers / GeneralsGamePatch**

https://github.com/TheSuperHackers/GeneralsGamePatch

The experimental modernization work in this fork should not be interpreted as
the official development direction, status or release plan of the upstream
GeneralsGameCode project.

---

# License & Legal Disclaimer

EA has not endorsed and does not support this project.

All trademarks are the property of their respective owners.

The source code is licensed under the
[GPL-3.0 License](https://www.gnu.org/licenses/gpl-3.0.html).

See [LICENSE.md](LICENSE.md) for details.

The source-code license does not grant permission to redistribute proprietary
*Command & Conquer* game assets. Original game data must be obtained from a
legitimate copy of the game.
