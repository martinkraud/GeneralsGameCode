# Modern Experience UI - architecture and roadmap (UI.1)

Status: **UI.1 (architecture) and UI.2 (Modern main menu / navigation shell, [MODERN_UI_2_MAIN_MENU.md](MODERN_UI_2_MAIN_MENU.md)) delivered; native acceptance of UI.2 pending. Match Setup, Options and the HUD are not redesigned yet.** Evidence base: [MODERN_UI_ARCHAEOLOGY.md](MODERN_UI_ARCHAEOLOGY.md). Read with
[ENHANCED_HUD_FRAMEWORK_V1.md](ENHANCED_HUD_FRAMEWORK_V1.md) (frozen in-match HUD) and [PROJECT_STATUS.md](PROJECT_STATUS.md).

## 1. Goal and principle

Preserve the identity, atmosphere, content and functionality of Generals / Zero Hour; replace the *structural* limits of the 800x600-era UI (absolute rectangles, one centred canvas,
per-screen hard-coded geometry) with a responsive, extensible interface for modern PCs. **Not** a generic desktop skin: the look stays the Generals console language (the retail button art,
gold/blue chrome, faction emblems, the shell map behind the menu, the sounds and transitions), reinterpreted where the layout needs more room.

Two paths, one codebase: **Classic / Original** = preservation; **Modern Experience** = the expandable path.

## 2. Classic / Modern contract

| | Classic (`-classic`, and any non-modern launch) | Modern Experience |
|---|---|---|
| Layout | retail WND rectangles, anisotropic stretch of the 800x600 canvas | uniform UI scale, anchors, content-width rules, the layout core (section 4) |
| Screens | retail screens exactly (Skirmish with its original controls, original Options page, original control bar) | Modern screens / panels; retail functionality is never removed, only re-homed |
| Match rules | Starting Cash + retail rules; no AI Behaviour / Superweapons controls; values forced to the retail value | all retail + modernization rules |
| Persistence | modern preference keys are neither read nor written for Classic behaviour | per-feature keys (`AIBehavior`, `Superweapons`, ...) |
| Gameplay defaults | unchanged | unchanged (a Modern UI never changes a rule by itself; rules are explicit match options) |
| Gate | `ModernExperience::classic()` / `enabled(feature)` and `DisplayRuntime::modern()` at every entry point | |

Rules that make this testable:
1. **Every Modern UI entry point is gated** by `DisplayRuntime::modern()` and `!ModernExperience::classic()` (or its feature). Pinned by `test_modern_ui_contract.py`; the retail `else` branch of WND scaling stays.
2. **The shared layout core is engine-free** (`Common/ModernUiLayout.h`: no engine includes, no heap, no statics; compile-time checked), so it cannot leak engine state into Classic.
3. **Window names are API.** Modern screens keep the retail window names wherever a retail window exists (callbacks, transitions, tooltips, automation depend on them); new windows get new names.
4. **No simulation coupling.** UI code reads game state; it writes only through the existing command / game-info paths.
5. A Classic regression is a release blocker: `Invoke-RegressionGates.ps1` + the Original golden gate run before every UI milestone package.

## 3. Information architecture

Retail functionality + existing modernization + room to grow. Retail source screens in parentheses.

```
Main Menu (MainMenu.wnd)
 ├─ Single Player
 │    ├─ Campaign: USA / China / GLA + difficulty (SinglePlayerMenu, DifficultySelect, MapSelectMenu)
 │    ├─ Skirmish  -> Match Setup (SkirmishGameOptionsMenu, SkirmishMapSelectMenu)
 │    ├─ Challenge (Zero Hour: ChallengeMenu)
 │    ├─ Load Game (SaveLoad popup)
 │    └─ Replays (ReplayMenu, PopupReplay)
 ├─ Multiplayer
 │    ├─ LAN -> lobby (LanLobbyMenu) -> Match Setup (LanGameOptionsMenu)
 │    ├─ Direct connect (NetworkDirectConnect)
 │    └─ Online (WOL / GameSpy screens: retained, hidden while the service is unavailable)
 ├─ Options (OptionsMenu + KeyboardOptionsMenu)
 │    ├─ Display / Graphics   (existing Modern Display tabs: resolution, mode, UI scale, VSync, FPS, anti-aliasing, anisotropy)
 │    ├─ Audio                (volumes, retail audio options)
 │    ├─ Controls             (keyboard map, scroll factor, edge scrolling, cursor capture, alternate mouse, retaliation, double-click attack-move)
 │    └─ Gameplay / Modern Experience (HUD style Original / Enhanced, Unit Motion, defaults for match rules, accessibility, future switches)
 ├─ Credits, Exit
 └─ (Modern) version / build footer (ModernBranding.h)
```
Rules: every retail control keeps a home (the retail Options controls are listed in `OptionsPageLayout.h` ownership tables and move category by category); a new setting is added to a
category through a **setting row spec** (section 5), never by finding free pixels.

