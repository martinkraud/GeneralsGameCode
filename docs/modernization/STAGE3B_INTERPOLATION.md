# Stage 3B — visual interpolation characterization

2026-10-08. Source checkpoint: `967dfb739 Add persistent render FPS options`,
branch `dev/modern-engine`. Starting worktree was clean. **Analysis only: no C++,
build configuration, input bindings, game data or runtime binaries changed.**
No game launch, CMake Install, commit or push. Stop after this report for review.

## 1. Executive summary

The engine has several distinct presentation clocks and smoothing mechanisms,
but **no general previous/current world-transform interpolation in the ordinary
Object -> Drawable -> W3DModelDraw path**. Infantry, vehicles, aircraft and model
projectiles therefore receive discrete authoritative root-transform changes even
when their skeleton, suspension, particles and camera move between logic ticks.
This distinction explains why a smooth camera does not establish smooth units.

Confirmed mechanisms include elapsed-time user camera panning/rotation/zoom,
fractional skeletal animation sampling (enabled by default), visual physics
pitch/roll/yaw/Z interpolation, and fractional particle integration. Their state,
methods and update gates differ; replacing them with one generic lerp is unsafe.

FramePacer already owns `getLogicFramePhase()`, but it is an approximate visual
phase rather than the scheduler's normalized remainder. Non-integer ratios can
lose remainder alignment at each phase reset. General world interpolation needs
a scheduler-aligned presentation phase contract before it uses this clock.
No second wall clock or timing patch is introduced in Stage 3B.

Stage 3A is now **developer manually accepted**: default 60, original gameplay
speed, dropdown, immediate Accept at 120, persistence through exit/relaunch and
source/runtime integrity checks. Normal 30-TPS behavior is inferred from gameplay;
there is still no measured TPS capture. Stage 3B measurements remain NOT RUN.

Keyboard follow-up: arrow panning is raw-key handling in shared LookAtXlat,
elapsed-time scaled each client frame. WASD conflicts exist in both titles;
future controls need explicit conflict handling and persistent, rebindable actions.
**No WASD controls are implemented here.**

## 2. Existing interpolation architecture and source evidence

Paths below are repository-relative; function names are durable anchors at this
checkpoint. `ZH` abbreviates `GeneralsMD/Code`; `G` abbreviates `Generals/Code`.
Both title variants were searched. Shared device/library code is compiled into
both, but title-specific lifecycle and mod behavior still require testing.

| ID | Source and functions | Evidence |
|---|---|---|
| E1 | ZH/G `GameEngine/Source/Common/GameEngine.cpp`: execute, update, canUpdateRegularGameLogic, canUpdateNetworkGameLogic | Client/render precedes current iteration's optional logic tick; offline accumulator retains remainder; network readiness takes precedence |
| E2 | `Core/GameEngine/Source/Common/FramePacer.cpp`: update, reset, getActualLogicTimeScaleFps, getActualLogicTimeScaleOverFpsRatio, getLogicFramePhase | Measured previous iteration duration, clamped logic time step, separate phase; default logic scaling enabled at 30 |
| E3 | ZH/G `GameEngine/Source/Common/Thing/Thing.cpp`: setPosition, setOrientation, setTransformMatrix; title `Include/Common/Thing.h` | Current transform/cached position/angle; old values are local callback arguments, not persistent previous-tick endpoints |
| E4 | ZH/G `GameEngine/Source/GameLogic/Object/Object.cpp`: reactToTransformChange, friend_bindToDrawable | Copies Object transform directly to Drawable; updates partition/containment/trigger state from authoritative movement |
| E5 | ZH/G `GameEngine/Source/GameClient/Drawable.cpp`: updateDrawable, reactToTransformChange, draw, applyPhysicsXform; title Drawable.h::PhysicsXformInfo | Generic root is current Thing transform; previous/current physics decorations retained separately; draw uses a local matrix |
| E6 | `Core/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp`: doDrawModule, handleClientTurretPositioning, getProjectileLaunchOffset, recalcBonesForClientParticleSystems | Sets W3D transform from supplied local matrix; turret bones use current AI angles; animation/attachment work follows |
| E7 | ZH/G `GameEngine/Source/GameClient/GameClient.cpp`: init, update, step, destroyDrawable, loadPostProcess | Input polling and FRAME_TICK each client update; drawable updates; particles gated by preceding hasUpdated; lifecycle/snapshot paths |
| E8 | `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp`: update, draw, step; `W3DView.cpp`: update, drawDrawable, scrollBy, updateCameraMovements, stepView, pickDrawable | View updates and visible drawable draw occur before scene rendering; minimized draw skips; update work moved out of draw |
| E9 | `Core/GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp`: translateGameMessage; `FramePacer::getBaseOverUpdateFpsRatio`; title InGameUI.cpp::update | Arrow states and elapsed-time camera offsets; rotation/zoom separately scaled |
| E10 | `Core/Libraries/Source/WWVegas/WW3D2/ww3d.cpp/.h`: Update_Logic_Frame_Time, Sync, Get_Logic_Time_Milliseconds | Fractional milliseconds accumulated every display update; integral sync advances on hasUpdated |
| E11 | ZH/G `Libraries/Source/WWVegas/WW3D2/animobj.cpp/.h`: Compute_Current_Frame, Anim_Update; title hrawanim.cpp::Get_Translation/Get_Orientation; Core htree.cpp::Anim_Update | Fractional animation time and translation lerp/quaternion slerp; raw interpolation defaults to 1 in Core WWLib/WWDefines.h |
| E12 | `Core/GameEngine/Source/GameClient/System/ParticleSys.cpp`: Particle::draw, ParticleSystemManager::update/draw/completeLogicFrameDrawUpdate/drawSystems | Integrates remaining phase portions; completes one step before changing particle rates; not endpoint lerp |
| E13 | Title `GameLogic/Object/Locomotor.cpp`, `Update/PhysicsUpdate.cpp`, `Update/AIUpdate/MissileAIUpdate.cpp` | Movement/rotation modifies Object during simulation; PhysicsBehavior integrates velocity/acceleration in tick units |
| E14 | Title `GameLogic/Object/Weapon.cpp`: calcProjectileLaunchPosition; Object.cpp bone/launch helpers | Simulation queries Drawable pristine offsets but reconstructs authoritative turret/object transforms; boundary must stay intact |
| E15 | Core `GameClient/GUI/AnimateWindowManager.cpp`, `GameWindowTransitions.cpp` | GUI animation progress scales by base/update FPS ratio |

