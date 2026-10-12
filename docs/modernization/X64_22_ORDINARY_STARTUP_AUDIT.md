# X64.22: ordinary Generals startup -> original interactive menu audit

Date: 2026-10-12. Baseline: `55bc829890f53118cc7205d4a8c32708c6ecb8b5` on
dev/modern-engine, clean worktree, synchronized with origin. Documentation-only:
no production code, build configuration, script or runtime changed. Neither title
was launched. Source line numbers are for this baseline; Generals only. Zero Hour
has separate WinMain/GameMain/GameEngine bodies (differences not audited here),
the device/display/GUI code below is shared.

Classes: **A** required for an original interactive main menu; **B** safely
deferrable for the first bounded real-startup/menu milestone (must fail or be
receipted explicitly, never silently stubbed); **C** enters world/gameplay/
simulation/replay/multiplayer or otherwise crosses the current safety boundary.

## Answer to the central question

The original MainMenu **can** be driven by a bounded menu-only update/event loop
without GameLogic/world simulation, but **not** by the stock `GameEngine::execute`,
`GameEngine::update` or `GameClient::update` as they are written, and not by
flipping `m_headless`. Reasons, all from source:

1. With the default `ShellMapOn=TRUE` the ordinary engine **does not push
   MainMenu.wnd from the client**. `GameClient::update` (GameClient.cpp:505..527)
   finishes `Intro`, then calls `showShellMap(TRUE)` (queues `MSG_NEW_GAME`,
   `GAME_SHELL`, `InitRandom(0)`: Shell.cpp:547..570) and `showShell()`, whose
   push is guarded by `!m_shellMapOn` (Shell.cpp:538..543). MainMenu is pushed
   later by `GameLogic::startNewGame` when the shell-map game finishes loading
   (GameLogic.cpp:1996..1999). So the stock path to the menu is the world path (C).
2. With `shellMapOn=FALSE` (+ `playIntro/playSizzle=FALSE`, i.e. `-quickstart`
   semantics), `showShell()` pushes MainMenu.wnd immediately and synchronously
   (`push` with an empty stack calls `shutdownComplete` -> `doPush`, Shell.cpp:
   333..397/700..718) and `MainMenuInit` runs with only UI effects. This is the
   only menu route that needs no GameLogic.
3. `GameEngine::update` (GameEngine.cpp:751..785) calls `TheGameLogic->UPDATE()`
   whenever `canUpdateGameLogic` is true. `GameLogic::update` (GameLogic.cpp:3174..)
   runs ScriptEngine/TerrainLogic/Recorder updates and `processCommandList`,
   which is where `MSG_NEW_GAME` becomes `startNewGame`. That call is the exact
   door to class C and must not be reachable from a menu-only loop.
4. Input is event-stream driven: Win32 WndProc -> `Win32Mouse::addWin32Event` ->
   `MouseXLat` -> `TheMessageStream` -> `WindowTranslator` -> `winProcessMouseEvent`.
   `propagateMessages` only runs inside `GameEngine::update` after
   `GameClient::UPDATE`. Unconsumed `MSG_FRAME_TICK`, `MSG_NEW_GAME`,
   `MSG_CLEAR_GAME_DATA` and network messages are *kept* by
   `GameClientMessageDispatcher` (GameClientDispatch.cpp:42..56) and appended to
   `TheCommandList`, which only `GameLogic::update` drains. A loop that skips the
   logic update therefore needs an explicit boundary: drain `MSG_FRAME_TICK`,
   and treat `MSG_NEW_GAME`/`MSG_CLEAR_GAME_DATA` as an explicit
   "menu boundary reached" stop. Without it the list grows every frame and a
   Single Player start silently wedges.
5. `W3DDisplay::draw` (W3DDisplay.cpp:1844..2150) is monolithic. It is not
   usable on the modern presentation as written (see A5).

## Established current truth (state of the accepted path)

- X64.21 passes with `m_headless == TRUE` for the entire engine lifetime. The
  non-headless branches that the ordinary game takes have **never executed on
  AMD64**: real ApplicationHWnd, DirectInput keyboard, W3DMouse, real
  W3DGameWindowManager, non-dummy radar/particles/audio/terrain visual, WW3D::Init.
- The accepted `-nativeUIMenuSmoke` attaches a diagnostic HWND *after* init
  through `WW3D::Init_Native_Presentation` (ww3d.cpp:274..302), draws with a
  private manager and parser, and explicitly suppresses callbacks, transition
  ownership and layout init. It validates producers and ordering, not ordinary
  display/manager/Shell lifecycle.
- A single flag, `m_headless`, selects ~30 independent behaviors (list below).
  There is no "UI without world" profile in the engine today.

## Blocker inventory

