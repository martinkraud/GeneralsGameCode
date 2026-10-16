# X64.36 Enhanced HUD - China (v1 candidate)

Status: **ACCEPTED** after native review of v2 (`x64-36-enhanced-hud-china-v2`, `dac818a0d`; v1 `f2b34c909` had the hover-tooltip flicker fixed in v2). GLA is not started and begins only after China
passes. CP10 is closed. Shared Enhanced HUD Framework v1 is FROZEN (`ENHANCED_HUD_FRAMEWORK_V1.md`); USA is the reference
and is untouched (every new construction switch is false in the USA theme).

## 1. Audit of the real China runtime data (retail Generals INI/WND archives)

Source: `Data\INI\{Science,Rank,CommandSet,CommandButton,PlayerTemplate,ControlBarScheme,SpecialPower}.ini`,
`Window\{GeneralsExpPoints,GenPowersShortcutBarChina,ControlBar}.wnd`, plus the project's modern-extension overlay.

* **Player template** `FactionChina`: side `China`, intrinsic science `SCIENCE_CHINA`, purchase command sets
  `SCIENCE_CHINA_CommandSetRank1 / Rank3 / Rank8`, shortcut bar `GenPowersShortcutBarChina.wnd` (retail 5 buttons; the project's modern
  extension adds the Neutron Missile, 6 slots).
* **Ranks** (`Rank.ini`, shared by every faction): 5 ranks, skill points 0 / 800 / 1500 / 2500 / 5000, one General Point per rank and
  three at rank 5. The three popup tiers open at `SCIENCE_Rank1 / 3 / 5` (the command sets are legacy-named Rank1 / Rank3 / Rank8).
* **General Powers** (14, every one costs one General Point, `SciencePurchasePointCost = 1`):

| tier (star / rank) | slot | power | prerequisite sciences |
|---|---|---|---|
| 1 star / rank 1 | 1 | Red Guard Training (`SCIENCE_RedGuardTraining`) | `CHINA`, `Rank1` |
| 1 star / rank 1 | 2 | Artillery Training (`SCIENCE_ArtilleryTraining`) | `CHINA`, `Rank1` |
| 1 star / rank 1 | 3 | Nuke Launcher (`SCIENCE_NukeLauncher`) | `CHINA`, `Rank1` |
| 3 star / rank 3 | 1 | Cluster Mines (`SCIENCE_ClusterMines`) | `CHINA`, `Rank3` |
| 3 star / rank 3 | 4-5-6 | Artillery Barrage I -> II -> III | `CHINA`+`Rank3`; `Barrage1`+`Rank3`; `Barrage2`+`Rank3` |
| 3 star / rank 3 | 7-8-9 | Cash Hack I -> II -> III | `CHINA`+`Rank3`; `CashHack1`+`Rank3`; `CashHack2`+`Rank3` |
| 3 star / rank 3 | 10-11-12 | Emergency Repair I -> II -> III (shared with every faction) | `Rank3`; `Repair1`+`Rank3`; `Repair2`+`Rank3` |
| 5 star / rank 5 | 1 | EMP Pulse (`SCIENCE_EMPPulse`) | `CHINA`, `Rank5` |

  The China tree has the **same retail topology as USA** (three standalone entry powers, one standalone power plus three three-step
  chains at rank 3, one standalone power at rank 5) with **entirely different data**. The topology is not assumed: the console reads
  every prerequisite from `ScienceStore::getPrerequisiteSciences` at draw time, so connectors and step numerals follow the data of
  whatever faction is shown (a different tree would draw differently without a code change).
* **Global Powers (shortcut bar)**: Cluster Mines, Artillery Barrage, Cash Hack, Emergency Repair, EMP Pulse (+ Neutron Missile
  from the overlay). The dock measures the real windows and handles any slot count.
* **HUD assets**: the `China8x6` control-bar scheme supplies the `SN*` bitmaps (General button `SNGeneral/H/P/I`, command marker
  `SNEmptyFrame`, `SNBarButtonGen2IN/ON`). They are bitmaps drawn by the retail windows: the Enhanced image policy aspect-fits them;
  the retail "blink" highlight (`SNGeneralH`) is suppressed while Enhanced is active (the HUD has its own notification lamp).
* **China-specific HUD behaviour**: none. Radar, cash, power, rank/XP, command bay, unit info, tooltips and the Battle Overview use
  the shared retail windows and rules. (Gameplay differences - Nuclear Reactor, Hackers, Propaganda - live in objects/upgrades, not in
  the HUD.)
* **China-specific UI assumptions in the Enhanced code**: none; the renderer never tests the faction (a contract test asserts it).
  `factionOfSide` maps any `China*` side (including Zero Hour sub-generals, which share this theme; Zero Hour candidates are not part of
  this review) to the China theme.

## 2. Visual construction (theme only)

