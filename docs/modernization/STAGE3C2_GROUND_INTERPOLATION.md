# Stage 3C.2: gated ground-unit translation

Status: implemented prototype, developer review and ALL runtime acceptance pending.
Started 2026-10-08 on clean `dev/modern-engine`, HEAD `14aed7d51`
(`Add scheduler-aligned presentation timing`). No commit, push, Install or game launch.
The process-local `-groundInterpolation` startup flag enables it in Release;
omitting the flag disables it. No Options/INI persistence or controls changes.

## 1. Architecture and root render boundary

Both titles share GroundTranslationHistory and gate/allowlist code. Each Drawable
owns history. Only Drawable::draw's local matrix and HUD projection coordinates
consume interpolated XYZ. Object, Thing, Drawable canonical getters/setters,
AI, locomotion, partition/collision and command coordinates stay canonical.
No temporary set-and-restore pattern is used.

Verified ordinary path: Object transform -> Drawable::getTransformMatrix (returns
the bound Object matrix) -> Drawable::draw local copy -> instance postmultiply ->
applyPhysicsXform -> DrawModule::doDrawModule -> W3DModelDraw local adjustments ->
RenderObjClass::Set_Transform. W3DTankDraw calls the standard model method before
its tread/emitter work. No assumption that arbitrary DrawModules are compatible.

## 2. Exact sample-capture point

At the start of both GameClient::update methods, after profiler entry and before
FRAME_TICK/input/client updates/views, enumerate firstDrawable/getNextDrawable.
This runs only with the gate enabled, independently of frustum visibility.
GameEngine updates the client BEFORE the next canUpdateGameLogic/preUpdate/logic
pass; FramePacer has already published the previous iteration's completion.
Capture only when GameLogic::hasUpdated() and timing.generation equals
GameLogic::getFrame(). Thus samples represent final completed world positions,
not intermediate setter calls. Same-generation capture is ignored; views and
repeated draws never rotate history. Newly created late-client objects snap until
two later completed samples. No scheduler/update ordering was moved.

## 3. Ownership, lifetime and layout

GroundTranslationHistory is a direct Drawable member: two Vector3 positions,
three unsigned generation/epoch values and two bools (40 bytes with this ABI).
Pool construction initializes fresh history; destruction/reconstruction cannot
reuse an address's old samples. No registry or Object pointers are stored.
Drawable::xfer explicitly enumerates fields and modules; no raw Drawable-layout
snapshot is used. History is absent from xfer and crc, and loadPostProcess clears
it. This changes internal class size/vtable compilation, not on-disk fields or
versions; all engine consumers are rebuilt together. Retail binary injection/ABI
compatibility is not claimed.

## 4. Exact eligibility

All conditions must pass at capture and consumption:

- Developer gate enabled, bound live Object, visible (not hidden, stealth-hidden
  or fully shrouded); offscreen/frustum culling alone does not prevent capture.
- Exact template name: AmericaInfantryRanger, ChinaInfantryRedguard,
  GLAInfantryRebel, AmericaTankCrusader, ChinaTankBattleMaster, GLATankScorpion.
  No inheritance, faction variant or prefix matching.
- Infantry or vehicle; no aircraft, projectile, structure or immobile kind.
- No contained-by Object, no own Contain module, no disabled/dead/airborne/
  significantly-above-terrain/parachuting state.
- AI exists, isDoingGroundMovement(), and AI state is exactly AI_IDLE or
  AI_MOVE_TO. All docking, entry/exit, repair, attack-move, waypoint, guard,
  rappel and other special states fall back. An active scripted camera also
  excludes interpolation. This deliberately reduces coverage, including combat.
- At least one draw module, EVERY module explicitly supports root translation.
  The base DrawModule returns false. W3DModelDraw accepts ONLY actual module keys
  W3DModelDraw/W3DTankDraw and empty AttachToBoneInAnotherModule. Bespoke derived
  classes, attached roots, trucks, helicopters and streams remain excluded.

