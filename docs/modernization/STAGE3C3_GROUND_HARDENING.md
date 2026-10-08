# Stage 3C.3: ground translation hardening

2026-10-08. Started clean on `dev/modern-engine` at `8fdc9c939`
(`Add gated ground unit translation interpolation`); Stage 3C.1 is `14aed7d51`.
The developer reports same-binary Stage 3C.2 laptop A/B: ON visibly smoother,
without an obvious speed/basic-selection regression. This is accepted baseline
evidence, not full high-refresh/CRC/content acceptance. This stage launches no game.

## Architecture preserved

GroundTranslationHistory remains a direct, nonserialized Drawable member. Both
GameClient::update entry points enumerate Drawables only while the gate is ON,
before client updates/views, capturing only final completed GameLogic generations
matching PresentationTiming. Duplicate captures/draws cannot rotate a generation.
The published clock pair, alpha, validity and epoch are unchanged. XYZ Lerp,
clamping, stale-canonical guard, first/gap/reset behavior and 12-unit defense
remain unchanged. No velocity prediction or new production timer/accumulator.

Drawable::draw still starts with a canonical matrix copy, changes only local XYZ,
then applies instance transforms, existing physics pitch/roll/yaw/Z decoration,
and the existing module/model transform. Object/Thing/Drawable getters remain
canonical. No set-interpolate/draw/restore pattern, basis or turret interpolation.
HUD projection offsets and terrain decals follow the same presentation root.
The Release `-groundInterpolation` startup parser and default OFF are unchanged;
no Options.ini, graphics UI or controls changes.

## Capability/state eligibility

The six-name allowlist is removed, not enlarged. GroundTranslationPolicy exposes
native kind queries, a pure capability decision and explicit movement-state policy.
The runtime adapter reads current Object/AI/module facts at capture AND consumption;
no eligibility registry, mutable Object cache, added allocation or persistent data.
The same policy is tested with a native KindOfMask view. It accepts renamed units
and faction variants only when their actual capabilities pass.

All are required:

- Bound, alive and visible Drawable; offscreen alone does not prevent history capture.
- INFANTRY or VEHICLE; none of AIRCRAFT, PROJECTILE, STRUCTURE, IMMOBILE, DOZER,
  HARVESTER, TRANSPORT, BOAT, CLIFF_JUMPER, DRONE, MOB_NEXUS,
  SPAWNS_ARE_THE_WEAPONS, PORTABLE_STRUCTURE, INERT.
- No contained-by/own Contain module, disabled/airborne/above-terrain state,
  parachuting, special ability, undergoing repair, reconstructing, status IMMOBILE
  or DEPLOYED. Active scripted camera and last command source CMD_FROM_SCRIPT reject.
- Exact ordinary AIUpdateInterface module capability (not just inheritance), a
  current locomotor with only GROUND/RUBBLE legal surfaces (GROUND required),
  and an approved ordinary AI state. Water, cliff, air and unknown surfaces reject.
- At least one DrawModule; every module explicitly supports root translation;
  no attached root. Base DrawModule still defaults false. Mixed lists fail closed.

Custom AI/draw subclasses with distinct module keys stay canonical. Mods retaining
these exact standard engine behaviors can qualify without stock object names.
Arbitrary custom Behavior modules can still perform unknown short relocations:
capabilities improve name-reuse safety but cannot prove all mod logic. No INI/mod
loading format or data change is required.

## Stock content and category evidence

Read-only BIG extraction into ignored `build/stage3c3-evidence/` examined INIZH.big
and ZH_Generals/INI.big, plus available patch Object entries. A declaration scan
found 487 ZH and 232 Generals ground-kind declarations; these are NOT counts of
eligible runtime templates (inheritance, patches, nested fields and runtime state
require the actual engine). Extracted text, stock-objects.json and compiler probes
are local supporting evidence. No Steam archive was written.

The original Ranger/Redguard/Rebel/Crusader/BattleMaster/Scorpion use ordinary
AIUpdateInterface and W3DModelDraw/W3DTankDraw in both base stock sets and satisfy
the new policy in equivalent safe states. Tests keep those six diagnostic fixtures.
Ordinary infantry, tracked tanks and wheeled ground units now qualify by capability;
examples for later acceptance include Tank Hunters/Missile Defenders, Paladin,
Gattling tanks, faction tank variants and Rocket Buggy (W3DTruckDraw).

