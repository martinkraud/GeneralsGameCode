# Agent handoff

## X64.15 graphical renderer-route audit complete (2026-10-12)

The combined Generals/Zero Hour X64.14 headless init-only milestone was finalized
and pushed in documentation commit1c82677fbbf8a6b55c13ad1e550d6d148b1728cd before
this audit. Both reviewed runs/symbols remain unchanged. No game was launched.
Actual source/factory and current AMD64 imports still select DX8: missing system
AMD64 D3D8 blocks ordinary graphical startup. Modern libraries occur in both
link inputs, but their imports are absent from the actual titles; no active
D3D11 backend is inferred from successful linkage.

D3D11 already owns device/context, windowed swap chain/backbuffer/RTV, viewport,
color-clear/begin/end/present, resize and bounded sentence/normal-alpha file-UI
rendering. Existing isolated tests pass4/4 per architecture; rendering Python28
passes. Hidden WARP presentation is not visible hardware or game-backend proof.
Factory switching alone leaves DX8 device/caps/resolution, depth clear, texture/
sorting/shader manager/cursor and teardown assumptions. Shell/menu is a separate
integration gate; do not enter execute/update to obtain a message loop.

See [X64_15_GRAPHICAL_ROUTE_FEASIBILITY.md](X64_15_GRAPHICAL_ROUTE_FEASIBILITY.md)
for exact call paths, blocker inventory, reusable/missing capabilities and gate
acceptance. This is documentation-only: no renderer/source changes, rebuild or
native link rerun. Existing Win32/resource/FP contracts and strict init-only
success remain binding. Rendering/gameplay/simulation/save/replay/multiplayer,
performance and determinism are not validated by the headless milestones.

Exactly one recommended next task: **X64.16 shared opt-in native window/device/
clear-present diagnostic gate** inside both title processes after verified
headless init, before execute/replay/simulation. Reuse D3D11Backend directly in
a shared bounded diagnostic host, without selecting it as the W3DDisplay game
backend. Add flushed phases, truthful Present/occlusion/error receipts, checked
window/pump and teardown, modern dependency preflight and strict separate mode.
Validate without launching either title and prepare fresh stages for separate
developer-reviewed execution. No shell/menu, world rendering or generic D3D8
emulation in this slice. This recommendation has not been implemented; no game
launch is authorized by the audit. Earlier next-step entries are historical.


## Both titles: reviewed native AMD64 headless init-only PASS (2026-10-12)

The separately reviewed Zero Hour run is now verified directly:
`build/dev-runtimes/native/ZeroHour/x64-14-final/runs/20261012T0837057620774Z`.
PID35832 actually exited0; no timeout or launcher error. All302 sequential
checkpoints are intact; all77 lifecycle begins excluding diagnostic opening
completed. Engine init293, init-only294 (execute/replay/simulation skipped),
cleanup298, MiniDumper shutdown300, late observer armed301 and WinMain302 passed.
shutdown-exception.bin is0 bytes; no late exception trailer or crash dump exists.
Module/CWD/isolated user-data/dump/base-assets paths match launch.json, exit.json
and startup.log. Staged EXE/PDB hashes match launch and stage manifests; GUID
7a285453-4110-4cad-a6c8-e5336ba19b1c/age46 matches the reviewed build. Read-only
Zero Hour stage preflight rerun PASS; both reviewed runs/symbols remain unchanged.

Together with Generals run `20261012T0830000434440Z` (PID30248,255 checkpoints,
actual exit0), BOTH titles have completed controlled native AMD64 headless
initialization, cleanup and normal process exit. This supersedes older pending
runtime entries below. It does NOT validate graphical startup, renderer,
execute/replay/simulation, gameplay, save/load, multiplayer, performance or
cross-architecture determinism. The native D3D8 dependency remains a graphical
blocker; the existing D3D11 slice is not a complete/selectable game backend.
Optional native audio/video/browser functionality, default user-data paths and
crash-dump generation remain unvalidated. No blocker was observed in either
reviewed init-only route; one pass per title is not universal shutdown proof.

This review changes documentation only. No rebuild, game launch, source change,
commit or push; generated evidence stays ignored/uncommitted. See
[X64_14_OBJECT_POOL_LAYOUT.md](X64_14_OBJECT_POOL_LAYOUT.md) for exact identities
and per-title evidence. Earlier validation counts describe prior executed tests.

The previously recommended native graphical renderer-route feasibility audit
is now complete as X64.15 above; its X64.16 recommendation is the current next
task. Neither title was launched and no renderer implementation was started.

## X64.14 ObjectPoolClass layout fix (2026-10-12)

Baseline: `31731578679faa80d5be07d7a93fb56466e12192`.
The reviewed Generals X64.13 run `20261012T0721055932956Z` established the
post-WinMain AV at RVA0x4e5e15 in the MultiListNodeClass,256 pool's CRT atexit
destructor. Matching age43 PDB/context/disassembly show corrupted block-chain
traversal. Header stores8 bytes on AMD64, but old uint32*+1 payload offset was4,
overlapping the link. This closes the earlier unresolved-cause entry below.
The original run, fault record and matched EXE/PDB remain preserved and ignored.

Production change is one mempool.h expression: byte advance by sizeof(uint32*)
when selecting the payload, giving8 on AMD64 and exactly4 on Win32. Existing
allocation size, counts/capacity, fresh-node and reuse order, links, locking,
object ownership and destruction remain. No allocator/teardown rewrite,
destructor suppression, gameplay/rendering/FP change or relaxed success gate.
Reachable pooled types' pointer/float/integer alignment is retained.

Seven real-template pool tests cover old overlap structurally, corrected
non-overlap/alignment, block links, four-block reverse-order release, LIFO reuse,
exact Win32 offset/order and construction ownership. The exact static
MultiListNodeClass,256 AutoPool is exercised across513 nodes/three blocks and
must exit0 through its actual CRT destructor. Test allocator observation is
confined to a standalone CPU test executable; no production hooks were added.

Validation PASS: full Win32 Release/CTest24, direct149/title (six disabled),
focused75/title, producer5/render2D15; both AMD64 links and independent CTest14
(existing native full-title test exclusions/device skips retained), pool7 per
architecture including clean CRT exit, FP4 per architecture and startup gate5
AMD64/gate4 Win32. Source8, PS6/parse5, Python28/16 and all resource/evidence
preservation checks pass. D3DX0/title and28 startup imports/title remain.
Both fresh build/dev-runtimes/native/{Generals,ZeroHour}/x64-14-final stages
preflight against the completed native build, with matching PDBs/hashes/assets.
Delivery: one coherent X64.14 source/test/docs commit on dev/modern-engine;
push only to origin/dev/modern-engine. All runtime/evidence/analysis/build outputs
remain ignored and uncommitted. Obtain the exact commit hash from git log.

No title was launched during X64.14 implementation. The separately reviewed
Generals runtime confirmation is now verified PASS (2026-10-12), from preserved
run build/dev-runtimes/native/Generals/x64-14-final/runs/20261012T0830000434440Z.
PID30248 actually exited0, without timeout/error. All255 sequential checkpoints
are intact; all75 lifecycle begins (excluding diagnostic opening) completed.
Engine init246, init-only247 (execute/replay/simulation skipped), cleanup251,
MiniDumper shutdown253, late observer armed254 and WinMain complete255 passed.
shutdown-exception.bin is0 bytes, no late exception trailer is present and
userdata/CrashDumps is empty. Module/CWD/user-data/dump paths match the receipts.
Actual staged EXE/PDB hashes match launch.json and manifest; GUID
c7323867-d82e-41f3-b190-20a5da4eae06/age45 matches the reviewed X64.14 build.
Read-only Generals stage preflight rerun PASS; run and symbols preserved unchanged.
The original post-WinMain fault did not recur in this controlled run.

This establishes Generals AMD64 headless initialization, cleanup and normal
process exit for this stage. It does not validate rendering, execute/replay,
simulation, gameplay, save/load, multiplayer or cross-architecture determinism.
At that Generals review, Zero Hour remained unexecuted; native game-renderer limits
remain. See [X64_14_OBJECT_POOL_LAYOUT.md](X64_14_OBJECT_POOL_LAYOUT.md) for exact
evidence, identity, limitations and the next guarded procedure.

Git baseline: 53f08fbdc0c12302e96839c186c8bf887391c829, already pushed to
origin/dev/modern-engine. This follow-up changes documentation only; it does not
rebuild, launch, commit or push. Generated evidence remains ignored/uncommitted.
The previously recommended Zero Hour-only reviewed init-only confirmation is
now complete and verified PASS; the current next task is specified above.
Neither title may be launched automatically during evidence review.


## X64.13 late native shutdown diagnostic boundary (2026-10-12)

The developer-reviewed Generals X64.12 run
`20261011T2250330638442Z` reached all254 checkpoints, including init-only,
engine cleanup, MiniDumper shutdown and WinMain complete, but actually exited
0xC0000005. No timeout; paths matched; no dump/fault address/stack was captured.
The exact fault/root cause is UNRESOLVED. Earlier entries saying neither title
was launched describe the prior implementation stage, not current runtime state.
Zero Hour has still not been run. The original run and matching EXE/PDB are preserved.

X64.13 adds only an opt-in AMD64 end-of-WinMain one-shot VEH evidence boundary:
preopened shutdown-exception.bin, original exception/context flushed before
best-effort module/16KiB raw stack capture, and a startup-log trailer. It retains
OS diagnostic handles until process exit, returns CONTINUE_SEARCH and leaves
existing ownership/shutdown/SEH/MiniDumper behavior unchanged. Win32/default route,
FP, resource compatibility and gameplay are preserved. This is additional
localization, NOT a proven fix. Launcher still requires actual exit0 and all
completion/path checks, plus an armed checkpoint and no recorded late exception.

