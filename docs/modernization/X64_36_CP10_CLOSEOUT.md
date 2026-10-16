# X64.36 / CP10 closeout - Enhanced Gameplay UI

Status: **ACCEPTED AND CLOSED 2026-10-17.**

| item | result |
|---|---|
| USA Enhanced HUD | PASS - `201b126b1` (native review of `x64-36-enhanced-hud-usa-final`) |
| China Enhanced HUD | PASS - v2 `dac818a0d` (v1 `f2b34c909` had the General Powers hover-tooltip flicker, fixed and re-reviewed) |
| GLA Enhanced HUD | PASS - `c1cde26c1` (`x64-36-enhanced-hud-gla-v1`) |
| Original preservation | PASS - every Enhanced entry point is gated by `EnhancedHud::active()`; Enhanced > Original > Enhanced restoration is exact (popup transition test, bar restoration tests); USA > China > GLA switching leaves no residue |
| Shared Enhanced HUD Framework v1 | FROZEN and proven across the three base Generals factions (`ENHANCED_HUD_FRAMEWORK_V1.md`, `EnhancedHud::kFrameworkVersion`, content-pinned shared headers) |

## What was delivered

* Options > Gameplay > Interface: Original | Enhanced (default Original), available for USA, China and GLA; every other side keeps Original.
* Shared, faction-independent geometry: bottom-flush centred island, radar / unit information / command bay (12 retail slots) /
  cash and power instrumentation / General rank-XP module / General Powers entry, Global Powers edge dock, Battle Overview; responsive
  from 800x600 to 5120x1440, UI Scale aware, compact and ultrawide covered by tests.
* Faction themes separated from the shared functional geometry (`EnhancedHudTheme`): USA precision-machined, China heavy industrial,
  GLA improvised field equipment; the renderer never tests the faction.
* General Powers implemented from the real runtime science data (`ScienceStore::getPrerequisiteSciences`): every power once, only real
  prerequisite connectors, I/II/III steps, real tier ranks, General Points, XP and NEXT promotion, retail buttons own every purchase and
  hit region. The USA, China and GLA trees were audited from the retail archives; each has the same retail grid with different data.
* Visual-state vocabulary shared by all factions (resting / hover / pressed / locked / needs points / available / purchased /
  notification); blue-ish accent rims are state, never decoration.
* The General Powers tooltip flicker (China v1): root cause was the retail tooltip window (enabled, topmost) being placed over the
  console, taking the pointer from the button under it so the button's tooltip callback stopped and the layout hid and reappeared in a
  loop. Fixed in `EnhancedHudPanels::tooltipPlacement` (clears the popup rectangle, never contains the pointer on compact surfaces);
  covered by regression tests for every power at centre / corners / edge midpoints.

## Validation at closeout (commit `c1cde26c1`)

Win64: ctest 16/16, `g_googletest` 332, `z_googletest` 331 passed. Win32: ctest 23/23, `g_googletest` / `z_googletest` 294 passed
each. All `scripts/native-runtime` contracts and both PowerShell workflow tests pass. The reference-token guards fail exactly as in
`REFERENCE_TOKEN_GUARD_BASELINE.md` (8 rendering + 1 performance), none caused by CP10.

## Deliberately deferred

* **Cross-faction Enhanced HUD polish** (`ENHANCED_HUD_POLISH_BACKLOG.md`): more faction-specific material detail and stronger separation
  of USA / China / GLA. The current designs are the accepted baseline.
* Zero Hour sub-generals share the faction themes but were not part of native review.
* Next milestone: X64.37 Modern Display / Settings completion (`X64_37_MODERN_SETTINGS.md`). Afterwards: CPU scalability /
  multithreading, starting with measurement (benchmarks, profiling, tick attribution, dependency and determinism analysis); nothing of
  it is implemented.
