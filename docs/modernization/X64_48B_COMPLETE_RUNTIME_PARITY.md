# X64.48B - complete MKRaudstein runtime parity in the overlay ZIPs

Status: **OPEN - packages rebuilt and validated automatically on a disposable layout; human retest on the second PC pending (section 8).**
Start point: HEAD `d7d6e2058` = `origin/dev/modern-engine`.

## 0. The requirement (supersedes every earlier "minimal ZIP" statement)
The earlier objective "a minimal player ZIP" is **cancelled**. Minimum size, minimum file count and minimum number of replaced files are **not** goals. For each title:

    validated staged MKRaudstein runtime  ==  clean legitimate Steam installation  +  the MKRaudstein ZIP overlay

Every completed modernization feature that works in the staged candidate must work after the ZIP is installed. A project-modified or project-generated runtime file is shipped at its relative path; "the exe starts without it" is not a reason to omit a file. Unchanged legitimate game files are not redistributed. Symbols (`.pdb`) stay in a separate optional archive. The package contents are **derived** from the canonical staged runtime, not from a hand-written allowlist.

## 1. Audit result: what the previous packages missed
The previous packages (X64.47A, X64.47B and the X64.48A rebuild `...-1ae0b0ca1`) were built from the executable plus a minimal allowlist. A file-by-file comparison of each staged candidate against the legitimate game files it was built from (SHA-256) shows:
| Title | files in the staged candidate that are NOT unchanged legitimate game files | in the previous ZIP? |
| --- | --- | --- |
| Zero Hour (`x64-48a-zh-v1`) | `generalszh.exe`, `generalszh.pdb` only | exe yes (pdb in Symbols) |
| **Generals** (`x64-48a-gen-v1`) | `generalsv.exe`, `generalsv.pdb` **and six generated project data files**: `Data/INI/CommandButton.ini`, `Data/INI/CommandSet.ini`, `Data/INI/PlayerTemplate.ini`, `Window/GenPowersShortcutBarUS.wnd`, `...China.wnd`, `...GLA.wnd` | exe yes, **the six data files: NO - omitted** |
The six Generals files are the "modern extensions" data overlay (X64.32; `Stage-NativeInitOnly.ps1 -ModernExtensions` runs `modern_extensions.py`): the Zero Hour style special-power shortcut sidebar for Generals (USA 8 slots instead of 5, China 6 instead of 5, GLA 5 instead of 3, with the superweapon, CIA, Spy Satellite and Radar Van buttons). They are loose files the game prefers over the same-named content inside its archives. **So the Generals ZIP that was tested on the second PC lacked a completed feature; its human PASS covers the launch / overlay mechanics only, not Generals feature parity.** Zero Hour has no project data files (its stage proves it); its previous ZIP was complete for what the stage contains.
No other runtime input was found: the Modern options, Enhanced HUD, display and settings code load no project data file (checked by search and by the stage comparison), user settings are created by the game, and `Stage-NativeOrdinaryPlay.ps1` adds nothing else.
Note for the project owner: the six Generals files are derived from the player's own retail archives (retail INI/WND text plus the sidebar changes; `modern_extensions.py` generates them from the installed game). Shipping them is what the parity rule requires; they are modified game data, not unchanged ones, so the "no unchanged EA files" rule is respected, but the legal position of redistributing derived data is a project decision to confirm before any public release.

