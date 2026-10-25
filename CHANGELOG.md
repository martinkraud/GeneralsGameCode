# Changelog - MKRaudstein Modern Engine

Notable changes of the `dev/modern-engine` branch relative to the upstream baseline (merge base `adac468d7`), grouped by area. This is a human-facing summary of roughly 180 commits;
the exact engineering history, evidence and per-milestone records are in [`docs/modernization/`](docs/modernization/) and the reusable findings in [`docs/findings/`](docs/findings/README.md).
Milestone names (`X64.n`, `Stage n`) are the names used by those documents and by commit subjects.

There are no tagged releases yet: this is a development branch, not an end-user release. Original gameplay semantics are the compatibility target throughout; anything that changes them
is called out explicitly (nothing in this list is intended to).

## 2026-10-25 - Modern main menu (UI.2)

* **Modern Experience main menu** for Generals and Zero Hour: a left-docked command panel over the live 3D shell map (Solo Play > Campaign / Challenge (Zero Hour) / Training (Generals) / Skirmish, Multiplayer, Load, Options, Credits, Exit), responsive from 1920x1080 to 5120x1440 and UI scales 75 - 300 %, retail button art, sounds, localised labels and behaviour; keyboard navigation (arrows, Tab, Enter, Escape) with focus restoration; `-classic` keeps the retail frontend. Foundation: pure focus / navigation model, design-system tokens (typography roles, title variants), main-menu model and geometry ([`MODERN_UI_2_MAIN_MENU.md`](docs/modernization/MODERN_UI_2_MAIN_MENU.md)).

## 2026-10-25 - Zero Hour native, packages, Enhanced AI, match rules, camera/input (X64.43-X64.52B) and Modern UI foundation (UI.1)

* **Zero Hour on the native engine:** Zero Hour reaches Skirmish play through the same AMD64/D3D11 path (X64.45 - X64.46); human-tested on a desktop and a laptop. Steam-overlay ZIP packages for both titles (runtime DLLs app-local,
  symbols separate, SHA-256 manifests, clean-install + ZIP == staged candidate validation) and an automatic retention cleanup of old generated packages/stages (X64.47 - X64.48B, X64.52A).
* **Display:** ultrawide black bands and vanishing fog of war fixed; Skirmish slots 7/8 lost clicks on lower drop-down rows (X64.48A).
* **Camera:** held scrolling integrates in presentation time (X64.49); screen-edge scrolling works again when an old `Options.ini` says `ScreenEdgeScrollEnabledInWindowedApp = no`; a real-input (SendInput) camera gate and a
  one-command regression-gate runner (X64.52B).
* **AI:** archaeology and bug audit (X64.50); opt-in per-match *AI Behaviour: Enhanced* with proven fixes (X64.51) and dynamic base/defence (`AI=E2`, X64.52); Original stays the default and is protected by golden replay digests.
* **Match rules:** Skirmish Starting Cash, AI Behaviour and a Superweapons limit (Disabled / Max 1 / 3 / 5 / Unlimited; enforced for humans and both AI modes, replay/save aware) with a rebuilt Match Options layout (X64.52A).
* **Modern UI foundation (UI.1):** archaeology of the retail UI, architecture and phase plan, a pure allocation-free responsive layout core with a resolution/scale test matrix, Classic/Modern contract test
  ([`MODERN_UI_ARCHITECTURE.md`](docs/modernization/MODERN_UI_ARCHITECTURE.md)). No screen was redesigned yet.

## 2026-10-17 - Simulation tail and replay correctness (X64.41-X64.42)

* **Performance:** removed a synchronous disk flush of the normal-launch log from special-power, payload-delivery and group-dispatch code. It caused a 70-110 ms tick every 300 logic frames and
  55-84 ms update spikes in large AI battles (same-executable A/B on the heavy benchmark: ticks over the 33.3 ms Normal-speed deadline 34 -> 4.8, p99.9 17.8 -> 9.1 ms). Opt-in script
  attribution (`GENERALS_SIM_SCRIPTS`). See [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md).
* **Performance:** exact per-search passability memo for the A* line walk (`path_search` -21 %, worst search 167 -> 75 ms).
* **Replay / determinism:** Generals Skirmish replays now record a non-default Starting Cash (`SC=` in the header); before this a replay recorded with 50 000 cash re-simulated with 10 000 and diverged at the
  first purchase it could not afford. Default-cash replays are unchanged. Per-frame CRC trace and comparison tooling for localising divergences.
* **Documentation:** retail Game Speed audit (Normal = 30 logic frames per second in both games) and a Game Speed design document (nothing implemented).

## 2026-10-16 - CPU scalability series (X64.38-X64.40) and Enhanced interface