Final Win32 Release/CTest23, direct149/title, focused75/title, producer5/render2D15,
native links/independent CTest13, FP4/architecture, gate5 AMD64/gate4 Win32, source8,
PS6/parse5, Python28/16 and prior corpus/evidence preservation checks pass.
Both fresh `build/dev-runtimes/native/{Generals,ZeroHour}/x64-13-final` stages
preflight; D3DX0/title,28 static imports/title. No title launched in X64.13.
All evidence/runtime/build/analysis outputs remain ignored and uncommitted.
See [X64_13_NATIVE_SHUTDOWN_DIAGNOSTICS.md](X64_13_NATIVE_SHUTDOWN_DIAGNOSTICS.md)
for the audit, limitations, validation and exact separately reviewed procedure.

Exactly one next task: **developer-reviewed Generals-only init-only diagnostic
validation and original late-shutdown context analysis**. Preserve the entire
new run and matched symbols; stop after one run regardless of outcome. Keep
Zero Hour held. Do not infer an allocator/destructor fix before the fault context.


## X64.12 native init-only gate implemented (2026-10-12)

Accepted baseline: `4a36a0cd2c2e3b2bc7fa6e579dd206aeaa7619d3`.
Current checkpoint implements the X64.11 preparation gate; earlier next-task
entries below are historical. See [X64_12_NATIVE_INIT_ONLY_GATE.md](X64_12_NATIVE_INIT_ONLY_GATE.md)
for the exact guarded PowerShell procedure. Neither title has been launched.

The sole diagnostic argv is `-headless -nativeInitOnly <absolute-run-directory>`.
Strict grammar excludes all scenario/replay/map/benchmark flags and refuses the
native option on Win32. Configuration occurs before GlobalData/static-startup
parsing, independently of engine allocation/logging. CREATE_NEW startup.log uses
bounded buffers,2048 records and WriteFile/FlushFileBuffers at each checkpoint;
write/capacity failures fail the result. Stages include WinMain/module/CWD/memory,
subsystems and nested GameClient init, actual dump path/status, initial reset,
FP, init-only stop and teardown. Typed/WinMain catches are logged; diagnostic
init errors cannot be silently accepted. Existing SEH/MiniDumper remains enabled.

Both titles use isolated run/userdata/ and CrashDumps before their first path
access/write. Ordinary Win32/default INI/registry paths are preserved. ZH's
Win32BIGFileSystem also uses staged ZH_Generals base archives only for this gate;
ordinary registry lookup, archive precedence and overwrite policy are unchanged.
The launcher receipts/verifies actual module, CWD, user-data, dump and ZH base path.
Default native Documents/registry behavior is not validated by this isolated run.

Both AMD64 setFPMode implementations use the shared supported nearest/masked/
gradual-underflow helper (no FTZ/DAZ, no unsupported precision request). Exact
Win32 body, restoration boundaries, compiler flags and X64.10 resource numerical
contracts remain. FP tests now call that production helper. Determinism/replay/
lockstep/save compatibility is not established by FP registers or init-only tests.

Native dual-title scripts are separate from the x86 ZH workflow. They verify
8664/PE32+, PDB GUID+age/hash, title/base assets, full staged inventory and native
startup imports; reject app-local DLL overlays and changed artifacts. Final real
stages: build/dev-runtimes/native/{Generals,ZeroHour}/x64-12-final. Both preflights
pass. Earlier x64-12-init-only stages are preliminary evidence, not final candidates.
The guarded launcher requires -RunReviewedInitOnly, starts one hidden child,
bounds it to120seconds, records launch/exit/log paths and never reruns or starts
another title automatically. It exits after init and normal cleanup before
execute/replay/simulation. First actual execution remains separately reviewed.

Validation PASS: full Win32 Release/CTest23, direct149/title (six disabled),
focused75/title, producer5/render2D15; native title links and independent CTest13;
FP4 and gate3 tests/architecture, source contracts7, PS guards6/parse5, Python28/16,
resource streams/corpora preservation,391 prior evidence/runtime files plus A.2
hashes and whitespace. Native D3D8 device tests skip3 remains. Generated artifacts
ignored; this checkpoint is one intended source/test/script/doc commit pushed only
to origin/dev/modern-engine. No renderer switch, benchmark resumption or game run.

Exactly one next task: **developer-reviewed first native init-only validation for
Generals and Zero Hour**, using the report procedure and analyzing the first
failure before any rerun/further execution. Graphical DX8 remains blocked; D3D11
is still an independent UI slice, not a game renderer.


## X64.11 native pre-launch audit complete (2026-10-12)

Baseline: `09b2983af689043845a447e7084a15bd5684e747`. This audit changes only
standalone FP tests/CMake and documentation. Production helpers, startup, FP,
renderer and gameplay remain unchanged. No game launched or runtime staged.
Native titles still link; external D3DX count remains0/title. Earlier next-step
entries below are historical. See [X64_11_NATIVE_STARTUP_READINESS.md](X64_11_NATIVE_STARTUP_READINESS.md).

Graphical startup is BLOCKED: factory always selects DX8; System32 AMD64 d3d8.dll
is absent; inspected Steam/developer d3d8.dll is014c. D3D11 is an isolated UI/
sentence slice, not a selectable game renderer. Both title PE import sets have
28 contracts; native system/VC runtime components are present. Bink/Miles loading
is disabled on AMD64, with existing no-video/audio-off behavior; no native media
support. Browser COM initialization/registration is still attempted after graphical
device startup, optional/failing gracefully but not proven native-safe. Headless
skips graphics/browser/input and uses dummy audio/mouse/window paths, while still
initializing W3D metadata/fonts/terrain/video/subsystems. No native init success
or simulation/replay/lockstep equivalence has been established.

The exact existing setFPMode probe shows Release UCRT ignores unsupported AMD64
PC bits: invalid callbacks0/errno0, MXCSR0x1f80, control0x8001f; Win32 PC_24
control0xa001f. Do not claim an observed invalid-parameter crash. Proposed AMD64
policy: supported nearest-even, masked exceptions, gradual underflow, no FTZ/DAZ,
explicit thread/restoration boundaries. Test-only proposal passes reset/idempotence,
tie and subnormal tests. PC_24 cannot enforce x87-equivalent SSE2/double expressions;
production policy change and matched deterministic evidence remain future work.
X64.10 resource PC_53 contract must remain separate.

First startup requires default-off flushed Release checkpoints before engine
logging is available, subsystem begin/end/fail/skip, typed/WinMain catches, FP,
actual DLL path/API failures and cleanup/exit. Existing native minidump support
is enabled; no new crash framework needed, actual dump generation remains untested.
The ZH staging adapter explicitly accepts x86 PE32 only; do not bypass its guard.
Native staging must handle PE32+/matching PDB and per-title assets/user data.
ZH registry uses native default view whereas current launcher uses Registry32;
Generals user-data leaf comes from INI. Receipt actual paths and dump location.

Validation: full Win32 Release/CTest22, direct149/title (six existing disabled),
focused75/title, producer5/render2D15; AMD64 independent CTest12; new FP4/architecture;
both title native links exit0. Python28/16, PS parse9 and preservation audit PASS.
Six Win32/three native CPU resource streams byte-identical; native resource device
skip3 remains. No broad native tool rebuild claimed; X64.10 blockers unchanged.
Eight-file audit/test/documentation checkpoint, authorized for one commit and
push only to origin/dev/modern-engine. Generated logs/probes/captures remain ignored.

Exactly one next task: **X64.12 bounded native headless startup diagnostic gate**:
opt-in flushed checkpoints, tested AMD64 FP branch, native dual-title staging/
preflight and init-only exit before execute/replay/simulation. Prepare a developer
procedure for review; no automatic game launch, renderer port or tool repair.


## X64.10 resource migration complete (2026-10-11)

Current checkpoint supersedes historical next-step entries below. From accepted
`9afa8cfac0ae78d4ec3d3838e9574b172b68e0c2`, engine-owned replacements now serve
the final file/filter/surface resource helpers. External D3DX resource count is
**3 -> 0/title**. Actual Generals and Zero Hour AMD64 title link probes both
exit **0**, with dumpbin confirming x64 machine type and no D3DX imports.
There are no new native title linker blockers. DX8 remains default; no game launched.

The complete accepted X64.9/X64.9B/C/D contract remains binding: packed fields,
BOX/TRIANGLE arithmetic and edge policy, exact DXT RGB/alpha decisions and 8/7
coefficient, float codec handoff, periodic small blocks, authored mips, file/RLE
quarantine, staging/access/failure ownership and aliases. Do not substitute a
generic codec or broaden unsupported flags/palettes. Win32 retains caller x87
precision; production encoder tests match the pinned encoder under PC_53 and
the game's PC_24. AMD64 matches the accepted PC_53 corpus; simulation precision
equivalence is not claimed. Old unequal-extent NONE prose is clarified by the
unchanged reference bytes: uncovered output is zero-filled before conversion.

Validation: full Win32 Release/CTest **21/21**, both resource suites **10/10**,
direct **149/title** (six existing disabled), focused **75/title**, producer5/
render2D15, AMD64 independent CTest **11/11**, Python rendering28/performance16,
PowerShell parse9 and preservation audit PASS. Six Win32 and three native CPU
evidence streams are byte-identical to X64.9; all compact corpora unchanged.
Native resource results are three CPU passes and three device skips (no system
AMD64 D3D8). Unfiltered native CTest's two unbuilt title tests were Not Run;
only the established independent native targets are claimed as tested.

