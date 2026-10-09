# X64.1 - Architecture audit and parallel Win64 build foundation

Date: 2026-10-09. Baseline: `848b564fe144d0226116e7dbc2906cad20a1ecf8`.

## Verdict and scope

The baseline was clean, on `dev/modern-engine`, tracking and synchronized with
`origin/dev/modern-engine` before edits. Win32 remains the reference configuration.
**Win64 configures and begins real engine compilation, but does not link either
game, tools or game test executables (outcome C). X64 is not complete.**

This change adds experimental parallel configure/build/test presets and a
compiler-pointer-size guard. Every pre-existing preset object is unchanged.
No engine, gameplay, allocator, renderer, audio, serialization, networking or
pathfinding source changed. No compiler errors were suppressed or engine
components excluded. No game was launched, no Install target was run, and no
runtime or Steam installation was written.

The checked-in DX8 dependency cannot provide native x64 graphics: all four
bundled libraries contain I386 members. The installed Windows SDK supplies x64
DirectInput/GUID libraries, but no D3D8/D3DX8 libraries. Therefore the roadmap must
allow **x64 build foundation + renderer boundary/native backend dependency work
before first x64 gameplay**. A complete x64 gameplay port followed only then by
D3D11 is not supported by the current dependency evidence. This conclusion is
about this repository and installed SDK, not a claim that no external x64 D3D8
implementation could exist. None has been selected or validated here.

## Environment and configurations

| Item | Actual configuration |
|---|---|
| Host | Windows, PowerShell 7, VS Community 18 |
| Compiler | MSVC 19.51.36260.0, toolset directory 14.51.36231 |
| SDK | Windows SDK 10.0.26100.0 |
| Build tools | CMake 4.3.1-msvc1; Ninja 1.13.2; Ninja Multi-Config |
| Win32 reference | Existing `build/win32`, x86 compiler, Release |
| Win64 experiment | Separate `build/win64`, Hostx64/x64 compiler, 8-byte pointers, Release |
| Language/CRT | Existing C++20 settings, `/MD` Release, `/MDd` Debug |
| Debug information | Existing Embedded/Z7 preset setting; no ABI or FP flag changes |
| Titles/tools/tests | Both games, both title tool sets and core tools enabled; tests ON |
| Other options | Retail compatibility DEFAULT, game memory pool/bounding walls ON, crash dumps ON; extras, legacy profile, Tracy, FFmpeg, ASAN and benchmark option OFF |
| vcpkg | No `VCPKG_ROOT` or executable available in this session; new optional preset unverified |

`win64` inherits the normal default settings, sets external architecture `x64`,
`RTS_EXPECTED_POINTER_SIZE=8`, `/W3` and tests ON. Ninja external architecture
metadata does not select a compiler: use an x64 VS developer shell. A fresh x86
shell configuration was deliberately tested and rejected after compiler detection:
`Expected 8-byte pointers, but compiler produces 4`. Existing Win32 configurations
leave the new guard unset and retain their original behavior.

`win64-vcpkg` adds the normal vcpkg toolchain and explicit `x64-windows` target
and host triplets in its own `build/win64-vcpkg` directory. The manifest baseline
is `9e593bb18ea69cc5095e012465dcd675a822ed0d`; dependencies are zlib/stb, with optional
FFmpeg. There is no custom triplet or vcpkg-configuration file. This preset is a
configuration foundation, not evidence that vcpkg dependencies or the game build
have passed. It cannot solve the DX8 backend or proprietary loader issues.

## Reproduction (no game launch)

Start at the repository root in a VS x64 developer shell:

```powershell
cmake --preset win64
cmake --build --preset win64 --parallel 4 -- -k 20
# Only after a future successful game-test build:
# ctest --preset win64
```

The actual local configure reused only previously fetched dependency **sources**
to avoid redundant network access; no Win32 objects/libraries were reused:

```powershell
cmake --preset win64 `
  -DFETCHCONTENT_SOURCE_DIR_GAMESPY=C:/Users/buskr/Documents/ProjectsX/GeneralsGameCode/build/win32/_deps/gamespy-src `
  -DFETCHCONTENT_SOURCE_DIR_STB=C:/Users/buskr/Documents/ProjectsX/GeneralsGameCode/build/win32/_deps/stb-src `
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=C:/Users/buskr/Documents/ProjectsX/GeneralsGameCode/build/win32/_deps/googletest-src `
  -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
```

