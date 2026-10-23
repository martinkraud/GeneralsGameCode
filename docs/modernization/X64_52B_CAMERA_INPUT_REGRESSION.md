# X64.52B - Camera / input regression investigation (X64.52A failed its first human native test)

Status: **edge scrolling FIXED and proven with real operating-system input; camera judder NOT reproduced in the camera path - the X64.49 mechanism is intact (byte-identical code) and proven smooth with real input at 60 / 120 / 240 FPS; ready for human native verification.**
Start point: HEAD `acab006f4` = `origin/dev/modern-engine`.

## 1. Report

Zero Hour X64.52A, ~120 FPS: small regular steps ("30 FPS-like") while moving the camera with the arrow keys and with RMB; moving the mouse to the screen edge no longer scrolls at all.

## 2. Evidence

### 2.1 What X64.49 changed and whether it is still there
X64.49 (`c74890a7e`): `LookAtTranslator` (MSG_FRAME_TICK) hands the held-scroll *velocity* to `View::userScrollByPresented`; `W3DView::update` calls `View::applyPendingUserScroll()` before the camera transform of the drawn frame and integrates `velocity x 30 x wall-clock seconds since the previous update`. Key, RMB and edge share that call.

`git diff c74890a7e..acab006f4` over everything that can influence this behaviour is **empty**: `LookAtXlat.cpp`, `View.cpp/.h`, `W3DView.cpp`, `W3DDisplay.cpp`, `PresentationScroll.h`, `FramePacer`, `GameEngine.cpp`, all of `Core/Libraries`, `Core/GameEngineDevice` except the developer-only `W3DDisplayOrdinaryHost.cpp` (autoflow audit code), `Win32Device` (mouse / keyboard), `Mouse.cpp`, `OptionPreferences.cpp`, the CMake presets / flags. The commits since X64.49 are X64.50 (docs), X64.51 / X64.52 (AI), X64.52A (Match Options, Superweapons, distribution scripts). **No later commit changed, bypassed or reverted the X64.49 camera path.**

Important record correction: X64.49 was **never recorded as a human PASS** (`AGENT_HANDOFF.md`, `FIRST_PLAYABLE_ROADMAP.md`: "OPEN: fix + automated validation done, human retest pending"). Its own document lists "render / present pacing at the top rates" as *not fixed*. So "previously accepted" is not supported by the repository; what is proven is that the code is unchanged.

### 2.2 Why the X64.49 probe and the X64.52A gates could not see the edge
`Invoke-CameraProbe.ps1` drives the translator's *scroll state* (`setScrolling(SCROLL_SCREENEDGE)`, `m_currentPos` forced to 0) - it never goes through the operating system, `Mouse`, the cursor capture or the edge rule. Dead real edge scrolling and a good probe are therefore consistent. The new harness (section 4) sends **real** `SendInput` keyboard / mouse input to the native game.

