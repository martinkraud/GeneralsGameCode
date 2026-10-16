# Shared Enhanced HUD Framework v1 (FROZEN)

Status: **frozen at X64.36 after native acceptance of the USA Enhanced HUD (`201b126b1`).** USA is the reference
implementation. CP10 is still IN REVIEW: it may be proposed for closure only after USA, China and GLA have each passed native
review and Original preservation has passed.

Version constant: `EnhancedHud::kFrameworkVersion == 1` (`Core/GameEngine/Include/Common/EnhancedHud.h`).

## 1. What is frozen (the shared contract)

The framework is what every faction shares. A faction supplies a `Theme` (`EnhancedHudTheme.h`) and nothing else.

| area | contract | where |
|---|---|---|
| Island | one bottom-flush, horizontally centred HUD island, 1000 x 180 reference units, fitted to the surface; compact and ultrawide rules | `EnhancedHudLayout.h` |
| Responsive fitting / UI Scale | layout scale = min(surface fit, UI scale rule); never fullscreen, never clipped, safe at 800x600 .. 5120x1440 | `EnhancedHudLayout.h` |
| Regions | radar, unit/selection information, command/build (12 retail slots), economy/cash, power instrumentation, General rank / XP / promotion module, General Powers entry | `EnhancedHudLayout.h`, `EnhancedHudPanels.h` |
| Global Powers | edge-docked dock of individual plates measured from the real windows (right edge, compact row fallback), any slot count | `EnhancedHudWindows.h`, `EnhancedHudLayout.h` |
| Battle Overview | one architecture: header (title, map, elapsed time), PLAYERS tab (others only when real data exist), player table, Done | `EnhancedHudPanels.h`, `EnhancedHudOverview.h` |
| General Powers console | content-sized console (about 440 x 500 reference units, centred above the HUD): header (rank, XP `n / m`, NEXT, General Points), tiers 1 / 3 / 5 star in rank order, Done in the foot; connectors only between REAL prerequisites (`ScienceStore::getPrerequisiteSciences`), step numerals, detail strip | `EnhancedHudPanels.h`, `EnhancedHudWindows.h` |
| Ownership | the retail windows own every command, purchase, tooltip and hit region; Enhanced only moves, hides (only flagged retail text windows) and draws behind them | `EnhancedHudWindows.h` |
| Hit testing | ancestors contain children (`fitContainers`), no overlap, full plates are hittable (centre, four corners, four edge midpoints verified on the real hierarchies), NO_INPUT marker windows never swallow clicks | `EnhancedHudWindows.h`, interactive tests |
| Bitmaps | aspect-fit / gauge / procedural policy per window; nothing is stretched | `EnhancedHudImageFit.h`, `EnhancedHudImage.h` |
| Original / Enhanced separation | every Enhanced entry point is gated by `EnhancedHud::active()`; geometry, images, visibility and draw functions are remembered and restored exactly (Enhanced > Original > Enhanced) | `ControlBarEnhanced.cpp` |
| Presentation only | no gameplay, simulation, command, economy, rank or science state is written; reads are read-only | contract tests |
| State vocabulary | interaction: resting / hover / pressed; data: available / purchased / locked / needs points; notification (unspent General Points) is a lamp, never a focus outline; blue-ish accent rims are state, never decoration | `EnhancedHudPanels.h` (`Interaction`, `Outline`, `PowerState`) |
| Faction gating | a side is Enhanced only when its theme is implemented; every other side keeps the Original bar | `EnhancedHudTheme.h::implemented` |

Enforcement:

* `EnhancedHudLayout.h` and `EnhancedHudWindows.h` are pinned by content hash in `scripts/native-runtime/test_enhanced_hud_contract.py`
  (`test_framework_v1_shared_headers_are_frozen`); the hashes change only with a deliberate version bump and a reason recorded here.
* The layout is asserted faction independent (`EnhancedHudFramework.LayoutDoesNotDependOnTheFaction`) and the accepted major geometry
  is frozen numerically (`AcceptedMajorGeometryIsFrozen`).
* The shared headers may not name a faction (`test_shared_framework_headers_do_not_name_a_faction`).
* A theme is plain data (`static_assert` trivially copyable / standard layout); a faction cannot move a control or change what a
  control does.

## 2. What is NOT frozen (faction presentation)

Per faction, through `Theme` only: colours; frame construction (corner profiles, bevel weight, rivets, brackets, caps, girders);
decorative geometry; lamps, vents, scratches and other material language; themeable typography accents; and the faction-specific
presentation of the General Powers console, which must adapt to the faction's REAL power hierarchy (the data decide the tree; the
renderer never assumes USA's arrangement - connectors and step numerals are derived from the science prerequisites at draw time).

A new faction passes when `differsBeyondColour(theme, usa)` is true (it is not a recolour), its state roles remain distinct, and
all shared tests run unchanged with the faction active.

## 3. Recorded acceptance

* USA Enhanced HUD (`201b126b1`): native review PASS (bottom HUD, Battle Overview, Global Powers dock, power meter, General Powers
  console, visual-state cleanup). Small visual refinements may follow in a future cross-faction polish pass; USA prototyping is not
  reopened.
* China (`dac818a0d`, v2 after the hover-tooltip fix): native review PASS.
* GLA: implemented (`X64_36_ENHANCED_HUD_GLA.md`), awaiting native acceptance.
* Original preservation: re-verified with every candidate.

## 4. Sequence

1. Freeze v1 (this document and tests) - done.
2. Enhanced China - native review.
3. Enhanced GLA (only after China passes) - native review.
4. Only after USA, China, GLA and Original preservation all pass may CP10 be proposed for closure. It is not closed automatically.

## 5. Roadmap note (not part of this work): parallelism after CP10

The likely next major engineering investigation is multithreading / parallelism for large-game scalability, with the objective of a
stable authoritative 30 TPS and good frame pacing in large AI-heavy battles. It must begin with profiling and architecture analysis,
not arbitrary threading. Audit areas: simulation / main-thread frame composition, AI, pathfinding, visibility / shroud, terrain work,
particle preparation, rendering preparation, animation, asset / resource work, audio, file / streaming work, jobs that can operate on
immutable snapshots, synchronisation points and existing global / singleton state. It must protect deterministic simulation, replay
behaviour, multiplayer / CRC determinism, save / load, object lifetime, command ordering, the authoritative 30 TPS and Original
gameplay. Measure first, identify ownership and dependencies, separate safe parallel work from deterministic authoritative work, then
design the job / thread model. Nothing of this is implemented here.