## 4. Component architecture

Layers (bottom up). The first one exists; the others are the design UI.2 - UI.5 deliver.

1. **Geometry and tokens** - `DisplayGeometry` (reference canvas, uniform scale, anchors; exists) and **`ModernUiLayout`** (**delivered in UI.1**): a declarative tree of containers (`Row` / `Column`)
   and leaves with `pref / min / max` sizes, `weight`, `padding`, `gap`, cross alignment, `scroll`, solved in reference units x UI scale. `contentRect(surface, maxWidth, scale)` gives a
   screen its centred content width (ultrawide gets the *same* content width as 16:9, backgrounds use the whole surface). `validate()` reports overlap / outside-parent / overflow.
   Properties (unit tested): deterministic, allocation-free (plain data: `Tree` 6.9 KB, `Result` 2.4 KB, `kMaxNodes = 96`), adding a row never moves earlier rows, a screen too tall for its surface scrolls instead of overlapping,
   one solve of a 20-rule panel (63 nodes, 3440x1440 at 150 %) measures **1.7 microseconds** (MSVC /O2, development PC, 20 000 solves).
2. **Components** (design): `Panel`, `Section`, `SettingRow` (label + control columns), `MatchRuleRow`, `Button`, `Dropdown`, `Toggle`, `Slider`, `TabStrip`, `ScrollArea`, `List`, `TextInput`,
   `Tooltip/Help`, `Dialog/Modal`, `HudWidget`. A component is **a pure spec** (what it is, its constraints, its binding id) **plus a binding to the existing retail gadget** (`GadgetPushButton`,
   `GadgetComboBox`, ...), created programmatically the way `MatchOptionControls::createCombo` already does. Rationale: reuse the engine's drawing (retail art), sounds, hit testing, modal and
   input paths instead of writing a second widget toolkit; Modern chrome is a *theme* of those gadgets (the Enhanced HUD `Theme` approach, already accepted by native review).
3. **Screens** (design): a screen is a C++ builder that declares its tree, creates/places the windows from the solved rectangles when the surface, the UI scale or its content changes (never per frame),
   and registers its focus order. C++ declarative builders were chosen over a new data format (JSON/INI/WND extension) because the engine's WND parser is fixed, behaviour is already C++ callbacks, and a
   data format can be layered on the same specs later without changing the components. Re-evaluate after UI.3 with two real screens.
4. **Navigation** (design, UI.2): a pure `ModernFocus` model - ordered focusables per screen, directional navigation derived from the solved rectangles, activate / back, hover = focus, focus
   visuals, sounds - bound to the Shell stack. Keyboard-first behaviour that retail never had; mouse behaviour unchanged. No gamepad in scope until the model exists.
5. **Shell** (design): Modern main menu and flow per section 3 on top of `TheShell`'s stack and `WindowLayout` lifecycle (callbacks keep working).

### Layout rules (the contract components obey)
* Reference units x UI scale; no per-resolution offsets anywhere. Rows are `label (min/pref) + control (min/max/weight)`; controls cap at a maximum so a wide surface never produces a 1500 px drop-down.
* Content width per screen type, in reference units: forms 640, Match Setup 1200 (three zones, section 6), Options 760, main menu column 360 (+ free backdrop). Wider surfaces add margins and background, not stretched controls.
* Safe margin 16 reference units; a surface smaller than the minimum content makes the screen scroll (`scroll` containers), it never overlaps or clips.
* Text: labels never wrap into controls (label column has a minimum); long strings get tooltips; layouts are checked with an expanded-text factor (test, section 8).

### Resolution matrix (the layout tests run all of it)
1920x1080, 2560x1440, 3440x1440, 3840x2160, 5120x1440 at UI scales 75 / 100 / 125 / 150 / 200 / 300 %. Classic is covered by the existing retail geometry tests.