IDs are stable for this document. "Dep" lists blockers that must precede it.

### Process, window and message ownership

| ID | Class | Source / what it does now | Why it matters on AMD64/D3D11 | Dep | Smallest consistent approach | Offline proof |
| --- | --- | --- | --- | --- | --- | --- |
| A1 | A | WinMain.cpp:694..769 `initializeAppWindows`: `RegisterClass`/`CreateWindow` unchecked, returns `true` unconditionally; `windowed==false` (default `m_windowed=0`, GlobalData.cpp:639) builds a borderless `WS_POPUP\|WS_EX_TOPMOST` "fullscreen" 800x600 window and disables painting. `ApplicationHWnd` is assigned only after `ShowWindow/UpdateWindow` (messages are dispatched before it is set). | Native presentation is a *windowed* swap chain and `Init_Native_Presentation` throws unless the window is exactly 800x600 (ww3d.cpp:277..279). A null `CreateWindow` would flow into D3D11 and DirectInput. | profile (A0) | Checked creation with `GetLastError` receipts; for the first milestone force windowed client == 800x600 via explicit strict grammar; refuse (not coerce) other requests. Keep legacy path unchanged on Win32. | Extract window/style computation into a pure helper; Win32/AMD64 unit test of style, AdjustWindowRect and client size; strict-grammar rejection tests. |
| A0 | A | No startup profile exists. `StartupDiagnostics` accepts only fixed diagnostic grammars (StartupDiagnostics.h:52..93) and always requires `-headless`. | Ordinary startup needs an explicit, strict, AMD64-only mode (windowed 800x600, no logo, no shell map, winCursors, isolated user data) so unvalidated options cannot silently widen scope. | - | Add a distinct mode in the existing strict-grammar style; do not reuse init-only/diagnostic success meaning. | Extend `NativeStartupGateTest` grammar tests. |
| A2 | A | WinMain.cpp:294..689 application WndProc. IME hook first; `WM_CLOSE`/`WM_QUERYENDSESSION` -> `MSG_META_DEMO_INSTANT_QUIT` if `ThePlayerList` exists else `setQuitting`; `WM_ACTIVATEAPP` -> `setIsActive`, `WM_ACTIVATE` mutes audio; mouse messages -> `Win32Mouse`; `WM_KEYDOWN` swallows *all* keys (return 0) and Escape only `PostQuitMessage`, which `serviceWindowsOS` discards (Win32GameEngine.cpp:134..167; its quit handling is commented out). | All handlers are null-guarded and already native-width (WPARAM/LPARAM/HWND). It is usable as-is. There is **no** `WM_SIZE`->backend resize and no occlusion handling; `gDoPaint` is cleared on first `WM_SIZE`. | A1 | Keep this WndProc for the first milestone; do not fork it. Record that Escape does not quit and that close goes through the command translator (`CommandXlat.cpp:4139`: `TheGameLogic->quit(TRUE)`, which must be verified not-in-game safe). | Source regression for the handled-message table; WM_CLOSE end-to-end through boundary test (see A9). |
| A3 | A | `Win32GameEngine::update` (Win32GameEngine.cpp:73..130) = `GameEngine::update()` **then** `serviceWindowsOS()`; it loops while `IsIconic`, calling `TheLAN`/`TheGameLogic`. | Cannot be used as a harmless pump (handoff). The iconic wait loop touches `TheGameLogic->isInInternetGame` and `TheLAN` and `TheAudio`. | A9 | Menu-only loop owns its own pump order: pump -> client update -> propagate -> present. Reuse `serviceWindowsOS`, not `update`. | Loop unit test with injected clock/pump. |
| B1 | B | `StartupDiagnostics::checkpoint` receipts exist only for diagnostic modes; ordinary WinMain still has MiniDumper/late observer. | Needed for review, not for function. | A0 | Reuse existing checkpoint/observer for the new mode identity. | - |

### Display, WW3D and renderer selection

