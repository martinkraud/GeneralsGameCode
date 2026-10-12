# X64.13: preserve the original late native shutdown exception

Date: 2026-10-12. Accepted baseline: `b79dc70d5c80b305579d3ab0784068f0b5743667`.

## Preserved developer run and conclusion

The exact Generals run is
`build/dev-runtimes/native/Generals/x64-12-final/runs/20261011T2250330638442Z`.
Read directly: launch.json, exit.json and all 254 startup.log records. Its
userdata/CrashDumps directory is empty. No corresponding local WER report or
Application Error event was found. No title was launched during X64.13.

PID26020 started with the strict headless/init-only argv. Preflight passed,
paths matched, there was no timeout and launcher Success was false. Exit code
was -1073741819 / 0xC0000005. There are no fail checkpoints. The final records are:

| Record | Checkpoint |
| --- | --- |
| 246 | engine_init complete, isolated user-data path |
| 247 | init_only complete; execute/replay/simulation skipped |
| 248 | engine_cleanup begin |
| 249-250 | FP complete; control0008001f / MXCSR00001f80 |
| 251 | engine_cleanup complete |
| 252-253 | minidumper_shutdown begin/complete |
| 254 | WinMain complete, code0, detail exit |

This proves arrival at the final checkpoint, not successful return from WinMain
or completion of CRT/process teardown. In X64.12, healthy(), logger close() and
the return still follow that checkpoint. There is no captured exception PC,
register context or stack; **the exact failure location/root cause is not yet
established**. Do not label this an allocator/static-destructor bug from the exit
code alone. No speculative ownership fix or altered shutdown order is justified.

Original staged EXE SHA256:
`443C7CE51D25740D2041327D01BB4966B48D18565F63861FBB7D51E23D9CD898`.
Original PDB SHA256:
`629FDDDCC2C7E801ACD50B3C5EA640C96117C4377ACD15E9143BCA8003BAE55B`.
Both match the launch receipt. Offline DbgHelp loads that staged PDB with GUID
`c7323867-d82e-41f3-b190-20a5da4eae06`, age41, mismatch0 and source lines enabled.
It resolves WinMain at preferred RVA0x4390, shutdownMemoryManager at0xb36c0 and
the global delete[] route at0xb28b0. These are symbol locations, NOT fault RVAs.
The original run's three files were hashed before work and checked unchanged.
Its EXE/PDB, logs and receipts remain available locally, unmodified and ignored.

## Source/linked ownership audit

There is no nontrivial local C++ object awaiting destruction in the final WinMain
return sequence. StartupDiagnostics' state is a zero-initialized POD with no
destructor; close uses CloseHandle. This reduces candidates but does not prove
that return, the stack, or a called API is healthy.

Actual linked symbols include many CRT-registered dynamic atexit destructors:
AsciiString/UnicodeString empty strings, audio event statics, renderer singleton,
temporary buffers, lists, auto-pools and PointerRemapper, among others. The legacy
Debug::StaticExit found in library source did not resolve in this linked PDB;
source presence alone is not proof it participates in this executable's exit.
ClientInstance state is plain handles/flags, not a C++ singleton destructor.

The actual build enables GameMemory.cpp, not the Null allocator. Its pre-main
allocation route intentionally retains the allocator through process exit;
shutdownMemoryManager destroys it only for the other initialization route.
Release global delete/STL deallocation uses TheDynamicMemoryAllocator. Global
string/pool destruction and lock lifetime therefore merit scrutiny if the fault
context points there. The original log does not expose which allocator ownership
branch ran, so a null/freed allocator is only a hypothesis. No flags, allocator
lifetime, pool ownership, critical-section order or engine destruction was changed.

The existing exception filter calls DumpExceptionInfo before MiniDumper handling.
DumpExceptionInfo uses engine AsciiString/global state. After the logged
MiniDumper shutdown, no initialized dumper remains. A late original exception
could be obscured by secondary failure in that filter. An empty dump directory
does not establish which of these paths occurred.

## Minimum additional boundary implemented

`Common/NativeShutdownDiagnostics.h` installs a first-priority VEH only for the
already accepted AMD64 nativeInitOnly gate, after existing cleanup/minidumper
shutdown and immediately before the final WinMain checkpoint. Ordinary startup
and Win32 retain their previous route. No simulation, FP, renderer, resource or
gameplay behavior changes. Existing SEH filter/MiniDumper support is untouched.

The observer preopens CREATE_NEW shutdown-exception.bin in the isolated run root
and logs native_shutdown/armed. It retains this OS handle and the startup.log
handle until process termination. Closing via a newly registered atexit callback
would precede earlier global destructors, recreating the evidence gap.

