# Stage 1: development runtime laboratory

Stage 2 follow-up: the developer has now manually validated the real `baseline`
runtime (startup/menus/skirmish/exit 0 and 369-file integrity before/after).
Keep it unchanged. Use [STAGE2_TIMING.md](STAGE2_TIMING.md) for the timing candidate
and a new runtime. The Stage 2 candidate defaults logic scaling enabled at 30;
the manual scale-key press counts below describe the original baseline only.
Historical NOT RUN statements below describe Stage 1 implementation, not this
later developer-reported session; broader tests remain open.

Infrastructure implementation, 2026-10-08 (Europe/Oslo). No engine/data/INI changes, build,
real game launch, registry writes or CMake installation were performed. The
initial investigation remains the evidence baseline; this document supersedes
its manual copy example with guarded helpers.

## Prerequisites and layout

Use **PowerShell 7 on Windows** (`pwsh`); Windows PowerShell 5.1 is not supported
by these helpers. Run at the repository root. Staging/backup do not require the
Visual Studio environment; later compilation does. Close all game processes.

Source must be supplied explicitly and must have the expected Steam directory
shape and major assets. Structural checks cannot authenticate ownership or
certify an unmodified install. The actual inspected source contains an existing
graphics wrapper and development DLLs, so call this a **local Steam-installation
baseline**, not a pristine retail baseline.

```powershell
$source = 'C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour'
./scripts/zh-runtime/Stage-ZHRuntime.ps1 -SourcePath $source -Name baseline -ValidateOnly
./scripts/zh-runtime/Backup-ZHUserData.ps1 -ValidateOnly
```

Validation performs no writes or launch. It checks installation shape/assets,
source tree for reparse points, destination guards and actual x86 EXE/PDB GUID
and age pairing. It does not hash/copy the entire real installation in dry-run.
Observed: 388 source entries, 3,035,133,563 file bytes; matching Release PDB.

When ready to create the full disposable copy:

```powershell
./scripts/zh-runtime/Stage-ZHRuntime.ps1 -SourcePath $source -Name baseline
```

Default staged game directory:

`<repo>/build/dev-runtimes/zh/baseline/game/`

Metadata is beside `game`, avoiding interference with recursive game asset
loading: `manifest.json`, `source-files.json`, `runtime-files.json`. The manifest
records revision, dirty worktree listing, script hashes, PowerShell version,
relevant build cache values, matching debug identity, SHA256s, important DLL/
wrapper hashes and UTC timestamps. Every copied file is hash-verified. Source
inventory is rechecked before a completed manifest is written.

Only `win32` and `win32-profile` Release artifacts can be selected. Existing
source game files are copied, then **only the staged** generalszh.exe/PDB are
overlaid with the selected development build. No links, registry edits, install
target, source executable replacement or build-cache changes are used.

Destinations are derived from a restricted single-component `-Name`; there is
no arbitrary destination parameter. Drive paths, traversal, separators and
Windows device names are refused. Existing destinations are refused. Use a new
name, or explicitly `-Refresh`: if the name already exists, this creates a
**new timestamped sibling** and prints its name. It never deletes/overwrites
the old runtime. An interrupted copy remains for inspection without a completed
manifest and cannot be launched through the helper. Use a fresh name to retry.

Reparse points in source, destination ancestors, staged tree and user data
are refused rather than followed. Keep these directories idle while operating;
these scripts do not defend against another process maliciously racing path
replacement. Keep Steam updates/verification and game/cloud-save writers idle
while hashing or copying. Sufficient disk space is needed for the complete
installation plus PDB and backups; partial copy failure leaves the source intact.

## Shared user data and backup

The resolver mirrors the x86 engine: Windows Documents known folder plus
UserDataLeafName, querying **32-bit** HKCU then HKLM at the long Zero Hour key,
falling back to `Command and Conquer Generals Zero Hour Data`. It reads only that
value, not CD keys. Unexpected leaf names, access errors, unavailable Documents
or links fail closed. There is no registry/path redirection or invented isolation.

Observed here:

`C:\Users\buskr\Documents\Command and Conquer Generals Zero Hour Data`

Inspect the printed path. A dedicated Windows test account remains the cleanest
isolation. A runtime asset copy alone does not isolate options, maps, saves,
replays, network preferences or crash output.

Standalone backup, with no game launch:

```powershell
./scripts/zh-runtime/Backup-ZHUserData.ps1
```