Mod data reusing these exact names is not automatically proven safe. Stock-data
scope and a disabled-by-default gate are intentional. Turret root XYZ may follow
a tank; turret yaw/pitch and all matrix basis values remain current/canonical.

## 5. Math

Use the existing PresentationTiming pair unchanged. Alpha 0 chooses previous,
0.5 midpoint, 1 current. Project-native Vector3::Lerp computes XYZ. Clamp outside
[0,1], reject NaN, never extrapolate or predict velocity. There is no new timer,
accumulator, FPS-specific render pattern or modulo. Valid consumption requires
matching epoch, generation AND previousGeneration, a consecutive history pair,
and current canonical XYZ exactly matching the captured current sample.

## 6. Matrix composition

Start with canonical current Matrix3D. Replace ONLY its translation in a local
copy. Preserve basis, yaw/pitch/roll and scale. Then use the existing instance
postmultiply, applyPhysicsXform and module/model adjustments in their original
order. No whole-matrix lerp, root rotation or turret interpolation.

## 7. Snaps and invalidation

Canonical fallback: invalid timing, no pair/first sample, epoch or generation
mismatch, unsupported object, stale canonical translation or NaN alpha. Capture
gaps/regressions and steps over 12 world units prime previous=current and require
a fresh consecutive sample. Epoch observation invalidates even on client frames
without a completed tick. Constructor, object binding, loadPostProcess, hidden
state changes and containment notifications explicitly reset history.
Drawable::reactToTransformChange invalidates on position changes outside the
logic update or individual steps over 12 units; it NEVER captures a sample.
Canonical updates during rendering also fail the exact current-position guard.
Clock resets/load/freeze/pause/halt/stalls and unsupported timing policies retain
Stage 3C.1's epoch/invalid semantics. Recovery needs fresh matching samples.

## 8. Teleports, parent changes and containment

Object::friend_setContainedBy invalidates the client history on every call,
covering transport/tunnel/garrison entry AND exit even within the same tick.
These contained states cannot interpolate; exit/entry AI states are excluded.
Drawable binding clears coordinate/lifetime history; attachment modules cannot
qualify. Known ScriptActions setPosition sites (six per title) create/spawn units
or transports/contents rather than move a pre-existing ordinary unit: fresh
construction/binding already primes them. No universal semantic teleport flag
was found. Large per-setter and completed-sample guards plus out-of-logic setter
invalidation are conservative fallbacks, NOT a complete teleport solution.
An unknown in-tick semantic relocation <=12 units ending in an eligible state
can still interpolate. Mods/scripted correction paths need dedicated validation
and explicit hooks before expanding/default-enabling this prototype.

## 9. Picking, selection and HUD policy

W3DView::pickDrawable casts a ray into the rendered W3D scene and resolves the
render object's DrawableInfo to the existing Drawable/Object ID. Interpolation
therefore changes hit geometry by up to roughly one completed tick of travel.
Selection-box iterateDrawablesInRegion projects canonical Drawable positions;
edge cases may disagree with the rendered model. No hit resolver, target ID,
command coordinates, command execution frames or targeting code was changed.
Safe command-picking behavior is NOT established: this is an explicit gated
limitation requiring developer click/drag/attack/move testing before promotion.

Pure HUD anchors use a read-only presentation offset before worldToScreen:
shared health region (health/status/icons/group text), formation letter,
veterancy marker, ammo/container pips, caption and construction text. Their
Object/geometry queries stay canonical. Containment/construction normally fall
back. W3DModelDraw::setTerrainDecal attaches the decal to m_renderObject, so
selection/shadow decals naturally follow its transient root. No separate decal
simulation transform is written. Verify shadows/markers on slopes manually.

## 10. Bones, weapons, attachments and FX

Object single/pristine bone queries call getPristineBonePositions and convert
through canonical Thing/Drawable transforms. Weapon::calcProjectileLaunchPosition
uses pristine launch offsets, authoritative Object matrix and AI turret angles;
it does not obtain interpolated root state. Passenger/transport exit coordinates
remain canonical, and containing/entry/exit states are excluded.
The only GameLogic getCurrentClientBonePositions call sites found in both titles
are ParticleUplinkCannonUpdate connector/fire bones; that structure is excluded.
W3DModelDraw's attachment cache explicitly uses pristine bones for logic.