| ID | Class | Source / what it does now | Why it matters | Dep | Approach | Offline proof |
| --- | --- | --- | --- | --- | --- | --- |
| A4 | A | `W3DDisplay::init` (W3DDisplay.cpp:738..985) non-headless branch: `WW3D::Init(ApplicationHWnd)` -> `Create_Render_Backend` -> `DX8Wrapper::Init` -> `LoadLibrary("D3D8.DLL")`; `Set_Render_Device` (+ up to 3 DX8 retry/enumeration attempts), MSAA/filter/anisotropy, `setGamma`, `init2DScene`, `init3DScene`, `W3DShaderManager::init`, debug display, `DX8WebBrowser::Initialize`; failure throws `ERROR_INVALID_D3D` (message box unless diagnostics enabled, GameEngine.cpp:594..601). | System AMD64 d3d8.dll does not exist; app-local DLL is I386. Ordinary init cannot proceed. Factory swap alone is unsound (X64_15). | A0,A1 | A profile-selected branch inside `W3DDisplay::init`: call `WW3D::Init_Native_Presentation(ApplicationHWnd, 800x600)` instead, skip every DX8-only creator **explicitly with receipts** (not via `m_headless`). Destructor mirrors it: `W3DDisplay::~W3DDisplay` (:420..482) calls `WW3D::Shutdown`, which already routes to `Shutdown_Native_Presentation` when a presentation is active (ww3d.cpp:380); order versus global assets is A7. | Extend the dual-architecture display lifecycle test; assert no `LoadLibrary("d3d8")` path is reachable under the profile (symbol/import and runtime-hook test). |
| A5 | A | `W3DDisplay::draw` (W3DDisplay.cpp:1981..2150): `TestCooperativeLevel` guard (null device -> skipped, safe), `WW3D::Begin_Render(true,true,...)` at :2016 where `PresentationLifecycle::Begin` **throws on depth** (PresentationLifecycle.cpp:16), `drawViews()` -> `W3DView::draw` -> `m_3DScene->doRender` -> `WW3D::Render` which throws "World rendering unsupported" in presentation mode (ww3d.cpp:954/992/1019/1090). Also `TheWaterTransparency->` deref, `m_breakTheMovie` gate. | Ordinary frame cannot run on the presentation lifecycle: explicit exceptions (good, no silent no-op) but also no menu frame. | A4 | A UI-only draw path selected by the profile: color-only Begin, then `TheInGameUI->DRAW`, `TheGameClient->DRAW`, `TheMouse->DRAW`, End, skipping views/water/shadow/particles/video. Keep legacy `draw` untouched. | Unit test with a recording backend asserting call sequence and that no `WW3D::Render` is issued. |
| A6 | A | D3D11 Render2D accepts only `SentenceShader` and `DiffuseAlphaShader`, rejects `grayscale` (D3D11Backend.cpp:350..361). `W3DPushButton.cpp:387` draws disabled buttons with `DRAW_IMAGE_GRAYSCALE`; additive/other `drawImage` modes build other shader bits. | A hover/pressed/disabled transition or any additive scheme element throws mid-menu. Static composition (X64.21) exercised only normal state. | A5 | Enumerate the actual draw-op/state set of MainMenu + its transitions + ShellMenuScheme, then implement exactly the missing ones in the neutral Render2D/D3D11 contract (grayscale, additive) with pinned tests; leave the rest rejecting. | Recording-backend test replaying real producers over the original MainMenu states; golden-pixel tests for new states against the legacy CPU reference where one exists. |
| A7 | A | Global texture/asset lifetime: ordinary images use `W3DAssetManager`/global `TextureClass` cache; the diagnostic used private atlases. D3D11 residency must be released before backend destruction. | Teardown order. A leaked global residency after `Shutdown_Native_Presentation` is a use-after-free of the device. | A4 | Prove the ordinary asset free-before-WW3D::Shutdown order (destructor 420..482) covers all UI textures, add an explicit residency-count assertion. | Hidden-window WARP test: load via ordinary asset path, destroy in destructor order, assert zero live residencies. |
| A8 | A | Resolution/config truthfulness. `GlobalData::m_xResolution/m_yResolution` can be overwritten by `OptionPreferences` (GlobalData.cpp:1249) and CLI (`-xres`); `W3DDisplay` sizes from them; `Mouse::onResolutionChanged`/`setMouseLimits` assume them; the Options menu resolution path calls `setDisplayMode` (DX8 enumeration). | Backend is fixed 800x600; a 1920x1080 preference would produce a mismatched UI/mouse space or an exception. | A0 | Under the profile, force and receipt 800x600/32/windowed and reject mismatching saved preferences explicitly. Resolution UI stays C/B (below). | Strict-grammar + preference-override unit tests. |
| B2 | B | Fullscreen, MSAA, resize, minimize/occlusion recovery, device removal/recovery, gamma, texture LOD reduction (`setTextureLOD`), `getDisplayMode*`. | D3D11 backend has resize but no loss/recovery policy. | A4 | Reject explicitly under the profile; WM_SIZE with a different client size is a failure receipt. | - |
| B3 | B | Embedded browser (`DX8WebBrowser::Initialize`), `W3DShaderManager`, debug display, screenshots thread, profiler capture. | Not on the menu path. | A4 | Skip with receipts, never success stubs. | - |

### Client subsystems chosen by `m_headless`

