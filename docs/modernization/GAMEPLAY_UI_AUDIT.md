# Modern Experience: gameplay UI and UX audit (post Zero Hour HUD, owner accepted `71b013524`)

Status: **implemented and automatically / natively validated; awaiting OWNER acceptance.** The accepted HUD at `71b013524` is the visual anchor and was not redesigned. Developer Preview 1 stays frozen at `c55234b5b`.

Scope: audit every gameplay-facing UI surface of both titles when Modern Experience / the Enhanced interface is active, and change only what is a real defect or a clear improvement. Classic is the preservation boundary. No simulation, network, AI, economy, save or replay semantics were touched.

## 1. Method (what was inspected, not assumed)

* **Resources:** every in-game window of both titles from the real archives (`ControlBar`, `ControlBarPopupDescription`, `Diplomacy`, `GeneralsExpPoints`, `GenPowersShortcutBar{US,China,GLA}`, `InGameChat`, `InGamePopupMessage`, `ReplayControl`, `IMEStatus` / `IMECandidate`, `MOTD`, and the in-game menus under `Menus/`), diffed Generals against Zero Hour.
* **Code:** `InGameUI` (messages, floating text, military subtitles, named timers, superweapon countdowns, popup messages, player info list), the Core GUI callbacks, `ModernDialog`, the Enhanced HUD renderer.
* **Native probe (new dev harness, no gameplay effect):** `GENERALS_AUTOFLOW=gameui` starts a Skirmish and, through presentation entry points only, shows the in-match surfaces one after the other (messages, command tooltip, quit menu, chat, Battle Overview, control bar hide / show, General Powers popup) and captures each; combined with `GENERALS_AUTOFLOW_SIDE` it needs no user data. Captured for both titles at 1024 x 768, 1920 x 1080 and 5120 x 1440 (plus the replay / observer view from the seeded benchmark replays at the same sizes).

## 2. Inventory and classification

Classes: **Modernized** (already in the Modern language), **Acceptable retail** (works, belongs to the game, deliberately kept), **Needs polish**, **Needs responsive fix**, **Functional defect**, **Classic-only**, **Out of scope**. Priority: P0 functional / readability defect, P1 strong inconsistency, P2 minor, Preserve.

