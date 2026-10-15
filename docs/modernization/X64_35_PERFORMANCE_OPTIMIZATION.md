# X64.35 Performance profiling + evidence-driven optimization

Baseline commit: `96766b2d9` (X64.34 accepted). Branch `dev/modern-engine`.
Status: **ACCEPTED AND CLOSED (2026-10-16)**: performance instrumentation + exact decal-geometry reuse kept; see section 27.
(Earlier status lines in this document describe the milestone while it was in progress.)

Rule for this milestone: MEASURE -> IDENTIFY BOTTLENECK -> CHANGE -> MEASURE AGAIN -> COMPARE -> KEEP OR REVERT.

## 1. Accepted baseline
X64.34 (Smooth presentation, Generals Performance Receipt) accepted 2026-10-15. 30 TPS simulation, Classic path and
determinism contracts unchanged by this delivery.

## 2. Reference hardware (recorded context)
The native test machine is a **laptop**: Lenovo ThinkPad L16 Gen 1, Intel Core Ultra 5 125U, 16 GB RAM, integrated
graphics. It is the current reference/baseline machine; it is not the owner's desktop. Consequences:
* No assumption of RTX-class GPU or high-end CPU. Results are low-spec scalability data.
* Optimizations must be architecture-driven (less work submitted, fewer state changes, less per-frame allocation), never
  specialised to this laptop.
* Every receipt now carries a `hardware:` line (CPU brand, logical cores, RAM) and a `render_context:` line (adapter,
  vendor/device ids, feature level, dedicated/shared VRAM, client size, VSync) so receipts from other machines are not
  compared as if identical.

## 3. Instrumentation audit (what existed, what was missing)
| Need | Before | Now |
|---|---|---|
| Frame time avg/p95/p99/max, logic ms, ticks, catch-up | Receipt (X64.34) | unchanged |
| CPU phases (logic, drawable update, terrain, particles, display draw, scene view, render end, ...) | `*-categories.csv` | unchanged |
| Draw calls, textures/buffers created, drawables drawn, objects, particles | Receipt | unchanged |
| 2D vs world vs water draws, triangles | missing | **added** (`draws_2d`, `draws_world`, `draws_water`, `triangles`) |
| State calls requested vs actually changed | missing | **added** (`state_requested`, `state_changed`) |
| Texture binds, shader changes, render-target changes, vertex/index buffer changes | missing | **added** |
| Hardware / adapter identification | missing | **added** |
| Visibility total/visible/culled, shadow passes, particle batches | missing | **deliberately not added yet** (needs invasive hooks in shared Generals/ZH draw code; add only if the baseline points there) |
| GPU timestamp queries | missing | **deliberately not added** (not required for a trustworthy CPU/submission baseline; adding queries risks stalls; revisit only if CPU-side data shows the frame is not CPU-bound) |

### How the new counters are measured
`D3D11Backend` re-issues the full pipeline state on every draw. A small tracker (`Impl::Note`) compares each state
value (render target, depth view, blend, depth-stencil, rasterizer, input layout, VB, IB, VS, PS, constant buffers,
4 SRVs, 4 samplers) with the last value issued. `state_requested` is the number of state values set; `state_changed`
the number that actually differ. **Behavior is untouched**: every D3D11 call is still made. The per-draw cost of the
tracker is ~20 pointer compares; counters are cumulative and are sampled as per-frame deltas only while a capture runs
(the first sample of a capture only primes the baseline, so no whole-session spike appears).
`triangles` = index_count/3 of issued triangle-list draws.
The ratio `state_changed / state_requested` is the evidence for or against a state-cache/sort optimization.

## 4. Baseline methodology
Fixed for all runs: same `generalsv.exe` (the baseline candidate), same map (Twilight Flame), same seed
(Quick Game default 0x4b1001), same resolution, **Smooth ON**, same FPS setting, same VSync, no other heavy
application, laptop on AC power with the "Best performance" power mode. Record the Windows power mode and
whether the laptop was plugged in. Each scenario: 60 s capture, **3 repeats**; compare medians.
Use FPS = uncapped, VSync off for the throughput baseline (shows CPU/renderer headroom). Optional second pass at the
capped 60 FPS setting for pacing.

Scenarios (Developer Mode, hold **Ctrl+Alt+Shift** for every key):
* **A LIGHT** - no spawns, early game, camera at the base. Capture 60 s.
* **B MEDIUM** - after load press **F12** (48 human units), then **B** (attack-move). Wait 5 s for the engagement, capture 60 s.
* **C HEAVY** - after load press **F10** (96 human) then **O** (96 enemy) then **B**. Wait 5 s, capture 60 s.

The spawn/battle sequence is deterministic given the same seed and sequence; exact unit positions can still vary with
timing of the key presses, so the receipt's object/particle counts are the control for "same workload". Replay-based
repeatability was investigated: the engine's replay/CRC path records only network/command streams of a played game and
does not yet carry the developer spawn actions, so it is not used as the benchmark vehicle in this delivery.

## 5. Capture instructions for the human (minimal)
1. Create an output folder, e.g. `C:\perf\x64-35-baseline\A1`.
2. From the candidate folder (`build\dev-runtimes\native\Generals\x64-35-baseline`) run:
```
generalsv.exe -devMode -quickGame -performanceProfile C:\perf\x64-35-baseline\A1
```
   (add `-devPreset battle` only for the HEAVY scenario if you prefer the automatic 96 v 96 setup.)
3. In-game Options > Gameplay set **Unit Motion = Smooth**. Do not change settings between runs.
4. When the match is playing, create the scenario (see section 4), then press **Ctrl+Alt+Shift+F11** to start the capture.
   It stops automatically at 60 s (or press the same keys again). Exit through the game menu.
5. Repeat for A1..A3, B1..B3, C1..C3 (a fresh launch per repeat, new output folder each).
6. Send, per run: `*-summary.txt` (contains the **Generals Performance Receipt**, hardware, render_context),
   and the `*-categories.csv`. Add power mode / plugged-in note.

## 6. Native baseline results
Complete: A1-A3, B1/B2/B4, C1-C3 accepted; B3 excluded (anomalous). Full tables, scaling, B3/spike/texture/state analysis
and the ranked hypotheses are in `X64_35_BASELINE_ANALYSIS_AND_DISK_AUDIT.md` section 9. Headline (medians, Intel
Core Ultra 5 125U laptop, 1920x1200, capped 120 fps, Smooth OFF): work per frame 2.8 / 2.9 / 4.4 ms (A/B/C), draw calls
160 / 196 / 372, triangles 74k / 86k / 110k, state values requested per frame 2,946 / 3,766 / 7,246 of which only
5-6 % actually change, textures created 1.35 per frame in all scenarios, pacer wait 48-74 % of each frame.
Ranked hypotheses (analysis doc 9.9): H1 bounded D3D11 state cache, H4 deterministic 21-43 ms logic spikes,
H2 per-frame texture creation, H3 split display_draw into submit vs Present.

## 7. Bottlenecks / 8. Hypotheses / 9. Optimizations / 10. Before-after / 11. Determinism evidence
_Pending baseline. Hypothesis to test first (not acted on): the per-draw full-state re-issue in `D3D11Backend`
(Render2D and world submit) - measured by `state_changed / state_requested` and `texture_binds` vs `draws_*`._

## 12. Test matrix (this delivery)
`PerformanceReceipt.*` (existing + new renderer/hardware receipt test), `PerformanceProfiler.*`,
`PresentationSmoothness.*`, full Win64/Win32 build + ctest, Python contracts, PowerShell workflows.

## 13. Remaining / later candidates
State caching and draw sorting by texture/shader, constant-buffer update batching, culling/shadow counters,
GPU timestamp queries, allocation counters - all evidence first.

## 14. Candidate
`build/dev-runtimes/native/Generals/x64-35-baseline` (built from `b320513d0`, staged and preflighted, not run by the agent).
* `game/generalsv.exe` SHA256 `69AA86EF1648932B5B71F327BE68D3A198C0C5BAF30319C5D8412E8CD5D6869C`
* `game/generalsv.pdb` SHA256 `F968F22F1363AE4E405283E1A38FF61D63CC61BBFA0A986D8A705E88FA8353C0`
Steam untouched.


---

# 15. x64-35-baseline2 - diagnostic candidate (instrumentation only)

Why it exists: the capped A/B/C baseline (see `X64_35_BASELINE_ANALYSIS_AND_DISK_AUDIT.md` section 9) cannot say whether
the Heavy scene is CPU-submission bound or Present/GPU bound, nor where the ~1.35 textures/frame come from. This
candidate adds the measurements needed to answer both with ONE uncapped Heavy run. **No optimization was made.**
Built from `0ae7d608f` (starting point `f472d4d63`).

## 15.1 New measurements (receipt fields)
All earlier receipt fields are unchanged and still present. New lines in `*-summary.txt` after `render_context:`:

```
fps_cap_override: none | uncapped (-devFps) | <n> (-devFps)
timing_ms_per_frame (CPU wall, inside backend): submit <ms> present <ms> texture_upload <ms> peak_present <ms> peak_submit <ms>
gpu: frames_measured <n> avg_gpu_ms <ms>
textures: created <n> destroyed <n> live_end <n> dynamic_updates <n> update_kb <n>
memory: page_faults <n> peak_faults_per_frame <n> working_set_mb_end <n> private_mb_end <n>
```
and, after `=== end receipt ===`, a `=== Texture creation attribution (this capture) ===` section: totals per entry point
(content_file / dynamic_ui / mipped) and the top 30 rows `entry WxH dxgi_format created kb label` where `label` is the
engine texture path when the creation came through the texture bridge. `*-frames.csv` gains the per-frame columns
`present_qpc, submit_qpc, upload_qpc, gpu_us, gpu_frames, textures_destroyed, texture_updates, texture_update_kb,
textures_live_gauge, page_faults, working_set_mb_gauge, private_mb_gauge` (`*_qpc` are QueryPerformanceCounter ticks;
divide by `qpc_frequency` in the summary header).

## 15.2 What each measurement is, and what it cannot prove
| field | measures | cannot prove |
|---|---|---|
| `submit` | CPU wall time inside `Submit_Render2D_Content` and `Submit_World` (state sets, constant-buffer updates, `DrawIndexed`) | time the driver spends later on the GPU queue; includes preemption by the OS |
| `present` | CPU wall time inside `IDXGISwapChain::Present(0,0)` | does not separate GPU wait from driver/DWM work. A large value with VSync off means the CPU was blocked (queue backpressure) - consistent with GPU/present pressure, not proof of it |
| `texture_upload` | CPU wall time of texture creation + dynamic texture updates | GPU-side upload cost |
| `avg_gpu_ms` | elapsed GPU time between the first command of a frame and its end (D3D11 timestamp + disjoint queries, ring of 4, polled with `DONOTFLUSH`, never waited on); frames whose results are not yet available are collected later | GPU *busy* time: gaps while the CPU is still submitting are inside the span. If the frame is CPU-bound the span approaches CPU submit time; if GPU-bound it approaches the frame interval. Disjoint frames are discarded and counted |
| `display_draw` (existing category) minus `submit` minus `present` minus `texture_upload` | remaining CPU work in the draw path (scene traversal, culling, W3D object preparation) | |
| `page_faults` | process page-fault counter delta (`PROCESS_MEMORY_COUNTERS_EX.PageFaultCount`: soft + hard faults) | hard faults/disk I/O specifically; it cannot show that disk pressure caused anything |
| `textures destroyed/live` | residency objects created/destroyed in the backend (texture + SRV) | where in the engine the owning texture object lives |

Overhead: two QueryPerformanceCounter reads per submitted draw and per upload/Present, a map update per texture
creation, one GetProcessMemoryInfo per frame, and 3 GPU timestamp queries per frame. GPU timing and page-fault sampling
are active only inside a running capture; the QPC reads and texture table are always on and negligible. No blocking
synchronization is added. Rendering output and D3D11 call arguments are unchanged.