Before adding the presets, the same native configuration was attempted with
`cmake -S . -B build/win64 -G "Ninja Multi-Config"`, tests ON, `/W3`, and matching
CRT/debug settings. Configuration succeeded. The bounded full build used
`--parallel 4 -- -k 20`, stopped with exit 2 at progress 1576/4722, and recorded
21 failed commands. These progress numbers include dependency scans, not 1576
successfully compiled source files. After adding the presets, configure again
passed and a full build with `-k 1` again stopped with exit 2 on `d3d8.h`.

Independent native targets were also built successfully from the same graph:
`deps_bink deps_miles deps_dbghelp deps_usp10 liblzhl libzlib gtest gtest_main`,
then `gamespy core_compression`. Static archive member inspection confirmed AMD64
(`0x8664`) for these outputs, plus browserenginewin/browserdispatchwin, wwdebug,
wwsaveload and resources. No x64 game executable linked; no x64 CTest or runtime
validation is claimed. Successful stub libraries do not prove audio/video works.

## Dependency matrix

Classification distinguishes a native static archive from a working subsystem.

| Dependency / route | Classification | Evidence and remaining work |
|---|---|---|
| min-dx8 SDK, pin `7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc` | **Likely blocker; replacement/configuration required** | d3d8/d3dx8/dinput8/dxguid `.lib` members all I386 (`0x14c`); included by root CMake only for 4-byte pointers. Its target also carries x86 `/SAFESEH:NO`. Do not import that link setup into x64. |
| D3D8/D3DX8 headers/backend | **Likely blocker** | WWMath matrix3d/matrix4, WW3D2 PCH and BezierSegment require DX8 headers. D3DX math/helper calls and device types cross engine boundaries. Header availability alone cannot establish a native renderer. |
| Windows SDK DirectInput, GUID, Win32/COM/socket system libs | **x64-ready SDK components** | Installed `um/x64` has dinput8/dxguid. Native availability does not prove the calling code is pointer-safe. No SDK d3d8/d3dx8 library found. |
| Bink loader | **Replacement/configuration required** | `Dependencies/Bink/BinkLoader.cpp` real loading route is `_WIN32 && !_WIN64`; native archive compiles the nonworking fallback. Legacy binkw32 exports/calling conventions are not a native playback solution. |
| Miles loader | **Replacement/configuration required** | `Dependencies/Miles/MilesLoader.cpp` likewise disables loading on Win64. mss32 ABI/callback context fields require an explicit native audio plan; compiling a stub is not that plan. |
| DbgHelp/Usp10 loader adapters | **Source-buildable; native archives proven** | System DLL wrappers compile AMD64. DbgHelp consumers still use incompatible x86 registers, stack frames and callbacks. |
| GameSpy | **Source-buildable; archive proven** | Pinned `07e3d15c500415abc281efb74322ab6d9c857eb8`, native gamespy archive built; Windows socket route. Runtime protocol/auth behavior untested. OpenSSL/voice routes are not enabled or validated. |
| LZHL | **Source-buildable; archive proven** | Pinned `dfd96e2ca64adaddb35dd4ebadd6add7d5586783`; native liblzhl built. No compressed-format compatibility claim from compilation alone. |
| zlib | **Source-buildable; fetched archive proven** | `find_package(ZLIB)` selects ZLIB::ZLIB when available (including vcpkg); otherwise fetched 1.1.4, pin `ac753eee3990a2f592bd4807ff0b30ff572c2104`. This experiment built that fallback Z_PREFIX archive; the vcpkg route was not tested. |
| EAC/RefPack/LZH wrapper code | **Source-buildable with width risks** | Native compression archive built, but huffencode pointer-to-long differences generate real truncation warnings; size_t-to-UnsignedInt buffer lengths also need bounded audits. Archive success is insufficient. |
| stb | **Source-buildable/header-only** | Pinned source (or Stb package) selected; no separate binary ABI. Engine image paths remain blocked upstream. |
| GoogleTest | **Source-buildable; native archives proven** | Pin `063de7e9578f82b369302001269680b4b1553359`; gtest/gtest_main AMD64 archives built using the same compiler/CRT. Full engine-linked Google executables are blocked. |
| STLPort/Utility | **Modern route source/header-buildable** | Modern MSVC STL adapter; old STLPort binaries belong to VC6, not the native preset. VC6-only atomics/assembly matches are not active x64 failures. |
| BrowserEngine/BrowserDispatch COM | **Replacement/ABI configuration required** | Generated IID archives compile AMD64, but both tracked BrowserEngine.dll files are I386. `dx8webbrowser.cpp` passes HWND as long and device through legacy IDL. MIDL commands have no explicit `/env x64`; the generated BrowserEngine header in this shell reports env=Win64, target_arch=AMD64. That does not widen IDL long or prove interface compatibility. |
| WOLAPI tool DLLs | **Likely blocker if tool route needed** | Tracked wolapi.dll/woldbg.dll are I386. Audit actual activation/distribution before choosing replacement; do not classify as a proved core-game link failure. |
| MFC/ATL and developer tools | **Unknown / requires complete build proof** | Native VS components can supply platform libs, but WorldBuilder/GUIEdit/W3D tools share DX8 and pointer-bearing tool IDs. Full native tool build not reached. |
| VC6 MaxSDK / 3ds Max exporter | **Legacy-only, unknown native replacement** | Selected only by IS_VS6_BUILD; not part of this modern preset. Old plugins and host ABI are a separate migration. |
| Optional FFmpeg | **Unknown / requires proof** | Existing alternate video source route; no package configured or native playback tested. Does not make the D3D8 renderer native automatically. |
| Optional Tracy / Google Benchmark / ASAN | **Unknown / requires proof in this tree** | Off in reference and experiment; no native integration validation claimed. Legacy function-level profiler additionally has naked/x86 hooks. |

