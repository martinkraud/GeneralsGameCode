# X64.36 Enhanced HUD - GLA (v1 candidate)

Status: **ACCEPTED** after native review of `x64-36-enhanced-hud-gla-v1` (`c1cde26c1`). USA, China and GLA are accepted; Framework v1 is frozen; CP10 is closed (`X64_36_CP10_CLOSEOUT.md`). Deferred: the cross-faction polish pass (`ENHANCED_HUD_POLISH_BACKLOG.md`).

## 1. Audit of the real GLA runtime data (retail Generals INI/WND archives; every number read from the data)

* `PlayerTemplate FactionGLA`: side `GLA`, intrinsic science `SCIENCE_GLA`, purchase command sets `SCIENCE_GLA_CommandSetRank1 / 3 / 8`,
  shortcut bar `GenPowersShortcutBarGLA.wnd` (retail 3 buttons; the project's modern-extension overlay makes it 5).
* `Rank.ini` (shared): 5 ranks, skill points 0 / 800 / 1500 / 2500 / 5000, one General Point per rank, three at rank 5. Tiers open at
  `SCIENCE_Rank1 / 3 / 5`.
* **General Powers** (14, each `SciencePurchasePointCost = 1`):

| tier (star / rank) | slot | power | prerequisite sciences |
|---|---|---|---|
| 1 star / rank 1 | 1 | SCUD Launcher | `GLA`, `Rank1` |
| 1 star / rank 1 | 2 | Marauder Tank | `GLA`, `Rank1` |
| 1 star / rank 1 | 3 | Technical Training | `GLA`, `Rank1` |
| 3 star / rank 3 | 1 | Hijacker | `GLA`, `Rank3` |
| 3 star / rank 3 | 4-5-6 | Rebel Ambush I -> II -> III | `GLA`+`Rank3`; `RebelAmbush1`+`Rank3`; `RebelAmbush2`+`Rank3` |
| 3 star / rank 3 | 7-8-9 | Cash Bounty I -> II -> III | `GLA`+`Rank3`; `CashBounty1`+`Rank3`; `CashBounty2`+`Rank3` |
| 3 star / rank 3 | 10-11-12 | Emergency Repair I -> II -> III (shared with every faction) | `Rank3`; `Repair1`+`Rank3`; `Repair2`+`Rank3` |
| 5 star / rank 5 | 1 | Anthrax Bomb | `GLA`, `Rank5` |

  The GLA tree has the same retail grid as USA and China (three standalone, one standalone + three three-step chains, one standalone)
  with entirely different data. As for China the topology is **not assumed**: connectors and step numerals are derived at draw time
  from `ScienceStore::getPrerequisiteSciences`.
* **Global Powers (shortcut bar)**: Rebel Ambush (icon follows the purchased level), Emergency Repair, Anthrax Bomb (+ the overlay's
  SCUD Storm and Radar Van Scan): 5 slots. The dock measures the real windows and takes any slot count.
* **Power / resource**: **GLA has no electrical grid.** All 14 GLA objects that define `EnergyProduction` have 0 (USA: -10..5, China:
  -10..10). The retail power bar exists but always reads 0 / 0. Presentation consequence (theme switch `powerIdle`): an empty grid
  shows the bolt idle (dim) and never "ready"; the segmented track and the real retail gauge are unchanged, and a grid that does carry
  values is shown exactly as for the other factions.
* **Radar, cash, rank/XP, command/build, unit info, tooltips**: the same shared retail windows and rules. GLA-specific gameplay
  (Cash Bounty, Tunnel Networks, Salvage, Black Market) lives in objects/upgrades, not in the HUD.
* **UI assets**: `GLA8x6` scheme: `SUGeneral / H / P / I` (General button), `SUEmptyFrame`, `SUBarButtonGen2IN/ON`, `SUExpBar`,
  `GLALevelUP`, `InGameUIGLABase`. They are retail bitmaps drawn by retail windows: the Enhanced image policy aspect-fits them; the
  retail blink highlight is suppressed while Enhanced is active (as for the other factions).
* **GLA-specific HUD assumptions in the Enhanced code**: none (a contract test asserts the renderer never tests the faction).
  `factionOfSide` maps any `GLA*` side (including Zero Hour sub-generals; not part of this review) to this theme.

## 2. Visual construction (theme only; the shared geometry is untouched)

| construction | USA | China | GLA |
|---|---|---|---|
| idea | precision-machined | heavy industrial | improvised, field-built |
| shoulders | both chamfered | square + caps, girder | **one** chamfered shoulder, one square (asymmetric) |
| modules | identical navy plates | square, L brackets | **mismatched** plates (deterministic per-module tint), small chamfers, triangular **gussets** on two diagonal corners |
| seams | machined seams | riveted | **welded beads** (offset ripples) along the ribs |
| repairs | - | - | riveted **repair plates** straddling two module gaps at different heights |
| plates / keys | chamfered | double-edged | square, welded-flat |
| surface | brushed grain, vents | flat steel | worn brown-grey grain, salvaged radiator grille |
| accents | blue | brick red / amber | restrained leaf green; gold stays rank; red-orange alarm |
| power meter | electrical state | electrical state | idle when the (non-existent) grid is empty |

State semantics are shared (resting / hover / pressed / locked / available / purchased / notification); only the colours differ:
available is green, hover brighter green, purchased gold, the unspent-points notification a lamp, the alarm red-orange.
Asymmetry is presentation only: the geometry, hit regions and information hierarchy are the frozen ones.

## 3. Protection and tests

* Original: every addition is behind the same `EnhancedHud::active()` gates; Enhanced > Original > Enhanced restores exactly (popup
  transition test); a faction switch USA > China > GLA > (unsupported) > GLA > China > USA leaves no residue (the theme is read
  fresh from `EnhancedHud::theme()` every frame; the layout is faction independent; the popup geometry is identical in all).
* Tooltip: placement is shared and faction independent (`EnhancedHudPanels::tooltipPlacement`, China v2 fix): the tooltip never
  covers the popup, and never contains the pointer on compact surfaces; the existing test covers all 14 buttons x probes x surfaces.
* New tests: `GlaPowerHierarchyIsRepresentedExactlyOnceWithOnlyRealConnectors` (14 audited powers once, no overlap, exactly 6 real
  connectors, no connector for Scud / Marauder / Technical / Hijacker / Anthrax, chain depth, tier ranks, shared power only Repair),
  `GlaThemeIsFieldBuiltConstructionNotARecolour` (switches, USA / China colour roles frozen, hierarchy, contrast, role separation),
  framework tests extended to GLA (layout identical for America / China / GLA), supported-side tests, faction-switch test, contract
  `test_gla_theme_is_field_built_construction_and_the_renderer_stays_faction_blind`.

## 4. Known limitations

* Visual quality is judged only natively; the decorative language is intentionally restrained and may need the later cross-faction
  polish pass. Zero Hour GLA sub-generals share this theme but are not part of this review.
