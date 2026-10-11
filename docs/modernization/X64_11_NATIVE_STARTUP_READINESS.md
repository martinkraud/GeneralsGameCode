# X64.11: native pre-launch readiness audit

Date: 2026-10-12. Accepted baseline:
`09b2983af689043845a447e7084a15bd5684e747`, `dev/modern-engine`.
Both title sources and their actual AMD64 PE import tables were inspected.
Neither game was launched; no runtime was staged or modified. This checkpoint
adds documentation and a standalone four-test FP audit target only. Production
startup, renderer factory, GameLogic, resource helpers and build FP flags remain
unchanged. X64.10 compatibility remains the specification.

## Decision

**Graphical native launch is blocked.** Both titles select DX8; system AMD64
D3D8 is absent and the existing app-local d3d8.dll is I386. D3D11 is not a
selectable or complete game renderer. Linking the titles does not resolve this.

There is a bounded next investigation route using the **existing headless
startup path**, which skips window/D3D8/input initialization. It needs a native
staging/preflight adapter, flushed startup checkpoints and a deliberate exit
after initialization before entering execute/replay/simulation. Those facilities
are not implemented here; an ordinary `-headless` command is not a bounded smoke
test and no launch command is authorized or supplied by this audit.

## Actual startup sequence, both titles

1. OS loader resolves static imports; CRT initializes globals before WinMain.
   Command-line startup parsing can already occur from static constructors.
   A WinMain log cannot diagnose an earlier loader/static-constructor failure.
2. `Generals[MD]/Code/Main/WinMain.cpp` installs the unhandled exception filter,
   sets string/memory/debug critical-section globals, initializes the memory
   manager and calls `CommandLine::parseCommandLineForStartup`. This creates
   GlobalData, handles startup flags and selects the working directory.
3. Splash bitmap is read opportunistically. With RTS_ENABLE_CRASHDUMP, MiniDumper
   initializes using the current user-data path. Non-headless mode creates the
   HWND; version and ClientInstance/mutex initialization precede GameMain.
4. Each title's `Common/GameMain.cpp` constructs FramePacer and the Win32GameEngine
   factory result; `Win32GameEngine::init` delegates to title `GameEngine::init`.
5. Engine initialization sets up filesystems/archives, loads GlobalData/INI and
   preferences, applies engine-init command-line options, and creates subsystems.
   Audio precedes GameClient. GameClient initializes fonts, display, UI, terrain
   visual and video. GameLogic initialization follows GameClient (after AI).
6. GameLogic::init/reset establishes FP mode. Remaining subsystems, postProcessLoad,
   map-cache work and resetSubsystems/HideControlBar complete initialization.
   GameMain normally enters execute, or replay simulation if requested, then
   destroys FramePacer/engine. Init completion alone does not mean no files changed:
   user directories, map-cache and dump housekeeping can already write files.

Both titles share the device factories and W3DDisplay implementation. Generals
and Zero Hour retain separate GameEngine/GlobalData/GameClient/WinMain bodies.
Zero Hour has localized splash lookup in debug/profile builds; the audited Release
uses the root bitmap, as Generals does. Missing splash is not the renderer blocker.
GameEngine catches ErrorCode/INI/other exceptions and uses release crash reporting;
WinMain also has an empty catch-all. An exception can therefore be caught without
reaching the unhandled SEH filter: checkpoint/catch logging is essential.

## Dependency inventory

The actual native import sets are identical, **28 DLL contracts per title**,
with no delay import directory and no D3D8/D3DX/Bink/Miles/BrowserEngine import:

| Direct imports (required before WinMain) | Local readiness |
| --- | --- |
| KERNEL32, USER32, GDI32, SHELL32, ADVAPI32, ole32, OLEAUT32 | System32 files inspected: AMD64 |
| WINMM, WS2_32, DINPUT8, IMM32, AVIFIL32 | System32 files inspected: AMD64; AVIFIL32 is a native system component despite its name |
| MSVCP140, MSVCP140_ATOMIC_WAIT, VCRUNTIME140, VCRUNTIME140_1 | Installed AMD64 VC runtime files present; required redistributable components, not optional media plugins |
| Twelve api-ms-win-crt contracts below | Windows UCRT/API-set route; not twelve DLLs to copy from a game install. System ucrtbase.dll is AMD64 and native test executables run with the installed CRT |