This creates a new GUID/timestamp directory under
`build/dev-runtimes/userdata-backups/`. It copies the complete user-data tree,
preserves empty directories, verifies SHA256s and rechecks the original for
concurrent changes, then writes `backup.json`. If user data is absent it records
that fact without creating a user-data directory. Existing user files/backups
are never deleted or overwritten. Failed backups remain incomplete for inspection.
There is no automatic restore: compare the backup first and manually restore
only selected files with the game closed. Backups may contain private data;
they remain local and ignored by Git.

The launch helper requires protection **on each invocation**, so a stale receipt
from another account/path cannot silently unlock future launches:

```powershell
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name baseline -ValidateOnly
# Explicit manual launch; makes and verifies a fresh backup first:
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name baseline -BackupUserData
# Optional first windowed test:
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name baseline -BackupUserData -GameArguments @('-win')
```

If intentionally accepting shared-data writes (for example in a dedicated test
account), use `-AcknowledgeSharedUserData` instead. This is an explicit risk
acknowledgement, not a claim of isolation. The helper will not launch without
either switch. Running the standalone backup does not permanently unlock launch;
the integrated switch takes a fresh backup or use an explicit acknowledgement.

Launch checks the completed stage, no reparse points, major assets and staged
EXE/PDB hashes. It launches only the staged EXE with OS CWD and `-setCwd` both
pointing to `game`; extra `-setCwd`/`-useCwd` options are rejected. Arguments are
passed as a native argument array, without shell interpolation; the exact array
is printed. Launch/exit receipts go beside the manifest. The helper waits for
exit and returns the game exit code. A nonzero exit code requires investigation;
zero alone does not establish content or gameplay correctness.

## Confirm source and runtime integrity

Before and after each first manual test session:

```powershell
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name baseline -CheckSource
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name baseline
```

The source check compares **every source file path, length and SHA256** to the
staging snapshot and detects added/deleted/changed files. The runtime check does
the same against its post-overlay snapshot. Runtime differences such as debug
logs may be expected after play; record them rather than treating all drift as
corruption. Source differences require explanation (including possible Steam
updates); never repair/overwrite Steam through these tools. Empty directory and
filesystem ACL/timestamp identity are not certified by the file-hash comparison.
Nothing can reconstruct a pre-staging source baseline retrospectively.

## First manual baseline sequence

All gameplay entries below start **NOT RUN**. Record Pass/Fail/Blocked/N/A,
UTC start/end, evidence and exact hashes in an ignored result file such as
`build/dev-runtimes/results/baseline-<timestamp>.md`. Create that results directory
manually when needed. Do not put logs/saves/traces/manifests in tracked docs.

1. Validate, stage, inspect manifest, run source/runtime hash checks. Confirm the
   printed user-data location and backup success. Record existing options,
   graphics wrapper/config hashes, driver and display refresh/DPI. Launch
   manually at the original/default cap; **do not raise FPS in Stage 1**.
2. **Startup/video:** watch the intro/logo/video instead of suppressing it;
   verify image and sound, load screens and absence of crash/missing assets.
   Check Bink playback separately from menu music. Record disabled/missing video
   explicitly; successful startup does not prove video support.
3. **Menus:** open Options, skirmish and campaign selectors; check text, cursor,
   tooltips, navigation and return paths.
4. **Skirmish:** choose and record a fixed map, faction, opponent AI/difficulty,
   starting settings and camera. Build/move/select units, fire projectiles, observe
   particles, shadows and shroud. Record seed if exposed; otherwise UNKNOWN.
5. **Audio:** check music, unit acknowledgements, positional sound, combat sounds
   and speech. Pause/resume; note device/volume settings.
6. **Save/load:** create a uniquely named *development* save, note path/hash,
   load it and continue play. Never overwrite a personal save. Retail save
   compatibility remains a separate test.
7. **Campaign:** start one recorded mission, check objectives/scripts/cinematics,
   play through a reproducible segment and exit to menu.
8. **Replay:** record a short modern-build match, exit, reopen the same-build
   replay, check playback and completion. Store map/replay hashes and any CRC
   diagnostics. Do not infer retail compatibility.
9. **Display:** test windowed (`-win`) and fullscreen/default separately,
   recording actual mode/size; saved options can affect fullscreen behavior.
   Test menu resolution selection and a conventional 16:9 mode. Then manually
   test `-GameArguments @('-xres','5120','-yres','1440')`, checking actual back
   buffer, FOV, HUD/text/cursor/selection/picking, menus and performance. Record
   unsupported display/mode as Blocked. Restore the original choice manually.