## 15.3 Frame-rate override (`-devFps`)
`Options > Display > Frame Rate = Uncapped` was **not** used: it works only after the match is running, persists the
user setting (the accept path writes `Options.ini`) and the Quick Game forces 120 at start. The smallest dev-only
alternative was added: `-devFps <15..1000|uncapped>`, honoured only by the interactive Quick Game
(`-devMode -quickGame`), runtime-only (never persisted), default unchanged (120), simulation stays at 30 ticks/s,
Smooth untouched. The receipt prints `fps_cap_override:` and `initial_requested_fps` (1000000 = uncapped).
A capture holds at most 16,384 frames: above ~273 fps the 60 s capture ends early at that frame count; the receipt
`duration_s` / `avg_fps` stay valid, so do not restart it.

## 15.4 Exact uncapped Heavy run (ONE run, separate from the A/B/C statistics)
Laptop plugged in, "Best performance" power mode, same resolution and options as the baseline (Smooth OFF, VSync off).
1. `mkdir C:\perf\x64-35-uncapped\C-U1`
2. From `build\dev-runtimes\native\Generals\x64-35-baseline2\game`:
```
generalsv.exe -devMode -quickGame -devFps uncapped -performanceProfile C:\perf\x64-35-uncapped\C-U1
```
3. Wait for the match, then hold **Ctrl+Alt+Shift** and press: **F10**, **O**, **B**; wait 5 seconds; **F11**.
4. Let the capture finish (60 s, or earlier if the frame cap is reached); exit via the game menu.
5. Send everything in `C:\perf\x64-35-uncapped\C-U1` (`*-summary.txt` is the key file).
Optional only if the first run looks odd: a second identical run `C-U2`.

## 15.5 How the result decides the next step
* `avg_fps` far above 120, `present` small and `avg_gpu_ms` well below the frame time: CPU-bound. H1 (state cache) and
  other CPU-side work are justified; `submit` is the quantity they must reduce.
* `present` large (or `avg_gpu_ms` close to the frame interval) with `submit` small: GPU/Present-bound. CPU-side state
  caching will not raise throughput; triangles/overdraw/resolution-side work would be the lever.
* `textures: created` split by label/size tells which engine textures are created every frame; `destroyed` and
  `live_end` distinguish transient churn from a leak.
* `page_faults` per frame and peak identify memory-pressure stalls (compare B3-style behavior).

## 15.6 Candidate
`build/dev-runtimes/native/Generals/x64-35-baseline2` (staged and preflighted, not run by the agent):
* `game/generalsv.exe` SHA256 `94626379991B99A4F9BA2DA009F6F0E111A7ED6FEC1FA766E40A62200F468239`
* `game/generalsv.pdb` SHA256 `A4E5FD6C7AE537791147A20159BBA52157A301FD467857F3CCD1E79527DE015`
Steam untouched. X64.35 is NOT accepted.


---

# 16. C-U1 result (uncapped Heavy, x64-35-baseline2) - summary

Full analysis: `X64_35_BASELINE_ANALYSIS_AND_DISK_AUDIT.md` section 10. Recorded separately from the capped A/B/C baseline.
* 271 fps / 3.69 ms per frame uncapped (60 s, 16,271 frames, 1,800 ticks). Render scene equal to C1-C3; simulation and
  particles much lighter (no combat): comparable for render, not for logic.
* **CPU limited; the CPU is not in D3D11 submission (0.154 ms, 4 %) nor Present (0.17 ms, 5 %; 0.06 ms for the first
  34 s) but in ~2.97 ms (80 %) of engine-side render preparation inside `display_draw` that has no active profiler
  category.** GPU timestamp span 1.9-2.5 ms (a span, not busy time) is shorter than the CPU frame.
* H1 state cache re-classified **C**: upper bound ~0.15 ms (4 %), realistic 1.5-3 %.
* Texture creation: churn (18,790 created = destroyed, live 54), 64x64 per frame and per tick + 6 every 0.5 s from the
  Developer Mode overlay; not a leak; unlabeled text content.
* Logic spikes: 20-30 ms single `path_queue` calls (likely `calculateZones`), deterministic, in every run.
* Page faults: 84 % of the capture's faults are two render frames at ~35 s (+26 MB), coinciding with an unexplained
  Present/GPU-span step (0.06 -> 0.35 ms; 1.9 -> 2.55 ms) at constant workload.
* **Recommended first batch: attribution scopes for the unclassified display_draw time + `calculateZones` scope +
  sentence-texture labels (no behavior change), then ONE Heavy uncapped run.** No optimization has been made;
  X64.35 is not accepted.


---

# 17. x64-35-attribution - attribute the unclassified display_draw time (measurement only)

Approved after the C-U1 analysis (section 16). Starting point `f10c8a0eb`; implementation `10b4503e9`. **No optimization,
no behavior change.** The D3D11 state cache was deliberately not started (bounded to <= 4 %).

## 17.1 Why the 2.97 ms was invisible
In native play the world is drawn by `W3DDisplay::drawOrdinaryMenuFrame` (W3DDisplayOrdinaryHost.cpp), not by the legacy
loop in `W3DDisplay::draw` that carries the old `SceneView`/`RenderEnd` scopes, so those categories read 0. The native
frame is: `Begin_Render` -> terrain (`HeightMapRenderObjClass::renderNative`) -> world objects
(`NativeWorldObjects::render`: gather, per-object CPU mesh processing/lighting, effects collection, geometry upload,
batch submission) -> world overlays -> `InGameUI::DRAW` (windows, developer overlay) -> mouse -> `End_Render`.

## 17.2 New profiler categories (frames.csv / categories.csv / receipt)
| category | scope | nesting |
|---|---|---|
| `native_begin_render` | `WW3D::Begin_Render` | top level |
| `native_terrain` | whole `renderNative` | top level |
| `native_terrain_overlays` | shoreline / extra blend / roads / bridges (to end of `renderNative`) | in terrain |
| `native_objects` | whole `NativeWorldObjects::render` | top level |
| `native_obj_gather` | drawable/shroud queries and input building | in objects |
| `native_obj_process` | per-object cull + `ProcessObject` (skinning, lighting, vertex build) | in objects |
| `native_fx_ground` / `_shadows` / `_water` / `_shroud` / `_particles` | the five `NativeWorldEffects::collect` families | in objects |
| `native_geom_upload` | `Update_Geometry_Vertices/Indices` for the chunks | in objects |
| `native_submit` | layer sort + `Submit_World` loop | in objects |
| `native_world_overlays` | `W3DView::drawNativeWorldOverlays` (selection rings, icons) | top level |
| `native_ui` | `InGameUI::DRAW` / `winRepaint` | top level |
| `native_dev_overlay` | `DeveloperTools::drawOverlay` (developer-only text) | in native_ui |
| `native_mouse` | cursor | top level |
| `native_end_render` | `WW3D::End_Render` (flush + Present) | top level |
| `zone_calc` | `m_zoneManager.calculateZones` | in `path_queue` (logic) |

The receipt gains a `native_frame_phases` table: for each category **inclusive** and **exclusive** ms per outer frame
averaged over all frames and calls per frame, plus `display_draw_unattributed` = `display_draw` inclusive minus the seven
top-level native phases. Read exclusive time to avoid double counting nested scopes. `zone_calc` calls per frame x 30
ticks tells how often zones are recalculated; `path_queue` exclusive time dropping by the `zone_calc` amount confirms or
rejects the hypothesis for the 20-30 ms spikes (no change to zone behavior, ordering or determinism).

## 17.3 Sentence texture labels
`Render2DSentenceClass` remembers the first 44 printable characters of the text it builds and gives the resulting
`TextureClass` a diagnostic label (`TextureClass::Set_Diagnostic_Label`, never used for lookup). The D3D11 bridge records
the label as `text:<characters>` in the creation table, so the `=== Texture creation attribution ===` rows distinguish
`text:DEV MODE | FPS ...` (the developer overlay) from game HUD strings. Texture creation behavior and lifetime are not
changed.

## 17.4 Overhead and dormancy
Every scope is `PerformanceProfile::Scope`, whose constructor reads one thread-local pointer and does nothing when no
capture is running; the `std::optional<Scope>` wrappers in `NativeWorldObjects` behave the same. No allocation, no
clock read and no D3D11 call happens outside a capture. Inside a capture each scope costs two QPC reads (about 20
scopes per frame, order of microseconds).

## 17.5 Validation
Win64: ctest 16/16, g 239 and z 238 gtests; Win32: ctest 23/23, g 209 and z 209 gtests (shared engine, Generals and Zero
Hour); all Python contracts (the Win32-reference byte-identity contract needed the new include in HeightMap.cpp guarded
by `_WIN64`) and PowerShell workflow tests pass. New tests: `PerformanceReceipt.NativePhaseTableSplitsDisplayDrawWithoutDoubleCounting`
(nesting, exclusive time, unattributed remainder) and the extended instrumentation contract.

## 17.6 Candidate
`build/dev-runtimes/native/Generals/x64-35-attribution` (staged and preflighted, not run by the agent):
* `game/generalsv.exe` SHA256 `996B972ECE88DEBF9F31ECA3532ED326F5D3569719A43086C6826B17B5E75B68`
* `game/generalsv.pdb` SHA256 `BDF59316C6299E86157CDB3B8F10B0D72F0DE8D99BB7E33782FFD6D2DC2C9572`
Steam untouched.

## 17.7 The ONE human run (same procedure as C-U1)
Laptop plugged in, Best performance mode, same resolution/settings, Smooth OFF, VSync off.
1. `mkdir C:\perf\x64-35-attribution\C-A1`
2. From `build\dev-runtimes\native\Generals\x64-35-attribution\game`:
```
generalsv.exe -devMode -quickGame -devFps uncapped -performanceProfile C:\perf\x64-35-attribution\C-A1
```
3. When the match is playing, hold **Ctrl+Alt+Shift** and press **F10**, **O**, **B**; wait 5 seconds; **F11**.
4. Let the 60 s capture finish (the recorder stops at 16,384 frames if the fps is very high), exit through the game menu.
5. Send everything in `C:\perf\x64-35-attribution\C-A1` (`*-summary.txt` first: it now contains `native_frame_phases`).
Keep the run out of the A/B/C statistics. Nothing else is required.


---

# 18. C-A1 result: the capture is NOT an attribution capture (wrong executable); useful as an uncapped repeat

Raw: `C:\perf\x64-35-attribution\C-A1`.

## 18.1 Validation: which build produced it
The summary header says `git=0ae7d608f85bfea3a8fda8a69116a8e5a02c1199`, `build=... Oct 15 2026 23:45:16`, and the
file contains none of the new `native_frame_phases` / `native_*` categories (`scene_view_draw` and
`render_end_flush_present` still read 0, texture rows are still `(untagged)`). That is the `x64-35-baseline2`
executable (SHA256 `94626379...68239`, built from `0ae7d608f`), **not** `x64-35-attribution` (`996B972E...75B68`, built from
`10b4503e9`, 00:20 on Oct 16). The capture was taken in the folder named `x64-35-attribution` only by name: no
attribution data exists in it. **The attribution questions (items 2-8 of the request) cannot be answered from this data
and I am not inferring any.** (Probable cause: the launch command was run from, or resolved to, the baseline2 `game`
folder. A launcher with a hash/commit check was added to prevent a repeat, see 18.4.)

## 18.2 What it is good for: a second uncapped run of the same build
Same machine, same settings, same exe as C-U1, so it measures run-to-run reproducibility:

| metric | C-U1 | C-A1 | difference |
|---|---|---|---|
| frames / duration | 16,271 / 60.00 s | 16,269 / 60.00 s | |
| avg fps / frame ms | 271.17 / 3.686 | 271.14 / 3.686 | 0.01 % |
| p95 / p99 / max frame ms | 4.59 / 5.69 / 24.8 | 4.65 / 5.86 / 23.0 | |
| `display_draw` | 3.380 | 3.383 | 0.1 % |
| backend submit / present / upload | 0.154 / 0.169 / 0.086 | 0.152 / 0.175 / 0.086 | |
| `display_draw` remainder (not submit/present/upload) | 2.970 | 2.969 | 0.03 % |
| GPU timestamp span | 2.18 ms | 2.09 ms | |
| draws / triangles / state requested / changed | 341 / 110.9k / 6,610 / 384 | 341 / 110.9k / 6,612 / 384 | identical |
| objects / drawables / particles | 650 / 121 / 11 | 650 / 121 / 11 | identical |
| textures created = destroyed, live | 18,790 / 54 | 18,788 / 54 | identical |

* The measurement is highly reproducible: **the 2.97 ms unattributed remainder is a stable, real quantity** (not noise),
  and the receipt counters are exact.
* The workload is again a no-combat Heavy scene (11 particles), identical to C-U1.
* Logic spikes (18.4-19.4 ms, all `path_queue`/`ai_global`): C-U1 at 7.2, 9.2, 15.3, 35.2, 55.2 s; C-A1 at 11.2, 13.2,
  19.3, 39.2, 59.2 s - **the same events shifted by exactly +4.0 s** (the capture started 121 ticks, i.e. 4.03 s of game
  time, later: `logic_before` 602 vs 481). They are tied to simulation time: deterministic. Whether they are
  `calculateZones` still needs the ZoneCalc scope (not present in this build).
* The late presentation step recurs but is **not** tied to simulation time: the two 13 MB page-fault frames (3,288 + 3,287
  faults, identical size) occur at 34.9 s / tick 1648 in C-U1 and at 43.7 s / tick 1791 in C-A1; after them `Present`
  rises from 0.05-0.06 ms to 0.3-0.8 ms and the GPU span from 1.9 to 2.5-2.8 ms (fps 280 -> 243-258) in both runs. So it
  is a **recurring event in this build/machine under sustained uncapped load that is not driven by the game
  state** (workload counters are flat): driver/memory/power-state behavior is a candidate, but nothing here identifies
  it. It does not affect the 3.68 ms frame figures above (those are dominated by the first 35-44 s). Still unexplained.

## 18.3 Consequence
Nothing about the ranking or the recommendation changes yet: item 1 (attribute the ~2.97 ms, now confirmed stable) is
still the first step, and `x64-35-attribution` is unchanged and still the right candidate. **One more capture is
genuinely required**, from the correct executable.

## 18.4 Repeat run, with a guard against the wrong executable
```
pwsh scripts\native-runtime\Invoke-NativeCandidateCapture.ps1 -Candidate x64-35-attribution `
     -OutputDirectory C:\perf\x64-35-attribution\C-A2 -ExpectPhases `
     -ExpectedSha256 996B972ECE88DEBF9F31ECA3532ED326F5D3569719A43086C6826B17B5E75B68
```
It prints the executable path and SHA256 before launching, starts `generalsv.exe -devMode -quickGame -devFps uncapped
-performanceProfile <dir>` from that candidate's `game` folder, and afterwards prints the summary's `git=` line and
whether `native_frame_phases` is present (exit code 3 if not). In the game do exactly as before: Ctrl+Alt+Shift **F10**,
**O**, **B**, wait 5 s, **F11**, let the capture finish, exit via the menu. The summary must show
`git=10b4503e9...` and contain `native_frame_phases`. Add `-WhatIf` to see the command without launching.
(Manual alternative: run the exe from `build\dev-runtimes\native\Generals\x64-35-attribution\game`; check `git=` afterwards.)


---

# 19. C-A2 - attribution capture (x64-35-attribution): where the frame time goes

Raw: `C:\perf\x64-35-attribution\C-A2` (wrapper-verified: exe SHA256 `996B972E...75B68`, `git=10b4503e9...`,
`dirty=1` only because the staged docs/scripts were uncommitted at the time of the run; the engine code is that commit).
Recorded separately from the capped A/B/C baseline and from C-U1/C-A1 (different build, instrumentation overhead differs).