The twelve UCRT imports are `api-ms-win-crt-{heap,stdio,math,string,filesystem,
convert,time,utility,runtime,multibyte,process,locale}-l1-1-0.dll`.
Static-link dependencies GameSpy, compression, browser IDL/IID archives and GUID
data do not imply similarly named startup DLLs. No claim is made that every
transitive system forwarder or game subsystem was executed by inspecting imports.
Native tests support the installed CRT route; asset and actual game startup remain
untested. Redistributable presence must be rechecked on any different machine.

| Dynamically accessed component | Actual route and classification |
| --- | --- |
| D3D8.DLL / Direct3DCreate8 | Required for graphical startup. Bare-name LoadLibrary in DX8Wrapper; native system file missing. Both Steam and accepted developer runtime copies are machine014c, not8664. Hard graphical blocker; no native replacement supplied. |
| dbghelp.dll | MiniDumper loads System32 first and requires a system instance for dumps. AMD64 system DLL present; X64.2 native diagnostics tests cover address/ABI contracts. Failure disables dumps, not a renderer fallback. |
| usp10.dll | Native system DLL present; compiled loader uses an absolute system path. No Usp10Loader/adapter caller found in current Core/title sources, so not established as a startup requirement. Do not copy old app-local binaries speculatively. |
| mss32.dll / MSS providers | Both titles use MilesAudioManager. Real loader is gated `_WIN32 && !_WIN64`; AMD64 returns failure and neutral wrapper values. openDevice turns audio off and returns. Existing mss32.dll is014c. Missing sound is a known functional limitation, not the observed pre-window loader blocker. Headless selects MilesAudioManagerDummy. |
| binkw32.dll | With FFMPEG OFF in both actual caches, W3DGameClient selects BinkVideoPlayer. AMD64 loader is disabled; init logs failure, BinkOpen returns null and videos cannot play. Existing DLL is014c. Optional for an intro-skipping/headless diagnostic scope; not native media support. |
| BrowserEngine.DLL / FEBrowserEngine2 COM class | ENABLE_EMBEDDED_BROWSER=1. After graphical device creation W3DDisplay calls DX8WebBrowser::Initialize. It tries CoInitialize/CreateInstance, then LoadLibrary/DllRegisterServer on REGDB_E_CLASSNOTREG. False return is ignored by W3DDisplay; browser methods guard a null instance. No native server was validated. Existing long-based COM/window/device conventions are not proven AMD64-safe. Avoid registration/online-browser functionality in first scope. Headless skips this call. The separate TheWebBrowser subsystem initialization is commented out. |
| inetmib1.dll / snmpapi.dll | GameSpyGameInfo network-adapter query path, not the first offline renderer gate. System files are AMD64; no online/network behavior validated. |
| DebugWindow, ParticleEditor, vtuneapi | Debug/editor/profiling paths outside the audited Release startup scope; not required native startup imports. |
| d3d11.dll / dxgi.dll / d3dcompiler_47.dll | Native system files present; isolated D3D11 WARP tests pass. Presence does not make these the game renderer. |

Read-only file inspection covered the accepted `stage4b1-devmode-pathfinding/game`
runtime and protected Steam installation; no files there changed. The inspected
app-local d3d8.dll hash is
`268efe19458c94f8bfa62236d39161bd85e091773a91af989c4a92dccb111126`.
An x86 shim cannot be loaded into an AMD64 process. Do not replace this with
an unverified download or change the installed game to attempt a launch.

### Assets, working directory, staging and user data

