# Changelog - MKRaudstein Modern Engine

Notable changes of the `dev/modern-engine` branch relative to the upstream baseline (merge base `adac468d7`), grouped by area. This is a human-facing summary of roughly 180 commits;
the exact engineering history, evidence and per-milestone records are in [`docs/modernization/`](docs/modernization/) and the reusable findings in [`docs/findings/`](docs/findings/README.md).
Milestone names (`X64.n`, `Stage n`) are the names used by those documents and by commit subjects.

There are no tagged releases yet: this is a development branch, not an end-user release. Original gameplay semantics are the compatibility target throughout; anything that changes them
is called out explicitly (nothing in this list is intended to).

## 2026-11-06 - Modern Experience UI product-completion audit (UI.4 audit)

* **Audit, not a restart:** the existing Modern UI (UI.1 - UI.3) was measured against the product-completion brief; results, ranked remaining work and the Classic / Modern boundary are in `docs/modernization/MODERN_UI_PRODUCT_COMPLETION.md`.
* **4:3 and 16:10 validated natively** for both titles (1024x768, 1600x1200, 1920x1200) together with the existing 16:9 / 21:9 / 32:9 / 4K matrix: main menu, Options, Match Setup, Replay, 0 defects after the two fixes below; Classic isolation re-checked on both titles.
* **Fixes:** the embedded retail Network block of Options could leave the content viewport at small 4:3 surfaces (the layout now holds a fixed-size block when a scale can, `ModernScreen::Input::minContentW/H`); at 4:3 the Options status line overprinted the build footer (the footer now yields when the status text needs its box).
* **New:** `docs/PRODUCT_ROADMAP.md` (Current -> Developer Preview -> Public Alpha -> Beta -> RC -> 1.0, feature states from the repository); +2 unit tests (559 Generals / 558 Zero Hour). No simulation change: Zero Hour 1x `1E5982E5B3A1B86B`, Generals 1x `1014EE85E384C19C`, golden AI gates PASS.

## 2026-11-06 - Final performance audit and baseline (X64.performance-final)

* **Failing path searches explained:** in Zero Hour about 1 400 searches per match (a quarter of the 1x simulation time, the whole >10 ms tail) exhaust the shared 30 000-record cell-info pool while flooding a basin on the way to a far HUNT target; the goal is reachable, units block the route in ~1 % of the cases, nothing is statically unreachable. It is an existing engine budget, not congestion, and no exact optimisation exists (repeats are never mutation-free, the outcome depends on the free pool and unit costs), so none was made.
* **Diagnostics (opt-in, inert by default):** failure classification floods, per-pair recurrence / recovery and refused-allocation counters in `GENERALS_SIM_PATHFAIL`; `GENERALS_PRESENT_STALL_LOG` probe in the D3D11 backend.
* **Present stalls / first-use hitch:** 0 of 5 new seeded graphical runs reproduced a Present stall; 21 of 22 long hitches in the profiled run were off-CPU stalls (thread not running) landing anywhere, not in Present (environment). First-use hitch attributed to creation and first touch of 104 textures and 146 buffers (CPU-bound, ~130 ms).
* **Final baseline and exit decision:** [PERFORMANCE_FINAL_AUDIT.md](docs/modernization/PERFORMANCE_FINAL_AUDIT.md); the broad performance phase is closed. No gameplay or determinism change (all guard digests, golden gates and tests unchanged). Real two-PC validation of `2071ad765` remains pending.

## 2026-11-06 - AI query / object update audit, seeded graphical benchmark and disk hygiene (X64.aq, X64.dh)