## 19.1 Validation and comparability
* 16,384 frames in 57.43 s (the recorder's frame cap, as expected uncapped), 1,724 ticks = 30.0/s, 0 catch-up frames,
  `fps_cap_override: uncapped`, same hardware / render context, Smooth OFF, VSync off.
* Workload equals C-U1/C-A1: objects 650, drawables 121, triangles 110,865, draws 341 (312 world + 29 2D), state
  requested 6,611 / changed 384, texture binds 183, particles 11 (no combat). Against capped C1-C3 it has the same render scene
  (110k triangles, 650 objects) but ~95 % fewer particles and ~62 % lower logic cost.
* Frame 3.50 ms (285 fps) vs 3.69 ms (271 fps) in C-U1/C-A1: the earlier runs include a late Present/GPU step (below);
  the clean first ~44 s of every uncapped run is 3.44-3.56 ms. Scope overhead is not visible (this run is, if anything, faster).

## 19.2 Frame breakdown (ms per frame, averaged over all 16,384 frames; calls/frame = 1 for every phase)
| phase | inclusive | exclusive | % of display_draw (3.235) | % of frame (3.504) |
|---|---|---|---|---|
| `display_draw` | 3.2349 | 0.0162 | 100 | 92.3 |
| `native_begin_render` | 0.0022 | 0.0022 | 0.1 | 0.1 |
| `native_terrain` | 0.1447 | 0.0209 | 4.5 | 4.1 |
| &nbsp;&nbsp; `native_terrain_overlays` | 0.1238 | 0.1238 | 3.8 | 3.5 |
| **`native_objects`** | **2.7259** | 0.0040 | **84.3** | **77.8** |
| &nbsp;&nbsp; `native_obj_gather` | 0.0233 | 0.0233 | 0.7 | 0.7 |
| &nbsp;&nbsp; **`native_obj_process`** (per-object CPU mesh processing) | **0.8616** | 0.8616 | 26.6 | 24.6 |
| &nbsp;&nbsp; `native_fx_ground` | 0.0087 | 0.0087 | 0.3 | 0.2 |
| &nbsp;&nbsp; **`native_fx_shadows`** | **1.0089** | 1.0089 | **31.2** | **28.8** |
| &nbsp;&nbsp; `native_fx_water` | 0.0053 | 0.0053 | 0.2 | 0.2 |
| &nbsp;&nbsp; **`native_fx_shroud`** | **0.5046** | 0.5046 | **15.6** | **14.4** |
| &nbsp;&nbsp; `native_fx_particles` | 0.0150 | 0.0150 | 0.5 | 0.4 |
| &nbsp;&nbsp; `native_geom_upload` | 0.1513 | 0.1513 | 4.7 | 4.3 |
| &nbsp;&nbsp; `native_submit` | 0.1434 | 0.1434 | 4.4 | 4.1 |
| `native_world_overlays` | 0.0658 | 0.0658 | 2.0 | 1.9 |
| `native_ui` | 0.2112 | 0.1971 | 6.5 | 6.0 |
| &nbsp;&nbsp; `native_dev_overlay` | 0.0141 | 0.0141 | 0.4 | 0.4 |
| `native_mouse` | 0.0104 | 0.0104 | 0.3 | 0.3 |
| `native_end_render` (flush + Present) | 0.0585 | 0.0585 | 1.8 | 1.7 |
| **`display_draw_unattributed`** | **0.0162** | | **0.5** | **0.5** |

Backend (inside the above): submit 0.151, present 0.054, texture upload 0.080 ms; GPU timestamp span 2.02 ms.
Remaining frame outside display_draw: 0.27 ms (logic 0.40 ms x 1,724/16,384 = 0.04, drawable client updates 0.08,
display update 0.05, message propagation 0.04 ...).

## 19.3 The former 2.97 ms mystery is resolved
`display_draw` minus backend submit/present/upload was 2.97 ms (C-U1/C-A1); with this build 3.235 - 0.285 = 2.95 ms, of
which **2.934 ms (99.5 %) is now attributed**; 0.016 ms remains.
* **Three phases account for 2.376 ms = 73 % of display_draw / 68 % of the frame:** shadow volumes 1.009, per-object CPU
  mesh processing 0.862, shroud mesh 0.505. All three are CPU work executed *every frame* by the native world
  renderer (`NativeWorldObjects::render` -> `renderObjects` / `NativeWorldEffects::collect`).
* Next: geometry upload 0.151, terrain 0.145, submit 0.143, UI 0.211 (a 0.15-0.2 ms single string-rebuild is inside it,
  see 19.5). Everything else is below 0.07 ms. The D3D11 backend itself (submit + Present) is 0.2 ms: the premise
  of the state-cache hypothesis is now fully quantified at <= 0.15 ms.
* **Clear bottleneck: the per-frame CPU regeneration of derived world geometry (shadow volumes, lit/skinned object
  vertices, shroud) in `NativeWorldObjects`/`NativeWorldEffects`.**
* What it scales with (evidence): the capped baseline A (8 drawables drawn, no units) already had `display_draw`
  1.8-2.5 ms with 553 registered objects, and C (+96 units) only 3.3-3.7 ms. So the cost is dominated by the
  ~550 registered scenery objects (trees, props, buildings; all processed every frame) and grows only ~+1 ms with 100
  units. Combined with the per-frame timings being flat over time (shadows 1.010 / 1.012 / 1.012 ms in the first,
  middle and last 10 s of this static-camera run), the work is largely *identical from frame to frame*: it is
  regenerated, not changed. It does not scale with draw calls, UI or particles in this scene.

## 19.4 ZoneCalc: the path_queue spikes are confirmed
5 `calculateZones` calls in 57 s (one every ~11 s): **19.67, 19.97, 20.12, 20.36, 19.92 ms**. Every `path_queue` spike
equals its `zone_calc` time (`path_queue` exclusive time on those frames = 0.0006-0.001 ms): **100 % of the 20 ms
`path_queue` events are zone recalculation**; 100 ms in total = 14.6 % of all logic time in the capture; two of the five
fall on consecutive ticks (37.6 s, 37.7 s). Cost is constant (~20 ms) and independent of the army. The remaining logic
outliers are small (`ai_player_strategy` 4.1-8.7 ms, 1-2 per run). Each event makes one 23-24 ms frame (the six frames
over 16.7 ms in the run). These are simulation hitches, not renderer throughput: they cost 0.3 % of average frame time
but are the visible frame-time spikes. They are deterministic (same game times in C-U1/C-A1/C2/C3). What triggers each
recalculation (obstacle add/remove: construction, destruction, bridge state) is not yet known; **not modified**.
Any change is a simulation-side change that must be CRC/replay-neutral and is a separate batch.

## 19.5 Sentence textures (labelled)
18,797 created = 18,797 destroyed, live 54. Identified in the labelled table:

| label (string) | creations / 57.4 s | per | source |
|---|---|---|---|
| `$ 10000` | **16,384 (87 %)** | **every rendered frame** | the Control Bar money static text (`GUI:ControlBarMoneyDisplay`, "$ 10000" = starting cash) |
| `.00` ... `.29` | **~1,724 (9 %)**, 57-58 per value | one per logic tick, cycling 0..29 | `InGameUI::drawGameTime`: `m_gameTimeFrameString->setText(L".%2.2d", currentFrame % 30)` (game timer HUD) |
| `DEV MODE | FPS ...` | 445 (2.4 %): 267 x 128x128 + 178 x 64x64 | twice per second | the developer overlay (`DeveloperTools::drawOverlay`, text rebuilt every 0.5 s) |
| other | the remainder of 18,797 | | not in the top-30 rows |
* **The per-frame creation is a normal-game engine behavior, not developer instrumentation**: `W3DStaticText.cpp:107-109`
  calls `text->setUseHotkey(TRUE/FALSE, ...)` on every draw and `W3DDisplayString::setUseHotkey` calls
  `notifyTextChanged()` unconditionally (reset + `Build_Sentence` + a new texture on the next draw) even when nothing
  changed. `DisplayString::setText` already guards equality, `setFont`/`setWordWrap` also; `setUseHotkey` is the
  missing guard. Cost: one 64x64 glyph raster + texture create/destroy per frame (`texture_upload` 0.080 ms plus
  the raster inside `native_ui` 0.197 ms exclusive).
* The `.NN` per tick is legitimate text change (the string changes every tick). The 0.5 s burst is developer-only.
* Conclusion: allocation churn from a missing equality guard; not a leak; not caching of texture lifetimes.

## 19.6 Other observations
* The late Present/GPU step (page-fault pair of 3,287 + 3,286, then Present 0.05 -> 0.1-0.8 ms, GPU span 1.9 -> 2.3-2.8 ms)
  occurred in all three uncapped runs at a different time (34.9 s, 43.7 s, 50.3 s after capture start; game tick
  1648 / 1791 / 2039) and with the same 2 x 13 MB signature. It is not tied to game state or to the F10/O/B moment;
  still unexplained, relevant only to long sustained runs, and is excluded from the clean-phase numbers above.

## 19.7 Re-ranked X64.35 work (all evidence)
| # | item | evidence | expected benefit | risk | notes |
|---|---|---|---|---|---|
| 1 | **Shadow-volume geometry reuse** (`NativeWorldEffects::EmitVolumeShadow`) | 1.009 ms = 29 % of frame, rebuilt every frame from unchanged inputs | 0.4-0.7 ms (11-20 %) | low-medium (render only; exact-by-construction key) | **recommended first batch** |
| 2 | Per-object CPU mesh processing reuse (`ProcessObject`/`ProcessMesh`) | 0.862 ms = 25 %, ~550 mostly static scenery objects | 0.4-0.6 ms | medium (lighting, shroud, team color inputs) | after #1; same mechanism |
| 3 | Shroud mesh reuse (`CollectShroud`) | 0.505 ms = 14 %, static camera/shroud | 0.3-0.45 ms | low-medium (needs a shroud/terrain revision key) | simple key; after #1 |
| 4 | `W3DDisplayString::setUseHotkey` equality guard | 16,384 textures/capture, up to ~0.15 ms | 0.1-0.15 ms (3-4 %), also less driver churn | very low | tiny, exact; can ride with any batch |
| 5 | `calculateZones` hitches | 5 x 20 ms per 57 s, 100 % of path_queue spikes | removes 20 ms frame spikes, no throughput | medium-high (simulation, CRC) | separate simulation batch |
| 6 | Geometry upload + submit (0.15 + 0.14) and D3D11 state cache | <= 0.15 ms state calls | <= 4 % | low | after the above |
| 7 | Terrain overlays rebuild (0.124 ms) | | <= 3.5 % | low | later |
| 8 | Late Present/GPU step, B3 stall | unexplained | stability | n/a | watch list |

## 19.8 Recommended first optimization batch (not implemented)
**"x64-36-shadow-reuse": exact frame-to-frame reuse of shadow-volume geometry for unchanged shadow casters.**

* **Measured bottleneck:** `native_fx_shadows` 1.009 ms/frame (28.8 % of the frame), flat over time and identical across
  the static-camera heavy runs.
* **Code path:** `Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldEffects.cpp`:
  `CollectShadows` -> `EmitVolumeShadow` (+ `GatherShadowGeometry`) -> `NativeWorldObjects::emitEffect`. Native Win64 path
  only; the Win32 DX8 renderer and the simulation are untouched.
* **Proposed implementation:**
  1. Add three free sub-scopes/counters inside the batch itself (gather, project, emit; shadows seen / emitted /
     triangles / cache hits) so the before/after receipt shows exactly what moved.
  2. Per `NativeShadow`, keep a cache entry holding: the gathered world-space positions, the projected vertices, the
     index list, and the inputs they were computed from (light direction, shadow color, planar flag, and the three
     ground-height samples `g0/gx/gy`). Each frame: run `GatherShadowGeometry` (needed to observe the pose), and if the
     positions are **bit-identical** (`memcmp`) to the cached ones and the other inputs are equal, skip the projection loop
     and the triangle/index rebuild and emit the cached arrays; otherwise recompute and overwrite the entry. The key is
     complete by construction, so reuse is exact: output bytes equal what a fresh computation would produce.
  3. Restrict v1 to the planar case (`bounding radius <= 30`: units, trees, props); structures (the non-planar per-vertex
     terrain refinement, which depends on the height field) keep the current path unless a terrain revision key is added
     later. Entries are released when the shadow is removed and the cache is bounded (e.g. 16 MB total, LRU eviction).
  4. Remove the redundant per-shadow `std::vector<unsigned>` -> `unsigned short` conversion by gathering indices
     directly in final form (exact, behavior-neutral).
* **Expected benefit:** 0.4-0.7 ms/frame (11-20 % of the 3.5 ms frame, ~285 -> ~320-355 fps uncapped) on a static
  scene; less in combat (moving/animating casters miss), never a regression beyond one memcmp per shadow. The
  attribution counters will state the hit rate.
* **Risks:** stale geometry if the key misses an input (mitigated: key = all inputs, compared bit-exactly, no
  version counters); memory growth (bounded); pointer reuse after a shadow object is destroyed (entries are
  keyed on the `NativeShadow*` and cleared in its destructor/removal).
* **Preservation / determinism:** render-only; reads object transforms and writes nothing back; no simulation call, no
  RNG, no CRC input. Classic and Smooth presentation produce identical pixels (Smooth changes the *input* poses, which
  are part of the key). Generals and Zero Hour share the file; Win32 reference untouched.
* **Automated validation:** unit test with a synthetic shadow caster: cached output byte-identical to a freshly
  computed one across N frames; any change of position, light, ground height or color forces recomputation; cache
  eviction and shadow removal; extend the existing NativeWorldEffects tests / world readback tests (pixel equality of a
  frame with shadows before vs after); the existing receipt/contract tests; full Win64/Win32 matrix.
* **Native before/after:** the same Heavy uncapped run (`Invoke-NativeCandidateCapture.ps1`, F10, O, B, wait 5 s, F11),
  baseline = C-A2 (`native_fx_shadows` 1.009 ms, frame 3.50 ms, triangles 110,865, draws 341), 1 run, plus an optional
  second to check repeatability (the repeatability of this scene is 0.1 %). Visual check: shadows identical (units
  stationary and moving).
* **Keep / revert criterion:** KEEP if `native_fx_shadows` exclusive drops by >= 0.25 ms (>= 25 %) **and** the frame
  mean improves by >= 5 %, with identical triangles/draw counters (reused geometry is the same size) and no visual
  difference in the review. REVERT if the saving is < 0.15 ms, any visual difference appears, or a stale-shadow case
  is found.
* **Not part of this batch:** per-object processing reuse, shroud reuse, the setUseHotkey guard (a one-line exact
  change that can be approved separately), zone calculation, the state cache, any visual change.


---

# 20. X64.35 optimization 1: shadow-volume geometry reuse (implemented, awaiting native before/after)

Candidate `x64-35-shadow-reuse`; implementation commit `1a37a4c4d`; baseline for comparison: **C-A2**. This remains
X64.35 (not a new milestone). **Not accepted** until the native before/after measurement and the human visual review.
The `setUseHotkey` guard, per-object mesh reuse, shroud reuse, ZoneCalc, the state cache and the Present investigation
are deliberately not part of this batch.

## 20.1 What changed (render-only)
* New header-only `NativeShadowVolumeCache.h` (`Core/GameEngineDevice/Include/W3DDevice/GameClient`):
  `NativeShadowVolume::Cache`, plus `ProjectPlanar()` = the former inline planar projection loop with unchanged arithmetic.
* `NativeWorldEffects.cpp::EmitVolumeShadow`: gathering of the posed geometry is unchanged. For **planar casters
  (bounding radius <= 30)** the projection and index conversion now go through the cache keyed on the shadow object:
  the entry is reused only if **positions, triangle indices, light direction, colour, caster center and the three
  ground samples g0/gx/gy are all bit-identical** (`memcmp`/bit compare, no version counters, no animation or
  identity assumptions); otherwise the geometry is recomputed canonically and the entry replaced. Casters with radius
  > 30 (structures and large vehicles) and anything uncertain stay on the previous path byte for byte.
* Lifetime: `NativeShadow::release()` forgets the entry (address reuse cannot match); `uninstall()` clears the cache;
  bounded to 16 MB with least-recently-used eviction; an entry larger than the budget is never retained.
* Nothing in the simulation is read for the key beyond what the renderer already read; nothing is written. The cache takes
  const pointers only and has no GameLogic/RNG/CRC dependency (asserted by a contract test).

## 20.2 New measurements (receipt / frames.csv / categories.csv)
* Scopes nested in `native_fx_shadows`: `native_shadow_gather`, `native_shadow_project` (cache lookup + projection),
  `native_shadow_emit`.
* Counters per frame: `shadow_cache_hits`, `shadow_cache_misses`, `shadow_uncached`, `shadow_cache_evictions`; receipt line
  `shadow_volume_reuse (per frame): hits .. misses .. uncached .. evictions ..`. Dormant outside a capture as before.

## 20.3 Honest expectation
A retail-model test shows the full-size Paladin has radius **35.2 > 30**, i.e. large tanks stay on the canonical path in this
version; units, trees and props below 30 are cached. The receipt's hit/miss/uncached split will state exactly what
fraction of the 1.009 ms became cacheable. A low hit rate means the radius cut-off, not the cache, limits the gain.

## 20.4 Validation (automated)
* `NativeShadowVolumeCache.*` (7 tests): result byte-identical to the former loop on miss and hit; every input
  (light, colour, g0, gx, gy, center, positions, topology, vertex count, -0.0 vs 0.0) forces recomputation and yields the
  fresh result; forget / address reuse; bounded eviction and oversize entries never return stale data; uncertain inputs
  use the canonical path and are not stored; inputs are never modified.
* Contract test: header has no simulation dependency, the key is a bit-exact comparison, the cache is only used for planar
  casters, entries are forgotten on release and cleared on uninstall.
* Retail-model readback test (`PlanarVolumeShadowIsBlendedOncePerPixel`, opt-in `GENERALS_OFFLINE_ASSETS`; run here with
  the Paladin model extracted from the retail W3D/Textures archives): full-size tank = uncached and identical frame to
  frame; scaled to a planar caster = miss then hit with **bit-identical images**; moving it recomputes and changes the
  image; moving it back restores the identical image.
* Matrix: Win64 ctest 16/16, g 246 and z 245 gtests; Win32 ctest 23/23, g 210 and z 210 gtests; all Python contracts and
  PowerShell workflow tests pass. Win32/DX8 reference code is untouched.
* Not claimed: any performance number. Automated tests do not establish the benefit.

## 20.5 Candidate and the native run
`build/dev-runtimes/native/Generals/x64-35-shadow-reuse` (staged and preflighted, not run by the agent):
* `game/generalsv.exe` SHA256 `C1501FB6D5C456DBB8500BBE8D6ACAAD0242DDC8B72256B9F63AB36CF0DBFAF7`
* `game/generalsv.pdb` SHA256 `1CC3FF6A3E17964D13ED8A42D08EFDCBA4D72B1889D554601D9ABEF8B38EA509`
* the capture summary must show `git=1a37a4c4d...` and contain `shadow_volume_reuse`.

Same Heavy uncapped scenario as C-A2 (same settings, Smooth OFF, VSync off, plugged in, Best performance):
```
pwsh scripts\native-runtime\Invoke-NativeCandidateCapture.ps1 -Candidate x64-35-shadow-reuse `
     -OutputDirectory C:\perf\x64-35-shadow-reuse\C-S1 -ExpectPhases `
     -ExpectedSha256 C1501FB6D5C456DBB8500BBE8D6ACAAD0242DDC8B72256B9F63AB36CF0DBFAF7
```
In game: Ctrl+Alt+Shift **F10**, **O**, **B**, wait 5 s, **F11**, let it finish, exit via the menu. Please also look at the
shadows during the run (units idle and moving, trees, buildings) for any difference from before.

## 20.6 Keep / revert (unchanged)
KEEP if `native_fx_shadows` drops >= 0.25 ms/frame and the frame improves >= 5 % with identical draw/triangle workload,
no visible shadow difference and no stale shadow. REVERT/REWORK if the saving is < 0.15 ms, any visual difference
appears, a stale shadow occurs, or a preservation/determinism contract is violated.


---

# 21. C-S1 result: shadow-volume reuse did NOT move the measured cost (before = C-A2)

Raw: `C:\perf\x64-35-shadow-reuse\C-S1` (wrapper-verified: exe SHA256 `C1501FB6...FAF7`, `git=1a37a4c4d...`). Before reference:
C-A2 (`x64-35-attribution`). Recorded separately from the capped baseline.

## 21.1 Comparability
Identical render workload; no material difference: objects 649.6 vs 649.8, drawables 121.2 vs 121.1, particles 10.9 vs 10.9,
draws 340.77 vs 340.72, triangles 110,865 vs 110,865, state requested/changed 6,612/384 vs 6,611/384, same hardware and
settings (uncapped, Smooth OFF, VSync off). Differences in the capture itself: C-S1 ran 60.0 s / 16,102 frames / 1,800 ticks
(C-A2 stopped at the 16,384-frame cap after 57.4 s / 1,724 ticks), and **C-S1 contains the late Present/GPU step starting at
38.5 s** (page-fault pair 3,287 + 3,288; fourth recurrence: 34.9, 43.7, 50.3, 38.5 s), which inflates its full-run
`native_end_render` (0.199 vs 0.059 ms) and frame time. For a fair comparison the first 40 s (before the step in both runs)
are used below.

## 21.2 Result
| (ms/frame, 0-40 s window) | C-A2 | C-S1 | delta |
|---|---|---|---|
| frame (outer) | 3.480 | 3.582 | +0.101 (+2.9 %) |
| `display_draw` | 3.212 | 3.301 | +0.089 |
| `native_objects` | 2.718 | 2.764 | +0.046 |
| `native_fx_shadows` inclusive | 1.0076 | 1.0275 | **+0.020** |
| `native_obj_process` | 0.861 | 0.875 | +0.014 |
| `native_fx_shroud` | 0.504 | 0.511 | +0.007 |
| `native_geom_upload` / `native_submit` | 0.146 / 0.142 | 0.145 / 0.146 | -0.002 / +0.004 |
| `native_end_render` | 0.051 | 0.074 | +0.023 |
| fps (clean window) | 287.3 | 279.2 | -2.8 % |
Full run: 3.504 -> 3.725 ms (268.4 vs 285.3 fps), dominated by the earlier late step. **The shadow time did not decrease
(+0.02 ms, within the run-to-run drift of unchanged phases: shroud +1.4 %, terrain +2.6 %, per-object processing +1.6 %).**

## 21.3 Why: the volume geometry was never the cost
New sub-scopes of `native_fx_shadows` (full run, per frame): `native_shadow_gather` 0.045 ms, `native_shadow_project` (cache
lookup + projection) 0.035 ms, `native_shadow_emit` 0.060 ms, **29 volume shadows per frame -> 0.140 ms in total**, while the
phase's **exclusive** time is **0.882 ms** (of 1.022). So only ~14 % of `native_fx_shadows` is the volume path this change
touches; **~86 % (0.88 ms) is spent in `CollectShadows` outside gather/project/emit**: the iteration over all registered
shadows (visibility/cull tests, object-space offsets), and in particular `EmitDecal` (decal shadows: a terrain-conforming grid
of up to 33 x 33 vertices with three `GroundHeight` lookups per vertex), which has no scope and no counter yet. The earlier
assumption that the 1.0 ms was volume geometry was a hypothesis built from reading the code without that split; the data now
refutes it. Subtracting the unchanged remainder, the volume path cost ~0.13 ms before the change and ~0.14 ms after it.
* Reuse distribution: hits 28.40 / misses 0.60 / **uncached 0** / evictions 0 per frame (98.1 % hit; 457,372 hits,
  9,586 misses over the run; the misses come in bursts of ~14 frames-worth of animated units at ~1 in 17 frames, plus the
  first frames). The cache behaves exactly as designed; nothing stale was observed in the counters.
* Radius rule: **0 casters were excluded** (`shadow_uncached` 0): all 29 volume casters in this scene are planar. The
  Paladin (r 35.2) is not in the scene; the 30-unit cut-off does not limit this result and widening it cannot help here.
* Memory: evictions 0; the cache stays under the 16 MB bound (bytes are not in the receipt; 29 casters of a few thousand
  vertices is on the order of 1-2 MB).

## 21.4 Hidden costs
None moved: geometry upload, submit and Present are unchanged within noise (Present clean-window 0.069 vs 0.047 ms is
the same drift as end_render); `native_objects` +0.046 ms is spread over unchanged phases. The cache's own overhead (3
scopes x 29 per frame plus memcmp + copy on a hit) is of the order of the 0.02 ms observed, i.e. it is not free and
currently buys nothing.