`m_headless` picks, in the ordinary non-headless run: MilesAudioManager
(GameEngine.cpp:451), W3DParticleSystemManager (:464), W3DRadar (:488, DX8
texture format queries + `TextureClass` in init), DirectInputKeyboard
(GameClient.cpp:265), W3DMouse (:320), W3DGameWindowManager (:338, loads
`TheTransitionHandler` and, via ControlBar init, ControlBar.wnd/GeneralsExpPoints.wnd
layouts), non-dummy TacticalView (`InGameUI.cpp:1357`), W3DTerrainVisual's water/
smudge/tracks/shadow systems (`W3DTerrainVisual.cpp:216..`), ghost-object manager,
`W3DAssetManager` recolor shortcuts, `playMovie` shortcuts, and
`ControlBar::update` early-out.

| ID | Class | Evidence / issue | Approach |
| --- | --- | --- | --- |
| A9 | A | **No UI-without-world profile.** Flipping `m_headless` would also create DX8-resource world subsystems (radar textures, water/shadow/tracks, W3DView camera). | Introduce a separate profile predicate (not a global `m_headless=false`) and per-subsystem selection: real UI factories (window manager, mouse, keyboard, display) + existing headless/dummy world factories. Keeps the accepted headless world init byte-for-byte. This is the main architecture decision for review. |
| A10 | A | Real window manager at init (`GameWindowManager::init` loads transitions; ControlBar::init creates windows through the real manager) loads UI assets that headless init never parsed. | Parse-only offline test of every layout loaded at init under the real manager; failures are A. Not yet verified here. |
| A11 | A | Mouse cursor mode. `Mouse` default `RM_W3D` (Mouse.cpp:497) or INI `CursorMode`; `-winCursors` selects `RM_WINDOWS`. `RM_DX8` calls `IDirect3DDevice8::SetCursorProperties` and starts a thread (W3DMouse.cpp:332..350, 475..520); `RM_W3D` calls `WW3D::Render` (throws in presentation). | First milestone: force `RM_WINDOWS` (`-winCursors` path already exists) and receipt it; `RM_POLYGON` (uses `drawImage`) is the later visual-parity candidate. DX8/W3D cursor modes rejected explicitly. |
| A12 | A | Keyboard: `DirectInputKeyboard::openKeyboard` (Win32DIKeyboard.cpp:~110..190) uses `ApplicationHInstance/ApplicationHWnd`, `DISCL_FOREGROUND\|NONEXCLUSIVE`, and on every failure only `assert(0)`+`closeKeyboard()` (release: silent no-keyboard). `ApplicationHInstance` is assigned after window creation (WinMain.cpp:873). | Keep DirectInput (dinput8 is a native import). Make the failure observable (explicit receipt/exception under the profile). Mouse-only is acceptable for the *first* interaction gate; keyboard is required before claiming "interactive". |
| A13 | A | IME: `TheIMEManager` (always created) uses `ApplicationHWnd`, `ImmAssociateContext`; WndProc consults it first. | Likely inert for MainMenu (no text entry), but handler order is reachable from the first message. Keep, receipt attach/detach. |
| B4 | B | Audio: MilesAudioManager non-dummy on AMD64: loader gated `_WIN32 && !_WIN64`, `openDevice` turns audio off (X64_11). `WM_ACTIVATE` mutes/unmutes, `AudioManager::UPDATE` runs. | Silent menu is an accepted limitation for the first milestone; must be receipted ("audio unavailable"), not claimed as working. Needs a one-time source check that every `TheAudio->*` call used by MainMenu is neutral when off. |
| B5 | B | Video/Bink: `BinkVideoPlayer` init fails on AMD64; `Display::playMovie` -> `open()` returns null -> returns. `Intro` plays EALogo/Sizzle movies and waits. | Disable intro explicitly (`m_playIntro=m_playSizzle=FALSE`); `Intro` then reaches `Done` immediately (Intro.cpp:42..). |
| B6 | B | `W3DTerrainVisual`, radar, particle, terrain tracks, shadows, water | Remain headless variants under the profile (see A9). |

### Shell, layout, callbacks and menu

