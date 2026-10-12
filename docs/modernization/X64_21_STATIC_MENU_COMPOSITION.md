# X64.21: static original Generals main-menu composition

## X64.21 failure localized; fix1 awaits reviewed runtime confirmation

Reviewed Generals `x64-21-final/runs/20261012T1822476915817Z` exited1
without timeout or launcher error. All288 contiguous checkpoints belong to PID31736.
Hardware Intel8086:7D45/feature11.0 device and retained display attachment completed;
`ui_menu_load` began, then `Original MainMenu.wnd parse failed` rejected setup.
No frame was drawn/presented. UI/display/backend/HWND/engine/MiniDumper cleanup,
late observer and final WinMain fail/exit completed; shutdown evidence is empty
and no dump exists. This is an orderly failure, not a crash or demonstrated
D3D11 rendering regression. X64.21 remains UNACCEPTED.

Concrete cause: bare `MainMenu.wnd` becomes `Window\MainMenu.wnd` in the
unchanged parser. Direct inspection of every BIG index in the reviewed stage
finds the original208336-byte file only at `Window\Menus\MainMenu.wnd`, in
Window.big. Original Shell requests `Menus/MainMenu.wnd`; the separate widget-ID
prefix remains `MainMenu.wnd:`. The diagnostic now passes the full archive key
and logs it. No parser/filesystem/renderer/widget/font/Win32 behavior changed.

New AMD64 regression exercises the actual inherited parser and actual diagnostic
Load entry point through a local-file spy, reproducing the old wrong request and
verifying the corrected key. The spy deliberately returns null: this isolates
path resolution, not full parsing/drawing. Both titles pass. No acceptance test
is weakened; original assets and legacy preservation contracts remain intact.

Offline validation PASS: full Win32 Release/CTest25, direct155/focused81 per title
(six pre-existing disabled tests), sentence5/render2d15; both AMD64 title links,
independent CTest15, texture/UI16 per title; Python rendering30/native17/performance16;
PowerShell graphics81/native6 guards/all10 script parses. Both AMD64 executables
retain30 imports,no delay or D3D8/D3DX imports;31 graphics dependencies pass each.
All51 recorded evidence hashes and32 prior staged EXE/PDB hashes remain unchanged.
The failed stage's actual EXE/PDB hashes and GUID/age61 independently match receipts.
No title was executed. Generated evidence/stages/analysis remain ignored.

Fresh Generals `x64-21-fix1` is prepared after validation. Exactly one next task:
separately reviewed Generals-only fix1 static original-main-dropdown confirmation.
Full63-window parsing, composition, presentation and lifetime acceptance remain
unverified until then. No interactive shell,world,input/update/execute/replay/
simulation/GameLogic or Zero Hour UI acceptance is implied. See the
[X64.21 report](X64_21_STATIC_MENU_COMPOSITION.md) for identity/commands/stop rules.

### Replacement candidate and developer boundary

Fresh-stage and separate full preflight PASS; no runs directory or app-local DLL.
Matching AMD64 Generals stage identity (no rebuild/restage after this copy):

- GUID `c7323867-d82e-41f3-b190-20a5da4eae06`, age62.
- EXE SHA256 `CA683A77F208FF528131DFE0CBCD57249206AB074DB056CFAA8F461790B15739`.
- PDB SHA256 `F1BEC3018A4A2323C98EDCC0E97C228729B05B965130B5B15DB654F9C545C0D9`.

