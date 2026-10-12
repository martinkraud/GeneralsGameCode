# X64.19 - actual original-menu sentence producer gate

## Scope and source-backed integration decision

Generals X64.18 is accepted from reviewed fix1 run20261012T1421514563884Z;
see X64_18_UI_IMAGE_GATE.md for complete evidence,identity and size audit.
The actual Render2D producer preserves the original287x94 logo extent and UVs.
No scaling defect was found. Exact reference pixel placement was not validated:
X64.18 headless bias=false leaves a half-pixel translation. This next gate scopes
bias=true,as ordinary graphical W3DDisplay::init does,and restores it on cleanup.

Actual remaining menu routes were audited: W3DDisplayString::draw builds
Render2DSentenceClass geometry, draws shadow/text, flushes display batching,
then Render() creates TextureClass snapshots from pending ARGB4444 glyph
surfaces, assigns clamp/point/no-mip samplers and submits real Render2D quads.
The existing D3D11 sentence/content bridge supports those exact states and
immutable residency. Prior isolated GPU tests manually assembled quads around
GDI glyph content; they did not execute the engine's actual sentence producer.
This is a reachable integration gap worth resolving before window-manager work.

W3DFontLibrary normally gets FontChars from the global asset manager and adds
alternate Unicode fonts. The bounded gate instead owns its font/sentence
privately. This avoids introducing global GPU-backed sentence resources whose
normal lifetime currently extends past diagnostic device detach. It uses the
actual existing FontChars/Render2DSentenceClass algorithms,not a replacement
text renderer. General DisplayString/font-library/hotkey/wrapping/Unicode and
window-manager integration remain future work,not claimed by this probe.

Original staged Generals Window.big/MainMenu.wnd contains:

- StaticTextSelectDifficulty: rect(540,116)-(756,152),creation800x600;
- original label GUI:SelectDifficulty,font Arial,size14,bold0;
- enabled text RGB255,190,1 with alpha255;
- the item belongs to the difficulty panel and is initially HIDDEN.

The probe fetches the actual localized label through TheGameText,not a canned
production string,and draws it at the rectangle's top-left with matching font,
color and clip. This is explicitly a sentence producer probe. It does not
construct/unhide that widget or claim its margins,centering,shadow,wrapping,
visibility or full-menu state. It does not redesign the original UI. The
original logo remains at its accepted rectangle. No synthetic production asset
or substituted caption/font is used. Missing label/font/geometry or an extent
that does not fit216x36 fails closed. English stage assets are the reviewed
reference; arbitrary language/layout coverage is not implied.

## Implementation and ownership

Strict opt-in AMD64 grammar:

    -headless -nativeUISentenceSmoke <fresh isolated run directory>

Old nativeInitOnly/nativeGraphicsSmoke/nativeDisplaySmoke/nativeUIImageSmoke
modes remain independent; Win32 rejects the new flag. UISentence implies the
image/display contracts in tooling and routes after normal headless engine init,
before execute/replay/simulation. W3DDisplay/WW3D remain shared game-facing owners.
The same checked diagnostic HWND,hardware D3D11 backend,fixed800x600 target,
blue clear,normal Begin/End/Present and bounded5-second/300-frame host are reused.
No silent WARP fallback,DX8 emulation,fake device,unsupported-state no-op,
app-local graphics DLL,factory replacement or ordinary display draw is added.

Within attachment: enable graphical UV bias; prepare original logo/helper;
resolve original text; initialize private GDI FontChars using diagnostic HWND;
build actual sentence/ARGB4444 glyph surfaces; check extents; set original clip;
create geometry once. Each frame draws the logo and calls sentence->Render().
First Render builds TextureClass CPU snapshots,uploads actual residency and
submits the existing Render2D producer. Later frames reuse its resources.
No new shader/decoder/compressor or production font algorithm is introduced.

New flushed checkpoints: ui_pixel_policy,ui_sentence_load begin/complete,
ui_font_create begin/complete,ui_sentence_build begin/complete with original
label/font/rectangle,positive extent/character count; ui_sentence_draw
begin/complete; ui_sentence_release. Typed engine load errors retain ErrorCode.
Existing device/frame/Present/cleanup/late-exception evidence remains mandatory.

Shutdown on success and partial failure deletes sentence first (all glyph,
texture and residency references),releases private font,restores UV bias,then
releases image/helper,restores screen size,detaches backend,then host destroys
HWND/class. Normal engine/MiniDumper/late-observer/CRT cleanup follows. Actual
OS exit0,empty shutdown evidence,no dump,visible unoccluded S_OK Present and
bounded duration remain mandatory. No execute/update/replay/simulation/GameLogic,
Shell::showShell/layout callbacks,world rendering or application input is entered.

## Offline validation

Existing title-linked test harness now includes UIProducerTest:
actual Add_Quad vertices/UVs for original logo,both bias settings,D3D11 correction
and287x94 extent; actual FontChars/Render2DSentenceClass build/draw/clip/reset/
release40 times with production named pools on both architectures/titles.
No parallel test framework,game invocation or artificial allocator is used.
These tests do not Render the actual sentence on a device; existing independent
WARP tests cover glyph conversion/upload/blending and residency. The new manual
gate is required to validate the combined actual producer-to-GPU route.