An executable and PDB are not a complete runtime: matching title BIG archives,
INI, language/font/UI assets and a correct working directory are still required.
Use isolated copies of legal existing assets, no Install/Steam overwrite. Keep
Generals and Zero Hour manifests distinct. The existing `scripts/zh-runtime`
workflow hashes inventories, verifies PE/PDB GUID+age and backs up shared user data,
but `Common.ps1::Get-ZHBuild` explicitly requires machine0x14c and PE32/0x10b.
It cannot stage AMD64 unchanged and is ZH-specific. Preserve its x86 behavior;
native support must explicitly verify machine0x8664, PE32+/0x20b and correct debug
directory offsets, matched symbols, and app-local DLL architecture/search policy.

Registry reads use KEY_READ without a WOW64 view override. The ZH launcher resolves
Registry32; a native title reads its default view. ZH GlobalData uses registry
UserDataLeafName then the stock Documents fallback. Generals instead builds its
user directory from the INI UserDataLeafName after INI loading. Their backup/log
destinations cannot be assumed identical. Resolve and receipt both actual paths
before any write-capable run, preferably in a dedicated test account. Do not
silently edit registry/view rules as part of staging. Also record the dump path:
Generals' early m_userDataDir can still be empty before INI parsing, making the
existing MiniDumper CrashDumps path relative to the selected working directory.

## Renderer route and headless boundary

```text
GameMain -> Win32GameEngine::init -> title GameEngine::init
 -> W3DGameClient / GameClient::init -> W3DDisplay::init
 -> WW3D::Init -> Create_Render_Backend -> DX8Backend::Create
 -> DX8Wrapper::Init -> LoadLibrary("D3D8.DLL")
 -> GetProcAddress("Direct3DCreate8") -> create interface / enumerate
 -> WW3D::Set_Render_Device -> DX8Wrapper / real IDirect3DDevice8
```

`Backend/DX8Backend.cpp::Create_Render_Backend` always selects DX8Backend.
No supported native `-renderer d3d11` switch/factory branch exists. D3D11.1/.2
cover independent sentence/ordinary UI texture rendering with explicit unsupported
states, no full depth/world/terrain/shadow/water route. WW3D::Set_Render_Device
still calls DX8Wrapper directly; much world code bypasses IRenderBackend. Merely
changing the factory would leave direct DX8 resources and device calls invalid.
Do not implement that substitution or a renderer port in this audit.

Missing DLL/export/interface makes DX8 Init return false, WW3D initialization fail
and W3DDisplay throw ERROR_INVALID_D3D. Later 32/16-bit/safe-resolution device
retries only help after interface creation; they cannot supply a missing runtime.

`-headless` is parsed before HWND creation, disables intro/sizzle, and skips the
W3DDisplay WW3D::Init/Set_Render_Device/browser blocks. Keyboard initialization
is skipped; mouse/window/audio have dummy paths. W3DDisplay, fonts, image metadata,
terrain-visual and video objects still exist; this is not a new null renderer.
Terrain visual has explicit partial headless initialization. The branch is a
source-supported diagnostic route, **not proof of successful native initialization**.
Headless normal execution is also not a playable renderer or a cross-architecture
replay test. First diagnostic scope must exclude replay, map/game/network, developer
scenario and benchmark flags and stop before execute/simulation.

## AMD64 floating-point decision

Both title `GameLogic/System/GameLogic.cpp::setFPMode` bodies are identical:
`_fpreset`, `_statusfp`, RC_NEAR and PC_24 via `_controlfp(..., _MCW_PC|_MCW_RC)`.
The comment mentions CHOP, but that assignment is commented out: **actual policy
is nearest-even**. Call sites include init, reset, clearGameData, loading/transition
draw loops, update and getCRC; ScoreScreen also restores it. This is not solely a
one-time startup setting. Renderer/media/library calls can disturb thread FP state.

Focused test `NativeStartupFPTest.cpp` invokes that exact control sequence without
launching/linking a title. Installed Release UCRT **10.0.26100.9444**, VCRUNTIME
**14.51.36247.0** produced:

| Probe | Win32 | AMD64 |
| --- | --- | --- |
| Existing policy control word | 0x000a001f (PC_24) | 0x0008001f (precision bits absent) |
| Existing policy MXCSR | 0x00001f80 | 0x00001f80 |
| Invalid parameter callbacks / errno | 0 / 0 | 0 / 0 |

**The installed Release CRT ignores the unsupported AMD64 precision request.**
It did not abort or set EINVAL. Do not report an observed invalid-parameter startup
blocker. This does not guarantee behavior with another/debug CRT; the unsupported
legacy probe skips AMD64 Debug to avoid assertion UI. The proposed supported policy
and arithmetic tests are separate. No production FP call was changed in X64.11.

Decision for a future native startup implementation:

- Retain the exact Win32 policy. On AMD64 explicitly reset state, request only
  supported nearest rounding and masked exceptions, and establish MXCSR=0x1f80
  for relevant bits (clear accrued flags, FTZ and DAZ). Use control-word queries,
  not status flags, when checking policy. Check errors; do not rely on ignored PC bits.
- Retain gradual underflow; enabling FTZ/DAZ would change small-value behavior
  relative to the legacy x87 route. Apply/check on the actual simulation thread,
  retain existing restoration boundaries, and log unexpected foreign-call drift.
- Do not impose global /fp:fast, FMA contraction, reassociation or new conversion
  rules. Review compiler code generation and precision at compatibility boundaries.
  The focused proposal is built /fp:strict; this does not change the game's flags.
- Do not force all resource arithmetic to PC_24: X64.10 explicitly preserves
  Win32 caller precision and the AMD64 accepted PC_53 numerical corpus. Keep that
  helper contract separate from simulation policy.

Tests prove supported reset, idempotence, nearest-even half-ULP behavior, preserved
subnormal input/output and a precision counterexample: `(16777216+1)-16777216`
is0 with binary32 operations/Win32 x87 PC_24 and1 with binary64/x87 PC_53.
SSE2 single operations naturally round to binary32, but double expressions,
library calls, spills/order and conversion choices are not controlled by PC_24.
**No FP startup register setting alone proves x86/x64 simulation equivalence.**
Future replay/CRC/lockstep and save-load acceptance needs matched deterministic
simulation evidence; first init-only smoke must not claim it. Multiplayer and
cross-architecture replay compatibility remain unvalidated.

## Minimum first-launch diagnostics, not implemented here

Default-off, bounded startup-only checkpoints must be available in Release
without enabling cheats or replacing the profiler. Use an explicit absolute path
in a fresh isolated run directory. An early logger must not need GlobalData,
engine strings, the pool allocator or initialized debug critical sections.
Write complete bounded records and flush at every stage (`WriteFile` then
`FlushFileBuffers`, or an equivalently checked/flushed file sink). Preserve failure
codes immediately, before formatting/another API overwrites GetLastError.

| Checkpoint pair (enter/complete/fail or explicit skipped) | Minimum fields |
| --- | --- |
| External preflight / process creation / exit | Title, exe/PDB GUID+age and hashes, PE machine, asset/runtime manifest, exact argv/CWD, user-data/dump paths, loader/preflight result, exit code and timeout |
| WinMain entry / exception setup / memory init | Ordered sequence, UTC + QPC, PID/TID, build/compiler/architecture, initialized stage |
| Startup parse / GlobalData / working directory | Actual flags/headless state and paths; no ambiguous derived-only receipt |
| MiniDumper / HWND or headless skip / instance guard | Initialized or disabled reason, actual dump directory, failure code |
| Filesystem / archives / INI / preferences | Begin/end and named failing asset/subsystem; actual user-data path after INI |
| Each subsystem init / postProcessLoad / reset | Existing subsystem name and phase; GameClient nested font/display/UI/terrain/video checkpoints where it does not use initSubsystem |
| Renderer, if a later graphical scope is approved | Factory choice; LoadLibrary path/result/GetLastError, export resolution, interface/device HRESULT, adapter/caps/mode attempts; explicit skip in headless |
| GameLogic FP / initialization complete | Control query result and MXCSR on the current thread; log before/after policy and foreign-call restoration |
| Deliberate stop / cleanup / normal exit | Reached boundary, result code, begin/end teardown; no execute/replay/ticks in init-only scope |

