# Modernization baseline

Latest evidence (2026-10-08): the developer also verified the Stage 2 correction
at checkpoint `b68b2e82f`: increasing render FPS no longer accelerates offline
gameplay; 60 FPS via Ctrl+numpad + gives smoother camera motion and normal vehicle/
gameplay timing. TPS is inferred, not instrumented. Startup Release `-fps 60`
was ineffective (its parser entry requires RTS_DEBUG). See
[STAGE3A_FPS_OPTIONS.md](STAGE3A_FPS_OPTIONS.md) for the new persistent render-cap
candidate, whose runtime/UI/persistence acceptance remains UNVERIFIED. Existing
baseline/Stage 2 runtimes are preserved; historical evidence follows.

Stage 2 evidence update (2026-10-08): the developer reports that the guarded
`build/dev-runtimes/zh/baseline/game` runtime starts, menus work, skirmish gameplay
appears normal at the default cap, and exit returns 0. No obvious audio/render/
asset problems were observed. All 369 source/runtime file paths, lengths and
SHA256s matched before and after that session. Agent read-only rechecks before
the Stage 2 build also pass. Preserve this runtime unchanged.

The developer also verified: **Raising the render/frame-rate cap using Ctrl +
Numpad + caused offline gameplay/simulation to speed up.** This does not establish
measured TPS, campaign/save/replay/mod coverage or retail compatibility.
See [STAGE2_TIMING.md](STAGE2_TIMING.md) for the candidate correction; historical
investigation/session statements below describe the earlier work only.

Stage 1 follow-up (2026-10-08): use the guarded helpers and manual checklist in
[STAGE1_WORKFLOW.md](STAGE1_WORKFLOW.md). They supersede the manual copy example
below. Actual Steam/build/user-data paths passed read-only validation; no real
runtime staging, user-data backup or game launch has been performed by the agent.

Investigation date: 2026-10-07 (Europe/Oslo). Source revision:
`adac468d732503ba50f1de21bbfc5e83982c5430`.

## Evidence and scope

This session inspects source, CMake, CI, history, public upstream documentation,
build artifacts, and selected installation/registry metadata. It changes only
these modernization documents. No game launch, benchmark, configure, rebuild,
installation, registry write, commit, or push was performed.

Labels used throughout: **observed** means directly inspected; **reported** means
provided by the developer; **inference** means reasoned from source; **UNKNOWN**
means not established. Compilation does not prove runtime or retail compatibility.

## Machine and build

| Item | Baseline |
|---|---|
| OS | Windows; exact edition/build UNKNOWN |
| Hardware | Developer describes modern high-end CPU/GPU; models, RAM and drivers UNKNOWN |
| IDE | Visual Studio Community 2026 / installation directory `Visual Studio/18/Community` |
| Compiler | Observed generated compiler metadata: MSVC `19.51.36260.0`, X86, pointer size 4 |
| Compiler path | `C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/bin/Hostx86/x86/cl.exe` |
| CMake / Ninja | Reported `4.3.1-msvc1` / `1.13.2`; CMake-generated compiler metadata corroborates CMake directory version |
| Generator | Observed `Ninja Multi-Config` |
| Build | Developer reports successful `4620/4620`, clean worktree afterward |

Run in the Visual Studio **x86 Developer PowerShell**, at the repository root:

```powershell
cmake --preset win32
cmake --build --preset win32
```

Do not troubleshoot or rebuild this known-good configuration without a reason.
`CMakePresets.json` uses external architecture selection: Ninja relies on the
compiler environment. A preset name alone does not select an x86 compiler.

Observed output includes:

- `build/win32/Generals/Release/generalsv.exe`
- `build/win32/GeneralsMD/Release/generalszh.exe` (6,633,984 bytes)
- Zero Hour PDB and `WorldBuilderZH.exe`, `W3DViewZH.exe`, `guiedit.exe`,
  `imagepacker.exe`, `mapcachebuilder.exe`, `wdump.exe`.

