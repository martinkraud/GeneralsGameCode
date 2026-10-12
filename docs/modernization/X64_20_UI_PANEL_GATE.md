# X64.20 - original menu panel primitive composition gate

## Accepted baseline and exact remaining state blocker

Generals X64.19 reviewed run20261012T1653491849657Z is accepted from machine
and developer visual evidence. See X64_19_UI_SENTENCE_GATE.md for exact identity,
315 checkpoints,239 visible hardware frames/5000ms,exit0 and clean shutdown.
Original logo/sentence/resource preparation is retained as the baseline.

The next reachable original-menu blocker is untextured normal-alpha Render2D.
Original staged Generals Window.big/MainMenu.wnd contains USER MapBorder2:
rect(532,108)-(756,360),creation800x600,ENABLED,STYLE USER,no image,no custom draw,
fill RGBA(0,0,0,126),border RGBA(255,187,0,255). Its normal default USER draw in
W3DGameWindow.cpp calls winOpenRect first,then winFillRect with one-pixel inset.
GameWindowGlobal.cpp forwards these real base manager methods to
TheDisplay->drawOpenRect/drawFillRect. W3DDisplay.cpp uses setup2DRenderState
with null texture,DRAW_IMAGE_ALPHA,grayscale=false and actual Render2D
Add_Outline/Add_Rect. These original producers and their geometry are unchanged.
D3D11 previously whitelisted only the exact textured sentence/image shader.
Untextured diffuse-alpha therefore required explicit support before these
original menu primitives could be composed; it was not an asset-loading problem.

## Implementation and ownership boundaries

D3D11 accepts one additional exact shader bit pattern: existing normal sentence
state with texturing disabled. A dedicated PSColor returns interpolated vertex
color; existing SRC_ALPHA/INV_SRC_ALPHA blend,depth/raster/viewport and pixel
mapping remain. No white-asset emulation or generic DX8 layer is introduced.
Diffuse submissions carrying a texture/residency reject rather than ignore it.
Ordinary Submit_Render2D clears prior residency before diffuse draw; the following
textured draw restores the existing pixel shader. Other unimplemented states,
including additive,grayscale and alpha-test,continue to fail closed.

Strict AMD64-only opt-in grammar:

    -headless -nativeUIPanelSmoke <fresh isolated run directory>

PowerShell -UIPanel implies the existing UISentence/UIImage/display contracts.
Old modes remain distinct and unchanged; Win32 rejects the new diagnostic flag.
After normal headless initialization,retained W3DDisplay/WW3D attaches the
existing hardware D3D11 lifecycle to the checked800x600 diagnostic HWND.
Each frame clears blue,draws original panel primitives,both original image and
sentence,then End/Present. First ui_panel_draw begin/complete records identify
exact rectangle/colors and GameWindowManager -> W3DDisplay -> Render2D -> D3D11.
The shared classifier requires this ordered route and exact panel identity.
The existing5-second/300-frame bounds and visible/unoccluded S_OK Present,
actual exit0,empty shutdown evidence,no dump and cleanup requirements remain.

The existing headless TheWindowManager inherits the real primitive
methods; explicitly qualified base calls execute actual W3DDisplay rendering and are not dummy widget
callbacks. Null manager fails closed. No extra manager or global alias is made:
GameWindowManager::~GameWindowManager owns freeStaticStrings and global
TheTransitionHandler deletion,so a private second manager would violate ownership.
No widget tree,winCreateFromScript,layout callback,manager init/update or resource
cache is introduced. Panel geometry uses the existing scoped Render2D helper;
sentence/image/GPU resources release before display/device and HWND teardown.
This is a panel primitive composition probe,not full original widget/layout or
main-menu acceptance. Its isolated difficulty content does not claim original
menu visibility/interaction state. No shell side effects are activated.

## Why not enter the full layout yet

GameWindowManagerDummy::winCreateFromScript ignores the requested filename and
creates a100x100 dummy window. It cannot establish original menu compatibility.
The real parser creates windows and invokes create callbacks; header templates
can override raw WND fonts. Full MainMenuInit has mouse/focus/network/shell/RNG
side effects. W3DMainMenuDraw contains time-based line animation and further
mapped assets. Shell::showShell queues a shell map/RNG work; ordinary display and
engine updates reach world/DX8/simulation. These are not safe shortcuts.

Original Generals custom-font registration,DisplayString/font-library/cache and
GPU lifetime ownership,real layout/window callback isolation,mapped widget
assets and additional demonstrated shader states,application HWND/input/focus
remain unresolved integration work. No raw TTF was found in staged Generals
Data; the original language/asset font route needs actual evidence before
assuming a custom font is available. Do not substitute Arial for original
custom button fonts. Shared owner architecture and Win32 DX8 reference remain.

## Offline validation and limitations

Focused tests extend the existing harness:
- isolated real D3D11 WARP readback checks original black126-alpha fill over blue,
  opaque gold border color,untouched outside pixels,texture/diffuse shader
  switching and clearing stale texture bindings;
