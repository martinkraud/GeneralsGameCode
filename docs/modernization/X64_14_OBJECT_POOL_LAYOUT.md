# X64.14: pointer-width-correct ObjectPoolClass block layout

Date: 2026-10-12. Baseline: `31731578679faa80d5be07d7a93fb56466e12192`.

## Reviewed fault and bounded correction

The separately reviewed Generals X64.13 run is preserved at
`build/dev-runtimes/native/Generals/x64-13-final/runs/20261012T0721055932956Z`.
launch.json, exit.json, all startup.log records, shutdown-exception.bin and the
exact staged EXE/PDB were analyzed directly. CrashDumps was empty. Preflight and
paths passed; no timeout. Initialization, init-only stop, cleanup, MiniDumper
shutdown and WinMain's final checkpoint completed, but actual exit was0xC0000005.

Evidence version1/size18080 is complete; only1440 raw stack bytes were readable.
PID14740/TID19760, RIP0x7FF6E48C5E15, RSP0x5D042FFA60, module/executable base
0x7FF6E43E0000, RVA0x4E5E15. The matching age43 PDB, GUID
`c7323867-d82e-41f3-b190-20a5da4eae06`, resolves the fault to the dynamic atexit
destructor for `AutoPoolClass<MultiListNodeClass,256>::Allocator`, offset0x15.
There is no direct line record for that generated wrapper; disassembly maps its
loop to `ObjectPoolClass::~ObjectPoolClass`, mempool.h's next-block read.

The instruction is `mov rbx,qword ptr [rax]`; RAX/RBX contain the noncanonical
corrupted link0x0240606400000000. The exception is a read; its reported address
parameter is0xFFFFFFFFFFFFFFFF, which must not be silently equated to RAX.
Chained PE unwind metadata validates saved RBX at RSP+0x20, undoing the0x28 stack
allocation and popping caller0x7FF803E69C85. Deeper system frames are not fully
validated without a loaded-module map. A supporting raw return address resolves
to `__scrt_common_main_seh` immediately after `exit(main_result)`; CRT source
places this after invoke_main returns. This is post-WinMain atexit destruction.

The concrete layout defect is in the same live pool instantiation: allocation
reserves sizeof(uint32*) bytes for a block-link pointer and stores that pointer,
but `(BlockListHead + 1)` advances a uint32* by only4 bytes. Exact old binary code
writes a qword header at RVA0x492AD0 and advances the payload by4 at0x492ADA.
On AMD64, header bytes4..7 overlap the first object's/free-list's pointer write.
This structurally corrupts block traversal; it is not an inferred null global
allocator or speculative shutdown ownership change. The precise historical
write producing the final corrupt value is outside the captured heap evidence.

Production change: one expression in `Core/Libraries/Source/WWVegas/WWLib/mempool.h`:

```cpp
FreeListHead = (T*)((unsigned char *)BlockListHead + sizeof(uint32 *));
```

Offline inspection of the final Generals AMD64 EXE/PDB (matching age45) confirms
`add r9,8` at RVA0x492ADA, replacing the old `add r9,4`, with the same0x2808
block allocation request for MultiListNodeClass,256. Neither EXE was executed
during implementation; the separate reviewed Generals run is recorded below.

The advance now matches the stored pointer:4 bytes on Win32,8 on AMD64. Existing
allocation size, pointer fields, block capacity, ascending fresh-node order,
LIFO reuse, locking, counters, ownership, block links and destructor are unchanged.
No allocator rewrite, skipped destructor, changed startup/shutdown order,
launcher success relaxation, renderer, simulation or gameplay change.

Reachable pooled types were audited: MultiListNode, GenericSLNode, AAB-tree
node/link, grid link, sound AudibleInfo, renderer task records, animation combo
and camera shaker use ordinary pointer/float/integer alignment. The offset is
compatible with their required4-byte Win32/at-most8-byte AMD64 alignment and
their sizeof(T) stride. There is no new over-aligned-type allocator contract.
In particular, retaining the exact4-byte Win32 offset is intentional.

## Focused dual-architecture coverage

`object_pool_test` compiles the actual WWLib template and actual MultiListNode
definition, not a replacement algorithm. It is an independent CPU-only CTest
target using the existing GoogleTest infrastructure. Its seven tests cover:

- byte-level reproduction of the old4-byte offset overlap for8-byte pointers,
  absence of overlap for4-byte pointers, and pointer-width-correct non-overlap;
- exact first/subsequent node offsets, pointer alignment, capacity/free counters
  and header integrity while writing every byte of actual MultiList nodes;
- four blocks, preserved backward links and exact reverse-order release of each
  block through the unchanged production destructor;
- cross-block LIFO reuse without capacity growth or changed links;
- 4-byte-aligned objects with explicit legacy Win32 offset4 / AMD64 offset8;
- constructor/destructor ownership and reuse through Allocate_Object/Free_Object;
- the exact static `AutoPoolClass<MultiListNodeClass,256>` instantiation with513
  live allocations, three blocks, reuse and clean process exit through its CRT
  static destructor after test main returns.