Log typed startup catches and the WinMain catch-all; do not depend only on SEH
crash handling. A launcher must distinguish no WinMain checkpoint (loader/CRT/
early-static failure) from a named in-process failure, retain OS error/exit code
and allow debugger/loader diagnostics if needed. A watchdog bounds a hung
diagnostic child and records that it timed out; it must never kill unrelated games.
No per-cell/tick tracing or new performance profiler is needed.

Existing DEBUG_LOG flushes each record when DEBUG_LOGGING is compiled and initialized,
but both audited Release caches use RTS_DEBUG_LOGGING=DEFAULT with RTS_DEBUG off.
Those macros cannot be assumed present. DebugInit can itself parse command line
and initialize ClientInstance, making it unsuitable as the only earliest sink.
Existing performance CSV and launch receipts do not identify these startup stages.

Crash/minidump support **already exists**: RTS_CRASHDUMP_ENABLE=ON in both caches,
WinMain's SEH filter, release crash reporting, system DbgHelp and MiniDumper's
worker, full CONTEXT and minimal/full dumps. X64.2 platform tests validate native
ABI/signatures/address handling. No new crash framework is required. No deliberate
native crash/dump generation was tested here; initialization, directory permissions,
symbol pairing and caught-exception coverage must be logged. The existing dump
worker waits indefinitely, another reason the first child needs an external bound.
Pre-WinMain failures and fail-fast/assert paths are not guaranteed to reach it.

## Validation and preservation

- Full Win32 Release graph PASS, CTest **22/22**; direct **149/title** with six
  existing disabled/title; focused **75/title**, producer5 and render2D15 PASS.
- New FP target **4/4 Win32 and 4/4 AMD64**. Native independent CTest **12/12**;
  the resource target retains three CPU passes and three native-device skips.
- Both actual native title build/link targets rerun successfully, exit0; external
  D3DX count stays0/title. Unrelated native tool failures remain the X64.10 inventory;
  no tool source changed and no broad tool rebuild is claimed in this audit.
- Rendering Python28, performance Python16, PowerShell parse9/zero errors,
  preservation audit and diff whitespace PASS. All six Win32 and three native CPU
  resource streams remain byte-identical; compact corpora unchanged. The audit
  preserves391 prior evidence/runtime files and final A.2 capture hashes.
- Logs/probes under ignored `build/x64-11-*`: dependencies, fp-native/fp-win32,
  win32-build/ctest/direct/focused, native-build/ctest, rendering/performance-python,
  resource-compare and preservation. Only test/CMake and documentation are committed.

Reproduce the new probe in the appropriate configured VS environment with
`cmake --build build/win32 --config Release --target native_startup_fp_test` or
`cmake --build --preset win64 --target native_startup_fp_test`, then run that test
via CTest `-R '^native_startup_fp_test$'`. These launch test executables, never games.
Native baseline suite uses `ctest --test-dir build/win64 -C Release
-E '^[zg]_googletest$' --output-on-failure`; full native title GoogleTests are not
claimed as built/run. Resource tests' hidden device windows are not game launches.

## Exactly one recommended next task

**X64.12: prepare the bounded native headless startup diagnostic gate.** Implement
only the opt-in Release startup checkpoints, explicit AMD64 FP startup branch
validated above, native dual-title staging/preflight and init-only exit boundary.
Retain Win32/default behavior, protect actual per-title user data, do not select
a different renderer, start a match, resume benchmarks or claim determinism.
Prepare a concrete guarded developer procedure; first actual native game process
validation requires developer review. Graphical runtime remains blocked on a
separate renderer plan; that implementation is not folded into this next task.