| # | Surface | Class | Priority | Why / result |
|---|---|---|---|---|
| 1 | Command tooltip (`ControlBarPopupDescription`) | Acceptable retail | Preserve | Legible at 1024 / 1920 / 5120, scaled with the layout, placed above the command region by the Enhanced placement (`bandTop`), long descriptions wrap, cost and hotkey shown. Title-specific retail art kept (decision in section 4). |
| 2 | Cursor tooltips (object / supply dock text) | Acceptable retail | Preserve | Small retail text box; consistent with the retail look; nothing clipped. |
| 3 | Build queue and production feedback | Modernized | Preserve | Enhanced information step (accepted HUD). |
| 4 | Selection information | Modernized | Preserve | Enhanced information step. |
| 5 | Multi-selection | Modernized | Preserve | Retail windows inside the Enhanced module (accepted HUD). |
| 6 | Veterancy and upgrade indicators (HUD icons) | Modernized | Preserve | Enhanced upgrade icons; the world-space veterancy chevrons are renderer art: Out of scope. |
| 7 | General Powers popup / console (Generals) | Modernized | Preserve | Native capture: clean, no overlap (accepted layout). |
| 8 | General Powers popup / console (Zero Hour) | Functional defect | **P1 (fixed)** | Native capture found overlapping retail header text, tier texts left visible and 15-power tier 3 mis-mapped: the Enhanced mapping assumed the Generals window (296 x 432); the Zero Hour retail window is 394 x 431 with 4 / 15 / 4 powers. New Wide popup profile, chosen by the retail window's shape. |
| 9 | Global Powers shortcut dock | Modernized | Preserve | Accepted; Zero Hour's up-to-11-slot bars covered by the compact / column dock tests (previous milestone). |
| 10 | Battle Overview (Diplomacy) | Functional defect | **P1 (fixed)** | Zero Hour: the skin scaled by width (424) but the Zero Hour window is 596 wide, drawing the table 1.4 x too big and out of the window over the HUD; Generals: reached into the stepped HUD. Fixed (scale by height, table spans the window, kept clear of the HUD, shrunk only if it must). |
| 11 | Control bar hide / show | Modernized | Preserve | Native: hides and returns, tooltip does not linger. |
| 12 | Pause | Out of scope | - | The engine shows no pause overlay (pause is a behaviour); adding one is a design change. |
| 13 | Game speed indication (Skirmish Game Speed) | Acceptable retail | Preserve | Set in Match Setup (Modern); in-match feedback is the retail speed messages. |
| 14 | Objective and mission messages | Acceptable retail | Preserve | Script-driven text through the message lines and popup messages (fixed below); no content change. |
| 15 | Mission popup message (`InGamePopupMessage`) | Acceptable retail | Preserve | Positioned through `DisplayRuntime::hudX / hudY` already (scales and stays inside the reference block). |
| 16 | Countdown timers (named timers) | Acceptable retail | Preserve | Top-left anchor (5 %), clear of the HUD; script state needed to show them. |
| 17 | Superweapon countdown | Needs responsive fix | **P1 (fixed)** | Analytic: the INI anchor lands exactly on the top of the Modern command region (over the power meter) and the retail bottom margin lies inside the HUD. Lifted above the HUD when the Enhanced HUD is active. |
| 18 | In-match notification messages | Needs responsive fix | **P0 / P1 (fixed)** | Native: at 1024 x 768 a long line ran off the right edge (clipped), at 5120 x 1440 the retail point size is tiny. Modern wraps inside the surface and scales the size with the height. |
| 19 | Money / resource feedback | Modernized | Preserve | Enhanced cash display; message lines for "insufficient funds". |
| 20 | Subtitles and speech presentation | Acceptable retail | Preserve | Script-driven military subtitles, positioned by the reference scale; not altered. |
| 21 | Floating text (world numbers) | Acceptable retail | Preserve | Drawn in world space at the retail size; legible; renderer-adjacent. |
| 22 | Victory / defeat presentation | Acceptable retail | Preserve | Retail screens scaled in the reference block; unchanged. |
| 23 | End-of-match and score / statistics screens | Acceptable retail | Preserve | Retail menus; consistent with the retail identity. |
| 24 | Replay controls | Acceptable retail | Preserve | `ReplayControl` and the observer bar render and scale; captured at 1024 / 1920 / 5120. |
| 25 | Observer controls / observer HUD | Acceptable retail | Preserve | The observer keeps the Original bar (Enhanced is not offered to the observer in either title); a Modern observer HUD would be a new HUD: recorded as future work. |
| 26 | Beacon UI | Modernized | Preserve | Beacon windows are mapped with the command container by the Enhanced layout. |
| 27 | Multiplayer in-game UI: chat | Acceptable retail | Preserve | Multiplayer only (nothing shows in Skirmish); retail window in the reference block; audited from the code, not natively (needs two PCs). |
| 28 | Multiplayer in-game UI: disconnect / connection status | Acceptable retail | Preserve | Retail `DisconnectWindow`; audited from the code; the real two-PC validation of `2071ad765` remains pending. |
| 29 | Diplomacy / team information | Modernized | Preserve | The Battle Overview player table (row 10). |
| 30 | Confirmation dialogs during gameplay | Modernized | Preserve | `ModernDialog` is applied to every message box created through the window manager. |
| 31 | Quit menu (Esc) | Acceptable retail | Preserve | Native: the retail menu scales with the layout and is readable at 1024 / 1920 / 5120; its identity is part of the game. |
| 32 | Save / Load popup in a match | Modernized | Preserve | Modern popup (UI.3). |
| 33 | Campaign mission UI (objectives, timers, briefing) | Acceptable retail | Preserve | Uses rows 14-16 / 18 / 20; no script or content change; briefing movies are the known 64-bit limitation. |
| 34 | Title-specific Zero Hour surfaces (General's Challenge in-game, 14-slot bar, generals shortcut bars) | Modernized | Preserve | Zero Hour HUD parity milestone. |
| 35 | IME status / candidate windows | Out of scope | - | Text-input method windows, not gameplay presentation. |
| 36 | Message of the day window | Classic-only | - | Online-era window; no Modern equivalent needed. |
| 37 | Game-time / frame-rate overlay | Acceptable retail | Preserve | The optional developer overlay; its top-left text shares the corner with the message lines only when enabled. |

## 3. Prioritised findings

* **P0:** notification line clipped at 1024 x 768 (row 18).
* **P1:** Zero Hour General Powers popup overlap (row 8); Battle Overview over the HUD / mis-scaled in Zero Hour (row 10); superweapon countdown behind the Modern HUD (row 17); retail message size unreadable on 1440p / ultrawide (row 18).
* **P2 / Preserve:** everything else. Not implemented on purpose: a Modern tooltip skin, a Modern quit menu, a Modern observer HUD, toast-style notifications, a pause overlay.

## 4. Tooltip decision

The Zero Hour tooltip is retail navy, the Generals one retail black. **Kept as title identity; no shared Modern tooltip skin.** Evaluated: readability (legible at all three sizes, text scales with the layout), placement (above the command region, clear of the stepped HUD and of the General Powers popup), size and wrapping (long descriptions wrap inside the box), screen-edge behaviour (clamped by `tooltipPlacement`), ultrawide (anchored to the centred command region, not the surface), hotkey and cost / prerequisite lines (all shown, unchanged). A shared skin would change identity without fixing a defect.

## 5. What changed

* `GameplayTextLayout.h` (pure, tested): message point size follows the surface height (100 % up to 900 px, 160 % at 1440 px), message lines wrap inside the surface and never wider than 900 px x scale; the superweapon list is lifted above the Enhanced HUD. `InGameUI::addMessageText` / `postDraw` (both titles) use it only when Modern and not Classic; Classic keeps the retail single line at the retail size.
* Battle Overview: the skin scales by the retail root HEIGHT (397 in both titles; the width is 424 in Generals and 596 in Zero Hour), the table spans the window, and the overview keeps its retail x but is moved up (or, if the free height is too small, scaled down uniformly, never below 55 %) so it never covers the HUD (`overviewFitAboveHud`); the retail geometry is restored for Original.
* General Powers popup: `PopupProfile::Wide` for the Zero Hour retail window (parent 394 x 431: 4 / 15 / 4 powers, its own header geometry), chosen by the shape of the remembered retail window; the Generals profile and every Generals number are untouched. `EnhancedHudWindows.h` now normalises with `popupRetailW()` (pin updated, reason recorded).
* Dev harness: the `gameui` autoflow probe (presentation entry points only).

## 6. Results

* **Responsive (native, both titles):** 1024 x 768, 1920 x 1080 and 5120 x 1440 for messages, tooltip, quit menu, Battle Overview, General Powers popup and hide / show (`evidence/gameplay-ui/gameplay-ui-*.jpg`); replay / observer at 1024 x 768 and 5120 x 1440 (`observer-replay-*.jpg`); the AirForce general's 5-column popup (`general-powers-zerohour-airforce-general-1920x1080.jpg`). The layout rules are additionally unit tested for 800 x 600 .. 5120 x 1440 (`GameplayTextLayout.*`, `EnhancedHudOverview.*`, `EnhancedHudPopupProfile.*`).
* **Classic (native, both titles, 1024 x 768, `-classic`):** unchanged retail presentation: the one-line message (clipped at the surface edge exactly as in retail), the retail Battle Overview, the retail General Powers window, the retail control bar and quit menu (`classic-*-1024x768.jpg`). Every change is gated on Modern and not Classic (messages) or on the Enhanced HUD being active (Overview, popup, superweapon list).
* **Campaign:** the mission-start frame of both titles is the scripted intro camera with the UI hidden by the mission script, so it exercises none of these surfaces; the campaign surfaces reuse rows 14-16 / 18 / 20 (no script, content or balance change).
* **Performance:** no per-frame allocation, font creation or resource lookup was added. Message fonts are created once per message (as before) at a scaled size; the Overview fit and the popup profile run once per show; the superweapon clamp is two integer operations per frame. The HUD frame draw measured in the previous milestone is unchanged.

## 7. Known limitations

* Chat, disconnect and network status are audited from the code, not natively (they need a multiplayer session; the real two-PC validation of `2071ad765` is pending).
* Superweapon and named countdown timers need scripted / special-power game state: the superweapon fix is validated by its layout tests and the analytic anchor, not by a native frame.
* The General Powers popup in Zero Hour was captured for the base USA side and the generals sampled in the evidence; the Boss side cannot be a human in Skirmish.

## 7. Closure: owner review findings (2026-11-11)

Direction accepted by the owner; two findings fixed, nothing else reopened. **Modern Gameplay UI and UX Polish is COMPLETE.**

* **General Powers overlapped the Modern HUD in real Zero Hour play.** Root cause: `popupPlacement` sized the popup from the surface and the UI scale only and, when it was taller than the room above the HUD (`y < 0`), centred it on the whole surface, over the HUD; the earlier validation used the automatic UI scale at three sizes. Fix: `EnhancedHudPanels::popupPlacementClear` (safe area: `popup bottom <= top of the highest HUD step its horizontal extent reaches - gap`, scale reduced only as needed, minimum 0.25; an already fitting popup keeps exactly the accepted placement). Native evidence at 1024 x 768, 1280 x 720, 1920 x 1080, 2560 x 1440, 3440 x 1440, 5120 x 1440 for both titles: `evidence/gameplay-ui/general-powers-safe-area-{zerohour,generals}.jpg`.
* **Selection ring too dominant.** Archaeology, the restrained Modern ring and the Selection Ring setting: [SELECTION_RING.md](SELECTION_RING.md); before / after evidence `evidence/selection-ring/`.