An allocation boundary confined to the test executable records released watched
block addresses. It adds no production hooks and links no game allocator. Tests
return all live objects before pool destruction; they do not suppress a destructor.
The static-pool test is accepted only if the actual test process exits with code0.

## Validation and preserved evidence

Final exact-source validation passed:

- Full Win32 Release build and CTest24/24; direct GoogleTest149/title (six disabled),
  focused75/title, producer5 and render2D15.
- Both native AMD64 title links PASS. Independent native CTest14/14; established
  full-title GoogleTest exclusions retained. FP4 and gate5 on AMD64, FP4 and gate4
  on Win32 remain green. Native resource device-dependent skips remain unchanged.
- Focused object_pool_test7/7 on each architecture, including actual process exit0
  after the static256-node pool destructor. Generated CTest timeout30s verified
  on both architectures. Test executables were run; neither game was launched.
- Source contracts8, rendering Python28, performance Python16, native PowerShell
  guards6 and parse5. Static startup imports28/title and external D3DX0/title.
- Accepted resource streams byte-identical: six Win32 and three AMD64 CPU streams.
  Prior391 runtime/evidence files and final Stage4A.2 capture hashes preserved,
  plus both exact reviewed X64.12/X64.13 runs and original matched symbols.

Fresh final native stages at
`build/dev-runtimes/native/{Generals,ZeroHour}/x64-14-final` both pass actual
EXE/PDB identity/hash, asset-inventory and startup-dependency preflight. They were
created after the final native rebuild completed; older stages were not changed.
No additional diagnostics or launcher edits were needed. All runtime/capture,
analysis helper, build/test log and generated corpus files remain ignored.

Delivery is one X64.14 source/test/documentation commit on dev/modern-engine,
with push restricted to origin/dev/modern-engine. The exact hash is available in
git log and the developer report. Runtime evidence is separate from these
static/test gates; neither establishes simulation determinism.

## Reviewed Generals native runtime confirmation (2026-10-12)

Implementation baseline: `53f08fbdc0c12302e96839c186c8bf887391c829`, already
pushed to origin/dev/modern-engine. The developer separately executed the single
reviewed Generals run; this analysis launched neither title and made no source
change. Evidence was read directly from:
`build/dev-runtimes/native/Generals/x64-14-final/runs/20261012T0830000434440Z`.

**Verified PASS:** exit.json records PID30248 started, actual process ExitCode0,
TimedOut=false, Error=null. Receipt interval is08:30:00.0609571Z to
08:30:01.8386252Z (includes launcher work, not an exact process duration).
The complete startup.log contains255 ordinary nine-field checkpoint records,
sequential1..255, all PID30248/TID17816, with zero status codes. All75 lifecycle
begin records excluding the diagnostic opening have a later matching complete.

| Checkpoint | Evidence |
| --- | --- |
| 10 | HWND_DX8_browser_input skipped: existing headless route |
| 246 | engine_init complete after subsystem init, postProcessLoadAll, map_cache, initial_reset and HideControlBar |
| 247 | init_only complete: execute/replay/simulation skipped |
| 251 | engine_cleanup complete |
| 253 | minidumper_shutdown complete |
| 254 | native_shutdown armed: return/CRT/process teardown; first AV or in-page error |
| 255 | WinMain complete / exit |

The late-shutdown observer remained intact: shutdown-exception.bin exists and
is exactly0 bytes. All log bytes parse as the255 normal checkpoints; there is
no late exception trailer. userdata/CrashDumps exists and contains no files.
Actual exit0 independently closes the prior distinction between WinMain's final
checkpoint and the process result. No late fault was observed in this run;
empty evidence does not itself test crash-dump generation or prove every possible
exception type is covered by the observer.

Identity was checked against launch.json, manifest.json and actual staged files:

- AMD64 PE32+ EXE/PDB GUID `c7323867-d82e-41f3-b190-20a5da4eae06`, age45.
- EXE SHA256 `6A51B5D5EACAB0561E08ED372AA546115FC1A30E744A36C0B452094D0EC7EDB8`.
- PDB SHA256 `DACAD6B01218AAC46C51E3AC3163C678E9AD7CD87843C63EC0AAC156F45A9313`.
- Actual module is x64-14-final/game/generalsv.exe; working directory is that
  stage's game directory. The isolated userdata and CrashDumps paths resolve
  beneath this exact run, consistently in launch/exit receipts and startup.log.
- Arguments are exactly -headless -nativeInitOnly followed by this absolute run
  directory. Read-only Test-NativeInitOnly for Generals/x64-14-final reran PASS,
  validating GUID/age pairing, current-build and staged hashes, inventory/assets
  and startup dependencies. It launches no game process.

All run files and the staged EXE/PDB were hash-checked unchanged after this
analysis/documentation update. They remain local, ignored and uncommitted.
No rebuild or regression rerun was needed for this documentation-only review;
the implementation validation above remains the previously executed suite.