Repository searches included interpolation/lerp/slerp, previous/last transforms,
all phase consumers, setters, draw modules, cameras, locomotors, physics, missile
updates, containment, snapshot/CRC paths and keyboard/meta/button hotkeys. A class
being called Client is not itself evidence of isolation from simulation.

## 3. Simulation-to-render call path

```text
Iteration i (single engine thread)
  GameClient::update
    append MSG_FRAME_TICK; poll Keyboard/Mouse and create raw messages
    Drawable::updateDrawable(logic/render ratio) [client modules/fades]
    TerrainVisual::update
    W3DDisplay::update
      Update_Logic_Frame_Time(scaled milliseconds)
      WW3D::Sync(previous iteration's GameLogic::hasUpdated)
      updateViews -> W3DView::update
        iterateDrawablesInRegion -> drawDrawable -> Drawable::draw
          local = current Drawable Thing transform * instance matrix
          applyPhysicsXform(local) [if enabled, eligible, not held]
          DrawModule::doDrawModule(local)
            W3DModelDraw -> RenderObjClass::Set_Transform
            animation/turret bones/attached effects
    ParticleSystemManager::update [only !freeze && hasUpdated]
    W3DDisplay::draw -> views/WW3D scene/GUI [can skip]
    InGameUI::update [including held rotation/zoom]
  MessageStream::propagateMessages [camera FRAME_TICK and raw input]
  Network::update [if present]
  GameEngine::canUpdateGameLogic -> GameLogic::preUpdate clears hasUpdated
    offline accumulator OR network frame readiness
  optional GameLogic::update
    update modules/AI/locomotor/physics/weapons
      Thing setters -> Object::reactToTransformChange
        Drawable::setTransformMatrix -> draw-module notifications
    completed world tick increments m_frame, sets hasUpdated
    GameClient::step -> W3DDisplay::step -> stepViews [unless frozen]
  FramePacer::update -> wait/measure elapsed time; reset/advance visual phase
```

E1/E7/E8 establish the ordering. Drawing uses the preceding accepted world state;
camera messages propagated after drawing affect a later view update. `hasUpdated`
is carried across iterations and cleared at the readiness check, not at client
entry. Moving sampling, clearing or simulation work across this boundary changes
semantics. W3DView may update more than once for auxiliary rendering paths, so
future snapshot rotation must use a completed-logic generation, never draw count.

## 4. Camera and keyboard/input findings

### User camera versus world interpolation

Shared LookAtXlat.cpp handles raw key down/up at its KEY_UP/DOWN/LEFT/RIGHT switch
(around 207–253). It stores four `scrollDir` flags, starts SCROLL_KEY while any is
held and stops when none are held, subject to shell/selection/scroll-mode gates.
The `MSG_FRAME_TICK` branch (around 438–535) applies keyboard, RMB or edge offsets.
GameClient.cpp creates this message once per **client update**, not per logic tick.

For a fixed direction and settings:

```text
fpsRatio = BaseFps / max(measuredUpdateFPS, 5)
         = 30 * min(measuredUpdateSeconds, 0.2 seconds)
keyboardOffset = scrollSpeedFactor * SCROLL_AMT * keyboardScrollFactor * fpsRatio
SCROLL_AMT = 200
LookAt -> View::userScrollBy -> W3DView::scrollBy -> setPosition2D
```

