# Engine completion: AMD64 audit, D3D11 stabilisation, render hitches and CPU performance

Milestone start: `2071ad765` (multiplayer presentation fix, **automated validation passed; real two-PC validation pending - not claimed**).
Scope: engine infrastructure, AMD64 correctness, D3D11, performance and smoothness. No new AI, gameplay, UI, rebalance, Vulkan or Linux work. The authoritative simulation stays at ~30 ticks/s;
nothing in this milestone changes a tick (golden gates: section 8).
Machine for every number below: the development laptop (Intel Core Ultra 5 125U, 14 logical processors, 16 GB, Intel Graphics iGPU, 1920x1200 panel at 60 Hz, Windows 11). The desktop/high-end
case is emulated by a smaller client (1280x800) and VSync off, as in `MULTIPLAYER_SMOOTHNESS_AUDIT.md`; real desktop numbers are pending.

## 1. AMD64 audit

### Method
* `/W3` (the `win64` preset) reports pointer truncation (C4311 / C4302) and integer-to-pointer widening (C4312). A **full** rebuild of both titles (3 417 steps) was scanned with
  `scripts/diagnostics/Summarize-PointerWarnings.py`; the earlier X64 scan (`docs/findings/AMD64_PORTING_FINDINGS.md` A-6) had left the LAN / GameSpy / UI cases "not individually audited".
* Targeted greps: `reinterpret_cast`/C casts of pointers to 32-bit integers, `(Get|Set)WindowLong`, 32-bit types holding handles, inline `__asm`, `sizeof` / alignment assumptions, memory pool layout.
* Upstream (`TheSuperHackers/GeneralsGameCode`, 179 commits ahead of our merge base `adac468d7`) was compared commit by commit (section 9): the relevant AMD64 fixes were ported.