Do not reuse the failed `x64-21-final` stage/run. After separate developer review:

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-21-fix1 -UIMenu
./scripts/native-runtime/Invoke-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-21-fix1 -UIMenu -RunReviewedGraphicsSmoke
```

Run the second command only if preflight passes. Expect about five seconds of
fixed800x600 blue diagnostic background with the original ruler/logo and main
panel, six textured buttons and readable original localized labels (Single Player,
Multiplayer,Replay,Options,Credits,Exit). Do not click/press anything. Automatic
exit0 and the full machine/visual contract remain mandatory. No world or
interactive shell is expected; no next run starts automatically.
Evidence: `build/dev-runtimes/native/Generals/x64-21-fix1/runs/<timestamp>`.
If preflight/runtime fails, the window closes early, any element is absent/corrupt,
text is unreadable or cleanup/visibility fails, stop and preserve the exact run.
Return run path, console result and visual observations. Do not retry, rebuild,
restage or launch Zero Hour. The original commands/preparation below are historical
and superseded by fix1; the acceptance contract is unchanged.

## Accepted baseline and scope

Generals X64.20-fix1 is formally accepted; see the final acceptance section of
[X64.20](X64_20_UI_PANEL_GATE.md). The failed X64.20 stage/run and all prior evidence
remain immutable. This larger offline batch prepares one substantial visual gate;
no Generals or Zero Hour process was executed by the agent.

## Reachable route and original contract

Headless GameClient initialization already creates the real W3DFontLibrary,
W3DDisplayStringManager,HeaderTemplateManager,mapped images,GameText and W3DDisplay.
Only its WindowManager is a dummy. W3DDisplay/WW3D remain the shared game-facing
owners; DX8 remains the Win32/reference backend. After verified headless engine
init,before execute/replay/simulation,the retained presentation driver attaches the
existing hardware D3D11 lifecycle and creates its800x600 Render2D helper. The new
AMD64-only `-headless -nativeUIMenuSmoke <isolated-run-root>` diagnostic loads the
original `Window/MainMenu.wnd` using the existing parser and actual widget factories.

A private NativeStaticMenuManager temporarily replaces TheWindowManager and isolates
TheTransitionHandler without initializing a second transition owner. Its virtual
system-message boundary permits only original push-button/static-text
CREATE/DESTROY/SET_LABEL/GET_LABEL functions/messages. Other application callbacks,
including MainMenuSystem CREATE and parent SCRIPT_CREATE input,are explicitly
excluded and counted. No WindowLayout is constructed: the parser resolves layout
callback identities but no layout init/update/shutdown is invoked. No global parser
mode or ordinary manager/producer behavior is changed.

The original parser creates63 windows. All are hidden before selecting the following
11-window static main-dropdown subset; other submenus,hidden faction widgets and
time/scheme callbacks remain undrawn. This is a deliberate diagnostic selection,
not a claim to reproduce MainMenuInit's animation/focus/first-entry state machine.
The real manager traverses/draws its tree in the original ordering. Each selected
callback is checked against the original callback,then wrapped only for bounded
first-draw begin/completion diagnostics. Actual default USER and three-piece
push-button draw functions and DisplayString producers perform rendering.

| Original WND element | Original800x600 rectangle | Content/route |
|---|---|---|
| MainMenuParent |0,0,800,600|original W3DNoDraw; no synthetic backdrop|
| MainMenuRuler |0,0,800,600|original ruler mapped artwork|
| MapBorder2 |532,108,756,360|original default USER border/fill|
| EarthMap2 |532,108,756,352|original inner USER fill|
| ButtonSinglePlayer |540,116,748,152|GUI:SinglePlayer|
| ButtonMultiplayer |540,156,748,192|GUI:Multiplayer|
| ButtonLoadReplay |540,196,748,231|GUI:ReplayMenu|
| ButtonOptions |540,236,748,272|GUI:Options|
| ButtonCredits |540,276,748,312|GUI:Credits|
| ButtonExit |540,316,748,352|GUI:Exit|
| Logo |504,16,791,110|original GeneralsLogo UVs|

Button labels use the original MainButton header template: Generals15 nonbold,
original orange/red enabled text with black drop color; no forced yellow difficulty
label appears in this normal main-dropdown subset. Existing FontChars.cpp explicitly
maps Generals to Arial while requesting a narrow lfWidth(-font_height*0.40),fixed96DPI,
PixelOverlap and original GDI rasterization. No custom font is missing or substituted.
The Steam archives contain no TTF/OTF/FON; English Language.ini has LocalFontFile
commented out,and HeaderTemplate.ini confirms Generals15. Focused tests exercise
the actual legacy narrow-font rule versus ordinary Arial15.

The five selected image identities are MainMenuRuler,GeneralsLogo,Buttons-Left,
Buttons-Middle,Buttons-Right. Original ImageCollection loads TextureSize512 first,
then HandCreated: the latter selects MainMenuRuleruserinterface.tga1024x1024;
the stale earlier MainMenuRuler.tga is absent and is not used or replaced.
The logo/button atlas is SCSmShellUserInterface512_001.tga512x512. Both installed
assets are ordinary uncompressed32-bit TGA,available through the existing filesystem.
Private Image clones retain original UV/image/atlas sizes and point to two privately
owned single-mip TextureClass contents/residencies. The global mapped collection and
asset texture cache remain untouched. Root W3DNoDraw's unused backdrop is not loaded.

The gate verifies63 parsed windows,all11 callback/geometry/label contracts,nonzero
six localized strings,the original font/template and text fit,normal state,all three
button images,nonempty image sizes and matching decoded atlas dimensions. Existing
normal-alpha textured and diffuse Render2D states suffice; no additional shader,
D3D8 emulation or renderer fallback is required. GPU glyph atlases are prepared by
actual W3DDisplayString/Render2DSentence on first draw. Per-image/atlas and per-widget
first-draw checkpoints localize resource/font/render failures.

## Lifetime, diagnostics and preservation

No mouse/UI-input dispatch,application HWND takeover,transition draw/update,Shell,
world,network,GameLogic,RNG or engine frame update is introduced. Dedicated checked
HWND/message handling,hardware device,viewport/clear/Begin/End/Present remain the
accepted retained-display host,with5000ms/300-frame limits and strict visible
non-minimized unoccluded S_OK Present/actual exit0 criteria. The new mode is distinct
from init/image/sentence/panel gates; conflicting selectors and ZeroHour UIMenu
staging are rejected (original Generals reference is first,shared C++ still links
both titles). No existing accepted mode is changed or weakened.

Cleanup destroys all private windows and their actual DisplayStrings/glyph GPU
resources,then private images/atlas owners and the display helper,before backend
and HWND teardown. Original engine manager/transition pointers,screen coordinates
and UV bias are restored. Normal engine/MiniDumper/late observer/WinMain cleanup
remains mandatory; zero shutdown bytes,no trailer/no dump and actual process exit0
are still required. Engine-owned FontChars CPU cache entries remain engine-owned;
private window sentence resources do not survive device detachment.

The classifier additionally requires all11 distinct widget draw completions,five
image identities,two atlas completions,63/11/6 load coverage and ordered load/draw/
release through retained Begin/End/Present and normal cleanup. It rejects missing,
duplicate,wrong-mode,wrong-coverage and reordered evidence. The old scalar/null Count
regression and previous gate contracts remain covered.

## Offline validation and remaining boundary

Final exact-source validation PASS:

- Full Win32 Release build,CTest25/25,both full title test executables155/155
  each (six pre-existing disabled tests),focused81/81 per title; actual sentence
  producer5/5 and Render2D submission15/15.
- AMD64 independent CTest15/15,including compatibility/reference,FP/startup,
  object-pool,diagnostics,Render2D/sentence and isolated D3D11 UI/WARP suites;
  both Generals and Zero Hour native title links; texture/UI15/15 per title.
- New focused coverage: strict new-mode grammar and flushed isolation/rejection
  on both architectures; actual Generals narrow font; real manager application
  callback exclusion,allowed gadget CREATE and unexpected-message rejection;
  original singleton ownership restoration; actual three-piece widget geometry,
  tiling and tree destruction. Older producer/state regression tests remain green.
- Python renderer30/30,native17/17,performance16/16; new byte-preservation
  contracts for original parser,push buttons,DisplayString,font producer;
  graphics81 synthetic evidence/dependency/guard checks,native6 guards/all10
  PowerShell script parses. No synthetic workflow invokes a title process.
- Actual staged and newly linked PE/PDB/import/dependency inspection; both AMD64
  titles have30 imports,no delay imports,no D3D8/D3DX imports;31 validated startup/
  graphics dependency routes including system DXGI. No app-local replacement DLL.
- All47 reviewed evidence files and32 prior staged EXE/PDB files rehash unchanged.
  git diff --check passed. Generated build/test/analysis/asset/runtime artifacts
  remain ignored and excluded from the explicit coherent commit.

The new graphical route has not yet executed in a game process. The original parser,
font and widget sources are byte-preserved by reference tests. Offline tests prove
the isolation/ownership boundary and actual button geometry; they do not claim full
original WND runtime composition or visual PASS. That remains the single reviewed
native gate. No complete interactive shell,hover/pressed widget state,new input,
shell-map/world,save/replay/gameplay/multiplayer/performance/determinism or Zero Hour
UI parity is accepted. Future work must address reachable interactive menu side
effects/application input and further state coverage after this composition passes.

## Fresh stage and exact developer procedure

Fresh isolated Generals `x64-21-final` was prepared only after final validation.
Stage and separate full preflight PASS: actual AMD64 EXE/PDB GUID-age/hashes,
original asset inventory,system hardware graphics/compiler dependencies. No runs
folder or app-local DLL exists. Do not rebuild/restage this immutable candidate.

- GUID `c7323867-d82e-41f3-b190-20a5da4eae06`,age61.
- EXE SHA256 `DD49326AA89E80EFE129648B26B4256C693B0B35EF2EE5D2F3DBE34E15AFD7E0`.
- PDB SHA256 `22BBF9FA6914F56F08ED1108FB7E97D0723569BE10DEC7F5DD2B834C0E04AC7A`.

Both titles link with the shared architecture; only Generals is staged for this
original-Generals UI reference gate. The bounded diagnostic code/tests/scripts/docs
are the intended commit; no runtime/visual acceptance is claimed for X64.21 yet.

Exactly one next task is the separately reviewed Generals-only static original
main-dropdown confirmation. From PowerShell7:

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-21-final -UIMenu
./scripts/native-runtime/Invoke-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-21-final -UIMenu -RunReviewedGraphicsSmoke
```

Run the second command only if preflight passes. Expect a fixed800x600 diagnostic
client with blue background,original logo near the upper-right,original ruler,
dark/gold main-dropdown panel and six original textured,labeled buttons:
Single Player,Multiplayer,Replay,Options,Credits,Exit (actual installed English
localization determines spelling). Labels use original enabled orange/red text,
not the earlier yellow difficulty probe. No world or fully interactive menu is
expected. Do not click/press anything; the window exits automatically after about
five seconds. No next run starts. Record visibility,readability,layout,alpha and
any artifact; do not accept solely from the console message.

If preflight/runtime fails,window closes early,any element is missing/corrupt,
text is unreadable,cleanup fails or visibility differs,stop and preserve the fresh
`build/dev-runtimes/native/Generals/x64-21-final/runs/<timestamp>` receipts,complete
startup.log,shutdown-exception.bin and userdata/CrashDumps. Do not retry,rebuild,
restage,mutate evidence or launch Zero Hour. Return the exact run path,console
result and visual observations for independent evidence review.
