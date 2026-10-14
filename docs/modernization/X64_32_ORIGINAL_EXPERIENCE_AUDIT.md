# X64.32 — Original experience completion and frontend / QoL audit

Direction: *original Command & Conquer: Generals gameplay and visual identity on the modern AMD64/D3D11 engine,
with carefully selected QoL and frontend improvements.* Decision order for every candidate: (1) keep a good
Generals implementation, (2) adapt what Zero Hour improved, (3) design something that belongs in the Generals UI.
No balance, stat, price, timer or AI change is made anywhere in this milestone.

Evidence sources: `evidence/x64-31-accepted/` (the accepted fix5 normal-launch log, 1,100+ checkpoints, exit 0),
the human review reports of fix1–fix5, the retail Generals archives (`ZH_Generals/*.big`) and Zero Hour archives
(`*ZH.big`) read directly, and the repository source (Generals vs `GeneralsMD`). Nothing here claims native proof
that the evidence does not contain. Classes: **A** proven working · **B** implemented, native-unverified · **C**
known incomplete · **D** known broken · **E** intentionally deferred.

## 0. X64.31 closeout

Accepted by native review of `x64-31-fix5` (HEAD `2dcdc6ed7`, in sync with `origin/dev/modern-engine`): normal
executable launch, frontend, full Skirmish match with victory flow and return to the menu, a second match in the
same process, Campaign start and the USA intro/mission through first combat, D3D11 world rendering, original
audio (SFX, voices, positional stereo, music), Daisy Cutter / A-10 / supply and crate drops, Starting Cash
extension. Repository evidence shows no unresolved X64.31 blocker; known remaining items are carried below, not
open X64.31 defects. Accepted evidence: `evidence/x64-31-accepted/` (fix5 log + receipt, exit 0, no dumps,
`audio_stats` over a long session: `starved=0`, completions == delivered, 15,313 completions).

## A. Original-experience completeness (native path)

| Area | Item | Class | Evidence / note |
|---|---|---|---|
| Startup | normal executable launch, init, clean shutdown | **A** | fix5 log: exit 0, no dumps; repeated across 5 reviews |
| Startup | splash / logo / intro movies | **C** | Bink (32-bit only) — skipped, not faked |
| Frontend | main menu, shell map, buttons, music | **A** | reviews fix1–5 |
| Frontend | Options screen (all original settings) | **B** | original code + frame-rate selector; display modes list is the single 800x600 mode; never exercised in a review |
| Frontend | Skirmish setup (map, faction, team, color, slots, AI difficulty) | **A**/**B** | map/faction/color/team proven; AI difficulty/slots per-combination **B** |
| Frontend | Starting Cash extension | **A** | accepted fix5 |
| Frontend | Single Player / Campaign selection, difficulty | **A** (USA) / **B** (others) | USA campaign started; China/GLA and difficulty variants unverified |
| Frontend | Load screen, score/end-game screens, return to menu | **A** | match end → score → menu, twice |
| Frontend | Save, Load | **B** | original code; two Zero Hour fixes ported this milestone (Save/Load dialog); never exercised natively |
| Frontend | Replay (record on game end, Replay menu, playback) | **B** | recorder initialized in every log; menu/playback unverified |
| Frontend | LAN / online lobbies | **E** | UI present, networking not modernized by design |
| Frontend | Credits, quit/exit | **B**/**A** | exit **A**; Credits **B** |
| Gameplay UI | command bar, production, selection | **A** | played matches |
| Gameplay UI | radar / minimap | **A** | X64.30 accepted |
| Gameplay UI | shortcut special-power sidebar | **A** (retail content) | works; retail content is 5/5/3 slots (see C) |
| Gameplay UI | superweapon timers / building powers | **B** | AI/Player powers fire; building-selected firing of Particle/Neutron/SCUD not exercised |
| Gameplay UI | general powers purchase and use | **A** | A-10, Daisy Cutter, Paradrop, Spy Drone, Crate Drop used |
| Gameplay UI | control groups, pause, game speed | **B** | original code; slider intentionally hidden (render FPS lives in Options) |
| Gameplay | victory | **A**; defeat | **B** |
| Campaign | mission start, scripted sequence to first attack | **A** | USA mission 1 |
| Campaign | cutscenes / briefing videos | **C** | no movie decoder on AMD64 |
| Campaign | mission transitions, campaign save/load | **B** | |
| Audio | music, SFX, voices, 3D positional, long-session stability | **A** | fix5 `audio_stats` |
| Audio | movie audio | **C** | with video |
| Rendering | world, terrain, shroud, particles, water | **A** | X64.30/31 |
| Rendering | selection ring fidelity | **E** | renderer-fidelity backlog |
| Input | edge scrolling, keyboard scroll, radar navigation | **A** | |
| Input | double-click attack move, retaliation toggle | **C** | engine support exists; no Options control (ZH has them) — class 3 below |