- rejects diffuse with residency,additive and alpha-test without drawing;
- title-linked actual Render2D producer verifies exact diffuse-alpha shader bits,
  original inset geometry and existing graphical pixel-center policy;
- dual-architecture strict grammar,independent modes,Win32 rejection,flushed
  isolated AMD64 configuration;
- shared classifier synthetic missing/wrong/reordered panel identity/state;
- source-preservation freezes original manager/default draw/primitive forwarding
  and rejects second-manager ownership or script/layout shortcuts.

The initial readback assertion omitted Clear's explicit alpha1 and correctly
observed destination alpha0. Test input was corrected; no production blend or
runtime acceptance was weakened. Isolated GPU tests do not launch a title.
Final results and isolated stage identity are recorded below after validation.
No native/manual title execution is performed in this implementation stage.

## Separately reviewed Generals-only procedure

Run these from PowerShell7 only after developer review:

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-20-final-2 -UIPanel
./scripts/native-runtime/Invoke-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-20-final-2 -UIPanel -RunReviewedGraphicsSmoke
```

Preflight starts nothing. Invoke starts exactly one Generals process. Expect
fixed800x600 blue client area,accepted original logo near upper right,and readable
yellow difficulty label,now on a dark translucent panel with a thin gold border
at(532,108)-(756,360). Original inset fill is(533,109)-(755,359). Logo rectangle
and sentence placement/size remain unchanged. No buttons or interactive menu
are claimed. The process should close normally after about5seconds with exit0.
No input is needed; do not resize,minimize,close or switch away during the probe.

Evidence stays at build/dev-runtimes/native/Generals/x64-20-final-2/runs/<UTC>/:
launch.json,exit.json,startup.log,shutdown-exception.bin,userdata/CrashDumps.
Return the exact run directory and visual description of panel/logo/text.
Stop before Invoke if preflight fails. Any launch/classifier error,missing or
wrong panel/logo/text,corruption,crash or timeout: preserve everything and stop;
no automatic rerun,Zero Hour execution,rebuild/restage or workaround.

Exactly one next developer action: separately reviewed Generals-only X64.20
panel-composition execution followed by machine evidence and visual review.
No title execution in implementation. Do not claim the original full main menu,
normal application input,world/gameplay,save/load,replay,multiplayer,performance
or determinism. Earlier next-task recommendations are historical/superseded.

## Final validation and isolated stages

Full Win32 Release build PASS; CTest25/25; direct153 tests and focused79 tests
per title PASS(six pre-existing disabled direct tests). Sentence producer5 and
Render2D submission15 PASS. AMD64 CTest15/15 independent tests PASS; both native
Generals and Zero Hour Release links PASS;11 production-memory texture/UI tests
per title PASS. Native startup gate15 AMD64/14 Win32 and isolated D3D11 sentence10
per architecture PASS. Renderer reference30,native/display16,performance16 Python
checks PASS; shared graphics workflow67 and native workflow6 guards/all10 script
parses PASS. git diff --check PASS. No title process was launched.

After final source/build/test validation,both fresh x64-20-final-2 panel stages
were copied and preflighted. Actual matching EXE/PDB hashes/GUID-age,asset
inventory,AMD64 system D3D11/DXGI/compiler and all required dependencies PASS.
Both import sets remain30 with no delay imports and no D3D8/D3DX imports;
no app-local DLL replacements. UISentence-only mode rejects each panel stage.
Neither new stage has a runs directory; neither is a runtime/visual PASS yet.

- Generals: GUID `c7323867-d82e-41f3-b190-20a5da4eae06`,age55; EXE SHA256 `43C317414CA8EA532F6FCF635A9806E8B8A02E78E02563E4BF8F0C6634AFF489`; PDB SHA256 `9194091760107123F8269EDC4665FA0513B72D9A1C64FD60D8E8C975CFE396CB`.

- ZeroHour: GUID `7a285453-4110-4cad-a6c8-e5336ba19b1c`,age56; EXE SHA256 `C8575CDF4224DB683CD5AC9C669CF666A361942BD338AD92F3B5B69A163E7477`; PDB SHA256 `00F5778D09C407F323C95FDB294F29489EDBD9EE420C7C90DA2593AA75C5DE08`.

All39 reviewed evidence files and20 previous staged EXE/PDB copies were rehashed
unchanged,including accepted X64.19 and failed X64.18. Old stages and installed
Steam content remain untouched. Generated stages,evidence,assets,audit helpers,
analysis JSON and build/test logs remain ignored and excluded from commits.
Only the intended source/test/scripts/modernization documents are staged for a
coherent X64.20 commit on dev/modern-engine; push only origin/dev/modern-engine.
The exact commit/push result is reported to the developer,not inferred here.

The superseded,unexecuted x64-20-final stages are also preserved. Final source
explicitly qualifies GameWindowManager base primitive calls,ensuring real
forwarding even if a derived manager later overrides those virtual methods.
The final-2 stages contain this exact source; do not execute the earlier stage.