Native startup tests cover strict new grammar,old-mode distinction,extra/missing
arguments,Win32 rejection and actual flushed isolated AMD64 configuration.
Shared PowerShell classifier checks ordered sentence and inherited image/display
milestones and identity; synthetic negatives remove/reorder/duplicate phases,
change label/font and mix modes. Existing scalar/array postprocessing tests and
no-launch guards remain. Source-preservation tests freeze original DX8/display,
Render2D/sentence/texture behavior and require resource release before detach.

Final results: full Win32 Release build,CTest25/25,152 direct/78 focused tests
per title PASS (six existing disabled direct tests). AMD64 independent CTest15/15,
both native title links and10 engine-linked texture/UI tests per title PASS.
Native startup13 AMD64/12 Win32 tests PASS. Renderer reference30,native/display15,
performance16 Python tests PASS; graphics59/native6 PowerShell guards and all10
PowerShell scripts parse PASS. git diff --check PASS. No title was launched.
Native AMD64 imports remain30 each,no delay or D3D8/D3DX imports; system AMD64
D3D11/DXGI/compiler and matching EXE/PDB hashes/GUID-age preflight required below.

## First reviewed Generals-only procedure

Fresh per-title x64-19-final stages are prepared only after final validation.
Zero Hour is prepared/preflighted but not authorized for execution by this step.
Stage identities and final preservation checks are recorded below.

```powershell
Set-Location 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode'
./scripts/native-runtime/Test-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-19-final -UISentence
./scripts/native-runtime/Invoke-NativeGraphicsSmoke.ps1 -Title Generals -Name x64-19-final -UISentence -RunReviewedGraphicsSmoke
```

Preflight launches nothing. Invoke launches exactly one reviewed Generals process.
Expect a fixed800x600 blue client area,the original logo near the upper right,
and a small yellow original difficulty label immediately below the logo,starting
at(540,116). English assets should show the original localized difficulty wording.
Text must be readable,not blank,corrupt or stretched. This is not the main menu.
About5seconds then automatic close and actual exit0. No clicks/keys/intervention
are needed; do not resize,minimize,close or switch away during the probe.

Evidence: build/dev-runtimes/native/Generals/x64-19-final/runs/<UTC>/
launch.json,exit.json,startup.log,shutdown-exception.bin,userdata/CrashDumps.
Send the exact new run directory and visual observations. If preflight fails,
stop before Invoke. On launcher error,missing/wrong logo or text,abnormal size,
crash or timeout,preserve everything and stop. No rerun,Zero Hour launch,
rebuild/restage or workaround before evidence review.

Exactly one recommended next task: separately reviewed Generals-only X64.19
actual original sentence/image confirmation,followed by preserved evidence and
visual review. This native/visual execution is a mandatory boundary; it is not
performed during offline implementation.

## Remaining original-main-menu blockers

Additional solid/additive/grayscale states must be implemented only from their
reachable original UI contracts; current unsupported states still reject.
Original atlas/widget coverage,custom Generals font registration/verification,
DisplayString/hotkey/wrapping/Unicode/cache lifetimes,window-manager layout and
callback isolation remain. Shell::showShell queues a shell map and mutates RNG;
ordinary W3DDisplay::draw reaches DX8/world paths; GameEngine::update runs engine
updates before its message pump. None is a safe menu-only shortcut. Application
HWND/input/focus callbacks are not migrated by the separate diagnostic host.
Full original menu composition,normal input,world/gameplay,save/load,replay,
multiplayer,performance and determinism remain unvalidated. Keep DX8 Win32 as
reference and retain the selected shared W3DDisplay/WW3D architecture.

## Final isolated stage identities and preservation

Both per-title x64-19-final stages passed complete current-build/AMD64 EXE/PDB,
GUID-age/hash,asset inventory and system graphics dependency preflight. Wrong
UIImage-only mode rejects. Neither has a runs directory. No title was launched.
Both import sets remain30,no delay imports,no D3D8/D3DX or app-local graphics DLL.

- Generals: GUID `c7323867-d82e-41f3-b190-20a5da4eae06`,age53; EXE SHA256 `9FAE08C0A44BFDBBCF92602A2E706CE9576DF4EB91D7034310CD0CE522CB2BA2`; PDB SHA256 `E2EA93627A2D1B48A86E9800F4FAD0D9FB435E3A3DE9F219BF6E6D57EE9D27F0`.
- ZeroHour: GUID `7a285453-4110-4cad-a6c8-e5336ba19b1c`,age54; EXE SHA256 `751E535C697058A246895568BDA37966E30AEFFBB0F2E921AA81FA669BB3FB37`; PDB SHA256 `11D8A8EE21F489201B8AF01F26CE3A49A8B0AE162C6C401814E7CFF006B9E76B`.

All35 inventoried accepted/failed evidence files rehash unchanged,including the
reviewed X64.18-fix1 run. All16 staged EXE/PDB copies for both titles across
X64.16-final-2/X64.17-final/X64.18-final/X64.18-fix1 match their original manifests.
The failed X64.18 run is neither mutated nor reclassified. Reviewed fix1 staged
PE and PDB GUID/age were also parsed independently and match launch.json exactly.
Generated stages/assets/evidence/helpers/build logs remain ignored and excluded
from commits. This coherent source/test/docs change follows ac38a1c5b on
dev/modern-engine; only origin/dev/modern-engine is authorized for push.