`git ls-files` found four tracked DLLs above and no tracked `.lib`/`.asm` files.
Fetched SDK libraries were inspected separately. This is not an inventory of all
DLLs in a retail installation, which was neither modified nor used as proof.

## Actual compiler failures and warnings

The initial bounded build logged **109 error occurrences**, not 109 root defects:

| Root family | Observed occurrences | Interpretation |
|---|---:|---|
| Missing DX8 headers | 17 | d3d8types.h 2; d3d8.h 9; d3dx8math.h 6. One dependency/boundary family repeated through both games and PCH/scans. |
| x86 diagnostic register/assembly/DbgHelp assumptions | 91 | Except.cpp 51; debug_except.cpp 24 excluding dialog signature below; debug_stack.cpp 8; debug_debug.cpp 8. Most parser errors follow unsupported asm/register accesses. |
| Native dialog callback ABI | 1 | debug_except.cpp:410 passes BOOL-returning handler to INT_PTR-returning DLGPROC. Distinct pointer-width ABI issue within diagnostic code. |

Thus **three observed compiler root families**, with the first two dominating.
The build was deliberately bounded; this is not an exhaustive native compile
defect count. 43 C2039 register/context diagnostics and 4 C4235 unsupported asm
diagnostics are included in the counts, not added to them. Except.cpp also has
StackWalk64 callback mismatches and a symbol-call cascade. The source still uses
IMAGE_FILE_MACHINE_I386 and 32-bit symbol/address storage.

There were **245 warning occurrences**: C4267 149; C4311 16; C4302 16;
C4312 35; C4477 3; C4313 2; C4473 14; C4018 6; C4535 1; C5287 3.
These mix repeated diagnostics, genuine pointer/length defects, logging formats,
and unrelated warnings. They are not interchangeable with confirmed bugs.
In particular, EAC huffencode.cpp:1053-1054 subtracts pointers after conversion
to Windows `long` (still 32 bits on Win64). The intended offset must eventually
be proved and represented as a bounded pointer difference; no blanket widening
or warning suppression was applied.

Categories not yet exposed as compiler errors include GUI pointer transport,
allocator alignment, persisted IDs and lockstep/FPU behavior. They remain
source-audit risks; absence from this short failed build is not clearance.

## Architecture-risk inventory

Windows x64 uses LLP64: pointers/size_t/ptrdiff_t become 64-bit, while int, long,
DWORD and LONG stay 32-bit. Genuine pointer temporaries and integer gameplay IDs
must be treated differently. Searches covered casts, WinAPI message/user-data
storage, size/offset arithmetic, packing/unions/raw copying, address hashes and
ordering, inline assembly/intrinsics, hooks, serialization and dependency CMake.
Full local search outputs are retained under ignored `build/x64-audit-*.txt`.
Search hits were classified rather than mechanically replaced.