### Result of the compiler scan (full rebuild, after the ports below)
82 distinct sites: **80 are C4312 "int -> void \*"** (an integer stored in a user-data / item-data pointer slot: list-box item data, module type, progress percent, player index, GameSpy ids). The
producers are non-negative or small values and the readers now take them through `(Int)(intptr_t)` / `(UnsignedInt)(uintptr_t)` (upstream #3459); no pointer or handle is stored this way. They are left as they are
(a cosmetic `(intptr_t)` on the producer side would not change behaviour; "no broad cosmetic refactors").
Two are genuine truncations that cannot be fixed here:
| Site | Why it stays |
|---|---|
| `VideoDevice/Bink/BinkVideoPlayer.cpp:327` `BinkSoundUseDirectSound(void*)` -> `unsigned long` | the Bink SDK prototype is 32-bit; there is no AMD64 Bink and the native runtime does not instantiate this player (movies are out of scope: FFmpeg/Bink excluded) |
| `WWAudio/SoundSceneObj.h:245` `(LogicalListenerClass*)param1` from `uint32` | `AudioCallbackClass` carries `uint32` parameters; the logical-listener event is not used by the engine's audio path (our mixer replaces Miles, `X64.31`); changing the interface touches every implementer |

### Fixed in this milestone (real defects, all pointer-width or UB; none on a simulation path)
| Change | Defect | Provenance |
|---|---|---|
| IME candidate list | `(Char*)((UnsignedInt)clist + (UnsignedInt)clist->dwOffset[i])`: pointer arithmetic through 32 bits | upstream #3459 (`2bdb1bee4`) |
| `GadgetListBox` `GLM_GET_SELECTION` | `*(Int*)mData2 = (Int)list->selections` wrote a truncated pointer | #3459 |
| `LocalFile::writeFormat/writeChar` | returned `(Int)character` (a pointer) as the byte count | #3459 |
| `huffencode.cpp`, `surfaceclass.cpp`, `assetmgr.cpp` (both titles) | pointer differences / addresses through `long`, `unsigned int`, `int` | #3459 |
| ~60 `userData` / `GadgetListBoxGetItemData` readers | `(Int)ptr` -> `(Int)(intptr_t)ptr` | #3459 |
| `MemoryPool` alignment | pool blocks and objects only 4-byte aligned on AMD64 (`MEM_BOUND_ALIGNMENT 4`, "make this work for 8") | #3609 (`e2a1216b1`) |
| `cpudetect.cpp` | CPUID registers stored through `(unsigned&)` into unaligned strings (UB) | #3636 |
| `W3DVolumetricShadow.cpp` (both titles) | UB when swapping silhouette indices / writing shadow-volume vertices | #3635, #3628 |
| `Keyboard` | key-repeat timing at high frame rates | #3591 |
| `Recorder::readNextFrame` | replay frame value not range-checked | #3090 |

### Remaining x86 heritage (documented, not removable yet)
* The **Win32 reference build** (`win32` preset, `DX8Backend`, `DX8Wrapper`, legacy `dx8*` render path) is kept compiling as the behavioural reference and for the reference-token contract tests; it is not part of the native runtime.
* `setFPMode()` asks for 24-bit x87 precision; the AMD64 CRT ignores `_PC_24` (`REPLAY_DETERMINISM_FINDINGS.md` R-3). The authoritative simulation is deterministic per executable (golden gates); **cross-architecture
  (Win32 <-> AMD64) lockstep / replay equivalence is not claimed**.
* Inline `__asm` blocks remain in WWMath / debug / profile helpers behind `_M_IX86` guards; none is compiled on AMD64.
* `ThreadClass::handle` (`WWLib/thread.h`) stores a thread handle in `unsigned long`; the class is not used by the native runtime.
* `lzo_ptr_t` is `unsigned long` (alignment arithmetic only; compression paths run the X64.10 tested kernels).
* Third-party: Miles and Bink are 32-bit only (replaced / excluded), DX8 D3DX helpers re-implemented (`AMD64_PORTING_FINDINGS.md` B-1 / B-2).

**Verdict.** The native AMD64 runtime is *substantially complete and audited*: no unreviewed pointer truncation is left on a reachable gameplay path, the two exceptions above are third-party interface limits, the memory
allocator is 8-byte aligned, and both titles pass the determinism gates. "Production-ready" additionally needs the human/native checks listed in section 11.

## 2. Direct3D 11 audit (`Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11Backend.cpp`)

| Area | State | Decision |
|---|---|---|
| Device creation | `D3D11CreateDeviceAndSwapChain`, feature levels 11_0 / 10_1 / 10_0, vs_4_0 / ps_4_0 shaders, hardware or WARP (tests); no debug layer flag | fine. Shaders compile at startup (`D3DCompile`); a pipeline cache would shorten startup only. |
| Swap chain | was legacy blit `DXGI_SWAP_EFFECT_DISCARD`, 1 buffer, `Present(vsync, 0)` | **changed**: `FLIP_DISCARD` + 2 buffers (+ `ALLOW_TEARING` with VSync off) with fallback to the legacy chain (`GENERALS_D3D11_BLT=1` forces it). Log line `graphics_device ... swap=flip+tearing`. |
| Frame latency | DXGI default (3 queued frames) | measured switch only: `GENERALS_FRAME_LATENCY=1..3`; default untouched (no input-latency instrument here). |
| Resize / mode change | `ResizeBuffers` after releasing RTV/DSV/low-res target; now repeats the creation flags | fine. Fullscreen is borderless (no exclusive mode): DWM / independent flip. |
| Present / VSync / limiter | `Present(1)` is the VSync switch; render limiter is the engine's own QPC limiter (`FrameRateLimit`), no sleeping in the backend | fine; stable 8.33 ms at 120 Hz, 16.65 ms at 60 Hz VSync (section 4). |
| Device loss | any failure throws `std::runtime_error("...HRESULT")` -> fatal | **improved diagnostics**: removed / reset devices now report `GetDeviceRemovedReason()`. A full recreation of every texture / buffer from CPU copies is **not implemented** (a TDR in a heavy match is fatal); recorded as a risk. |
| Dynamic buffers | whole-buffer `MAP_WRITE_DISCARD` per 2D flush; world geometry in CPU-backed buffers uploaded per submit | measured: `submit` 0.3-0.45 ms and `upload` 0.1-0.3 ms per frame in the hitch table; not a bottleneck. |
| Textures | immutable file textures, dynamic UI textures, mipped atlases; `TexturesCreated/Destroyed` counters | UI sentence textures are created/destroyed in pairs a few times per second (2-3 per hitch row, upload < 0.3 ms): churn, not cost. |
| State changes | `Impl::Note` tracks the last issued state; `state_requested` vs `state_changed` counters (X64.35) | already measured and de-duplicated; no new redundant-state finding. |
| GPU synchronisation | timestamp queries only on request, polled `DONOTFLUSH`; the only blocking call is `Present` (VSync) and the staging read-back for screenshots | fine. |
| Legacy behaviour | `TestCooperativeLevel` (D3D8) is skipped on the native path; the D3D8 device-reset fixes upstream made recently (flat heightmap, tree buffer, tools) are **obsolete** here because there is no device reset path | -- |
| Shutdown / leaks | `ClearState` + `Flush` in the destructor; long-session handle and private-byte counters stay flat (section 7) | fine. |

**Verdict.** D3D11 is the production renderer of the native runtime. Remaining legacy code: the DX8 reference path (Win32 build only) and the absence of device-loss recovery.

## 3. Render hitches (the earlier "550 ms" frames)

### Instrumentation (kept, off by default)
`GENERALS_FRAMEDELIVERY=<dir>` writes `<pid>-hitches.csv`: every iteration >= `GENERALS_FRAMEDELIVERY_HITCH_MS` (default 40 ms) with the time before the display draw / inside it / after it (messages, network, logic) / in the limiter
wait, the logic tick time, thread CPU time, Present / submit / upload time, texture and buffer creations, page faults, working set, private bytes and handles. `GENERALS_FRAMEDELIVERY_PROFILE=1`
adds `<pid>-hitch-categories.csv` (exclusive time per `PerformanceProfile` category of the hitch frame). `Invoke-FrameDeliveryRun.ps1 -Endurance` adds the X64.44 per-tick log.

### What the measurements say
1. **The 550 ms frames did not reproduce** in three further runs (Skirmish 330 s + 420 s, LAN 330 s). The earlier observation was taken while the machine was in a slow state (34 ms frames offline too); the
   frame-delivery summary of that run reported tick-duration *p99* (1.2 ms), which hides a single outlier tick per window - the conclusion "not a simulation stall" was not proven. **Corrected here: use `-Endurance`
   (exact worst tick) next to the frame log.** If it appears again the hitch row names its phase.
2. Reproducible hitch classes (Zero Hour, Twilight Flame, 1 human + 4 medium AI, 60 Hz VSync, 330 s, 45 hitch rows >= 40 ms; the 150 s profile run adds the category attribution):

| class | typical size | cause (attributed) | where | fix status |
|---|---|---|---|---|
| match start | 150-190 ms, once | first draw creates 49 textures + 138-586 buffers, 15-43 k page faults (display_update 90 ms: terrain, objects, UI) | `draw` / `display_update` | not changed (one frame after the loading screen); pre-creating at load is a candidate |
| **terrain full update when a structure changes the height map** | **31-33 ms** (one 45 and one 99 ms), 6 in 150 s | `setRawMapHeight` -> `staticLightingChanged` -> `scheduleFullUpdate` -> `W3DView::updateTerrain` -> `updateCenter` -> `updateBlock(0,0,x,y)`: the whole draw window is re-lit and its vertex buffers rebuilt for one flattened footprint (inside `W3DDisplay::update`; native_terrain 6-7 ms is only its visible child) | render thread, `predraw` | **documented, not changed**: needs a dirty-rect `updateBlock` plus the scorch / road lighting invalidation; render-only and exact-pixel testable, a good next item |
| first-use asset work | 30-40 ms, every 5-40 s early in a match | `predraw` with 3-19 k page faults and 1-3 texture creations (models / textures / effects first seen, memory growing) | main thread | candidate for asynchronous preloading (class A, section 6) |
| **simulation tail** | **24-77 ms** (frames 130-346 s: 28 of 45 rows; `tick_ms` ~ interval - draw) | `path_search` 15-35 ms (failing A\* searches, `HUNT` AI units against a stationary army), `ai_player_strategy` 7-13 ms, `object_update_loops` 9 ms | logic tick | known (`PERFORMANCE_FINDINGS.md` P-7: exhaustive failing searches have no exact shortcut) |
| Present wait | 13-14 ms inside `draw` | VSync at 60 Hz (`native_end_render`) | not a hitch | -- |

3. **Large frames are simulation ticks.** In the 804 s Zero Hour run (13 hitch-recorder rows per minute, 694 rows >= 40 ms) **634 rows are dominated by the logic tick (tick time > 60 % of the frame), 5 by the
   pre-draw phase and 1 by the draw phase.** The worst frames (237, 208, 196, 172 ms at 4.0-4.4 game minutes) are single ticks of 155-195 ms with 4 failing searches (>= 5 ms each) and one pathfinder
   zone recalculation in the same interval (ZoneCalc itself costs ~15 ms there: label 2.9 + relabel 1.7 + blocks 1.1 + hierarchy 3.2 + flatten 5.2 ms): after a structure changes the pathfinding map the
   queued, previously deferred searches run and exhaust their open lists (P-7). The same pattern at 7.5 min (100 ms, 29 failing searches). **So the "550 ms class" - if it recurs - is a simulation tick, and
   the hitch row will say so (`tick_ms` ~ `interval_ms`).**

Not seen: GPU stalls (Present 0.07-0.7 ms with VSync off), filesystem or logging stalls, handle growth.

## 4. Single-player smoothness and frame pacing

(Zero Hour Skirmish autoflow, laptop; per-frame statistics from `Summarize-FrameDelivery.py`.)

| configuration | interval p50 | p95 | p99 | p99.9 | notes |
|---|---|---|---|---|---|
| 60 Hz VSync, legacy blit chain (2 runs) | 16.67 / 16.64 | 17.65 / 17.65 | 19.30 / 19.05 | 44 / 41 | interpolating 97.5 % |
| 60 Hz VSync, flip chain (2 runs) | 16.63 / 16.63 | 17.52 / 17.40 | **18.69 / 18.35** | 48 / 44 | interpolating 97.0-97.4 % |
| 120 cap, VSync off, blit (2 runs) | 8.33 / 8.33 | 8.97 / 8.33 | 11.99 / 8.34 | 40 / 39 | |
| 120 cap, VSync off, flip + tearing (2 runs) | 8.33 / 8.33 | 8.34 / 8.33 | **9.70 / 8.34** | 42 / 38 | |

Render-rate sweep (Quick Game: `-devMode -quickGame -devFps N`, a fixed offline Twilight Flame match, 1280x800, VSync off, 40 s each, final binaries):

| render cap | interval p50 | p95 | p99 | p99.9 | frames > 50 ms | ticks / s | interpolating |
|---|---|---|---|---|---|---|---|
| 30 | 33.33 | 33.33 | 33.34 | 37.2 | 1 (map start) | 30 | 0 % (by design: nothing to interpolate) |
| 60 | 16.67 | 16.67 | 16.67 | 21.0 | 1 | 30 | 97.4 % |
| 120 | 8.33 | 8.33 | 8.33 | 8.53 | 1 | 30 | 97.4 % |
| 144 | 6.94 | 6.94 | 6.95 | 7.97 | 1 | 30 | 97.4 % |
| uncapped (diagnostic) | 3.66 | 4.04 | 4.25 | 4.86 | 1 | 30 | 92.0 % (273 fps) |

Every capped rate delivers frames at the cap to within 0.02 ms at p99 (a stable 120 beats an uncapped 270 with a wider tail); the single frame above 50 ms in each run is the match-start frame (section 3).


Interpolation (units, turrets) is active whenever the render interval is below the 30 Hz tick period and the clock is valid (97-100 % of iterations); the camera integrates the measured render
interval (`getBaseOverUpdateFpsRatio`); nothing extrapolates authoritative state. The residual hitches are the classes of section 3, not pacing.

## 5. CPU performance (simulation budget 33.333 ms)

Endurance run, Zero Hour, Twilight Flame, 1 human (idle) + 4 medium AI, 7.0 game minutes, 12 609 ticks (`GENERALS_ENDURANCE`, laptop):

| metric | value |
|---|---|
| tick mean / median / p90 / p95 | 3.14 / 1.98 / 3.92 / 8.11 ms |
| **p99 / p99.9 / worst** | **41.9 / 61.2 / 77.7 ms** |
| ticks above 16.67 / 25 / 33.3 / 50 / 66.7 ms | 335 / 243 / **159** / 60 / 4 |
| model missed deadlines / max backlog | 190 / 97.6 ms (real-time lag at the end 5.6 s) |
| share of the tick in the last window | `path_search` 52 %, `ai_object_base_update` 27 %, `object_update_loops` 15 %, scripts 6 %, zone calc 0 % |
| searches | 18 522, exhausted the open list 206 (1.1 %), 6 277 ms in failures (avg 30 ms each) |

Longer session (same setup, 804 s until the idle human base was destroyed, 23 303 ticks): mean 4.40 / median 3.15 / p95 13.7 ms, **p99 36.4 ms, p99.9 62.2 ms, worst 194.7 ms**, 292 ticks (1.25 %) above 33.3 ms,
385 model-missed deadlines, max backlog 173.6 ms. The tail grows with the army (objects 461 -> 640, units 190 -> 276, stationary non-idle units 12 -> 52).

The headless golden-gate replays (no render, AI vs AI, 5 game minutes) give p99 4-6 ms for 7 AIs: the tail seen here comes from the *graphical, human-versus-AI* case in which AI units hunt a stationary army (P-7), and is
about 1.3 % of ticks over budget on this laptop CPU. **The simulation is therefore not "comfortably inside 33.333 ms" in this case; it is inside it for 98.7 % of ticks.** No exact shortcut exists (P-7), so none was
implemented; each such tick is also a visible render hitch because the logic runs on the render thread (section 6).

Other subsystems (same run, per frame): rendering submission 0.3-0.45 ms + upload 0.1-0.3 ms (CPU side, iGPU), UI / display_update 0.5-2 ms except the terrain event above, audio on the mixer thread (X64.31),
filesystem none in steady state. Network cost is measured in `MULTIPLAYER_SMOOTHNESS_AUDIT.md`.

## 6. Multithreading feasibility (builds on `X64_38_CPU_SCALABILITY.md`, whose audit still holds)

Ranked by measured cost (this section's data) with the A / B / C classification requested:

| # | Subsystem | measured cost | benefit if parallel | determinism risk | complexity | class | recommendation |
|---|---|---|---|---|---|---|---|
| 1 | A\* path search (failing searches, 52 % of the tick at the tail) | tail ticks 24-77 ms | large for the tail (searches are independent *reads* of one tick's state) | high: scratch lists live in the shared cells (`PathfindCell::m_info`), results must be published in request order, blockers depend on unit state | high (per-worker scratch, request-order commit) | B | **future plan**: worker-owned search state + index-ordered commit, proven against the golden digests; not built |
| 2 | AI strategy / object updates (27 % + 15 %) | 1-3 ms mean | small mean, tail only with #1 | RNG + object creation inside | very high | C | keep serial |
| 3 | First-use asset loading / decode (predraw 30-40 ms, start 150-190 ms) | main thread, page-fault bound | removes the hitch | none for the simulation (client side), but the asset managers are not thread-safe | medium-high | A | **candidate**: preload at map start from the map's player templates, then asynchronous decode behind a bounded worker; needs the asset manager's ownership audit first |
| 4 | Terrain full update (31 ms events) | 31-99 ms | removes the hitch without threads | none | medium | (render) | do the dirty-rect update instead of threading |
| 5 | Telemetry / logging / replay writing | negligible | none | -- | -- | A | already off the hot path (X64.42: no synchronous flush) |
| 6 | ZoneCalc | 0 % of the tail (zone_c 22 -> 3 calls) | none | -- | -- | B | already O(1) per merge (X64.39) |

Idle CPU: 14 logical processors, main-thread bound (one core near the tick tail, the rest idle). The GPU (iGPU) is not the limit at 60 Hz VSync. **No threads were added**: the only workloads with a strong case (#1)
need a deterministic per-request commit that cannot be proven inside this milestone, and #3 needs a thread-safety audit of the asset managers first. A job system is therefore not justified yet; when it is, the
design in `X64_38` section 10 (bounded workers, index-addressed results, serial commit, serial fallback) applies.

## 7. Memory, allocations and long sessions

Runs: Zero Hour 804 s (until the idle human base was destroyed; 23 303 ticks) and Generals 900 s (26 984 ticks), both Twilight Flame, 4 medium AI, 60 Hz, endurance + frame-delivery windows every 5 s
(`ws_mb`, `private_mb`, `handles` columns).

| | Zero Hour | Generals |
|---|---|---|
| working set / private bytes | 654 -> 690 MB for 7 min, 800 -> 845 MB from 7.8 min (army 276 units / 640 objects), back to 788 MB | 660 -> 770 MB (army growth); flat 659-682 MB for 12 min |
| handle count | 767-778 (no growth) | 716-727 (no growth) |
| render interval p99 per 5 s window | 17.6-29.7 ms; windows above 40 ms coincide with simulation tail ticks | 19-28 ms |
| achieved tick rate | 30.0 except windows with a 150-200 ms tick (23-29 TPS) | 30.0 throughout (one 23 TPS load window) |
| tick p99 / worst | 36.4 / 194.7 ms (292 ticks > 33.3 ms) | **9.1 / 59.4 ms (2 ticks > 33.3 ms)** |
| model backlog / missed deadlines | 173.6 ms / 385 | 26 ms / 2 |

No leak signature: memory follows the object count, handles are flat, no timer drift (30.0 TPS steady states) and no degradation of the render interval over 15 minutes. The Zero Hour run ended when the AI destroyed the
idle human base (score screen; the autoflow cannot quit from there - the process was closed by hand), which is also why it is shorter than the 25 minutes planned. Generals' tail is an order of magnitude smaller
than Zero Hour's in the same setup (different AI scripts / factions), which matches the P-7 attribution (failing searches against a stationary army).


## 8. Determinism and regression gates

* Golden AI-behaviour gate (`Invoke-AiBehaviorGate.ps1`, Original mode, quick, final binaries of this milestone): **Zero Hour 4/4 and Generals 4/4 scenarios have digests identical to the X64.51 reference executables**,
  runs repeatable (`FEB85F7AFBD9673F`, `B669DB7C069075D4`, `96AFF1E57AF26425`, `4D2107AE64EF81D2` / `1078C5BDABF7C2A3`, `F26E9F602EC1F5C9`, `503FAEE79AC59769`, `3D88B0F00C919ABA`).
* Google Test: 556 (Zero Hour) / 557 (Generals) pass (including `NetworkPresentationTest`).
* Python suites: `native-runtime` 329 OK, `performance` 49 OK, `rendering` 30 OK, `diagnostics` 5 OK.
* The multiplayer smoothness changes of `2071ad765` are untouched. **Real two-PC validation of `2071ad765` remains pending.**

## 9. Upstream audit (`TheSuperHackers/GeneralsGameCode` main `116fbcfd3`, 2026-11-04; 179 commits past `adac468d7`)

Method: `scripts/diagnostics/Classify-UpstreamCommits.py` (mechanical `git apply --check` of the code part, both directions) plus review of each patch. `git fetch upstream main` was used (read-only); nothing was pushed upstream.

| Classification | Commits (upstream hash, PR) |
|---|---|
| **Ported** (cherry-picked with `-x`, builds, gates equal) | `2bdb1bee4` #3459 x64 pointer truncations (adapted: GameWindow.h / Except.cpp / debug_stack.cpp keep ours, unused tree id not taken), `e2a1216b1` #3609 8-byte pool alignment, `992869b75` #3636 CPUID UB, `6a69f844b` #3635 + `7b91c661c` #3628 shadow-volume UB, `c42ea9f0c` #3591 key repeat, `88faea1aa` #3090 recorder range, `d914b0f15` #3476 / `8e6d7bf18` #3485 handle and buffer leaks, `ac4965a02` #3462 / `4a4a1b23f` #3499 / `a8464bcff` #3498 / `2d6761cc0` #3488 LAN leaks and sockets, `d6c32650f` #3546 LAN tooltip crash, `203e0944f` #3600 speech lock, `afc2a103f` #3641 modifier reset, `68d1668ab` #3579 particle load, `3d82630f2` #3529 weapon slot sanity |
| Taken then **reverted** | `fed53656f` #3559 DeathTypeFlags: moves every flag bit by one position; flags are INI-parsed / module data and may reach saves or state CRCs (equivalence unproven). Reverted because they edit **reference-frozen files** (the contract guards of X64.x / Stage4B.1 would need a reviewed re-pin; applicable later together with that re-pin): `c96b610fa` #3633 locomotor reset crash (`Locomotor.cpp`, Stage4B.1 guard), `fa16f49de` #3603 texture creation fallback, `02b5b56e2` #3489 river index overflow, `9c9d8fdf8` #3517 cinematic display string leak (`W3DDisplay.cpp`), `e98d64467` #3497 `GameWindowManager` file-handle leak (static-menu parser guard), `5d69c00a8` #3486 map size overflow (`WorldHeightMap.cpp`) |
| Still present, **applicable but deliberately not taken now** (changes timing, protocol, saves or gameplay; needs its own gate) | `c00312568` netpacket repeat commands, `a30d06138` leave from disconnect screen (network protocol: no speculative MP changes), `4cc804db3` / `28bb4554f` / `5cf7802da` / `abcd6fdb9` save / load serialisation (save-format effect), `1abf73d5d` #3332 path following, `d1bd190af` #3003 diagonal speed, `8ed77a761`, `b7f0e31cb`, `ea40abbd5`, `c529745ce`, `f37300016`, `2119cc9b6` AI / gameplay behaviour, `090934812` / `2886271c5` tunnel fixes (gameplay safety, simulation state) |
| Needs manual adaptation / diverged | `454ac9329` #3532 bit_cast (touches the D3D8 wrapper), `c7b7814e9` #3588 malformed BIG (our `BIGFileSystem` differs), `7dd3786c3` #3630 hash alignment (different hash code here), `516471dc3` #3549, `7c4143284` #3566, `8b0430b9e` #3440 shroud (D3D8 texture paths), `f51097de5` / `116fbcfd3` path-separator helpers |
| Obsolete in our architecture | D3D8 device-reset fixes (`4f71d3791`, `dd161b047`, `169b42f38`), `ab568af44` / `a36928dc6` / `fd62135da` Renderer refactor, `78ba6ea6f` D3D8 texture cache shutdown, `0e6a06c7c`, `fb2e2f0b5` D3D8 screenshot surface |
| Incompatible / dangerous for now | the deterministic-math series `6f14ae8da`..`91baad276` (#2670: new CRC unless `RETAIL_COMPATIBLE_CRC`), `b1fec67c7` #3613 XferCRC byte-swap removal (perf; must prove identical CRCs first) |
| Not relevant | wdump / World Builder tools, MinGW, unify refactors, docs / CI |

## 10. What was done in this milestone (commits)
`e2ee7e3d7` pointer truncations (#3459) - `0b2452b24` / `762892bc7` hitch instrumentation - `fb5dc706f`...`bebdc4835` upstream UB / alignment ports - `1b1caf5e8` D3D11 flip model - the leak / crash ports - this document.

## 11. Remaining risks and human validation
* **Real two-PC validation of `2071ad765` (multiplayer smoothness) is pending.**
* Flip-model swap chain on other GPUs / drivers and in all window modes (windowed, borderless, mode switches, monitor change): automated runs and a native capture pass on this iGPU only; `GENERALS_D3D11_BLT=1` is the immediate fallback.
* No device-loss recovery (TDR is fatal); no input-latency measurement (frame-latency cap left at the DXGI default).
* The 31 ms terrain full update and the first-use asset hitches are measured and attributed but not fixed; the simulation tail (P-7) has no exact shortcut.
* The 550 ms frames did not reproduce; instrumentation is in place.
* Upstream leak / crash ports are build- and gate-tested, not exercised by a LAN / GameSpy session here.