## B. Generals vs Zero Hour frontend/QoL comparison

Method: file-by-file diff of `.../GUICallbacks/Menus` (Generals vs `GeneralsMD`), WND geometry and names from
`Window.big`/`WindowZH.big`, INI/command data from `INI.big`/`INIZH.big`. Classes: **1** original already
sufficient · **2** missing/broken original behavior · **3** ZH QoL suitable for adoption · **4** ZH gameplay/content —
do not adopt · **5** possible Modern Experience feature.

| # | Zero Hour change | Where | Class | Decision |
|---|---|---|---|---|
| 1 | Starting Cash combo (4 choices, default from MultiplayerSettings) | Skirmish/LAN | **3** | Adopted in X64.31 (own native control, Generals default, extension gate) |
| 2 | Shortcut sidebar up to 11 slots, `*FromShortcut` buttons, superweapons/CIA/Spy Satellite/Radar Van on the bar | PlayerTemplate, CommandSet, GenPowersShortcutBar*.wnd | **3** | Adopted as a data overlay this milestone (section C) |
| 3 | Save/Load dialog: kill the delete confirmation when leaving with ESC (retail "Patch 1.01" crash fix) | PopupSaveLoad | **2** | Ported (crash fix) |
| 4 | Save/Load dialog: strip a trailing ".xxx" from the default save description | PopupSaveLoad | **3** | Ported |
| 5 | Options: "Double-click attack move" checkbox (`m_doubleClickAttackMove`) | OptionsMenu | **3**/**5** | Engine support present in Generals; control belongs to the Options migration (E) |
| 6 | Options: "Retaliation" checkbox (`m_clientRetaliationModeEnabled`) | OptionsMenu | **5** | Alters how orders are issued after attacks — keep off the default path; candidate for Gameplay settings |
| 7 | Options: "Heat effects" checkbox | OptionsMenu | **3** | Visual toggle; native renderer has no heat distortion yet — adopt with the renderer feature |
| 8 | Skirmish map select: tooltip for brutal AI success (`MapMaxBrutalSuccess`) | SkirmishMapSelectMenu | **3** | Cosmetic; needs the ZH string — low value, deferred with map-list work |
| 9 | Limit Superweapons checkbox (`m_superweaponRestriction`, ZH data uses `DeterminedBySuperweaponRestriction`) | Skirmish/LAN | **4** | Engine plumbing is shared, but Generals data has no restricted objects — would need data/gameplay change |
| 10 | Generals Challenge (ChallengeMenu, ScoreScreen portraits/remarks, main-menu button) | MainMenu, ScoreScreen | **4** | New campaign content; not Generals |
| 11 | "War School"/Academy advice list on the score screen | ScoreScreen | **3**/**5** | `AcademyStats` already exists in Core; surfacing it is a possible Modern feature (needs the ZH WND controls) |
| 12 | LAN/GameSpy menu revisions, PopupPlayerInfo overhaul, WOLQuickMatch | WOL*, Lan* | **E** | online/LAN out of scope |
| 13 | Keyboard options, QuitMenu, Skirmish map select layout | — | **1** | no functional difference |

## C. Special-power / superweapon shortcut sidebar

