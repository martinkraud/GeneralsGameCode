# X64.23: ordinary interactive MainMenu (real WindowManager -> Shell -> MainMenu -> input)

Date: 2026-10-12. Baseline: accepted X64.22 (`47bd08345`). Implementation commit
`eacf37eae`. Generals AMD64 only. No title was executed by the implementer; the
first native run is a separate developer gate.

## What this batch integrates

One coherent change from the X64.22 presentation host to the original
interactive menu, built on the approved A9 UI-without-world profile
(`m_headless` stays TRUE, the world/audio/radar/view selection is the accepted
headless one):

| Layer | Change |
| --- | --- |
| Profile | `-win -xres 800 -yres 600 -nologo -noshellmap -winCursors -nativeOrdinaryMenu <root>`: same strict 11-token grammar, distinct token (`-nativeOrdinaryHost` is not accepted for the menu and vice versa). `ordinaryMenu()` implies the shared `ordinaryHost()` window/display lifetime. |
| Real UI components (GameClient.cpp) | Under `ordinaryMenu()` only: `W3DGameWindowManager` (loads the real `TheTransitionHandler`), `W3DMouse` (Windows-cursor redraw mode), `DirectInputKeyboard` (any open failure is an explicit exception under this profile, previously a silent `assert(0)`). Radar, particles, audio, terrain visual, tactical view, ghost objects stay on the headless variants. |
| Shell | The ordinary `Shell`/`WindowLayout`/`MainMenuInit/Update/Input/System` callbacks run unmodified. `GameClient::startMenuOnlyShell()` is exactly the intro-finished branch of `GameClient::update` minus movies and save/replay loading: `showShellMap(TRUE)` then `showShell()`; with the shell map off by profile `showShell()` pushes `Menus/MainMenu.wnd` synchronously. |
| Push guard | `Shell::setPushGuard` (null by default = legacy behavior). The host permits only `Menus/MainMenu.wnd`; any other push (Options -> DX8 enumeration, Skirmish, Credits, ...) is refused and ends the loop as an explicit boundary stop `shell_push`. |
| Input | Win32 messages -> existing WndProc -> `Win32Mouse` events; `Keyboard`/`Mouse::UPDATE` + `createStreamMessages`; the host routes raw mouse/key messages through `WindowTranslator` only (gameplay translators, `propagateMessages` and `TheCommandList` are never involved) and deletes every stream message. |
| Bounded menu-only loop | `W3DDisplay::runOrdinaryMenuHost`: pump, keyboard, mouse, WindowTranslator, `Anim2DCollection`, `WindowManager::UPDATE` (destroy list + transitions), `TheDisplay->DRAW()`, `DisplayStringManager::update`, `Shell::UPDATE`, then a second stream drain for requests raised by callbacks. Never `GameLogic::update`, `GameClient::update`, `GameEngine::update/execute`, `TheAudio->UPDATE`. |
| Draw | `W3DDisplay::draw` profile branch -> `drawOrdinaryMenuFrame`: black color-only frame, `winRepaint()`, `TheMouse->DRAW()`. No views/world/depth. |
| D3D11 Render2D states (A6) | Textured and flat variants of normal alpha, additive (`ONE,ONE`) and opaque (`ONE,ZERO`), plus the legacy disabled-button grayscale combiner (opaque, luminance weights 0.294118/0.584314/0.113725 from the DOT3 `0x80A5CA8E` factor, vertex RGB ignored). WARP exact-product tests. Every other state still throws `Unsupported`. |

## Boundary (class C stays closed)

The loop ends, with an explicit receipt and without emulating success, when:

| Exit reason | Cause |
| --- | --- |
| `menu_exit` | Exit button / `quitCallback` -> `setQuitting` |
| `window_close` | WM_CLOSE / Alt+F4 -> `MSG_META_DEMO_INSTANT_QUIT` |
| `boundary` | `MSG_NEW_GAME`, `MSG_CLEAR_GAME_DATA`, any network message, or a refused Shell push (`shell_push`) |
| `idle_bound` | 120 s without any input |
| `time_bound` | 10 minutes total |

Not entered: GameLogic, shell map/world, execute, replay/save, LAN/online
(Online/Network buttons create overlays/DNS only after their callbacks, and the
resulting screen push or network message is a boundary).

## Offline evidence

