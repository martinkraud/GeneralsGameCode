# X64.18 - retained-display original menu image gate

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
./scripts/native-runtime/Test-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-18-final -UIImage
./scripts/native-runtime/Invoke-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-18-final -UIImage -RunReviewedGraphicsSmoke
```

Preflight launches nothing. Invoke starts exactly one isolated native process,
no next trial/title. Expect the same fixed800x600 blue client area, now with the
original Generals logo near the upper right at its original menu rectangle;
about5seconds then automatic close/exit0. This is an image probe, not the menu.
Do not click, resize, minimize, press Escape or close it; no intervention needed.
On any error or missing/incorrect logo, preserve evidence and stop: no automatic
rerun, rebuild/restage or Zero Hour launch. Send the exact new directory under
`build/dev-runtimes/native/Generals/x64-18-final/runs/<UTC>` and visual observations,
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