W3DView projects the offset onto its yaw-oriented view plane; its camera state
is not Object movement. InGameUI.cpp::update uses the same ratio for held
keyboard rotation/zoom. Scripted camera paths instead consume logic-scaled
milliseconds; waypoint motion requests IgnoreFrozenTime but respects halt, and
outer view pause/debug-freeze checks still apply. Camera lock/follow has additional
per-update `0.1` smoothing factors, so **not every camera mode is proven rate
invariant**. `stepView` deliberately makes sharp fixed-step shakes without lerp.

At stable 30/60/120/144/165/240 FPS, keyboard offsets are respectively scaled by
1, 1/2, 1/4, 5/24, 2/11, 1/8. Over equal wall time their sum is equal, assuming
fixed projection/settings, no constraints, no stalls >200ms and equal input-held
duration. The code therefore supports constant normal keyboard-pan speed with
more, smaller presentation updates at high FPS. It is elapsed-time integration,
**not** previous/current camera-state interpolation. This explains observed
smoothness without making a claim about world-object roots.

**Verification boundary:** source algebra and an ideal double-precision model
confirm equal normalized distance (300 base steps over 10 seconds) for all six
caps. Ignored `build/stage3b-evidence/ideal-clock-model.csv` records the calculation;
it is not a game/unit test, measured FPS/TPS or a measured camera-speed result.
Actual speed at changed caps remains a required manual test. Render timing is
from the preceding loop, and map constraints, follow smoothing, focus and camera
projection changes can affect the observation.

### W/A/S/D conflicts, both titles

Source alone does not contain the stock localized bindings. For evidence, copied
Stage 3A runtime BIG files were opened read-only; CommandMap/CommandButton and
English CSF labels were extracted/decoded only under ignored build evidence.
No Steam/runtime asset was changed. No patch-archive replacement for the selected
files was found in the inspected PatchINI/PatchZH archives. Loose/mod/language
overrides and the active in-memory map remain runtime-UNKNOWN.

| Key | Generals copied English data | Zero Hour copied English data | Conflict |
|---|---|---|---|
| W | No active unmodified W in base English CommandMap; contextual `CONTROLBAR:ConstructGLAWorker = &Worker`, `UpgradeAmericaTOWMissile = TO&W Missile` | CommandMap SELECT_ALL_AIRCRAFT, KEY_W, DOWN, NONE, GAME (line 670); Worker/TOW labels use K/I here | Selection/production; not universally free |
| A | Contextual `CONTROLBAR:AttackMove = &Attack Move`, plus build/upgrade labels | Same Attack Move label and contextual actions | Attack Move/UI button hotkey on release |
| S | CommandMap STOP, KEY_S, DOWN, NONE, GAME (line 672); `&Stop`, `&Scorpion` labels | Same STOP mapping (line 690) and contextual labels | Stop and faction/button actions |
| D | Contextual `ConstructAmericaDozer = Construction &Dozer`, plus Dragon Tank and other labels | Same Dozer/Dragon Tank and additional faction/ability labels | Production/ability buttons |

Evidence files are `ZH_Generals/English.big` and `EnglishZH.big`:
`Data/English/CommandMap.ini`, `generals.csf`; matching `INI.big`/`INIZH.big`
`Data/INI/CommandButton.ini` selects those TextLabels. HotKeyManager::searchHotKey
extracts the character after `&`. ControlBar::populateButton registers it;
HotKeyTranslator executes unmodified printable hotkeys on KEY_UP. CommandMap
S/W can consume KEY_DOWN earlier. The commented DEPLOY/KEY_D block is **not** an
active D mapping. Other A/D conflicts are real contextual labels, not that block.

Debug maps contain modified WASD commands (e.g. Ctrl+W win, Ctrl+D debug
selection, Shift+Ctrl+A asset dump); RTS_DEBUG gates their load. Zero Hour also
loads CommandMapDemo under _ALLOW_DEBUG_CHEATS_IN_RELEASE (e.g. Ctrl+S special
power delays). Future bindings must preserve modifier/context distinctions.

Both clients attach translators in order: Window 10, MetaEvent 20, HotKey 25,
PlaceEvent 30, GUICommand 40, Selection 50, LookAt 60, Command 70. WindowXlat
routes text/modal focus first and blocks disabled cinematic input. MetaEvent
matches data-defined transitions/modifiers/usable contexts and can destroy raw
messages. Thus simply adding WASD cases to LookAt would miss consumed W/S presses
and could still fire A/D button actions on release. KeyboardOptionsMenu's Assign
branch only comments "check grammar in text field"; this screen is **not an
implemented persistent rebinding system**. Existing MetaMap stores one record per
meta action, which is also not a ready multiple-binding solution for arrow+WASD.

Recommended future controls architecture (separate reviewed follow-up):

- Introduce local PanUp/PanDown/PanLeft/PanRight held actions with configurable
  multiple physical bindings, preserving arrows and existing RMB/edge controls.
  Reuse existing direction/elapsed-time camera motion, not Object/simulation code.