- AMD64 and Win32 builds of both titles and all tests: 0 errors.
- AMD64 CTest 15/15, `g/z_googletest` 157 each; Win32 CTest 23/23, 156 each
  (6 pre-existing disabled). Existing graphics 81 / native 6 / host 31 workflow
  checks unchanged; new menu workflow 32; Python contracts 33 (7 new); native gate
  tests 24 (menu grammar/implication); `d3d11_ui_test` 15 including 4 new state
  tests (opaque/additive/gray/flat and the explicit-failure set).
- `menu_texture_census.py` over the staged archives: every image reachable from
  MainMenu, BlankWindow, QuitMessageBox, IMECandidateWindow and MessageBox resolves
  to one of 9 atlas files, all raw 24/32-bit TGA (supported); 0 unsupported/missing.
- Contract tests pin: no global headless flip, factories only under the menu
  profile, `GameEngine.cpp` identical to the accepted baseline, the accepted
  X64.22 gate/draw functions byte-identical, loop free of
  update/execute/GameLogic/propagateMessages, push guard default null.

## Known unverified (first run exercises them)

1. Real-manager init-time layouts (ControlBar, IMECandidateWindow, InGameUI
   windows) never loaded under the native presentation before.
2. The full original shutdown path (`Shell` stack destroy, transition handler,
   window manager before display) in this process.
3. Exact original behavior of hover/pressed transitions and the Single Player
   drop-down; they run original code but were not visually compared.
4. `MilesAudioManagerDummy` request queue growth from button sounds (not
   processed; bounded by loop length).
5. IME messages with a real window manager (no text entry on this screen).

## First reviewed native run (Generals only)

Stage `x64-23-ordinary-menu`, preflighted, never executed; do not rebuild or
restage it (a changed build gets a new name):

- Debug GUID `c7323867-d82e-41f3-b190-20a5da4eae06`, age 67.
- EXE SHA256 `B8C0F45AE8BAD5EBF767C810B3126DFDC765EFCF411424FEE0C0E1186D4112DF`.
- PDB SHA256 `BC384C0291EE2CC2DD289E1D67BE76F9383C3C704E189BADC0970F241260604E`.
- PE32+ AMD64, 30 imports, no delay/D3D8/D3DX imports, no app-local DLLs.

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeOrdinaryMenu.ps1 -Title Generals -Name x64-23-ordinary-menu
./scripts/native-runtime/Invoke-NativeOrdinaryMenu.ps1 -Title Generals -Name x64-23-ordinary-menu -RunReviewedOrdinaryMenu
```

Run the second line only after the first prints PASS.

**What to do and expect.** An 800x600 framed window opens black. Move the mouse
over the window: the original main menu appears (with shell map off the first
mouse movement shows it) with the original transition animation. Then:

1. Hover the buttons: they highlight and the Windows cursor stays an arrow-style
   original cursor; labels are readable.
2. Click `Solo Play` (Single Player): the faction drop-down animates in; click
   `Back`/the main entry to return.
3. Optionally open the multiplayer drop-down and back.
4. End the run by clicking `Exit` (and confirm if a quit box appears). Closing
   the window (X) is also accepted.

Clicking a faction/Training and selecting a difficulty (new game), or `Options`,
`Credits`, `Skirmish`, `Load` etc. is the class C boundary: the run ends
immediately with `exit=boundary` (that is a controlled stop, not a crash). Do
not use the Online/Network buttons.

**PASS requires all of:** exit code 0; launcher prints PASS (it independently
requires: original MainMenu as the only presented screen with Windows cursors,
hardware device, an unoccluded S_OK first Present, at least one proven mouse
movement and one left click, exit reason in {menu_exit, window_close, boundary,
idle_bound, time_bound}, `d3d8=0 d3dx8=0`, `live_residencies=0` before backend
release, window released after engine cleanup, empty `shutdown-exception.bin`,
no dumps, final `WinMain complete`); and your visual confirmation that the menu
looked and responded as the original (no missing/black buttons, no corruption,
no stuck cursor/clipped cursor after exit).

**FAIL:** non-zero exit, hang/timeout, crash or dialog, black window that never
shows the menu after mouse movement, missing/garbled UI, wrong or missing hover
states, cursor remaining clipped to the window after exit, or the launcher
message `Ordinary-menu validation failed`. The run directory
`build/dev-runtimes/native/Generals/x64-23-ordinary-menu/runs/<timestamp>` is
preserved untouched and nothing reruns automatically. Report the path, console
output and `startup.log`; the `fail` row names the exact phase.
