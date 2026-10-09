/* Copyright 2026 TheSuperHackers. SPDX-License-Identifier: GPL-3.0-or-later */
#include "PreRTS.h"
#include "Common/DeveloperHarness.h"
#include <cstring>
#include <cmath>
#include "Common/PerformanceProfile.h"
#include <iomanip>
#include <ostream>
#include <locale>
namespace DeveloperTools
{
bool validDevOwners(int humanIndex,bool humanActive,bool humanType,
                    int enemyIndex,bool enemyActive,bool enemyComputer)
{
    return humanIndex>0 && enemyIndex>0 && humanIndex!=enemyIndex &&
        humanActive && humanType && enemyActive && enemyComputer;
}
const char* name(State state)
{
    static const char* names[]={"disabled","setup","warmup","capturing","finalizing","complete","failed"};
    return names[static_cast<unsigned>(state)];
}
bool writeBenchmarkResult(std::ostream& out,const PerformanceProfile::Recorder& r,const PerformanceProfile::Metadata& meta,const char* stem,const BenchmarkResult& result)
{
    out<<"{\n\"schema\":1,\"scenario\":\"pathfinding-heavy\",\"version\":2,\n"
       <<"\"status\":"<<std::quoted(name(result.state))<<",\"reason\":"<<std::quoted(result.reason)<<",\n"
       <<"\"source\":"<<std::quoted(meta.git)<<",\"dirty\":"<<meta.dirty<<",\"build\":"<<std::quoted(meta.build)<<",\n"
       <<"\"title\":"<<std::quoted(meta.title)<<",\"report_stem\":"<<std::quoted(stem)<<",\n"
       <<"\"map\":"<<std::quoted(result.map)<<",\"map_crc\":"<<result.mapCRC<<",\"seed\":"<<result.seed<<",\n"
       <<"\"configuration\":\"China human slot0/start0 + allied easy China AI slot1/start1; 48 BattleMaster + 48 Redguard; western banks v2; min40 spacing; one move per unit at capture start\",\n"
       <<"\"warmup_ticks\":"<<WarmupTicks<<",\"capture_ticks\":"<<CaptureTicks<<",\"order_period\":"<<OrderPeriod<<",\n"
       <<"\"spawned_units\":"<<result.units<<",\"objects_end\":"<<result.objects<<",\"actual_seed\":"<<result.actualSeed<<",\n"
       <<"\"start_tick\":"<<result.startTick<<",\"end_tick\":"<<result.endTick<<",\"start_crc\":"<<result.startCRC<<",\"end_crc\":"<<result.endCRC<<",\n"
       <<"\"rng_start\":"<<result.startRNG<<",\"rng_end\":"<<result.endRNG<<",\"path_fingerprint\":"<<std::quoted(std::to_string(result.fingerprint))<<",\n"
       <<"\"internal_searches\":"<<result.internal<<",\"internal_over40ms\":"<<result.severe<<",\"queue_max\":"<<result.maxQueue<<",\n"
       <<"\"records\":"<<r.paths().size()<<",\"dropped\":"<<r.droppedPaths()<<",\"errors\":"<<r.errors()+result.phaseErrors<<",\n"
       <<"\"outer_frames\":"<<r.frames().size()<<",\"completed_logic_ticks\":"<<result.completedTicks<<",\"elapsed_seconds\":"<<r.elapsed()/static_cast<double>(meta.frequency)<<",\n"
       <<"\"requested_fps\":"<<(r.frames().empty()?0:r.frames().front().requestedFps)<<",\"effective_fps\":"<<(r.frames().empty()?0:r.frames().front().effectiveFps)<<",\n"
       <<"\"interpolation\":"<<meta.interpolation<<",\"path_detail\":"<<r.deepPaths()<<",\"path_phase\":"<<r.samplePhases()<<",\n"
       <<"\"overlay\":false\n}\n";
    return static_cast<bool>(out);
}
bool writeBenchmarkFailure(std::ostream& out,const FailureResult& result)
{
    out<<"{\"schema\":1,\"status\":\"failed\",\"reason\":"<<std::quoted(result.reason)
       <<",\"failure_state\":"<<std::quoted(name(result.state))<<",\"logic_tick\":"<<result.tick
       <<",\"spawned_units\":"<<result.units<<",\"game_requested\":"<<result.gameRequested
       <<",\"profiler_running_at_failure\":"<<result.profiling
       <<",\"orders_issued\":"<<result.orders<<",\"warmup_ticks\":"<<result.warmup
       <<",\"profiler_configured\":"<<result.configured<<",\"failed_unit_index\":"<<result.failedUnit
       <<",\"failure_check\":"<<std::quoted(result.check)
       <<",\"candidates_generated\":"<<result.selection.generated<<",\"pairs_accepted\":"<<result.selection.accepted
       <<",\"validator_rejected\":"<<result.selection.validatorRejected<<",\"relocation_rejected\":"<<result.selection.relocated
       <<",\"spacing_rejected\":"<<result.selection.spacing
       <<",\"capture_start_tick\":"<<result.startTick<<",\"capture_end_tick\":"<<result.endTick<<"}\n";
    return static_cast<bool>(out);
}
bool readyForScenarioStart(bool introComplete,bool moviePlaying,bool loadingMap,bool loadingSave,bool clearing)
{
    return introComplete && !moviePlaying && !loadingMap && !loadingSave && !clearing;
}
bool writeStartupProgress(std::ostream& out,const StartupProgress& p)
{
    out<<"{\"schema\":1,\"startup_substage\":"<<std::quoted(p.stage)<<",\"state\":"<<std::quoted(name(p.state))
       <<",\"logic_tick\":"<<p.tick<<",\"map_requested\":"<<p.mapRequested<<",\"map_loaded\":"<<p.mapLoaded
       <<",\"slots_ready\":"<<p.slotsReady<<",\"players_ready\":"<<p.playersReady
       <<",\"spawned_units\":"<<p.units<<",\"orders_issued\":"<<p.orders<<",\"warmup_ticks\":"<<p.warmupTicks
       <<",\"intro_complete\":"<<p.introComplete<<",\"movie_playing\":"<<p.moviePlaying
       <<",\"logic_ready\":"<<p.logicReady<<",\"profiler_running\":"<<p.profilerRunning<<"}\n";
    return static_cast<bool>(out);
}
bool workloadCandidate(unsigned index,WorkloadPair& pair)
{
    if(index>=WorkloadCandidates)return false;
    const float x=480.0f+(index%16)*40.0f;
    const float offset=(index/16)*40.0f;
    pair={{x,1300.0f+offset},{x,2800.0f+offset}};
    return true;
}
bool selectWorkloadPair(unsigned& cursor,const WorkloadPair* selected,unsigned count,
                        WorkloadPair& result,WorkloadValidator validate,void* context,SelectionStats* stats)
{
    if(count>=ArmySize || !validate || (count && !selected))return false;
    const auto distance2=[](WorkloadPoint a,WorkloadPoint b){const float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;};
    for(;cursor<WorkloadCandidates;)
    {
        WorkloadPair raw;workloadCandidate(cursor++,raw);auto pair=raw;
        if(stats)++stats->generated;
        if(!validate(pair,context)){if(stats)++stats->validatorRejected;continue;}
        // Normal cell snapping is permitted; a large destination relocation is not.
        if(!std::isfinite(pair.start.x) || !std::isfinite(pair.start.y) ||
           !std::isfinite(pair.goal.x) || !std::isfinite(pair.goal.y) ||
           distance2(raw.start,pair.start)>100 || distance2(raw.goal,pair.goal)>100){if(stats)++stats->relocated;continue;}
        bool separated=true;
        for(unsigned i=0;i<count;++i)
            if(distance2(selected[i].start,pair.start)<1600 || distance2(selected[i].goal,pair.goal)<1600)separated=false;
        if(separated){if(stats)++stats->accepted;result=pair;return true;}
        if(stats)++stats->spacing;
    }
    return false;
}
bool writeGoalTrace(std::ostream& out,const GoalTrace& trace)
{
    out.imbue(std::locale::classic());out<<std::setprecision(9);
    out<<"{\"schema\":1,\"scenario_unit_index\":76,\"dropped\":"<<trace.dropped()<<",\"samples\":[";
    for(unsigned i=0;i<trace.size();++i)
    {
        const auto& s=trace.at(i);if(i)out<<",";
        out<<"{\"stage\":"<<std::quoted(s.stage)<<",\"tick\":"<<s.tick<<",\"orders\":"<<s.orders
           <<",\"object_id\":"<<s.object<<",\"ignored_id\":"<<s.ignored<<",\"layer\":"<<s.layer
           <<",\"surfaces\":"<<s.surfaces<<",\"start_zone\":"<<s.startZone
           <<",\"owner\":"<<s.owner<<",\"player_type\":"<<s.playerType<<",\"geometry_radius\":"<<s.radius
           <<",\"own_goal_cell\":["<<s.ownGoalX<<","<<s.ownGoalY<<"],\"current\":["<<s.current.x<<","<<s.current.y
           <<"],\"prepared_goal\":["<<s.goal.x<<","<<s.goal.y<<"],\"cells\":[";
        for(unsigned j=0;j<s.cells.size();++j){const auto& c=s.cells[j];if(j)out<<",";
            out<<"{\"x\":"<<c.x<<",\"y\":"<<c.y<<",\"type\":"<<c.type<<",\"flags\":"<<c.flags
               <<",\"zone\":"<<c.zone<<",\"goal_id\":"<<c.goalID<<",\"obstacle_id\":"<<c.obstacleID<<",\"goal_owner\":"<<c.goalOwner
               <<",\"goal_relationship\":"<<c.goalRelationship<<",\"obstacle_owner\":"<<c.obstacleOwner
               <<",\"obstacle_template\":"<<std::quoted(c.obstacleTemplate.data())<<",\"ignored_obstacle_present\":"<<c.ignoredObstaclePresent<<"}";}
        out<<"]}";
    }
    out<<"]}\n";return static_cast<bool>(out);
}
bool validScenario(const char* value) { return value && std::strcmp(value,"pathfinding-heavy")==0; }
bool allowAction(bool dev,bool scenario,bool offline,Action action)
{ return dev && !scenario && action<Action::Count && (action==Action::Overlay || offline); }
unsigned Controller::tick(unsigned frame)
{
    if(m_state==State::Setup)return Event::Setup;
    if(m_state!=State::Warmup && m_state!=State::Capturing)return Event::None;
    if(frame<m_last){fail();return Event::None;}
    if(frame==m_last)return Event::None;
    m_last=frame;
    const unsigned elapsed=relative(frame);
    unsigned events=Event::None;
    if(m_state==State::Warmup && elapsed>=WarmupTicks){m_state=State::Capturing;events|=Event::Start|Event::Orders;}
    if(m_state==State::Capturing && elapsed>=WarmupTicks+CaptureTicks){m_state=State::Finalizing;return Event::Stop;}
    return events;
}
}
