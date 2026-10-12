# X64.18 - retained-display original menu image gate

Current state: first reviewed X64.18 runtime FAILED before the first frame.
Missing Texture2DCPUContent pool registration is reproduced and repaired offline.
Use only the new x64-18-fix1 candidate after review; do not rerun/reuse the failed
x64-18-final stage. Full failure/repair evidence is recorded below.

## Scope and route

Generals X64.17 is formally accepted from reviewed run
`build/dev-runtimes/native/Generals/x64-17-final/runs/20261012T1243165104474Z`.
Actual exit0,293 sequential checkpoints, Intel Graphics8086:7D45/feature11.0,
239 visible frames in5000ms, correct owner/cleanup order, empty observer and no
crash dump match the developer's normal blue-window observation. See the X64.17
report for exact build identity and checkpoint evidence. Zero Hour X64.17 remains
unexecuted; its X64.16 hardware gate and both titles' init-only gates remain accepted.

The next smallest useful integration is one original-menu image through the
actual retained display. This adds strict AMD64-only `-headless
-nativeUIImageSmoke <isolated-root>`; existing init-only, direct graphics and
clear-only retained-display modes remain separate. It keeps verified headless
init, checked hardware HWND/device/frame host and normal cleanup. Both title
WinMain logs explicitly identify UI image smoke; both GameMain paths enter the
existing retained-display hook before execute/replay. No engine update, ordinary
Display::draw, GameLogic, Shell::showShell, input dispatch or simulation is entered.

`W3DDisplay::runNativePresentationSmoke` borrows the diagnostic HWND; WW3D owns
the backend via PresentationLifecycle. After attachment it reads the existing
initialized ImageCollection's GeneralsLogo metadata and prepares one private
single-mip ordinary TextureClass through the existing D3D11 file-content bridge.
The actual GameFileClass resolves texture names through Art/Textures and engine
BIG/file-system priorities, including Zero Hour's existing base-asset route.
No decoder, compressor, texture conversion policy or legacy producer is changed.

Source and original staged assets establish:

- Generals MainMenu.wnd:Logo uses GeneralsLogo at504,16 through791,110 in800x600.
- Both titles' SCSmShellUserInterface512.INI map GeneralsLogo to
  SCSmShellUserInterface512_001.tga,512x512,coords1,1 through371,121,statusNONE.
- Generals' staged atlas is raw32-bit512x512 TGA (1048620 bytes), supported by the
  existing bounded UI decoder. Zero Hour has the same mapped definition and
  retains its normal base-asset resolution. Its asset execution is still untested.

The scoped diagnostic Image uses the existing RAW_TEXTURE contract with those
unaltered UVs and an owned TextureClass, avoiding the global asset cache's
longer lifetime. A temporary actual Render2D helper uses800x600 coordinate/screen
state. Each frame clears blue through WW3D Begin_Render, calls the unchanged
W3DDisplay::drawImage with alpha/white modulation at the original rectangle,
then WW3D End_Render/Present. That exercises the title's existing Render2D
producer, renderer-neutral submission, real TextureClass content/residency bridge
and D3D11 UI shader. It does not recreate or redesign the menu.

## Ownership, failure and evidence contract

The host always calls Driver::Shutdown, including partial setup/draw failure.
Release order: Render2D helper (drops its texture ref), private TextureClass
(drops CPU content/GPU residency), pooled Image; restore static screen state;
WW3D device detach; diagnostic HWND/class; normal engine/MiniDumper/late-observer
cleanup and final WinMain. Display dimensions/windowed/depth restore as in17.
No texture is inserted into WW3DAssetManager's cache, no GPU residency survives
attachment. Missing/unsupported/invalid file, unexpected mapped status or atlas
size mismatch fails explicitly. There is no fallback/no-op rendering or DX8
emulation. Unsupported shader states remain rejected.

Flushed ui_image_load begin/complete reports actual filename,atlas,UVs,rectangle;
ui_image_draw begin/complete bounds actual draw; ui_image_release precedes
completed display detach. Guarded scripts require a distinct UIImage stage and
classifier mode, all three ordered UI completions, hardware visible unoccluded
S_OK Present, bounded frames/time, normal lifecycle, actual OS exit0, isolated
paths, matching EXE/PDB, empty observer/no trailer/no dump. Existing capture and
shutdown evidence are untouched. Visual validation of the actual logo remains
mandatory; successful draw return/Present alone is insufficient.

## Offline validation