USA reads as a precision-machined navy console. China reads as heavy military-industrial equipment:

| construction | USA (reference) | China |
|---|---|---|
| silhouette | chamfered 14-unit shoulders | square shoulders with raised corner caps |
| top edge | lit bevel line | riveted structural girder across the chassis top |
| modules | chamfered, bevelled | square, deeper wells (3 vs 2), L reinforcing brackets on every module corner |
| plates (commands, utility keys, Global Powers, power plates) | single bevel, chamfered | square, double-edged structural bevel |
| fasteners | slotted screws, 2 per bezel | domed rivets: 4 on the radar bezel, along ribs and the girder |
| chassis | brushed grain, vent grilles | flat plate steel, no grilles |
| General Powers / Battle Overview | chamfered window | square window with four corner caps |
| colour | navy gunmetal, blue accents, red attack warning | warm olive-charcoal steel, restrained brick-red accent, red-orange ready/hover, **amber** warning and low-power lamps, gold rank |

State semantics are shared (resting / hover / pressed / locked / available / purchased / notification); only the colours of the roles
differ. "Available" is red-orange (never the rank gold), purchased is gold, the notification is a lamp, the warning is amber.

## 3. Tests

* `EnhancedHudFramework.*`: version 1, layout independent of the faction, every implemented faction more than a recolour.
* `EnhancedHudTheme.ChinaThemeIsAHeavyConstructionNotARecolour`: heavy switches on (and all off for USA), depth hierarchy, content
  contrast, accent / warning / rank roles distinct.
* `EnhancedHudPanels.ChinaPowerHierarchyIsRepresentedExactlyOnceWithOnlyRealConnectors`: the 14 audited powers placed once, no
  overlap, exactly six real connectors (Barrage, Cash Hack, Repair), no connector for Cluster Mines / Red Guard / Artillery Training /
  Nuke Launcher / EMP Pulse, chain depth, tier ranks.
* `EnhancedHudPanels.ChinaAndUsaConsolesShareTheTierGridButNotAnyPowerName`.
* `EnhancedHudInteractive.FactionSwitchesNeverChangeThePopupGeometryOrLeaveResidue`; the existing popup tests (every power + Done
  hit-probed at centre, corners, edge midpoints on 9 surfaces x 5 UI scales, six hidden windows, Enhanced > Original > Enhanced) run
  on the shared retail hierarchy that China uses unchanged.
* Contracts: Framework v1 headers pinned by hash; shared headers name no faction; the renderer contains no faction test and every
  heavy-construction branch is a theme switch.

## 4. Native review of v1 and the hover-tooltip flicker (fix 1)

Native result: China visual direction ACCEPTED (not reopened). One defect: the hover information screen of the General Powers
console flickered for some entries (reported: the two last ones on the left from the 3-star section downward).

**Root cause (from the source, not a delay or redraw problem).** The info screen is the retail `ControlBarPopupDescription` layout.
`GameWindowManager::processMouse` gives the mouse to the topmost ENABLED window under the pointer, and the tooltip callback of the
button under it re-asserts `m_showBuildToolTipLayout` every frame (`ControlBar::update` clears it each frame; the layout hides itself
when it is not re-asserted). In Enhanced the tooltip is placed above the island centred on the command frame
(`getEnhancedTooltipPosition`); the General Powers console is also centred above the island, so on many surfaces the tooltip
rectangle overlaps the console. When the tooltip appears over the button the pointer is on, the pointer now belongs to the tooltip
window: the button no longer calls its tooltip callback, the layout hides itself, the button owns the pointer again, the tooltip delay
elapses, the tooltip appears again - a loop with the tooltip visible for a frame or two. Which powers flicker depends on the
tooltip's size (long descriptions are taller), the surface and the UI scale, which is why only some entries (and not the console's
own detail strip, which is drawn, not a window) were affected. The USA console is shared and has the same overlap.

**Fix.** `EnhancedHudPanels::tooltipPlacement`: with the popup open the tooltip is placed clear of the popup's real rectangle (right,
left, between popup and island, above the popup, inside the island strip), and on compact surfaces where nothing is clear it is
placed beside the pointer so it never contains the pointer. Without the popup the placement is exactly the previous one. Original
never calls it (`getEnhancedTooltipPosition` returns FALSE unless Enhanced is active). No delays or redraw suppression were added;
the Design6 information architecture is unchanged.

**Tests.** `HoverTooltipNeverCoversAGeneralPowersButton` (9 surfaces x 5 UI scales x 4 tooltip heights x every power and Done x
centre / corner / edge probes: the pointer is never under the tooltip; with room the tooltip clears the whole console),
`TheOldTooltipPlacementReallyCoveredPowersOnSomeSurfaces` (documents the defect), contract
`test_hover_tooltip_is_placed_clear_of_the_general_powers_console`. The behaviour itself (no flicker on screen) needs native
verification.