| ID | Class | Source / issue | Approach |
| --- | --- | --- | --- |
| A14 | A | Shell push/layout lifecycle is plain UI code: `Shell::push/doPush/shutdownComplete` (Shell.cpp:333..397, 700..790), `WindowLayout::runInit/runUpdate`, `GameWindowManager::update` (processes destroy list + `TheTransitionHandler->update`). No renderer or GameLogic dependency found in these. | Reuse unmodified. |
| A15 | A | `MainMenuInit` (MainMenu.cpp:398..): `showShellMap(TRUE)` (C if `TheGlobalData->m_shellMapOn`), `TheMouse->setVisibility`, name-key lookups, `initialHide`, `TheMapCache->updateCache()` again (file scan), `TearDownGameSpy` only if the peer queue exists (null at start), `TheShell->loadScheme("MainMenu")`, first-run `TheTransitionHandler->reverse("FadeWholeScreen")`, `winSetFocus`. `W3DMainMenuInit` just delegates (custom draw setup commented out). | Under the profile `m_shellMapOn` must be FALSE *before* `MainMenuInit`, so `showShellMap(TRUE)` takes the background-layout branch (BlankWindow) and no `MSG_NEW_GAME`/`InitRandom(0)`. |
| A16 | A | `MainMenuUpdate` (:748..890): `TheGameLogic->isInGame()` (requires the object, not an update), transitions, `HTTPThinkWrapper`/`GameSpyUpdateOverlays` (inert unless `s_asyncDNSLookupInProgress`/overlays exist), `startGame` -> `doGameStart` (C). `MainMenuInput` (:894) packs/decodes mouse as `UnsignedInt` (`SHORTTOLONG`) and is correct under the f6e8c1aee `uintptr_t` `WindowMsgData`. | No change required. Online/Network/Single-Player buttons are boundary-gated (see C items). |
| A17 | A | Transitions: `GameWindowTransitionsHandler::load/init/update` are driven by `GameWindowManager::update`; `MainMenu` relies on several named groups (`FadeWholeScreen`, `MainMenuDefaultMenu*`, ...). The diagnostic nulled the transition owner; real ownership/teardown order is unvalidated. | Validate create/teardown order of `TheTransitionHandler` vs. `TheWindowManager`/display (GameClient destructor order, GameClient.cpp:176..214 shows manager deleted before display). |
| B7 | B | `signalUIInteract` pushes hover hook names into `ScriptEngine::m_uiInteractions` (ScriptEngine.cpp:6559), drained only by `ScriptEngine::update`. Unbounded but tiny without logic update. | Accept; document. |
| B8 | B | Other screens/popups reached from MainMenu (`Options`, `Credits`, `SaveLoad`, `Replay`, message boxes, `QuitMessageBox`). Exit uses `QuitMessageBoxYesNo` + `quitCallback` (`pop`, `setQuitting`, `signalUIInteract`). | Allow Exit only; every other `Shell::push` target is a boundary stop for the first gate. |

### Boundary / class C

| ID | Class | Source / issue |
| --- | --- | --- |
| C1 | C | `GameLogic::update` / `GameEngine::update` logic branch / `GameClient::step`. Must not run in the menu-only loop. |
| C2 | C | Shell map and its dependencies: `showShellMap(TRUE)` with `m_shellMapOn`, `GameLogic::startNewGame`, `MSG_NEW_GAME` handling, `W3DView`/terrain/water/shadows rendering, `InitRandom(0)`. |
| C3 | C | `doGameStart` (MainMenu.cpp:268..290: `MSG_NEW_GAME`, `InitRandom(0)`), Skirmish/Map select, Load/Replay (`TheGameState`, `TheRecorder`), LAN/Online/GameSpy (DNS lookup, `TheLAN`, HTTP), `FileTransfer`. |
| C4 | C | `GameEngine::execute`, `ReplaySimulation`, developer/performance harness hooks inside `execute` (GameEngine.cpp:794..887). |

RNG: only `InitRandom(0)` in `showShellMap(TRUE, shellMapOn)` and `doGameStart`
use or reseed RNG on the menu path; the menu/shell/gadgets use none. With
`shellMapOn=FALSE` the menu consumes no gameplay RNG. `InitRandom()` at
`GameEngine::init` is already part of the accepted headless init.

## Pointer-width findings (reachable only; no generic cleanup)

- `IMEManager.cpp:1441` `(Char*)((UnsignedInt)clist + (UnsignedInt)clist->dwOffset[i])`
  truncates a pointer on AMD64. Real defect, but reachable only when an IME
  candidate list is shown for a text-entry gadget. **B**: not on the MainMenu
  path; fix with the first text-entry screen or earlier if an IME is active.
- `Keyboard.cpp:349` `(UnsignedInt)kLayout & 0xFFFF`: keeps the language id, benign.
- Window messages/mouse: `WindowMsgData` is `uintptr_t` on `_WIN64` (fixed);
  mouse coordinates are packed as `UnsignedInt` and zero-extended; Win32 mouse
  event storage uses `WPARAM/LPARAM`. No further defect found on the traced path.
- `GadgetListBox.cpp:1795` `(Int)list->selections` and similar item-data casts
  were **not** analyzed (not on the MainMenu path); leave until reachable.

## Critical path to the first real interactive-menu candidate