Final results and staged identities are recorded below after validation.
The added tests cover strict independent grammar, mixed/replay/Win32 rejection,
flushed isolated AMD64 UI-mode configuration, positive/negative ordered evidence,
mode separation and missing image identity. Actual WARP readback verifies a
synthetic atlas crop at the original menu rectangle blends over the blue clear
and leaves outside pixels unchanged; residency is released before backend.
Legacy source/reference tests continue to freeze original W3DDisplay.cpp,
Render2D producers, texture code and the Win32 owner path. These tests do not
replace a native execution of the new display image path.

## Remaining work toward original main menu

Normal UI requires additional shader states (solid/additive/grayscale), original
mapped asset coverage/failure policy, actual sentence/font drawing and resource
lifetimes; application HWND/input/window callbacks are still separate from the
checked diagnostic host. Layout/window-manager rendering must be isolated from
Shell::showShell's shell-map messages/RNG mutation and ordinary engine updates.
World rendering is absent. No menu input, ordinary graphical startup, gameplay,
save/replay/network, performance or determinism claim follows from this gate.
Retain W3DDisplay/WW3D and neutral contracts, DX8 Win32/reference and original UI.

## First reviewed developer procedure (manual boundary)

From PowerShell7, after developer review, use only this Generals stage:

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-18-fix1 -UIImage
./scripts/native-runtime/Invoke-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-18-fix1 -UIImage -RunReviewedGraphicsSmoke
```

Preflight launches nothing. Invoke starts exactly one isolated native process,
no next trial/title. Expect the same fixed800x600 blue client area, now with the
original Generals logo near the upper right at its original menu rectangle;
about5seconds then automatic close/exit0. This is an image probe, not the menu.
Do not click, resize, minimize, press Escape or close it; no intervention needed.
On any error or missing/incorrect logo, preserve evidence and stop: no automatic
rerun, rebuild/restage or Zero Hour launch. Send the exact new directory under
`build/dev-runtimes/native/Generals/x64-18-fix1/runs/<UTC>` and visual observations,
including launch.json,exit.json,startup.log,shutdown-exception.bin and CrashDumps.

Exactly one recommended next task: separately reviewed Generals-only X64.18
original-menu image confirmation, followed by evidence/visual review. Do not
perform that execution inside implementation/validation.

## Final offline results and stage identities

Win32 Release full build and CTest25/25 PASS (including accepted resource
characterization/reference comparisons); direct149 and focused75 per title PASS
(six existing disabled tests per direct suite). AMD64 independent CTest15/15
PASS and both native title links PASS. Focused native startup11/11 and actual
WARP UI10/10 PASS; Win32 includes its10 startup tests and the same GPU UI test.
Native source14,renderer28,performance16 reference tests PASS. Graphics workflow46,
native workflow6 and all10 PowerShell script parses PASS. git diff --check PASS.

Fresh per-title x64-18-final stages were created only after final validation.
Both pass EXE/PDB GUID-age/hash, assets, AMD64/system graphics dependency and
distinct UIImage mode preflight; old DisplayLifecycle mode rejects. Both import
sets remain30, no delay imports and no D3D8/D3DX dependencies. No app-local graphics
DLLs. Neither stage has a runs directory; no game process was launched.

- Generals: GUID `c7323867-d82e-41f3-b190-20a5da4eae06`, age50; EXE SHA256 `FFB8E92B4CE89111DB7DE6EB76926AE26F512ECE199723D311390ED35C2DBF9A`; PDB SHA256 `BC3552AA64A8122A634B9325B740636735D67FA59B0EB550AD81205B410CE615`.
- ZeroHour: GUID `7a285453-4110-4cad-a6c8-e5336ba19b1c`, age51; EXE SHA256 `20966F3A69413BE6B9547ABBD4EC6C02364D6CC25991CD820E35D4B59951FB93`; PDB SHA256 `8846266C2D0DCAA2D47CA226C39BD4FFCE9FF76ACBEA62EB4399652372B3F265`.

All27 inventoried reviewed evidence files (including four Generals X64.17
receipts/log/observer files), and both titles' X64.16-final-2/X64.17-final EXE/PDB
copies were rehashed unchanged after staging. Generated inventories/audit helpers,
assets,stages,build/test logs remain ignored and are excluded from commits.
Implementation/reference/guard changes and these docs are the coherent X64.18
change on dev/modern-engine, following X64.17 acceptance commit8c74fdbb1.
Only origin/dev/modern-engine is an authorized push destination.

## First reviewed failure and established repair (2026-10-12)

Preserved Generals run20261012T1357003637599Z under x64-18-final failed normally
with OS exit1,PID27192,no timeout/launcher error,matching isolated paths and
GUID/age50/EXE/PDB identity above. Complete log contains287 sequential records.
Initialization246,hardware device255 (same Intel8086:7D45/11.0),target259,
setup261,viewport263,display size264 and attach265 succeed. ui_image_load266
begins,then graphics_backend267 catches an unknown exception. No load completion,
frame Begin/End,clear,draw or Present occurs; summary277 records0 frames/0ms.
Cleanup is ordered:ui release270,display detach271,backend272,HWND274,class276,
engine283,MiniDumper285,late observer286,WinMain287 fail/exit1. Observer0 bytes,
no trailer/dump. This is a caught startup resource failure, not a late crash.
The X64.17 display lifecycle still reaches the same successful attachment.

Root defect: Texture2DCPUContent declares W3DMPO_CODE but neither title registers
that name in GameMemoryInitPools. UITextureContent's ordinary TGA decoder first
allocates this class before writing pixels. createW3DMemPool requests counts0/0;
userMemoryAdjustPoolSize cannot find the name; createMemoryPool consequently
throws engine ErrorCode ERROR_OUT_OF_MEMORY (0xDEAD0002). The host only caught
std::exception specifically; this enum was reduced to unknown exception.
An engine-linked test reproduces the actual raw32-bit TGA route and exact enum
3735879682 in BOTH title test executables before the fix. Isolated UI/sentence
tests define DISABLE_GAMEMEMORY and therefore missed this integration defect.
The reviewed log has no throw-site stack or resolved asset sub-checkpoint; root
attribution rests on the deterministic production-memory reproduction and source
contract, not a claimed captured stack. No unrelated allocation/ownership cause
is inferred. The atlas format/metadata are established from staged assets; the
failed log itself does not prove file resolution/CPU decoding completed.

Repair registers only Texture2DCPUContent with16 initial/16 overflow metadata
objects per title. All existing pool entries/capacities/layouts are unchanged.
Regression additionally covers40 simultaneous objects (overflow),snapshot bytes,
release and repeat allocation/reuse under actual engine memory. Load diagnostics
are split into resolved metadata,TextureClass creation,CPU-content preparation
and Render2D construction; engine enum errors retain their value. No fallback,
original-logo substitution,decoder/shader/ownership/acceptance relaxation or
engine update is added. New candidate is x64-18-fix1; do not reuse x64-18-final.
Final repair validation/stage identity follows below.

## Fix1 final validation and immutable replacement stage

Before repair, both engine-linked Win32 test executables fail the real raw-TGA
allocation with ErrorCode3735879682/0xDEAD0002. After repair: full Win32 Release
build/CTest25/25,150 direct tests per title (six existing disabled),76 focused
tests per title PASS. AMD64 independent CTest15/15,both native title links,
and BOTH actual engine-memory Texture2DTest suites8/8 PASS. This specifically
closes the DISABLE_GAMEMEMORY coverage gap. Existing WARP UI10/10,sentence,
startup/shutdown,resource/reference and pool regression targets remain green.
Renderer references30,startup/display14,performance16 PASS; graphics46/native6
workflow checks and all10 PowerShell parses PASS; git diff --check PASS.
Pool preservation tests verify every pre-existing entry/value is unchanged.

Both new x64-18-fix1 UIImage stages were created only after final builds/tests.
Preflight verifies matching AMD64 EXE/PDB identity/hash,assets,system graphics
dependencies;30 imports per title,no delay imports,no D3D8/D3DX. Old-mode
preflight rejects; neither stage has a runs directory. No game was launched.

- Generals: GUID `c7323867-d82e-41f3-b190-20a5da4eae06`, age51; EXE SHA256 `2C8C218F2F6C07C67462021EAFC81D4BB263318E7F0E23A354EF066AFEFD9BA4`; PDB SHA256 `7ED24CB7591EA447DA1AD83B7EABA18267662B40AD38947CD6A872B4E4494A3F`.
- ZeroHour: GUID `7a285453-4110-4cad-a6c8-e5336ba19b1c`, age52; EXE SHA256 `286AA732E09D140A1D3E937FAEDCC069CF7A10C622D9C159F98BD850155AF763`; PDB SHA256 `32EC9279DFB651D5EA0CEBF5395AB8EA4CA9CDFA28A9E19935980B55D73F0C40`.

All31 inventoried accepted/failed evidence files and both titles' original
X64.16-final-2/X64.17-final/X64.18-final EXE/PDB copies rehash unchanged.
The original failed run remains preserved,not edited/reused/reclassified as PASS.
Generated assets/stages/evidence/reproduction helpers/logs are ignored and
excluded from the repair commit. Runtime repair acceptance remains pending the
separately reviewed Generals-only x64-18-fix1 run; use the procedure above.
No source change beyond the two new pool entries and diagnostic load boundaries.