Dozers/workers/collectors fail kind and/or special-AI checks. Ambulance/Medic,
Technicals, Humvees/troop carriers and bikes fail transport/Contain/special AI,
cliff-jumper or attached/bespoke draw checks. DeployStyleAIUpdate is excluded;
DEPLOYED/ability/immobile states are excluded regardless of AI name. Standard
SCUD launcher uses ordinary AI + truck draw and does not relocate its world root
for its model firing/deployment animation; its ordinary movement can qualify,
while special/deployed states snap. Do not treat every unit called a launcher as
safe: state/module capabilities, not labels, decide. Amphibious/boat/drone/mob
and scripted/cinematic movement remain canonical. Some decorative civilian units
with truly ordinary capabilities may qualify; custom cinematics without script
ownership/flags remain a mod/content limitation.

## DrawModule classification

| Classification | Module | Evidence/policy |
|---|---|---|
| SAFE for this XYZ prototype | W3DModelDraw | Consumes local matrix; root Set_Transform; pristine logic offsets unchanged |
| SAFE for this XYZ prototype | W3DTankDraw | Calls standard root path; canonical physics velocity; client treads/emitters |
| SAFE, newly admitted | W3DTruckDraw | Calls standard root path first; wheel/cab/trailer bones, suspension, dirt/powerslide particles/audio are client decoration; no Object transform setter |
| UNKNOWN, canonical | W3DTankTruckDraw | Separate combined implementation, not implicitly opted in; Quad Cannon stays excluded |
| UNKNOWN, canonical | W3DPoliceCarDraw | Siren/custom draw behavior; not implicitly opted in by Truck inheritance |
| UNSUPPORTED, canonical | W3DOverlordTankDraw/W3DOverlordTruckDraw | Attached subobject ownership/Contain; Avenger and Overlord families excluded |
| UNSUPPORTED, canonical | aircraft/helicopter/projectile/stream/laser draw, all other/custom modules | Outside approved ground root path; default false |
| UNSUPPORTED, canonical | Any otherwise safe root with AttachToBoneInAnotherModule | Another root/coordinate-space owner |

Only the actual model/tank/truck keys are accepted by W3DModelDraw's predicate.
Do not infer safety from inheritance. The truck timing, bones, emitter and audio
implementation is unchanged. Current orientation still accompanies delayed XYZ.

## Movement-state classification

| Enabled | Evidence |
|---|---|
| AI_IDLE, AI_MOVE_TO | Stage 3C.2 working baseline; unchanged completed-position history |
| AI_WAIT | Stationary wait; no semantic world relocation |
| AI_ATTACK_POSITION, AI_ATTACK_OBJECT, AI_FORCE_ATTACK_OBJECT, AI_ATTACK_AND_FOLLOW_OBJECT | AIAttackState uses ordinary approach locomotion; authoritative aim/weapon/target queries stay canonical |
| AI_ATTACK_MOVE_TO | Attack/move submachine uses ordinary move/attack trajectories |
| AI_GUARD; ZH-only AI_GUARD_RETALIATE | Guard return/chase uses standard movement; tunnel guard is a separate excluded state |
| AI_MOVE_OUT_OF_THE_WAY, AI_MOVE_AND_TIGHTEN | Yield/formation correction inherits ordinary internal move/path approach |

All other states reject: waypoint/follow-path (including exact/team/script and
attack-follow variants), hunt/wander/panic/retreat-like states not yet audited,
entry/exit/instant exit, docking, repair, rappel/combat drop, tunnel guard,
evacuate/delete, ability/busy, face/pick-up and unknown values. Script ownership
also rejects even when the numeric state is MOVE_TO. No state machine decisions,
paths, speeds, turret angles or locomotor integration were changed.

## Picking, box selection and command boundary

W3DView::pickDrawable still raycasts rendered geometry (GUI occlusion and pick-type
filters unchanged), resolves DrawableInfo, and returns the same Drawable/Object ID.
SelectionXlat uses this for single click, double-click and hover/highlight; the
mouseover hint is still an existing ID. CommandXlat builds attack/context commands
from IDs and move/location commands from existing canonical/terrain queries.
screenToTerrain and all command source/frame/coordinate writers have zero diff.

SelectionXlat::selectInRegion and InGameUI select-matching/select-by-kind call the
screen-region W3DView iterator. Its isolated projected center now uses
Drawable::getGroundPresentationPosition(), a read-only local UI API. It computes
the same root XYZ as drawing, or canonical XYZ with gate OFF/invalid/unsupported
state. Null-region selection-all and point-region ray picking keep their branches.
World-space GameClient/partition iteration and culling remain canonical.

