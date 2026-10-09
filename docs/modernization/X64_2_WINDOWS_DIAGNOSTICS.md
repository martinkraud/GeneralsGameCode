# X64.2 - Native Windows diagnostics and callback ABI

Date: 2026-10-09. Baseline: `0267c2975f0531668eca9b47aa94b80cf0f84a30`.

## Result

Baseline HEAD, branch `dev/modern-engine`, tracking `origin/dev/modern-engine`,
clean working tree and synchronization were verified before edits. The baseline
Win64 build failures were reproduced before changing code.

**The 92 reproduced diagnostic/callback error occurrences are removed.** Actual
native WWLib/debug/DbgHelp libraries compile, the independent platform test
executable links and passes, and both enabled title StackDump bodies compile on
x86 and AMD64. The full Win64 game build still fails on DX8 headers. No native
game, engine-linked Google test or full tool build is claimed to link.

No gameplay algorithm, pathfinding, renderer, persisted ID, save/network/replay
format or timing policy changed. No game was launched. No Install target, Steam
write, new runtime candidate, D3D11 work or X64.3 implementation was performed.

## Reproduction and exact build progression

Environment remains VS Community 18 / MSVC 19.51.36260.0, Windows SDK
10.0.26100.0, CMake 4.3.1-msvc1, Ninja Multi-Config, Release, existing CRT/options.
Win32 reference and Win64 experiment use separate build directories as in
[X64.1](X64_1_BUILD_FOUNDATION.md).

Both bounded full native attempts used `cmake --build --preset win64 --parallel 4
-- -k 20` from the x64 developer environment, with both games/tools/tests enabled.

| Attempt | Error occurrences | Failed commands / final progress | Roots |
|---|---:|---|---|
| X64.2 baseline reproduction | 110 | 22; 32/3169 incremental actions | 18 DX8 header failures + 91 x86 diagnostic failures/cascades + 1 dialog ABI failure |
| Final X64.2 source | 23 | 23; 46/3175 incremental actions | All C1083: d3d8types.h 2, d3d8.h 15, d3dx8math.h 6 |

The original 92 diagnostic occurrences were Except.cpp 51, debug_except.cpp 25
(including the dialog mismatch), debug_stack.cpp 8, debug_debug.cpp 8. They
included missing Eip/Esp/Ebp/Eax/etc. and FloatSave members, four unsupported asm
diagnostics, incompatible StackWalk64 callbacks and parser cascades. X64.1's
initial full attempt had 109 errors rather than this reproduction's 110 because
one more DX8 scan failed before Ninja stopped here. Parallel `-k` attempts can
finish additional failing commands in flight. These counts are observations,
not exhaustive counts of independent defects or a percentage of port completion.
Progress includes dependency scans and cached work, not compiled-source totals.

The final full build still exits 2, **outcome C: engine compilation begins but
dependency/source blockers remain**. No diagnostics failure is hidden behind
the DX8 stop: `core_wwlib core_debug deps_dbghelp windows_diagnostics_test` were
built explicitly and passed on the exact final native source. No engine target
was removed from the full graph, and no renderer stub or x86 library was linked.

## API boundary and old assumptions corrected

`Dependencies/Utility/Utility/windows_diagnostics.h` contains the small shared
runtime-address/context boundary. It supports x86 and AMD64 explicitly; other
Win64 architectures are rejected rather than silently treated as AMD64.

| Contract | Win32 reference | Native AMD64 |
|---|---|---|
| Runtime address | ULONG_PTR, 32 bits | ULONG_PTR, 64 bits |
| DbgHelp address / symbol displacement | DWORD | DWORD64 |
| Source line number / line displacement | DWORD | DWORD (still 32 bits) |
| Machine | IMAGE_FILE_MACHINE_I386 | IMAGE_FILE_MACHINE_AMD64 |
| Instruction / stack / frame | Eip / Esp / Ebp | Rip / Rsp / Rbp |
| Stack structure / APIs | Existing STACKFRAME / StackWalk / Sym* | SDK-native STACKFRAME64 / StackWalk64 / Sym*64 aliases |
| Current context capture | Existing x86 inline register capture | RtlCaptureContext at the walking call site |
| Walk context parameter | Existing null contract retained | Full mutable local context copy |
| Registers/FPU logging | Existing x86 register/FLOATING_SAVE_AREA branch | Native general registers, FltSave/MXCSR and raw x87/XMM payloads |