- Default to legacy bindings. Offer an explicit opt-in WASD profile and conflict
  review/reassignment for W selection, S Stop and localized contextual A/D/W/S
  buttons. Never silently trigger pan and gameplay commands together.
- Resolve input after UI/text/cinematic focus gating and before competing actions,
  with consistent key-down/up ownership. Support modifiers, layout/localization
  and mod bindings. Retain ownership until release so a changed selection cannot
  cause a release to execute a different button hotkey.
- Track held physical keys per action: releasing W must not cancel held Up.
  Clear holds on focus loss, modal/chat entry, shell changes, disabled input,
  remapping and reset. Preserve current scroll-mode/selection/drag rules.
- Persist through a focused user-binding configuration and expose meaningful
  conflict/rebind UI. Do not assume the old keyboard screen already saves changes
  or overwrite localized string tables to implement rebinding. Keep all camera
  actions out of network/replay command serialization; preserve existing IDs.

## 5. World-object findings

Thing setters hold only current transform and pass temporary old values to
callbacks (E3). Object::reactToTransformChange directly copies the new matrix to
Drawable (E4). Drawable::draw copies its current matrix, composes instance and
optional interpolated decoration, and passes the result to draw modules. Ordinary
W3DModelDraw sets the render object to this matrix (E5/E6). There is no root
position/yaw lerp or saved prior completed-tick root in that path.

Infantry, ground vehicles and aircraft follow this common root path. Locomotor
turning changes authoritative orientation; PhysicsBehavior integrates velocity
and acceleration into a matrix per update and commits it to Object. Missile AI
also changes Object transforms. These updates can sleep or perform multiple
setter calls in one logic tick; setter-call history is not equivalent to tick
history. Aircraft's smooth decorative bank/hover does not interpolate its XYZ.

Turret yaw/pitch are read directly from AIUpdateInterface and used to control
W3D bones each draw. No previous turret angle pair or angle lerp appears there.
ProjectileStreamDraw ignores the supplied root matrix and draws points from
ProjectileStreamUpdate; a generic root change would not fix that special path.
Other bespoke draw/containment modules need explicit eligibility, not a blanket
assertion that every visible object follows W3DModelDraw.

## 6. Per-system classification

Status concerns **existing temporal presentation smoothing**, not code maturity:
CONFIRMED = mechanism traced; PARTIAL = only specified subset; NONE = absent in
the traced ordinary path; UNKNOWN = unproven case. Rates below assume normal
unpaused 30-TPS policy; sleeping updates, freezes, network waits and draw culling
qualify them. R means achieved client/render cadence, not requested cap.

| System | Authoritative owner/state | Simulation update | Render update | Existing interpolation? | Method | Writes to simulation state? | Safe presentation candidate? | Risk | Evidence |
|---|---|---|---|---|---|---|---|---|---|
| User camera pan/rotate/zoom | View/W3DView local camera; input holds | None required | Client R; view R when device valid | CONFIRMED smooth integration; not endpoint interpolation | Measured-delta scaling | No Object-transform writes in traced camera path; picking can affect live commands | Already smooth; retain mechanism | Low for pure pan; medium input/picking | E7–E9 |
| Script/follow/shake camera | Local view, script requests, referenced Object | Requests at logic cadence; shake step at tick | View R | PARTIAL | Timed paths; follow 0.1 per-update; deliberately sharp shake | Reads Object; no world-transform write in traced path | Separate mode audits | Medium | E8/E9 |
| Infantry translation | Object/locomotor/PhysicsBehavior XYZ | Up to 30, sleep-aware | Current root reapplied R | NONE in common root | Direct copy | Authoritative movement writes at logic only; proposed local matrix must not | Yes, eligible uncontained units | Medium | E3–E6/E13 |
| Vehicle translation | Object/physics XYZ | Up to 30 | Root R; decorative suspension R | NONE root; PARTIAL decoration | Current root plus physics decoration lerp | Decoration changes client state/local matrix, not Object XYZ | Yes, after clock and attachment audit | Medium | E3–E6/E13 |
| Aircraft translation | Object/air locomotor/physics XYZ | Up to 30 | Root R; bank/hover R | NONE root; PARTIAL decoration | Same root; appearance-specific offsets | Object XYZ unchanged by decorative interpolation | Later opt-in with transitions | High | E5/E13 |
| Object yaw/full orientation | Object Thing matrix | Up to 30, possibly multiple mutations/tick | Root copied R | NONE root | Direct orientation/matrix copy | Authoritative orientation setters are simulation; future quaternion result local only | Later shortest-arc orientation | Medium/high | E3–E6/E13 |
| Turret yaw/pitch | AI turret state | Logic update cadence | Bone controls R | NONE in handleClientTurretPositioning | Current angles plus art offsets | Traced positioning reads AI and writes render bones | Later, separate visual angle pair | High: muzzle/bone consumers | E6/E14 |
| Projectile translation | Projectile Object/Physics/MissileAI; stream points | Logic cadence | Root/stream R | NONE common root/stream temporal lerp; bespoke effects UNKNOWN | Current Object matrix or stream points | Simulation owns trajectories/collision; render matrix must not | Defer broad projectile work | High | E6/E13; ProjectileStreamDraw |
| Skeletal animation | Model animation state, authored tracks; logic selects conditions | Conditions at logic cadence, plus client transitions | Fractional animation sampling R | CONFIRMED default raw bone interpolation; PARTIAL all animation paths | Translation lerp, quaternion slerp; fractional WW3D time | Client pose, not Object root; pristine query boundary required | Preserve; audit overrides before changes | Medium/high | E10/E11/E14 |
| Physics decoration | Drawable PhysicsXformInfo/loco visual state; reads locomotor/physics | Totals advance on nonzero WW sync step | Lerp R | CONFIRMED for supported appearances | Prior/current pitch/roll/yaw/Z, phase; snap after undrawn gap | Client decoration/local matrix; no Object setter found in these calculators | Preserve; compose after future root | Medium | E5 |
| Particles/effects | Particle/System client state; emitter attachments can read objects/bones | Lifecycle/rates gated by hasUpdated | Fractional integration R | CONFIRMED particles; PARTIAL all FX | Delta of phase, complete remaining step before new rates | Mutates particle state, not authoritative Object; particle state IS save-transferred | Preserve; do not change save/integration semantics casually | High for attached/saved FX | E7/E12 |
| UI motion/fades | Window animation/transitions, Drawable opacity | No universal logic gate | Client R | CONFIRMED selected paths; PARTIAL all GUI | Base/update ratio or logic/render ratio | No authoritative transform writes in traced paths | Usually already scaled | Low/medium | E5/E15 |
| Terrain/world surface | TerrainLogic authoritative heights; W3D terrain/visibility/deformations | Static plus logic-driven changes | View/terrain update R; scene draw R | NONE general temporal height history; spatial interpolation is unrelated | Static mesh/height sampling; separate deformation/effect code | Never smooth authoritative terrain/collision queries | Exclude from first prototype | High for deformation/bridges | E8; BaseHeightMap/W3DTerrainVisual |

