# X64.48A - second-PC correctness pass (fog of war, ultrawide world, Skirmish slots 7/8)

**SUPERSEDED 2026-10-18 by `X64_48B_COMPLETE_RUNTIME_PARITY.md`:** the "minimal player ZIP" objective is cancelled; the distribution rule is *validated staged runtime == clean installation + ZIP overlay*, and the package is derived from the staged candidate. The rebuilt ZIPs in section 11 (`...-1ae0b0ca1`) are superseded: the Generals one still lacked the six project data files; the executables inside are the ones of the X64.48A fixes and are unchanged.

Status: **OPEN - three correctness defects fixed and validated automatically on a disposable layout; human retest on the second PC pending (section 9).** Not closed until the human confirms. No performance work, no Enhanced HUD, no UI redesign, no gameplay/AI/RNG/replay change.
Start point: HEAD `4fa91cbdd` = `origin/dev/modern-engine`.

## 0. Human result that triggered this (second PC, Samsung Odyssey G9, 5120x1440)
- **Generals preview ZIP: extracted over the Steam installation, launched through Steam Play: HUMAN PASS.**
- **Zero Hour preview ZIP: extracted over the Steam installation, launched through Steam Play: HUMAN PASS.**
- Therefore the basic *ZIP -> overlay -> Steam Play* distribution model is **human validated on a second PC for both titles** (no missing-DLL / runtime / startup blocker; the app-local Visual C++ DLLs, the `Generals.exe` replacement and the Steam launch chain worked).
- The packages are **not public-release ready**: the three defects below and the release-hygiene items (build path in the executable, unsigned executable) remain.
- Reported defects: (1) fog of war did not seem to work, (2) the world did not fill the 32:9 display (black regions) in Windowed and Borderless, (3) with eight AI players the 7th and 8th Skirmish slots misbehaved.

## 1. Method (nothing assumed from the report)
New developer-only capture aids (all off by default, normal launch only): `GENERALS_FORCE_CLIENT_SIZE=WxH` presents at a client size the monitor cannot show (the developer PC is 1920x1200, so 5120x1440 and 3440x1440 were otherwise impossible), `GENERALS_AUTOSHOT_LOOKAT=center` puts the camera over the middle of the map, `GENERALS_AUTOFLOW=slots` fills the Skirmish slots and probes them, and a `view_footprint` log line records where the view lands on the ground and the shroud state. Scripts: `Invoke-NativeAutoCapture.ps1 -ClientSize -LookAtCenter -Flow Slots`, `Measure-NativeWideDisplay.ps1` (black pixels and fog luminance per size), `Test-NativeSkirmishSlots.ps1`. Windowed and Borderless differ only in how `DisplayConfig::resolve` picks the client size; the render chain below the client size is identical, so the forced-size captures cover both.

## 2. Defect 2 - ultrawide: black bands on both sides of the world
**Reproduction.** `Invoke-NativeAutoCapture.ps1 -Title Generals -Flow Skirmish -ClientSize 5120x1440` (Zero Hour identical): the 3D world is drawn at full width, but a curved black band cuts off both sides; **22.4 % of the world view was pure black** at the start position (1920x1080: 0.11 %). With `-LookAtCenter` (camera over the middle of the 4430 x 4340 map, the whole view footprint inside the map) it was still **25.1 %** black, which excludes "the map edge".
**False lead, reverted.** A first hypothesis was the retail camera-area constraint (it uses the vertical extent for both axes). Making it stricter did change the camera but hid the player's own base off-screen and did not remove the black; it was reverted before any commit. The footprint log (the ground point under each screen corner) proved the black was inside the map.
**Root cause.** `W3DView::getDesiredTerrainDrawSize` returns the retail 4:3 terrain draw window (`NORMAL_DRAW_WIDTH` = 129 cells = 1290 world units across, 257 at low pitch). Modern Hor+ keeps the 4:3 vertical field of view, so a 32:9 view sees about 2.7x more ground sideways (2400 units at the far row); everything outside the draw window is simply not drawn.
**Fix** (`Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp`, `getDesiredTerrainDrawSize`; `W3DView.h`): when Modern, the window is sized to the ground the view really shows - the four screen corners at the pivot height, per world axis (so a rotated camera is covered), rounded up to whole vertex-buffer tiles, capped at 513 cells, **only growing** (a resize rebuilds the terrain buffers) and reset with the view. Never smaller than the retail size, so Classic and 4:3 are untouched. The far clip plane follows the draw region automatically (existing code). No stretch, no crop, no per-aspect constants: the projection (Hor+, X64.33) was already correct.

