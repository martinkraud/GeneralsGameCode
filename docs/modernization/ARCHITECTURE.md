# Engine architecture map

Scope: revision `adac468d7`, investigated 2026-10-07. Entry point for runtime
instructions and evidence labels: [BASELINE.md](BASELINE.md).

## Source organization

| Directory | Role |
|---|---|
| `Generals/Code` | Base Generals game-specific implementation and tools |
| `GeneralsMD/Code` | Zero Hour game-specific implementation and tools; prioritize this title |
| `Core/GameEngine` | Shared interfaces, simulation support, AI/pathfinder, common systems, GUI/messages |
| `Core/GameEngineDevice` | Shared Win32, W3D, Miles audio and video device implementations |
| `Core/Libraries/Source/WWVegas` | W3D scene/mesh/animation/rendering, math, containers, save/load and utility libraries |
| `Dependencies` | Bink/Miles loaders, utility adapters, debug/text dependencies and optional legacy SDK |
| `cmake` | Build/retail/debug/memory configuration, dependency fetching, compiler and toolchain setup |
| `Tests/Google`, `Benchmarks/Google` | Google Test/Benchmark suites for dependencies, Core and game variants |
| `GeneralsReplays` | Replay/map compatibility corpus; CI replay jobs |
| `.github/workflows`, `scripts` | CI, packaging and build/install/replay automation |

Game-specific includes precede Core includes. Many translation units are supplied
through CMake INTERFACE targets and compiled for each title. This is not three
independent complete engines: track each target's source and include ownership.
Evidence: `GeneralsMD/Code/CMakeLists.txt`, `Generals/Code/CMakeLists.txt`,
`Core/GameEngine/CMakeLists.txt`, `Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt`.

## Lifecycle and dependencies

`GeneralsMD/Code/Main/WinMain.cpp` creates the Windows application/window and
engine; startup command parsing resolves working directory before data loading.
`Win32GameEngine` supplies platform behavior. Game factories create client,
logic, file systems, audio, display and other subsystems. `GameEngine::init`
registers/initializes global subsystems; `TheSubsystemList` coordinates reset.
Global `The...` pointers are pervasive: ownership and lifetime are not contained
in independent service objects.

The principal loop is in each title's `GameEngine.cpp::execute/update`:

```text
execute
  update
    radar / audio / GameClient update
      keyboard + mouse -> MessageStream
      drawable / terrain / display / GUI updates and drawing
    MessageStream propagation
    Network update if present
    logic readiness check
      GameLogic update if allowed
      GameClient::step unless time frozen
  FramePacer update/wait
```

The **client runs before the current iteration's simulation update**. Visual
sync and particle updates observe the preceding logic update through
`hasUpdated`. Reordering this is a behavior change, not just code cleanup.
See [TIMING.md](TIMING.md).

## Important systems