| Area / source | Finding and compatibility boundary |
|---|---|
| GameWindow.h / GadgetListBox.cpp / GadgetComboBox.cpp | WindowMsgData is UnsignedInt, but list/combo callers pass addresses of addInfo/text and selection pointers. This native GUI transport needs a producer/consumer audit, separate from fixed network GameMessage fields. |
| IMEManager.cpp:1441 | Candidate-list byte arithmetic casts base pointer through UnsignedInt before applying offset; real truncation risk. |
| GUIEditProperties.cpp:486, HierarchyView.h:148 | Font pointer encoded as DWORD and pointer-derived hash as UnsignedInt. Use native pointer-sized tool storage only after checking consumers; hash equality/order must be understood. |
| Both WorldBuilder wbview3d.cpp | MapObject pointers cast through Int into DrawableID. DrawableID also has persisted/game consumers: widening the global ID is not a safe mechanical fix. Local tool indirection may be needed. |
| GameMemory.cpp / MemoryPoolSingleBlock | MEM_BOUND_ALIGNMENT=4 and Int-based round-up/stride. Pointer-bearing headers grow, and x64 allocation alignment must be designed/tested (including 16-byte requirements), not guessed by replacing 4 with 8. |
| Dict.h / Dict.cpp | Pointer-sized union-like value storage; Bool/Int/Real alias its bytes. DictPairData uses three unsigned shorts and then `(DictPair*)(this+1)`: alignment/padding of pointer-bearing pairs needs explicit layout proof. Existing size checks do not prove alignment or serialized portability. |
| WinAPI APIs | SetWindowLong/GWL_USERDATA/WNDPROC pointer patterns occur especially in legacy exporter/tools. GWL_STYLE integer access is a different legitimate 32-bit use. LPARAM/WPARAM transport consumers must retain pointer width. |
| Debug/WWLib/StackDump | Active x86 contexts, 32-bit addresses, stack callbacks and asm; both title StackDump.cpp contain IMAGE_FILE_MACHINE_I386 and context assumptions beyond initial reached files. Native unwinding is architectural work, not renaming Eip to Rip. |
| Guarded assembly | intrin_compat/interlocked_adapter VC6 paths and CPUDetect fallback asm are guarded; modern intrinsics exist. matrix3d assembly is under #if 0, quat FastSlerp disabled, vp Intel-compiler assembly guarded. Do not count these as active MSVC x64 errors. WWMath has architecture-dependent math fallbacks worth determinism checks. |
| Optional profile hooks | profile_funclevel.cpp naked _penter/_pleave and x86 hooks need their own architecture gate/design if enabled; profile is OFF here. |
| Address ordering/identity | GameMemory's map<const char*,...,less<const char*>> orders allocation-label addresses; GUI hierarchy hashes pointer values; WorldBuilder persists pointer-derived tool IDs temporarily. No proof that every simulation container is address-independent: audit authoritative iteration/CRC consumers before native lockstep claims. |
| Platform patch/hook search | Legacy diagnostic/instrumentation hooks dominate observed x86 register/address assumptions. No executable patch/hook was introduced or ported here; search coverage does not certify all dormant compatibility paths. |

## Save, replay, network, CRC and deterministic simulation

These interfaces must retain format width independently of host pointer size:

- GameType.h defines ObjectID/DrawableID/FormationID with Int backing on the
  modern route. They are gameplay identities, not general pointer containers.
- Xfer has typed fixed-size scalar/ID transfers and object-pointer lists
  transferred as ObjectIDs. `xferUser` accepts raw byte blocks: each relevant
  enum/aggregate caller still needs layout/padding review. No general claim that
  every save structure is pointer-free follows from inspecting typed transfers.
- Both Recorder.cpp files already use `int32_t replay_time_t`, rather than host
  time_t, and write typed message arguments, fixed IDs and coordinates. Preserve
  those fields/offsets. SYSTEMTIME and other raw structures require byte-level
  fixtures. Replay compatibility has not been demonstrated on an x64 executable.
- GameMessageArgumentType's union contains scalar/ID/coordinate fields, not
  pointers. NetMessageStream copies that union using sizeof; Transport headers
  have packed fixed-width CRC/address/port fields. This is a promising stable
  boundary, not permission to resize messages or serialize native class layouts.
- CRC and XferCRC consume supplied byte ranges. Native padding, stale pointer
  bytes, object ordering and raw aggregate sizes can change results even if the
  source algorithm is untouched. Require format fixtures and per-tick CRC/replay
  comparison before claiming mixed-architecture lockstep.