```text
A0 profile/grammar
  -> A1 checked ApplicationHWnd + A8 forced 800x600 windowed
  -> A9 UI-without-world subsystem selection
  -> A4 ordinary W3DDisplay native-presentation init (no DX8 load)
  -> A5 UI-only draw  (+ A7 teardown order)
  -> A14/A15/A17 real manager, transitions, Shell push of MainMenu (shellMapOn=FALSE, no logo)
  -> A11/A12 cursor + input (windows cursor, DirectInput keyboard)
  -> A16/A3 bounded menu-only loop + command-list boundary guard (A9/C1)
  -> A6 interactive state rendering (hover/pressed/disabled draw ops)
```

## Recommended first implementation slice (not started)

**Ordinary-lifetime presentation host:** make the *ordinary* process own the
window and display lifetime for the new profile, still without Shell, input or a
loop.

1. A0: strict profile grammar (AMD64, windowed 800x600, no logo/shell map,
   winCursors, isolated paths; reject everything else).
2. A1: checked `initializeAppWindows` with receipts and the real WndProc/pump.
3. A9/A4: profile-selected `W3DDisplay::init` native-presentation branch that
   replaces `WW3D::Init/Set_Render_Device` and skips DX8-only creators with receipts;
   world subsystems keep the accepted headless variants.
4. A5: a bounded N-frame UI-only `draw` (color clear + the existing real UI
   producers; the existing static composition may be reused only as a content
   source) through the ordinary `W3DDisplay` ownership, then ordinary
   destructor/engine teardown, HWND last, exit 0.

It proves ordinary HWND/WndProc ownership, no `d3d8.dll` load, display lifetime
owned by `W3DDisplay` rather than a diagnostic attachment (A7 order), a truthful
fixed-size configuration, and that the ordinary engine init can coexist with a
modern presentation. It deliberately defers: real window manager/Shell/
`MainMenuInit` (next slice), all input, the menu loop and boundary guard,
hover/pressed/disabled states, audio/video, every other screen, fullscreen/
resize/device loss, and everything in class C.

## Offline validation available before any native run

- Strict grammar tests (`NativeStartupGateTest`) for the new profile.
- Pure window-style/client-size helper tests; WndProc handled-message table check.
- Hidden-window WARP lifecycle test: ordinary asset textures + draw + destructor
  order (extends `d3d11_ui_test` infrastructure).
- Recording-backend replay of the real MainMenu/ShellMenuScheme producers to
  enumerate Render2D shader/state demands (A6) per state.
- Loop/boundary tests with an injected pump and message stream: FRAME_TICK is
  drained, `MSG_NEW_GAME` stops explicitly, `WM_CLOSE` quits.
- Parse-only test of all layouts loaded at init under the real manager (A10).
- Dual-architecture `UIProducerTest` remains the pointer-width regression.

## Not verified by this audit (do not assume)

`InGameUI::draw`/`W3DInGameUI` idle behavior with a dummy view; the complete
`W3DFunctionLexicon` callback set; `GameLogic::quit(TRUE)` when not in game;
`ControlBar::init` asset set; `MilesAudioManager` init/update neutrality when
audio is off; ShellMenuScheme draw-op set; Zero Hour WinMain/GameMain/
GameEngine differences; behavior of saved `Options.ini` on the target machine.

## Implementation: ordinary-lifetime presentation host (first slice)

The audit direction was approved (A9: a narrowly scoped UI-without-world profile,
not a global `m_headless=false`). This slice integrates A0/A1/A4/A5/A7/A8/A9.
Implementation commit `66d23d7d1`; no title was executed by the implementer.

### What changed (all `_WIN64`-guarded, Generals only)