Existing client bone particle attachments and FX may naturally follow the
RenderObj root. No particle or animation timing was changed. A visual muzzle
versus canonical projectile-origin offset of up to one tick is possible; no
attempt to move authoritative projectile spawn/range/hit/collision was made.
Validate combat, emitters, load continuation and mod-specific bone queries.
The audit establishes source boundaries, not runtime equivalence of all content.

## 11. Existing physics decoration

applyPhysicsXform runs after the new root-translation replacement and existing
instance transform. Its pitch/roll/yaw/Z smoothing and legacy logic-frame phase
are untouched. W3DTankDraw still reads canonical physics velocity for treads and
client emitters. Presentation translation and current orientation can differ in
phase while turning; orientation smoothing is out of scope. No physics state is
written by the interpolation helper.

## 12. Determinism and compatibility audit

No xfer/CRC versions or serialized fields, RNG calls, messages, command IDs,
replay/save/network formats or simulation tick/update order changed. Containment
adds only a client-history reset. Canonical matrix getters remain unchanged.
Stage 3C.1 clock/scheduler/FramePacer and legacy getLogicFramePhase have zero diff;
particle/animation/decorative timing code has zero diff. Network timing remains
invalid and therefore canonical. The feature is not network interpolation.
Picking can affect human command choice; unchanged command encoding is not proof
of identical interactions. Modern MSVC retail CRC compatibility remains unknown
as documented in COMPATIBILITY.md. Same-build CRC/replay/save/LAN acceptance is
still required; no runtime determinism result is claimed.

## 13. Automated validation

GroundTranslationTest covers first/consecutive samples; alpha endpoints/midpoint
and clamping; epoch/gap/reset; invalid timing/unsupported/mismatched pairs;
destruction/reconstruction at the same address; duplicate captures/repeated draws;
final completed position after intermediate mutations; explicit reset/large jump;
stale canonical state; stationary pairs; six synthetic render caps including
144/165; default gate/exact allowlist. Matrix tests assert the input is unchanged,
all basis elements remain identical, and existing instance composition preserves
the expected root offset. Engine Object/GPU integration is source-audited and
manual, not represented as a mocked runtime test.

Release configure/build PASS; CTest 2/2 PASS; 45 Google tests per title,
90 total (17 new cases each). No warnings on changed/new lines; 338 existing
warning diagnostics in the successful build, principally C4018/C4267/C5055.
Changed-file diagnostics are on unchanged Object.cpp/W3DModelDraw.cpp lines.
Configure explicitly warns MSVC 19.51 is not retail CRC-compatible.
Test helpers are included
in BOTH title Google binaries. No Options.ini/user-data architecture refactor.

## 14. Guarded runtime candidate

Target: `build/dev-runtimes/zh/stage3c2-ground-interpolation`, fresh destination.
Existing Stage-ZHRuntime.ps1 copies/hash-verifies the complete explicit Steam
source, overlays only the matching newly built EXE/PDB in the new copy, rechecks
source inventory and records provenance. Test-ZHRuntime.ps1 verifies full runtime
and source inventories; baseline/Stage 2/Stage 3A are checked read-only too.
No Install, Steam writes, existing-runtime overwrite or launch is authorized.
Candidate staged successfully; 369 runtime files and 369 Steam source files
match full path/length/SHA256 inventories. Baseline, Stage 2 and Stage 3A also
pass all 369 files each. No launch occurred. Exact hashes are below.

## 15. Exact developer manual acceptance procedure

After review, in PowerShell 7 from the repository root:

```powershell
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage3c2-ground-interpolation
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage3c2-ground-interpolation -CheckSource
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage3c2-ground-interpolation -BackupUserData -GameArguments @('-groundInterpolation') -ValidateOnly
# Developer only, after reviewing validation and backup destination:
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage3c2-ground-interpolation -BackupUserData -GameArguments @('-groundInterpolation')
# Reference run of identical binary with gate off:
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage3c2-ground-interpolation -BackupUserData
```

Runtime copies share the Windows user-data directory; launcher backup protects
its current contents, not a separate profile. Retain backup paths. Set render FPS
in existing Options, Accept, then test each cap below. Release `-fps` is RTS_DEBUG
only and must not be used to select caps. Use standard USA/China/GLA factions,
not Zero Hour general variants. Compare identical map/unit/order scenarios with
flag on/off. Avoid attack-move/guard/waypoint orders when demonstrating eligibility.
Record actual FPS/TPS where available, scenario, flag, cap and observed result.

| Caps | Required observations for every cap (all pending) |
|---|---|
| 30 | Canonical reference, unchanged speed, no corruption; invalid clock snaps |
| 60, 120, 240 | Walking one/several eligible infantry, straight tank movement, stationary units |
| 144, 165 | Same cases, inspect periodic cadence; no synthetic result substitutes for this |
| All six | Tank turns/physics suspension; fixed and moving camera; slopes; shadows/selection markers/health/status/ammo/veterancy/group/formation/caption |
| All six | Click/drag selection at model edges, move/attack target picking and object identity; weapon muzzle/projectile/results |
| All six | Spawn/destroy; save/load; pause/resume; minimize/restore; frustum exit/reentry |
| All six | Transport/tunnel/garrison entry/exit, parachute/dock/cinematic unsupported fallback; sudden/scripted reposition if practical |

Pass criteria: normal gameplay stays 30 TPS with unchanged speed/results; eligible
translation smoother above 30, no obvious 144/165 cadence, teleport trails, visual
corruption or selection/picking regression. Unsupported units/states snap.
Repeat deterministic save/replay/CRC scenarios with gate off/on and retain logs;
retail compatibility remains a separate compiler/build validation exercise.
ANY selection/target regression blocks promotion. All rows are NOT RUN.

## 16. Known limitations and review risks

Default off; six stock names and idle/move-to only. First-pair/recovery snaps,
one-tick presentation delay, current orientation, AI-state/shroud boundaries may
be visible. 12-unit cutoff is intentionally conservative and may snap fast units;
short in-tick semantic teleports are not universally detected. Mod name reuse,
custom bones, particle save continuation, HUD edge anchors, visual projectile
origins, picking versus canonical box selection, and actual FPS/cadence remain
unverified. Added per-Drawable memory and gated linear capture traversal are not
profiled. No game launch or manual acceptance occurred in this stage.

## 17. Proposed Stage 3C.3

First review and manually validate this exact candidate across six caps. Resolve
picking/box-selection policy and any visual FX/HUD regressions with isolated
presentation APIs, without moving authoritative targeting. Add semantic reset
hooks for demonstrated short relocation/coordinate transitions. Measure cost
and audit stock/template overrides before expanding eligibility. Only then
consider broader ground states/templates. Keep network, orientation/turrets,
aircraft/projectiles, WASD/rebinding and graphics work in separate approved scopes.


## Final review record (22 requested items)

1. Started clean on dev/modern-engine at 14aed7d51; HEAD/branch unchanged.
2. Capture: both GameClient::update entries, before client mutation/views, only
   prior completed GameLogic frame matching PresentationTiming (section 2).
3. Ownership: 40-byte direct Drawable member, pooled lifetime reconstruction,
   reset on bind/load; no address registry or serialization (section 3).
4. Eligibility: exact six names, idle/move-to ground AI, visibility/state and
   all-module allowlist; complete exclusions in section 4.
5. Math: native XYZ Lerp of matching completed pair, clamped alpha, no prediction
   or timer; canonical input unchanged (section 5).
6. Composition: canonical copy -> replace XYZ -> instance -> existing physics ->
   module -> model -> RenderObj (section 6).