## 21.5 Criteria and decision
| criterion | target | result |
|---|---|---|
| `native_fx_shadows` reduction | >= 0.25 ms | **0.00 ms** (+0.02) |
| frame improvement | >= 5 % | **-2.8 % (clean window), -6 % (full run, late step)** |
| comparable draws/triangles | | yes (identical) |
| stale/incorrect shadows | none | none observed in counters; visual review not part of this analysis |
Verdict: **fails the keep target and the "<0.15 ms" revert line.** The hit/miss/uncached distribution shows the design is
sound and eligibility is not the limiting factor (0 uncached), so option B is excluded by the evidence; the limiting fact
is that the cached work is ~0.13 ms of the 1.0 ms.

**Recommendation: C - revert the batch.** Concretely: revert the cache logic (`NativeShadowVolumeCache.h`, the
`g_volumeCache` use in `EmitVolumeShadow`, its tests), **keep** the three sub-scopes and a `shadow_volume_casters`
counter, and add a scope + count around `EmitDecal` and the loop so that the 0.88 ms remainder is attributed
(a measurement step, not an optimization) before choosing the next target. Nothing is implemented in this analysis and the
radius cut-off is unchanged.


---

# 22. Human observation during C-S1: helicopter shadows pop in/out (suspected pre-existing; not fixed here)

**Status of C-S1 visual acceptance: NOT performed.** The reviewer did not inspect shadow fidelity during the capture, so
`x64-35-shadow-reuse` has no human visual shadow acceptance. (The performance decision in section 21 does not depend on it.)

## 22.1 Observation
A helicopter can be visible at the right edge of the screen while its shadow is absent; the shadow appears suddenly when the
helicopter moves further in, and can disappear again while the helicopter is still clearly inside the view.

## 22.2 Does it touch the cache path? Evidence says no
1. **The visibility/culling code was not changed by the batch.** `git diff f10c8a0eb..HEAD` of `NativeWorldEffects.cpp`
   touches only `EmitVolumeShadow` (projection/emit), `NativeShadow::release`, `uninstall` and the new scopes; the lines
   `isRenderEnabled()`, `isInvisibleEnabled()`, `Is_Not_Hidden_At_All()`, `camera.Cull_Sphere(object->Get_Bounding_Sphere())`
   and the `shadowCulled` accounting in `CollectShadows` have no added or removed lines (0 matches in the diff). The cull
   originates from X64.29 (`ee13ca8c6`).
2. **Ordering:** the cache is consulted last, after every early-out (render-enabled, not invisible, not hidden, frustum
   test, gather non-empty, triangle budget). A shadow that is skipped by an early-out never reaches the cache; the symptom
   "no shadow drawn" is an early-out symptom.
3. **The cache cannot produce absence or a stale pose:** for a given set of inputs it returns the geometry the canonical
   loop would produce (bit-exact key over positions, topology, light, colour, center, ground samples); a moving helicopter
   changes the positions every frame, so it always misses and is computed fresh. A first-sight shadow has no entry.
4. The behavior is described for a helicopter at altitude: the helicopter bounding radius may exceed 30 (then it is not
   cached at all) and the observed pop-in happens on first appearance, before any cache entry exists.
Conclusion: **independent of the cache; treated as a pre-existing (suspected) shadow correctness issue.** It cannot yet be
proven pre-existing by an old binary run (no one has compared x64-34 or earlier), only that the code path was not touched.

## 22.3 Suspects (from reading the current code; not verified)
| # | suspect | where | how it would look |
|---|---|---|---|
| 1 | **Caster-sphere culling instead of shadow-bounds culling**: the volume/decal shadow is skipped when the *caster's* bounding sphere is outside the frustum, although a shadow is located elsewhere (projected along the light, `altitude x light slope` away, or straight below for decals). | `CollectShadows`: `camera.Cull_Sphere(object->Get_Bounding_Sphere())` | shadow missing while the ground region it would cover is visible, or (mirror case) absent/late appearance relative to the aircraft; pops when the caster sphere crosses the frustum |
| 2 | **Frame triangle budget** `triangleBudget = 200000` shared by all volume shadows in registration order, silently skipping later casters (`return` without a counter). Aircraft are typically created later than the ground army. | `EmitVolumeShadow` | whole shadows toggling as other casters enter/leave view or change LOD in busy scenes; newest casters lose first |
| 3 | **HLod LOD / alpha rotor meshes**: `GatherShadowGeometry` takes the current LOD level and skips alpha/translucent meshes unless flagged `CAST_SHADOW`; a different LOD or a rotor-only model yields "no geometry" (`shadowNoGeometry`). | `GatherShadowGeometry` | shadow vanishes when the LOD switches |
| 4 | **Drawable-driven flags**: `setShadowsEnabled(false)` (stealth look), `enableShadowInvisible(fullyObscuredByShroud)` toggled as the aircraft crosses shroud/fog boundaries, hidden render object. | `W3DModelDraw` shadow flags, `Drawable.cpp` | pop on/off at fog or stealth state changes |
| 5 | **Widescreen/viewport geometry**: the native world uses the 3D camera frustum (same camera as drawing), so a plain 4:3 mismatch there is not apparent; but drawable-level visibility used by the game client (view-region based culling, the 0.80 viewport height scale, centered HUD island) is a place where an old 4:3 assumption could hide. | `W3DView`/GameClient visibility | pop-in near the left/right edges that depends on aspect ratio |
Caster versus shadow position (the question raised): for volume shadows `EmitVolumeShadow` projects each vertex along the
light onto the terrain; for decals the grid is placed under the caster. A high helicopter can therefore have its shadow
meaningfully displaced from its screen position, and the caster-only cull in suspect 1 ignores that displacement.

## 22.4 How to settle it (next, separate from the performance work; nothing changed here)
* Per-reason shadow drop counters in the receipt: skipped by disabled / invisible / hidden / frustum / no geometry / budget
  / over 60k vertices / emit refused, split by aircraft vs other (cheap, measurement only).
* One native look with the counters: a helicopter flying in from the right, plus the same on `x64-34-smooth-presentation`
  and the Win32 reference if available, to establish whether the behavior predates X64.35 and whether the reference
  renderer shows it.