## 2. How the packages are built now (`New-ZeroHourOverlayPackage.ps1 -Title -Candidate`)
Input: the name of the canonical staged candidate (an intact, preflighted stage). Every file under its `game` directory is classified against the stage manifest:
1. byte-identical to a legitimate game file -> **not redistributed** (counted in the manifest: Generals 130 files, Zero Hour 278 files);
2. the staged executable -> `Generals.exe` (the file Steam's Play button starts, see X64.47A);
3. the `.pdb` -> the optional Symbols archive;
4. run artifacts (`generals64.log`, crash dumps) -> ignored;
5. **anything else -> shipped at the same relative path** (a stage file that differs from its legitimate original is shipped too: "REPLACES the game file ...").
Plus what the stage gets from the machine and a clean PC may lack: the four app-local Microsoft Visual C++ runtime DLLs the executable imports (derived from its import table), the licence, the README and the manifest. Deterministic (same inputs, same bytes). Outputs next to the ZIP: `.sha256`, `.manifest.json` (every file with source, reason, ownership, replaced-or-new, size, SHA-256), `.stage-parity.json` (not distributed) and the optional `-Symbols.zip`. The README is generated from the derived list (replaced files, project data files and their purpose).

## 3. How the validator enforces it (`Test-ZeroHourOverlayPackage.ps1 -Title -Candidate`)
Fails when: a project runtime file of the staged candidate is absent from the ZIP; the ZIP holds a file the staged candidate does not have (other than the documented runtime DLLs / README / licence / manifest); a project file differs byte-for-byte; an unchanged legitimate game file (by path or by hash anywhere) is redistributed; the installed tree is not file-for-file equal to the staged candidate; an import does not resolve from the extracted folder; a fresh staging of the same build differs from the canonical candidate; the staged-candidate run or the overlay run fails; or the **feature probe** differs.
Flow: (1) package audit against the canonical staged candidate (computed live); (2) a disposable copy is staged with the stager from the read-only Steam assets and proven identical to the canonical candidate; (3) **reference run**: that staged runtime is normal-launched through the real screens (Generals: Skirmish, Zero Hour: General's Challenge) and its log kept; (4) it is turned into a **clean installation** (every project / generated file and run artifact deleted; only unchanged legitimate files remain, verified), the ZIP is extracted over it like a player does, and the installed tree is compared with the canonical candidate **file-for-file** (the executable under its Steam name; additions limited to the runtime DLLs, README, licence, manifest); (5) imports resolve with the VC++ runtime from the extracted folder; (6) **overlay run**: the extracted `Generals.exe` is started without arguments, with a Windows-only `PATH`, through the same screens, and its log must show the same **feature probe** as the reference run.
**Feature probe** (new `feature_probe` log lines, written at the match frame): for every playable side the special-power shortcut slot count, the shortcut command set and the number of command buttons it really holds, and the shortcut window. It is the runtime's own report of the completed data-driven feature, so a missing or different data file shows up as a different line. Generals reports America 8 / China 6 / GLA 5 slots with 8 / 6 / 5 buttons (retail: 5 / 5 / 3); Zero Hour reports its 13 sides' own data.
`Test-OverlayValidatorDetectsOmissions.ps1` proves the validator rejects a tampered package: an omitted project file, a redistributed legitimate game file, a modified project file (both titles: PASS; the good package passes the same audit).
Contract tests (`test_overlay_package_contract.py`, 4 tests): the builder takes a canonical candidate and classifies the stage (no hand-written project-file list, no size goal), the validator contains the missing-file / file-for-file / feature-parity failures, the runtime emits the probe, and the Generals extension files are part of the stage.

## 4. The packages (source commit `08d74caaa`; later commits: documentation and evidence only)
| | Zero Hour | Generals |
| --- | --- | --- |
| Canonical staged candidate | `x64-48b-zh-v2` (preflight PASS) | `x64-48b-gen-v2` (preflight PASS) |
| ZIP (`build\dist\`) | `MKRaudstein-ZeroHour-x64-Preview-08d74caaa.zip` | `MKRaudstein-Generals-x64-Preview-08d74caaa.zip` |
| Size | 4,116,364 bytes | 4,010,848 bytes |
| SHA-256 | `D169D90B3407FF057809CA3784C4F95FDC7DC15936560E5AC3434DE7F9764E76` | `02B3A18669AEEAD7FC330175FB76CAC23B298A1FF09FB9EB29862D62E1C2FC64` |
| Files | 8 (see below) | 14 (see below) |
| Optional symbols | `...-Symbols.zip` | `...-Symbols.zip` |
Native candidate on the developer PC: `build\dev-runtimes\native\ZeroHour\x64-48b-zh-v2\game\generalszh.exe` (launcher default: `Run-ZeroHourCandidate.cmd`, equivalent `-Name x64-48b-zh-v2`); Generals: `build\dev-runtimes\native\Generals\x64-48b-gen-v2\game\generalsv.exe`.
**Zero Hour (8):** `Generals.exe` (8,634,880 B, SHA-256 B5960DE5...28AF, replaces the EA launcher), `vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll`, `msvcp140_atomic_wait.dll`, `MKRaudstein-ZeroHour-README.txt`, `MKRaudstein\LICENSE.md`, `MKRaudstein\manifest.json`. 278 legitimate game files not redistributed.
**Generals (14):** the same eight kinds for Generals (`Generals.exe` 8,281,600 B, SHA-256 69EC77A2...15E9, replaces the EA launcher; README `MKRaudstein-Generals-README.txt`) **plus the six project data files** `Data\INI\CommandButton.ini` (114,167 B), `Data\INI\CommandSet.ini` (25,277), `Data\INI\PlayerTemplate.ini` (4,959), `Window\GenPowersShortcutBarUS.wnd` (55,433), `Window\GenPowersShortcutBarChina.wnd` (42,410), `Window\GenPowersShortcutBarGLA.wnd` (35,851). 130 legitimate game files not redistributed.
**Replaced in an installation:** `Generals.exe` only (both titles); the six Generals data files are new loose files that override same-named content inside the game archives (`Verify integrity` does not remove them; deleting them restores retail behaviour of the sidebar). A game file that a future stage modifies would also be listed as REPLACES.

## 5. Automated validation (disposable layout; this is NOT the human retest)
`Test-ZeroHourOverlayPackage.ps1`: **26 of 26 checks passed for each title**, including: ZIP == manifest; ZIP built from this canonical candidate; no project file missing; byte-identical project files; nothing extra; no legitimate file redistributed; AMD64; only the runtime DLLs; fresh staging identical to the canonical candidate; reference run clean; clean installation verified; **file-for-file: clean installation + ZIP == the staged candidate** (Generals 144 files, Zero Hour 286); all 31 imports resolve with the runtime DLLs from the extracted folder; overlay run exit 0 with `normal_launch`, shell reached, the real screens driven to a match, no failed phase; **feature probe identical** (Generals 3 lines, Zero Hour 13) and identical screen steps. Evidence: `docs/modernization/evidence/x64-48b/`.

## 6. Contract and test status
Python native-runtime suites: all tests OK (new: `test_overlay_package_contract.py`). The rest of the matrix is unchanged by this packaging-only work (the only runtime change is the `feature_probe` log lines in the capture aid; Win64 builds, gtests and contracts re-run: see the final report).

## 7. Known limits
The validator proves the ZIP equals the staged candidate and that the data-driven feature probe is identical; features that depend only on the executable are covered by the executable being byte-identical. Behaviour that only a human can judge (looks, feel, the real Steam launch) needs the retest. The embedded build path and the unsigned executable remain release-hygiene items (X64.51).

## 8. Human retest (second PC) - replaces the Generals and Zero Hour ZIPs of X64.48A
Install both new ZIPs over the Steam installations (extract, replace `Generals.exe`, Steam Play), then:
1. **Generals only - the shortcut sidebar (new in this ZIP):** start a Skirmish as USA, China and GLA; the special-power shortcut bar must show the extended slots (USA up to 8, China 6, GLA 5): after building a Particle Uplink Cannon / Nuclear (Neutron) Missile launcher / SCUD Storm / CIA camp / Radar Van, their buttons must appear on the bar (retail shows only 5/5/3 slots and never these).
2. The three X64.48A checks (fog of war, 5120x1440 world, eight Skirmish slots) in both titles - the executables are the ones from the X64.48A fixes.
3. Report anything that differs from the staged build you tested on the developer machine.