## 7. Interpolation clock analysis

E1 offline scheduler adds `min(lastMeasuredDelta, T)` to m_logicTimeAccumulator,
subtracts T on acceptance and allows at most one tick per loop. When logic rate
is at least effective render cap or TiVO fast mode applies, it updates immediately.
Network scheduling uses frame-data readiness, not this offline accumulator.

E2 phase resets to `min(1, logicFPS * latestMeasuredDelta)` after hasUpdated,
otherwise adds this increment and clamps at 1. Reset/start phase is 1. It never
reads m_logicTimeAccumulator. Only Drawable physics decoration and particles
currently call getLogicFramePhase (both title Drawable variants plus shared
particles). This limited use is not evidence of a general interpolation clock.

| Nominal render/30-TPS | Intermediate renders per nominal tick | Phase increment for constant delta | Offline tick spacing in render iterations |
|---|---|---|---|
| 30 | 0 | 1 | 1 |
| 60 | 1 | 1/2 | 2 |
| 120 | 3 | 1/4 | 4 |
| 144 | Variable | 5/24 | Mix of 4 and 5 |
| 165 | Variable | 2/11 | Mix of 5 and 6 |
| 240 | 7 | 1/8 | 8 |

At 144, five deltas total 25/24 of T; scheduler retains 1/24 T after that tick,
but phase resets to 5/24 for the next draw rather than carrying the remainder
(the schedule-aligned next-render fraction would be 6/24 in this example).
At 165 the first six deltas leave 1/11 T. An ideal constant-delta model shows
legacy-phase versus remainder look-ahead differences up to 1/6 at 144 and 1/11
at 165. Integer ratios align ideally; real jitter can break that alignment too.
These are **source-derived/model findings**, not observed visual artifacts.

Recommended authoritative presentation clock: keep FramePacer as the public
owner/access point, driven by **actual completed logic generations and scheduler
timing**, not a new independent QPC timer or render-frame modulo counter.
For the normal offline accumulator branch, investigate exposing its existing
remainder read-only to presentation. Because drawing precedes the next scheduling
decision, a candidate fraction is:

```text
T = effective logic period
alphaAtNextClientDraw = clamp((remainderAfterLastDecision + min(latestDelta, T)) / T, 0, 1)
```

This is a design candidate for a delayed previous/current sample pair, not a
drop-in replacement proven for all branches. Sample/phase generation must agree;
validate exact reset/ordering with deterministic tests before implementation.
No new simulation accumulator and no scheduler policy changes are needed.
Do not silently replace the existing phase for particles/physics, whose endpoint
completion and saved state depend on current semantics.