* Only then choose a fix (e.g. cull on the shadow's projected bounds, budget by distance rather than registration order).
Not fixed, not part of the C-S1 performance analysis, and the cache is unchanged.


---

# 23. Shadow-volume reuse reverted; shadow attribution extended (decision C approved)

## 23.1 Record of the experiment
* **Hypothesis (from C-A2):** `native_fx_shadows` (1.009 ms, 29 % of the frame) is dominated by per-frame regeneration of
  planar shadow-volume geometry from unchanged inputs, so exact reuse should save 0.4-0.7 ms.
* **Implementation (`1a37a4c4d`, candidate `x64-35-shadow-reuse`):** bit-exact keyed per-shadow cache (positions, topology,
  light, colour, center, g0/gx/gy), planar casters only, bounded LRU; byte-identical to the former loop in tests.
* **Result (C-S1 vs C-A2):** 98 % hit rate (28.4 hits / 0.6 misses / 0 uncached per frame), **0.00 ms saved** (+0.02 ms),
  frame +2.9 % within drift. Criteria failed (>= 0.25 ms saving, >= 5 % frame improvement).
* **Why:** the volume path (gather 0.045 + project 0.035 + emit 0.060 = 0.14 ms for 29 casters) was ~14 % of
  `native_fx_shadows`; **0.88 ms (86 %) is exclusive time of `CollectShadows` outside it**. The hypothesis was built from
  reading the code without a split of the phase; the sub-scopes added in the same batch produced the refutation.
* **Decision:** rejected and **reverted** (`git revert` of `1a37a4c4d`): the cache header, `g_volumeCache` integration, LRU
  machinery, cache tests and cache counters are removed. The retail-model readback test returns to its original form.

## 23.2 What was kept / added (measurement only, no behavior change)
* Sub-scopes in `native_fx_shadows`: `native_shadow_gather`, `native_shadow_project`, `native_shadow_emit` (volume
  casters) and the new **`native_shadow_decal`** (`EmitDecal`).
* The **exclusive** time of `native_fx_shadows` in the phase table is, by construction, the `CollectShadows` loop itself
  (iteration, render-enabled/invisible/hidden/frustum tests, object-space offset, distance, dispatch): the former
  unattributed 0.88 ms is now decomposable into `decal` + `loop remainder`.
* Per-frame counters and a receipt line `shadows (per frame): seen .. skipped .. decals .. volumes ..`
  (`shadows_seen`, `shadows_skipped` = all early-outs, `shadow_decals` = decal shadows emitted, `shadow_volumes` = volume
  casters emitted). Dormant outside a capture. These counters also give per-frame numbers for the existing
  helicopter-shadow issue below, without changing any behavior.
* Test: `PerformanceReceipt.ShadowPhaseIsDecomposedIntoVolumeDecalAndLoopRemainder`.

## 23.3 Validation
Win64 ctest 16/16, g 240 and z 239 gtests; Win32 ctest 23/23, g 210 and z 210; all Python contracts and PowerShell workflow
tests pass (the instrumentation contract now asserts that the cache is gone and the shadow scopes exist). No performance
claim is made for this instrumentation commit and no new native candidate is staged (see 23.5).

## 23.4 Separately tracked: helicopter shadow pop-in (pre-existing, suspected) - see section 22
Not fixed here; the cache is gone, so it can no longer be a factor. The new `shadows_skipped` counter and the existing
suspects (caster-sphere cull vs shadow position, shared 200,000-triangle budget in registration order, LOD/alpha rotor
meshes, drawable shadow flags, widescreen visibility) remain to be investigated separately.

## 23.5 What was learned and the next measurement target
* A phase-level hypothesis must be split by sub-scopes **before** an optimization is built; the same batch that adds
  the sub-scopes cannot also be the optimization.
* The shadow loop and decals are now the target: **0.88 ms** in `CollectShadows` outside the volume path. Candidates (from
  code reading, unverified): `EmitDecal` builds a terrain-conforming grid of up to 33 x 33 vertices with three `GroundHeight`
  lookups per vertex for every decal shadow every frame; the loop visits every registered shadow, including ones with no
  visible caster.
* **Does this need another native run?** Yes, one: the current data cannot split the 0.88 ms between `EmitDecal` and the
  loop (the receipt of C-S1 has no scope for either), so one candidate was staged for that single purpose:

## 23.6 Candidate `x64-35-shadow-attribution` (instrumentation only; built from `f0c347b34`; not run by the agent)
* `game/generalsv.exe` SHA256 `0AAC4EE6C50160AA53F4EC0813D9023B3FE8B1300EB11AF1DF67A6847A377EC9`
* `game/generalsv.pdb` SHA256 `716DD369730229D3A1CF93D2FCBF7EAC9F32CE50541B8E3B50299710F7D566D6`
* Same Heavy uncapped run as C-A2/C-S1:
```
pwsh scripts\native-runtime\Invoke-NativeCandidateCapture.ps1 -Candidate x64-35-shadow-attribution `
     -OutputDirectory C:\perf\x64-35-shadow-attribution\C-D1 -ExpectPhases `
     -ExpectedSha256 0AAC4EE6C50160AA53F4EC0813D9023B3FE8B1300EB11AF1DF67A6847A377EC9
```
  (Ctrl+Alt+Shift F10, O, B, wait 5 s, F11; summary must show `git=f0c347b34...` and the line `shadows (per frame): ...`.)
* The superseded `x64-35-shadow-reuse` candidate contains the reverted code and can be retired by the retention tool
  after review (dry run first); nothing was deleted.


---

# 24. C-D1: `native_fx_shadows` decomposed - decal generation is 85 % of the shadow cost

Raw: `C:\perf\x64-35-shadow-attribution\C-D1` (wrapper-verified: exe SHA256 `0AAC4EE6...77EC9`, `git=f0c347b34...`, `dirty=0`).
Compared with C-A2 (attribution baseline) and the clean parts of C-S1/C-A1/C-U1. Recorded separately from the capped baseline.
**The shadow-volume cache (section 20-23) was rejected and reverted; it must not be reintroduced without new evidence.**

## 24.1 Comparability
Same render workload as C-A2: objects 649.5 vs 649.8, drawables 121.3 vs 121.1, particles 10.9 vs 10.9, draws 340.8 vs 340.7,
triangles 110,865 vs 110,865, state requested/changed 6,613/384 vs 6,611/384, **29 volume casters per frame** (as in C-S1),
same hardware/settings (uncapped, Smooth OFF, VSync off). Capture: 16,384 frames in 59.4 s (frame cap), 1,781 ticks. The late
Present/GPU step again occurred (`native_end_render` 0.121 ms vs 0.059 in C-A2; peak faults 3,287), which only affects
`end_render`/frame totals; all phase numbers below are insensitive to it (unchanged phases agree with C-A2 within 1-2 %:
objects 2.761 vs 2.726, per-object processing 0.863 vs 0.862, shroud 0.515 vs 0.505).

## 24.2 Decomposition of `native_fx_shadows` (1.0221 ms/frame)
| part | ms/frame | % of shadow phase | calls/frame | per call |
|---|---|---|---|---|
| **B. decal generation (`EmitDecal`)** | **0.8703** | **85.1 %** | **53** | **16.4 us** |
| A. volume shadows (gather 0.0441 + project 0.0484 + emit 0.0358) | 0.1283 | 12.6 % | 29 | 4.4 us |
| C. `CollectShadows` loop remainder (exclusive) | 0.0236 | 2.3 % | 633.5 inspected | 37 ns / shadow |
Per frame: **shadows_seen 633.5, shadows_skipped 551.5 (87.1 %), shadow_decals 53, shadow_volumes 29** (53 + 29 = 82 drawn =
633.5 - 551.5). **The dominant cost is decal generation: 0.87 ms = 24 % of the whole 3.62 ms frame, 26 % of `display_draw`.**
(The first shadow-volume optimization targeted part A, which is 12.6 % of the phase: that is why it could not help.)

## 24.3 What decal cost scales with
`EmitDecal` (`NativeWorldEffects.cpp`) builds, for each decal shadow every frame, a terrain-conforming grid of
`nu x nv` vertices with `nu = ceil(sizeX / 2.5) + 1`, `nv = ceil(sizeY / 2.5) + 1` (each clamped to 2..33) and, **per
vertex, three `GroundHeight` lookups** (`h`, `h(x+1.25, y)`, `h(x, y+1.25)`), each of which is
`TerrainLogic::getGroundHeight` -> `HeightMapRenderObjClass::getHeightMapHeight`. At 16.4 us per decal the grid is on the order
of 80-100 vertices (~250-300 lookups at ~55-65 ns), i.e. a decal of roughly 20-25 world units. The cost therefore scales
with (number of decal shadows on screen) x (decal area / 6.25) x 3 lookups; it does not depend on draws, triangles, UI or
particles. Decals are emitted for casters with a decal shadow type (the 53 are visible object/prop decals; the
skipped 551 are mostly culled or disabled casters, 37 ns each to reject). Grid sizes of individual decals are not recorded;
the 80-100 vertex figure is an inference from per-call time and the implementation, not a measurement.
Everything the vertices depend on is known: decal center (caster position + object-space offset), yaw, size X/Y, diffuse
colour, and the terrain height field (from a source search, heights are written through `WorldHeightMap::setRawHeight`, reached from
`W3DTerrainVisual` - terrain flattening for construction among them - and at map load; any other writer through
`getDataPtr()` would have to be ruled out first, which is part of the proposed work)

## 24.4 Helicopter pop-in (separate correctness issue, not conflated)
The new counters show that 87 % of registered shadows are rejected by an early-out each frame (`shadows_skipped`), but the
counter does not split the reasons (render-disabled / invisible / hidden / frustum), so C-D1 gives no direct evidence about
the helicopter case. It remains tracked separately in section 22.

## 24.5 Ranking of the remaining X64.35 targets (measured ms per frame, clean scene, uncapped)
| # | target | cost | share of frame | exactness/risk | verdict |
|---|---|---|---|---|---|
| 1 | **Decal geometry reuse (`EmitDecal`)** | **0.870** | 24 % | exact key incl. terrain revision; render-only | **recommended next** |
| 2 | Per-object CPU mesh processing (`ProcessObject`) | 0.863 | 24 % | many inputs (pose, lights, shroud, team colour); medium-high | later |
| 3 | Shroud mesh (`CollectShroud`) | 0.515 | 14 % | inputs: shroud levels, terrain heights, visible rect, colour; the same terrain revision as #1 | after #1 |
| 4 | `setUseHotkey` guard ("$ 10000" texture churn) | <= 0.15 | <= 4 % | one-line, exact | tiny, separately |
| 5 | `calculateZones` hitches | 20 ms x 5 / min | smoothness only | simulation, CRC | separate batch |
| 6 | Volume shadows (29 casters) | 0.128 | 3.5 % | | not worth it (tested: no gain) |
| 7 | Loop remainder | 0.024 | 0.7 % | | no |

## 24.6 Recommended next optimization (one): exact reuse of decal-shadow geometry
* **Measured cost:** `native_shadow_decal` 0.870 ms/frame, 53 decals, 16.4 us each (24 % of the frame).
* **Code path:** `NativeWorldEffects.cpp`: `CollectShadows` -> `EmitDecal` (grid build with 3 x `GroundHeight`/vertex) ->
  `NativeWorldObjects::emitEffect`.
* **Proposed bounded change:** a per-shadow cache of the *generated grid* (vertices and indices) keyed on **every** input of
  the generation, compared bit-exactly: center `cx, cy`, `yaw`, `sx, sy`, diffuse colour, the grid dimensions, and a
  **terrain height revision** (the heightmap object identity + a counter incremented in the single height mutator
  `WorldHeightMap::setRawHeight`). The texture lookup and `emitEffect` still run every frame, so blend state and texture
  residency are never cached. On any mismatch/uncertainty the canonical path runs. Bounded size with LRU, entry released
  with the shadow, as the earlier cache did (that machinery was sound; only its *target* was wrong).
* **Expected benefit:** most decals belong to static scenery, so the large majority hits: 0.5-0.8 ms/frame (14-22 % of the
  3.6 ms frame, ~275 -> ~330-350 fps uncapped). Moving casters (units' decals) miss and cost what they cost today.
  The new `decal` counters would report hits/misses so that the figure is not assumed.
* **Preservation risk:** low-medium. The only non-local part is the terrain revision: a missed mutation site would
  leave a stale decal on changed terrain (construction flattening). Mitigation: the revision lives in the single mutator
  and the unit tests drive `setRawHeight`; the counter is render-side bookkeeping only.
* **Determinism/CRC risk:** none for simulation: client-only geometry; the counter is a non-serialized member of the
  heightmap object (the logic and client height maps are separate instances); no RNG, no CRC input, no GameLogic write.
  Win32/DX8 renderer untouched (native Win64 path only; the member addition must keep the Win32 contracts byte-identical,
  which the established contract suite verifies).
* **Automated validation:** bit-exact cache tests as before (hit identical to the canonical grid; each input change - center,
  yaw, size, colour, terrain revision - recomputes; removal/eviction cannot return stale data; oversize not retained);
  a terrain test: `setRawHeight` under a decal invalidates and the new grid follows the new height; retail-model/readback
  equality; the full Win64/Win32 matrices and contracts.
* **Native before/after:** same Heavy uncapped capture; baseline C-D1 (`native_shadow_decal` 0.870 ms, frame 3.62 ms).
  Also look at decals (trees, buildings, units, construction on flattened ground) for any difference.
* **Keep / revert:** KEEP if `native_shadow_decal` falls by >= 0.35 ms **and** the frame improves >= 7 % with identical
  draw/triangle counts and no visible decal difference. REVERT/REWORK if the saving is < 0.2 ms, any decal differs, a stale
  decal appears after terrain flattening, or a contract is violated.
Not implemented. The volume-shadow cache stays reverted.


---

# 25. X64.35 optimization 2: decal-geometry reuse (implemented, awaiting native before/after vs C-D1)

Candidate `x64-35-decal-reuse`; implementation commit `b63d86149`; baseline for comparison **C-D1**. Still X64.35,
**not accepted**. This batch is decal geometry reuse only. The reverted shadow-volume cache is not involved.

## 25.1 Terrain-mutation audit (done before any code)
**Where the height consumed by `GroundHeight` comes from.** `GroundHeight()` (NativeWorldEffects.cpp) calls
`TheTerrainLogic->getGroundHeight(x, y)`. In the game `TheTerrainLogic` is `W3DTerrainLogic`, whose `getGroundHeight` is
`TheTerrainRenderObject->getHeightMapHeight(x, y, normal)`, which samples
**`TheTerrainVisual->getLogicHeightMap()`** (the *logic* `WorldHeightMap`; `m_map` of the render object only when there is no
terrain visual) through `getDataPtr()` and, outside the playable area, `getClipHeight` (same data). It uses no flip state,
no layers/bridges (those are `getLayerHeight`), nothing else.

**Every runtime path that can change those bytes** (repo-wide search of `setRawHeight`, `getDataPtr()`, `m_data`):
| path | effect | covered by |
|---|---|---|
| `WorldHeightMap::setRawHeight` (single inline mutator), reached from `W3DTerrainVisual::setRawMapHeight` <- `TerrainLogic::flattenTerrain` (construction flattening: `DozerAIUpdate`, `WorkerAIUpdate`) and the seismic lowering loop in `TerrainLogic.cpp` | per-cell height lowered in place | `m_heightRevision` incremented inside `setRawHeight` |
| `W3DTerrainVisual::xfer` with `XFER_LOAD`: `xfer->xferUser(data, len)` writes the whole logic height array **bypassing `setRawHeight`** (found by this audit; the first search had missed it) | bulk replace on savegame load | `noteHeightsReplaced()` called right after the bulk write |
| `W3DTerrainVisual::load`: `REF_PTR_RELEASE(m_logicHeightMap); m_logicHeightMap = NEW WorldHeightMap(...)` | map / world replacement, new object (possibly at the **same address**) | per-instance process-unique `renderSerial` (atomic counter, not the pointer) |
| client map `setRawHeight` in the seismic simulation (`m_clientHeightMap`) | not read by `GroundHeight` (logic map) | harmless: it bumps that instance's own revision |
| WorldBuilder/tools writers (`getDataPtr()` writes in editor code) | not part of the game | not built into the game; a contract asserts no other `setRawHeight` writer exists in the game sources |
| `WorldHeightMap` constructors/parse (`m_data` fills, resampling) | before the object is visible | new instance, new serial |
All other `getDataPtr()` users (terrain mesh build, volumetric shadow, scorch, `getMaxHeightInRegion`, ...) only read.
**Result: a complete and reliable revision exists without architectural change**: a render-side `renderSerial` +
`heightRevision` pair on `WorldHeightMap` (members guarded by `_WIN64`, so the Win32 reference code and its byte-identity
contracts are unchanged), never serialized, never read by game logic, no RNG, no CRC involvement.

## 25.2 Implementation
* `NativeDecalGridCache.h`: `Build()` is the former `EmitDecal` grid loop (same arithmetic and order); `Cache::get()` reuses a
  grid only when **centre, yaw, size X/Y, grid dimensions, colour, terrain serial and terrain revision are bit-identical**.
  Entry per shadow, LRU with an 8 MB budget, oversize entries never retained, released with the shadow
  (`NativeShadow::release`), cleared on uninstall. Texture lookup, blend state, `emitEffect`, ordering and every
  visibility/culling decision stay canonical every frame.
* `TerrainStamp()` supplies the terrain identity/revision **only** when the terrain logic is the W3D one
  (`typeid(*TheTerrainLogic)` name contains `W3DTerrainLogic`) and a logic height map exists; otherwise the stamp is 0 and
  the grid is built canonically every frame and never stored (tests/stubs, unknown sources).
* Counters (dormant outside capture): `decal_cache_hits`, `decal_cache_misses`, `decal_uncached`, `decal_cache_evictions`,
  `decal_cache_kb_gauge`; receipt line `decal_geometry_reuse (per frame): hits .. misses .. uncached .. evictions ..
  retained_kb_end ..`. `native_shadow_decal` is unchanged and comparable to C-D1 (it now measures lookup + build-or-reuse).

## 25.3 Validation (automated)
* `NativeDecalGridCache.*` (8 tests): `Build` equals the former inline loop; hits bit-identical; centre x/y, yaw, size
  X/Y, colour, grid width/height, terrain revision and terrain identity each force a recompute and the recomputed grid is
  the canonical one; terrain change with a new revision follows the new heights; unknown terrain stamp/null owner never
  cached; a moving caster misses every frame and matches canonical, then hits once it stops; forget and address reuse;
  bounded eviction and oversize; clear.
* `NativeWorldEffectsTest.DecalGeometryReuseIsPixelIdenticalAndFollowsChanges`: through the real native effects pipeline
  and the D3D11 WARP backend, the cached frames are **pixel-identical** to the canonical (uncached) frame; a moved/rotated
  decal recomputes and changes the image and restoring it restores the identical image; a terrain revision or identity
  change forces a rebuild. (The production stamp source - W3D terrain logic + logic height map - cannot be constructed in
  the hermetic test and is replaced by the test hook; it is exercised by the native run.)
* Contract tests: revision bump is inside `setRawHeight`, the serial is per instance, the savegame bulk write calls
  `noteHeightsReplaced`, no other `setRawHeight` writer exists in the game sources, the cache has no simulation dependency,
  unknown terrain logic is canonical.
* Matrix: Win64 ctest 16/16, g 249 and z 248 gtests; Win32 ctest 23/23, g 211 and z 211; all Python contracts (including the
  Win32-reference byte-identity contracts, untouched) and PowerShell workflow tests pass.
* Not claimed: any performance number, or visual acceptance.

## 25.4 Candidate and native run
`build/dev-runtimes/native/Generals/x64-35-decal-reuse` (staged and preflighted, not run by the agent):
* `game/generalsv.exe` SHA256 `F8C8803A8F6A3255CA36F7F1F489EC2CD599D405E47AD70F787E03772FB9E751`
* `game/generalsv.pdb` SHA256 `00DD89373B014DBBAFB3C0676A553DE63CAE45E2BBE9662DEE0514A5A62BFC1D`
* summary must show `git=b63d86149...` and a `decal_geometry_reuse` line.
Same Heavy uncapped scenario as C-D1:
```
pwsh scripts\native-runtime\Invoke-NativeCandidateCapture.ps1 -Candidate x64-35-decal-reuse `
     -OutputDirectory C:\perf\x64-35-decal-reuse\C-R1 -ExpectPhases `
     -ExpectedSha256 F8C8803A8F6A3255CA36F7F1F489EC2CD599D405E47AD70F787E03772FB9E751
```
(Ctrl+Alt+Shift F10, O, B, wait 5 s, F11.) Please also look at decals (tree/building/unit ground shadows, any construction
on flattened ground if you build something) for any difference or a stale decal.

## 25.5 Keep / revert (as approved)
KEEP if `native_shadow_decal` falls >= 0.35 ms/frame and the clean-window frame improves >= 7 % with comparable
draw/triangle counts, no visible decal/shadow difference and no stale decal after terrain changes. REVERT/REWORK if the saving
is < 0.20 ms, terrain invalidation proves unreliable, a stale decal appears, visual output differs unexpectedly, or a
contract fails. Not combined with any other optimization.


---

# 26. C-R1 result: decal-geometry reuse (before = C-D1)

Raw: `C:\perf\x64-35-decal-reuse\C-R1` (wrapper-verified: exe SHA256 `F8C8803A...FC1D`, `git=b63d86149...`, `dirty=0`).
Before reference: C-D1. Thresholds are the ones agreed before the run; none was moved.

## 26.1 Comparability
Same render workload as C-D1: objects 649.6 vs 649.5, drawables 121.2 vs 121.3, particles 10.9 vs 10.9, draws 340.78 vs 340.81,
triangles 110,865 vs 110,865, state requested/changed 6,612.7/384.4 vs 6,613.2/384.5, texture binds 182.8 vs 182.8,
**shadows seen 633.7 vs 633.5, skipped 551.7 vs 551.5, decals 53.0 vs 53.0, volumes 29 vs 29**; same hardware/settings
(uncapped, Smooth OFF, VSync off). Capture: 16,384 frames in 55.2 s (frame cap), 1,657 ticks. **The late Present/GPU step
occurred earlier than before (page-fault pair at 25.6 s; C-D1 at 49.1 s)** and inflates the full-run `native_end_render`
(0.363 vs 0.121 ms). Matching clean windows are used: **first 24 s of both runs** (before the step in both), 6,777 vs 7,675 frames.

## 26.2 Decal cache effectiveness
* Per frame: **hits 49.2, misses 3.77, uncached 0, evictions 0** -> hit rate **92.9 %** (53 decals/frame).
* **The production terrain stamp is active**: `decal_uncached` = 0 and the hits show the W3D terrain-logic check and the
  logic height map were found; the run did not fall through to the uncached path.
* Misses are **constant at ~3.8 per frame in every 10 s window and on tick and non-tick frames alike (4 on the first
  frame, none of the frames has 0)**: about four decal casters whose inputs change every frame (moving/turning units or a
  fading colour); they recompute as designed. The identities are not recorded, so which objects they are is inferred, not
  measured. No miss storms, no growth, no evictions.
* Retained size: the receipt prints `retained_kb_end 32171`, **which is an instrumentation artifact**: the gauge is
  `count()`-ed once per decal call (53x per frame). Divided by 53 it is **~607 KB** retained, far below the 8 MB budget.
  The gauge should be recorded once per frame (a later one-line instrumentation fix; no behavior impact).

## 26.3 Before / after (clean window 0-24 s, ms per frame)
| phase | C-D1 | C-R1 | delta |
|---|---|---|---|
| **`native_shadow_decal`** | **0.870** | **0.278** | **-0.593 (-68 %)** |
| `native_fx_shadows` | 1.022 | 0.433 | -0.589 (-58 %) |
| exclusive remainder (loop) | 0.0235 | 0.0237 | 0.000 |
| `native_shadow_gather / project / emit` | 0.044 / 0.048 / 0.036 | 0.045 / 0.050 / 0.038 | +0.001 .. +0.002 |
| `native_objects` | 2.754 | 2.279 | -0.475 |
| `display_draw` | 3.262 | 2.835 | -0.427 |
| **frame** | **3.540** | **3.125** | **-0.415 (-11.7 %)** |
| fps | 282.5 | 320.0 | +13.3 % |
Full run (including the earlier late step in C-R1): frame 3.622 -> 3.370 ms (-7.0 %), 276.0 -> 296.6 fps (+7.5 %), so the
gain is visible even through the larger Present penalty.

## 26.4 Did cost move elsewhere?
Phases that are unchanged code moved by +1-3 % (the same drift seen between earlier runs): per-object processing +0.012,
shroud +0.014, submit +0.014, UI +0.020, terrain +0.004, Present/end_render +0.005. **One phase moved more than drift:
`native_geom_upload` 0.148 -> 0.216 ms (+0.068, +46 %).** The geometry sent is the same, so a plausible reading is that
more frames per second push the same dynamic buffers harder (driver-side renaming/hazards), but this is not verified. It is
already inside the net result (-0.415 ms) and is the first candidate to look at if more throughput is wanted. Memory/page
faults: 8,200 vs 8,114, peak 3,286 (the same late-step pair), working set 501 vs 500 MB: no new memory cost; no new
unexplained phase (`display_draw_unattributed` 0.018 ms).

## 26.5 Work preservation
Draw calls, triangles, state counts, decal count (53.0), volumes (29) and skipped counts are identical to C-D1, so no decals
stopped rendering. The remaining 0.278 ms of `native_shadow_decal` is the work that stays canonical every frame: the texture
name/lookup, `emitEffect` and 3.77 rebuilds per frame (~0.06 ms at the previous ~16 us each), i.e. roughly
0.06 + 53 x ~4 us (canonical lookup/emit) = 0.28 ms, versus 53 x 16.4 us = 0.87 ms before. The saving (0.593 ms) is the
removed grid generation for ~49 reused decals, not missing rendering. Pixel equivalence of cached and canonical frames is
established by the automated pipeline test, not by this run.

## 26.6 Keep / revert (pre-agreed criteria)
| criterion | threshold | result |
|---|---|---|
| `native_shadow_decal` saving | >= 0.35 ms | **0.593 ms** |
| clean-window frame improvement | >= 7 % | **11.7 %** (full run 7.0 %) |
| comparable draws/triangles/decals | yes | **identical** |
| missing/stale decal work | none | none in counters; stale-after-terrain-change **not exercised** by this run (static scene) |
| strong result 0.5-0.8 ms | | **0.593 ms - in range** |
**Recommendation: KEEP**, subject to human visual confirmation (below). Strong result by the pre-agreed definition.

## 26.7 Visual / terrain acceptance status
**No human visual observation of decals/shadows has been reported for C-R1; visual acceptance is NOT claimed.** The automated
pixel/readback test establishes equivalence of cached and canonical output through the real pipeline, not human
acceptance. The terrain-invalidation path (construction flattening, savegame load) was not triggered by the Heavy
scenario: it is covered by the audit, contract tests and unit tests, and needs one human look: build a structure on sloped
ground (or load a savegame) and check that decals under/around it follow the flattened terrain. The helicopter shadow
pop-in (section 22) remains a separate pre-existing correctness issue and is unrelated to this cache.

## 26.8 X64.35 status and next step - recommendation B (stop optimizing, prepare closeout)
Optimizations kept: decal-geometry reuse (+ the earlier no-benefit attempts rejected and reverted). Clean-window frame in the
Heavy uncapped scene: 3.69 (C-U1/C-A2 era) -> 3.13 ms; ~320 fps on the laptop iGPU against a 120 fps cap, GPU span ~2.1 ms.
* **Why not another optimization now:** (1) the engine already runs ~2.6x above the 120 fps cap in the heaviest scene
  measured (CPU 3.1 ms, GPU span 2.1 ms); (2) the next targets are worth 0.5-0.9 ms each and carry more risk (per-object
  processing has many inputs: pose, lights, shroud, team colour; the shroud mesh needs a shroud + terrain key) or are tiny
  (`setUseHotkey` <= 0.15 ms); (3) what remains visible to a player is not average cost but the **20 ms ZoneCalc
  hitches** (5 per minute) and the **helicopter shadow pop-in**, which are correctness/smoothness items, not throughput;
  (4) the Heavy scenario had no combat (particles 11): logic cost under real combat is untested, and that, not another
  render micro-optimization, is the more informative measurement.
* **Proposed closeout:** human visual acceptance of decals/shadows plus the terrain-change look above; keep the
  instrumentation; record the late Present/GPU step and B3 as open observations; carry the ZoneCalc hitch, the helicopter shadow
  issue, the `setUseHotkey` guard and the `native_geom_upload` observation as separately scoped follow-ups. An optional
  final capped Heavy capture would give a like-for-like before/after against the original 120 fps baseline.
No further optimization is started; X64.35 is not marked accepted.


---

# 27. X64.35 ACCEPTED AND CLOSED (2026-10-16) - CP9 acceptance record

## 27.1 Decision
Human native visual acceptance of `x64-35-decal-reuse` is complete: shadows and ground decals looked normal in native
gameplay, no stale or incorrect decal behavior was seen. **The decal-geometry reuse (`b63d86149`) is formally KEPT.** The
helicopter shadow pop-in observed earlier is a separate pre-existing correctness issue and is not a failure of this
optimization. X64.35 deliberately stops here.

## 27.2 Accepted native result (C-D1 -> C-R1, clean window 0-24 s, same Heavy uncapped scenario)
| metric | C-D1 | C-R1 |
|---|---|---|
| `native_shadow_decal` | 0.870 ms | **0.278 ms (-0.593 ms, -68 %)** |
| `native_fx_shadows` | 1.022 ms | 0.433 ms |
| frame | 3.540 ms | **3.125 ms (-11.7 %)** |
| fps | 282.5 | **320.0** |
| draws / triangles / decals / volumes | 341 / 110,865 / 53 / 29 | unchanged |
| decal cache | - | hit rate 92.9 %, no uncached fallback, no evictions, ~0.6 MB retained |
| human visual check | - | accepted |
Full-run (including the earlier late Present step): 3.622 -> 3.370 ms, 276.0 -> 296.6 fps.

## 27.3 Closeout instrumentation fix (measurement only)
`retained_kb_end` was recorded once per decal call and therefore over-reported the retained cache size ~53x (C-R1 printed
32,171 KB for ~607 KB). The gauge is now recorded **once per frame after the `CollectShadows` loop**
(`NativeWorldEffects.cpp`); the cache, the renderer and every counter other than this gauge are unchanged. A contract test
asserts a single gauge write after the loop and outside `EmitDecal`. No native run was required for this accounting fix.

## 27.4 What X64.35 established
* **Baseline profiling** identified `game_client_update` / render preparation as the dominant CPU cost in the measured
  scene (logic 0.3-1.1 ms per tick; ~2.8-4.4 ms of work per frame; the 120 fps cap hid ~60 % idle time).
* **Fine-grained attribution** (backend submit/Present/upload timers, GPU timestamp span, native frame phases) explained
  99.5 % of `display_draw`: shadows, per-object CPU processing and the shroud mesh were the three large phases, while the
  D3D11 submission and Present were small (~0.2 ms).
* **The first shadow-volume cache hypothesis was disproven by measurement** (98 % hit rate, 0 ms saved; the volume path was
  ~14 % of the shadow phase) and was **reverted**; it must not return without new evidence. Its sub-scopes were kept and
  exposed the real cost.
* **Further attribution found decal generation to be the actual dominant shadow cost** (0.87 ms, 53 decals/frame).
* **Exact decal-geometry reuse** (bit-exact key including a render-side terrain identity/revision, after a mutation audit
  that also found the savegame bulk write) produced the measured improvement and passed automated, pipeline-pixel and
  native visual validation.
* **Deliberately not done** (no demonstrated need; lower confidence/higher risk): per-object CPU processing reuse, shroud
  reuse, D3D11 state cache, `setUseHotkey` guard.

## 27.5 Retained and accepted
Accepted state of X64.35: the performance instrumentation (receipt, phase table, backend timers, GPU span, texture
attribution, page-fault counters, shadow/decal counters, `-devFps`, capture wrapper); the decal-geometry reuse; the
retention policy and cleanup tool; all X64.33/X64.34 preservation contracts unchanged; the authoritative 30 TPS
simulation unchanged (the optimization is render-only, nothing in it reads or writes simulation state, RNG or CRC).
Native-verified candidate: `x64-35-decal-reuse` (built from `b63d86149`): `game/generalsv.exe` SHA256
`F8C8803A8F6A3255CA36F7F1F489EC2CD599D405E47AD70F787E03772FB9E751`, `generalsv.pdb` SHA256
`00DD89373B014DBBAFB3C0676A553DE63CAE45E2BBE9662DEE0514A5A62BFC1D`. (The closeout commit only moves one measurement gauge.)

## 27.6 Validation (closeout matrix)
Win64 ctest 16/16, g 249 and z 248 gtests; Win32 reference ctest 23/23, g 211 and z 211; all Python contracts and
PowerShell workflow tests pass; no test was weakened.

## 27.7 NOT claimed
* No multiplayer/replay/lockstep determinism claim beyond what was already proven (the work is render-only and was not
  replay-tested in this milestone).
* No GPU optimization (the GPU span was measured, not reduced).
* ZoneCalc is **not** resolved (attributed only).
* The helicopter shadow pop-in is **not** resolved.
* The late Present/GPU step is **not** resolved.

## 27.8 Remaining backlog (documented, not scheduled)
| item | measured / observed | note |
|---|---|---|
| per-object CPU processing (`ProcessObject`) | ~0.86 ms/frame in the measured scene | many inputs (pose, lights, shroud, team colour) |
| shroud mesh (`CollectShroud`) | ~0.51 ms/frame | key: shroud levels + terrain revision (now available) + visible rect |
| `setUseHotkey` / "$ 10000" texture churn | <= ~0.15 ms/frame (16,384 textures/capture) | missing equality guard in `W3DDisplayString::setUseHotkey` |
| `native_geom_upload` | +0.068 ms after the decal change | unverified reading: higher frame rate through the same buffers |
| ZoneCalc hitches | ~20 ms every ~11 s (path_queue = `calculateZones`) | simulation-side, must be CRC-neutral |
| late Present/GPU/page-fault step | recurs in uncapped runs at 25-50 s, Present 0.05 -> 0.3-0.8 ms, 2 x 13 MB faults | unexplained, not tied to game state |
| B3 long-stall anomaly | one run, logic stalls up to 1.7 s | environmental/unexplained |
| real heavy combat | not profiled (the Heavy scenario had ~11 particles) | most informative next measurement |
| helicopter shadow pop-in | pre-existing, suspected caster-sphere cull / triangle budget / LOD / flags | separate correctness issue (section 22); `shadows_skipped` counter available |
| instrumentation | per-reason shadow-drop counters not yet added | only if the helicopter issue is investigated |