### 2.3 Edge scrolling - reproduced, root cause
Real input, Zero Hour, `build/diag/camera-real-ZeroHour-cur`: the cursor sits at x = 1 of 1920, `captured=1`, `windowed=1`, but `edgeMode=2` (bit 2 = fullscreen only) -> `canScrollAtScreenEdge()` is false -> the camera travels 0.0 units.
The mode comes from `Options.ini`: `ScreenEdgeScrollEnabledInWindowedApp = no` (present in the test machine's file). The original game and every Win32 build default that key to *no* and the Options screen writes it back; the modern AMD64 runtime *always* presents in a window (`LookAtXlat.h`: "A saved ... = no in Options.ini still wins"), and it has no control to change the key, so one stale value switches the edge off forever. Nothing in X64.50-X64.52A touched this; the edge default of X64.31 only helps when the key is absent.
**Fix**: `ScreenEdgeScroll::allowed(...)` (pure, `Common/ScreenEdgeScroll.h`): in the modern runtime either preference enables the edge; only both = `no` disables it. Classic (non-WIN64) semantics are unchanged. The edge band test (start, stop, direction) is now one function, `ScreenEdgeScroll::hit`, same 3 px band.

### 2.4 Camera judder - not reproduced; what was measured
Real input (SendInput: Left arrow, cursor at the right edge, RMB drag), `PresentationScroll` active, vsync off so a 60 Hz test monitor can run the caps:

| title | cap | key ripple | edge ripple | RMB ripple | stale frames | distance key / edge |
|---|---:|---:|---:|---:|---:|---|
| Zero Hour | 60 | 0.009 | 0.012 | 0.109 (accelerating drag) | 0 | 1580 / 1508 |
| Zero Hour | 120 | 0.038 | 0.026 | 0.099 | 0 | 1575 / 1538 |
| Zero Hour | 240 | 0.026 | 0.018 | 0.046 | 0 | 612* / 1516 |
| Generals | 120 | 0.053 | 0.032 | 0.035 | 0 | 1016 / 1011 |
| Generals | 240 | 0.058 | 0.032 | 0.038 | 0 | 1023 / 993 |

(ripple = mean |speed change between adjacent drawn frames| / mean speed; *the 240 key run reached the map border early, smoothness measured on the available frames.) The camera moves on every drawn frame (stale 0), the step follows the frame interval, and the distance over equal wall-clock time does not depend on the render rate (Generals 1016 -> 1023, Zero Hour 1580 -> 1575 for 60 -> 120).
The same harness with the **former** model (`GENERALS_CAMERA_PROBE_LEGACY=1`, post-draw step sized by the previous interval) measures ripple 1.07 / 0.81 / 1.17 and **fails** the gate - so the gate does detect the defect class, and the current build does not have it. A camera tied to the 30 Hz tick would show 75 % stale frames at 120 FPS (gate: > 10 % fails).
The test PC is a 60 Hz Intel-graphics machine; the tester's 120 FPS display cannot be reproduced here. What automation cannot show: frame *presentation* cadence on a 120 Hz scan-out (the render / present pacing item X64.49 left open), swap-chain behaviour on that GPU, and the subjective impression. **This needs the human native test** (section 6) - and, if the judder persists there, the output of the harness run on that PC (`-ExePath`), which records exactly where the cadence breaks.

### 2.5 Same regression?
No. Edge scrolling is a persisted-preference bug (input path gated off), reproduced and fixed. The judder is a different item: the camera integration is correct and unchanged; any remaining judder is render / present cadence on high-refresh hardware (known open item of X64.49) or a settings combination not present on the test PC.

### 2.6 Introduced by
Edge: the stale value predates X64.52A; the modern default (X64.31) never overrode a saved `no`. The human test was the first time edge scrolling was exercised through real input after that value existed on the machine. Judder: no commit introduced it (code identity), X64.49 itself was never human-verified.

### 2.7 Runtime composition audit
* The staged candidate executables are byte-identical to the freshly built `build/win64` executables (SHA-256 compared; section 5).
* The overlay ZIP of each title contains only the executable, the runtime DLLs, README / licence / manifest (Zero Hour) plus the INI / WND overrides (Generals); the validator proves clean install + ZIP == staged candidate file-for-file.
* The cleanup change only deletes old generated artifacts after a validated distribution and never touches staging inputs; the stale-stage failure it exposed (leftover `x64-48b-overlay-test-*`) was a validator leftover, fixed by the cleanup.
* No stale-binary path was found; the behaviour of the X64.52A binaries is the behaviour of the X64.49 binaries for the camera.

## 3. Code changed (presentation / input only; no simulation, AI, Match Options or superweapon change)
* `Common/ScreenEdgeScroll.h` (new): `hit`, `allowed`; `LookAtXlat.cpp` uses them (start / stop / direction + `canScrollAtScreenEdge`).
* `Common/PresentationScroll.h`: `Integrator` (the pending-velocity state that lived in `View`); `View.h/.cpp` use it - same arithmetic, now unit tested.
* Real-input harness: `CameraProbe.h` / `LookAtXlat.cpp` (`GENERALS_CAMERA_PROBE_REAL`, phases RMB, translator state log, `..._VSYNC=0`), `Invoke-CameraRealInput.ps1`, `analyze_camera_real.py`.

## 4. Permanent regression coverage
* gtest `ScreenEdgeScroll.*` (4): modern runtime ignores a stale windowed = no (the failing case), classic semantics, edge band at 800x600 .. 5120x1440, degenerate sizes.
* gtest `PresentationScrollIntegrator.*` (6): keyboard / RMB-shaped velocities cover the same distance at 30 / 60 / 120 / 240 FPS steady and jittered; equal wall-clock travel; irregular sequence; the camera moves on every render frame (no 30 Hz quantisation); a velocity moves the camera once (no double integration); invalid ratio; diagonal / negative. Together with the six X64.49 tests. gtest totals: Generals 468, Zero Hour 467.
* `test_camera_real_input_gate.py`: the analyzer passes the accepted behaviour at 30 / 60 / 120 / 240 FPS (jittered) and FAILS a dead edge, a 30 Hz camera, a speed ripple, a missing phase; wiring contracts.
* `Invoke-CameraRealInput.ps1` + `analyze_camera_real.py`: the native gate with real OS input (about 40 s per title, needs the desktop). Results in `evidence/x64-52b/`.
* Verified failing on the broken candidate: `camera-real-ZeroHour-cur` and `-v120` (built before the fix) FAIL the gate (edge travelled 0.0).

## 5. Runner
`scripts/native-runtime/Invoke-RegressionGates.ps1` chains the permanent gates: Python contracts and unit gates (always), `-GoogleTest` (built gtest executables), `-Native -GeneralsCandidate .. -ZeroHourCandidate ..` (real-input camera gate at 60 / 120 / 240 FPS per title). Run it before every milestone's candidate packages.

## 6. Clean build, packages and what is still unproven

* Clean rebuild (`cmake --build --preset win64 --clean-first`, both titles, 3733 steps, 0 errors) at source commit `f38ba6418`; gtests Generals 468 / Zero Hour 467 passed; Python suites passed; `build/dist` was empty (the tester cleared it), the distribution ran from scratch: both titles built from the clean executables, packaged, validated (26 of 26 overlay checks per title, clean install + ZIP == staged candidate file-for-file) and the retention cleanup ran against a state with **no previous distribution** (kept the new one, removed only older generated stages, fabricated nothing).
* Provenance: the `Generals.exe` inside each ZIP is byte-identical (SHA-256) to the freshly built `build/win64` executable of that title (Generals `6a0a745d...`, Zero Hour `1d306c27...`); no PDB in either player ZIP; symbols are separate archives.

| file | size (B) | SHA-256 |
|---|---:|---|
| `MKRaudstein-Generals-x64-Preview-f38ba6418.zip` | 4 051 258 | `EF650EE7DBACA3D7CDA6A43F06427C24FE35594678AF19601FBA02B7A84A5488` |
| `MKRaudstein-ZeroHour-x64-Preview-f38ba6418.zip` | 4 157 412 | `A4AD433DEB6CB354AA9C13C5A21460A0A3DA104AA54F678E3E4A6565BE490208` |
| `MKRaudstein-Generals-x64-Preview-f38ba6418-Symbols.zip` | 22 221 482 | (manifest) |
| `MKRaudstein-ZeroHour-x64-Preview-f38ba6418-Symbols.zip` | 22 701 031 | (manifest) |

* **The real-input camera gate on exactly these clean executables is INCONCLUSIVE**: the workstation was locked (LogonUI running, no foreground window) when the clean build finished, so no window could take the foreground and the mouse phases cannot be judged. The gate passed (3 phases, zero stale frames) on an incremental build of the identical source before the clean build: Zero Hour 60 / 120 / 240, Generals 120 / 240 (`evidence/x64-52b/gate-*-fix*.txt`). The driver now reports exit code 3 / INCONCLUSIVE instead of a false FAIL. Re-run on the unlocked desktop: `pwsh scripts/native-runtime/Invoke-RegressionGates.ps1 -Native -GeneralsCandidate dist-f38ba6418-gen -ZeroHourCandidate dist-f38ba6418-zh`.
* Automation cannot establish: the frame presentation cadence on a 120 Hz scan-out and the subjective smoothness; whether the tester's judder comes from the render / present pacing (open since X64.49) or from a settings combination absent on the 60 Hz test PC.

### Human native test (ready for human native verification)
Launch `pwsh scripts/native-runtime/Run-ZeroHourCandidate.ps1 -Name dist-f38ba6418-zh` (`build\dev-runtimes\native\ZeroHour\dist-f38ba6418-zh\game\generalszh.exe`; Generals `build\dev-runtimes\native\Generals\dist-f38ba6418-gen`) or install the two ZIPs over Steam as before.
1. **Screen edge** (the fix): in a Skirmish move the mouse to each of the four screen borders: the camera must scroll in that direction, also if your `Options.ini` still contains `ScreenEdgeScrollEnabledInWindowedApp = no`.
2. Hold an arrow key 3 - 5 s, then drag with the right mouse button: judge the smoothness at your 120 FPS setting and at 60.
3. If the judder is still there, run the measurement on your PC and send the folder: `pwsh scripts/native-runtime/Invoke-CameraRealInput.ps1 -Title ZeroHour -ExePath <path to the installed generalszh/Generals.exe> -Fps 120` (needs the desktop for ~40 s: do not touch keyboard / mouse), then `python scripts/native-runtime/analyze_camera_real.py build/diag/camera-real-ZeroHour-real`; also send your `Options.ini`.
4. Everything X64.52A listed (Match Options, Superweapons, AI Behaviour, Starting Cash) is unchanged.

## 7. Audit of the existing gates against the accepted invariants

"Behavioural" = executes the engine code or the game; "static" = reads source text (pins that something is wired, proves nothing about what it does).

| Invariant | Permanent gate(s) | Kind | Verdict |
|---|---|---|---|
| Native AMD64 startup / runtime | distribution validator (starts the packaged exe in a clean disposable install, reaches the menu, drives the Challenge screens, renders and captures a frame, no failed phase); `Invoke-NativeNormalLaunch` / `OrdinaryGame` flows; `test_startup_contract`, `test_normal_launch_contract` | behavioural + static | adequate |
| D3D11 path | same validator frame capture and feature probe; D3D11 stage scripts | behavioural (renders; no pixel baseline) | adequate for "works", not for visual regressions |
| Original / default behaviour preservation | `Invoke-AiBehaviorGate.ps1`: FNV digest of the GameLogic CRC every 30 frames, new executable vs a reference executable, Enhanced repeatability | behavioural, strong | adequate; needs the previous milestone's executable as the reference (kept in `build/baseline-x64-51`) |
| Modern Display / resolution handling | gtests `DisplayGeometry*`, `DisplaySettings*`, `DisplayTransaction*`, `RenderScale*`; `Invoke-MatchOptionsAudit.ps1` (18 resolution / scale configurations, overlap + reachability); `test_modern_display_contract`, `test_ultrawide_contract` (static) | behavioural + static | adequate |
| Smooth presentation / camera | gtests `PresentationClock`, `PresentationSmoothness`, `PresentationScroll` (+ the new `Integrator` tests); `Invoke-CameraProbe.ps1` (synthetic state); **new** `Invoke-CameraRealInput.ps1` | behavioural | **hole closed**: the probe never exercised the keyboard / mouse / edge path, the new gate does |
| Starting Cash | `test_skirmish_match_options_contract` (static); behaviour proven by hand with `Invoke-AiBaseExperiments.ps1 -Cash` fingerprints (X64.52A evidence) | static + manual | **open**: no automated behavioural gate; not touched by this incident |
| AI Behaviour Original / Enhanced selection | gtest `AIBehavior*` (option string round trip, strict parsing), `AIEnhancedBase*`; contracts; golden gate | behavioural (pure) + static | adequate for the rule; the drop-down itself is audited manually by `Invoke-MatchOptionsAudit` |
| Enhanced replay revision (`AI=E2`) | gtest `AIBehavior.OptionValueRoundTripAndStrictParsing`, contract, headless replays | behavioural | adequate |
| Superweapon Match Option | gtest `SuperweaponLimit*`; `test_superweapon_limit_contract` (static); the dynamic matrix (`make_sw_aids.py` / `check_sw_events.py`, 20 runs) is a documented manual headless run | mixed | adequate; the dynamic matrix is not in the quick runner |
| `-classic` | static contracts (`test_normal_launch_contract`, `test_modern_settings_contract`, `test_skirmish_match_options_contract`) and `DisplaySettings` gtests | mostly static | **open**: no behavioural native test |
| Campaign / Challenge isolation | static contracts; Original golden gate (Skirmish only) | static | **open**: no behavioural test |
| Deterministic simulation / replay | gtest `DeterminismFingerprint*`; golden gate; replay repeatability | behavioural, strong | adequate |
| Distribution / runtime parity | validator (clean install + ZIP == staged candidate file-for-file, overlay run, feature parity), `test_overlay_package_contract`, `test_distribution_cleanup` | behavioural | adequate |
| Persisted preferences vs. defaults (new class) | `ScreenEdgeScroll` gtests; the other `OptionPreferences` getters were reviewed: only `ScreenEdgeScrollEnabledIn*` carries a `_WIN64`-dependent default, so it is the only stale-value hazard of this kind | behavioural | closed for the one instance found |

Added because of this incident: the real-input camera gate (+ analyzer and its failure-class tests), the `ScreenEdgeScroll` rules and `PresentationScroll::Integrator` as unit-tested code, and the runner. Not added (not exposed by this incident, listed so they are not forgotten): a behavioural Starting Cash gate, a native `-classic` / Campaign / Challenge gate.
