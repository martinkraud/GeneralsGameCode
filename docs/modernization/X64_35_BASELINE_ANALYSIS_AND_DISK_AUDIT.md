# X64.35 baseline analysis (A, B) and disk-usage audit

Source of truth: `C:\perf\x64-35-baseline\{A1,A2,A3,B1,B2,B3}` (receipts + category CSVs read directly).
Candidate: `x64-35-baseline` (`generalsv.exe` built from `b320513d0`). Machine: ThinkPad L16 Gen 1, Core Ultra 5 125U
(14 logical), 15.9 GB, Intel Graphics (feature level 11_0, 128 MB dedicated / 9 GB shared), client 1920x1200, VSync off.
Status: **baseline COMPLETE (A1-A3, B1/B2/B4, C1-C3; B3 excluded). See section 9. Disk cleanup done (section 9.11). No optimization performed.**

## 1. Settings actually used (facts from the receipts)
* `requested_fps = 120` for all runs: the Quick Game render cap (120) was active, **not uncapped**. `frame_ms` is therefore
  pacer-limited (8.33 ms) and is NOT a throughput measure. The useful CPU measure is work time per frame =
  `outer_frame - frame_pacer_wait`.
* `interpolation(smooth): 0` / `ground_interpolation=0` in **every** run: Smooth was OFF. Treat the baseline as
  "Original motion". It does not change the renderer-submission counters materially; record it and keep it identical for C.

## 2. Results (60 s each)
| run | fps | work ms/frame (outer - pacer wait) | draw calls avg/peak | triangles avg | state_requested | state_changed | changed/requested | texture binds | shader chg | objects | particles avg/peak | logic ticks |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A1 | 119.65 | 2.89 | 160 / 160 | 74.3k | 2947 | 170 | 5.8 % | 56.9 | 22.0 | 552.5 | 11 / 14 | 1800 |
| A2 | 119.81 | 2.15 | 160 / 160 | 74.3k | 2946 | 170 | 5.8 % | 56.9 | 22.0 | 552.6 | 11 / 14 | 1800 |
| A3 | 119.58 | 2.76 | 155 / 155 | 74.3k | 2879 | 163 | 5.7 % | 52.8 | 20.0 | 553.2 | 11 / 14 | 1799 |
| B1 | 119.62 | 3.88 | 266 / 451 | 89.8k | 5132 | 269 | 5.2 % | 123.8 | 18.1 | 608.3 | 405 / 750 | 1799 |
| B2 | 119.79 | 2.94 | 196 / 378 | 85.2k | 3766 | 217 | 5.8 % | 90.8 | 13.1 | 602.1 | 101 / 257 | 1800 |
| B3 | 98.18 | 5.12 | 163 / 322 | 85.2k | 3108 | 194 | 6.2 % | 72.5 | 14.0 | 602.9 | 65 / 112 | 1551 |

## 3. B-run contamination decision
* **No B run was contaminated by `O`.** `O` spawns 96 enemy units; all B runs show 602-608 objects (A: 553), i.e. only
  the +48 from F12. The enemy-spawn workload would give about 700.
* **B3 is anomalous and is rejected from the baseline statistics.** For the first ~45 s it behaves like B1/B2, then
  simulation collapses to ~10-25 ticks/s (ticks per 5 s window: 150,...,126, 46, 30; one 1.74 s `game_logic_update`
  exclusive stall at the very end; 37 long logic frames; 1551 ticks in 61.5 s). Objects/particles are *not* elevated, so it
  is not a heavier workload. Cause unknown (possible OS/background interference or a real logic stall). It is worth
  one look but must not enter the median.
* **B1 vs B2 differ in combat intensity** (particles 405 vs 101) although the workload is otherwise identical: the
  attack-move (B) and the engagement timing are manual, so the intensity of the fight inside the 60 s window varies.
  Both are usable as "medium"; do not read B1->B2 differences as noise in the renderer.
* **Replacement needed: one run, B4**, to replace B3 (B1 and B2 are accepted). A1-A3 are valid and tightly
  consistent; they are not repeated.

## 4. What the baseline already says (observations, not conclusions)
1. **At the 120 fps cap the CPU work is ~2-4 ms per frame**, i.e. ~60-70 % of every frame is pacer wait. The renderer
   is not CPU-bound at 120 fps in A/B. Potential CPU headroom: ~250-450 fps. GPU time is not measured, so
   whether the iGPU limits an uncapped frame rate is unknown.
