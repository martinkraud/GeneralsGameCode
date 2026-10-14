# X64.35 Performance profiling + evidence-driven optimization

Baseline commit: `96766b2d9` (X64.34 accepted). Branch `dev/modern-engine`.
Status: **FIRST DELIVERY (baseline instrumentation + benchmark procedure). Waiting for native baseline receipts.
No optimization has been made. X64.35 is NOT accepted.**

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
_Pending human capture. No number is claimed here._

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