- Both GameLogic.cpp files explicitly request `_PC_24` and round-to-nearest with
  `_controlfp`. x64 SSE2 arithmetic cannot be assumed to reproduce Win32 x87
  precision behavior just because this source call remains. Validate compiler
  FP settings, transcendental helpers, conversions, SIMD fallbacks, RNG streams,
  tie/expansion ordering and 30 TPS behavior. Do not silently change FP flags.

Required later proof: native layout/size assertions for disk/wire aggregates,
round-trip and known-byte fixtures, same-input RNG/command/CRC traces, cross-build
replay and save/load comparisons, and multiplayer lockstep tests. X64.1 changes
none of these formats and provides no native gameplay compatibility certificate.

## Renderer decision and staging

Root CMake intentionally skips the min-dx8 package for 8-byte pointers. Engine
targets still mention `d3d8lib`; with no target, CMake accepts the bare name as a
library string, so configure success does not prove dependency resolution. The
missing SDK include usage requirements fail compilation first; a bare unresolved
library name would not repair linking later.

Do not re-enable the x86 SDK library directory for x64 or stub out the renderer
to claim a game build. A future boundary task must separate legacy DX8 headers,
D3DX math/helper dependencies and device-facing code, and establish a verified
native graphics contract. D3D11/native backend work must satisfy that contract
before first x64 gameplay unless an explicitly reviewed native DX8-compatible
alternative is proven. Audio/video loaders and browser COM ABI are additional
functional gates. Full renderer implementation is outside X64.1.

## Exact regression validation

All results below were run during X64.1 after the build-system edits:

| Check | Result |
|---|---|
| Win32 configure and full default Release build | PASS, exit 0; both titles, tools and tests (63 incremental actions, not a clean rebuild) |
| CTest `--test-dir build/win32 -C Release --output-on-failure` | PASS 2/2 |
| Direct g_googletest / z_googletest | PASS 132 each; 6 pre-existing disabled each |
| Focused RetailOpenList/PerformanceProfiler/PathProfiler/PathPhaseProfiler/DeveloperHarness | PASS 58 each title |
| Python unittest discovery under scripts/performance | PASS 16, including frozen/reference comparisons |
| PowerShell AST parsing | PASS all 9 script files; no process launch |
| Existing preservation audit | PASS engine/logic/input/UI gated-adapter token checks; 391 previous runtime/evidence files plus final A.2 capture hashes unchanged; index empty at check time |
| Existing preset structural comparison vs baseline | PASS every pre-existing configure/build/test/workflow preset object unchanged |
| Engine/test/script source diff vs baseline | Empty across Core, Generals, GeneralsMD, Dependencies, Tests and scripts |
| Win64 actual preset configure | PASS exit 0, native compiler and pointer size 8 |
| Wrong-shell guard | PASS expected configure rejection for pointer size 4 |
| Independent Win64 libraries | PASS targets above; AMD64 members confirmed statically |
| Win64 full build | Expected FAIL exit 2; DX8 and diagnostic source blockers, no game executable |
| git diff --check | PASS before commit |

No game test was disabled to achieve these results. No runtime validation or
fresh developer runtime candidate is claimed. Existing runtime/capture outputs
were preserved. Build logs, binary-audit/count helpers and search outputs stay
ignored under build and are not part of the commit.

## One recommended next phase

**X64.2 - Native Windows diagnostics and stack-unwind foundation.**

Scope: WWLib Except.cpp, debug_except/debug_stack/debug_debug, their DbgHelp
type/adaptor definitions, and both title StackDump.cpp consumers. Add explicit
native context capture, AMD64 stack unwinding/address formatting and correctly
typed DbgHelp/dialog callbacks while retaining the existing Win32 path. Remove
the four reached diagnostic compilation failures without suppressing errors or
disabling crash diagnostics. Use focused synthetic/native stack-context and
callback tests; no game launch is needed for this bounded platform step.

This is first because 92 observed errors are concentrated in a small diagnostic
subsystem and native fault reporting is needed for subsequent bring-up. It does
not solve the independent renderer gate. Keep renderer boundary/dependency work
as a prerequisite for first native gameplay, followed by focused GUI pointer
transport, allocator/layout, tool ABI and deterministic format/FP validation.
Do not fold these separate designs into X64.2, blanket-widen IDs, or begin D3D11
or Pathfinding 2.0 during that task. X64.2 has not been started.