Full native graph still fails in tools: broad34 compiler diagnostics; fixed
probe exactly the same30 (C2065=8/C2440=6/C2555=4/C2664=8/C2737=4). No tool fixes.
Successful title linkage does not establish a usable native renderer/audio/video
or safe FP startup. Existing PC_24 setup needs an AMD64 policy review.

This is the coherent nineteen-file X64.10 implementation/test/documentation
checkpoint, authorized for commit and push only to `origin/dev/modern-engine`.
Generated evidence/logs/scripts, captures, binaries and runtime copies remain
ignored and preserved. See [X64_10_RESOURCE_COMPATIBILITY.md](X64_10_RESOURCE_COMPATIBILITY.md)
for exact implementation, validation commands, logs, limits and native results.
Stage4A.3 remains paused; no performance optimization work was resumed.

Exactly one next task for developer review: **X64.11 native pre-launch readiness
audit**, bounding both titles' renderer/dependency startup and FP setup before
proposing a safe first-runtime-validation scope. Do not launch automatically,
begin tool ABI repair or infer lockstep compatibility from the resource tests.

## X64.9D numerical convergence complete (2026-10-11)

The independent test-only RGB/DXT5 numerical checker closes **13/13 compressed
gates and 9/9 overall X64.9 readiness gates** for the frozen production contract.
It reproduces endpoint refinement, projection/rounding/ties, degenerate exits,
six/eight-step alpha decisions, x87 binary32 store order and the pinned 8/7
coefficient. Production resource helpers remain unchanged; DX8 remains default.

All 1,710 existing direct encoder records and eighteen small-block extensions
are predicted independently. One fixed 32-block validation set adds 96 complete
encodings, all matching; all four RGB orientations and both DXT5 modes occur.
The conventional-coefficient negative control differs on eighteen blocks.
No lookup of reference outputs occurs in the numerical model. No further broad
fixture expansion was needed. Earlier OPEN entries below are historical.

Final validation: full Win32 Release build, CTest20/20 (resource target 8/8),
direct149/title (six existing disabled/title), focused75/title, native independent
CTest10/10, rendering Python28/28, performance Python16/16, PowerShell parser9,
and preservation audit PASS. Six earlier evidence streams and all three compact
reference corpora are unchanged. Native title link diagnostics still contain
exactly the same three D3DX helpers/title; the fixed tool probe retains the exact
30-error multiset. Native full-graph failure remains expected, not resolved.

This checkpoint is the combined twelve-file X64.9 characterization commit,
`X64.9: characterize legacy resource conversion and filtering`, based on
`3283218029fea5eb5e1785d6220a661d5d965815`, on `dev/modern-engine`.
Only these tests, compact references, CMake test registration and modernization
documentation belong in the commit; generated evidence/logs/runtimes remain
ignored. Commit/push to origin/dev/modern-engine is authorized after validation.
No game was launched. See the final X64.9D section in
[X64_9_RESOURCE_CHARACTERIZATION.md](X64_9_RESOURCE_CHARACTERIZATION.md).

Exactly one recommended next task for developer review: **X64.10, implement and
migrate the final bounded resource-helper group** (D3DXCreateTextureFromFileExA,
D3DXFilterTexture, D3DXLoadSurfaceFromSurface) against this accumulated contract.
Preserve file/RLE quarantine disposition, authored mip handling, packed fields,
float filter handoff, exact DXT decisions, access/staging failure cleanup and
snapshot-equivalent aliasing. Do not substitute a generic compressor or silently
reassociate x87 arithmetic. No characterization blocker remains within this
frozen matrix; production implementation and existing tool ABI failures remain.
X64.10 is a recommendation only and was not started. Stage4A.3 stays paused.

## X64.9C compressed encoder continuation (2026-10-11, review pending)

Readiness remains **OPEN**. The pinned encoder route, RGB preconditioning,
DXT1 transparency, periodic small-mip extension and float filter handoff are
bounded. A test-only independent DXT3 alpha checker matches 762 inputs. New
ignored evidence contains 1,899 CPU records and 171/171 real-device agreements;
128 compact encoder references are exercised. Earlier 3,010/371 and 334/380
streams and 58/94 references remain byte-identical.