| Situation | Current evidence | Required future presentation policy |
|---|---|---|
| Pause/freeze | Logic queries return 0 unless ignored; engine may still run script updates with IgnoreFrozenTime; phase stops advancing or can reset to 0 if hasUpdated | Key snapshots to completed world-state generation, not merely an update function call; define freeze at current visual pose versus snap explicitly |
| Network wait/halt | Network readiness owns acceptance; rate query precedes offline scale; halt gives 0 | Advance only within available completed pair, clamp and hold; accepted network ticks notify presentation; no extrapolation or changed deadlines |
| Replay normal | Same simulation path, replay commands by frame; render-cap overrides possible | Same tick-pair contract; reset on playback load/start/seek-like discontinuities |
| Fast-forward/script time multiplier | Limiter can bypass caps; script-fast display path returns early; immediate-tick branch possible | Bypass/snap while skipping visuals; reset upon return, never replay missed visual ticks |
| Long stall/low achieved FPS | One tick max/iteration, accumulation delta clamped to T; phase clamped | Follow accepted simulation time, not raw wall-clock extrapolation; snap after stale pair; do not invent catch-up |
| Minimized/device unavailable | Display draw skips when iconic; update can still sync; view update requires cooperative device | Capture state independent of visibility/device, reset stale render history on restore; no catch-up trail |
| New map/reset/speed change | FramePacer::reset phase 1; engine accumulator is separate | Explicit generation/epoch invalidation and re-prime; do not reuse old world's remainder/history |

## 8. Required previous/current state and ownership

There is no general persistent pair in Thing/Object/Drawable for completed-tick
root positions and orientations. Thing's local oldMtx/oldPos/oldAngle expire at
each setter; many setters in one tick would overwrite a naive callback pair.
PhysicsXformInfo's prior/current decoration totals are not world transforms.
Object/list `m_prev` links and particle emitter m_lastPos are also not root history.

Prefer a compact, **nonserialized client presentation cache** associated with
Drawable lifetime (or a GameClient-owned sidecar if Drawable snapshot/layout
constraints make that safer): previous/current root position, later normalized
rotation, completed-frame/generation, validity and discontinuity epoch only.
No Object-state clone, velocity prediction or authoritative-state setter.

Capture the final canonical Drawable/Object root once per completed tick at the
client boundary before view/bone/effect updates, including off-screen eligible
drawables. Rotation is per generation, not per setter, drawable update or view.
Keep Drawable's existing Thing transform canonical: logic queries and containment
already use it. At Drawable::draw, produce a local interpolated root matrix, then
compose existing instance scale/matrix and decorative physics. Pass it to eligible
draw modules only. Never temporarily mutate Object or Drawable Thing transforms
and restore them. Begin with translation-only opt-in ground units; require later
quaternion/shortest-yaw handling rather than lerping arbitrary matrix elements.

Do not serialize/CRC this cache; explicitly clear it after load/reset/rebind.
The existing GameClient/Drawable snapshot traversal is real and must be audited
before adding fields. A sidecar avoids silently adding them to saved state.
Previous/current interpolation adds roughly one logic interval of visual delay;
document that tradeoff and align selection markers, attachments and muzzle FX.

## 9. Teleport/snap handling

No universal "teleported this tick" marker was found in the traced setter path.
All callers can use the same Thing setters; distance alone cannot distinguish
teleport from a fast missile or aircraft. Future client invalidation notifications
must not change authoritative movement or introduce serialized simulation flags.

| Discontinuity | Source evidence | Required presentation reset |
|---|---|---|
| Create/spawn/respawn | ThingFactory::newObject/initObject; ScriptActions creation sets orientation/position after allocation; names can be reused | Initialize previous=current at final spawn transform; key by lifetime/generation, not name/address alone |
| Destroy/rebind | GameClient::destroyDrawable unbinds Object; Object::friend_bindToDrawable | Remove cache immediately; no dangling interpolation or trail to a new object reusing an ID |
| Transport/tunnel entry/exit | TransportContain positions riders at exit bone; TunnelContain moves to container/exit and changes hidden state | Snap both ends; invalidate while contained and prime at visible exit |
| Garrison | GarrisonContain::putObjectAtGarrisonPoint sets Object position and creates independent GarrisonGun Drawable; other exits relocate | Snap contained identity/anchor and effect ownership; never lerp through structure |
| Scripted reposition/teleport-like move | ScriptActions setters, AI recovery moves and tunnel transit use ordinary setters | Explicit presentation discontinuity notification at known callers; conservative distance guard only as fallback |
| Map/load/reset | GameClient::reset/loadPostProcess; Drawable snapshot/loadPostProcess; engine/FramePacer reset | Clear generation/clock history globally; first new pair equals loaded canonical pose |
| Hidden/shroud/off-screen gap | Drawable::draw early-out; physics decoration already snaps if sync history is discontinuous | Update snapshots independently of draw; otherwise invalidate/re-prime on return |
| Aircraft/parachute/rider/docking transitions | Air locomotor plus contain modules change attachment/local/world relationships | Defer until reviewed; snap on parent/coordinate-space change, not every ordinary flight tick |
| Large instantaneous movement | No shared reliable semantic flag | Clamp alpha; never extrapolate; fallback snap threshold per eligible class, not a universal speed rule |
| Cinematic/bookmark/camera lock changes | W3DView scripted state, setCameraLock/snapImmediate, reset/waypoint paths | Clear future camera smoothing history separately; do not interpolate across deliberate camera cuts |

