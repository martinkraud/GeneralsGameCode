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
The initial implementation rejected diffuse submissions carrying a logical
texture or residency. The reviewed failure below establishes that a retained
inactive logical texture is valid; fix1 corrects only that backend restriction.
Explicit neutral GPU residency remains rejected for diffuse submissions.
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

## First reviewed failure and bounded fix1 (2026-10-12)

Preserved exact failed Generals run:
`build/dev-runtimes/native/Generals/x64-20-final-2/runs/20261012T1731053035584Z`.
PID17716 actually exited1,Started true,no timeout or launcher error,PathsMatch
true. This is a real runtime gate failure,not a PowerShell classification bug.
The complete317-record log has coherent sequence numbers. Full stage preflight
before edits verified actual matching EXE/PDB hashes/GUID-age and original assets;
Generals GUID c7323867-d82e-41f3-b190-20a5da4eae06,age55,with final-2 hashes above.
The immutable failed run is not overwritten,reused or reclassified as PASS.

Initialization completed. Hardware Intel8086:7D45/11.0,800x600 target,retained
display attachment,original logo metadata/CPU content/helper,sentence/font/build
all completed. First panel draw(record289),image291,sentence293,End294 and
Present295 completed; actual HRESULT0,visible1,occluded0,removed0. Then record296
reports Unsupported D3D11.1 renderer state/operation. The summary is FAIL:
1 visible frame,0 occluded,47ms. Present/removed still report0: graphics_present
is the host's coarse drawing/presentation exception boundary,not evidence of a
failed DXGI Present. Engine/MiniDumper cleanup and late observer completed;
WinMain reports fail/exit1. shutdown-exception.bin exists and is empty; no dump
or late-exception trailer. Execute/update/replay/simulation/GameLogic stayed
skipped. The required five-second visible loop was not satisfied: X64.20 is
NOT accepted. No developer visual result is inferred from console output.

Concrete root cause: W3DDisplay::setup2DRenderState(nullptr,...) resets geometry
and disables texturing but deliberately does not Set_Texture(nullptr).
Render2DClass::Reset clears vertices/UVs/colors/indices only; Enable_Texturing
changes the shader bit only. After first-frame logo drawing,the reusable helper
therefore retains the actual logo TextureClass/refcount. The next panel submits
that non-null pointer with the exact valid untextured shader. X64.20's new
D3D11Backend::Submit_Render2D explicitly rejected this pair before drawing.
The first frame had no retained texture and succeeded. This explains the
second-frame divergence without assuming device loss,allocator corruption or
an asset failure. The logo's retained identity is needed for later image draws.

Before production edits,the new isolated WARP repeated-panel/image regression
reproduced the identical unsupported-state exception on frames2/3 while frame1
succeeded. Both title-linked actual Render2D tests independently confirmed
Reset/disable preserve the real TextureClass identity and exact reference count.
These tests close the gap in the original X64.20 tests,which cleared backend
GPU residency but left submission.texture null on every diffuse draw.

Small backend-only fix: for the exact supported diffuse-alpha shader,copy the
submission,normalize only the local inactive texture pointer to null,clear the
backend binding,and Draw the diffuse shader. Never dereference,prepare,upload or
sample that inactive logical pointer. The caller's pointer/refcount/producer
state remains intact and the next textured draw follows its original route.
Shader-disabled texturing is implemented explicitly; this is not a no-op for
unsupported drawing or missing content. Other states still reject. The explicit
neutral-content API's non-null GPU residency rejection remains; Draw's strict
normalized-state validation remains. No original producer,W3DDisplay,DX8,
resource ownership,layout or acceptance criteria is changed.

Regression coverage: three successive panel/image transitions,first-use versus
retained identity,actual diffuse pixel readback,textured glyph readback afterwards,
cleared stale GPU binding,unchanged caller pointer,six real draws and exactly one
residency creation. The actual title-linked producer verifies real texture
retention/refcounts across Reset,disable,re-enable and final explicit release.
The old unsupported-state and neutral-residency rejection tests remain green.

