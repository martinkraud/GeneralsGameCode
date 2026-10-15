# X64.35 Performance profiling + evidence-driven optimization

Baseline commit: `96766b2d9` (X64.34 accepted). Branch `dev/modern-engine`.
Status: **BASELINE COMPLETE (2026-10-15); awaiting review before any optimization. No optimization has been made.
X64.35 is NOT accepted.**

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