2. **The pipeline re-issues ~18 state values per draw but only ~5-6 % actually change** (A: 2947 requested vs 170
   changed per frame). This ratio is stable across all runs (5.2-6.2 %) - the most stable metric in the set.
   Hypothesis (below), but the *absolute* cost of 2,800 redundant driver calls per frame is unknown; the per-frame work is
   only 2-4 ms total, so the upper bound of this optimization is the fraction of `display_draw` (1.8-3.2 ms) spent on
   submission.
3. **`textures_created` is large even at idle** (A: ~9,700 per 60 s = ~1.35 / frame; B2: 24,023 = ~3.3 / frame).
   Per-frame texture creation is suspicious (UI text/shroud/cloud? dynamic textures). It is the most unexpected
   number and needs attribution before any decision.
4. **Scaling A -> B**: +48 units gives draws 160 -> 196-266 (+25-65 %), triangles +15-21 %, work ms +0.1..1.0 ms; the
   particle count swing (11 -> 100-400) explains most of B1's extra. Objects are dominated by non-unit objects (~550 in
   A); `drawables_drawn` is only 8-50, so most world objects are not drawn per frame.
5. **Logic**: a 30 Hz tick costs 0.3-0.7 ms on average in A/B; p99 1.4-2.0 ms. Isolated 20-45 ms logic spikes (max
   21.9, 37.9, 43.5, 84.8 ms) occur once or twice per 60 s (also in A) - not tied to workload; likely pathfinding or
   allocations. Worth a look once the C data exists.
6. **Instrumentation**: counters are internally consistent (draw_calls = draws_2d + draws_world; texture binds < draws;
   tri counts repeat across runs to 0.01 %). No evidence that the counters themselves perturb timing (A1 vs A2
   work time differs by 0.7 ms with identical counters, i.e. run noise dominates).

## 5. Missing for a complete baseline
C1-C3 (HEAVY). Required procedure unchanged (fresh launch, same settings, same Smooth state i.e. whatever was used
for A/B = OFF, 120 cap, same resolution, AC power, Best performance):
`Ctrl+Alt+Shift+F10`, `O`, `B`, wait 5 s, `Ctrl+Alt+Shift+F11`, let the 60 s finish; folders `C1`, `C2`, `C3`.
Also one run `B4` (F12, B, wait 5 s, F11). Optional (recommended, one run only): `C-uncapped` after C3 with the FPS
option set to Uncapped, everything else unchanged, to measure throughput when the pacer wait disappears. This needs
a settings change and must not be mixed with C1-C3.

## 6. Hypotheses (not implemented)
| # | Observation | Hypothesis | Change | Expected effect | Risk | Validation |
|---|---|---|---|---|---|---|
| H1 | 2,900 state values requested / 170 changed per frame | redundant D3D11 state submission costs measurable CPU in `display_draw` | bounded cache in `D3D11Backend` that skips unchanged `OMSet*/IASet*/VSSet*/PSSet*` calls (the Note() tracker already identifies them) | fewer API calls, lower `display_draw` ms | stale state after external changes (Reset on Present/resize/ClearState, Invalidate_Cached_Render_States) | state-correctness unit test (shadow state vs ID3D11 queries), pixel-identical readback tests, same A/B/C benchmark |
| H2 | ~1.4-3.3 textures created per frame | something recreates dynamic textures every frame | attribute creation sites (instrument by caller), then reuse/update in place | fewer allocations, lower p99 | texture content correctness | creation counter drops, readback identical |
| H3 | logic spikes 20-85 ms 1-2x/min | allocation or path bursts | attribute with existing path/ai categories | fewer long ticks | simulation determinism | CRC/replay equality, long_logic_frames |
| H4 | 8-50 drawables of 550-600 objects drawn | object loops visit many non-drawn objects | none yet - only if a category dominates after C | - | - | - |

No change will be made before C1-C3/B4 are analyzed and you review this document.

## 7. Disk usage audit (read-only; nothing has been deleted)
Total repository directory: **199.7 GB**, of which `build/` is 199.6 GB and `.git` 0.05 GB (source is < 0.1 GB).
C: has 9.0 GB free of 476 GB (99 % full).