Fix1 validation and final isolated stage identities are recorded below. The
original x64-20-final/x64-20-final-2 candidates and all reviewed evidence remain
preserved. Do not rerun the failed stage. The command below supersedes the
original first-run procedure; neither title is launched during repair.

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-20-fix1 -UIPanel
./scripts/native-runtime/Invoke-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-20-fix1 -UIPanel -RunReviewedGraphicsSmoke
```

Expected blue800x600 window,original logo,readable yellow difficulty label and
dark translucent MapBorder2 with thin gold border for about5seconds,then automatic
normal exit0. No interaction. The evidence directory is
build/dev-runtimes/native/Generals/x64-20-fix1/runs/<UTC>/ with launch.json,
exit.json,startup.log,shutdown-exception.bin and userdata/CrashDumps.
Stop on preflight failure before Invoke. Any error or visual defect: preserve
all evidence and stop; no rerun,Zero Hour execution,rebuild/restage or workaround.
Exactly one next task: separately reviewed Generals-only X64.20-fix1 panel
confirmation,then machine and visual evidence review. Full original menu,
application input,world/gameplay,replay/save/multiplayer/performance/determinism
remain unvalidated and outside this repair.

## Fix1 final validation and replacement-stage identity

Full Win32 Release build/CTest25 of25 PASS;154 direct and80 focused tests per
title PASS(six existing disabled direct tests); sentence5/Render2D15 PASS.
AMD64 independent CTest15 of15,both native title links and12 production-memory
texture/UI tests per title PASS. The new WARP regression fails on the unchanged
backend before repair and passes in both final architecture suites after repair;
all11 isolated D3D11 sentence/state tests per architecture pass. Existing startup
15 AMD64/14 Win32 tests are retained by the full suite. Renderer30,native16,
performance16 Python checks,graphics67/native6 workflow guards and all10 script
parses PASS. git diff --check PASS. No title process was launched.

Fresh per-title x64-20-fix1 panel stages were created only after final validation.
Both full preflights PASS: actual matching EXE/PDB hashes/GUID-age,original asset
inventory,AMD64 system D3D11/DXGI/compiler/dependencies. Both have30 imports,
no delay/D3D8/D3DX imports,no app-local DLL replacements. UISentence-only mode
rejects each panel stage. Neither has a runs directory. No runtime/visual PASS
is claimed for fix1; the developer-reviewed Generals execution is still required.

- Generals: GUID `c7323867-d82e-41f3-b190-20a5da4eae06`,age56; EXE SHA256 `21A2A3DE7F9DE9BF3E7B222DC29902BF10C7EAE4CF5A5DDEB57BEAE139101522`; PDB SHA256 `5CAC5FE967670803741D98A96E19C32B730C9639EC4D4C0FCFDB96FBBEE568A3`.

- ZeroHour: GUID `7a285453-4110-4cad-a6c8-e5336ba19b1c`,age57; EXE SHA256 `56FE4757BF9ADF9EB49D807781A58C24D1C37013D2F5597BCCEE67B5DC5A146A`; PDB SHA256 `453E33C456E64F06E6F37F6C66955D321249ACD6D847C2776E795792D3074A94`.

All43 inventoried reviewed evidence files and28 previous stage EXE/PDB copies
remain hash-identical. Failed run/stage and all accepted evidence are preserved.
Generated logs,repro helpers,analysis/inventory JSON,assets and stages remain
ignored and excluded from the coherent backend/test/documentation commit.
Push is authorized only to origin/dev/modern-engine; exact commit/push state is
reported to the developer. Do not rebuild/restage the final candidate casually.

## Generals X64.20 formally accepted (2026-10-12)

Reviewed run `build/dev-runtimes/native/Generals/x64-20-fix1/runs/20261012T1743032823349Z`
was independently reviewed from launch/exit receipts, all317 flushed checkpoints,
shutdown evidence, dump directory and actual staged EXE/PDB identity. Actual exit0,
no timeout/launcher error, matching isolated paths, hardware Intel8086:7D45/11.0,
800x600 display/target,238 visible non-minimized unoccluded S_OK frames/5000ms.
Original logo/atlas/UV/rectangle and yellow sentence remained unchanged. Original
MapBorder2 primitive draw completed before image/text; UI release preceded display,
backend and HWND release. Engine/MiniDumper cleanup,armed late observer and final
WinMain completion passed. Shutdown-exception.bin is empty,no trailer/no dump.
Execute/update/replay/simulation/GameLogic remained outside the diagnostic boundary.
The developer visually confirmed original logo/readable yellow label,dark translucent
panel/thin gold border,correct alpha and clean automatic completion without flicker.
The combined machine/visual evidence formally ACCEPTS Generals X64.20.

Actual PE/PDB GUID `c7323867-d82e-41f3-b190-20a5da4eae06`,age56; EXE SHA256
`21A2A3DE7F9DE9BF3E7B222DC29902BF10C7EAE4CF5A5DDEB57BEAE139101522`;
PDB SHA256 `5CAC5FE967670803741D98A96E19C32B730C9639EC4D4C0FCFDB96FBBEE568A3`.
All47 inventoried reviewed evidence files and32 prior staged EXE/PDB files rehash
unchanged,including the failed original X64.20 run. This accepts the bounded panel
composition only,not full menu/input/world/gameplay or Zero Hour UI parity.
The next offline batch is [X64.21 static original menu composition](X64_21_STATIC_MENU_COMPOSITION.md).