Client capture/update/display/draw precedes the outer message propagation, so the
region policy consumes the current published pair, not a new input timer. Local
selection changes the chosen ID set as intended, without changing ID meaning,
message format, command coordinates or execution frames. This is input resolution,
not an authoritative position rewrite. Runtime click/context/drag regressions are
still manual acceptance: root-center selection is not whole-mesh extent selection,
physics/instance offsets and frustum edges may still differ, and human command
choice can change when a different presented unit is selected.

## Semantic relocation hardening

Preserve Stage 3C.2 constructor/bind/load/hidden/epoch/generation/current-position,
containment and large/out-of-update guards. Added resets touch ONLY Drawable history:

- AIStateMachine::setState clears before crossing into/out of an unsupported state,
  including transitions which enter and leave inside one tick. Reset-to-default
  also clears. Ordinary enabled-to-enabled transitions keep their pair.
- Explicit AIRappelState landing and fallback scatter; startup idle grid placement;
  AIAttackApproachTargetState final-goal correction. These are identifiable
  reposition sites, not every ordinary locomotor setter.
- Locomotor-set rebuild and actual selected-locomotor change. The existing set,
  path selection, old/new pointers and movement state are unchanged.
- RailroadBehavior's collision displacement/lift of OTHER units. The actor train
  can move an otherwise eligible victim a small amount; actor exclusion alone
  was insufficient. Damage, physics impulses and RNG lines are unchanged.

Transport/tunnel/garrison/parachute paths pass through existing contained-by reset;
entry/exit states reject. ScriptActions' six position sites create/spawn objects,
transports/contents: construction/binding resets cover them. Respawn/replacement
uses new Object/Drawable lifetimes; dozer/worker rebuild and aircraft/deck/special
projectile relocations remain excluded. No universal semantic teleport API exists.
Unknown in-tick relocations <=12 units outside these hooks can still interpolate;
no claim of universal teleport detection. No new scattered distance constants.

## HUD, bones, weapons and visual FX

Existing health/status/group/formation/veterancy/ammo/container/caption/construction
projection offsets are preserved, now sharing the read-only root-position helper.
Terrain selection decals/shadows attach to RenderObj; slopes, instance transforms
and existing suspension/physics decoration retain their composition order.
Canonical geometry, range, targeting and passenger exit queries are unchanged.

Pristine launch/bone paths and Weapon::calcProjectileLaunchPosition remain
canonical. ParticleUplinkCannonUpdate's gameplay client-bone calls belong to an
excluded structure. A wider search also found ZH LaserUpdate::updateStartPos:
despite its GameLogic directory it derives from ClientUpdateModule, is attached
to a Drawable created by Weapon::createLaser, and reads world-space client bones
for the visual beam endpoint only. Damage/weapon origin is not recomputed from it.
Its beam start can follow presentation while target end remains canonical; no FX
correction or particle timing change is included. Muzzle/projectile and beam/end
latency, emitter continuity and save continuation remain manual visual risks.

## Memory measurement

MSVC x86 Release compiler layout probes compiled pre-Stage-3C.2 (14aed7d51) and
current Drawable headers with identical include/define settings in isolated build
shadow directories. Both Generals and ZH: old sizeof(Drawable)=344, current=384;
history is at offset 136. Benchmark sizeof(history)=40 and sizeof(void*)=4 confirm
exact +40 bytes, with NO additional Stage 3C.3 per-Drawable fields/allocations.
Keep the simple member: packing/deriving generation fields for aesthetics is not
worth changing the established pair/lifetime correctness contract.

| Drawables | Additional history bytes | KiB | Current Drawable object bytes |
|---:|---:|---:|---:|
| 1,000 | 40,000 | 39.06 | 384,000 |
| 5,000 | 200,000 | 195.31 | 1,920,000 |
| 10,000 | 400,000 | 390.63 | 3,840,000 |