7. Snap: first/gap/epoch/generation/stale/invalid plus explicit lifetime/load/
   hidden/containment/large-step/out-of-update hooks (section 7).
8. Teleport: explicit containment hooks and spawn/lifetime reset; 12-unit guards
   are not universal semantic detection; short in-tick moves remain a limitation
   (section 8).
9. Picking: rendered ray/unchanged Object IDs; canonical box selection, no command
   rewrite; presentation HUD/decal anchors; gated regression risk (section 9).
10. Bones/weapons: pristine/canonical launch and exit queries; excluded uplink
    structure uses client bones; visual FX/origin mismatch needs testing (section 10).
11. Physics: original instance/physics/module order, phase and state untouched
    (section 11).
12. Exact source/test/build files changed (documentation additionally listed in
    git status below):

```text
Core/GameEngine/CMakeLists.txt
Core/GameEngine/Source/Common/CommandLine.cpp
Core/GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h
Core/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp
Generals/Code/GameEngine/Include/Common/DrawModule.h
Generals/Code/GameEngine/Include/GameClient/Drawable.h
Generals/Code/GameEngine/Source/GameClient/Drawable.cpp
Generals/Code/GameEngine/Source/GameClient/GameClient.cpp
Generals/Code/GameEngine/Source/GameLogic/Object/Object.cpp
GeneralsMD/Code/GameEngine/Include/Common/DrawModule.h
GeneralsMD/Code/GameEngine/Include/GameClient/Drawable.h
GeneralsMD/Code/GameEngine/Source/GameClient/Drawable.cpp
GeneralsMD/Code/GameEngine/Source/GameClient/GameClient.cpp
GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Object.cpp
Tests/Google/Core/CMakeLists.txt
Core/GameEngine/Include/Common/GroundTranslation.h
Core/GameEngine/Source/Common/GroundTranslation.cpp
Tests/Google/Core/GameEngine/Common/GroundTranslationTest.cpp
```

13. Added GroundTranslationTest.cpp: 12 regular tests + five parameter weights =
    17 cases per title; existing tests unchanged. All minimum history/math cases
    covered as detailed in section 13.
14. cmake --preset win32 and cmake --build --preset win32 PASS, Release x86,
    MSVC 19.51.36260, Ninja Multi-Config, tests ON, retail option DEFAULT,
    RTS_DEBUG OFF. No CMake Install. Initial local compile errors (temporary
    matrix operator in test; nonconst shroud getter) were corrected before the
    successful complete build; no test failure remains.
15. ctest --preset win32 --output-on-failure: 2/2; direct full binaries: 45/45
    each, 90/90 total. git diff --check and untracked-file whitespace checks PASS.
    No compiler warnings introduced on changed/new lines. See ignored build/
    stage3c2-configure.log, stage3c2-build.log, stage3c2-ctest.log and per-title logs.
16. Candidate:

```text
C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\dev-runtimes\zh\stage3c2-ground-interpolation
EXE SHA256: 63D8C2C2900FC9B6CA2B692C99212D9367769E9789FF8C6733FADE66C04EE4BA
PDB SHA256: 9833ADBA367BA9D075728763BDD72A2E46ACDA80715C6A89843F2B26A92F3E0E
EXE/PDB debug GUID: 5706bc8d-51e9-49fc-9096-a636d0fd56be
EXE/PDB age: 8
Source inventory SHA256: 25045D3C968BB5F55B4DFCCD7080108551906A6B120B86148748C8CDE95FF8E3
Runtime inventory SHA256: 6D2BE2AACFBBEDE18F5EC7BD89E830CDF11BDC2CC6195B2BACDE58D24AB76C12
```

    Full runtime/source inventories PASS, 369 files each. Baseline/Stage 2/Stage 3A
    inventories PASS, 369 each; Steam and old runtimes preserved. Manifest records
    HEAD plus dirty/untracked worktree paths; code changes are intentionally not
    committed. Later documentation/line-ending-only edits do not change behavior
    of the staged EXE/PDB pair. No game process launched.