## 5. Reusable setting rows
A setting is declared once: `{id, label key, help key, kind (dropdown / toggle / slider / text), values, default, feature flag, preference key, apply hook}`; the Options category or the Match Rules panel
renders every spec through the same `SettingRow` component. The Skirmish AI Behaviour / Superweapons / Starting Cash selectors (X64.52A, `MatchOptionControls`) become the first three specs; their
serialization (`GameInfo` option string keys `AI=`, `SW=`, `SC=`, SkirmishGameInfo xfer versions) stays an explicit, separately tested contract (a UI never invents replay data).

## 6. Match Setup (UI.3 target)

A first-class screen, not a retail panel with controls squeezed in. Three zones on the content rectangle (Match Setup width 1200 reference units):

```
 wide (>= ~1100 content units)                       narrow / high scale: stacked, rules in a scroll area
 ┌───────────── Map ────────────┬──────── Players ───────┬──── Match Rules ───┐
 │ preview, name, size, players │ 8 slots: name, human/AI, │ Economy | AI | Powers │
 │ [Change map]                 │ difficulty, faction/     │ rule rows (specs)     │
 │ map description              │ general, team, color,    │ + room for future     │
 │                              │ start position           │ rules, "Advanced" fold│
 └──────────────────────────────┴──────────────────────────┴───────────────────────┘
                        [Back]                              [Start]  (info / validation line)
```
* **Match Rules panel** = `MatchRulesPanel`: an ordered list of rule specs grouped in sections (Economy: Starting Cash; AI: AI Behaviour; Powers: Superweapons; future: crates, fog, build radius, handicaps, ...).
  Adding a rule = one spec + its gameplay hook; no layout code. Specs carry the feature flag so Classic simply gets an empty panel (the retail Starting Cash stays where retail had it).
* Players: the retail 8-slot grid, same windows (names preserved), laid out by the solver; Generals' Battle Honors / medals and the faction emblems stay (Generals identity); Zero Hour adds the
  generals selection.
* Title identity: Generals and Zero Hour share the layout code; the theme supplies the art (emblems, frame, colours) per title.
* Validation line: the start button explains why it is disabled (no human, map missing, ...) - new information, no rule change.
* The LAN lobby game setup (LanGameOptionsMenu) reuses the same screen with the network slot semantics; the Modern LAN controls are a stated gap since X64.52A.

## 7. Animated background and movies

* **The animated main-menu backdrop is the 3D shell map**, which already runs natively. The Modern frontend keeps it as the backdrop (it is Generals' identity), full-surface (Hor+), with
  the UI on top in the content rectangle. Obligations: a frame-cost budget for the shell map while menus are interactive (measure in UI.2), correct behaviour on 21:9 / 32:9 (verified by the existing wide-display
  measurements, X64.48A), and no UI work on the render thread's critical path.
* **Movies** (Bink 1, 41 `Video.ini` entries: logos, intro, 15 briefings, Challenge screens and the 6 s `GC_Background` loop) need a decoder on AMD64. Proposed architecture: build the upstream
  `FFmpegVideoPlayer` (same `VideoPlayerInterface`, libavcodec decodes Bink 1, reads through `TheFileSystem`) with `RTS_BUILD_OPTION_FFMPEG`; decode off the render path into the existing
  `W3DVideoBuffer` D3D11 dynamic-texture path (proven by the radar); audio through a PCM queue stream on the native mixer, video clock = audio clock; open/prefetch asynchronously at screen init so
  navigation never blocks; a missing decoder or file stays the current graceful skip (logged `movie/skipped`).
* **Scaling policy**: backdrops (`GC_Background`) = *fill* (cover, centre-crop) on any aspect; cutscenes and briefings = *fit* (pillar/letter-box, never stretched); small portrait clips = integer-scaled
  with the layout scale. Pre-render the 6 s loop once into a texture strip if decode cost matters (it is 150 KB).
* **Not done in UI.1.** No movie asset is redistributed; the player reads the user's installed files. Decisions that need the owner: how to ship FFmpeg (LGPL: static linking vs the "no app-local DLL"
  staging rule, section 10), and whether movies are in the UI.2 scope or a parallel "UI.V" track.

## 7b. Modern HUD component strategy (UI.5)