Supported milestone: Generals native AMD64 completed the controlled headless
initialization, init-only boundary, engine cleanup and normal process exit.
The captured X64.13 post-WinMain corruption fault did not recur after the bounded
pool layout fix. One successful scenario does not prove universal allocator or
shutdown correctness.

At the Generals-only review, Zero Hour had not yet executed this gate. No native game
renderer, execute/replay/simulation, gameplay, save/load, multiplayer, performance
or cross-architecture lockstep/determinism is validated here. The established
native D3D8 renderer blocker and optional audio/video/browser limitations remain;
D3D11 is not made the game backend. Default registry/Documents paths were not
exercised because this gate isolates user data. Crash-dump generation remains
untested. No additional blocker is observed in this Generals init-only route.

## Reviewed Zero Hour confirmation and combined milestone (2026-10-12)

The separately reviewed Zero Hour run was analyzed directly at
`build/dev-runtimes/native/ZeroHour/x64-14-final/runs/20261012T0837057620774Z`.
exit.json records PID35832 started, actual ExitCode0, TimedOut=false and Error=null.
The launch/exit receipt interval is08:37:05.7758773Z to08:37:08.0373802Z, including
launcher work. All302 normal nine-field log records are sequential, PID35832 /
TID27084, with zero status codes and increasing QPC values. All77 lifecycle begins
excluding diagnostic opening have a later corresponding completion. No late
exception trailer exists. shutdown-exception.bin exists and is exactly0 bytes;
userdata/CrashDumps exists and is empty.

| Boundary | Generals checkpoint | Zero Hour checkpoint |
| --- | --- | --- |
| engine_init complete | 246 | 293 |
| init_only complete: execute/replay/simulation skipped | 247 | 294 |
| engine_cleanup complete | 251 | 298 |
| minidumper_shutdown complete | 253 | 300 |
| native_shutdown armed | 254 | 301 |
| WinMain complete / exit | 255 | 302 |
| Actual process exit code | 0 | 0 |

Zero Hour additionally resolves its staged ZH_Generals base-assets path at
checkpoint20. Its title-specific ChallengeGenerals and SnowManager initialization
checkpoints complete. Different checkpoint counts reflect distinct title startup
paths; they are not a performance comparison. Both runs explicitly skip the
HWND/DX8/browser/input route at checkpoint10 and stop before execute/replay/simulation.

Actual staged Zero Hour identity matches launch.json and manifest.json:

- AMD64 PE32+ EXE/PDB GUID `7a285453-4110-4cad-a6c8-e5336ba19b1c`, age46.
- EXE SHA256 `B99114E99981A465277AFAE6B3D22D03A2BA9B1982CECC32F7A4D2416DF43467`.
- PDB SHA256 `312BF41FB8D160CAEA70BDC7758844124E6B38645BD58A44542F9AAF1C6AC4E4`.
- Module is ZeroHour/x64-14-final/game/generalszh.exe and CWD is its game directory.
  Isolated userdata/CrashDumps and diagnostic paths match this exact run. Base
  assets resolve to the stage's game/ZH_Generals directory. Launch/exit receipts
  and log agree independently; arguments are exactly -headless -nativeInitOnly
  followed by this absolute run directory.
- Read-only Test-NativeInitOnly for ZeroHour/x64-14-final reran PASS: actual
  EXE/PDB GUID/age pairing, build/stage hashes, inventory/assets and startup DLLs.
  No game process is launched by that preflight. Both reviewed runs and their
  staged EXE/PDB files were hash-checked unchanged after documentation edits.

**Combined milestone: BOTH Generals and Zero Hour have completed controlled
native AMD64 headless initialization, cleanup and normal process exit.** The
Generals post-WinMain fault did not recur, and the equivalent Zero Hour lifecycle
also exited normally with its late observer armed and empty evidence. There is
no observed startup/shutdown blocker within either reviewed init-only route.

Claims remain bounded to these two runs, their staged assets and isolated paths.
Native graphical startup/renderer, execute/replay/simulation, gameplay, save/load,
multiplayer, performance and determinism remain unvalidated. The missing native
D3D8 route, incomplete/nonselectable D3D11 game backend, and optional native
media/browser limitations remain. Default registry/Documents routes and actual
crash-dump generation were not tested. One clean run per title does not prove
all allocator/shutdown paths correct.

This review is documentation-only, on top of the accepted Generals milestone
commit (25b314c4e). No source changes, rebuild, regression-suite rerun, game launch,
staging, commit or push were performed. git diff --check passed. Generated
runtime evidence and the ignored preservation manifest remain uncommitted.

## Exactly one next task

**Native graphical renderer-route feasibility audit**, for developer review:
define the smallest testable route past the AMD64 D3D8 blocker using accepted
renderer boundaries and existing D3D11 slices. Identify missing startup, device
and presentation capabilities and compatibility risks before proposing bounded
implementation. Do not assume the existing D3D11 slice can run the game, replace
an installed DLL speculatively, or launch either title during that audit.
This next task has not begun.