* **Performance:** measurement foundation (tick distribution, derived deadline/backlog model, determinism fingerprint, headless self-playing benchmark replay).
* **Performance:** `ZoneCalc` exact algorithmic optimization, 23.5 -> 5.5 ms per recalculation, bit-identical results with a retained legacy oracle (`GENERALS_ZONE_LEGACY` / `GENERALS_ZONE_VERIFY`).
* **Performance:** exact index for the retail A* open-list insertion (23.5 M insertions verified identical); worst tick 265 -> 160 ms, derived backlog 2.9 s -> 127 ms in that milestone's A/B.
* **Gameplay UI:** optional **Enhanced** faction interface for USA, China and GLA on a shared HUD framework; Interface Original/Enhanced option.
* **Display / settings:** transactional Apply/Keep/Revert (automatic revert), Render Scale, Advanced display panel (X64.37).
* **Rendering:** exact reuse of decal-shadow geometry between frames (-0.59 ms per frame in the measured scene); a shadow-volume reuse experiment was measured, found useless and reverted.

## 2026-10-14 to 2026-10-15 - Modern display, smooth presentation, audio, normal launch (X64.31-X64.35)

* **Engine:** normal launch of the AMD64 executable without diagnostic options (real main loop, display, window manager, audio), with a bounded `generals64.log`.
* **Audio:** native AMD64 audio mixer behind the original loader interface; fixed audio completion handles that were truncated to 32 bits (sounds disappeared in long sessions); stereo channel orientation,
  max-distance and callback-thread fixes.
* **Display / Modern Experience:** central display geometry (physical/reference/canvas), monitor and mode enumeration, native resolution and borderless window, UI scale with anchors, Hor+ field of view,
  **ultrawide** support, widescreen control-bar extension, a Modern Options panel with tabs (X64.33).
* **Presentation:** optional **Smooth** presentation (presentation-only interpolation of ground units, aircraft, projectiles, headings, turret yaw and barrel pitch) (X64.34 and Stage 3).
* **Gameplay UI:** shortcut-sidebar overlay for Generals; Starting Cash option in Generals Skirmish (an extension; retail Generals has none).
* **Fixes:** a shortcut special power could be dispatched through a cargo plane (Daisy Cutter); a crash on laser textures with mip chains.
* **Instrumentation:** renderer/world counters and frame-phase attribution for native gameplay (X64.35).

## 2026-10-12 to 2026-10-13 - Native D3D11 world (X64.15-X64.30)

* **Rendering:** Direct3D 11 presentation host, original UI and frontend on D3D11, ordinary main menu, Skirmish setup, map load, live `GameLogic`, native terrain (cloud/noise combiner, shoreline alpha,
  roads, bridges), world objects, picking, scenery, particles, decals, planar shadows, water, sky box, radar, shroud. "First playable" accepted at X64.28.
* **Tooling / validation:** guarded native graphics smoke gates, retained-display lifecycle, preflight of staged candidates (executable and symbol identity, asset inventory, import tables).
* **Fixes:** pointer-width UI message payload (`WindowMsgData`), CPU texture pools, terrain atlas partial mip chains, front-face culling convention, null shadow-manager guards (radius cursor / debris crash).

## 2026-10-09 to 2026-10-11 - AMD64 foundation (X64.1-X64.14, RendererBoundary, D3D11.1/2)

* **Engine / AMD64:** experimental Win64 build; pointer-width-safe diagnostics and stack walking; DX8 declarations separated from x86 linkage; the external D3DX math, FVF, texture-creation,
  filter, error-string and resource helpers replaced with native, reference-tested CPU code (external helper count 3 -> 0 per title); legacy pixel-shader byte code pinned; AMD64 floating-point policy characterised.
* **Engine / AMD64:** fixed `ObjectPoolClass` block layout (payload offset equalled a 4-byte pointer; crash at exit on AMD64).
* **Rendering:** `Render2D` submission, texture residency and sentence content isolated from DX8 behind backend-neutral interfaces; first native D3D11 sentence and file-backed UI texture paths.
* **Tooling:** guarded headless native init-only gate with startup and shutdown-exception diagnostics.

## 2026-10-08 to 2026-10-09 - Timing and profiling (Stage 1-4)

* **Presentation / timing:** offline simulation rate decoupled from render FPS (the authoritative simulation stays at its original 30 ticks per second regardless of the render cap); persistent render FPS
  options; scheduler-aligned presentation clock; ground-unit translation and orientation interpolation.
* **Performance:** bounded performance and pathfinding profiling harness; sampled path-phase profiling; a bounded path-policy hoist and the retail open-list investigation that preceded the X64.40 index.
* **Tooling:** guarded Zero Hour development runtime workflow.

## Known limitations (see the README for the current list)

Movies/cutscenes are incomplete; multiplayer and cross-architecture determinism are not validated; Zero Hour has not received the native gameplay validation that Generals has; the remaining simulation
tail is dominated by rare exhaustive failing path searches (see [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md)).