The AMD64 caller captures context in its own function, not in a helper that
returns with an obsolete stack frame. Supplied exception contexts are copied
before walking. Frame initialization sets all three offsets and flat modes.
The existing frame-skip/address-selection policy is retained. WWLib's supplied
output capacity is now also enforced when a context is provided: the old loop
could emit N+1 entries into an N-entry array.

The Windows SDK maps legacy function/type tokens to their native forms. Dynamic
resolution must expand that mapping before stringification, otherwise a native
prototype can accidentally load the 32-bit-address export. The new shared export
name macro does this; all affected loaders resolve typed function-pointer slots.
WWLib's old writes through assumed adjacent unsigned-long globals and the debug
loader's unsigned function-pointer union are removed. WWLib's misspelled
`SymGetModuleBaseType` export is corrected to the actual API. Win64 WWLib loads
DbgHelp; Win32 retains its ImageHlp route.

The debug symbol handler now uses GetCurrentProcess consistently instead of
casting a numeric process ID to HANDLE. The main DbgHelp loader keeps its
existing synchronization/lifetime behavior. This is not a unification or
concurrency redesign of the three legacy diagnostic systems.

Module bases, symbol offsets, stack signatures, runtime frame/log-group hash keys,
exception return addresses, access-violation addresses, stack words and function
details now retain pointer width. Symbol storage is aligned to IMAGEHLP_SYMBOL,
and SizeOfStruct identifies the structure rather than the whole name buffer.
Address buffers/formatting retain all native digits; line offsets stay separate.
Runtime-only diagnostic signatures and their in-tree callers were updated
together. No serialized identity is widened. The debug signature ordering still
compares complete addresses from the bottom of the stack as before.

## Callback ABI and other crash paths

`ExceptionDlgProc` now has the API contract `INT_PTR CALLBACK (HWND, UINT,
WPARAM, LPARAM)`. Message parameters/calling convention were already correct;
BOOL was the wrong native return width. The similar, clearly identified
`debug_dlg` test-program DialogProc is corrected too. Both source bodies were
compiled on x86 and AMD64. EnumThreadWndProc remains BOOL, as required by its
different callback contract; exception filters remain LONG. Unrelated window,
gameplay and UI callbacks were not mass-converted.

MiniDumper's full CONTEXT/EXCEPTION_RECORD copies, DWORD process/thread IDs,
DWORD WINAPI thread callback and MiniDumpWriteDump contract were audited; none
requires widening. MiniDumpWriteDump's wrapper signature is checked against the
real SDK in the new platform tests. No deliberate crash or dump-file test ran.

Primary contracts were checked against the installed SDK and Microsoft:
[StackWalk64](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-stackwalk64),
[RtlCaptureContext](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlcapturecontext),
and [DLGPROC](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-dlgproc).

## Tests and final validation

The independent `windows_diagnostics_test` target links the real DbgHelp loader
and GoogleTest, without a game or renderer. It does not replace the normal
engine-linked test targets. Its five tests cover full-width synthetic contexts,
flat frame initialization/nonmutation, zero/high-bit address formatting,
unloaded-API failure behavior, real native exports/module/symbol addresses,
DWORD line-offset output canaries, and a real current-process stack walk.
Compile-time assertions compare ten wrapper function types directly to the SDK,
including callback/return/displacement types, and verify address width/alignment.

| Final check actually run | Result |
|---|---|
| Full Win32 Release configure/build | PASS both games, tools and all tests; incremental build |
| Win32 CTest | PASS 3/3, including the new platform test |
| Direct engine-linked Google tests | PASS 132/title; 6 existing disabled/title |
| Focused RetailOpenList/PerformanceProfiler/PathProfiler/PathPhaseProfiler/DeveloperHarness | PASS 58/title |
| Python/reference discovery | PASS 16 |
| Native diagnostic targets + platform test build | PASS |
| Native platform CTest | PASS 1/1 target, five tests |
| Actual enabled title StackDump sources | PASS both titles, x86 and AMD64 |
| Similar debug_dlg source compile | PASS x86 and AMD64 |
| Static machine checks | Platform test PE and title probe COFF objects: I386 on Win32, AMD64 on Win64 |
| Evidence preservation audit | PASS 391 prior runtime/evidence files and final A.2 capture hashes; accepted gated-adapter token checks; empty index when checked |
| PowerShell AST | PASS all 9 scripts, unchanged |
| git diff --check | PASS before commit |