Only the FIRST access violation or in-page exception is recorded, including on
another thread. An interlocked guard rejects subsequent/reentrant records. The
original EXCEPTION_RECORD and AMD64 CONTEXT are written/flushed FIRST, before
best-effort enrichment: fault allocation/module base and module filename, and
up to16KiB of raw memory at the original RSP via ReadProcessMemory. Fixed POD
storage also records schema version1, structure size, PID/TID and executable
load base. This is bounded raw evidence, not a minidump or a decoded call stack.
Prefix-only/partial records remain useful original-context evidence; missing
stack bytes or module filename must be treated as unavailable, not inferred.

The callback uses Windows APIs and fixed storage; it does not invoke engine
allocation/strings, CRT formatting, DbgHelp, engine locks, new threads, or the
already stopped dumper. It appends a literal native_shutdown/exception trailer
to startup.log (a diagnostic trailer, not a nine-field checkpoint). It always
returns EXCEPTION_CONTINUE_SEARCH. It does not consume an exception, prevent
termination, bypass CRT destruction or convert failure to exit0.

Decode a fault file using the matching native Evidence layout in the header:
verify version1/size and available bytes; ExceptionAddress/RIP and
executableBase give an executable RVA when applicable. Preserve that stage's
EXE/PDB and resolve against its exact GUID/age, NOT the old X64.12 symbols. Raw
stack words are candidates requiring unwind/source validation. Nested record
pointers are raw addresses, not file offsets. The binary contains process memory;
keep it local with the other diagnostic evidence for review.

An armed run without an observed exception leaves an empty binary. The launcher
still requires actual exit0, all existing completion/path criteria and no fail
checkpoint. It additionally requires the armed checkpoint, the empty evidence
file and no late exception trailer, and receipts its exact path/size. A handled
first-chance late AV cannot silently be accepted as clean. File/observer setup
failure exits nonzero. Fail-fast, abrupt termination, faults outside the armed
window, or failures after exception dispatch is unavailable are not covered;
their nonzero exit must still fail the launcher. No claim of a full crash dumper.

## Validation and fresh stages

Final-source validation: both AMD64 titles link; independent native CTest13/13
passes. Gate tests5 include default-off plus real test-only children: normal
post-main exit0/empty file, and an access violation during a CRT exit callback
after main returned. The latter exits0xC0000005 and preserves original PC/context,
write target1, module base/path and stack bytes. A later test exception filter
changes the target to999 and invokes the observer again; the original target1
remains in the single bounded record. These are test executables, not games.

Full Win32 Release build/CTest23, direct149/title (six disabled), focused75/title,
producer5/render2D15 pass; gate4/FP4 on Win32 and FP4 on AMD64 pass. Source contracts8,
native PowerShell guards6/parse5, rendering Python28 and performance Python16 pass.
X64.10 recording/sampling/encoder corpora remain byte-identical (six Win32, three
native CPU streams); existing native device-test skip limitations remain.
Preservation checks cover391 earlier runtime/evidence files and final Stage4A.2
capture, plus the exact X64.12 run and matched EXE/PDB hashes.

Final stages are `build/dev-runtimes/native/{Generals,ZeroHour}/x64-13-final`.
Both preflight against the completed build without launch; imports remain28/title
and external D3DX0/title. Preliminary Generals x64-13-shutdown was staged during
the last rebuild and correctly rejected as changed-build; it is NOT a candidate.
Use only x64-13-final. Old stage/run evidence was never overwritten.

## Exactly one next task

**Developer-reviewed Generals-only init-only diagnostic validation and analysis
of its original late-shutdown context.** Another reviewed Generals run is required
to establish the faulting instruction/root cause; Zero Hour remains held. Do not
rerun automatically, and do not treat this diagnostic implementation as a fix.

After separate developer approval, from the repository in PowerShell7:

```powershell
& ./scripts/native-runtime/Test-NativeInitOnly.ps1 -Title Generals -Name x64-13-final
& ./scripts/native-runtime/Invoke-NativeInitOnly.ps1 -Title Generals -Name x64-13-final -RunReviewedInitOnly
```

No rebuild is needed unless source/build changes; then a fresh stage is required.
Preserve the entire new run directory: launch.json, exit.json, startup.log,
shutdown-exception.bin (even empty), and any userdata/CrashDumps files. Stop after
that one run regardless of outcome. Analyze the original exception against the
new stage's matching EXE/PDB before any targeted ownership fix or further launch.
