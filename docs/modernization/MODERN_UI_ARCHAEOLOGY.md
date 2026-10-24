# Modern UI archaeology (UI.1)

How the retail Generals / Zero Hour user interface actually works, traced through ownership and lifecycle, and what the project has already built on top of it.
This is the evidence base of [MODERN_UI_ARCHITECTURE.md](MODERN_UI_ARCHITECTURE.md). Everything here was read from the source at HEAD or measured from the installed
game data; where something could not be established it is marked **unverified**.

Method: source reading (`Core/GameEngine/Source/GameClient/GUI`, `.../Shell`, `.../ControlBar`, `Core/GameEngineDevice/.../W3DDevice/GameClient/GUI`, the per-title
`GUICallbacks/Menus`), git history of the Modern Display / Options / HUD milestones (X64.33 - X64.36, X64.52A), and an offline census of the installed archives
(`scripts/native-runtime/ui_archaeology.py`, read-only, output in `evidence/ui-1/wnd-census-{generals,zerohour}.json`).

## 1. Ownership and lifecycle

```
GameEngine::init
 └─ GameClient::init
     ├─ TheWindowManager  (W3DGameWindowManager)     owns every GameWindow; parses WND scripts; hit-tests; dispatches input and draws
     ├─ TheShell          (Shell, stack of 16 WindowLayout) owns the frontend screens
     ├─ TheTransitionHandler (GameWindowTransitionsHandler)   plays WindowTransitions.ini groups on a layout
     ├─ TheControlBar     (ControlBar + ControlBarScheme)     owns the in-game HUD windows (ControlBar.wnd) and the per-general scheme
     ├─ TheInGameUI       (InGameUI)                         world-space UI: selection, hints, placement, tooltips, scroll state
     ├─ TheMouse / TheKeyboard (Win32 + DirectInput devices) produce raw messages
     └─ TheGameText, TheFontLibrary, TheDisplayStringManager, TheMappedImageCollection  strings, fonts, images
```

* **Screen life**: `TheShell->push("Menus/X.wnd")` creates a `WindowLayout`, loads the WND (`GameWindowManager::winCreateLayout` -> `winCreateFromScript`), runs the layout's
  `LAYOUTINIT` callback, shows it (optionally through a transition group), then calls `LAYOUTUPDATE` every frame (`Shell::update`) until `pop()` runs `LAYOUTSHUTDOWN` and
  destroys the windows. The stack is at most 16 deep (`Shell::MAX_SHELL_STACK`); only the top layout receives input; lower ones stay loaded.
* **Window life**: a window is a `GameWindow` node (parent / child / sibling links, `m_status` flags, `m_instData` with draw data and text, a user-data pointer, and four
  function pointers: system, input, tooltip, draw). Ownership is the manager's window list; layouts keep their own list for destruction.
* **Callbacks are the behaviour**: every screen is a pair of C++ files-worth of callbacks in `GUICallbacks/Menus/*.cpp` (Init / Update / Shutdown / Input / System). The WND file
  contains only geometry, drawing data and the *names* of those callbacks. There is no data-driven behaviour, binding or layout logic.
* **In game**: `ControlBar.wnd` (98 windows) is loaded once; `ControlBar` swaps its contents by the selected object's command set (`CommandSet.ini` / `CommandButton.ini`),
  `ControlBarScheme.ini` supplies the per-general artwork and positions; the Modern Enhanced HUD (`ControlBarEnhanced.cpp`) re-places those same retail windows by name.

## 2. The WND files and the 800x600 assumption (measured)

| | Generals | Zero Hour |
|---|---:|---:|
| WND files (effective file system, Patch archives override) | 78 | 80 (adds `ChallengeMenu.wnd`, `ChallengeLoadScreen.wnd`) |
| Windows | 1 532 | 1 667 |
| PUSHBUTTON / STATICTEXT / USER (container) | 382 / 522 / 360 | 424 / 539 / 411 |
| COMBOBOX / SCROLLLISTBOX / CHECKBOX / ENTRYFIELD | 112 / 38 / 23 / 32 | 115 / 39 / 45 / 32 |
| PROGRESSBAR / RADIOBUTTON / HORZSLIDER / VERTSLIDER | 33 / 21 / 8 / 1 | 34 / 19 / 8 / 1 |
| **Creation resolution** | **800x600 for all 1 532** | **800x600 for all 1 667** |
| Fonts used (windows) | Arial 10 / Times New Roman 14 / "Generals" 15 / Arial 8 / 14 / 12 | same families |
| `ControlBarScheme.ini` | 393 lines | 1 347 lines (one scheme per general) |
| `WindowTransitions.ini` | 2 405 lines | 2 596 lines |

