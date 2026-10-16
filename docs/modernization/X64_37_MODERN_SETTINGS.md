# X64.37 Modern Display / Settings completion (CP11)

Status: **ACCEPTED AND CLOSED 2026-10-16** - human native test of v4 (`8d4f6838d`) passed (`x64-37-modern-settings-v1`, fixes in `-v2`, final round `x64-37-modern-settings-v3`, Render Scale fix `x64-37-modern-settings-v4`). CP10 (Enhanced HUD) closed first
(`X64_36_CP10_CLOSEOUT.md`). The full control-by-control audit is `SETTINGS_COMPLETION_AUDIT.md`.

## What changed

| area | change | where |
|---|---|---|
| Transaction | Apply -> Keep / Revert with a 15 s automatic revert; failed apply rolls back; safe fallback; pending / current / last-known-good | `Common/DisplayTransaction.h`, `ModernDisplayOptions.cpp`, `Shell::update` |
| Persistence | risky fields (size, mode, monitor, UI scale) written only after Keep; only fields the player changed are written; command-line flags are session overrides | `DisplayConfig::planPersistence`, `applyOverrides`, `DisplayRuntime::State::{stored,overrides}`, `CommandLine.cpp` |
| Monitor | stable identity by Windows device name (`DisplayMonitorName`), index fallback, primary fallback with a logged note; refresh shown read-only | `DisplayConfig::chooseMonitor`, `monitorId` |
| Resolution | mode- and monitor-aware list over all modern aspect families with aspect labels; follows Mode / Monitor before Apply | `DisplayConfig::sizeChoices`, `ModernDisplayOptions::populateResolution/onSelectionChanged` |
| Live display | the logical display now follows every client-size change (mode / monitor too): `Display::setDisplayMode` after the swap-chain resize | `NativeApplyDisplayMode` |
| Controls | gamma slider removed (no backend); language filter, camera and scroll-anchor options restored; in-match controls labelled "(main menu)" | `OptionsPageLayout.h`, `ModernDisplayOptions.cpp` |

## Not done (explicit)

Exclusive fullscreen and a refresh selector, in-match display changes, gamma, anti-aliasing, key rebinding, Network re-flow: see the audit.
No native run was made by the agent.

## Tests

`DisplayTransaction.*` (apply / keep / revert / timeout / wrap-around / failed apply / failed rollback / safe fallback / one transaction for
mode + monitor + scale), `DisplayMonitors.*` (name before index, reordering, disconnected monitor, round trip), `DisplayPrecedence.*`
(command line > stored > defaults, overrides never written back, risky vs safe fields), `DisplayResolutionList.*` (families, fitting,
borderless desktop only, driver modes, labels, refresh semantics), `DisplayGeometryChanges.*` (Enhanced HUD across surface changes and
back to Original, UI scale vs surface), the existing Display* / OptionsPageLayout / Enhanced HUD suites (800x600 ... 5120x1440 and odd
sizes), and `scripts/native-runtime/test_modern_settings_contract.py`.

## v2 (native findings of v1)

Interface dropdown unreachable (sibling checkboxes over the open list: restored checkboxes now share the page's host panel), Advanced
Display panel overlapping the tab strip (now a sub-screen with the tabs hidden), `MISSING: 'GUILanguageFilter'` (Zero Hour token; control
removed), borderless resolution made visibly read-only with its reason, one table for the in-match "(main menu)" gating. Details and the
six-tab audit: `SETTINGS_COMPLETION_AUDIT.md` section 9. CP11 stays IN REVIEW.

## v3 (final settings/display round)

Compact Global Powers dock, real Render Scale (internal 3D resolution, presentation-space UI), live Original <-> Enhanced switching in a match,
and an evidence-based live-change matrix (Render Scale, VSync, frame limit and the HUD are live; resolution / mode / monitor / UI scale stay
main-menu-only with the concrete lifecycle reason). Architecture, matrix and limits: `SETTINGS_COMPLETION_AUDIT.md` section 10. Nothing in v3
is natively verified. CP11 stays IN REVIEW.

## v3 native result and v4

v3: compact dock, live HUD switching and the settings UI were staged for review; **Render Scale failed natively with a black world**
(`SETTINGS_COMPLETION_AUDIT.md` section 11: the composite triangle was culled by the UI raster state). v4 fixes the composite, adds an explicit
end-of-world-phase and a tested per-picture lifecycle, and keeps 100 % on the reference path. Render Scale is not natively verified. CP11 stays
IN REVIEW.

## Native acceptance (v4, `8d4f6838d`)

The human native test of `x64-37-modern-settings-v4` passed: settings usable, Interface selection works, Render Scale works, the v3 black
world is gone. CP11 is ACCEPTED AND CLOSED; settings are not reopened unless a regression appears. The earlier "CP11 stays IN REVIEW" note
above is superseded.
