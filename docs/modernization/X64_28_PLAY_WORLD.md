# X64.28 - play world (CP1 of FIRST PLAYABLE)

Status: offline batch complete, candidate staged and preflighted, **not executed**. Awaiting the developer's
native run. Plan and dependency graph: `FIRST_PLAYABLE_ROADMAP.md`. Current state: `AGENT_HANDOFF.md`.

## 1. Scope

Adds the fifth strict profile `-nativeOrdinaryPlay` (Generals AMD64 only; Win32 and Zero Hour reject it).
It implies `-nativeOrdinaryTerrain` (and Game/Menu/Host) and adds only:

1. native rendering of W3D world objects (`NativeWorldObjects`) through the D3D11 TerrainOverlay path,
2. native picking against the same registry,
3. the in-game UI draw (`drawNativeWorldOverlays` + `InGameUI::DRAW`) and the full `ControlBar::update`,
4. a user-driven loop (no scripted camera, no logic-frame bound, 60 minute wall-clock bound).

The terrain/game/menu/host tokens, Zero Hour and the Win32 reference keep their behavior
(`test_ordinary_play_contract.py`). `m_headless` stays TRUE; world selection is UI-without-world.

## 2. Renderer summary

- `WorldBlend` enum on the TerrainOverlay material; D3D11 blend states for SrcAlpha, Opaque, Additive,
  AdditiveAlpha, Multiply, InvMultiply, Premultiplied; alpha test reference 0x60.
- Registry fed by `W3DModelDraw`/`W3DDebrisDraw` (else-branch of the legacy scene add/remove; no-op outside play).
- Per frame: frustum cull, drawable hidden/ghost skip, HLod current LOD + additional models, animation step,
  CPU skin deformation, ALIGNED/ORIENTED meshes, CPU vertex lighting (ambient + up to 3 directional from the
  terrain-object lighting, tint, infantry scale), alpha override/stealth, tread UV scroll, team color
  (ZHCD palette / ZHCA hue / HOUSECOLOR), pooled geometry chunks (60k vertices), opaque then back-to-front
  translucent.
- Native static scenery: trees (foliage state) and props, replacing the absent legacy buffers.
- Selection rings: screen-space circle on terrain (green own / red enemy / white other).

## 3. Receipts (in `startup.log`, run directory)

`world_objects_first_frame`, `world_objects_texture_failure` (per name, bounded), `play_progress`
(15 s x3 then every 60 s: logic frame, CRC, money, selection, camera, objects drawn, input counters),
`play_summary`, `world_objects_summary`, `play_frame_N.bmp` (3 composed frames), plus all terrain receipts.
Log capacity is 2048 records and fails closed; play receipts are capped (`playReceipts < 120`).
Exit reasons: `game_ended`, `game_quit`, `boundary` (shell push, e.g. score screen), `time_bound`.

## 4. Validation evidence (commit `d9e595ce2`, rerun after the final distance-sanitize edit)

| Check | Result |
| --- | --- |
| Win64 `g_generals z_generals g_googletest d3d11_ui_test native_startup_gate_test` build | pass |
| Win64 ctest | 17/17 |
| Win32 full build + ctest (reference intact) | pass, 25/25 |
| `g_googletest` with offline asset env vars (169 tests, 6 disabled) | 169 pass |
| Real-model WARP render (tank, humvee, barracks, skinned ranger, foliage), team colors | reviewed images, 0 texture failures |
| Skinned animation advance through native path | 7184 differing bytes between frames |
| Registry ray cast: 4 objects hit, sky misses | pass |
| All 37,350 retail W3D prototypes | 36,861 rendered, 52 intentionally empty, 0 exceptions, 0 chunk overflow, 11 missing texture names (white fallback) |
| Texture census (Patch.big terrain) | DXT1/3/5 DDS + 24/32 bpp TGA only |
| Python contracts (7 files), PowerShell workflow tests (terrain/play/game/menu/host) | all pass |

Offline evidence is not visual acceptance. No game executable was launched by the agent.

## 5. Candidate

| Item | Value |
| --- | --- |
| Name | `x64-28-play-world` |
| Path | `build/dev-runtimes/native/Generals/x64-28-play-world` |
| Source commit | `d9e595ce2bef2c9cb166c0f70c9637582cf762e8` (documentation commit follows; code identical) |
| `generalsv.exe` SHA-256 | `58F5EF9515EA3BAB7B702AED64445ECF46CB2F1D8C071763678D1CD0754ADE4C` (7,526,912 bytes) |
| `generalsv.pdb` SHA-256 | `B2B0553041858970ACE2DFADF3123FF598D709D8757EBFFF8588DAEA342B4145` |
| `manifest.json` SHA-256 | `CF8FD8E1534378AE861266BF8032949AFC8D7F0C94193361588228DA50D825CB` |
| `ordinaryplay.json` SHA-256 | `40B0748E1A5D508A1CA4F9A72D7C0716F01778DA48A0E5F644838EC9DA89CC24` |
| Stage/preflight | `Stage-NativeOrdinaryPlay.ps1` + `Test-NativeOrdinaryPlay.ps1` PASS (AMD64, no D3D8/D3DX imports) |

Run (developer only):

```powershell
pwsh -NoProfile -File scripts/native-runtime/Invoke-NativeOrdinaryPlay.ps1 -Title Generals -Name x64-28-play-world -RunReviewedOrdinaryPlay
```

Select **Lone Eagle** in Skirmish (bridge/shoreline coverage still owed from X64.27). Quit through the menu.

## 6. Expected

Terrain as X64.27; player base with buildings, team-colored units, trees; original Control Bar whose command
card changes with selection; health bars on selected/damaged units; selection rings; RMB scroll / arrow keys /
wheel zoom; box select; move/attack orders; building placement and production; AI base activity.

## 7. Known omissions (by design, fidelity backlog)

Water; shadows; particles and weapon/explosion effects; tracers, lasers, projectile streams, ropes
(invisible, guarded); shroud/fog and fogged ghosts; radar minimap (empty bezel); bibs, terrain tracks, scorch
marks; waypoints; tree sway/topple; specular, env map, multi-pass, MODULATE2X, dynamic lights; score screen
(shell push ends the run); audio, video.

## 8. Watch list

- Black/white/untextured objects (texture failures; see `world_objects_texture_failure`).
- Inside-out or missing faces, mirrored models; wrong team color.
- Frozen treads/animations; units not selectable or picking offset; rings misplaced.
- Control Bar context not updating or dead buttons; build/production not progressing.
- Camera unresponsive (compare `play_progress` cam/keys/left counters).
- Trees missing or z-fighting; translucent sorting artifacts, stealth fade.
- Frame hitches (CPU lighting/skinning); crash at first tracer/laser/police car.
- Run ending early with `boundary`/`menu_exit`; log capacity approaching 2048.
