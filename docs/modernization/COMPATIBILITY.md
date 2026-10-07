# Compatibility and deterministic behavior

Scope: revision `adac468d7`, 2026-10-07. Compatibility is a design constraint;
successful compilation is not compatibility evidence.

## Explicit upstream contract

README targets Generals 1.08 / Zero Hour 1.04 compatibility. Core
`GameEngine/Include/Common/GameDefines.h` defines retail guards for CRC, data,
Xfer saves, pathfinding/allocation, circle-fill algorithm, networking and AIGroup.
The CMake retail option defaults to DEFAULT; OFF enables incompatible fixes.
Record every guard and compiler setting with test results.

`cmake/config-retail.cmake` states compilers other than VC6 SP6 `12.00.8804`
are not CRC compatible with retail. `TESTING.md` requires optimized VC6 with
engine debug mode OFF for retail replay verification. Modern MSVC is a useful
development target, not demonstrated retail multiplayer equivalence.

## How multiplayer/replays preserve simulation agreement

Core `GameNetwork/Network.cpp::update` transfers commands into connection
management, waits for all commands for a logic frame and for a pacing deadline,
then relays the frame's commands and marks frame data ready. Engine scheduling
advances GameLogic only when network frame data is ready. ConnectionManager,
transport, run-ahead and frame rate coordinate command delivery and pacing.

Determinism relies on the same initial state/data/seed, commands in the same
execution-frame/order, and equivalent computations. The wall clock decides when
a tick may execute; objects advance in logical frame units. Changing wall-clock
pacing is not necessarily a state change, but changing which commands enter
which tick can be one.

ZH `Common/Recorder.cpp` stores game metadata and seed, executable/INI CRCs,
commands with execution frame, and CRC messages. Playback queues commands by
logic frame. It is not a recording of rendered images or every object state.
Simulation changes can therefore invalidate old replays without changing their
file parser.

Core `Common/RandomValue.cpp` maintains separate game logic/client/audio seed
arrays. `GameLogic::getCRC` traverses objects and relevant subsystem snapshots
and includes the logic RNG seed CRC. Network/replay CRC comparisons expose
divergence; a CRC mismatch is a symptom, not its root cause.

Client updates sit inside VERIFY_CRC checks in GameEngine. Command-line
`-VerifyClientCRC`, CRC logging/module/deep-save options and `-NetCRCInterval`
exist for diagnosis (see `CommandLine.cpp` comments). Availability/usefulness
depends on build guards. Do not move GameLogic into the client CRC block.

## Compatibility matrix and tests still required

| Area | Preserve | Validation |
|---|---|---|
| Original content | BIG/W3D/textures/audio/maps/localization and data lookup order | Vanilla campaigns/skirmishes, languages, asset-resolution logs |
| INI/mods | Parsing, defaults, template/module interpretation and override precedence | Fixed representative mods, missing/extra field behavior, known maps |
| Saves | Snapshot versions, widths/order, object IDs, state restoration | Retail/new save load, save/load continuation CRCs, campaign and mod saves |
| Replays | Metadata and command encoding, frame semantics, simulation behavior | Existing corpus plus fresh same-build recordings; exact CRC reports |
| Multiplayer | Protocol, execution frames/order, command grouping, random seed | Long LAN matches, mixed render caps, stalls, reconnect/disconnect handling |
| Determinism | Floating-point behavior, RNG consumption and traversal order | Identical replay runs, frame-indexed CRCs, rendered vs headless comparisons |
| Visuals | Timing, attachments, camera, GUI, picking | Side-by-side fixed scenarios and interactions across frame caps/resolutions |

This session performs none of those runtime tests. Content compatibility of the
current MSVC executable, same-build network determinism, retail saves and mod
coverage are UNKNOWN. Retail goals in source guards do not resolve compiler
differences.

## Highest-risk changes

- Changing BaseFps, WWSyncPerSecond, unit conversions, tick counts, update sleep
  schedules, AI/pathfinding budgets or order changes gameplay.
- Floating-point compiler/optimization/ISA changes, math functions, intermediate
  precision, rounding or fused operations can change deterministic results.
  Do not assume x64 and x86 produce the same simulation CRC.
- Parallel simulation work, unordered containers, allocator changes, race-
  dependent iteration and nondeterministic job completion can reorder results.
- Rendering/client callbacks can reach logic objects; visual work is not
  automatically safe just because its class name contains Client. For example,
  StealthUpdate calls updateDrawable from logic paths. Audit both directions.
- Consuming logic RNG in a render update varies random state with render rate.
- Serialization type/padding/size changes, pointer-derived values and reordered
  snapshot traversal risk saves, replay metadata and CRC compatibility.
- Archive precedence, duplicate INIZH, mod overrides, data changes and differing
  executable/INI checksums can invalidate comparisons before code behavior is
  involved.
- Input sampling frequency, message propagation order, speed/pause/fast-forward
  transitions and network-ready handling can change command-to-frame assignment.

## Safer visual work, subject to verification

Render-only interpolation of copied previous/current visual transforms,
camera/input smoothing that leaves command targets/ticks intact, GUI animation,
cursor/display handling, and presentation/limiter instrumentation are plausible
lower-risk work. Interpolated state must never be written back to Objects,
pathfinding, collisions, weapon calculations, saves or command targets.

Use `-VerifyClientCRC` where supported and compare state by logic-frame index
across 30/60/120/144/240 render caps. Then test gameplay input separately: a
replay with fixed commands cannot prove live input still chooses the same ticks.
Headless tests cannot catch camera/UI/render attachment regressions.

## Existing test entry points

`TESTING.md` documents copied replay/map placement in the game's user-data
folders and `generalszh.exe -jobs 4 -headless -replay subfolder/*.rep` with exit
code/log capture. Use the correct optimized VC6 baseline for retail compatibility;
label modern-MSVC tests separately. User-data folders are shared unless isolated
through a dedicated Windows account. CI replay checks currently target VC6.

Optional unit tests and microbenchmarks do not substitute for full engine
determinism/content tests. Every future compatibility-breaking proposal must
first document which formats/peers/mods it affects, why, migration/versioning
strategy and fallback. Keep default behavior compatible wherever feasible.