10. **Mod (if available):** use a fresh stage name and `-mod` with a known owned
    mod path, recording exact mod/map hashes/version. Do not install or edit mod
    files in Steam or change INIs for this task. If none is available mark N/A.
11. **Clean exit:** exit normally, record helper exit code, check process ended,
    inspect crash/log locations and created user/runtime files. Repeat source
    SHA256 verification to confirm Steam content matches the staging snapshot.

Crash/log collection: `ReleaseCrashInfo.txt` is documented in the game's
user-data directory; MiniDumper receives that path from WinMain. Debug logs,
when compiled in, are based on the **executable directory**, with DebugLogFile/
Prev and possible instance suffixes (`Common/System/Debug.cpp`). Inspect both
the resolved user-data tree and staged `game` directory. Exact dump/log filenames
and availability depend on configuration and remain unverified. Preserve first
failure evidence before relaunch; the engine can rotate/overwrite its own logs.

## Later Tracy preparation, separate from known-good Release

These commands are documented for later developer use, **not executed here**.
Use the x86 Visual Studio Developer PowerShell at the repository root:

```powershell
cmake --preset win32-profile
cmake --build --preset win32-profile
./scripts/zh-runtime/Stage-ZHRuntime.ps1 -SourcePath $source -Name profile-baseline -BuildPreset win32-profile -ValidateOnly
./scripts/zh-runtime/Stage-ZHRuntime.ps1 -SourcePath $source -Name profile-baseline -BuildPreset win32-profile
```

This uses `build/win32-profile`, preserving `build/win32`. Never invoke Install.
Use Tracy profiler **v0.13.1** as identified by README and cmake/tracy.cmake.
Start its viewer, then manually launch the profile stage with a fresh backup;
connect to the game and save captures under ignored `build/dev-runtimes/results`.
No viewer download/installation or capture is performed in this task.

Record an uninstrumented Release reference and an instrumented Release-profile
run of the same scenario, resolution, cap, speed, wrapper, camera and warmup.
Tracy frame images/readback and legacy profiling can add overhead. README notes
possible dbghelp.dll conflicts: if encountered, investigate and test a **new
copied runtime only**, recording the variation; never remove a Steam DLL.

Collect at least three runs, 30 seconds warmup then a fixed 120-second segment:

| Measurement | Definition / qualification |
|---|---|
| FPS | Render/client frames divided by sampled elapsed seconds; record source of count |
| Frame times | Individual frame durations; median, histogram and p95/p99; state percentile method (e.g. nearest rank) and excluded warmup/stalls |
| Logic ticks/second | Count completed GameLogic::update ticks / elapsed time, not FPS or an on-screen target setting |
| Client/logic/render | Tracy/legacy scopes where actually present; separate inclusive/exclusive timings |
| Present/wait | Record available scopes/sampling; separate blocking Present from limiter sleep/spin; mark unavailable coverage UNKNOWN |
| CPU/GPU | Observe per-core CPU and GPU utilization/clock/power plus process totals; name tool/sample interval; utilization alone is not a bottleneck proof |
| Environment | Resolution/window mode/refresh/DPI, FPS cap, effective logic policy, wrapper/config DLL hashes, hardware/driver/OS, profiler options |
| Provenance | EXE/PDB, data inventory, mod/map/replay hashes, revision/dirty state/compiler/build flags; map/seed/camera/unit counts as available |

The existing `PROFILER_FRAME_MARK` is in GameClient::update. Logic has a profiler
scope in GameLogic::update; count actual invocations, verifying paused/new-game
early returns instead of equating every scope with a completed tick. If the
viewer cannot identify completed ticks reliably, use a debugger/available
diagnostic to correlate m_frame without changing engine code, or mark the metric
UNKNOWN. A future diagnostic gap is a separate Stage 2 task. Do not claim any
bottleneck from source inspection or average FPS alone.

## Stage 2 preparation: existing controls and controlled matrix

**Source-verified, not runtime-tested:** MetaEvent.cpp::generateMetaMap defaults
unmapped events to these keys and marks them usable everywhere. Zero Hour
GameEngine.cpp calls it after loading mappings. Existing data/mod mappings may
override defaults. CommandXlat handles the events without a debug-build guard.

- Ctrl+numpad `+` / `-`: next/previous render cap; on-screen message.
- Ctrl+Shift+numpad `+` / `-`: offline logic time scale in increments of 5;
  on-screen message shows requested/actual rate and ratio. Rejected in network games.
- Logic scaling is enabled only when its value is **less than** render cap.
  Release minimum is 30. At render cap 30, scale cannot be enabled at 30; normal
  behavior is one tick per iteration and must be measured as such.

