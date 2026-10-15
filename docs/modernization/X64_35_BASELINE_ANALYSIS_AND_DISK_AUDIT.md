# X64.35 baseline analysis (A, B) and disk-usage audit

Source of truth: `C:\perf\x64-35-baseline\{A1,A2,A3,B1,B2,B3}` (receipts + category CSVs read directly).
Candidate: `x64-35-baseline` (`generalsv.exe` built from `b320513d0`). Machine: ThinkPad L16 Gen 1, Core Ultra 5 125U
(14 logical), 15.9 GB, Intel Graphics (feature level 11_0, 128 MB dedicated / 9 GB shared), client 1920x1200, VSync off.
Status: **baseline incomplete (C1-C3 missing, B3 anomalous). No optimization performed.**

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