17. Exact guarded launch/reference commands, shared-data backup requirement and
    six-cap scenario/transition acceptance matrix are in section 15. NOT RUN.
18. Exact git status --short:

```text
 M Core/GameEngine/CMakeLists.txt
 M Core/GameEngine/Source/Common/CommandLine.cpp
 M Core/GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h
 M Core/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp
 M Generals/Code/GameEngine/Include/Common/DrawModule.h
 M Generals/Code/GameEngine/Include/GameClient/Drawable.h
 M Generals/Code/GameEngine/Source/GameClient/Drawable.cpp
 M Generals/Code/GameEngine/Source/GameClient/GameClient.cpp
 M Generals/Code/GameEngine/Source/GameLogic/Object/Object.cpp
 M GeneralsMD/Code/GameEngine/Include/Common/DrawModule.h
 M GeneralsMD/Code/GameEngine/Include/GameClient/Drawable.h
 M GeneralsMD/Code/GameEngine/Source/GameClient/Drawable.cpp
 M GeneralsMD/Code/GameEngine/Source/GameClient/GameClient.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Object.cpp
 M Tests/Google/Core/CMakeLists.txt
 M docs/modernization/AGENT_HANDOFF.md
 M docs/modernization/ROADMAP.md
 M docs/modernization/TIMING.md
?? Core/GameEngine/Include/Common/GroundTranslation.h
?? Core/GameEngine/Source/Common/GroundTranslation.cpp
?? Tests/Google/Core/GameEngine/Common/GroundTranslationTest.cpp
?? docs/modernization/STAGE3C2_GROUND_INTERPOLATION.md
```

19. Exact git diff --stat (tracked files only; four untracked files are listed
    above and are deliberately not staged):

```text
 Core/GameEngine/CMakeLists.txt                     |  2 +
 Core/GameEngine/Source/Common/CommandLine.cpp      |  8 +++
 .../W3DDevice/GameClient/Module/W3DModelDraw.h     |  1 +
 .../GameClient/Drawable/Draw/W3DModelDraw.cpp      |  8 +++
 .../Code/GameEngine/Include/Common/DrawModule.h    |  1 +
 .../Code/GameEngine/Include/GameClient/Drawable.h  |  6 ++
 .../Code/GameEngine/Source/GameClient/Drawable.cpp | 77 +++++++++++++++++++++-
 .../GameEngine/Source/GameClient/GameClient.cpp    |  4 ++
 .../GameEngine/Source/GameLogic/Object/Object.cpp  |  3 +
 .../Code/GameEngine/Include/Common/DrawModule.h    |  1 +
 .../Code/GameEngine/Include/GameClient/Drawable.h  |  6 ++
 .../Code/GameEngine/Source/GameClient/Drawable.cpp | 77 +++++++++++++++++++++-
 .../GameEngine/Source/GameClient/GameClient.cpp    |  4 ++
 .../GameEngine/Source/GameLogic/Object/Object.cpp  |  3 +
 Tests/Google/Core/CMakeLists.txt                   |  1 +
 docs/modernization/AGENT_HANDOFF.md                | 15 +++++
 docs/modernization/ROADMAP.md                      | 15 +++++
 docs/modernization/TIMING.md                       | 15 +++++
 18 files changed, 245 insertions(+), 2 deletions(-)
```

20. Source audit: no new serialized history/CRC/RNG/message/replay/save fields,
    simulation ordering or clock/legacy phase changes. Runtime determinism and
    saves/replays/LAN remain untested; MSVC retail CRC incompatibility is an
    existing toolchain constraint (section 12).
21. Risks: picking, short semantic relocation, mod name reuse, FX origin and
    particle continuation, snap/turn phase boundaries, unmeasured traversal/memory
    cost and all actual visual/cadence observations (section 16).
22. Stage 3C.3: review/manual acceptance first, resolve demonstrated picking/
    teleport/HUD/FX issues, measure cost; expand only after evidence. No controls,
    orientation/network/aircraft/projectile scope implied (section 17).
