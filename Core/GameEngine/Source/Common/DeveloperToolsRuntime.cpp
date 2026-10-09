/* Copyright 2026 TheSuperHackers. SPDX-License-Identifier: GPL-3.0-or-later */
#include "PreRTS.h"
#include "Common/DeveloperHarness.h"
#include "Common/PerformanceProfile.h"
#include "Common/GameEngine.h"
#include "Common/GlobalData.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/RandomValue.h"
#include "Common/Recorder.h"
#include "Common/MessageStream.h"
#include "Common/GroundTranslation.h"
#include "Common/FramePacer.h"
#include "GameNetwork/GameInfo.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Pathfinder/PathfindCell.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameClient/MapUtil.h"
#include "GameClient/GameClient.h"
#include <cstring>
#include "GameClient/KeyDefs.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/Display.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace DeveloperTools
{
namespace
{
bool scenario=false,scenarioValid=false,dev=false,initialized=false,overlay=true,startedGame=false;
Controller controller;
constexpr unsigned Seed=0x4a3001;
const char* Map="Maps\\Twilight Flame\\Twilight Flame.map";
std::array<ObjectID,ArmySize> units{};
std::array<WorkloadPair,ArmySize> workload{};
unsigned unitCount=0,devUnits=0,pendingActions=0,pendingEvents=0,armedKeys=0,startTick=0,endTick=0;
unsigned startCRC=0,endCRC=0,startRNG=0,endRNG=0,mapCRC=0;
PerformanceProfile::Tick deadline=0,nextOverlay=0,previousOverlay=0;
unsigned previousTicks=0;
DisplayString* text=nullptr;
std::string failure;
FailureResult failureResult;
StartupProgress progress;
SelectionStats selection;
const char* rejectNames[]={"interface_or_radius","movement_layer","start_footprint","start_adjustment","goal_adjustment","forward_connectivity","reverse_connectivity"};
std::array<unsigned,7> rejectCounts{};
std::array<WorkloadPair,7> rejectExamples{};
unsigned failedUnit=ArmySize;
const char* failureCheck="unavailable";
Coord3D failedCurrent{},failedGoal{},failedAdjusted{};
GoalTrace goalTrace;
void snapshotGoal(const char* stage)
{
    // Fixed suspect index from real Fix5 evidence. Getter-only observation;
    // no adjustDestination/quick-path calls, reservations, RNG, or mutations.
    constexpr unsigned index=76;
    if(!scenario || unitCount<=index || !TheAI || !TheGameLogic)return;
    auto* obj=TheGameLogic->findObjectByID(units[index]);auto* ai=obj?obj->getAIUpdateInterface():nullptr;
    auto* pathfinder=TheAI->pathfinder();if(!ai || !pathfinder)return;
    GoalSnapshot sample;sample.stage=stage;sample.tick=TheGameLogic->getFrame();sample.orders=progress.orders;
    sample.object=obj->getID();sample.ignored=ai->getIgnoredObstacleID();
    sample.radius=obj->getGeometryInfo().getBoundingCircleRadius();sample.surfaces=ai->getLocomotorSet().getValidSurfaces();
    const auto startLayer=TheTerrainLogic->getLayerForDestination(obj->getPosition());
    if(const auto* startCell=pathfinder->getCell(startLayer,obj->getPosition()))sample.startZone=startCell->getZone();
    if(auto* player=obj->getControllingPlayer()){sample.owner=player->getPlayerIndex();sample.playerType=player->getPlayerType();}
    sample.current={obj->getPosition()->x,obj->getPosition()->y};sample.goal=workload[index].goal;
    sample.ownGoalX=ai->getPathfindGoalCell()->x;sample.ownGoalY=ai->getPathfindGoalCell()->y;
    Coord3D goal;goal.x=sample.goal.x;goal.y=sample.goal.y;goal.z=TheTerrainLogic->getGroundHeight(goal.x,goal.y);
    const auto layer=TheTerrainLogic->getLayerForDestination(&goal);sample.layer=layer;
    ICoord2D center;pathfinder->worldToCell(&goal,&center);
    unsigned next=0;
    for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)
    {
        auto& c=sample.cells[next++];c.x=center.x+dx;c.y=center.y+dy;
        const auto* cell=pathfinder->getCell(layer,c.x,c.y);if(!cell)continue;
        c.type=cell->getType();c.flags=cell->getFlags();c.zone=cell->getZone();c.goalID=cell->getGoalUnit();c.obstacleID=cell->getObstacleID();
        c.ignoredObstaclePresent=cell->isObstaclePresent(ai->getIgnoredObstacleID());
        if(auto* obstacle=TheGameLogic->findObjectByID(cell->getObstacleID()))
        {
            if(auto* owner=obstacle->getControllingPlayer())c.obstacleOwner=owner->getPlayerIndex();
            std::strncpy(c.obstacleTemplate.data(),obstacle->getTemplate()->getName().str(),c.obstacleTemplate.size()-1);
        }
        if(auto* other=TheGameLogic->findObjectByID(cell->getGoalUnit()))
        {if(auto* owner=other->getControllingPlayer())c.goalOwner=owner->getPlayerIndex();c.goalRelationship=obj->getRelationship(other);}
    }
    goalTrace.append(sample);
}
bool rejectPair(unsigned reason,const WorkloadPair& pair)
{
    if(!rejectCounts[reason])rejectExamples[reason]=pair;
    ++rejectCounts[reason];return false;
}
void writeFailureDetails()
{
    // Fixed 96-unit/7-reason snapshot, written only after measured scopes close.
    std::ofstream trace(std::filesystem::path(PerformanceProfile::outputDirectory())/"goal-relocation-trace.json");
    if(!writeGoalTrace(trace,goalTrace))OutputDebugStringA("Developer scenario: goal trace write failed.\n");
    std::ofstream out(std::filesystem::path(PerformanceProfile::outputDirectory())/"workload-failure-details.json");
    out.imbue(std::locale::classic());out<<std::setprecision(9);
    auto point=[&out](const Coord3D& p){out<<"["<<p.x<<","<<p.y<<","<<p.z<<"]";};
    out<<"{\"schema\":1,\"failed_unit_index\":"<<failedUnit<<",\"check\":"<<std::quoted(failureCheck)
       <<",\"current\":";point(failedCurrent);out<<",\"goal\":";point(failedGoal);out<<",\"adjusted\":";point(failedAdjusted);
    out<<",\"rejections\":[";
    for(unsigned i=0;i<rejectCounts.size();++i){if(i)out<<",";out<<"{\"reason\":"<<std::quoted(rejectNames[i])<<",\"count\":"<<rejectCounts[i]
        <<",\"example_start\":["<<rejectExamples[i].start.x<<","<<rejectExamples[i].start.y<<"],\"example_goal\":["<<rejectExamples[i].goal.x<<","<<rejectExamples[i].goal.y<<"]}";}
    out<<"],\"units\":[";
    for(unsigned i=0;i<unitCount;++i){if(i)out<<",";auto* obj=TheGameLogic->findObjectByID(units[i]);
        out<<"{\"index\":"<<i<<",\"object_id\":"<<units[i]<<",\"template\":"<<std::quoted(i%2?"ChinaTankBattleMaster":"ChinaInfantryRedguard")
           <<",\"owner\":"<<(obj && obj->getControllingPlayer()?obj->getControllingPlayer()->getPlayerIndex():-1)
           <<",\"order_issued\":"<<(i<progress.orders)<<",\"prepared_start\":["<<workload[i].start.x<<","<<workload[i].start.y
           <<"],\"prepared_goal\":["<<workload[i].goal.x<<","<<workload[i].goal.y<<"],\"current\":";
        if(obj)point(*obj->getPosition());else out<<"null";out<<"}";}
    out<<"]}\n";
    if(!out)OutputDebugStringA("Developer scenario: workload diagnostic write failed.\n");
}
bool checkpointPending=false;
void flushCheckpoint()
{
    if(!checkpointPending)return;
    checkpointPending=false;
    std::ofstream out(std::filesystem::path(PerformanceProfile::outputDirectory())/"scenario-startup.json");
    if(!writeStartupProgress(out,progress))OutputDebugStringA("Developer scenario: startup checkpoint write failed.\n");
}
void checkpoint(const char* stage)
{
    if(!scenario || !PerformanceProfile::configured() || std::strcmp(progress.stage,stage)==0)return;
    progress.stage=stage;progress.state=controller.state();
    progress.tick=TheGameLogic?TheGameLogic->getFrame():0;progress.units=unitCount;
    progress.introComplete=TheGameClient && TheGameClient->isStartupIntroComplete();
    progress.moviePlaying=TheDisplay && TheDisplay->isMoviePlaying();
    progress.logicReady=TheGameLogic && !TheGameLogic->isLoadingMap() && !TheGameLogic->isLoadingSave() && !TheGameLogic->isClearingGameData();
    progress.profilerRunning=PerformanceProfile::runtimeStatus().running;
    checkpointPending=true;
    // Tick callbacks only update bounded memory; persist between measured outer frames.
    if(!TheGameLogic || !TheGameLogic->isInGameLogicUpdate())flushCheckpoint();
}
bool offline()
{
    return TheGameLogic && TheGameLogic->getGameMode()==GAME_SKIRMISH && !TheNetwork &&
        (!TheRecorder || !TheRecorder->isPlaybackMode()) && ThePlayerList;
}
void recordFailure(const char* reason)
{
    failureResult.state=controller.state();failureResult.tick=TheGameLogic?TheGameLogic->getFrame():0;
    failureResult.units=unitCount;failureResult.gameRequested=startedGame;
    failureResult.profiling=PerformanceProfile::runtimeStatus().running;
    failureResult.startTick=startTick;failureResult.endTick=endTick;
    failureResult.orders=progress.orders;failureResult.warmup=progress.warmupTicks;
    failureResult.selection=selection;failureResult.failedUnit=failedUnit;failureResult.check=failureCheck;
    failureResult.configured=PerformanceProfile::configured();
    failure=reason;
}
void fail(const char* reason)
{
    if(controller.state()==State::Complete || controller.state()==State::Failed)return;
    recordFailure(reason);controller.fail();pendingEvents=0;
}
unsigned objectCount()
{
    unsigned result=0;for(Object* obj=TheGameLogic->getFirstObject();obj;obj=obj->getNextObject())++result;return result;
}
// All fingerprint words are encoded explicitly; no padding, pointers, clocks or RNG draws.
void hashWord(std::uint64_t& hash,std::uint64_t word)
{ for(unsigned i=0;i<8;++i){hash^=(word>>(i*8))&255;hash*=1099511628211ULL;} }
void reportBenchmark(const PerformanceProfile::Recorder& r,const PerformanceProfile::Metadata& meta,const char* base,bool written)
{
    if(!scenario)return;
    bool valid=written && controller.state()==State::Finalizing && meta.mode=="scenario_ticks" &&
        !r.errors() && !r.droppedPaths() && r.deepPaths() && endTick-startTick==CaptureTicks;
    std::uint64_t fingerprint=14695981039346656037ULL;
    unsigned internal=0,severe=0;std::uint64_t phaseErrors=0,maxQueue=0,completedTicks=0;
    for(const auto& f:r.frames())completedTicks+=f.counters[static_cast<unsigned>(PerformanceProfile::Counter::CompletedTicks)];
    for(const auto& p:r.paths())
    {
        phaseErrors+=p.phases.errors;maxQueue=std::max(maxQueue,static_cast<std::uint64_t>(p.queueBefore));
        if(p.kind==PerformanceProfile::PathKind::Internal){++internal;if(p.inclusive*1000.0/meta.frequency>40)++severe;}
        for(auto v:{p.id,p.parent,static_cast<PerformanceProfile::Tick>(p.kind),static_cast<PerformanceProfile::Tick>(p.request),
            static_cast<PerformanceProfile::Tick>(p.logic-startTick),static_cast<PerformanceProfile::Tick>(p.object),static_cast<PerformanceProfile::Tick>(p.outcome)})hashWord(fingerprint,v);
        hashWord(fingerprint,p.fromX);hashWord(fingerprint,p.fromY);hashWord(fingerprint,p.toX);hashWord(fingerprint,p.toY);
        for(auto v:p.work)hashWord(fingerprint,v);
    }
    valid=valid && !phaseErrors && internal>0 && completedTicks==CaptureTicks;
    if(valid)controller.finish(true);else fail("incomplete_capture_or_profiler_errors_or_no_internal_searches");
    std::ofstream out(std::string(base)+"-benchmark.json");
    BenchmarkResult result;
    result.state=controller.state();result.reason=failure.c_str();result.map=Map;result.mapCRC=mapCRC;result.seed=Seed;result.actualSeed=GetGameLogicRandomSeed();
    result.units=unitCount;result.objects=objectCount();result.startTick=startTick;result.endTick=endTick;result.startCRC=startCRC;result.endCRC=endCRC;
    result.startRNG=startRNG;result.endRNG=endRNG;result.internal=internal;result.severe=severe;result.fingerprint=fingerprint;
    result.maxQueue=maxQueue;result.phaseErrors=phaseErrors;result.completedTicks=completedTicks;
    writeBenchmarkResult(out,r,meta,std::filesystem::path(base).filename().string().c_str(),result);
    out.close();if(!out){recordFailure("benchmark_metadata_write_failed");controller.fail();}
}
bool startScenarioGame()
{
    if(TheNetwork){fail("scenario_network_present");return false;}
    if(!TheGameLogic || !TheMapCache || !ThePlayerTemplateStore || !TheMessageStream || !TheWritableGlobalData || !TheFramePacer)
        {fail("scenario_startup_subsystem_unavailable");return false;}
    if(TheRecorder && TheRecorder->isPlaybackMode()){fail("scenario_replay_active");return false;}
    if(TheGameLogic->isInGame() && !TheGameLogic->isInShellGame()){fail("scenario_live_match_present");return false;}
    const MapMetaData* map=TheMapCache->findMap(Map);
    int faction=-1;
    for(int i=0;i<ThePlayerTemplateStore->getPlayerTemplateCount();++i)
        if(ThePlayerTemplateStore->getNthPlayerTemplate(i)->getName()==AsciiString("FactionChina")){faction=i;break;}
    if(!map){fail("scenario_map_not_cached");return false;}
    if(!map->m_isMultiplayer || !map->m_doesExist || map->m_numPlayers<2){fail("scenario_map_not_available_for_two_players");return false;}
    if(faction<0){fail("scenario_china_template_missing");return false;}
    mapCRC=map->m_CRC;
    if(TheGameLogic->isInShellGame())TheGameLogic->clearGameData(FALSE);
    checkpoint("allocating_skirmish_info");
    if(!ensureScenarioGameInfo()){fail("scenario_skirmish_info_unavailable");return false;}
    auto* game=TheSkirmishGameInfo;game->init();game->clearSlotList();game->setLocalIP(0);game->enterGame();
    for(int i=0;i<2;++i){GameSlot slot;slot.setState(i?SLOT_EASY_AI:SLOT_PLAYER,UnicodeString(L"Performance Scenario"));
        slot.setIP(i?1:0);slot.setColor(i);slot.setPlayerTemplate(faction);slot.setStartPos(i);slot.setTeamNumber(0);slot.setAccept();game->setSlot(i,slot);}
    Money cash;cash.setStartingCash(10000);game->setStartingCash(cash);
    game->setMap(Map);game->setMapCRC(map->m_CRC);game->setMapSize(map->m_filesize);game->setSeed(Seed);game->setUseStats(FALSE);game->startGame(0);
    progress.slotsReady=true;checkpoint("slots_ready");
    TheWritableGlobalData->m_mapName=Map;
    TheWritableGlobalData->m_framesPerSecondLimit=120;
    TheFramePacer->setFramesPerSecondLimit(120); // Runtime-only scenario setting, never persisted.
    InitRandom(Seed); // Same ordinary new-game initialization as the skirmish menu; gated scenario only.
    checkpoint("requesting_map");
    auto* msg=TheMessageStream->appendMessage(GameMessage::MSG_NEW_GAME);
    msg->appendIntegerArgument(GAME_SKIRMISH);msg->appendIntegerArgument(DIFFICULTY_NORMAL);msg->appendIntegerArgument(0);
    msg->appendIntegerArgument(TheGlobalData->m_framesPerSecondLimit);
    progress.mapRequested=true;checkpoint("map_requested");
    return true;
}
// All preflight calls use existing public legality/zone services. No path search
// is invoked here and no pathfinder policy is modified.
bool validateWorkloadPair(WorkloadPair& pair,void* context)
{
    auto* obj=static_cast<Object*>(context);auto* ai=obj->getAIUpdateInterface();
    auto* pathfinder=TheAI?TheAI->pathfinder():nullptr;
    if(!ai || !pathfinder || obj->getGeometryInfo().getBoundingCircleRadius()>18.0f)return rejectPair(0,pair);
    auto position=[](WorkloadPoint point){Coord3D pos;pos.x=point.x;pos.y=point.y;pos.z=TheTerrainLogic->getGroundHeight(pos.x,pos.y);return pos;};
    Coord3D start=position(pair.start),goal=position(pair.goal);
    if(TheTerrainLogic->getLayerForDestination(&start)!=LAYER_GROUND || TheTerrainLogic->getLayerForDestination(&goal)!=LAYER_GROUND)return rejectPair(1,pair);
    obj->setPosition(&start);
    const auto& locomotors=ai->getLocomotorSet();
    if(!pathfinder->validMovementPosition(obj->getCrusherLevel()>0,LAYER_GROUND,locomotors,&start))return rejectPair(2,pair);
    if(!pathfinder->adjustDestination(obj,locomotors,&start))return rejectPair(3,pair);
    start.z=TheTerrainLogic->getGroundHeight(start.x,start.y);obj->setPosition(&start);
    if(!pathfinder->adjustDestination(obj,locomotors,&goal))return rejectPair(4,pair);
    if(!pathfinder->clientSafeQuickDoesPathExist(locomotors,&start,&goal))return rejectPair(5,pair);
    if(!pathfinder->clientSafeQuickDoesPathExist(locomotors,&goal,&start))return rejectPair(6,pair);
    pair={{start.x,start.y},{goal.x,goal.y}};return true;
}
bool spawnScenarioArmy(Player* player,const ThingTemplate* tank,const ThingTemplate* infantry)
{
    unsigned cursor=0;
    for(unsigned i=0;i<ArmySize;++i)
    {
        Object* obj=TheThingFactory->newObject(i%2?tank:infantry,player->getDefaultTeam());
        obj->setOrientation(0);
        if(!selectWorkloadPair(cursor,workload.data(),unitCount,workload[unitCount],validateWorkloadPair,obj,&selection))return false;
        const auto& point=workload[unitCount].start;Coord3D pos;pos.x=point.x;pos.y=point.y;pos.z=TheTerrainLogic->getGroundHeight(pos.x,pos.y);
        obj->setPosition(&pos);units[unitCount++]=obj->getID();progress.units=unitCount;
        if(unitCount==77)snapshotGoal("pair_accepted");
    }
    snapshotGoal("army_prepared");
    return true;
}
bool spawn(Player* player,unsigned count,bool benchmark)
{
    if(!player || !player->isPlayerActive() || !TheTerrainLogic || !TheThingFactory)return false;
    const ThingTemplate* tank=TheThingFactory->findTemplate("ChinaTankBattleMaster",FALSE);
    const ThingTemplate* infantry=TheThingFactory->findTemplate("ChinaInfantryRedguard",FALSE);
    if(!tank || !infantry || (benchmark && count>units.size()) || (!benchmark && !canSpawn(devUnits,count)))return false;
    if(benchmark)return count==ArmySize && spawnScenarioArmy(player,tank,infantry);
    Region3D bounds;TheTerrainLogic->getExtent(&bounds);
    if(bounds.hi.x-bounds.lo.x<1000 || bounds.hi.y-bounds.lo.y<1000)return false;
    for(unsigned i=0;i<count;++i)
    {
        Coord3D pos;pos.x=bounds.lo.x+(bounds.hi.x-bounds.lo.x)*0.22f+(i%12)*24.0f;
        pos.y=bounds.lo.y+(bounds.hi.y-bounds.lo.y)*0.22f+(i/12)*24.0f;
        pos.z=TheTerrainLogic->getGroundHeight(pos.x,pos.y);
        Object* obj=TheThingFactory->newObject(i%2?tank:infantry,player->getDefaultTeam());obj->setOrientation(0);obj->setPosition(&pos);
        if(!obj->getAIUpdateInterface())return false;
        if(benchmark){units[unitCount++]=obj->getID();progress.units=unitCount;}else ++devUnits;
    }
    return true;
}
void orders(unsigned relative)
{
    if(relative!=WarmupTicks)return; // One move per unit, never interrupt an unresolved path.
    auto* pathfinder=TheAI?TheAI->pathfinder():nullptr;
    if(!pathfinder){fail("workload_pathfinder_unavailable");return;}
    snapshotGoal("before_orders");
    for(unsigned i=0;i<unitCount;++i)
    {
        auto* obj=TheGameLogic->findObjectByID(units[i]);
        auto* ai=obj?obj->getAIUpdateInterface():nullptr;
        if(!ai || obj->isEffectivelyDead()){fail("workload_unit_unavailable");return;}
        Coord3D pos;pos.x=workload[i].goal.x;pos.y=workload[i].goal.y;pos.z=TheTerrainLogic->getGroundHeight(pos.x,pos.y);
        Coord3D adjusted=pos;
        // Require the prepared goal to remain legal after warmup; don't silently
        // turn an invalid goal into a distant Closest benchmark.
        // Preserve check order/short-circuiting; identify the FIRST rejected check.
        const char* rejected=nullptr;
        if(!pathfinder->adjustDestination(obj,ai->getLocomotorSet(),&adjusted))rejected="goal_adjustment_failed";
        else if((adjusted.x-pos.x)*(adjusted.x-pos.x)+(adjusted.y-pos.y)*(adjusted.y-pos.y)>0.01f)rejected="goal_relocated_after_warmup";
        else if(!pathfinder->clientSafeQuickDoesPathExist(ai->getLocomotorSet(),obj->getPosition(),&pos))rejected="forward_connectivity_after_warmup";
        if(rejected)
        {
            snapshotGoal("preflight_rejected");
            failedUnit=i;failureCheck=rejected;failedCurrent=*obj->getPosition();failedGoal=pos;failedAdjusted=adjusted;
            fail("workload_goal_changed_or_unreachable");return;
        }
        ai->aiMoveToPosition(&pos,CMD_FROM_SCRIPT);++progress.orders;
        snapshotGoal("after_order");
    }

}
}
bool ensureScenarioGameInfo()
{
    if(!scenario || !scenarioValid || TheNetwork || !TheGlobalData)return false;
    // The normal Skirmish menu allocates this lazily; automatic scenarios bypass it.
    // GameEngine owns and deletes the singleton on shutdown.
    if(!TheSkirmishGameInfo)TheSkirmishGameInfo=NEW SkirmishGameInfo;
    return true;
}
bool configureScenario(const char* value){scenario=true;scenarioValid=validScenario(value);return scenarioValid;}
bool scenarioEnabled(){return scenario;}
void enableDevMode(){dev=true;}
bool devModeEnabled(){return dev;}
void beginOuter()
{
    if(!scenario && !dev)return;
    try
    {
        if(!scenario)beginInteractiveOuter();
        if(scenario)flushCheckpoint();
        if(scenario && !initialized)
        {
            initialized=true;controller=Controller(true);overlay=false;
            PerformanceProfile::setAutomaticControl(true);PerformanceProfile::setReportObserver(reportBenchmark);
            deadline=PerformanceProfile::now()+PerformanceProfile::frequency()*180;
            if(!scenarioValid)fail("unknown_scenario");
            else if(!PerformanceProfile::configured() || !PerformanceProfile::runtimeStatus().details)fail("scenario_requires_performanceProfile_and_pathProfile");
            checkpoint("waiting_for_startup");
        }
        if(scenario && controller.state()!=State::Complete && controller.state()!=State::Failed && timedOut(PerformanceProfile::now(),deadline))fail("180_second_timeout");
        if(scenario && !startedGame && controller.state()==State::Setup)
        {
            if(!TheGameClient || !TheDisplay || !TheGameLogic)fail("scenario_startup_subsystem_unavailable");
            else if(readyForScenarioStart(TheGameClient->isStartupIntroComplete(),TheDisplay->isMoviePlaying(),
                TheGameLogic->isLoadingMap(),TheGameLogic->isLoadingSave(),TheGameLogic->isClearingGameData()))
            {
                if(!(startedGame=startScenarioGame()))fail("scenario_map_or_offline_setup_unavailable");
            }
        }
        const unsigned events=pendingEvents;pendingEvents=0;
        if(scenario && (events&Event::Start))
        {
            startTick=TheGameLogic->getFrame();startRNG=GetGameLogicRandomSeedCRC();startCRC=TheGameLogic->getCRC(CRC_RECALC);
            checkpoint("starting_capture");
            if(!PerformanceProfile::startCapture("pathfinding-heavy-v2"))fail("capture_start_failed");
            checkpoint("capture_started");
        }
        if(scenario && (events&Event::Stop))
        {
            endTick=TheGameLogic->getFrame();endRNG=GetGameLogicRandomSeedCRC();endCRC=TheGameLogic->getCRC(CRC_RECALC);
            checkpoint("stopping_capture");
            if(!PerformanceProfile::stopCapture("scenario_ticks"))fail("capture_stop_failed");
            checkpoint("capture_stopped");
        }
        if(scenario && controller.state()==State::Capturing && startTick && !PerformanceProfile::runtimeStatus().running)fail("profiler_stopped_early");
        if(!scenario && (pendingActions&(1u<<static_cast<unsigned>(Action::Capture))))
        {
            pendingActions&=~(1u<<static_cast<unsigned>(Action::Capture));
            if(interactiveOffline()){if(PerformanceProfile::runtimeStatus().running)PerformanceProfile::stopCapture("developer_stop");else PerformanceProfile::startCapture("developer-shortcut");}
        }
        if(scenario && (controller.state()==State::Failed || controller.state()==State::Complete))
        {
            if(controller.state()==State::Failed)
            {
                PerformanceProfile::stopCapture("scenario_failed");
                if(PerformanceProfile::configured()){writeFailureDetails();std::ofstream out(std::filesystem::path(PerformanceProfile::outputDirectory())/"benchmark-failure.json");failureResult.reason=failure.c_str();if(!writeBenchmarkFailure(out,failureResult))OutputDebugStringA("Developer scenario: failure marker write failed.\n");}
            }
            checkpoint(controller.state()==State::Failed?"failed":"complete");
            TheGameEngine->setQuitting(TRUE);
        }
    }
    catch(...){if(scenario){fail("scenario_exception");TheGameEngine->setQuitting(TRUE);}else pendingActions=0;}
}
void logicTick(unsigned frame)
{
    if(!scenario && !dev)return;
    try
    {
        if(scenario)
        {
            if(!startedGame || controller.state()==State::Failed || controller.state()==State::Complete)return;
            if(!offline()){fail("not_offline_skirmish");return;}
            if(TheGameLogic->isLoadingMap() || TheGameLogic->isLoadingSave() || TheGameLogic->isClearingGameData())return;
            if(TheGameInfo!=TheSkirmishGameInfo || !TheGameInfo){fail("scenario_active_game_info_not_ready");return;}
            progress.mapLoaded=true;
            progress.tick=frame;progress.state=controller.state();
            if(controller.state()==State::Warmup)progress.warmupTicks=controller.relative(frame);
            const unsigned events=controller.tick(frame);
            if(events&Event::Setup)
            {
                Player* owner=nullptr;for(int i=0;i<ThePlayerList->getPlayerCount();++i){auto* p=ThePlayerList->getNthPlayer(i);if(p && p->getPlayerType()==PLAYER_COMPUTER && p->isPlayerActive()){owner=p;break;}}
                progress.playersReady=owner!=nullptr;checkpoint("spawning");
                if(!spawn(owner,ArmySize,true)){fail("workload_spawn_failed");return;}
                checkpoint("spawn_complete");controller.prepared(frame);checkpoint("warmup_started");
            }
            if(events&Event::Orders){orders(controller.relative(frame));if(controller.state()==State::Failed)return;checkpoint(events&Event::Start?"warmup_complete":"orders_issued");}
            pendingEvents|=events&(Event::Start|Event::Stop);
        }
        else
        {
            const unsigned actions=pendingActions;pendingActions=0;
            // Capture is handled outside measured scopes in beginOuter.
            pendingActions=actions&(1u<<static_cast<unsigned>(Action::Capture));
            interactiveTick(frame,actions);
        }
    }
    catch(...){if(scenario)fail("workload_exception");else pendingActions=0;}
}
bool consumeKey(unsigned key,unsigned state)
{
    if(!dev || scenario)return false;
    const bool chord=(state&KEY_STATE_CONTROL) && (state&KEY_STATE_SHIFT) && (state&KEY_STATE_ALT);
    const unsigned keys[]={KEY_F5,KEY_F6,KEY_F7,KEY_F8,KEY_F9,KEY_F10,KEY_F11,KEY_F12,KEY_O,KEY_M,KEY_B};
    for(unsigned i=0;i<static_cast<unsigned>(Action::Count);++i)if(key==keys[i])
    {
        const unsigned bit=1u<<i;
        if(!chord && !(armedKeys&bit))return false;
        if(state&KEY_STATE_UP)
        {
            const bool armed=(armedKeys&bit)!=0;armedKeys&=~bit;
            if(armed && allowAction(dev,scenario,interactiveOffline(),static_cast<Action>(i)))
            {if(i==static_cast<unsigned>(Action::Overlay))overlay=!overlay;else pendingActions|=bit;}
        }
        else armedKeys|=bit;
        return true;
    }
    return false;
}
void drawOverlay()
{
    if(!dev || scenario || !overlay || !TheDisplayStringManager || !TheFontLibrary)return;
    const auto now=PerformanceProfile::now();
    if(now>=nextOverlay)
    {
        const auto s=PerformanceProfile::runtimeStatus();const auto freq=PerformanceProfile::frequency();nextOverlay=now+freq/2;
        const unsigned ticks=TheGameLogic?TheGameLogic->getFrame():0;
        const double seconds=previousOverlay?static_cast<double>(now-previousOverlay)/freq:0;
        const double tps=seconds && ticks>=previousTicks?(ticks-previousTicks)/seconds:0;
        const double fps=TheDisplay?TheDisplay->getAverageFPS():0;
        const double frameMs=fps>0?1000.0/fps:0;
        const unsigned objects=interactiveOffline()?TheGameLogic->getObjectCount():0;
        char line[1024];snprintf(line,sizeof(line),"DEV MODE | FPS %.1f frame %.2f ms | TPS %.1f tick %u logic %.2f ms | objects %u\n%s\nLast recorded frame queue %llu cells %llu\nProfiler %s | path %s | %.1f s / manual max60s | records %llu/%u dropped %llu errors %llu\nCtrl+Alt+Shift F5 overlay F6 money F7 instant F8 reveal F9 group F10 army F11 capture F12 large O enemy M move B battle",
            fps,frameMs,tps,ticks,s.logicMs,objects,interactiveStatus(),s.queuedPaths,s.queueCells,!s.configured?"OFF":s.running?"RECORDING":s.frames?"COMPLETE":"READY",s.details?"ON":"OFF",
            s.frequency?static_cast<double>(s.elapsed)/s.frequency:0,s.records,PerformanceProfile::MaxPaths,s.dropped,s.errors);
        if(!text){text=TheDisplayStringManager->newDisplayString();text->setFont(TheFontLibrary->getFont("Arial",12,FALSE));}
        UnicodeString value;value.translate(AsciiString(line));text->setText(value);
        previousOverlay=now;previousTicks=ticks;
    }
    if(text)text->draw(8,8,0xFFFFFFFF,0xFF000000);
}
void shutdown()
{
    if(text && TheDisplayStringManager)TheDisplayStringManager->freeDisplayString(text);text=nullptr;
    shutdownInteractive();
    PerformanceProfile::setReportObserver(nullptr);scenario=dev=initialized=false;
    pendingActions=armedKeys=0;
    std::string().swap(failure);
}
}