**What retail Generals exposes** (`ZH_Generals/INI.big`: `CommandSet SpecialPowerShortcut{USA,China,GLA}`,
comment "A max of 5 controls are supported at this time"; `GenPowersShortcutBar*.wnd` have 5/5/3 slot pairs):
USA — Spy Drone, Paradrop, A-10 Strike, Emergency Repair, Daisy Cutter; China — Cluster Mines, Artillery Barrage,
Cash Hack, Emergency Repair, EMP Pulse; GLA — Rebel Ambush, Emergency Repair, Anthrax Bomb. Generals' own
`SpecialPower.ini` has no `ShortcutPower = Yes` flags; the sidebar is satisfied by the presence of a Command
Center (`canShowSpecialPowerShortcut`, a TheSuperHackers adaptation).

**Why Particle Cannon / Nuclear (Neutron) Missile / SCUD Storm are missing:** original Generals limitation (class
2 "improved by Zero Hour"), not a regression and not an intentional exclusion. Their buttons in Generals are plain
`SPECIAL_POWER` buttons that only exist on the building's own command set. Zero Hour added `FromShortcut` buttons
and enlarged the bar to up to 11 slots (48x41 slots on a 35 px pitch) with `SpecialPowerShortcutButtonCount`
5→10 per side; ZH's lists add: USA Particle Cannon, Spy Satellite, CIA Intelligence (+ Spectre Gunship, Leaflet
Drop); China Neutron Missile (+ Carpet Bomb, Frenzy); GLA SCUD Storm, Radar Van Scan (+ Sneak Attack, GPS
Scrambler). The parenthesized items are Zero Hour content and are not adopted.

**Implemented (data overlay, no engine/mechanics change):** `scripts/native-runtime/modern_extensions.py`
derives, from the player's installed Generals archives at staging time, loose override files:

| Faction | Retail slots → new | Added (Zero Hour order) |
|---|---|---|
| USA | 5 → 8 | Particle Uplink Cannon, Spy Satellite, CIA Intelligence |
| China | 5 → 6 | Neutron Missile |
| GLA | 3 → 5 | SCUD Storm, Radar Van Scan |

* new buttons are clones of the retail Generals buttons with `Command = SPECIAL_POWER_FROM_SHORTCUT` and
  `NEED_SPECIAL_POWER_SCIENCE` added (Zero Hour's rule: shown only when an object that has the power exists, hidden
  when the science/upgrade is missing); labels, tooltips, images, cursors, targeting radii are the Generals ones;
* the Spy Satellite uses the retail `…FromCommandCenter` button that was never on the bar;
* the bars are regenerated on Zero Hour's slot geometry, slot 1 on top as in retail, bottom-anchored;
* **unchanged:** cooldowns, ownership, science requirements, targeting, `findMostReadyShortcutSpecialPowerOfType`
  semantics (most ready instance wins — multiple instances of one superweapon are handled by the existing code),
  command messages (so replays/multiplayer determinism are unaffected), Generals art and strings.
* classic = stage with `-Classic` (or delete the six generated files); `-classic` additionally switches off the
  code-level extensions (Starting Cash).
* tests: `test_modern_extensions.py` (clone/patch logic on synthetic text, geometry vs the ZH archive, retail
  build, set/button consistency). **Native visual verification is required** (slot proportions, hit boxes,
  tooltip placement, interaction with the superweapon timers at the top right, which Generals draws at
  x = 0.96·width; the 8-slot US bar starts at y = 143).