* **Disk (X64.dh):** the dev disk had fallen to 21.8 GB free; `build/` held 74.8 GB. Stages now hard-link their immutable `.big` / `.bik` assets to a content-addressed store (a new Zero Hour stage costs ~0.09 GB instead of ~2.7 GB), existing stages were converted (31.7 GB), the superseded distribution `4f5e092c1` and the overlay-test stages were removed (8.5 GB), in-repo evidence identical to the verified backup was hard-linked (6.4 GB), raw screenshots were pruned (2.3 GB): **free disk 21.8 -> 70.3 GB, no content lost**. Free-space guards in the staging / packaging / benchmark scripts, store pruning, a diagnostic-output retention tool. Details: [DISK_RETENTION.md](docs/modernization/DISK_RETENTION.md).
* **Seeded graphical benchmark:** `Invoke-GraphicalBenchmark.ps1` plays a seeded replay in the normal client and compares its authoritative fingerprint with the headless run (equal on every checkpoint in 8 of 8 runs); `GENERALS_SIM_FINGERPRINT`, `-loadreplay`, VSync off.
* **Exact optimisation:** the HUNT-state enemy scan no longer collects the whole map: staged, bounded scans over the sorted prefix with a provable stop rule (24 713 answers verified against the full scan, 0 differences; all digests unchanged). Seeded graphical Zero Hour mean tick -17 %, p99 -14 % (4 runs); Generals mean -30 %; headless mean -3 %; the sustainable object count rises from ~2 200-2 700 to ~2 700-2 900.
* **Measurements:** query workload by caller, guard / hunt chain (62 % of Zero Hour's failing searches repeat a hunter and goal pair), update-module and AI-state profile, Zero Hour vs Generals gap decomposition, graphical vs headless attribution, congestion classification. Idle-guard scan avoidance and sleeping helper updates were analysed and rejected (not provably exact). Details: [AI_QUERY_AUDIT.md](docs/modernization/AI_QUERY_AUDIT.md).
* No gameplay, determinism or multiplayer change. Real two-PC validation of `2071ad765` remains pending.

## 2026-11-05 - Pathfinding performance (X64.pf) and engine scalability audit (X64.sc)

* **Pathfinding (X64.pf, `83cdd966d`):** exact micro-optimisations of the A* line phase (inlined line walk, memo-hit fast path, integer half-step, lazy terrain height / zone test, hot-first `PathfindCellInfo`), about 17 % less path-search CPU time with the established digests unchanged. A negative-result cache was proven infeasible (0 replayable hits) and a ground-layer `getCell` bypass was rejected because it changed the line-decision digest. Details: [PATHFINDING_PERFORMANCE_AUDIT.md](docs/modernization/PATHFINDING_PERFORMANCE_AUDIT.md).
* **Scalability audit (X64.sc):** deterministic unit-multiplier ladder (`GENERALS_SIM_CLONE`), `getClosestObjects` characterisation (`GENERALS_SIM_GCO`), memory accounting in the bench report, graphical endurance runs. Result: Zero Hour keeps p99 < 25 ms up to about 2 200 objects (6x mobile units) and breaks 30 TPS at about 3 000; Generals stays under 16 ms at 10x. No subsystem is quadratic; the per-unit AI / object update is the first bottleneck. Multithreading is not justified by the measurements. Details: [ENGINE_SCALABILITY_AUDIT.md](docs/modernization/ENGINE_SCALABILITY_AUDIT.md).
* **Upstream:** `28bb4554f` (restore pending path requests after save load) ported to both titles; `1abf73d5d`, `8ed77a761`, `a4ac54b83` deferred and `21323414f` rejected with reasons in the audit.
* No gameplay, determinism or multiplayer change. Real two-PC validation of `2071ad765` remains pending.

## 2026-11-05 - Engine completion audit (AMD64, D3D11, render hitches, CPU tail)

* **AMD64:** full-rebuild pointer-truncation scan (82 sites, 80 benign integer-in-`void*`), upstream #3459 pointer-truncation fixes, 8-byte pool alignment (#3609), UB fixes (#3636, #3635, #3628), key-repeat timing at high frame rates (#3591), recorder range check (#3090) and a set of upstream leak / crash fixes ported with provenance (`cherry picked from` lines).
* **D3D11:** flip-model swap chain (`FLIP_DISCARD`, tearing Present with VSync off) with legacy fallback (`GENERALS_D3D11_BLT=1`); device-removed reason in the failure message.
* **Render hitches:** `GENERALS_FRAMEDELIVERY` now attributes every >= 40 ms frame to a phase / category (`-hitches.csv`, `GENERALS_FRAMEDELIVERY_PROFILE=1`); large frames are simulation ticks (failing path searches after a pathfinder zone recalculation), the 30 ms recurring ones are the terrain full update on structure height changes, early ones are first-use asset work. See `docs/modernization/ENGINE_COMPLETION_AUDIT.md`.

## 2026-11-05 - Multiplayer smoothness and frame pacing audit

* **Network games now interpolate unit/vehicle/turret presentation like offline games.** The presentation clock was disabled whenever `TheNetwork` existed (0 % of render iterations interpolated at 120 Hz, offline 99.8 %): units advanced in 30 Hz steps while the camera ran at the render rate. The lock-step schedule now feeds the clock (read after the limiter wait, never changed); the authoritative 30 ticks/s, commands, RNG and ordering are untouched (golden gates of both titles identical to the X64.51 baseline).
* LAN matches no longer lift the render FPS limit; the negotiated network logic rate can no longer be asked above 30 ticks/s by a high render cap (`NetworkPacing`); a 10 % self-slug no longer resets interpolation history.
* New opt-in measurement: `GENERALS_FRAMEDELIVERY=<dir>` (render interval percentiles, tick spacing, interpolation validity, stalls, negotiated rate), `Invoke-FrameDeliveryRun.ps1` (offline and single-PC LAN autoflow `lan`), `Summarize-FrameDelivery.py`, `Start-GameWithFrameDelivery.ps1`. See `docs/modernization/MULTIPLAYER_SMOOTHNESS_AUDIT.md`.

## 2026-10-25 - Modern Frontend system: full visual redesign and frontend migration (UI.3)

* **A real Generals Modern design system**, not the retail art rearranged: procedural chamfered / hairline / accent-tick components (navigation items, primary / secondary / danger buttons, tabs, dropdowns, sliders with value labels, check boxes, list rows with scroll bars, text entries, dialogs, status line) driven by one component-style table, title accents (Generals sand-gold, Zero Hour orange), typography roles that name real installed fonts (the retail "Generals" family is not installed anywhere; Bahnschrift with Segoe UI / Arial fallbacks, nothing shipped), contrast checked by unit tests. Retail gadgets keep every callback, message and sound.
* **Screens migrated for both titles:** the main menu (full-height navigation band over the shell map, no retail button art), Options (category rail Display / Graphics / Audio / Controls / Gameplay / Modern Experience / Network, rows generated from the page tables, scrolling, Apply / Defaults / Cancel, unsaved-changes line and Back confirmation, Page Up / Down categories, wheel), Skirmish Match Setup (Map | Players | Match Rules, the rules generated from a spec list: Starting Cash, AI Behaviour, Superweapons), Load Game and Replays (list + action column, Modern inline confirmations, full-screen and in-game popup), every message / exit dialog, Credits over the shell map. LAN lobby, Online, Challenge general select, Map select and the Advanced Display popup stay retail and are listed in the document.
* Responsive composition validated at 1920x1080, 2560x1440, 3440x1440, 3840x2160 and 5120x1440 at UI scales 75 / 100 / 150 / 200 / 300 % (the scale is reduced, never clipped; ultrawide keeps a centred 16:9 composition and the live shell map). New tools: `Invoke-FrontendMatrix.ps1`, audit flows `options`, `matchsetup`, `replay`, `loadgame`, `credits`, `wnd_tree.py`; new tests `ModernComponents`, `ModernScreenLayout`, `ModernMatchSetup`, `ModernSettingsModel` (gtest) and `test_modern_frontend.py`. `-classic` creates none of it ([`MODERN_UI_3_FRONTEND.md`](docs/modernization/MODERN_UI_3_FRONTEND.md)).

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