Start each experiment fresh, with capped rendering enabled and no fast-forward.
Set the cap at a menu before starting gameplay, then decrement logic scale until
the message shows 30, actual 30, ratio 1.00. Do not time the setup interval.
If the mapping/message does not work in the local data, stop and report it; do
not edit mappings/INIs. There is no verified CLI/persistent setting to atomically
select a fixed offline logic rate at startup. That reproducibility/setup gap is
the first likely Stage 2 implementation item if existing keys are insufficient.

| Render cap | Logic policy | Decrease presses from fresh disabled state at that cap |
|---:|---|---:|
| 30 | Legacy coupling, nominal 30 ticks/s at achieved 30 FPS | 0; enabling equal-rate scale is not supported |
| 60 | Enable scale and lower to 30 | 6 |
| 120 | Enable scale and lower to 30 | 18 |
| 144 | Ceil initial logic value to 145, lower to 30 | 23 |
| 240 | Enable scale and lower to 30 | 42 |

Press counts are source-derived expectations, not a substitute for the on-screen
value, enabled state and measured tick count. Key repeats can skip counts.
Changing the render cap, speed, pausing, loading or starting another scenario
requires rechecking state. At 144/30, the 4.8 render/logic ratio exercises a
non-integer relationship; add 50/30 or 75/30 as optional confirmation.

For **each row**, use the same map/seed or same-build replay/camera and repeat
the capture window. Record gameplay speed using a fixed game-time/build/movement
event versus wall time and count completed logic ticks. Inspect animations,
camera motion, particles, unit/projectile transforms, input response/selection,
pause/resume, speed changes then return to 30, save/load, replay playback,
minimize/restore and Alt-Tab/device-loss recovery. Test actual fullscreen and
windowed separately. Check long stalls and achieved FPS below 30: current
scheduler clamps accumulation and cannot catch up multiple ticks per iteration.
These tests may demonstrate a remaining limitation; do not label it passed
because the desired cap was selected.

## Compatibility result separation

**A — modern same-build:** use identical modern EXE/data/flags to record/play
replays and compare repeated runs at logic-frame-indexed CRCs. Compare rendered
and headless runs separately; live input tests are also required. Later headless
command through the protected helper (after map/replay placement in test user
data) is:

```powershell
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name baseline -BackupUserData -GameArguments @('-jobs','4','-headless','-replay','stage1/*.rep')
```

**B — retail/VC6:** keep distinct results/runtimes for the documented optimized
VC6 SP6 12.00.8804, engine debug OFF reference and existing GeneralsReplays corpus.
The Stage 1 MSVC staging selector does not silently switch to VC6. Modern MSVC
replay success or failure is not a retail compatibility verdict.

`-VerifyClientCRC`, `-DebugCRCFromFrame`, `-LogObjectCRCs`, and
`-NetCRCInterval` are build-guarded diagnostics; the normal profile preset does
not imply they are enabled. Check Debug.h/CMake flags before use and document
any separate diagnostic build. Avoid `-SaveDebugCRCPerFrame` for routine tests:
its source comments say an existing output directory is deleted. No destructive
CRC-output command is part of this workflow. Preserve RNG, command frames,
simulation scheduling, wire formats and serialization unchanged.

## Verification and limits

Run safe infrastructure checks with:

```powershell
./scripts/zh-runtime/Test-ZHWorkflow.ps1
```

The suite uses generated placeholder assets plus copies of the matching existing
EXE/PDB under ignored build directories. It validates copy integrity/empty
directories, guarded names/arguments, no-write dry-runs, existing-destination
refusal, non-destructive refresh planning, launch acknowledgement gating and
drift detection, and absent/empty/populated synthetic user-data backups. All 29
assertions passed during implementation. It never starts a process for the game, edits registry, deletes
fixtures or backs up the real user-data tree. **Never launch synthetic test
runtimes.** Tests retain them for inspection.

No full real asset stage or real user-data backup was created in implementation.
No real game test, profiling capture, retail validation or Stage 2 experiment
has run. Unknowns include actual startup/assets/legacy registry fallback,
graphics-wrapper identity/effect, CRT availability at launch, save/mod/replay
behavior, 5120x1440 correctness, real hotkey overrides and effective tick rate.
Stage 1 helpers are ready for the developer's guarded staging and first manual
launch, subject to successful full copy and fresh user-data protection.

All generated files live under the already ignored `/build*` tree. `.gitignore`
needs no change. Do not force-add these artifacts, private backups, paths or logs.