* Not added (Generals powers that exist but ZH's own bars never listed): Carpet Bomb, Crate Drop, Napalm Strike,
  Terror Cell, Black Market Nuke, Defector — see candidates in §F.

## D. Options / Modern settings architecture

Existing architecture (Generals `OptionsMenu.cpp`, 1,700 lines; `OptionsMenu.wnd`, 88 windows):
one modal screen with two layouts (basic + "Advanced" popup), controls looked up by name at init, state read
from `GlobalData`/`OptionPreferences` (`Options.ini` in the user-data directory, key/value text) and written on
Accept. About 50 keys are persisted today (audio volumes/provider/speaker, resolution, gamma, texture reduction/filter,
anisotropy, antialiasing, shadows, LOD/particles, scroll speed, screen-edge-scroll per windowed/fullscreen,
alternate mouse, camera in replays, cursor capture ×4, FPS limit, HUD font sizes, language filter, firewall…).
The project already extends it natively without asset overrides (`FrameRateOptions.cpp`: runtime-created combo +
caption cloned from existing controls).

Zero Hour: same structure, plus Double-click attack move, Retaliation, Heat effects (checkboxes at
(387,302), (160,326), (384,240) in the 800x600 layout), `Unlock FPS` moved. It did not change the architecture.

**Plan** (visual language stays Generals: MinorTitle headers, ComboBoxEntry/TextEntry templates, `GUI:` strings,
the existing push-button art; layout may grow beyond 800x600):

| Category | Content | Source today |
|---|---|---|
| DISPLAY | resolution list from the device, aspect ratio, monitor, refresh, windowed / borderless / fullscreen, VSync, FPS limit, UI scale | currently only 800x600 windowed (review scale env var) |
| GRAPHICS | detail preset, shadows, particles, texture reduction/filter/aniso, AA, water, heat effects | existing keys |
| AUDIO | music/SFX/voice volumes, provider/speaker, Modern audio options | existing keys |
| CONTROLS | scroll speed, edge scroll, alternate mouse, double-click attack move, keyboard options (existing sub-screen), key rebinding | existing keys + ZH |
| GAMEPLAY | classic/extended camera, replay camera, Starting Cash default, retaliation | extension gate (`ModernExperience`) |

Reusable: WND control library (check box, combo, slider, text entry, static text), the runtime-gadget cloning
helper pattern (FrameRateOptions, SkirmishGameOptions), `OptionPreferences` persistence. Replace: the single
2003-sized modal layout with a category tab strip (new WND authored in the Generals style, or generated like the
shortcut bar); the resolution list (needs a device-mode enumerator behind `IRenderBackend`, not DX8). Classic
defaults: every new key absent ⇒ retail value (no behavior change from existing `Options.ini`); a one-way
migration writes new keys only after the player changes them. `ModernExperience` (`-classic`) is the runtime gate.

## E. Match-setup expansion candidates

Principle (Starting Cash): default stays original; the control only exposes a rule the engine already has.

| Option | Generals default | In ZH? | Engine support | Changes rules? | MP / replay |
|---|---|---|---|---|---|
| Starting Cash (done) | `DefaultStartingCash` 10000 | yes | yes (`GameInfo::m_startingCash`) | exposes existing rule | replay stores game info; CRC unaffected for default |
| Limit Superweapons | none | yes | partial (shared plumbing; no Generals data uses it) | needs data | would change ThingTemplate limits — **no** |
| Random seed / map shuffle | — | n/a | yes | no | deterministic via seed in slot list |
| Handicap / game speed per match | speed fixed 30 TPS | no | no | yes | **no** (logic rate) |
| Fog/shroud, build-radius, crates on/off | retail rules | no | scripts only | yes | no |
| Observer slot, AI names | existing | partly | yes (Observer in PlayerTemplate) | no | LAN-visible |
| Quick presets (e.g. "Classic 10k / Rich 50k") | n/a | no | UI-only over Starting Cash | no | safe |
Only the last two rows are cheap, rule-neutral additions; everything else is gameplay and out of scope.

## G. Video / cutscene path

Current: `Display::playMovie` / `InGameUI::playMovie` / `LoadScreen` / `WindowVideoManager` all go through
`VideoPlayerInterface`; `BinkVideoPlayer` loads `binkw32.dll` (32-bit only) so every `open()` returns null and
callers skip. `FFmpegVideoPlayer` (upstream, `RTS_BUILD_OPTION_FFMPEG`, vcpkg `ffmpeg` feature) already implements the
same interface over libavcodec (which decodes the retail Bink 1 `.bik` files) and `FFmpegFile` reads through
`TheFileSystem` (BIG or loose), so original sequencing is untouched.

What is missing for AMD64 (measured against the source, none requires modifying the installation):
1. **Build:** ffmpeg is not installed here (no vcpkg root); enable `RTS_BUILD_OPTION_FFMPEG` in a configured
   vcpkg environment (static triplet preferred: the staging gate forbids app-local DLLs).
2. **Video buffer:** `W3DVideoBuffer` allocates `TextureClass(w,h,format,MIP_LEVELS_1)` and writes through
   `Get_Surface_Level()->Lock()` — exactly the CPU-backed dynamic texture path that the X64.30 radar proves on
   D3D11 (`Supports_Dynamic_Render2D_Content`). Expected to work unchanged; to be verified with a synthetic buffer
   test (no ffmpeg needed).
3. **Audio:** `FFmpegVideoPlayer` plays movie sound through `OpenALAudioStream`. The native audio path is
   NativeMiles; it needs a push/queue PCM stream voice (decoder-fed ring buffer with the existing mixer clock and
   EOS) and `FFmpegVideoPlayer`'s audio hook retargeted to it behind the existing audio interface.
4. **Clock:** movie time must follow the audio clock, not render FPS (pause/fast-forward handled by
   `VideoPlayer::loseFocus/regainFocus`).
5. **Gate:** `-novideo`/missing decoder stays graceful (current behavior).
Strategy: FFmpeg-in-process (not a Bink SDK dependency), same player interface, NativeMiles movie stream.
Offline-testable now: video-buffer texture path, NativeMiles PCM queue stream, interface contract; native-
dependent: actual decode (needs ffmpeg libs) and audible/visual sync. Not started in code this milestone because
the decoder cannot be built or exercised on this machine; the design above is implementation-ready.

## H. High-FPS (30 TPS logic, independent presentation) status

| Area | Status |
|---|---|
| Fixed 30 Hz authoritative GameLogic, independent render pacing, FPS presets 30/60/120/144/165/240 | **ALREADY COMPLETE** (Stage 1–3A) |
| Presentation clock, completed-generation capture, phase in [0,1] | **COMPLETE** (3C.1) |
| Ground-unit translation interpolation, orientation (heading) | **PARTIALLY COMPLETE** — implemented behind `-groundInterpolation`, default OFF, native acceptance pending; excludes aircraft, articulated/terrain-aligned, infantry combat states |
| Drawable physics decoration (pitch/roll/yaw/Z) | **COMPLETE** (interpolates by phase) |
| Turret rotation, aircraft, projectiles/missiles | **NOT IMPLEMENTED** (draw at logic rate; 3C notes) |
| Animation (W3D animations via sync clock) | **PARTIALLY COMPLETE** — `WW3D::Sync` accumulates fractional ms; skeletal/wheel continuity **REQUIRES NATIVE VISUAL TEST** |
| Particles | **PARTIALLY COMPLETE** — phase integration exists; attached-FX ordering unverified natively |
| Camera, edge/keyboard scrolling, UI animation/transitions | **COMPLETE by elapsed-time ratios**; GUI timers independence not proven for every widget |
| Water, shroud animation, effect timers | **NOT AUDITED natively** (native D3D11 water/ shroud use wall-clock/logic mixes) |
| Timing helpers (UI tooltips, double-click, replay speed) | audit pending — render-cadence dependence unknown |
| Native D3D11 presentation path (this runtime) | **REQUIRES NATIVE VISUAL TEST** at 60/120/144 with ground interpolation on |
Remaining before production-quality 60–240 Hz: promote ground interpolation after native acceptance; turret and
aircraft root interpolation; projectile/missile path smoothing; native verification of the audits above. This is
"High-FPS completion (timing & interpolation)", not a new render/simulation architecture.

## F. Candidate list for later (not done here)

* Shortcut sidebar: Carpet Bomb, Crate Drop, Napalm Strike, Terror Cell, Black Market Nuke, Defector (retail
  Generals powers not on ZH's bars) — decide after native review of the overlay.
* Options migration (section D), starting with Controls (double-click attack move) and Display (device modes).
* Score screen academy advice, Quick-preset buttons in Skirmish, movie path (section G).

## Candidate

`x64-32-original-experience` (generalsv.exe SHA256 `CC306D1C1544AC2C0E0DCB984C5E7ABA593509B243540F2736227B53E1EA122B`), staged with the shortcut-sidebar overlay (`-Classic` omits it).