| path | size | purpose | regenerable | needed now | class |
|---|---|---|---|---|---|
| `build/dev-runtimes/native/Generals` (54 staged candidates x ~1.56 GB) | 84.0 GB | staged milestone candidates, each with a **full copy of the retail `.big` assets** (Textures.big 318 MB, SpeechEnglish 257, W3D 176, Music 151, Audio 122, ...) plus `generalsv.exe/pdb` | yes (git + Stage script + installed game) | only `x64-35-baseline` (and `x64-34-smooth-presentation` as fallback) | 7 (old staged candidates) |
| `build/dev-runtimes/native/ZeroHour` (13 x 2.7 GB) | 33.9 GB | old X64.12-20 Zero Hour bring-up candidates | yes | no (ZH stays shared-engine healthy through tests) | 7 |
| `build/dev-runtimes/zh` (25 Win32 stage runtimes x 2.9 GB) | 65.8 GB | Stage 2-4 Win32 reference/profiling runtimes (Oct 7-9) | yes, but they are the "reference" runtimes for those stages | **unknown** | 9 (review) |
| `build/dev-runtimes/userdata-backups` | 3.8 GB | backups of user data | partly | unknown | 9 |
| `build/win64`, `build/win32` | 4.4 / 4.3 GB | CMake/Ninja trees | yes | yes (current build) | 5 |
| `build/performance` | 2.5 GB (2.45 GB in `stage4a1-B-cpu`) | earlier profiling captures | no | evidence | 4 |
| `build/*-evidence`, logs | < 0.2 GB | milestone evidence | no | keep | 4 |
| `.git`, source, docs | 0.05 GB | | | | 1/2 |

**Why 200 GB**: each `Stage-Native*` run copies the full retail asset set (~1.5 GB) into a new candidate folder;
~67 candidates x 1.5-2.7 GB = ~118 GB, plus 66 GB of Win32 Stage 2-4 runtimes, 3 GB of crash dumps and 12 GB of build trees.
The PDB/obj/lib build output itself is small (the two build trees total 8.7 GB).