| Area | Change |
| --- | --- |
| Profile (A0) | `-win -xres 800 -yres 600 -nologo -noshellmap -winCursors -nativeOrdinaryHost <root>`, exactly 11 argv tokens, exact values, case-insensitive names, Generals only (`RTS_ZEROHOUR` rejects), Win32 never enables it. Any missing/extra/reordered/different token (including `-fullscreen`, other resolutions, replay/developer switches) fails `configure` and exits 87 instead of being coerced. `StartupDiagnostics::ordinaryHost()` is orthogonal to `m_headless`. |
| World selection (A9) | `m_headless` stays TRUE (forced by the existing diagnostic parse path), so audio/radar/particles/terrain/view/ghost/asset-recolor/GameClient keyboard-mouse-window-manager keep the accepted headless variants unchanged. Only the application window and `W3DDisplay` lifetime become real. `m_winCursors` is set by the profile. |
| Window (A1) | `initializeOrdinaryHostWindow` (WinMain.cpp): same class name, **existing WndProc**, legacy windowed style bits, window-class arrow cursor; every API result checked with `GetLastError` receipts, client must be exactly 800x600 before and after show, `ApplicationHWnd` is the same global. Legacy `initializeAppWindows` and `WndProc` are identical to the X64.21 baseline (contract-tested). `gDoPaint=false` so the loading bitmap never repaints over the swap chain. |
| Display init (A4/A8) | `W3DDisplay::init` gets one `#if _WIN64 else if (ordinaryHost())` branch that calls `initOrdinaryHostPresentation()` instead of the legacy non-headless block: verifies windowed 800x600 and exact client, receipts six bypassed DX8-only initializers, calls `WW3D::Init_Native_Presentation(ApplicationHWnd)`, creates the ordinary `m_2DRender`, sets 800x600x32 windowed and the legacy UV bias, verifies device/target dimensions and audits loaded modules (`d3d8`/`d3dx8` loaded = failure). `WW3D::Init`, `Create_Render_Backend`, `DX8Wrapper`, `Set_Render_Device` are not entered; `DX8Backend.cpp` and `ww3d.cpp` are unmodified. |
| UI-only draw (A5) | In `W3DDisplay::draw`, a profile branch placed before the unchanged `if (m_headless) return;` runs `drawOrdinaryHostFrame()`: color-only `Begin_Render`, ordinary `drawFillRect/drawOpenRect/drawImage/DisplayString::draw`, `End_Render(true)`. No views, water, particles, depth or `WW3D::Render`. |
| Gate | `W3DDisplay::runOrdinaryHostGate` (called from `GameMain` after init, before replay/execute): `TheGameEngine->serviceWindowsOS()` (ordinary WndProc), `TheDisplay->DRAW()`, bounded by the existing `NativeGraphicsSmoke::Evaluate` (5000 ms hold, 300 frames, visible unoccluded S_OK Present required, resize/close/iconic handling). No `update`, `execute`, GameLogic, Shell or message propagation. |
| Teardown (A7) | Display strings freed by the gate; `~W3DDisplay`: `m_2DRender` -> `Free_Assets` (global texture cache) -> `D3D11Backend::Live_Residencies()` must be 0 (else `ui_residency_release fail` and `markFailed` -> exit 1) -> `Shutdown_Native_Presentation` (backend/device) -> normal engine/MiniDumper teardown -> only then `DestroyWindow` + `UnregisterClass` in WinMain. |

The ordinary-host display code lives in its own file
(`W3DDisplayOrdinaryHost.cpp`), separate from the attached diagnostic in
`W3DDisplayPresentation.cpp`, so the accepted diagnostic source is unchanged.

### Deliberately not done

Real window manager, Shell, MainMenuInit, mouse/keyboard (still `MouseDummy`, no
DirectInput), transitions, hover/pressed/disabled states, intro movies, audio
(MilesAudioManagerDummy), fullscreen/resize/device loss, Zero Hour, anything in
class C. `GameClient::update`, `GameEngine::update` and `execute` are never called.

### Offline validation

- AMD64 build of `g_generals z_generals g_googletest z_googletest` and all
  native test targets: 0 errors. Win32 reference full build: 0 errors.
- AMD64 CTest 15/15; `g_googletest`/`z_googletest` 157 each (6 pre-existing
  disabled); Win32 CTest 23/23 and `g/z_googletest` 156 each.
- New: `native_startup_gate_test` 22 (ordinary-host strict grammar incl. every
  mutation, configure/isolation/`markFailed`, window geometry/exact client and
  resize detection, no-D3D8 module audit helper, residency verdict);
  `d3d11_ui_test` residency counter on WARP; Python contract tests (26 total,
  9 new: no global headless flip, display branch ordering and absence of DX8 calls,
  UI-only draw has no world/depth, gate has no update/execute, teardown order,
  legacy WinMain/WndProc/factory/ww3d unchanged, launcher requirements);
  `Test-NativeOrdinaryHostWorkflow.ps1` 31 synthetic evidence/guard checks;
  existing graphics 81 / native 6 workflow suites unchanged and passing;
  all PowerShell scripts parse.

### Known unverified (the first run exercises them)

1. Engine init after the presentation backend is active: any eager
   `TextureClass::Init()` of an unsupported/absent texture in the remaining
   headless init path would throw (the bridge rejects explicitly). None was found
   by source reading (world resource creators are headless-guarded) but it is not
   proven.
2. WM_IME_* messages now reach `TheIMEManager` with a dummy window manager.
3. Zero live residencies at teardown through the ordinary global cache and
   font/DisplayString paths (the private-clone diagnostic could not prove this).
4. First native-hardware frame through ordinary `W3DDisplay::draw`.

## First reviewed native run (Generals only)

Stage `x64-22-ordinary-host` was staged fresh and preflighted without execution.
Do not rebuild or restage it; a changed build gets a new name.