Exact RGB endpoint refinement/color-selector ties and DXT5 alpha fitting/ties
still lack an independent numerical reproduction. Compressed gate items
1/3/5/7/8/9/13 remain NO; overall readiness 5/9 remains NO. Next task: finish
only that numerical decision checker in the same characterization harness,
reusing the existing cases and pinned tables (including pD8's 8/7 anomaly).
Do not migrate helpers or start X64.10. See the X64.9C sections in
[X64_9_RESOURCE_CHARACTERIZATION.md](X64_9_RESOURCE_CHARACTERIZATION.md).

Full Win32 Release/CTest20, direct149/title, focused75/title, native CTest10,
Python28/16, parser9 and preservation checks pass. Three unresolved D3DX helpers
per title are unchanged. The fixed baseline tool-target probe reproduces the
exact 30-error multiset; the parallel full graph additionally reached two
unchanged Generals WorldBuilder objects (four extra ABI errors), documented in
the report. No tool ABI fix or native-link progress. All eleven combined files
remain unstaged/uncommitted; no push or game launch. Earlier entries are
historical checkpoints; DX8 remains default.


## X64.9B sampling/access continuation (2026-10-11, review pending)

The original X64.9 evidence/assertions are preserved byte-for-byte. Added 334
CPU records, 94 compact references and 380 real-device/access records. Supported
surface cases agree 294/294; sixteen DXT3/5 compressed mip levels also agree.
BOX dispatch/odd-edge trimming, general TRIANGLE/2D float accumulation and native
packed interaction are now bounded. Scaled owned/standalone/back-buffer access,
allocation/readback/upload failures and snapshot-equivalent aliases were tested.

**Readiness remains OPEN: required exact compressed re-encoding is not bounded.**
Compressed SurfaceClass copies/mips are reachable; a generic DXT compressor is
not an acceptable substitute. Next task: inspect only the required pinned
D3DXEncodeDXT1/3/5 decisions and add distinguishing block fixtures in the same
harness. Do not start X64.10 or migrate any helper. Full accepted regressions
pass; native three-helper/title and 30-error tool multisets are unchanged.
See [X64_9_RESOURCE_CHARACTERIZATION.md](X64_9_RESOURCE_CHARACTERIZATION.md),
X64.9B sections, for confidence levels, exact findings and nine readiness answers.
All ten combined files remain unstaged/uncommitted; no push or game launch.
Earlier checkpoints below are historical; DX8 remains default.

## X64.9 resource characterization (2026-10-11, review pending)

No production D3DX helper was migrated. The pinned Win32 observer/device suite
now records 3,010 CPU rows and 371 device rows; 58 compact exact CPU references
and exhaustive packed expansion checks pass. All 108 matching surface copies
and 40 readable mip levels agree between CPU recording and the real device.
The remaining file/filter/surface helpers and exact native/tool diagnostics are
unchanged. Full Win32 Release/CTest20, direct149/title, focused75/title,
native CTest10, Python28/16, parser9 and preservation audit pass. No game launch.

**Readiness decision C: additional characterization before migration.** Exact
general BOX/TRIANGLE dispatch, edge and mixed-channel rounding rules, relevant
compressed conversion and scaled/non-lockable staging boundaries remain open.
Next task: complete that contract using the existing pinned oracle; do not begin
X64.10 implementation. The isolated RLE decoder heap-corruption observation is
explicitly quarantined, not treated as a proven production requirement.
See [X64_9_RESOURCE_CHARACTERIZATION.md](X64_9_RESOURCE_CHARACTERIZATION.md) for
caller classification, exact bytes/rules, device restrictions and validation.
X64.9 is unstaged and uncommitted because its conditional acceptance gate has
not passed. Older checkpoints below are historical; DX8 remains default,
Stage4A.3 remains PAUSED and Pathfinding 2.0 remains backlog.

## X64.8 native CPU diagnostic compatibility (2026-10-11)

Both DX8Wrapper error loggers now use engine-owned pinned-reference error text.
All 32 original names, exact lowercase eight-digit fallback, S_OK returns and
buffer behavior match the actual Win32 helper. The 256-byte production buffers,
logging/assert distinctions and surrounding wrapper behavior are unchanged.
1,264 literal output cases pass on Win32/AMD64; live Win32 comparison also
covers the complete D3D HRESULT low-word facility and seeded truncation cases.
No resource helper, gameplay or renderer implementation changed; no game launch.

Native probes remove only D3DXGetErrorStringA: 4 -> 3 helpers/title, combined
LNK2019 8 -> 6, LNK2001=8, LNK1120=2. Neither game links; exact 30 separate tool
ABI errors are unchanged. Full Win32 Release/CTest19, direct149/title,
focused75/title and native CTest10 pass, including prior math/FVF/texture/shader,
Bezier, RB1/RB2/RB3 and D3D11.1/.2 coverage. Native CPU binary has Windows/CRT
imports only. See [X64_8_ERROR_STRING.md](X64_8_ERROR_STRING.md) for the measured
contract, validation, exact remaining helper inventory and resource evidence plan.

**Next proposed task: X64.9 - characterization-only of the coherent file/filter/
surface resource group.** Three helpers remain, but pixel conversion, sampling,
mip, compressed-format and device-access semantics are not yet bounded by
allocation or single-mip UI work. No combined migration or next-stage probe
has started. Pathfinding 2.0 remains backlog; Stage4A.3 remains PAUSED.

## X64.7 fixed legacy shader bytecode (2026-10-11, historical checkpoint)

The four immutable pixel shaders (three water ps.1.1, one profiler ps.1.4)
now use exact pinned-D3DX8 token arrays, including assembler comments and END.
Actual original source bytes are compile-time identity-checked and compared
exactly in independent recorded/live reference tests. Real Win32 DX8 creation
passed for all four through both routes. Runtime assembly/buffer allocation is
retired; device creation/failures, handles, reset/release and profiler gates are
preserved. Both titles' actual profiler-enabled TU also compiles on Win32/AMD64.
No D3D11 migration or game launch occurred; DX8 remains default.

Native probes remove only D3DXAssembleShader: 5 -> 4 helpers/title, combined
LNK2019 10 -> 8, LNK2001=8, LNK1120=2. Neither game links; the exact 30 separate
tool ABI errors are unchanged. Full Win32 Release, CTest17, direct149/title,
focused75/title and native CTest9 pass, including prior texture/FVF/math,
Bezier, RB1/RB2/RB3 and D3D11.1/.2 coverage. Native CPU proof does not establish
AMD64 DX8 shader execution or whole-game visual equivalence.
See [X64_7_FIXED_SHADER_BYTECODE.md](X64_7_FIXED_SHADER_BYTECODE.md) for exact
sources/tokens, reference/device evidence, contract change and remaining risks.

**Next proposed task: X64.8 - pinned-reference CPU diagnostic error-string
compatibility for D3DXGetErrorStringA.** The file/filter/surface group remains
coherent but allocation work does not bound its uncharacterized conversion,
filtering and file semantics. No next task has begun. Pathfinding 2.0 remains
backlog; Stage4A.3 remains PAUSED. Older checkpoints are historical.

## X64.6 DX8 texture creation compatibility (2026-10-11, historical checkpoint)

The ten 2D/cube/volume creation calls now use engine-owned LegacyD3DX policy
and actual DX8 COM creation. The pinned Win32 library and replacement match
145,755 controlled full transcripts; 90 literal transcripts also pass natively.
Real system Win32 DX8 validation ran without skipping: 108 2D, 108 cube and 81
volume successful pairs match all mip descriptors, alongside matching failures.
Measured defaults, format fallback/query ordering, caps, mip and output ownership
rules are retained. Wrapper invalidation/retries and all other resource routes
remain unchanged. DX8 remains default; no game was launched.

Native probes remove exactly three helpers/title: 8 -> 5; combined LNK2019
16 -> 10, LNK2001 remains 8, LNK1120 remains 2. Neither game links yet; all 30
separate tool ABI errors are unchanged. Full Win32 Release/CTest14, direct149
and focused75/title, native CTest8, math/FVF/Bezier, RB1/RB2/RB3 and D3D11.1/.2
checks pass. Native policy proof does not establish real AMD64 resource runtime.
See [X64_6_TEXTURE_CREATION.md](X64_6_TEXTURE_CREATION.md) for measured semantics,
real-device evidence, exact file/helper inventories and limitations.

**Next proposed task: X64.7 - pinned-reference fixed legacy pixel-shader bytecode
compatibility for all four production D3DXAssembleShader strings.** Preserve
exact tokens, device creation/reset/failure/ownership and conditional profiler
coverage. The larger file/filter/surface group and diagnostics were weighed
separately; they are not implemented here. No next task was started. Pathfinding
2.0 remains backlog; Stage4A.3 remains PAUSED. Older checkpoints are historical.

## X64.5 integer FVF stride compatibility (2026-10-11, historical checkpoint)

The three production D3DXGetFVFVertexSize calls now use a header-only integer
LegacyD3DX boundary. All 1,245,282 reference inputs match the actual pinned
Win32 library and the native AMD64 recorded oracle. Vertex layouts, offsets,
packing, DynamicVB/IB and gameplay remain unchanged. The dormant 48-byte struct /
52-byte displacement FVF mismatch is preserved and documented explicitly.

Native probes remove exactly one helper per title: 9 -> 8; aggregate LNK2019
18 -> 16, LNK2001 stays 8 and LNK1120 stays 2. Neither game links yet. The exact
30 separate tool ABI errors remain. Full Win32 Release, CTest11, direct149/title,
focused75/title, native CTest7, math/Bezier, RB1/RB2/RB3 and D3D11.1/.2 checks
pass. DX8 remains default; no game was launched or runtime equivalence claimed.
See [X64_5_FVF_STRIDE.md](X64_5_FVF_STRIDE.md) for callers, integer semantics,
reference provenance, validation, helper inventory and remaining risks.

**Next proposed task: X64.6 - reference-tested DX8 texture creation compatibility
for D3DXCreateTexture, D3DXCreateCubeTexture and D3DXCreateVolumeTexture.** The
coherent allocation group offers a three-helper reduction and shared policy
needed by later resource routes. Characterize pinned capability/format/size/mip
negotiation and failures first; do not replace it with bare device calls or
fake resources. Diagnostic formatting alone and fixed shader bytecode were
weighed separately in the report. No next task was started. Pathfinding 2.0
remains backlog; Stage4A.3 remains PAUSED. Older checkpoints are historical.

## X64.4 CPU D3DX math compatibility (2026-10-11, historical checkpoint)

The eight external matrix/vector math helpers now route through the engine-owned
LegacyD3DX boundary. Exact Win32 reference words, live comparisons, aliasing and
native AMD64 tests cover the pinned modern-x86 SSE2 arithmetic contract. Both
titles' authoritative Bezier projectile route is explicitly risk-audited; unit
tests do not certify cross-machine multiplayer/replay/save equivalence.
DX8 stays default and the accepted D3D11 slices are unchanged. No game launch,
FP optimization, format/layout change or next-stage implementation occurred.
See [X64_4_D3DX_MATH.md](X64_4_D3DX_MATH.md) for conventions, inverse behavior,
validation, actual linker progression, limitations and the exact inventory.

Final native title probes remove exactly eight helpers: 17 -> 9 per title;
aggregate LNK2019 34 -> 18, LNK2001 24 -> 8, LNK1120 remains two. Neither game
links yet. The bounded full native graph retains the exact 30 tool ABI compiler
errors, separate from game linkage. Full Win32/CTest9 and native CTest6 pass.
**Next proposed task: X64.5 - reference-tested native CPU FVF vertex-stride
compatibility (D3DXGetFVFVertexSize).** No next task was started.

## D3D11.2 ordinary UI file content (2026-10-11, historical checkpoint)

Bounded raw/RLE 24/32-bit TGA and DXT1/3/5 DDS top-mip content now feeds
immutable D3D11 residency and normal-alpha Render2D. Reference 16/32-bit
selection and DEFAULT/repeat sampler intent are explicit. Real WARP tests
and installed SAUserInterface512_001.tga draw/readback pass on x86/AMD64.
DX8 remains the default; the real FileFactory/TextureClass bridge is compiled,
not game-validated. Nonzero HSV, rescaling and native missing fallback remain
explicit restrictions, alongside all unmigrated renderer features.
Full Win32 Release/CTest7 passes; native independent CTest5 passes. Full native
and title link probes retain exactly 30 tool errors and 17 D3DX helper names.
See [D3D11_2_FILE_UI_TEXTURES.md](D3D11_2_FILE_UI_TEXTURES.md) for actual loader
policy, proof, inventory, unsupported cases and validation evidence.

**Next proposed task: X64.4 - remove the eight external D3DX math helper
dependencies through a tested native CPU boundary.** Live Bezier/W3D callers
and native links support returning to x64 now; another UI slice would not
remove these failures. Preserve operation/FP ordering, aliasing and DX8 behavior;
first playable native linkage still has resource/shader/tool gates. No X64.4
work was started. No game was launched; captures/ETL/runtime evidence remain
preserved. Pathfinding 2.0 remains backlog; Stage4A.3 remains PAUSED. Older
checkpoints below are historical and superseded.

## D3D11.1 real sentence Render2D foundation (2026-10-11)

Implemented the bounded native backend behind IRenderBackend with private
device/COM state, real sentence upload, explicit supported pipeline and retained
residency/buffers. Offscreen readback and hidden DXGI Present/resize pass on
real WARP devices on x86 and AMD64. Full Win32 Release/CTest passes; bounded
native full/link probes retain exactly 30 tool errors and 17 D3DX helper names.
See [D3D11_1_FOUNDATION.md](D3D11_1_FOUNDATION.md) for tests, inventory and limits.
DX8 remains the default; TextureClass bridge execution in game, hardware-driver
coverage, full-game D3D11 and visual equivalence are not established. No game
was launched. Captures/ETL/runtime evidence remain preserved.

**Next proposed task: D3D11.2 - ordinary file-backed single-mip normal-alpha UI
Texture2D CPU loading.** W3DDisplay::drawImage requests file-backed MIP_LEVELS_1
textures; the native loader remains the next real content boundary. Preserve
asset/missing/format policy; do not expand into all materials or mutable video.
Pathfinding 2.0 remains backlog; Stage4A.3 remains PAUSED. Do not begin the next
slice automatically. Older checkpoints below are historical and superseded.

## RendererBoundary.3 CPU sentence content (2026-10-11)

The actual FontChars glyph producer now writes W3D CPU A4R4G4B4 backing.
Sentence textures snapshot these bytes without native resource allocation;
DX8Backend creates/uploads managed residency at first use. Layout/glyph/sampler
semantics and retained legacy producers are protected by reference checks.
See [RENDERER_BOUNDARY_3_CONTENT_BACKING.md](RENDERER_BOUNDARY_3_CONTENT_BACKING.md)
for device-free real producer proof, validation and compatibility limitations.
**Next proposed task: D3D11.1 - the bounded sentence textured Render2D slice.**
Readiness is YES for that real content path, not all UI/Font3D/radar/video or the
whole renderer. Native D3DX/tool ABI blockers remain. No game was launched;
no visual-equivalence or playable Win64 claim is made. Pathfinding 2.0 remains
backlog; Stage4A.3 remains PAUSED.

The following X64.3 checkpoint is historical; this next-step state supersedes it.

## X64.3 DX8 declarations and renderer boundary (2026-10-11)

X64.3 separates the original pinned SDK declarations from I386 linkage.
Win32 still uses the unchanged upstream D3D8 target: compile commands and link
requirements match the reference exactly, and full Release validation passes.
Native WWMath and both titles' W3D/engine/device archives compile. The full build
now stops on 30 tool ABI error occurrences; separate native game link probes
fail on 17 D3DX helpers each, with input/GUID dependencies resolved through the
native Windows SDK. This is not a playable native game or a D3D11 backend.
Real DX8 sources remain enabled.

**Next proposed renderer task: RendererBoundary.1 - W3D resource/state submission
for the existing Render2D vertical slice.** The current IRenderBackend covers
scene/viewport/light operations but most resources, state and drawing bypass it.
Retain DX8 as the functional adapter, migrate only the real slice's neutral
contract, then use that seam for a separately validated D3D11.1 device/draw step.
Common Bezier external D3DX math and general GUI/persistence/allocator/audio/
browser/FP-lockstep gates remain separate; compilation is not compatibility proof.
No D3D11 implementation or next x64 phase begins automatically. Pathfinding 2.0
remains backlog; Stage4A.3 remains PAUSED. No game was launched.

See [X64_3_DX8_RENDERER_BOUNDARY.md](X64_3_DX8_RENDERER_BOUNDARY.md) for the full
consumer inventory, math/helper split, contract, actual native build results,
remaining blockers and validation. Older next-step notes below are historical
and superseded by this result.


## X64.2 native diagnostics (2026-10-09)

X64.2 removes the 92 reproduced diagnostic/callback error occurrences. Native
WWLib/debug/DbgHelp libraries and five independent platform tests pass. Both
conditional title stack bodies compile on x86 and AMD64; Win32 Release remains
validated (CTest3/3, direct132/title, focused58/title, Python16). Full Win64 still
stops on DX8 headers; no native game linked or launched. X64 is not complete.

**Next proposed task: X64.3 - Separate legacy DX8 header/math requirements from x86 binary linkage.**
Keep Win32 behavior and the full graph; define the native backend dependency
contract without renderer stubs, x86 link substitutions or D3D11 implementation.
GUI/allocator/audio/browser and format/FP lockstep gates remain. X64.3 has not
begun; Pathfinding 2.0 remains backlog and Stage4A.3 remains PAUSED.

See [X64_2_WINDOWS_DIAGNOSTICS.md](X64_2_WINDOWS_DIAGNOSTICS.md) for exact native
API/address changes, before/after counts, tests and limits. Older next-step notes
below are historical and superseded by this result.

## X64.1 build foundation (2026-10-09)

X64.1 audit and parallel experimental Win64 presets are complete. Win32 Release
remains validated; Win64 configures and compiles independent native libraries,
but full engine compilation is blocked by DX8 dependencies and x86 diagnostics.
No native game linked or launched; x64 is not complete. Renderer boundary/native
backend dependency work must precede first x64 gameplay with this dependency set.

**Next proposed task: X64.2 - Native Windows diagnostics and stack-unwind foundation.**
Keep that scope bounded to contexts/unwinding/DbgHelp/dialog ABI and Win32
regression proof. Renderer dependency, GUI pointers, allocator/layout and
format/FP lockstep validation remain separate gates. No D3D11 or X64.2 work begun.
Pathfinding 2.0 remains backlog; Stage4A.3 remains PAUSED.

See [X64_1_BUILD_FOUNDATION.md](X64_1_BUILD_FOUNDATION.md) for actual compiler
counts, dependency matrix, renderer decision, validation and exact next scope.
The older X64.1 next-step notes below describe the prior closeout state.

## Performance cycle closeout (2026-10-09)

Stages 1-3 high-FPS timing/interpolation foundation and Stage 4A/A.1/A.2
profiling foundation are complete. Developer Mode / Quick Game / battle preset
are the accepted practical interactive development and stress workflow. Stage4A.3
automation remains **PAUSED**, with neutral-owner selection and warmup goal
relocation defects unresolved; it is not accepted representative automation.

Stage4B.1 surface-mask hoist is retained; no measurable speedup is claimed.
Stage4B.2 investigation is complete with no production movement cache. Stage4B.3
oracle/equivalence/microbenchmark work is complete with no selected production
insertion optimization; exact retail insertion behavior remains unchanged.

**Immediate next major phase: X64.1 - Architecture/dependency audit and Win64 build foundation.**
No x64 implementation is part of this closeout. Prefix/rank-hint feasibility and
other pathfinding work belong to the future **Pathfinding 2.0 backlog**, subject
to the existing equivalence and meaningful-benefit gates, not the immediate task.

See [PERFORMANCE_CYCLE_CLOSEOUT.md](PERFORMANCE_CYCLE_CLOSEOUT.md) for the reviewed
file inventory, commit grouping and final integrated validation. All earlier
next-step, uncommitted and no-push statements below describe their historical
task states and are superseded by this closeout. Preserve all local evidence.

## Stage4B.3 retail-list investigation: Outcome B (2026-10-09)

Actual production insertion oracle/microbenchmark and adversarial tests are added.
No production insertion optimization selected: cost snapshot and exact two-step
loop do not show a broad repeatable worthwhile gain. Three test-access friend
headers add no runtime state; actual insertion .cpp is unchanged. Exact identity,
links, tail,5000 cutoff, repair and noncanonical-owner behavior are covered.

Full x86 Release build PASS; CTest2/2; direct132/title; focused58/title; Python16;
PowerShell9. No stage4b3 runtime, launch, commit/push or staging. Stage4B.2 remains
rejected for implementation and Stage4A.3 remains PAUSED.

Future Pathfinding 2.0 backlog: bounded prefix/rank-hint feasibility against the
actual oracle, with complete mutation/repair validity proof and unchanged logical
hop cutoff. No production finger/index authorized; stop if broad invalidation is
needed. Larger Internal checking remains the alternative with its reference gate.

[STAGE4B3_RETAIL_OPEN_LIST_INVESTIGATION.md](STAGE4B3_RETAIL_OPEN_LIST_INVESTIGATION.md)
contains exact semantics, measured baseline/candidates, limits and reproduction.
Earlier next-step notes below are historical and superseded by this outcome.


## Stage4B.2 audit: production cache rejected pending equivalence (2026-10-09)

Stage4B.1 remains accepted for retention. Stage4B.2 added no gameplay code or
measurement observer. The audit establishes substantial successful line-query
repetition, but existing captures do not measure full query keys/bounded-cache hits,
and exact actual-helper/path reference equivalence has not been demonstrated.
Pool occupancy IDs share search storage; non-ground getCell fallback prevents an
unconditional30000-key bound. Across7 Internal>40ms, conservative successful-line
queries>=7851431 and repeats>=4701431 using15 layers; these are derived bounds,
not measured cache hit rates or saved milliseconds.

No Stage4B.2 candidate or runtime command. Next proposed optimization investigation:
actual-retail-list equivalence tests/microbenchmark for forwardInsertionSortRetailCompatible,
whose repeated cost is measured and whose production primitive can be tested without
an object/terrain world. Preserve exact ties/5000-hop/repair/order; reject noise-sized
gains. Movement caching remains promising but requires the explicit reference gate.
No automatic heap/fixed-mode/policy change. Stage4A.3 remains PAUSED.

Full dependency/pool audit, bounds, rejection reason, alternatives and validation:
[STAGE4B2_PATH_MOVEMENT_CACHE_AUDIT.md](STAGE4B2_PATH_MOVEMENT_CACHE_AUDIT.md).
The prior next-step proposals below are historical and superseded by this audit.


## Developer runtime review and latest capture — PASS (2026-10-09)

Developer confirms TEST1 quick-game/overlay/money/reveal/spawn/capture/normal exit
PASS and TEST2 battle preset PASS (96 units per side, movement/fighting, interactive,
capture and normal exit). Release instant build remains documented/deferred and is
not a blocker. Matching launch receipts for both runs show the expected candidate
EXE hash and exit0.

Latest exact battle capture: build/performance/dev-20261009T082332Z-c286325d,
stem20261009T082505Z-29724-1;60.0094s clean automatic60-second stop,30375path records,
zero drops/stack-clock/phase errors,120requested/effective cap and interpolation1.
GameLogic mean/p95/p99/max3.3180/8.2369/27.8651/165.1720ms;15logic frames>30ms,
all pathfinding-dominated.770Internal searches,64>10ms/11>30ms/7>40ms; worst156.232ms.
Severe sampled Line/Neighbor/insertion61.87/13.22/24.90%; all7checking winners.

Retain Stage4B.1; optimized route is exercised, manual gameplay is healthy, no
regression evident. NO matched speedup claim; its inline getter hoist may be too
small to distinguish from run variation. Proposed next high-value task: bounded
single-Internal-search reuse of pure checkForMovement footprint/query results in
the line callback, subject to read/write invariance proof and exact reference
comparisons. No Stage4B.2 implemented or authorized by this analysis. Sorted insertion
is a later target; rendering/present is sustained wall cost but GPU/wait cause is
unresolved. Stage4A.3 debugging stays PAUSED; do not resume Fix6 or add another
selection capture/profiler.

Full measured analysis, risks and acceptance plan:
[STAGE4B1_INTERACTIVE_CAPTURE.md](STAGE4B1_INTERACTIVE_CAPTURE.md).
All earlier source/forensics/evidence preserved; documentation-only update, no
build/game launch/commit/push. The earlier never-launched runtime-review state below
is historical and superseded by the developer's successful manual review.


## Current priority — interactive Dev Mode / Stage4B.1 (2026-10-09)

Developer review explicitly PAUSES the Stage4A.3 relocation investigation and Fix6.
Do not rerun old scenarios or stage another diagnostic Fix6. All automation, forensic
snapshots, reference1–5, previous runtimes and working -skipIntro remain preserved.
The neutral-owner0 defect and goal_relocated_after_warmup remain unresolved harness
issues; neither is corrected or used as a prerequisite for current optimization.
Earlier "Stage4B locked" / "next diagnostic trial" instructions below are historical
and superseded by this priority decision.

Part A independent interactive Quick Game and actions are implemented and validated
without game launch. Explicit slot0 human / slot1 enemy ownership rejects neutral.
Part B implements one local surface-mask hoist in the accepted Internal line callback;
source-level exact-reference/order proof passes. No runtime speedup or gameplay success
is claimed. See [DEVELOPER_MODE.md](DEVELOPER_MODE.md) and
[STAGE4B1_PATH_POLICY_HOIST.md](STAGE4B1_PATH_POLICY_HOIST.md).

Next: developer runtime review of ONE fresh isolated stage4b1-devmode-pathfinding
candidate, first Quick Game/actions, then a bounded existing-profiler interactive
battle capture. No further five-AI selection capture required. No automation/Fix6
prerequisite. No launch, commit or push by agent. Future reproducible benchmark repair
remains a separate follow-up, after developer priority review.



Latest Stage4A.3 forensic state (2026-10-09): Fix5/reference-5 failed at tick91
with goal_relocated_after_warmup, unit293/index76, (600.5,3080.5)->(610.5,3080.5).
Center checkForAdjust failed; right-neighbor first spiral cell succeeded. Exact
terrain/occupancy/reservation/connectivity branch is unrecorded. Same validation
primitive runs at preparation/order time, but world state differs and prior orders
can synchronously write reservations. No speculative endpoint/tolerance fix.
Bounded getter-only index76 cell snapshots added to source for review; no corrected
Fix6 runtime staged because root cause remains unproven. All96 units were owner0
(neutral): independent ownership defect must be resolved before benchmark acceptance,
not assumed to explain this relocation. Developer confirms real -skipIntro success.
Preserve reference1..5/runtimes; no rerun, launch, commit/push or Stage4B. Current
forensic report atop STAGE4A3_PERFORMANCE_HARNESS.md supersedes older launch procedures.



Current Stage4A.3 state (2026-10-09, supersedes historical entries below):
Fix4/reference-4 controlled failure at tick91 after96 spawns/90 warmup/76 orders;
no capture started. Exact reason workload_goal_changed_or_unreachable conflates
three checks, so no endpoint/pathfinder correction is justified yet. Fix5 adds
bounded first-check/unit/candidate diagnostics and safe opt-in -skipIntro (implicit
for scenario), preserving all readiness guards/checks/thresholds. Release/CTest,
119 direct and45 focused tests/title,11 Python and actual intro CLI tests pass.
Next: review never-launched stage4a3-pathfinding-workload-fix5-final; authorize ONE
fresh reference-5 diagnostic trial using the current Stage4A3 report procedure.
No game launched, no commit/push; all old evidence retained. Stage4B stays locked,
with accepted future target Internal goal-directed line/movement/neighbor checking.



Stage 4A.3 workload fix4 (2026-10-09): automation completion and coordinate
schema repair are accepted; v1/reference-3 remains invalid for comparison and
Closest dominated. Source/CSV/map inspection identifies pre-expansion destination
footprint rejection followed by normal Closest retry; crowded8-unit goals and
repeated redirects are the leading workload cause, with exact cell ownership
unrecorded. Prepared version2 candidate uses bounded legal/connected western
bank pairs, min40 spacing and one ordinary script move/unit at capture start.
No pathfinder or normal-game policy change. New qualification gate is structural:
96 Internal records,3 >=20k pops/500k info attempts with both checking samples,
and >=75% Internal pop share versus Closest; timing remains diagnostic only.
Accepted A.2 passes; v1 fails. Release/CTest2of2,42 focused/title and10 Python
tests pass. Next: developer review then ONE guarded trial of never-launched
stage4a3-pathfinding-workload-fix4 into fresh stage4a3-reference-4. Do not launch
fix3, start extra trials or implement Stage4B before reviewing this result.
No launch/commit/push. See current one-trial procedure atop the Stage4A3 report;
older entries below are historical. Target remains Internal goal-directed
line/movement/neighbor checking.


Stage 4A.3 fix2 real trial review (2026-10-09): reference-3/trial-1 completed
96 units/480 orders/90 warmup/300 capture ticks, finalized all reports and exited
intentionally with code0. No new crash. Post-validation found float-coordinate
parsing and lossy six-digit coordinate serialization; reporting-only repair
keeps strict fingerprints (original trial still mismatches and cannot be reused
as a validated reference). Preserve all three reference directories and fix2.
No startup/gameplay/pathfinding change. Severe workload is 93 Closest retries,
not Internal: Internal max18.6461ms, zero>40ms. Accepted Stage4B target remains
Internal line/checking; do not run two more unchanged scenario trials or treat
this capture as representative. Next: review schema repair and narrow scenario
reachability/failed-request workload investigation before a separately approved
scenario revision/capture. Full Release build, CTest2/2, focused40/title and
8 Python tests pass. No launch/commit/push. See Stage4A3 report's real-trial section;
earlier pending-runtime entries below are historical.

Stage 4A.3 fix2 (2026-10-09): fix1 crashed at tick 0, GameLogic.cpp:3757,
reading null TheGameInfo+8 in CRC generation. Matching PID3592 full/minidumps,
exact staged PDB age27 and captured Sizzle/movie/loading state prove the harness
requested Skirmish before Intro completed. Slots were ready but no scenario
map load/player construction, units, orders, warm-up or profiler recording
completed. Preserve reference-1/reference-2, both earlier runtimes and dumps.
Fix2 waits for the complete Intro lifecycle and no movie/load/clear transition
before requesting the unchanged scenario. Adds only read-only readiness
accessors, bounded scenario breadcrumbs and focused regressions. Release build,
CTest 2/2, 113 direct and 39 focused tests per title, six Python tests and static/
evidence audits pass. Next: review the updated Stage 4A.3 report and fresh guarded
`stage4a3-performance-harness-fix2`, then developer trials into fresh
`stage4a3-reference-3`. Runtime remains unproven. No game launch, Stage 4B,
commit or push. Earlier entries are historical.

Stage 4A.3 fix1 (2026-10-09): the first developer trial failed in initial Setup:
menu-owned `TheSkirmishGameInfo` was null, and the automatic harness incorrectly
required it before initialization. No scenario map request, units, orders,
warm-up or capture occurred. Preserve reference-1/trial-1 and the original
runtime. Its old marker omitted the absolute logic tick; do not invent one.
Scenario-only lazy allocation now follows normal engine ownership; bounded
failure diagnostics include state/tick/progress and specific startup reasons.
Two focused regressions were added. Final Release build, CTest 2/2, 111 direct
and 37 focused tests per title, four synthetic profiler benchmarks per title,
six Python tests and static audits pass. Fresh guarded candidate is
`stage4a3-performance-harness-fix1`, staged but not launched. Next: developer
review, then three automatic trials into fresh `stage4a3-reference-2` using the
updated report commands. Runtime success/coverage/repeatability remain unproven.
No Stage 4B, game launch, commit or push. Earlier entries are historical.

Stage 4A.3 (2026-10-09): start at
[STAGE4A3_PERFORMANCE_HARNESS.md](STAGE4A3_PERFORMANCE_HARNESS.md).
Accepted baseline `0ff7f9c9b` is synchronized/pushed; current additions are
uncommitted, unstaged infrastructure only. Default-OFF scenario automatically
prepares fixed Twilight Flame offline allied China slots, spawns 96 AI-owned
ground units, schedules ordinary move orders, warms up 90 ticks, captures 300
ticks, writes bounded metadata/CRC/RNG/path fingerprints and exits. Existing
manual profiler commands remain available. Repeated-trial guarded scripts and
strict validity/configuration/correctness comparison are in scripts/performance.
Minimal gated Ctrl+Alt+Shift+F5â€“F11 tools provide overlay, money, reveal,
predefined spawning and capture. Instant build keeps its existing compile-time
gate and is unavailable in this Release build; scenario restart is a fresh process.
Full x86 Release build, CTest 2/2, 109 direct tests per title, 35 focused tests per
title, four profiler/overhead benchmarks per title, six Python tests and token
audits pass. Source/new/previous runtime inventories pass 369 files each; wrapper
two-trial validation-only checks create no output, backups, receipts or process.
Candidate `stage4a3-performance-harness` is staged and unlaunched (PDB age 25).
Next developer action: review, then execute three automatic unchanged-reference
trials from the report. Actual map/setup, severe Internal coverage and matching
fingerprints remain unvalidated. Keep Stage 4B unimplemented until that reference
is qualified; its selected target remains Internal line/movement checking.
No new manual five-AI selection battle, game launch, commit, push or generated-file
staging. Earlier entries are historical.

Stage 4A.2 final analysis (2026-10-08): selection gate resolved from all five
reports plus command.txt in `build/performance/stage4a2-B-final-2`.
Read [STAGE4A2_PATH_PHASE_SAMPLING.md](STAGE4A2_PATH_PHASE_SAMPLING.md) first.
Valid 43.9108-second capacity stop, phases enabled at stride 64, zero phase or
stack/clock errors, interpolation ON and requested/effective cap 120. Exclude
all final-frame detail because 15 records dropped; 32,718 complete rows remain.
The 31 Internal searches over 40 ms consistently favor checking: sampled
Line/Neighbor/insertion shares 51.19/15.56/33.25%, robust to the documented
clock-overhead sensitivity. First proposed Stage 4B route is Internal
goal-directed line/movement checking through `examineNeighboringCells` ->
integer `iterateCellsAlongLine` -> `examineCellsCallback`. Preserve all checks,
cell order, FP/cost, allocations, parents/reopens, insertion ties/5000-hop rule,
retry/hierarchy/queue policy and deterministic state. Closest insertion is a
later candidate; this decision does not generalize to all Ground/Closest work.
Next recommended infrastructure task: reproducible deterministic heavy developer
scenario/benchmark harness with unchanged reference and differential correctness
checks, before long optimization iterations. No further manual five-AI selection
capture or WPR requested. Analysis changed only the three modernization documents;
existing Stage 4A.2 source/test work is preserved. No behavior/harness implementation,
game launch, staging, commit or push in this analysis. Stop for developer review.
Earlier unresolved-gate and capture instructions below are historical.

Stage 4A.2 (2026-10-08): started clean/synchronized at accepted/pushed 854b8294d.
[STAGE4A2_PATH_PHASE_SAMPLING.md](STAGE4A2_PATH_PHASE_SAMPLING.md) is the current
observer/capture handoff. Only bounded per-search every-64th phase measurement is
added: line, remaining neighbor/layer and their nested insertion; no extra rows,
sort/policy/budget/state changes or game launch. Final x86 Release build, CTest 2/2,
198 direct tests, focused tests, sequential benchmark trials and token/static audit
pass. New guarded candidate is stage4a2-path-phase-sampling. Use the report's
single delayed 45-second busy ground-heavy developer + 5 AI capture, interpolation
ON and cap 120. No further WPR is requested. Select one Stage 4B target only after
analyzing repeated expensive individual phase records. No commit/push.
After target selection, prioritize a reproducible deterministic developer workload
harness before a long optimization series (build, isolated stage, launch, warm-up,
heavy pathfinding/object/AI load, bounded capture, exit, reference comparison).
No such harness is implemented now. Real user+AI matches remain milestone checks;
automated scenarios should become frequent development benchmarks. Older entries
below retain historical states.

Accepted Stage 4A/4A.1 tooling baseline (2026-10-08): developer authorized one
tooling commit, including the synchronized WPR analysis in
[STAGE4A1_PATHFINDING_DEEP_PROFILE.md](STAGE4A1_PATHFINDING_DEEP_PROFILE.md).
Pre-commit win32 Release build, CTest 2/2, 184 direct tests, both profiler benchmark
smoke suites and the pathfinder token audit pass. Release EXE/PDB hashes still
match the captured candidate exactly. Stage 4B selection remains unresolved:
truncated ETW caller chains and comparable insertion/checking leaf costs require
review of the minimal sampled phase observer proposal. That observer is not
implemented. No gameplay change, game launch or push; stop after the tooling
commit for developer review. Entries below retain historical states.

Stage 4A.1 update (2026-10-08):
[STAGE4A1_PATHFINDING_DEEP_PROFILE.md](STAGE4A1_PATHFINDING_DEEP_PROFILE.md) is current.
HEAD remains 1de7e65d173104ad2f288ac0a379e102a5057ebc; preceding Stage 4A changes
were already dirty and are preserved. Actual A/B summaries/categories/frames/slow
files were read and cross-checked. Pathfinding dominates several B logic tails,
but 163.287 ms is an aggregate of five search calls, not one measured search.
Do not choose an exact Stage 4B algorithm change yet. New -pathProfile plus
-performanceProfile <directory> records individual linked calls/work/maintenance;
without the extra flag the same candidate retains shallow aggregate profiling.
Final Release build/CTest 2/2, 184 direct tests and all profiler benchmarks pass;
source/new/previous Stage 4A full inventories pass (369 files each), and launch
validation passes without a launch or backup. Read the report for capacities,
inclusive counters, outcomes and delayed-start
manual commands. Fresh candidate stage4a1-pathfinding-deep-final is unlaunched.
No optimization, game launch, commit or push. Preserve original capture bytes
and all previous runtime candidates. Older entries describe historical states.

Stage 4A update (2026-10-08):
[STAGE4A_PERFORMANCE_BASELINE.md](STAGE4A_PERFORMANCE_BASELINE.md) is the current handoff.
Started clean on dev/modern-engine at 1de7e65d173104ad2f288ac0a379e102a5057ebc;
Stage 3C.4 is developer accepted/committed/pushed after full 1v1 and OFF comparison.
Stage 4A adds default-OFF developer CPU profiling with an explicit existing output
directory, file start/stop controls, 60-second/16,384-frame bound, QPC scope accounting,
CSV tails/top-N/counters and synthetic tests/overhead measurements. No optimization.
Final win32 Release build, CTest 2/2, 168 direct Google tests and both profiler
benchmarks pass. Initial report-test failure from metadata edits during build is
retained/documented; a consistent rebuild passes. Candidate stage4a-performance-baseline
is unlaunched; full source/new/six previous acceptance inventories pass (369 files
each), with matching EXE/PDB identity and hashes recorded in the report.
Use exact A (1 AI) and B (5 AI) capture instructions in the report;
primary accepted interpolation ON, stable finite cap, preserve metadata/settings.
Runtime profiler control/output and in-game overhead still need manual acceptance.
No real match bottleneck measured; do not select Stage 4B yet or assume pathfinding,
interpolation or threading is the answer. No commit/push/Install/Steam/user-data writes.
Earlier entries below are historical; later main-PC high-refresh/ultrawide still pending.

Stage 3C.4 update (2026-10-08):
[STAGE3C4_GROUND_ORIENTATION.md](STAGE3C4_GROUND_ORIENTATION.md) records root-heading interpolation.
Started clean at 289be9bf9 after developer Stage 3C.3 acceptance/commit/push.
Existing clock and completed XYZ pair now share two heading samples, +8 bytes
per Drawable (history 48, Drawable 392 in x86). Local shortest-yaw basis rotation
precedes existing instance/physics/model decoration; canonical getters, aiming,
turret/barrel states and simulation remain unchanged. Separate default-false
orientation capabilities exclude articulated trucks, terrain-aligned roots and
infantry combat while retaining translation. The same process-local flag remains
OFF by default. Release build/CTest 2/2 and 148 direct Google tests pass; both
Release synthetic benchmarks pass. New candidate stage3c4-ground-orientation;
full inventories PASS for it, Steam source and all five previous acceptance
candidates (369 files each). Hashes and manual 1v1 procedure are in the report.
Stage 3C.4 runtime acceptance remains pending. Developer + 5 AI combat spikes
already occurred ON/OFF before this stage; retain for later stress profiling.
Ordinary acceptance uses developer vs 1 AI. Recommend PERFORMANCE BASELINE /
PROFILING next after 1v1 review; no speculative spike fix or deeper presentation
stage implemented. No launch/Install/user-data/Steam changes/commit/push.
Earlier entries below describe their historical acceptance states.

Stage 3C.3 update (2026-10-08):
[STAGE3C3_GROUND_HARDENING.md](STAGE3C3_GROUND_HARDENING.md) records the hardening.
Developer Stage 3C.2 same-binary laptop A/B confirmed smoother eligible movement
with ON and no obvious speed/basic-selection regression. Starting clean at
8fdc9c939, preserve the clock/history/render architecture; default-OFF capability
policy replaces stock-name gating, admits standard truck roots and ordinary
combat/guard/formation movement, and excludes special AI/script/containment states.
Local box selection follows presentation XYZ; commands/getters stay canonical.
Explicit semantic/locomotor/train displacement history resets add no simulation
mutation. Release build/CTest 2/2 and 106 Google tests pass; actual x86 history
impact remains 40 bytes/Drawable, with synthetic cost measurements in the report.
Final fresh candidate: build/dev-runtimes/zh/stage3c3-ground-hardening.
Full SHA-256 inventories pass for it, the Steam source and all four older
candidates (369 files each); EXE/PDB identities/hashes are in the report.
Stage 3C.3 runtime acceptance is pending. Recommend more ground hardening/manual
acceptance before orientation. No launch/Install/user-data/Steam changes/commit/
push; previous entries are historical.


Stage 3C.2 update (2026-10-08): read
[STAGE3C2_GROUND_INTERPOLATION.md](STAGE3C2_GROUND_INTERPOLATION.md).
Default-off Release `-groundInterpolation` prototype uses the Stage 3C.1 pair
for XYZ only on six exact stock ground templates in idle/move-to states.
Drawable-owned nonserialized history captures final completed generations before
client updates/views; local matrix replacement precedes instance/physics
composition. Explicit lifecycle/containment/load/large-move snaps; unknown short
semantic relocations and rendered picking versus canonical selection remain
gated limitations. Clock, legacy phase, simulation ordering and formats stay
unchanged. Release build and 90 Google tests pass (CTest 2/2); fresh candidate
`stage3c2-ground-interpolation` and Steam source pass 369-file SHA256 checks each.
Baseline/Stage 2/Stage 3A remain intact. Runtime acceptance is entirely pending;
no game launch/Install/commit/push. Stage 3C.3 starts with candidate review and picking/teleport/FX acceptance,
not broader controls or interpolation. Earlier entries below are historical.

**Latest, Stage 3C.1 (2026-10-08):** read
[STAGE3C1_PRESENTATION_CLOCK.md](STAGE3C1_PRESENTATION_CLOCK.md) first. Started clean
on `dev/modern-engine` at `bc87b4954`; developer had committed/pushed Stage 3A/3B.
FramePacer owns a separate PresentationClock. Both engine schedulers copy actual
post-subtraction remainder/T; after its existing wait FramePacer publishes the
completed frame pair, epoch, alpha and validity for the next client pass. Normal
offline alpha is clamp((remainder + newly measured delta)/T). Existing
getLogicFramePhase and particle/physics consumers are unchanged.

Unsupported network/immediate/fast modes, pause/freeze/halt, >=T deltas and invalid
observations snap to canonical current state; valid timing needs two fresh
consecutive completions. Both title GameLogic reset/loadPostProcess and FramePacer
reset invalidate presentation history, never the existing engine accumulator.
No source changes outside this timing boundary, no world cache/interpolation,
controls, RNG or format/pacing changes. Release configure/build passes; CTest 2/2,
28 Google tests each (56 total). Existing warnings on unchanged lines are recorded
in the stage report. Runtime FPS/TPS, visuals and compatibility remain unverified.
No new runtime candidate was needed for this unused API; existing stages are
preserved. No launch, Install, commit or push. Stop for developer review.
Final read-only baseline/Stage 2/Stage 3A integrity checks pass all 369 files each.
Earlier updates below describe their historical state and authorization.

**Latest, Stage 3B (2026-10-08):** read
[STAGE3B_INTERPOLATION.md](STAGE3B_INTERPOLATION.md) first. Investigation started
clean on `dev/modern-engine` at `967dfb739 Add persistent render FPS options`.
Stage 3A is developer manually accepted: default 60, normal gameplay, Options
dropdown, immediate Accept at 120, persistence through exit/relaunch and integrity
checks. Approximately 30 TPS remains inferred, not measured. This stage changes
documentation only: no source/build/data/runtime change, launch, commit or push.
Baseline, Stage 2 and Stage 3A copied-runtime checks each pass all 369 paths,
lengths and SHA256 hashes. No build/tests rerun for this documentation change.

Generic Object -> Drawable -> W3D root translation does not interpolate.
Existing skeletal interpolation, decorative physics and particle integration
must be retained. FramePacer visual phase is separate from the scheduler's
remainder; resolve its contract before a gated Stage 3C ground-unit translation
prototype. Keep endpoint state client-owned; never mutate canonical Drawable
transforms temporarily: logic bone/launch queries and rendered picking can cross
the boundary. Snap spawn/load/reset/teleport/containment discontinuities.
Arrow panning uses elapsed client time; follow-camera smoothing includes a fixed
per-update factor. WASD conflicts include Stop in both titles, aircraft selection
in ZH and contextual button shortcuts. Plan opt-in rebindable actions separately,
preserving arrows, focus gates and explicit key/conflict ownership. Do not
implement controls or interpolation until the developer reviews follow-up scope.
The Stage 3B cap/visual/replay/network matrix is still NOT RUN.
Earlier updates below describe their historical acceptance state.

**Latest, Stage 3A (2026-10-08):** read
[STAGE3A_FPS_OPTIONS.md](STAGE3A_FPS_OPTIONS.md) first. Starting checkpoint was clean
`b68b2e82f` on `dev/modern-engine`. The developer has verified Stage 2's 60-FPS
offline smoke test: hotkeys no longer speed up gameplay, with smoother camera
motion and normal vehicles/timing; approximately 30 TPS is inferred, not measured.
Release `-fps` is RTS_DEBUG-table guarded, so use normal Options persistence.
Stage 3A adds default 60 and Options 30/60/120/144/165/240 via FrameRateLimit in
Options.ini, preserving Stage 2 scaling and temporary developer controls. The
legacy Skirmish slider has a complete behavior/call-site audit before retirement;
restart/replay message semantics remain intact. No broad persistence refactor.
Release build and 22 Google tests pass. The separate `stage3a-fps-options` runtime
is prepared for guarded manual UI/persistence acceptance, **not automatically
launched**. Preserve baseline and Stage 2. No commit/push; changes remain unstaged.
Stage 3A runtime success is still UNVERIFIED. Earlier updates below are historical.

Read this first, then the linked documents. Investigation: 2026-10-07,
revision `adac468d732503ba50f1de21bbfc5e83982c5430`.

**Stage 2 update (2026-10-08):** read [STAGE2_TIMING.md](STAGE2_TIMING.md) next.
Developer manually verified baseline startup/menus/skirmish/exit 0, normal
default-speed gameplay, no obvious audio/render/asset failures, and full 369-file
hash checks before/after. Agent rechecks also pass. **Raising the render/frame-rate
cap using Ctrl + Numpad + caused offline gameplay/simulation to speed up.**
The candidate enables FramePacer's existing 30-tick scale by default, shared by
both titles. The scheduler, constants and network path are unchanged. Explicit
developer speed/legacy controls remain. Preserve the existing `baseline` runtime;
use a new `stage2-timing` stage. Never launch automatically. 60-FPS manual
acceptance, higher rates, interpolation, replays and multiplayer remain UNVERIFIED.

**Stage 1 update (2026-10-08):** `scripts/zh-runtime` now provides guarded
staging, backup, launch and full SHA256 integrity helpers, with non-destructive
synthetic checks. Read [STAGE1_WORKFLOW.md](STAGE1_WORKFLOW.md) for usage and the
manual scenario/profiling checklist. PowerShell 7 required. Actual Steam source,
build EXE/PDB pairing and user-data resolution passed validation only. No real
runtime copy/user-data backup, game launch, profile build or gameplay test was
performed. Do not launch the synthetic workflow-check runtimes. No engine change,
commit or push; existing uncommitted investigation documents are retained.

- **Project:** developer fork of TheSuperHackers GeneralsGameCode; preserve
  Generals/Zero Hour gameplay/content/mod/save/replay/multiplayer identity.
  Engine stays C++; prioritize Zero Hour and mirror applicable Generals changes.
- **Workspace:** `C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode`.
- **Git:** `dev/modern-engine`; origin is martinkraud fork, upstream fetch is
  TheSuperHackers, upstream push DISABLED. Initial tree was clean. Only these
  eight investigation docs were created in the first session; Stage 1 adds scripts
  and workflow documentation. No commit/push/branch/config change in either session.
  Keep main for synchronization. Future focused branches only when needed.
- **Build:** x86 Visual Studio Developer PowerShell:
  `cmake --preset win32`, `cmake --build --preset win32`.
  Developer reported success 4620/4620. Observed MSVC 19.51.36260.0 / X86 /
  pointer size 4, Ninja Multi-Config, executables and PDBs. Stage 2 rebuild/test
  evidence and exact commands are in STAGE2_TIMING.md.
- **Outputs:** `build/win32/Generals/Release/generalsv.exe`;
  `build/win32/GeneralsMD/Release/generalszh.exe` and tools.
- **Protect Steam:** never modify
  `C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour`.
  Current CMake install destination points there: **do not run Install**.
  Use a full copied runtime under ignored build output. No hard links/junctions.
  `-setCwd <path>` and `-useCwd` exist; default is executable directory.
  Runtime copy does not isolate user data. Missing legacy Generals registry key
  may trigger debug assertion; do not silently edit registry.
- **x64:** badge = 3 closed milestone items, one abandoned, not a certified
  native x64 build. No supported Windows x64 Zero Hour method in this checkout;
  DX8 setup is 32-bit gated; Bink/Miles loading is 32-bit only; MinGW rejects x64.
  Community x64 Generals guide is for another fork. Do not begin a port.
- **Timing:** existing FramePacer, offline accumulator, network-ready gating and
  partial visual interpolation already exist. Offline logic scaling originally
  defaulted disabled; the Stage 2 candidate defaults it enabled at 30. Never change BaseFps or
  WWSyncPerSecond (30). At most one logic tick per render iteration currently.
- **Compatibility:** modern MSVC build is not retail CRC compatible according to
  CMake/TESTING; optimized VC6 SP6 is the retail replay reference. Preserve retail
  guards, update order, logic RNG, command frames and snapshot formats.
- **Profiling:** win32-profile enables Tracy 0.13.1 + legacy profiling; Google
  Test/Benchmark optional. Existing Tracy LogicFrame plot can measure frame-index
  change per elapsed time. No profiling measurements have been performed.
- **Safety:** no force pushes/history rewrite/shared rebases/mass formatting;
  no generated outputs committed; no invasive engine work in investigation.
  Read CONTRIBUTING.md and AI_POLICY.md before upstream contribution; human
  accountability/disclosure and submission restrictions apply.

## Documentation

[BASELINE](BASELINE.md): runtime steps, build/Git state, evidence/unknowns.
[ARCHITECTURE](ARCHITECTURE.md): source ownership, systems, profiling.
[X64](X64.md): milestone and current build evidence.
[TIMING](TIMING.md): exact scheduler/visual timing and high-FPS constraints.
[RENDERER](RENDERER.md): W3D/DX8, displays/UI and ultrawide.
[COMPATIBILITY](COMPATIBILITY.md): lockstep, replay/save/RNG risks and tests.
[ROADMAP](ROADMAP.md): staged objectives, tests, compatibility and rollback.
[STAGE3C1_PRESENTATION_CLOCK](STAGE3C1_PRESENTATION_CLOCK.md): implemented timing
contract, deterministic evidence, conservative exclusions and Stage 3C.2 gates.

## Next action

Developer: review STAGE3C1_PRESENTATION_CLOCK.md, especially alpha/sample IDs,
epoch handling, unsupported-mode policy and Stage 3C.2 prerequisites. Authorize
the narrow ground-unit translation cache separately before implementation.
Synthetic clock tests pass; the Stage 3B manual 30/60/120/144/165/240 matrix still
needs measured FPS/TPS, visual comparisons, pause/load/containment/stall cases,
same-build replay CRCs and mixed-cap LAN checks. Controls/rebinding remains
separate. Keep existing runtimes intact and protect shared user data for any
developer manual session. No automatic game launch, commit or push.