Every window of both titles is authored on an 800x600 canvas (`CREATIONRESOLUTION: 800 600`). A WND entry carries: type, rectangle, name (`File.wnd:WindowName`), status flags,
style, the system / input / tooltip / draw callback names, font, header template, tooltip delay, text colours, and three `*DRAWDATA` blocks (enabled / disabled / hilite) of nine
`IMAGE: .., COLOR: .., BORDERCOLOR: ..` slots each. Biggest layouts: ScoreScreen 113 windows, ControlBar 98, OptionsMenu 91, GameSpyGameOptionsMenu 87, SkirmishGameOptionsMenu 73
(32 of them combo boxes: 8 slots x 4 selectors), MainMenu 64.

### Generals vs Zero Hour (UI)
* ZH adds the General's Challenge screens, a second `ControlBarScheme` family (14 schemes against 4), `GenPowersShortcutBar{US,China,GLA}.wnd` with 23 windows against 7 - 11
  (the shortcut bar for the generals' powers), more ScoreScreen / Options / LAN / GameSpy windows (+5 / +3 / +3 / +5), `MapSelectMenu.wnd` is smaller (11 against 15).
* The only difference in the `TheShell->push` graph is `Menus/ChallengeMenu.wnd`, pushed by the Zero Hour menu callbacks (Generals has no such call). Everything else is identical.
* ZH links the FFmpeg video player when `RTS_BUILD_OPTION_FFMPEG` is on (`GeneralsMD/.../W3DGameClient.h`); Generals' `W3DGameClient` always creates `BinkVideoPlayer`.
* The shared engine code lives in `Core/`; the title trees only differ in menu callbacks, ControlBar subclasses and a few gadget details. A Modern UI built in `Core/` serves both.

## 3. Layout, resolution, scaling

### Retail
`GameWindowManagerScript.cpp` converts the WND rectangle at parse time with an **anisotropic** stretch: `xScale = displayWidth / createRes.x`, `yScale = displayHeight / createRes.y`
(the `else` branch of the modern check). At 16:9 the 4:3 art is stretched; on ultrawide it is stretched absurdly. Fonts are fixed point sizes (`GlobalLanguage::adjustFontSize`).
The control bar scales its scheme with `ScreenCreationRes` multipliers; `ProcessAnimateWindow` and the slider drawers use `referenceScale{X,Y}`.

### Modern (already in the tree, X64.33 and later)
* `Common/DisplayGeometry.h` (pure): the reference canvas (800x600) is placed on the physical surface with a **uniform UI scale**, never anisotropic; `Anchor` (Center for the
  frontend, Bottom / BottomRight for HUD files, chosen by `layoutKindForFile`) and `referenceToPhysical`. `DisplayRuntime` holds the live layout; `GameWindowManagerScript` calls it
  for every window when `DisplayRuntime::modern()`.
* Consequences, by design: a Modern frontend screen is the **original 800x600 composition, uniformly scaled and centred**; the surface around it is free (the shell map shows through).
  The world is Hor+ and always fills the whole surface.
* `GlobalLanguage::adjustFontSize` multiplies by the UI scale in Modern; `W3DStaticText` scales its margins.
* Screens that outgrew the canvas got their own pure geometry headers: `OptionsPageLayout.h` + `OptionsTabs.cpp` (tabbed Options), `MatchOptionsLayout.h` +
  `MatchOptionControls.*` (Skirmish Match Options, X64.52A), `HudComposition.h` and `EnhancedHud*.h` (in-match HUD island). Each is correct, tested and **specific**: nothing is shared
  between them, which is the structural cost this phase addresses.
* The Enhanced HUD (`ENHANCED_HUD_FRAMEWORK_V1.md`, frozen) is a table-driven placement of the *retail windows by name* (`EnhancedHudLayout` rows: retail rectangle -> island rectangle)
  plus one procedural renderer and per-faction `Theme`. Retail windows keep ownership of commands and hit regions.

## 4. Gadgets, states, drawing

Gadget types (Core `GUI/Gadget/*`): PushButton, CheckBox, RadioButton, ComboBox, ListBox, StaticText, TextEntry, HorizontalSlider, VerticalSlider, ProgressBar, TabControl
(the TabControl exists in the engine; the retail WND files use none - 0 TABCONTROL in the census; Modern Options builds its tab strip from push buttons, `OptionsTabs.cpp`).

* **Drawing**: a window's `DrawFunc` (e.g. `W3DGadgetPushButtonImageDraw`, registered in the W3D window manager tables) draws from the `*DRAWDATA` images / colours; states are
  enabled / disabled / hilite (+ pushed / selected variants inside the nine slots). There are `...Image...` and `...Draw` (flat colour + border) function families; the retail look is
  image-slot driven.
* **Hover / pressed / disabled**: status bits (`WIN_STATUS_*`) set by the manager from mouse events; no animation system inside gadgets - feedback is image swap plus the sounds below.
* **Sounds** hard-wired in gadget code and menu callbacks: `GUIClick`, `GUIComboBoxClick`, `GUIMessageReceived`, `GUILogoMouseOver`, `GUILogoSelect`, `GUITypeText`,
  `GUITransitionFade`, `GUIButtonsFadeIn`, `GUIBoarderFadeIn`, `GUIScoreScreenTick`, `GUIScoreScreenPictures`, `GUIBlip`, `LoadScreenAmbient`, `GUICommunicatorIncoming`
  (`AudioEventRTS` named events resolved through `Audio*.ini`).
* **Combo box** = text entry + drop-down button + list box child; the list is a sibling child drawn above and is the source of the open-list hit-testing defects found in X64.48A /
  X64.52A (an "above" window must be resolved before its covered siblings; fixed in the manager and audited by `Invoke-MatchOptionsAudit.ps1`).

## 5. Input, focus, navigation

* Raw input -> `Mouse` / `Keyboard` messages -> `GameWindowManager::winProcessMouseEvent` (hit test down the window tree: a child is hit only inside **every ancestor's** rectangle) and
  `winProcessKey` (delivers `GWM_CHAR` to the focus window and its ancestors until one handles it).
* Focus exists (`winSetFocus`, `WIN_STATUS_TAB_STOP`, `winNextTab`) but is used only by text entry fields and a few menus; **there is no general keyboard navigation model** (arrow
  keys / Tab through buttons, activation by Enter, Escape-to-back) - each menu's input callback implements Enter / Escape by hand. No gamepad. No focus visuals.
* Modality: `winSetModal` keeps a modal stack (message boxes, `QuitMessageBox.wnd`, popups); `gogoMessageBox` builds the standard OK / Yes-No dialogs from `MessageBox.wnd`.
* Tooltips: a tooltip callback and delay per window, drawn by `InGameUI` / the manager; text from the CSF string tables.

## 6. Text and localisation
Strings are `GameText` labels (`GUI:Foo`) resolved from the `.csf` tables per language (`TheGameText->fetch`). Layouts size rectangles for English; there is no text measuring or wrapping policy
in layouts (some static texts wrap, many clip). Fonts are TrueType families rasterised through GDI into glyph atlases (`FontCharsClass::Initialize_GDI_Font`), cached per (name, size, bold);
Modern scales the point size. Modern code added in X64.52A uses short English words in the new combos for exactly this reason (unverified for non-English builds).

## 7. Transitions and animation
`WindowTransitions.ini` (2.4k / 2.6k lines) defines named transition groups of `Transition` subclasses (`FadeTransition`, `ScaleUpTransition`, `MainMenuScaleUpTransition`, `ButtonFlashTransition`,
`TextTypeTransition`, `CountUpTransition`, `ControlBarArrowTransition`, `ScreenFadeTransition`, `FlashTransition`, ...) that move / scale / fade named windows of a layout
(`GameWindowTransitionsHandler`, sounds attached). `AnimateWindowManager` / `ProcessAnimateWindow` slide windows in (shell menus). They are keyed by **window names**, so any Modern layout that keeps
the retail window names keeps the retail animations; window rectangles are read at animation time and scaled by `referenceScale`.

## 8. The animated main menu and movies

Findings (measured, `Data/Movies`, `Data/English/Movies`, `Video.ini`, `MainMenu.wnd`):
* **The animated menu backdrop is the real-time 3D shell map** (`Maps/ShellMap1`, scripted camera), not a movie: `MainMenu.wnd` has no video reference; its parent window draws `MainMenuBackdrop`
  and the scene is the world view behind the UI (`TheShell->showShellMap(TRUE)`, `m_shellMapOn`). It already runs on the native engine (the human sequence "shell map, then Skirmish" is documented in X64.45).
* **Movies** (`Video.ini`, 41 entries) are: `EALogoMovie`, `Sizzle` (intro), the General's Challenge screens (`GeneralsChallengeBackground` = `Data/Movies/GC_Background.bik`, the 12 "Portrait*" generals and
  `VSSmall`), and the 15 mission briefings `MD_USA/China/GLA01-05`. Format: Bink 1 (`BIKi`). Measured headers: `GC_Background.bik` 800x600 30 fps 180 frames 150 KB (a 6 s loop, 1 audio track),
  `VS_small.bik` 96x120 71 frames, `sizzle_review.bik` 800x600 1961 frames 23.9 MB, `EA_LOGO.BIK` 720x486, `MD_USA01_0.bik` 800x600 1919 frames 23 MB, `Comp_*Gen_000.bik` 400x324 15 fps.
  None is inside a `.big`; they are loose files under `Data/`.
* **Why they are absent natively**: `BinkVideoPlayer` loads 32-bit `binkw32.dll` (`BinkLoader`), which cannot be loaded into an AMD64 process, so `open()` returns null, callers skip the movie and the
  transition continues; each skip is logged as checkpoint `movie/skipped`. This is *deliberately graceful*, not faked.
* **Existing alternative**: upstream `FFmpegVideoPlayer` (implements the same `VideoPlayerInterface`, reads through `TheFileSystem`, libavcodec decodes Bink 1) behind `RTS_BUILD_OPTION_FFMPEG`
  (vcpkg `ffmpeg` feature, OFF by default). Audit and plan: `X64_32_ORIGINAL_EXPERIENCE_AUDIT.md` section G. Open items there: build the FFmpeg dependency statically, route movie audio to the native
  mixer, and verify the video buffer on the D3D11 dynamic-texture path.
* **The Challenge menu background is the only menu-level animated video** and is Zero Hour only; it is 4:3 and needs a crop/fill policy on 16:9 / 21:9 / 32:9.

## 9. Screens inventory (retail flow, from `TheShell->push` call sites)
`MainMenu` -> `SinglePlayer` flow (Campaign difficulty `DifficultySelect`, `MapSelectMenu`, `SaveLoad` popup, `ChallengeMenu` [ZH]), `SkirmishGameOptionsMenu` (+ `SkirmishMapSelectMenu`),
`ReplayMenu` (+ `PopupReplay`), `OptionsMenu` (+ `KeyboardOptionsMenu`), `CreditsMenu`, multiplayer: `LanLobbyMenu` -> `LanGameOptionsMenu` (+ `LanMapSelectMenu`), `NetworkDirectConnect`,
GameSpy / WOL screens (`WOL*`, `GameSpy*`; online service is dead upstream, code retained), `ScoreScreen`, load screens, `QuitMenu`, popups (`PopupHostGame`, `PopupJoinGame`, `PopupPlayerInfo`,
`PopupLadderSelect`, `PopupCommunicator`). The Skirmish screen is the most complex retail layout (73 windows, 32 combo boxes) and the one that hit the structural limit (X64.52A).

## 10. Classic / Modern seams that already exist
* Switch: `ModernExperience::classic()` (set only by `-classic`, `CommandLine.cpp`) and `ModernExperience::enabled(Feature)` (STARTING_CASH, MODERN_DISPLAY, SMOOTH_MOTION, AI_BEHAVIOR, SUPERWEAPON_LIMIT);
  `DisplayRuntime::modern()` (normal native launch). Gated: WND scaling (`GameWindowManagerScript`), Options tabs (`OptionsTabs::create`), Enhanced HUD (`EnhancedHud::update` gets `classic`),
  the Skirmish match options (value forced to the retail value when the feature is off), font scaling.
* Data seam: Modern data overlays (extended power-bar WNDs, INI) are staged only for Modern packages.
* Locked by tests: `test_modern_ui_contract.py` (this phase), `test_modern_display_contract.py`, `test_modern_settings_contract.py`, `test_enhanced_hud_contract.py`, the layout gtests.

## 11. What can be reused and what is constrained

| Reusable as is | Constrained by retail assumptions |
|---|---|
| `GameWindowManager` tree, hit testing, modal stack, input dispatch | WND = absolute rectangles on 800x600; no containers, anchors, constraints |
| Gadget set and their W3D draw functions (images, colours, sounds) | Behaviour lives in per-screen C++ callbacks; no data binding |
| Shell stack / `WindowLayout` lifecycle | No keyboard / controller navigation model; focus only in text fields |
| `DisplayGeometry` / `DisplayRuntime` (uniform scale, anchors) | Frontend screens are one centred 800x600 composition |
| Transitions and sounds (name keyed) | Transitions assume the retail window names and rectangles |
| `GameText` strings, mapped images, `Theme` machinery of the Enhanced HUD | Text fits are tuned for English; combo/list widths are fixed rectangles |
| The existing pure layout headers (tested) | Each is bespoke (Options, Match Options, HUD) - no shared vocabulary |
| `FFmpegVideoPlayer` interface (opt-in) | Bink SDK is 32-bit only; movies are 4:3 |

## 12. Gaps found while tracing (not fixed here)
1. The in-match Enhanced HUD still places a few retail 30x22 button plates procedurally (`ButtonOptions`, `ButtonIdleWorker`, `ButtonPlaceBeacon`, `PopupCommunicator` in `EnhancedHudImageFit.h`). The
   "legacy Command Menu button" reported on the USA panel could not be identified without a screenshot; candidates are these four (UI.5 scope, needs the human to point at it).
2. TabControl gadget is unused by retail and by the Modern Options (a tab strip is rebuilt from buttons) - a Modern component should own tabs.
3. No automated keyboard-navigation or focus test exists because there is no model to test.