- Debug GUID `c7323867-d82e-41f3-b190-20a5da4eae06`, age 65.
- EXE SHA256 `A0BDFA5CE1E008284D6903670E99BAE4A49BD94C257548CACFA0FEC400133E66`.
- PDB SHA256 `718C6A95BC7D430A8D00709854B8A966FFB27A81E3EDE8AA2E8BFA4DC2AF96CA`.
- PE32+ AMD64, 30 direct imports (includes `d3d11.dll`, `D3DCOMPILER_47.dll`), no
  delay imports, no D3D8/D3DX imports, no app-local DLLs, no `runs` directory.

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeOrdinaryHost.ps1 -Title Generals -Name x64-22-ordinary-host
./scripts/native-runtime/Invoke-NativeOrdinaryHost.ps1 -Title Generals -Name x64-22-ordinary-host -RunReviewedOrdinaryHost
```

Run the second line only after the first prints PASS.

**Visual expectation (about five seconds, no interaction):** a normal framed,
centered 800x600 window titled "Command and Conquer Generals" with a solid blue
background; the original Generals logo at the top right (x 504..791, y 16..110);
a dark translucent panel with a thin gold border at the right (x 532..756,
y 108..360) containing the localized yellow "Select Difficulty" label at its
top-left; a white caption "AMD64 ordinary host - presentation gate (no gameplay)"
at the top-left of the window. The cursor is the Windows arrow. Do not click,
close or key the window: Escape intentionally does nothing and closing it is a
FAIL (abort). The window then disappears by itself and the console prints PASS.

**PASS requires all of:** exit code 0; the launcher prints PASS (it independently
requires an unoccluded visible S_OK first Present, 1..300 frames covering
5000..6000 ms, `d3d8=0 d3dx8=0` module audit, `live_residencies=0` before backend
release, window released only after engine cleanup, empty
`shutdown-exception.bin`, no dumps, matching paths, final `WinMain complete`);
and you confirm the visual above with no corruption.

**FAIL:** any non-zero exit, timeout, crash/dialog, missing/black/white window,
missing logo/panel/label/caption, garbled text, a window that stays open, or the
launcher message `Ordinary-host validation failed`. The run directory
`build/dev-runtimes/native/Generals/x64-22-ordinary-host/runs/<timestamp>` is
preserved untouched and nothing reruns automatically. Report that path, the
console output and `startup.log`; do not delete or edit the run.

## Acceptance: X64.22 ordinary-lifetime presentation host - FORMALLY ACCEPTED

Reviewed run (never rerun, immutable):
`build/dev-runtimes/native/Generals/x64-22-ordinary-host/runs/20261012T2015215339111Z`.

Independent machine review (not just the launcher verdict):

- OS exit 0, no timeout/error, PID 24172 / thread 2628 on all 292 contiguous
  checkpoints, zero `fail` rows, no `native_shutdown exception`,
  `shutdown-exception.bin` empty, no crash dumps, isolated paths match.
- Staged EXE/PDB SHA256 re-hashed and equal to the launch receipt
  (`A0BDFA5C...3E66` / `718C6A95...6CA`, age 65).
- Exact strict argv in `launch.json`; real `ApplicationHWnd` created with checks
  before engine init (`client=800x600 windowed=1`).
- Display configuration and six receipted DX8 bypasses precede device creation;
  hardware Intel(R) Graphics vendor 0x8086 device 0x7D45, feature 0xB000, 800x600;
  module audit `d3d8=0 d3dx8=0 d3d11=1 dxgi=1` (no D3D8 load).
- Ordinary `TheDisplay.init` -> `TheWindowManager`/`TheShell`/`TheInGameUI`/
  `TheTerrainVisual` init completed with the backend active: the unverified
  "eager TextureClass::Init" risk did not occur.
- Original `GeneralsLogo` through the **global asset cache**, original label and
  DisplayString caption drawn by ordinary `W3DDisplay::draw`; first Present
  S_OK, unoccluded; 239 visible frames in 5000 ms.
- Teardown: `live_residencies=0` before backend release, backend/device before
  engine cleanup, `DestroyWindow` after engine cleanup, MiniDumper shutdown,
  late observer, final `WinMain complete`.
- Developer visual confirmation: expected blue 800x600 window with original UI
  composition and the white caption, normal close, no corruption.

Proves: ordinary WinMain/ApplicationHWnd/WndProc ownership, ordinary headless
engine init coexisting with the modern presentation, ordinary W3DDisplay
init/draw/destructor ownership, global texture residency release order.
Does NOT prove: real window manager, Shell, MainMenu callbacks, transitions,
input/cursor, IME behavior, hover/pressed/disabled draw states, audio/video,
Zero Hour, any class C behavior.