For explicit title-body compilation, response files were derived from the actual
Release compile_commands entries: same source/defines/include graph, `/Y-` to
parse actual headers instead of consuming a PCH, removal of the build-generated
module-map response reference (these units use no imports), and
`/DIG_DEBUG_STACKTRACE=1` to enable the conditional body. No stand-in headers,
extra legacy DX8 include/library paths or altered retained build configuration
were used. Outputs stayed under ignored build. This proves compilation, not
execution of every legacy crash handler or equivalence of exact crash traces.
Game-linked x64 tests, native game execution, live SEH/dialog interactions,
dump generation and VC6/MinGW/ARM64 validation are not claimed.

Representative local logs: `build/x64-2-before.log`, `x64-2-after-final.log`,
`x64-2-native-final-validation.log`, `x64-2-win32-final-validation.log`,
`x64-2-stack-x86-final.log`, `x64-2-stack-x64-final.log`, direct/focused/Python
and preservation logs. Logs, response files, probes and all captures remain
ignored/uncommitted.

## Exact changed-file inventory

```text
Core/Libraries/Source/WWVegas/WWLib/Except.cpp
Core/Libraries/Source/WWVegas/WWLib/Except.h
Core/Libraries/Source/debug/debug_debug.cpp
Core/Libraries/Source/debug/debug_debug.h
Core/Libraries/Source/debug/debug_dlg/debug_dlg.cpp
Core/Libraries/Source/debug/debug_except.cpp
Core/Libraries/Source/debug/debug_stack.cpp
Core/Libraries/Source/debug/debug_stack.h
Core/Libraries/Source/debug/debug_stack.inl
Dependencies/DbgHelp/DbgHelpLoader.cpp
Dependencies/DbgHelp/DbgHelpLoader.h
Dependencies/Utility/CMakeLists.txt
Dependencies/Utility/Utility/windows_diagnostics.h
Generals/Code/GameEngine/Include/Common/StackDump.h
Generals/Code/GameEngine/Source/Common/System/StackDump.cpp
GeneralsMD/Code/GameEngine/Include/Common/StackDump.h
GeneralsMD/Code/GameEngine/Source/Common/System/StackDump.cpp
Tests/Google/Dependencies/CMakeLists.txt
Tests/Google/Dependencies/Utility/windows_diagnostics_test.cpp
docs/modernization/AGENT_HANDOFF.md
docs/modernization/ROADMAP.md
docs/modernization/X64.md
docs/modernization/X64_2_WINDOWS_DIAGNOSTICS.md
```

## Remaining blockers and one recommended next task

**X64.3 - Separate legacy DX8 header/math requirements from x86 binary linkage.**

Bounded scope: audit and separate the existing min-dx8 header usage requirements
from its I386 libraries and x86 linker flags; identify D3DX math/helper consumers
and define the native backend dependency contract. Preserve the full graph and
Win32 renderer behavior. Use real headers/native compile probes to advance
WWMath/engine compilation and classify newly exposed source defects. Do not link
I386 libraries into AMD64, add renderer stubs, or silently disable components.
No full D3D11 implementation is part of this recommended boundary task.

This follows the only root family left in the bounded full compile. Native
renderer/backend dependency work is still required before first x64 gameplay.
The earlier GUI pointer transport, allocator/layout, proprietary audio/video,
browser ABI and format/FP lockstep gates remain. New diagnostic-target build
coverage also exposes WWLib thread.cpp's stored unsigned-long thread handle to
HANDLE conversions, plus ordinary size_t-to-int/count warnings. Those are
classified follow-up risks, not fixed or hidden here. The enabled title probes
report existing AsciiString/UnicodeString/matrix header size-conversion warnings;
no direct diagnostic address truncation error remains in the validated paths.

No cross-architecture save/replay/multiplayer/CRC compatibility is inferred from
this platform work. X64.3 has not begun; Pathfinding 2.0 remains backlog and
Stage4A.3 remains paused.
