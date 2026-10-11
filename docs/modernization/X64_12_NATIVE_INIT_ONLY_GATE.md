# X64.12: bounded native headless startup diagnostic gate

Date: 2026-10-12. Accepted baseline: `4a36a0cd2c2e3b2bc7fa6e579dd206aeaa7619d3`.

## Scope and result

Both titles now have an opt-in AMD64 init-only diagnostic gate. No game process
was launched in this stage. Successful native initialization, teardown and actual
dump generation remain untested until the separate developer-reviewed run.
The DX8 factory is unchanged; D3D11 is not selected. Native graphical startup is
still blocked by missing native D3D8. Native audio/video limitations remain.

The only accepted diagnostic argv is:
`-headless -nativeInitOnly <absolute-existing-diagnostic-directory>`.
No replay/map/network/developer/benchmark or extra options are accepted. Win32
rejects this option. Without it, no diagnostic file, user-data override or init-only
boundary is enabled. A malformed request exits with code87 before GlobalData.

## Implementation and isolation

`Common/StartupDiagnostics.h` is independent of engine allocation/strings/debug
locks. Configuration precedes GlobalData, including startup parsing from static
constructors, and is repeated idempotently at WinMain. Direct local canonical
ASCII paths shorter than180 characters are required; existing root ancestors,
userdata and CrashDumps must be directories without reparse points. The launcher
creates all three in a fresh run directory. The logger uses CREATE_NEW, refuses
to overwrite startup.log, WriteFile plus FlushFileBuffers after every record,
fixed buffers and a2048-record cap. Failed writes/capacity make the diagnostic
result fail closed. It has no per-frame logging in ordinary play.

TSV fields: sequence, UTC FILETIME hexadecimal, QPC count, PID, TID, stage, status,
error code, sanitized bounded detail. These are ordered checkpoints, not profiling
measurements. WinMain cannot diagnose OS loader/CRT failures before it is reached;
launch/exit receipts distinguish those from named in-process failures. A static
constructor can configure the sink before the WinMain checkpoint.

Both titles log module/CWD, exception setup, memory manager, startup parsing,
headless HWND/DX8/browser/input skips, instance guard, minidumper status and actual
dump directory. Engine subsystem wrappers log begin/complete/unwind-fail; nested
GameClient font/display/window/UI/terrain/video initialization, postProcessLoad,
map cache, initial reset, HideControlBar and cleanup are checkpointed. Typed
ErrorCode and INI catches include error detail; diagnostic initialization failures
rethrow to the logged WinMain catch and return failure rather than accidentally
continuing. Ordinary catch behavior is retained.

The isolated user-data override is deliberately limited to this diagnostic gate.
Generals' early getter and post-INI assignment, and ZH's constructor assignment,
use the same `run/userdata/` before any corresponding CreateDirectory/preferences
access. This avoids both the native registry-view difference and Generals' early
empty dump prefix. Normal registry/INI/default paths are untouched; no registry
writes, Documents backups/restores or broad GlobalData refactoring are needed.
The actual in-process module, CWD, user-data and dump paths must match launch.json
before the launcher accepts success. Do not treat the isolated run as validation
of default native per-title Documents/registry resolution.

Existing SEH filter, MiniDumper creation, system DbgHelp, dump worker and shutdown
remain in place. Only a dump-path checkpoint was added to MiniDumper. Native dump
creation is still untested. A caught C++ initialization failure is logged but is
not promised to generate a dump. The launcher bounds the child, including a stuck
existing dump worker, without terminating unrelated games.

## AMD64 floating point

Both GameLogic setFPMode implementations now use the shared tested
`NativeFloatingPoint::reset` on `_M_AMD64`: reset, request supported nearest-even
rounding/masked exceptions, clear accrued SSE flags, FTZ and DAZ, and retain gradual
underflow (MXCSR relevant bits0x1f80). Errors use existing release crash reporting.
Diagnostic calls log resulting control/MXCSR. Existing setFPMode call boundaries
are retained. Win32's exact original body is preserved under the other branch;
no game compiler FP flags or resource-conversion arithmetic changed.

The four X64.11 arithmetic/reset probes now invoke the production shared helper.
The historical legacy PC_24 probe is retained as an audit reference. Passing these
tests does not prove x86/x64 replay, save/load, CRC or lockstep equivalence.

## Native staging and guarded launch tooling

`scripts/native-runtime` is separate from the accepted x86 ZH workflow. It reuses
read-only path/tree/inventory/JSON utilities; the old PE32 guard is not relaxed.
Native Get-NativeBuild verifies8664/PE32+, correct debug-directory offset and MSF7
PDB GUID+age plus hashes for Generals/generalsv and ZH/generalszh. PE imports are
checked against actual native System32 components/UCRT routes; unexpected game
delay imports are rejected. System DLLs may themselves legitimately delay-load.
This is direct-import readiness, not exhaustive transitive-loader execution.