## 10. Multiplayer/replay determinism analysis

No Stage 3B executable change means there is no newly introduced simulation or
format change. Future general interpolation is **not yet proven safe** merely
because the output is a render matrix. Critical evidence and boundaries:

- Object movement marks partition/containment/triggers dirty (E4). Never call its
  setters, physics/locomotor setters or collision queries with interpolated state.
- Weapon::calcProjectileLaunchPosition reads pristine Drawable offsets, then uses
  authoritative AI turret angles and Object world transform (E14). Keep that
  canonical path distinct from visual bones/root. Audit every bone/render-object
  query, including transport exits, parachutes, payload drops and custom mods.
- W3DView::pickDrawable casts into the rendered scene, so a changed visual matrix
  can change which Object ID a live click selects. Presentation picking needs a
  deliberate policy; it must not rewrite authoritative targets/coordinates or
  command-frame assignment. Identical replay inputs cannot prove live-input safety.
- Network::update/readiness and CommandList execution-frame semantics must remain
  untouched. New camera actions must remain local and outside serialized network
  ranges; no enum renumbering, changed message formats or new render-dependent
  command production. Camera changes can still affect user-issued target choices.
- GameLogic::getCRC traverses Object/subsystem state and logic RNG. New caches,
  alpha and visual results must be excluded. W3D animation uses client RNG for
  idle variants; interpolation itself must consume **no RNG**, especially logic RNG.
- Particles mutate client state during fractional draw integration and transfer
  particle data in saves; their crc methods inspected here are empty. "Client"
  does not imply "unsaved" or "safe to retime". Preserve their existing semantics.
- GameEngine's VERIFY_CRC surrounds client work. Use it where build guards allow,
  plus frame-indexed CRC comparisons across caps and rendered/headless runs.
  Do not move logic work into that block to make it pass.

Current modern-MSVC retail CRC compatibility is not established; the existing
VC6/retail guard requirements remain as documented in COMPATIBILITY.md. Same-build
lockstep, replay continuation, save/load and live picking results remain UNKNOWN.

## 11. Recommended architecture

Keep current authoritative simulation and canonical Drawable state. A single
FramePacer presentation API should pair an accepted-logic generation with a
scheduler-aligned alpha; GameClient owns lifetime-safe presentation history.
Drawable::draw composes a transient eligible render root. W3D receives that root;
logic, pristine bone queries, collision, targeting and save/replay paths retain
canonical state. Existing skeleton/decorative/particle clocks are preserved until
their composition is tested. This is a design boundary, not a safety proof.

Do not implement general interpolation yet: clock residual alignment, rendered
picking, custom draw modules, containment and logic-facing bone queries make this
larger than the exceptionally small/low-risk exception allowed for Stage 3B.
No minimal timing patch is necessary to complete this characterization.

## 12. Proposed Stage 3C implementation plan

1. Review this report and select a small opt-in prototype. First add deterministic
   tests for a presentation timing helper using existing deltas, accumulator
   remainder and accepted tick generations: 30/60/120/144/165/240, jitter, stalls,
   pause, no-network-readiness, reset and skipped rendering. Preserve scheduling.
2. Expose read-only presentation timing with explicit offline/network/fast-mode
   policy. Keep legacy physics/particle phase unchanged initially. Verify capture
   order and sample generation; never divide by requested render cap for alpha.
3. Add a nonserialized client cache for translation only on a restricted set of
   uncontained ground units using the standard model draw path. Implement spawn,
   destroy, load and known snap invalidation first; opt out unsupported modules.
4. Produce a local draw matrix and align indicators/picking/attachments under a
   documented policy. Audit pristine bone/weapon paths before enabling by default.
   No turret, aircraft, projectile, terrain or generalized rotation in the first
   slice; add separately after evidence from ground-unit tests.
5. Build win32 Release, focused tests, guarded backup and a separately named
   candidate only when source work exists. No automatic launch. Developer runs
   visual/timing/CRC/save/replay/LAN matrix; retain 30-TPS and original runtimes.
6. **Separate controls follow-up:** design an opt-in rebindable WASD profile with
   arrows retained, dual-key hold ownership, gameplay/button conflict handling,
   focus/modifier resets and persistent binding UI. Do not bundle controls into
   the interpolation prototype or treat adding raw key cases as sufficient.

## 13. Manual test matrix and measurement plan

All new Stage 3B rows are **NOT RUN**. Use the existing Stage 3A executable for
characterization; no Stage 3B runtime is needed for documentation-only work.
PowerShell 7 at repository root, developer only:

```powershell
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage3a-fps-options
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage3a-fps-options -BackupUserData -ValidateOnly
# Only the developer launches, after reviewing backup protection:
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage3a-fps-options -BackupUserData
```