Audit of what exists (X64.33 - X64.36, frozen as Framework v1): one bottom-flush, centred **island** (1000 x 180 reference units, fitted to the surface, `EnhancedHudLayout`), module roles (radar + utility,
selection info, command/build grid of 12 retail slots, economy/power, General rank/XP module, Global Powers dock, Battle Overview, General Powers console), one procedural renderer
(`ControlBarEnhanced.cpp`) and a per-faction `Theme` (colours, bevel, decorative switches; USA, China, GLA implemented). The **retail windows keep ownership** of every command, purchase, tooltip and hit
region; Enhanced only moves them (rows in `EnhancedHudLayout`: retail rectangle -> island rectangle), hides flagged text windows and draws behind them. Zero Hour reuses the side themes (the per-general
retail art lives in `ControlBarScheme.ini`, 14 schemes against Generals' 4); whether the Enhanced HUD should vary per *general* rather than per side is **unverified / a design question**.

Strategy:
* Keep Framework v1 frozen. New HUD work goes through (a) `Theme` data, (b) new *component renderers* that implement the shared component vocabulary (button plate, tooltip, lamp, meter), (c) a
  version bump only with a documented reason and a native review.
* Replace the remaining legacy-looking pieces by themed components, not by moving them again: the four 30x22 procedural plates (`ButtonOptions`, `ButtonIdleWorker`, `ButtonPlaceBeacon`,
  `PopupCommunicator`) and whatever control is the "Command Menu" button the owner saw on the USA panel (to be pointed out on a screenshot first). They become `Button` components (section 4) with the faction theme.
* Tooltips and General Powers console text move to the shared `Tooltip/Help` component so HUD, Options and Match Setup tooltips look alike.
* Responsive rules stay: island fits `min(surface fit, UI scale rule)`, Global Powers dock to the edge, 21:9 / 32:9 keep the world visible and clickable on both sides.
* Classic uses the original control bar unchanged (`EnhancedHud::update(..., classic, ...)`); Original/Enhanced switching restores geometry exactly (already tested).
* Testing: existing `EnhancedHud*` gtests and interactive hit-test tests, extended to the UI.1 resolution matrix; a faction-blind renderer contract (already enforced); per-faction native review.
* No gameplay change: HUD code reads state and issues the same commands through the retail windows.

## 8. Test strategy (more than screenshots)

| Concern | Mechanism | Status |
|---|---|---|
| Layout bounds / overlap / clipping / overflow | gtest `ModernUiLayout.*` over the resolution x scale matrix; `validate()` | **delivered (12 tests)** |
| Ultrawide content width, extensibility ("adding a rule moves nothing") | gtest (same file) | **delivered** |
| Layout cost | gtest micro-budget (solve < 100 us); later UI perf bench in the gate runner | delivered / planned |
| Screen audit on the real windows | extend `Invoke-MatchOptionsAudit.ps1` (overlap + reachability, 18 configs) into a generic `Invoke-UiAudit.ps1` taking screen names | planned (UI.2) |
| Focus traversal / keyboard navigation | pure `ModernFocus` gtests (order, directional moves from solved rects, wrap, activation, back) | planned (UI.2) |
| Mouse interaction / hover / pressed / disabled | the audit's `findWindowUnderMouse` probes; real-input harness (`Invoke-CameraRealInput.ps1` technique) for clicks | planned |
| Control state + option persistence + match-rule persistence | gtests over the setting specs (defaults, parse, round trip); existing `SuperweaponLimit`, `AIBehavior`, `OptionPreferences` tests | partial |
| Classic / Modern separation | `test_modern_ui_contract.py` (**delivered**) + launching `-classic` in the audit and asserting the Modern controls are absent | static delivered / native planned |
| Frontend transitions | audit asserts named transition groups still find their windows | planned |
| HUD component placement | existing `EnhancedHudLayout/Windows` gtests + matrix extension | existing |
| Asset fallback (missing image / video / decoder) | gtests with a stub file system; native with `-novideo` | planned |
| Text expansion / localisation | layouts solved with a 130 % text-width factor must pass `validate()` | planned (UI.3) |
| Native visual acceptance | human, per milestone (checklists in each milestone document) | required |

## 9. Performance policy
No active optimization milestone now; **gates stay**: simulation (golden Original digests, Enhanced repeatability), determinism, presentation / frame pacing (`Invoke-RegressionGates.ps1`, real-input camera gate),
Original preservation, package parity. UI rules: layouts are solved when the surface / scale / content changes, never per frame; no heap allocation in the layout core; no blocking asset or
movie loads during navigation (prefetch at screen init); input handled in the frame it arrives; UI never touches simulation state; menu frame cost with the shell map measured in UI.2. A new UI
baseline (menu frame time, screen open time) is recorded when the Modern UI phase closes, before performance work resumes.

## 10. Phase plan (validated; two adjustments)

| Phase | Content | Exit |
|---|---|---|
| **UI.1** (this) | archaeology, architecture, layout core + tests, Classic contract test, docs | delivered |
| **UI.2** Main frontend shell (**implemented**, native acceptance pending) | component bindings for push button / static text / panel on retail art; `ModernFocus`; Modern main menu + navigation shell (shell map backdrop, transitions kept); generic UI audit; frame-cost measurement | human visual acceptance of the Modern main menu |
| **UI.3** Modern Match Setup | `MatchRulesPanel` + setting specs; map / players / rules zones; Generals + Zero Hour themes; replaces the squeezed Match Options | acceptance + all matrix audits |
| **UI.4** Options + secondary screens | category pages from setting specs; Campaign / Load / Replays / LAN / dialogs | acceptance |
| **UI.5** Modern in-match HUD | replace remaining legacy plates (Options / Idle worker / Beacon / Communicator, and the "Command Menu" control once identified) with themed controls, per faction; same ownership rules as Framework v1 | acceptance per faction |
| **UI.6** Polish and acceptance | full matrix, scales, localisation stress, performance baseline | release candidate |
| **UI.V** (parallel track, adjustment 1) | FFmpeg backend, audio sync, scaling policy, async prefetch | after the licensing decision; independent of UI.2 - UI.5 |

Adjustment 2: focus / keyboard navigation moves into **UI.2** (the shell cannot be reviewed without it), and Match Setup (UI.3) precedes Options (UI.4) because it is the screen that hit the structural limit.

## 11. UI.2 - recommended exact scope
1. Binding layer: build `GameWindow`s from a solved `ModernUi::Tree` (push button, static text, panel), keeping retail names / art / sounds; relayout on surface or scale change only.
2. `ModernFocus` (pure) with gtests + binding to the Shell stack (Tab / arrows / Enter / Escape, hover = focus, focus visual).
3. Modern Main Menu: same entries and callbacks as retail, composed by the solver (design choice below), shell map backdrop, retail transitions and sounds retained, Zero Hour Challenge entry.
4. Generic UI audit script + contract tests for Classic separation; frame-cost measurement of the menu with the shell map.
5. Human-reviewable screenshots at the matrix; package for native review.
Explicitly **not** in UI.2: Match Setup, Options, HUD, movies.

## 12. Needs human design / native approval
1. Main-menu composition (centred column vs left-docked command column over the shell map) - a visual decision.
2. Whether movies join the UI.2 scope and how FFmpeg may be shipped (LGPL static vs the no-app-local-DLL rule).
3. Which on-screen control is the "legacy Command Menu button" on the Modern USA HUD (UI.5).
4. Typography: keep the existing GDI fonts or add a bundled font (legal check required).
5. Whether the Modern frontend should offer a faction-themed variant per title or one shared theme with title emblems.
6. Native visual acceptance of every UI milestone (no automation can replace it).

## 13. GitHub-facing documentation audit (recommendations only, nothing changed on GitHub)
* Done in this phase: README refreshed (status, Zero Hour, match rules, Modern UI direction, roadmap Phase 7b), `docs/modernization/PROJECT_STATUS.md` added, CHANGELOG brought up to date (it had stopped at X64.42).
* Recommended, not performed: a `docs/README.md` index (the `docs/modernization` folder has 100+ milestone files); screenshots / a short capture in the README; an issues label set (`area:ui`, `area:ai`, `native-test`) and an
  issue template for native-test reports (attach `generals64.log`, `Options.ini`, the real-input camera harness output); repository topics; pinning a pre-release with the overlay ZIPs once the owner is satisfied; a
  badge only for a workflow that is actually green on this fork (inherited upstream workflows are not all relevant); a CODEOWNERS or MAINTAINERS note. Legal / non-affiliation wording already exists in the README and must be preserved.