| System | Classes / source anchors | Relationship |
|---|---|---|
| Simulation | `GameLogic`, `Object`, `BehaviorModule`, `UpdateModule`, `ThingFactory` | Objects own behavior modules; frame-based update scheduling advances world state |
| Object updates | Zero Hour `GameLogic/System/GameLogic.cpp::update` | Processes messages, recorder, module updates, AI, partitions, destruction, weapons/locomotors/victory and frame increment |
| Spatial systems | `PartitionManager`; Core `GameLogic/AI/AIPathfind.cpp`; title AI update modules | Queries, shroud, movement and pathfinding interact with deterministic logic |
| Client representation | `GameClient`, `Drawable`, W3D draw modules | Drawables link to logic objects but maintain visual state; not a fully immutable render snapshot |
| Renderer | `Display` -> `W3DDisplay`; `View` -> `W3DView`; WW3D and DX8 backend | Scene/view rendering and legacy device facade; see RENDERER |
| Input / commands | `Keyboard`, `Mouse`, `MessageStream`, `CommandXlat`, `CommandList`, `GameMessage` | Raw input becomes UI or scheduled simulation commands |
| UI | `WindowManager`, `GameWindow`, `InGameUI`, `ControlBar`, `Shell`, display strings | WND/INI/data-driven UI with pixel/layout assumptions |
| Data / mods | `FileSystem`, `LocalFileSystem`, `ArchiveFileSystem`, `Win32BIGFileSystem`, INI parsers | Loose files, BIG archives and mod overrides feed assets/templates |
| Networking | Core `GameNetwork/Network.cpp`, `ConnectionManager`, transport and command buffers | Commands assigned to logic frames; readiness gates simulation |
| Replays | Title `Common/Recorder.cpp::RecorderClass` | Records seed/game metadata and execution-frame commands; CRC validation |
| Saves | Title `Common/System/SaveGame/GameState.cpp`, Core `XferSave`, `XferCRC`, snapshots | Versioned snapshot traversal; compatible field widths/order matter |
| Randomness | Core `Common/RandomValue.cpp` | Separate logic/client/audio seed streams; logic seed included in CRC |
| Audio/video | `AudioManager`, `MilesAudioManager`, `BinkVideoPlayer`, optional FFmpeg device | Updated through engine/client paths; timing is not uniformly simulation time |
| Memory | Core `Common/System/GameMemory.cpp`, memory pools and configuration | Allocation policy is legacy and can affect ordering/compatibility |

## Generals versus Zero Hour

Both `GameEngine.cpp` implementations have the FramePacer scheduler, and both
clients scale drawable updates. Core owns the common W3D/display/timing layer.
Zero Hour adds expansion behavior/content/network/UI details and is upstream's
primary focus. Do not infer complete behavioral parity from shared interfaces.
Future fixes should land in Zero Hour first and be mirrored closely in Generals
where applicable (`CONTRIBUTING.md`).

## Performance evidence and measurement plan

**Observed instrumentation, not measured bottlenecks:**

- `win32-profile` enables legacy profiling and Tracy. `cmake/tracy.cmake` pins
  the fallback Tracy dependency to 0.13.1; README requests the matching viewer.
- `Core/Libraries/Include/rts/profile.h` maps profiler scopes, frame marks,
  plots/messages and images to Tracy macros; disabled builds compile them out.
- `W3DProfilerFrameCapture.cpp` captures preview images when connected, normally
  at 500 ms intervals; account for readback/capture overhead in measurements.
- `PerfTimer.h`, `PerfGather`, legacy `Source/profile`, WWDebug statistics and
  W3DDisplay FPS/draw counters remain available, with build guards.
- Release MSVC linker settings produce PDBs (`cmake/compilers.cmake`); external
  Visual Studio CPU sampling is feasible in principle. No dedicated Visual
  Studio profiler session or Build Insights integration was found in inspected
  CMake/workflows. Build Insights would measure compilation, not gameplay speed.
- TESTING describes optional Google Test and Google Benchmark executables
  (`z_googletest`, `z_googlebenchmark`, and `g_` equivalents). CI builds/runs
  tests for win32 jobs; builds benchmarks without running them.

**Hypotheses requiring profiling:** AI/pathfinder cost at high unit counts;
spatial/shroud/ghost queries; drawable traversal/animation; W3D terrain blends,
sorting/skins/particles/shadows; GPU fill/bandwidth at 5120x1440; archive loading;
memory allocation; limiter spin time; waiting in Present; network stalls.
Source comments about costly shroud traversal and render-stat spike thresholds
are clues, not measurements on this machine.

First capture fixed scenarios with exact executable/data hashes, compiler,
resolution, render cap, simulation speed, wrapper, map/replay/seed, camera,
player/unit counts and warmup. Report distributions (median, p95/p99 frame time),
logic ticks/second, logic/client/render/Present/wait durations and CRC outcome.
Separate uncapped CPU-throughput tests from rendered pacing tests and headless
determinism checks. Run enough repeated samples to quantify variance. No
optimization ranking or claimed speedup is established yet.