## 3. Defect 1 - fog of war not visible
**Reproduction / measurement.** Mean luminance of the world view with the native shroud on versus `-Disable shroud`: 1920x1080 **83.1 vs 101.5** (the overlay works), 5120x1440 **99.3 vs 99.6** (the overlay is not drawn at all). The visibility state was correct (the shroud cells at the probe points held the expected levels: fogged 127, clear 255, unexplored near the map edge; in multiplayer-style Skirmish the whole map starts "explored/fogged" and only your units' surroundings are clear, as in the original, `MultiplayerSettings UseShroud`). So the state was right and the pass was lost.
**Root cause.** `CollectShroud` splits the visible rectangle into bands sized by *vertices* only (`59000 / width`). One object-pass chunk (`kChunkVertices = 60000`) also holds at most `kChunkIndices = 180000` indices, i.e. 30000 lattice cells; a wide Hor+ view (or a zoomed-out 16:9 one) produced a band of up to ~59000 cells, `emitEffect` refused it (`b.indexCount > kChunkIndices`) and the whole shroud overlay of that frame was dropped (counted in `effects dropped`, otherwise silent). The second PC's 32:9 surface hit it at the default camera; it is a latent defect for any zoomed-out view.
**Fix** (`NativeWorldEffects.cpp/.h`): `Geometry::ShroudBandRows(cellsAcross)` honours both chunk limits and `CollectShroud` uses it. Nothing else in the shroud path changed (levels, filtering, colour, layer).

## 4. Defect 3 - Skirmish slots 7 and 8
**Reproduction.** `GENERALS_AUTOFLOW=slots` pushes the real Skirmish screen, fills slots 2-8 with AI through the real combo boxes, and for each slot opens the drop-down (the same `GBM_SELECTED` the button sends) and asks the *real mouse lookup* (`findWindowUnderMouse` with the three status passes and the "no input" discard of `winProcessMouseEvent`) who owns the centre of every row. **Generals, 1920x1080, 2560x1440 and 5120x1440: slots 1-5 reach 5/5 rows, slot 6 (7th player) 4/5, slot 7 (8th player) 3/5.** Zero Hour: 5/5 for every slot at every size.
**Root cause.** The Generals `SkirmishGameOptionsMenu.wnd` contains `StaticTextStartingCash` (flagged *no input*, 175x42 at 1920x1080: x 360-535, y 608-650) which lies over the lower part of the drop-down lists of the last two slots. The combo window grows to include its open list, but `findWindowUnderMouse` tries siblings in creation order: the label comes first, its rectangle covers the row, it is returned, and `winProcessMouseEvent` then discards it because it takes no input - the click is lost. (Probe output: `covering_siblings ... StaticTextStartingCash ... ComboBoxPlayer7`.) It is **Generals-specific data**; Zero Hour's layout never overlapped. It is independent of the display size and of the Modern layout (same result at three sizes), and it is not the Enhanced HUD tooltip mechanism.
**Fix** (`Core/GameEngine/Source/GameClient/GUI/GameWindowManager.cpp`, `findWindowUnderMouse`, one reviewed hunk recorded in `x64_33_deltas.py` so the byte-identical-to-baseline contract keeps guarding the rest of the file): the open combo box (`m_loneWindow`, the manager's own "the one window that just opened") is tried first when the point is inside its rectangle. It only acts while a combo is open and enabled; everything else, including the `NO_INPUT` rule, is unchanged.

## 5. Files and functions changed
`W3DView.cpp/.h` (`getDesiredTerrainDrawSize`, `reset`, constructor), `NativeWorldEffects.cpp/.h` (`ShroudBandRows`, `CollectShroud`, `shroudSummary`), `GameWindowManager.cpp` (`findWindowUnderMouse`), `W3DDisplayOrdinaryHost.cpp` (capture aids only: size override, look-at-centre, view footprint, `slots` flow), `Invoke-NativeAutoCapture.ps1`, `Measure-NativeWideDisplay.ps1`, `Test-NativeSkirmishSlots.ps1`, tests below.

## 6. Tests added
- `NativeWorldEffects.ShroudBandsFitTheObjectPassChunkAtEveryWidth` (gtest, Win64): the band size fits 60000 vertices and 180000 indices for widths 1..4000 cells.
- `test_ultrawide_contract.py` (3 tests): the Modern draw window is sized from the view, grows only, resets, and Classic gets the retail sizes; the shroud bands use `ShroudBandRows` and the chunk constants it depends on; the size override is inert without its variable.
- `test_skirmish_slots_contract.py` (2 tests): the open combo box is consulted before the top-level scan and the `NO_INPUT` rule is intact; the probe uses the real mouse lookup and the script covers 1920x1080 / 2560x1440 / 5120x1440.
- Runtime: `Test-NativeSkirmishSlots.ps1` (disposable stage; fails unless every probed slot is reachable) and `Measure-NativeWideDisplay.ps1`.
- The preservation registry (`x64_33_deltas.py`) lists exactly the one new reviewed hunk; `test_deltas_module_lists_only_reviewed_files` names the file. No contract was weakened.

## 7. Before / after (automated, disposable stage, developer PC; this is not the human retest)
World view, pure-black pixels (%) and mean luminance with the fog of war on / off (lower on = fog drawn):
| Title / size | before: black % | before: fog on / off | after: black % (start view) | after: fog on / off | after: black % (map centre, fog on) |
| --- | --- | --- | --- | --- | --- |
| Generals 1920x1080 | 0.11 | 83.1 / 101.5 | 0.11 | 82.9 / 101.4 | - |
| Generals 2560x1440 | 0.11 | - | 0.11 | 83.1 / 101.3 | - |
| Generals 3440x1440 | 5.31 | - | 2.44 | 74.0 / 100.1 | 0.08 |
| Generals 5120x1440 | **22.39** (25.08 at the map centre) | **99.3 / 99.6 (fog missing)** | 13.03 | **59.8 / 99.8** | **0.05** |
| Zero Hour 1920x1080 | - | - | 0.15 | 85.2 / 102.4 | - |
| Zero Hour 2560x1440 | - | - | 0.17 | 84.9 / 102.7 | - |
| Zero Hour 3440x1440 | - | - | 3.47 | 75.3 / 102.2 | 0.02 |
| Zero Hour 5120x1440 | - | - | 15.20 | 60.4 / 100.6 | **0.01** |
Reading: the remaining black at the *start view* of 3440x1440 / 5120x1440 (2-15 %) is the shrouded border and void beyond the playable edge: the start base sits ~540 units from a map edge and the 32:9 view is wider than that margin; with the camera over open ground the black is 0.01-0.08 %. That is geometry, not a fault (it needs the camera constraint to change, which hid the base and was rejected, or terrain beyond the border, which is out of scope). Zero Hour before-numbers were not separately recorded for sizes 2560-5120 (the same code path; the Zero Hour Skirmish slots were measured before and after: 5/5 always).
Skirmish slots, reachable list rows of 5 (slots 2..8): before Generals 5 5 5 5 5 **4 3** at 1920x1080, 2560x1440 and 5120x1440; after Generals 5 5 5 5 5 5 5 at all three; Zero Hour 5 5 5 5 5 5 5 before and after.

## 8. Generals versus Zero Hour impact
| Defect | Generals | Zero Hour |
| --- | --- | --- |
| Terrain draw window (black bands) | affected (Core code) | affected (same) |
| Fog overlay dropped (index limit) | affected | affected |
| Skirmish slots 7/8 | **affected** (Generals window layout) | not affected |
Classic mode: the draw window is only enlarged when Modern; the shroud band fix is behaviour-preserving for every case that fitted before; the mouse fix acts only while a combo box is open.

## 9. Validation (automated; details in the final report)
Win64 and Win32 builds; ctest 16/16 (Win64) and 23/23 (Win32); gtests Generals 421 / Zero Hour 420 on Win64 (the new test included) and 382 / 382 on Win32; Python native-runtime suites 231 tests OK; PowerShell workflow tests (NormalLaunch 22, OrdinaryPlay 45, OrdinaryGame 31, OrdinaryHost 31, OrdinaryMenu 32, OrdinaryTerrain 59) PASS. Stage4B / Generals fingerprint not re-run: no simulation, pathfinding, AI, RNG or replay code was touched (the changes are the terrain draw window size, the shroud batch size, a mouse lookup order and capture aids).

## 10. Candidates, ZIPs and the human retest
See section 11. Packaging status of the two previews: `X64_47A_...md` / `X64_47B_...md` carry the second-PC human PASS; public release readiness stays blocked by the three defects (fixed here, pending the human retest) and by release hygiene (build path in the executable: `/PDBALTPATH:%_PDB%`; unsigned executable).

## 11. New candidates, rebuilt ZIPs, retest checklist
Both ZIPs were rebuilt (the executables changed) from source commit `1ae0b0ca18d6e2761edc39d61123fed32dd51eb9` (later commits are documentation only) with the deterministic builder and re-validated on a disposable overlay layout (`Test-ZeroHourOverlayPackage.ps1`: **16 of 16 checks passed for each title**: only manifest files, hashes, no EA/proprietary types, AMD64, imports resolve from the extracted folder with a Windows-only `PATH`, the extracted `Generals.exe` started without arguments logged a normal launch, reached the main menu, drove Skirmish / General's Challenge to a match, exit code 0). This is automated validation, **not** the human retest.
| | Zero Hour | Generals |
| --- | --- | --- |
| ZIP | `build\dist\MKRaudstein-ZeroHour-x64-Preview-1ae0b0ca1.zip` | `build\dist\MKRaudstein-Generals-x64-Preview-1ae0b0ca1.zip` |
| Size | 4,115,518 bytes | 3,988,468 bytes |
| SHA-256 | `E33DD85E8BF4C6A647316FCBCB4A4E7ABFDADF768C4C00302364D269C7AF9503` | `094D536E5076F2738E2B108F45E53EB7119647110528D6378F8686A04D8DD883` |
| `Generals.exe` inside | 8,634,368 B, `24EC4F9D257C65F2A9FA0E55F3149D8104B5903ACDB848D9FBD41770D40E6CE6` | 8,280,576 B, `490B3AE22222148EC6C936AB2BAC53B16643D81DCB28C7CFF12A408D66C05B4E` |
| Contents / replaced file | unchanged from X64.47A: `Generals.exe` (replaces the EA launcher) + four app-local VC++ DLLs + README + `MKRaudstein\` licence and manifest | unchanged from X64.47B |
| Native candidate (developer PC) | `build\dev-runtimes\native\ZeroHour\x64-48a-zh-v1\game\generalszh.exe` (same executable as the ZIP's `Generals.exe`), preflight PASS; launcher default: `Run-ZeroHourCandidate.cmd` or `-Name x64-48a-zh-v1` | `build\dev-runtimes\native\Generals\x64-48a-gen-v1\game\generalsv.exe` (same executable as the ZIP's), preflight PASS |
The previous previews (`...-951547ec0` and `...-afe92caa2`) are superseded; replace them on the second PC.

### Human retest checklist (second PC, 5120x1440; do not mark anything validated until you report)
Install the new ZIPs the same way as before (extract over the Steam folder, replace `Generals.exe`, Steam Play), Zero Hour and Generals in turn.
1. **Fog of war.** Start a Skirmish with at least one AI. At the start the map should be dim ("fogged") except the area around your units, which is bright; scroll away and back: areas your units left must dim again; enemy units and buildings must not be visible in the dim areas. Report if the whole screen is uniformly bright or uniformly dim.
2. **5120x1440 world.** In Borderless (and once in Windowed at the largest size), with the camera over open ground the 3D world must fill the whole width outside the control bar with no black bands on the sides (rotate the camera with the mouse wheel / keys and zoom in and out). Black only beyond a map edge is expected (the map ends); report black in the middle of a map, and whether the picture looks stretched or cropped.
3. **Eight Skirmish slots.** In Skirmish setup fill all eight positions with AI. Open the difficulty list of every slot (especially 7 and 8) and pick each entry in turn (Easy, Medium, Hard, Brutal); hover over and click all rows. Every slot must behave the same (Generals was the affected title).
4. Report anything else that changed (performance feel, the HUD, menus).

## 12. Closeout status
Open until the human confirms section 11. After that this document closes and the order stays: Zero Hour native endurance/performance validation (X64.48) -> evidence-driven optimization only if justified -> later Enhanced HUD / polish (X64.49) -> release engineering (X64.51, including the build-path stripping `/PDBALTPATH:%_PDB%` and the signing decision).