Cache: profiling, Tracy, ASan, tests, benchmarks, engine debug mode and FFmpeg
are OFF; retail compatibility is DEFAULT. `cmake/config-retail.cmake` explicitly
warns that compilers other than VC6 SP6 `12.00.8804` are not retail CRC compatible.
The MSVC build must not be described as retail multiplayer/replay verified.

## Git and safety

Observed initial worktree clean; branch `dev/modern-engine`.

| Remote | Fetch | Push |
|---|---|---|
| origin | `https://github.com/martinkraud/GeneralsGameCode.git` | Same developer fork |
| upstream | `https://github.com/TheSuperHackers/GeneralsGameCode.git` | `DISABLED` |

Keep `main` for upstream synchronization. Future focused feature branches may
branch from development; do not create one for this investigation. No force
push, history rewrite, shared rebase, branch deletion, mass formatting, generated
output commits, or Git configuration changes. Never push upstream.
Read `CONTRIBUTING.md` and `AI_POLICY.md` before future contributions; upstream
requires human accountability/disclosure and prohibits autonomous AI submissions.

## Development runtime: supported mechanisms and safest procedure

Protected source installation:

`C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour`

**Do not run CMake Install against the current cache.** The observed
`RTS_INSTALL_PREFIX_ZEROHOUR` points to that protected directory.
`GeneralsMD/CMakeLists.txt` installs directly to that destination, including PDBs
and enabled tools. `--prefix` alone is not a reliable override for this explicit
absolute destination. The upstream [MSVC guide](https://github.com/TheSuperHackers/GeneralsGameCode/wiki/build_with_msvc22)
uses installation beside game data; the following separate copy adapts that
layout using mechanisms present in this revision.

Recommended future steps (documented, **not executed**):

1. Close existing game processes. Create a new disposable runtime beneath the
   ignored build tree. Copy the complete legitimate installation, preserving
   relative directories. Use copies, not hard links or directory junctions, so
   test writes cannot reach the reference installation.
2. Preserve all `.big` archives, `Data`, `MSS`, and `ZH_Generals`. Base Generals
   assets are required as well as Zero Hour assets. Do not flatten archives or
   selectively omit language, patch, map, shader, audio or W3D packs.
3. Copy the freshly built `generalszh.exe` and matching PDB into the runtime
   root. This is the most predictable DLL search arrangement. Preserve original
   `binkw32.dll`, `mss32.dll`, and MSS provider files. Keep other DLLs on the
   first replication, recording their hashes and provenance.
4. Back up the existing Zero Hour user-data folder before a future launch, or
   use a dedicated Windows test account. A separate asset directory does **not**
   isolate user preferences, maps, saves, replays or crash output.
5. Launch the copied executable with an explicit runtime working directory.
   No mandatory special game argument was found for ordinary play. Optional
   `-nologo -noshellmap` reduces introductory work; retain defaults for the
   first comparable gameplay measurement.

Example PowerShell, after verifying the destination is new:

```powershell
$taskSource = 'C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour'
$taskRuntime = 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\runtime-zh-baseline'
if (Test-Path -LiteralPath $taskRuntime) { throw 'Use a new runtime directory.' }
New-Item -ItemType Directory -Path $taskRuntime | Out-Null
Get-ChildItem -LiteralPath $taskSource -Force |
    Copy-Item -Destination $taskRuntime -Recurse
Copy-Item -LiteralPath '.\build\win32\GeneralsMD\Release\generalszh.exe' -Destination $taskRuntime
Copy-Item -LiteralPath '.\build\win32\GeneralsMD\Release\generalszh.pdb' -Destination $taskRuntime
# Only after user-data backup or selection of a dedicated test account:
& "$taskRuntime\generalszh.exe" -setCwd $taskRuntime
```

Alternatively run the build executable with `-setCwd <runtime>`; DLLs must then
be resolvable through normal Windows loader search, so colocating the binary
with its runtime DLLs is preferable. `-useCwd` preserves the OS startup CWD.
Without either argument, startup selects the **executable directory**, so merely
setting a debugger's CWD while running from the build output is insufficient.
Evidence: `Core/GameEngine/Source/Common/CommandLine.cpp:461,1188,1427` and
`WorkingDirectory.cpp::setExecutableWorkingDirectory`.

### Data and registry caveats

`Win32BIGFileSystem::init` loads `*.big` from the working directory recursively,
then attempts original Generals assets from the legacy Generals `InstallPath`.
`loadBigFilesFromDirectory` skips the duplicate `Data/INI/INIZH.big`; preserve
that behavior and archive order. `FileSystem` also handles loose files and mods;
the copy must not silently acquire extra override INIs.

Observed Steam layout: 20 root `.big` files, root `Data`/`MSS`, and a
`ZH_Generals` subtree with base game archives. Root runtime DLLs include Bink,
Miles, `d3d8.dll`, DebugWindow, ParticleEditor, P2XDLL, patchw32 and steam_api.
Their mere presence does not prove they are all dependencies of this binary.

Selected read-only registry inspection found:

- HKLM 32-bit `.../EA Games/Command and Conquer Generals Zero Hour`:
  InstallPath = Steam root, Language = english, UserDataLeafName =
  `Command and Conquer Generals Zero Hour Data`.
- HKLM 32-bit `.../EA Games/ZeroHour`: installPath = Steam `ZH_Generals` subtree.
- Legacy `.../EA Games/Generals` absent in inspected HKCU, HKLM 32/64 locations.

Runtime `registry.cpp` uses the legacy Generals key for base assets and the long
Zero Hour key for language/user-data settings, preferring HKCU to HKLM. CMake's
Steam detection is separate; successful detection does not fix runtime registry
lookups. The recursive runtime scan can discover the copied `ZH_Generals`
archives. However a debug assertion still checks the missing legacy key.
**UNKNOWN:** whether every asset resolves correctly for this machine's SKU and
archive precedence, and whether debug startup requires a registry adjustment.
Do not silently create keys; establish Release startup first and report any
failure. No CD-key values were read.

`GlobalData::BuildUserDataPathFromRegistry` uses the Windows Documents known
folder (including redirection), plus UserDataLeafName or its default. No
user-data path CLI override was found. A dedicated Windows account is the
cleanest isolation without changing game registry settings.

Miles and Bink are loaded dynamically by name (`Dependencies/*/*Loader.cpp`).
Loader failure can disable their features; their presence is required for a
complete audio/video baseline. MSVC uses the DLL CRT; the matching **x86** VC
runtime must be available. Read-only `dumpbin /dependents` on this executable
observed `MSVCP140.dll`, `MSVCP140_ATOMIC_WAIT.dll`, `VCRUNTIME140.dll` and UCRT
API-set imports, plus Windows networking, input, windowing, shell, COM, registry,
IMM32, AVIFIL32 and WINMM DLLs. The static import list does not include Steam,
Bink or Miles. Direct3D is also dynamically loaded: `dx8wrapper.cpp:295` loads
`D3D8.DLL` and resolves Direct3DCreate8. Transitive dependencies and availability
on another test machine still require validation.
PDBs are for debugging, not required to play. Tools may have additional DLL needs.

No Steam API linkage/calls were found in the game build/source search. Inference:
the rebuilt executable is launched directly, without replacing Steam's launcher
or `Game.dat`. Steam overlay/launch integration is UNKNOWN. Existing `d3d8.dll`
and `d3d8.cfg` mean the local installation includes an app-local graphics layer;
its identity/version and performance impact are UNKNOWN. Preserve it initially,
then compare wrapper variants only in separate disposable runtimes.

## Baseline still to establish

Startup, skirmish/campaign, sound/video, save/load, mod loading, rendered replay,
retail replay CRC, modern-build replay CRC, LAN determinism, frame pacing,
display behavior and performance are **not tested in this session**. The minimum
asset set is UNKNOWN; a full owned copy avoids making an unsupported minimal
deployment claim. See [ROADMAP.md](ROADMAP.md) for controlled verification.