### Evidence inside candidates that must be preserved before a candidate is removed
`runs\` and `normal-launch\` directories contain 3.06 GB: 6 full crash dumps (x64-21-fix1 339 MB, x64-27 357 MB,
x64-28 565+568 MB, x64-31-fix1 745 MB, x64-32 363 MB; all from crashes fixed in accepted milestones), 20 replays and
66 logs. The plan archives these, plus each candidate's `manifest.json`/`ordinaryplay.json` (exe hashes), to
`build/evidence-archive/` before deleting anything.

## 8. Cleanup plan
| action | estimated | status |
|---|---|---|
| Archive `runs`/`normal-launch` + manifests of old candidates to `build/evidence-archive/` (-3.1 GB kept) | - | **not done** |
| Remove 52 Generals candidates except `x64-34-smooth-presentation` and `x64-35-baseline` | ~81 GB | **not done (see below)** |
| Remove 12 ZeroHour candidates, keep `x64-20-fix1` | ~32 GB | **not done** |
| After the baseline: remove `x64-34-smooth-presentation`; drop `x64-33*` etc. | ~1.6 GB | later |
| `build/dev-runtimes/zh` Win32 stage runtimes (review; intermediates such as `*-before-*`, `workflow-check-*`, `*-pre-*` first) | up to 66 GB | **review with owner** |
| Crash dumps (archived) | 3 GB | optional, on request |

Estimated size now 199.7 GB; ~113 GB reclaimable immediately (candidates), up to ~66 GB more from `zh`, leaving ~20-85 GB.

### Why nothing was deleted
The archive-then-delete step was attempted as one scripted operation; the environment's permission layer denied the
destructive step, and the follow-up read-only checks were denied too. The tree was verified unchanged afterwards
(all candidate manifests are still present). No cleanup was performed; nothing was worked around. The deletion is
scoped as above (candidate directories only, never `x64-35-baseline`, source, Git or Steam) and is ready to run
once you approve the permission for it, or you can run the equivalent yourself:
keep `x64-34-smooth-presentation` and `x64-35-baseline`, copy each other candidate's `runs`, `normal-launch`,
`manifest.json`, `ordinaryplay.json` to `build\evidence-archive\<title>\<name>\`, then delete the candidate folder.

### Staging architecture note (future, not changed here)
Each candidate duplicates ~1.5 GB of immutable assets. A shared read-only asset directory (junction/symlink or
hardlinks of the `.big` files into candidates) would cut a candidate to ~50 MB (exe, pdb, INI/WND overrides). It must keep
candidate isolation and must never point into the Steam folder for writing.


---

# 9. COMPLETE BASELINE (A, B, C) - analysis update

Raw data: `C:\perf\x64-35-baseline\{A1..A3,B1,B2,B3,B4,C1..C3}` (receipts, categories, frames, slow). Same candidate
(`x64-35-baseline`, `generalsv.exe` SHA256 `69AA86EF...6869C`), same machine, same render context
(`Intel Graphics 0x8086:0x7D45, FL 11_0, 1920x1200, vsync 0`), Smooth OFF and cap 120 in **all ten runs**
(`ground_interpolation=0`, `requested_fps=120`). Section 1's caveats (capped; Smooth off) apply unchanged.

## 9.1 Capture validation
| run | verdict | notes |
|---|---|---|
| A1 A2 A3 | valid | 60.0 s, 1799-1800 ticks, identical counters (A3 has 5 fewer 2D draws per frame; cause not investigated) |
| B1 B2 B4 | valid (accepted MEDIUM) | 60.0 s, 1799-1800 ticks, objects 602-608 (+48 units vs A: no `O`) |
| B3 | **excluded** (anomalous) | see 9.5; preserved as evidence |
| C1 C2 C3 | valid (HEAVY) | 60.0 s, 1800 ticks, objects 648-652 (+~96 over A; F10 adds 96 human units, O the enemy army, with simulation-time deaths/spawns in between), 0 catch-up frames |
Hardware and render-context lines are byte-identical in all ten receipts. No capture hit the 16384-frame cap.
Poor performance was not used as a rejection criterion.

## 9.2 Per-scenario results (median [min..max] of the three accepted runs)
| metric | A LIGHT | B MEDIUM (B1,B2,B4) | C HEAVY |
|---|---|---|---|
| fps (capped 120) | 119.65 [119.58..119.81] | 119.77 [119.62..119.79] | 119.75 [119.70..119.75] |
| **work ms/frame** (outer - pacer wait) | 2.77 [2.15..2.90] | 2.94 [2.67..3.88] | **4.36** [3.91..4.37] |
| work p99 ms | 4.44 | 5.67 [4.32..7.67] | 7.20 [7.04..7.88] |
| logic ms per tick (avg) | 0.38 [0.28..0.41] | 0.66 [0.44..0.74] | 1.09 [1.08..1.17] |
| logic p99 ms | 0.66 [0.47..1.40] | 1.75 [1.00..2.02] | 3.09 [1.95..3.92] |
| max logic ms | 38.0 [21.9..43.5] | 24.7 [23.5..84.8] | 23.0 [22.2..23.1] |
| draw calls avg (peak) | 160 (160) | 196 [168..266] (378) | 372 [362..409] (696) |
| world draws / 2D draws | 124 / 36 | 141-239 / 23-28 | 344 / 21-29 |
| triangles avg (peak) | 74.3k (74.4k) | 85.8k [85.2k..89.8k] (127.7k) | 110.2k [104.2k..115.5k] (171k) |
| drawables drawn | 8 | 45 [45..63] | 78 [76..86] |
| objects (gauge) | 553 | 602 | 650 |
| particles avg / peak | 11 / 14 | 101 [16..405] / 257 | 203 [118..237] / 700 |
| state_requested / frame | 2946 | 3766 [3173..5132] | 7246 [7090..8063] |
| state_changed / frame | 170 | 226 [217..269] | 359 [340..361] |
| changed / requested | 5.8 % | 5.8 % [5.2..7.1] | 5.0 % [4.4..5.0] |
| state values requested per draw | 18.4 | 19.2 | 19.6 |
| texture binds / frame | 57 | 91 [91..124] | 171 [164..179] |
| shader changes / frame | 22 | 18 [13..18] | 12 [9..18] |
| render-target changes / frame | 2.0 | 2.0 | 2.0 |
| buffer changes / frame | 40 | 41 | 42 |
| textures created / frame | 1.35 | 1.36 [1.35..3.34] | 1.35 [1.35..1.52] |
Coefficients of variation between the three accepted runs: A < 2 % for counters (work time 12 %); C 3-6 %;
**B counters 14-20 %** because the combat intensity inside the window depends on manual timing (particles 16/101/405).
Treat B as a "variable workload" scenario; C is the stable heavy comparison.

## 9.3 Where the time goes (mean ms per active frame, medians)
* Pacer wait is 4.0-6.2 ms of every 8.33 ms frame (48-74 %): the engine could run roughly 1000/work = 230-360 fps on
  the CPU side if nothing else limited it. **This is not a measured uncapped fps.**
* `game_client_update` is 86-90 % of work in every scenario; `display_draw` is 2.2-2.5 ms (A), 2.2-3.2 (B), 3.3-3.7 (C).
  Logic is small: a tick costs 0.4-1.2 ms and ticks occur on 1 of 4 frames, i.e. ~0.1-0.3 ms amortized per frame.
* Simple fit across A->C: work grows ~1.6 ms for +212 draws = **~7.5 us per extra draw**, leaving a **fixed ~1.6 ms
  per frame (~55 % of A's work)** that does not scale with the object count. `display_draw` contains it; the existing
  categories cannot split it into submission vs. flush/Present. `scene_view_draw`, `render_end_flush_present` and
  `terrain_visual_update` read 0 in the native path (not instrumented there). **Caveat: if the fixed part is Present
  blocking on the iGPU, display_draw is partly GPU wait and CPU-side savings would be smaller than they look.**
* Path search categories rise only in B4/C1/C2 (0.8 ms mean per tick in C1): pathfinding is the second-largest logic part.

## 9.4 Scaling A -> B -> C
| quantity | A -> B | B -> C | A -> C |
|---|---|---|---|
| work ms/frame | +6 % | +48 % | +57 % |
| draw calls | +23 % | +90 % | +133 % |
| world draws | +14..+93 % | | +177 % |
| triangles | +15 % | +28 % | +48 % |
| state_requested | +28 % | +92 % | +146 % |
| state_changed | +33 % | +59 % | +111 % |
| texture binds | +59 % | +88 % | +200 % |
| logic ms/tick | +74 % | +65 % | +187 % |
| drawables drawn | x5.6 | +73 % | x9.7 |
| objects | +9 % | +8 % | +17 % |
| pacer wait | -3 % | -26 % | -29 % |
Observations: (1) state requests scale *with draw calls* (~19 per draw), triangles scale far slower than draws (the
heavy scene is more draws of small geometry, not more geometry); (2) texture binds grow faster than draws (x3 for
x2.3 draws); (3) shader changes *fall* with load (units use fewer distinct shaders than terrain/UI); (4) render
targets/buffers are constant (2 and ~41): not a lever; (5) simulation cost grows ~3x for +17 % objects, driven by
unit AI/pathing (`ai_object_base_update` 0.02 -> 0.55 ms, `path_request_entries` 0.06 -> 0.2-0.9 ms), still small in absolute terms.

## 9.5 B3 anomaly (classification: **unexplained; environmental interference likely**)
Timeline (B3 per second): seconds 0-45 are indistinguishable from B1/B2 (120 fps, 30 ticks/s, work ~3.4 ms,
logic ~20 ms/s). From **t = 46.4 s** work/frame jumps to 4.9 ms, and from t = 47 s the logic update itself stalls:
85 ms (47.1 s), 1,276 ms (49.4 s), 1,682 ms (53 s), 1,129 ms and 1,297 ms (57-58 s), 1,744 ms (end); 66 logic spikes >10 ms
(15 over 100 ms) and ticks fall to 5-16 per second. During this period **objects (606), particles (46-51), draw calls
(~186) and queued path counters are flat or tiny**, and the stalls are 100 % exclusive time inside `game_logic_update`
(exclusive 1,743 of 1,743 ms in the worst frame) with rendering ~normal in the frames in between. Not pathfinding (no queued
paths, no cells), not workload, not object growth, not render. The engine could not attribute it further; it behaves like
the whole process being descheduled/paged (the candidate's stalls sit inside whichever code runs: first logic). B3 ran
at 17:28Z on 2026-10-15 **when C: had 9 GB free of 476 GB (99 % full)**; B4 and C1-C3 ran after the disk cleanup
(116.8 GB free) and none showed any recurrence. That is circumstantial support for environmental cause
(pagefile/paging pressure, Defender, thermal), not proof. Stays on the watch list: if a stall of this kind appears again
on a healthy machine it is a real stability issue (candidates: unbounded allocation in logic, an I/O call in the
tick). Cheap future instrumentation: record process page-fault counts and working set once per second in the receipt.

## 9.6 State churn
`state_requested` is 18.4-19.7 per draw in every run; `state_changed` is 4.4-7.1 % of requests. In C that is ~6,900
redundant API calls per frame, ~830,000 per second at 120 fps, ~0.9 million state calls per second to the driver.
The ratio is the most stable number of the baseline and is independent of the scenario, so the redundancy is
structural (full re-issue per draw), not workload dependent. It still supports a bounded D3D11 state cache. The
expected benefit is a function of an unknown: the CPU cost per redundant D3D11 call on this Intel driver. At 50-150
ns/call, C would save 0.35-1.0 ms of 4.4 ms work (8-23 %); at 20 ns it is 0.14 ms. It is a CPU-side benefit only, so it
matters mainly if the heavy scene turns out CPU-bound (see 9.9). It does not reduce draws, triangles or GPU time.
Scope: `Render2D` submit + world submit in `D3D11Backend.cpp`. Risks: stale state after ClearState/resize/device
reset, other code (legacy DX8 compatibility layer, shadow/overlay passes) changing the pipeline between draws,
and SRV/sampler hazards (a resource bound as SRV then render target). Validation: unit test with a mock context that counts
calls and a shadow-state compare against the unoptimized path, pixel-identical readback tests (existing d3d11 UI/world
tests), `state_requested`/`changed` unchanged by construction, `display_draw` and work ms down on identical C1-C3.

## 9.7 Texture creation
Not a workload effect: **1.35 textures per frame in A, B and C alike** (corr. with 2D draws, world draws, particles,
drawables ~ 0.0). Structure (A1, 7,180 frames): 1 texture on 5,235 frames, 2 on 1,761 (= the logic-tick frames: 1,800
ticks), and **a burst of 6-7 textures every 0.5 s** (120 bursts / 60 s). B3's lower frame count gives the same per-frame
rate (1.37), confirming it is per-frame, not per-second. Larger extras exist only in B2 (14,300 extra: 396 frames of 27
and 137 of 28 textures, corr. 0.84 with 2D draw count, i.e. a UI-driven event such as selection/command-bar text) and
C2 (2,400). There is no matching destruction counter, so a leak cannot be ruled out. A texture created every frame
during ordinary gameplay with nothing visually changing is **suspicious** (UI text strings, radar/minimap, shroud,
water/cloud animation or a per-frame readback copy would be candidates); the call site is unknown. The next step is
attribution, not optimization: tag the creation counter by purpose (UI sentence, mip/atlas, dynamic) and add a
destruction counter plus live residency gauge.

## 9.8 Logic spikes
Every run, including idle A, has 3-10 logic ticks above 10 ms (A 7/7/10, B 3/6/7, C 3/6/6) and 0-2 above one tick
period (A1 38 ms, A3 43 ms, B1 85 ms), none above 25 ms in C. They are **not workload-correlated** (A has more,
C has the cleanest p99 apart from the tail). In C2/C3 they occur at **identical game times** (6.3, 8.3, 14.4, 34.3,
54.3 s vs 6.5, 8.6, 14.6, 34.5, 54.6 s) with a nearly constant cost of 21-23 ms: a deterministic sim-time event,
not noise. B3 is a **different failure mode**: durations are 50-1,700 ms, non-periodic, grow late in the run and the
rest of the frame is unaffected. A 22-43 ms spike shows as one visible frame hitch at 120 fps (the frame max in A/B/C
is 24-46 ms, B1 90 ms), so it matters for the Smooth-presentation quality goal even though it is invisible in averages.
Cause unknown; the categories do not isolate it (`game_logic_update` exclusive time).

## 9.9 Ranked hypotheses (nothing implemented)
| rank | hypothesis | evidence | expected impact | impl. risk | preservation risk | benchmark cleanliness |
|---|---|---|---|---|---|---|
| 1 | **H1 bounded D3D11 state cache** | 19 requests/draw, 95 % redundant, stable | 0.1-1.0 ms of 4.4 ms; only if CPU-bound | medium | none for simulation; render correctness | excellent: counters unchanged, direct before/after |
| 2 | **H4 attribute and remove the deterministic 21-43 ms logic spikes** | identical times in C2/C3 | removes visible hitches; no throughput change | low to find, unknown to fix | simulation determinism is the contract: any change must be CRC-neutral | good: spikes/run, p99 |
| 3 | **H2 per-frame texture creation** | 1.35/frame constant, bursts every 0.5 s | allocation/driver cost + possible leak; magnitude unknown | low (attribute first) | UI output must be identical | good |
| 4 | **H3 fixed ~1.6 ms in display_draw** | linear fit A->C | up to 35 % of A's work if CPU, 0 if GPU wait | unknown until split | n/a | needs Present/submit split first |
| 5 | H5 sort/batch world draws by material (texture binds 57 -> 171) | binds grow x3 | subsumed partly by H1 | high | ordering/transparency correctness | medium |
| 6 | H6 B3-like stalls | 1 of 10 runs | stability | n/a | n/a | needs recurrence |
Per-hypothesis detail (observation/hypothesis/experiment/effect/risk/validation/keep-revert):
* **H1** - Observation: 2,946-8,063 requests vs 170-361 changes per frame. Hypothesis: redundant D3D11 calls cost
  measurable CPU. Experiment: shadow cache of the last bound objects in `Impl` for the ~20 slots `Note()` already tracks;
  skip a call only when the pointer/parameters are identical; invalidate on `ClearState`, resize, Present, device
  loss and `Invalidate_Cached_Render_States`. Effect: `display_draw` ms and work ms down; draw/triangle/pixels identical.
  Risk: stale state. Automated: mock-context call counting + readback equality + existing D3D11 tests. Native: C1-C3
  repeat (same settings); keep if median work ms improves >= 0.2 ms (or >= 5 %) with no visual diff; revert otherwise.
* **H4** - Observation: ~22 ms ticks at identical sim times. Experiment: enable the existing deep path/AI profile for the
  spike frames and add per-module top-N exclusive time to the slow report; no behaviour change. Effect: cause named.
  Risk: none. Keep if spike time attributed and removable without changing CRC.
* **H2** - Experiment: counters by creation purpose + destruction/live gauge. Keep/revert: keep fix if creations/frame
  falls and screenshots/readback match.
* **H3** - Experiment: QPC around `Present`/swap, around world submit, around 2D submit (CPU only, no GPU queries).
  Effect: tells whether the 1.6 ms is CPU submission or GPU wait. Prerequisite for ranking 1 vs 4.

## 9.10 Uncapped capture decision
**Yes - one run is worth doing before any optimization**, and it should be done with a build that also splits
`display_draw`. The capped baseline cannot answer: is the heavy scene CPU-bound or GPU/Present-bound, and what is
the real throughput headroom? Prediction from the data: CPU-bound would give ~1000/4.4 = ~225 fps for C; a result far below
that points to the iGPU/present as the limiter and would demote H1.
* Preferred (recommended): the next candidate (`x64-35-baseline2`: no optimization, only the Present/submit split,
  texture-creation attribution/live gauge, page-fault count in the receipt) is staged, then **1 run: C uncapped**
  (optionally a 2nd identical run to see variance). Not part of A/B/C statistics.
* If you want the answer now with the current candidate (fps only, no split): launch as before, then in-game
  `Options > Display > Frame Rate` = Uncapped, Apply; everything else unchanged (Smooth OFF as in the baseline, VSync off,
  AC power, Best performance), press Ctrl+Alt+Shift F10, O, B, wait 5 s, F11, 60 s, output folder `CU1`. If the option
  is not changeable once the match started, tell me: the developer scenario forces 120 and I will add a dev-only
  `-devFps` override to the next candidate.

## 9.11 Disk cleanup result
Repository 199.7 GB -> **90.6 GB**; C: free 9.0 GB -> **116.8 GB**. 62 superseded native candidates removed
(50 Generals, 12 Zero Hour, 112.1 GB), 3,062 MB of run evidence archived in `build/evidence-archive/`
(429 files, verified). Kept: x64-35-baseline, x64-34-smooth-presentation, ZeroHour x64-20-fix1, `dev-runtimes/zh`,
`build/performance`, userdata-backups, all A/B/C evidence.

## 9.12 Follow-up
The uncapped run decision (9.10) was approved and implemented as candidate `x64-35-baseline2`; see `X64_35_PERFORMANCE_OPTIMIZATION.md` section 15 for the added measurements, limitations and the exact run.