These exclude memory-pool bookkeeping, render objects, bones, textures, scene
resources and the benchmark's separately allocated canonical-matrix vectors.
Probe logs/scripts: ignored build/stage3c3-evidence/*-layout.log/measure-layout.py.

## Release performance method

Opt-in disabled Google benchmark: counts 100,250,500,1000,2000,5000; 500 traversals,
five repetitions, median elapsed milliseconds and nanoseconds/object. Same x86
Release MSVC 19.51.36260, /O2, Ninja Multi-Config, tests ON, retail DEFAULT,
RTS_DEBUG OFF. steady_clock is benchmark-only; no production timer was added.
Both title binaries were run directly, never the game executable.

Operations: gate-OFF draw branch and HUD branch; native kind mask + pure eligibility
policy; linear contiguous history capture traversal + policy; full returned render
matrix; HUD/selection XYZ; policy+full render matrix. Matrix measurements use a
noinline harness to materialize all basis values; they include call/checksum cost.
The other loops retain observable checksums. Branch cases are fixed per measured
batch; this is a deterministic hot-cache synthetic workload, not a live Drawable
or virtual-module benchmark. Prepared state excludes Object/AI/terrain query,
linked-list locality and virtual draw-module cost; GPU/scene/picking work is NOT
measured. Never convert these results into a real-game FPS/large-army claim.

GameClient's full linear traversal was already gated ON; it remains so. OFF now
explicitly bypasses eligibility/history in draw and returns zero HUD offset before
matrix work. The position API returns canonical immediately when OFF. These remove
avoidable work without caching stale runtime eligibility. No multithreading or
speculative optimization. Detailed final measurements follow below.

## Tests, build and determinism review

All 17 prior history/math cases remain, including six caps and 144/165 timing,
canonical basis, lifetime reuse, final completed samples, clamps/gaps/epochs and
multiple draws. Only obsolete six-name assertions were replaced with capability
fixtures; gate-default coverage remains. Eight new policy tests cover original six
+ renamed infantry/vehicle fixtures; forbidden kinds; missing/blocked runtime
facts; zero/unsupported/mixed/attached modules; approved and excluded states;
gate OFF; NaN; read-only HUD/selection XYZ. Runtime transition hooks, actual stock
object assembly, GPU picking and UI interaction are source-audited/manual rather
than represented as fake engine integration tests.

Release configure/build PASS. CTest 2/2; direct Google binaries 53 tests each,
106 total PASS. One benchmark is disabled by default in each binary, invoked
explicitly for measurement. Initial shared-test compile failed on two ZH-only AI
enums; title guards corrected both policy/test references before final passes.
A final audit found isDoingGroundMovement alone also accepts water/cliff; an
explicit tested ground/rubble-only surface guard closes that gap. The preliminary
new staged copy is retained as stage3c3-ground-hardening-pre-surface-audit; it is
not the final acceptance candidate. Final exact-name staging uses the rebuilt pair.
An initial PowerShell direct-test invocation failed in native output redirection;
Python subprocess capture ran both binaries successfully. No test failure remains.
Warnings on changed lines, final inventories and git record are below.

Source diff preserves clock/FramePacer/scheduler, getLogicFramePhase, particle and
physics/animation timing. No Object transform mutation, serialization/xfer/CRC,
save version, replay/network message/command ID/coordinates/frame writer or logic
RNG call was added or changed. AI files add only client-history resets around
existing operations; authoritative movement/damage/path/RNG statements are intact.
Modern MSVC explicitly is not retail CRC-compatible; no runtime compatibility or
same-build CRC/LAN/save/replay result is claimed.

## Manual developer acceptance (NOT RUN for Stage 3C.3)

From the repository root in PowerShell 7, after review:

```powershell
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage3c3-ground-hardening
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage3c3-ground-hardening -CheckSource
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage3c3-ground-hardening -BackupUserData -GameArguments @('-groundInterpolation') -ValidateOnly
# Developer-only actual ON launch after validation/backup review:
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage3c3-ground-hardening -BackupUserData -GameArguments @('-groundInterpolation')
# Same binary OFF reference:
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage3c3-ground-hardening -BackupUserData
# Optional synthetic measurement, not a game launch:
./build/win32/Tests/Google/Release/z_googletest.exe --gtest_also_run_disabled_tests --gtest_filter=GroundTranslationBenchmark.DISABLED_ReleaseCost
```

Launch backups protect shared Windows user data, not a separate profile. Retain
backup paths. Set 30/60/120/144/165/240 in existing Options and Accept; Release
-fps is RTS_DEBUG-only. Use identical binary/map/orders, ON/OFF, existing original
six AND expanded units/faction variants. Laptop checks can precede main-PC high
refresh checks; no ultrawide hardware is required. Record actual caps/TPS if
instrumented, unit/state/map, elapsed time, selection IDs and outcome.

| Scenario | Acceptance (all new rows pending) |
|---|---|
| Several infantry, tanks, tracked variants, Rocket Buggy | Original six unchanged in safe movement; expanded coverage visibly smoother above 30 |
| Idle -> move -> attack/chase/attack-move -> move; guard return; formation/yield | Continuous eligible XYZ; current orientation retained; unsupported states snap |
| Fixed/moving camera, slopes, tank turns, suspension, frustum exit/reentry | No corruption; HUD, decals/shadows follow; no visible periodic 144/165 cadence |
| Click/double-click, hover, tight drag-box, box edges, overlapping friends/enemies | Presented unit selected; unchanged identity; center/mesh differences recorded |
| Attack/move/context/force-attack commands and weapon/beam FX | Canonical coordinates/range/damage results; no wrong target or changed command timing |
| Spawn/destroy, save/load, pause/resume, minimize/restore | Fresh pair/recovery snaps; no stale travel or history reuse |
| Transport/tunnel/garrison/parachute/docking/rappel, dozer/collector/deployable/attached units | Unsupported categories/transitions remain canonical |
| Scripted reposition, locomotor switch, train collision if practical | Explicit short relocation snap; no trails; document unknown cases |
| Fixed large armies 100/250/500/1000/2000+ ON/OFF | Record real frame distributions/capture/eligibility cost separately from synthetic results |

Gameplay stays normal 30 TPS, unchanged speed/results; 30 FPS remains reference.
Repeat same-build deterministic replay/save/CRC checks and later LAN scenarios.
Any selection/command regression blocks broader/default-on promotion.

## Risks and next-stage decision

Recommend A: additional ground hardening/manual acceptance before root orientation.
Evidence: working Stage 3C.2 developer A/B, preserved math tests, generalized policy
source audit, 106 passing tests and bounded synthetic memory/helper measurements.
Open: expanded runtime/GPU/selection/FX/mod behavior, short unknown relocations,
center-vs-mesh/frustum edge differences, phase mismatch when turning, actual
linked-list/query/render cost and runtime CRC/save/replay/LAN. Synthetic tests
cannot close these risks. Keep the gate OFF by default. No orientation/turret/
aircraft/projectile/network/WASD/rebinding/renderer scope is implemented or implied.

## Final synthetic measurements

All cells are median milliseconds for 500 traversals (five repetitions).
Counts refer to synthetic histories, not live game objects. Divide a cell by
count * 500 and multiply by 1,000,000 for nanoseconds per object.

### Generals

| Operation | 100 | 250 | 500 | 1000 | 2000 | 5000 |
|---|---:|---:|---:|---:|---:|---:|
| off_draw_branch | 0.0942 | 0.2338 | 0.4669 | 0.9450 | 1.9931 | 4.7905 |
| off_hud_branch | 0.0273 | 0.0622 | 0.1205 | 0.2369 | 0.4813 | 1.1740 |
| on_eligibility_policy | 0.4254 | 0.6189 | 1.2542 | 2.6357 | 5.4530 | 13.5194 |
| on_capture_traversal | 0.4091 | 0.5638 | 1.1231 | 2.2543 | 5.2884 | 12.0384 |
| on_render_matrix | 0.2580 | 0.6386 | 1.2758 | 2.5520 | 5.1015 | 18.3449 |
| on_hud_selection_position | 0.4759 | 0.7635 | 1.3555 | 3.3253 | 6.0524 | 19.7306 |
| on_combined | 0.4860 | 1.1952 | 2.3957 | 5.1124 | 11.6786 | 26.6771 |

At 5000, measured ns/object: off_draw_branch 1.916, off_hud_branch 0.470, on_eligibility_policy 5.408, on_capture_traversal 4.815, on_render_matrix 7.338, on_hud_selection_position 7.892, on_combined 10.671.

### Zero Hour

| Operation | 100 | 250 | 500 | 1000 | 2000 | 5000 |
|---|---:|---:|---:|---:|---:|---:|
| off_draw_branch | 0.0941 | 0.2339 | 0.4670 | 0.9333 | 1.9787 | 5.2798 |
| off_hud_branch | 0.0280 | 0.0637 | 0.1210 | 0.2375 | 0.4706 | 1.1749 |
| on_eligibility_policy | 0.2586 | 0.6477 | 1.2972 | 2.5829 | 5.7060 | 13.9061 |
| on_capture_traversal | 0.2285 | 0.6294 | 1.1303 | 2.5808 | 5.3167 | 12.2653 |
| on_render_matrix | 0.2580 | 0.6477 | 1.2820 | 2.5643 | 5.9096 | 13.6580 |
| on_hud_selection_position | 0.3614 | 0.7288 | 1.3658 | 2.8914 | 5.8683 | 15.8613 |
| on_combined | 0.4791 | 1.1981 | 2.4082 | 6.6725 | 10.9386 | 25.9225 |

At 5000, measured ns/object: off_draw_branch 2.112, off_hud_branch 0.470, on_eligibility_policy 5.562, on_capture_traversal 4.906, on_render_matrix 5.463, on_hud_selection_position 6.345, on_combined 10.369.

The full runtime adapter also queries Object/AI, terrain/airborne state and virtual
module capabilities at capture/draw/HUD/selection consumption. These costs are not
in the prepared-policy microbenchmark. Live profiling is required before judging
large-army performance or caching eligibility. OFF avoids the capture traversal.

## Final validation record

Final rebuilt surface-guard candidate: configure/build PASS; CTest 2/2 PASS;
direct tests 53 Generals + 53 Zero Hour PASS; both opt-in benchmarks PASS.
Successful build logs contain 248 unique existing compiler warning diagnostics,
zero on changed/new lines. No unrelated warning cleanup. Final edits after these
builds are documentation and whitespace only. Tracked and new-file whitespace
checks pass. No game launch, user-data write, Steam write, install, commit or push.

## Final runtime candidate and integrity

Fresh final candidate: `build/dev-runtimes/zh/stage3c3-ground-hardening/game`.
Staged from the Steam source with guarded Stage-ZHRuntime, using the rebuilt
Zero Hour Release executable and matching PDB. No existing candidate overwritten.
The preliminary pre-surface-audit copy was moved intact to
`build/dev-runtimes/zh/stage3c3-ground-hardening-pre-surface-audit` before final
staging; its original metadata paths still refer to its original staging location.
Do not use that preliminary copy for acceptance or through the guarded launcher.

PE/PDB identity: GUID `5706bc8d-51e9-49fc-9096-a636d0fd56be`, age 11.

| Evidence | SHA-256 |
|---|---|
| Final EXE | `E6C2A68FE5813CFC93C9F30CF1727460CE02053A54D79F76D90B8A5C5B24C1D7` |
| Final PDB | `A7F3C4B090A8F0558C1BFA78A038E26721CA9FA8132D877F4E0B820E9B88D831` |
| Source inventory JSON | `25045D3C968BB5F55B4DFCCD7080108551906A6B120B86148748C8CDE95FF8E3` |
| Final runtime inventory JSON | `00154547FD59CF75E8DF22F5A58C158571BF36C9A36185E7F959BCD0515042D3` |

| Read-only full inventory check | Result |
|---|---|
| Steam source (-CheckSource) | PASS: 369 paths, lengths and SHA-256 values |
| stage3c3-ground-hardening | PASS: 369 paths, lengths and SHA-256 values |
| baseline | PASS: 369 paths, lengths and SHA-256 values |
| stage2-timing | PASS: 369 paths, lengths and SHA-256 values |
| stage3a-fps-options | PASS: 369 paths, lengths and SHA-256 values |
| stage3c2-ground-interpolation | PASS: 369 paths, lengths and SHA-256 values |

Source inventory covers 3,035,133,563 bytes. Staging rechecked the source after
copy. Final runtime overlays only the development EXE/PDB; full inventories and
manifest are retained with the candidate. Hash checks prove integrity, not runtime
compatibility or gameplay/performance. No launch/ValidateOnly launch/backup was run.

## Reviewed source and evidence

The changed-file record below lists implementation/test/doc edits. Read-only audit
also covered Stage 3C.2 documentation, FramePacer/PresentationTiming, both
GameClient capture/update paths, GameEngine scheduler/message ordering,
SelectionXlat/CommandXlat/InGameUI, Object/Thing transform and containment paths,
ScriptActions, AIStateMachine/AIStates/AIUpdate/Locomotor, Contain modules,
Dozer/Worker/rebuild paths, W3DModel/Tank/Truck/TankTruck/PoliceCar/Overlord and
aircraft draw paths, DrawModule defaults, Weapon launch/bone/projectile logic,
LaserUpdate, ParticleUplinkCannonUpdate, drawable HUD/decals/shadows and
runtime staging/validation scripts. Stock BIG Object declarations and isolated
x86 compiler layout probes supplement this source review; neither is a game test.

## Exact final Git review record

Branch `dev/modern-engine`; HEAD `8fdc9c939597634af6887a9dc743a44f0b271588`.
Started clean; final changes are intentionally uncommitted. No staged changes.

`git status --short`:

```text
 M Core/GameEngine/CMakeLists.txt
 M Core/GameEngine/Include/Common/GroundTranslation.h
 M Core/GameEngine/Source/Common/GroundTranslation.cpp
 M Core/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp
 M Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp
 M Generals/Code/GameEngine/Include/GameClient/Drawable.h
 M Generals/Code/GameEngine/Source/GameClient/Drawable.cpp
 M Generals/Code/GameEngine/Source/GameLogic/AI/AIStates.cpp
 M Generals/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate.cpp
 M Generals/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/RailroadGuideAIUpdate.cpp
 M GeneralsMD/Code/GameEngine/Include/GameClient/Drawable.h
 M GeneralsMD/Code/GameEngine/Source/GameClient/Drawable.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AIStates.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/RailroadGuideAIUpdate.cpp
 M Tests/Google/Core/CMakeLists.txt
 M Tests/Google/Core/GameEngine/Common/GroundTranslationTest.cpp
 M docs/modernization/AGENT_HANDOFF.md
 M docs/modernization/ROADMAP.md
 M docs/modernization/TIMING.md
?? Core/GameEngine/Include/Common/GroundTranslationPolicy.h
?? Tests/Google/Core/GameEngine/Common/GroundTranslationBenchmark.cpp
?? Tests/Google/Core/GameEngine/Common/GroundTranslationPolicyTest.cpp
?? docs/modernization/STAGE3C3_GROUND_HARDENING.md
```

`git diff --stat` (tracked changes only, exactly as Git reports):

```text
 Core/GameEngine/CMakeLists.txt                     |  1 +
 Core/GameEngine/Include/Common/GroundTranslation.h |  8 ++-
 .../GameEngine/Source/Common/GroundTranslation.cpp | 44 ++++++++++----
 .../GameClient/Drawable/Draw/W3DModelDraw.cpp      |  5 +-
 .../Source/W3DDevice/GameClient/W3DView.cpp        |  7 +--
 .../Code/GameEngine/Include/GameClient/Drawable.h  |  1 +
 .../Code/GameEngine/Source/GameClient/Drawable.cpp | 69 +++++++++++++++-------
 .../GameEngine/Source/GameLogic/AI/AIStates.cpp    | 17 ++++++
 .../Source/GameLogic/Object/Update/AIUpdate.cpp    |  4 ++
 .../Update/AIUpdate/RailroadGuideAIUpdate.cpp      |  6 ++
 .../Code/GameEngine/Include/GameClient/Drawable.h  |  1 +
 .../Code/GameEngine/Source/GameClient/Drawable.cpp | 69 +++++++++++++++-------
 .../GameEngine/Source/GameLogic/AI/AIStates.cpp    | 17 ++++++
 .../Source/GameLogic/Object/Update/AIUpdate.cpp    |  4 ++
 .../Update/AIUpdate/RailroadGuideAIUpdate.cpp      |  6 ++
 Tests/Google/Core/CMakeLists.txt                   |  2 +
 .../GameEngine/Common/GroundTranslationTest.cpp    |  7 +--
 docs/modernization/AGENT_HANDOFF.md                | 19 ++++++
 docs/modernization/ROADMAP.md                      | 19 ++++++
 docs/modernization/TIMING.md                       | 19 ++++++
 20 files changed, 256 insertions(+), 69 deletions(-)
```

Untracked additions excluded from the tracked diff stat:

- `Core/GameEngine/Include/Common/GroundTranslationPolicy.h`
- `Tests/Google/Core/GameEngine/Common/GroundTranslationBenchmark.cpp`
- `Tests/Google/Core/GameEngine/Common/GroundTranslationPolicyTest.cpp`
- `docs/modernization/STAGE3C3_GROUND_HARDENING.md`

The new report itself includes this record; builds/evidence/runtime directories
are ignored. Final `git diff --check` and individual untracked-file whitespace
checks pass. Clock, scheduler, capture traversal, RNG, physics/particle timing,
serialization and command writers have no diff. Stop here for developer review.