Use Options/Accept to select cap; do not use Release -fps or logic-speed hotkeys.
Each manual launch backs up shared Documents data. No script edits Options.ini;
normal in-game preference writes are expected. Use unique development saves only.

| Requested FPS | Measured render FPS | Measured completed logic TPS | Required observations |
|---|---|---|---|
| 30 | UNKNOWN | UNKNOWN | Original reference; no nominal intermediate frames |
| 60 | UNKNOWN | UNKNOWN | One nominal intermediate frame; separate root translation from animation/decorations |
| 120 | UNKNOWN | UNKNOWN | Three nominal intermediates; preserve accepted Stage 3A preference behavior |
| 144 | UNKNOWN | UNKNOWN | Non-integer cadence; inspect periodic phase resets/stepping |
| 165 | UNKNOWN | UNKNOWN | Alternating 5/6 render-iteration cadence ideally; inspect periodic stepping |
| 240 | UNKNOWN | UNKNOWN | Seven nominal intermediates; headroom/limiter/load sensitivity |

For **every row**, use the same map/units/seed/settings/camera/wrapper and enough
headroom. Record executable/data identity, achieved frame-time distribution,
resolution/mode, zoom/angle, UI scale and Options scroll factor. Repeat stable
120-second captures three times after warmup. Use existing GameClient Tracy frame
marks and ZH `LogicFrame` plot delta / elapsed seconds (not count of plot samples),
as in STAGE2_TIMING.md. A profile executable requires its own guarded runtime
and is not interchangeable with the accepted Release runtime. Without counters,
write TPS UNKNOWN; stopwatch speed checks are supporting evidence only.

| Cases at each cap | Observation / acceptance for characterization and later prototype |
|---|---|
| Infantry walking and fast infantry | Stationary camera; root displacement samples versus foot/bone animation; no changed arrival time |
| Vehicle straight-line/turning | Root XY/yaw versus suspension/pitch/roll; no changed path/turn/arrival or collision |
| Tank turret tracking/fire | Bone yaw/pitch versus hull; muzzle flash/projectile spawn alignment; unchanged targeting/hit results |
| Aircraft and missiles/projectiles | XYZ banking/height, fast travel and impact; classify custom trails separately; no stretched spawn/impact |
| Arrow panning, diagonal panning, camera rotation/RMB/edge | Hold for a timed interval on flat terrain away from borders; measure world-coordinate displacement and angular change; repeat fixed zoom/angle/settings; diagonal speed kept as existing behavior |
| Camera follow/script/shake/cuts | Separate fixed-factor follow from elapsed pan; waypoint/lock/zoom/teleport cuts; do not smooth intentional sharp shake |
| Particle-heavy combat | Emission, bone attachments, trails, smudge, disappearance; unchanged lifetime/logic result; isolate low achieved FPS |
| Pause/resume, speed/fast-forward transitions | No state leak/extrapolation; record current legacy behavior before proposing reset policy |
| Minimize/restore, Alt-Tab/device loss and stalls | No stale-history travel on return; distinguish suppressed draw from client updates |
| Spawn/destroy/transport/garrison/tunnel/script reposition | No cross-world or cross-lifetime interpolation; visibility/parent/reset transitions snap appropriately |
| Picking/markers and save/replay/LAN | Visual model/selection/health bar alignment; command targets/frame indices; same-build frame-indexed CRC and save/load continuation |

Keyboard-specific baseline checks: arrows continue alongside text/chat/modal and
cinematic input gating; release before/after focus loss; direction combinations,
RMB drag and selection. WASD still performs original actions in Stage 3B. For a
future controls candidate, add W aircraft/worker/TOW, A Attack Move, S Stop and
D Dozer/ability conflicts in both titles/languages, key down/up/repeat, modifiers,
simultaneously held arrow+W, rebinding while held, and live command-frame checks.

## 14. Remaining unknowns, validation and handoff

No measured Stage 3B root/camera/phase trace, high-cap visual acceptance, actual
TPS capture, LAN/replay/retail compatibility, save continuation or complete mod/
language binding audit has run. Full bespoke aircraft/projectile/containment/draw
module coverage, camera-follow rate invariance and future picking semantics remain
open. Terrain deformation, ultrawide/FOV, x64, D3D11 and multithreading are outside
this stage. Smooth appearance cannot certify deterministic lockstep.

Documentation-only change: no build or production unit-test rerun is required or
claimed. Source inspection and the explicitly ideal clock/camera calculation are
the analysis evidence; `git diff --check` is the editing check. Read-only runtime
integrity checks PASS for baseline, Stage 2 and Stage 3A: each matches all 369
inventory paths, lengths and SHA256 values. This proves copied-file integrity,
not gameplay or compatibility. Extracted
asset evidence/calculation CSV/logs stay ignored under build; no source assets or
private user data are committed. No Stage 3B executable/candidate was created.

Review the clock contract, presentation/canonical boundary, eligibility, snap
signals and input-conflict policy before authorizing Stage 3C implementation.