Staging copies title root BIG archives and Data assets from explicit installed
sources; excludes EXE/DLL/PDB overlays, MSS, movies and unrelated installation
files. Only the matched native EXE/PDB are added. First headless stage permits no
app-local DLL anywhere in game. No old shim/media/COM server is copied or registered.
ZH also stages ZH_Generals BIG/Data assets. Its diagnostic-only Win32BIGFileSystem
route loads that relative directory after ZH archives instead of consulting the
Generals InstallPath registry value. Archive precedence/overwrite policy and the
ordinary registry route are unchanged; the resolved base-asset path is receipted.
The manifests record source assets, hashes, dependencies and complete staged
inventory. Validation rejects runtime mutation and changed current build artifacts;
a rebuild requires a fresh stage. Source installations are read-only; no recursive
cleanup/removal operation is provided. Generated stages/receipts remain ignored.

The guarded launcher requires `-RunReviewedInitOnly`, refuses existing named games,
revalidates the entire stage, creates a unique run directory, writes launch.json
before Process.Start, launches one hidden child with exact argv/CWD and waits at
most120 seconds by default. On timeout only its owned Process handle is terminated,
with a bounded post-kill wait. exit.json records PID, start/error/timeout/exit status,
actual paths and gate result. It never launches another trial/title automatically.
PASS requires exit0, init-only/cleanup/WinMain/FP completion, no fail record and
matching actual path receipts. Failure evidence is preserved; no automatic rerun.

## Developer procedure (PowerShell7, first game execution remains separate)

Start in `C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode`.
The commands below use the existing local legal Steam installation. `ZH_Generals`
is its Generals asset directory. Use a fresh stage name if this name already
exists; never overwrite a completed or failed stage. No rebuild is required for
reviewed stages unless source/build artifacts have changed.

### Preparation and verification: does not launch either game

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
$assets = 'C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour'
./scripts/native-runtime/Test-NativeWorkflow.ps1
./scripts/native-runtime/Stage-NativeInitOnly.ps1 -Title Generals -Name x64-12-final -AssetSource "$assets\ZH_Generals"
./scripts/native-runtime/Stage-NativeInitOnly.ps1 -Title ZeroHour -Name x64-12-final -AssetSource $assets
./scripts/native-runtime/Test-NativeInitOnly.ps1 -Title Generals -Name x64-12-final
./scripts/native-runtime/Test-NativeInitOnly.ps1 -Title ZeroHour -Name x64-12-final
```

X64.12 runs preparation/preflight only. Once the stages below have already been
prepared locally, run just the two Test-NativeInitOnly commands; do not repeat Stage.

### First actual run: only after separate developer review

```powershell
./scripts/native-runtime/Invoke-NativeInitOnly.ps1 -Title Generals -Name x64-12-final -RunReviewedInitOnly
```

Expect no game window, menus, intro, movement or battle. Do not click or press keys.
The child must finish initialization and teardown, exit automatically, and report
PASS. On failure/timeout, stop, preserve the printed run directory and send its
reports; do not rerun or proceed to ZH automatically. If Generals passes, the
reviewed counterpart is:

```powershell
./scripts/native-runtime/Invoke-NativeInitOnly.ps1 -Title ZeroHour -Name x64-12-final -RunReviewedInitOnly
```

Send each title's manifest.json plus the exact run's launch.json, exit.json,
startup.log (if created), and userdata/CrashDumps files if any. Preserve the whole
run directory. A missing log is itself evidence; retain launch/exit receipts.

## Validation

- Full Win32 Release graph and CTest **23/23 PASS**; direct **149/title** with
  six existing disabled/title, focused75/title, sentence producer5/render2D15 PASS.
- Both actual native title links PASS, external D3DX helpers remain0/title;
  independent AMD64 CTest **13/13 PASS**. Native resource tests retain3 CPU
  passes and3 native-D3D8 device skips.
- Shared FP helper **4/4 per architecture**; startup gate **3/3 per architecture**
  covers strict grammar/default-off/refusal, flushed reads, isolated paths, bounded
  failure and evidence non-overwrite. Win32 rejects the native option.
- Source boundary/preservation tests **7/7**, native PowerShell workflow/guard
  checks **6/6**, scripts parsed **5/5**. Rendering Python28/performance Python16
  PASS; all six Win32/three native CPU resource streams remain byte-identical;
  compact reference hashes unchanged. Prior391 runtime/evidence files and final
  A.2 capture hashes preserved. Whitespace check PASS.
- Real native stages prepared/preflighted for both titles at
  `build/dev-runtimes/native/Generals/x64-12-final` and
  `build/dev-runtimes/native/ZeroHour/x64-12-final`. Both matched AMD64 EXE/PDB,
  title/base assets, complete inventory and28 startup import contracts pass.
  These are the final candidates for the guarded procedure above. Earlier local
  x64-12-init-only stages are preliminary retained evidence, not the run candidates.
- Logs, probes, staged files and receipts are ignored under `build/x64-12-*` and
  `build/dev-runtimes/native`; none belong in the source commit. Only intended
  source, tests, scripts and modernization docs are staged. Neither game launched.

No game initialization/dump-generation success or native gameplay/determinism
compatibility is claimed. The guard and logger tests run standalone test processes;
they do not execute a title. Native tool blockers outside the title targets remain
the historical X64.10 inventory; no unrelated tool fixes or broad native tool build
are claimed.

## Exactly one recommended next task

**Developer-reviewed first native init-only validation for Generals and Zero Hour.**
Use the guarded procedure, inspect the first failure before further execution,
and analyze receipts/checkpoints. Do not begin graphical renderer work, matches,
benchmarks or determinism claims as part of that first initialization check.
