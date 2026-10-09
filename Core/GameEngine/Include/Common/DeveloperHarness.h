/* Copyright 2026 TheSuperHackers. SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstdint>
#include <array>
#include <iosfwd>
namespace PerformanceProfile { class Recorder;struct Metadata; }
namespace DeveloperTools
{
constexpr unsigned WarmupTicks=90, CaptureTicks=300, OrderPeriod=0, ArmySize=96, MaxDevUnits=192;
constexpr unsigned ScenarioVersion=2, WorkloadCandidates=192;
struct WorkloadPoint { float x=0,y=0; };
struct WorkloadPair { WorkloadPoint start,goal; };
// Candidate order and spacing are fixed, not selected using timing or RNG.
bool workloadCandidate(unsigned index,WorkloadPair& pair);
struct SelectionStats { unsigned generated=0,accepted=0,validatorRejected=0,relocated=0,spacing=0; };
using WorkloadValidator=bool (*)(WorkloadPair&,void*);
bool selectWorkloadPair(unsigned& cursor,const WorkloadPair* selected,unsigned count,
                        WorkloadPair& result,WorkloadValidator validate,void* context,SelectionStats* stats=nullptr);
// Forensic-only snapshot of scenario index76, before capture. No path searches.
constexpr unsigned GoalTraceCapacity=100,GoalTraceCells=25;
struct GoalCellSnapshot
{
    int x=0,y=0,type=-1,flags=-1,zone=-1,goalOwner=-1,goalRelationship=-1,obstacleOwner=-1;
    std::array<char,64> obstacleTemplate{};
    unsigned goalID=0,obstacleID=0;
    bool ignoredObstaclePresent=false;
};
struct GoalSnapshot
{
    const char* stage="unset";
    unsigned tick=0,orders=0,object=0,ignored=0,surfaces=0;
    int layer=-1,startZone=-1,owner=-1,playerType=-1,ownGoalX=-1,ownGoalY=-1;
    float radius=0;
    WorkloadPoint current,goal;
    std::array<GoalCellSnapshot,GoalTraceCells> cells{};
};
class GoalTrace
{
public:
    bool append(const GoalSnapshot& sample) { if(m_count==GoalTraceCapacity){++m_dropped;return false;}m_samples[m_count++]=sample;return true; }
    unsigned size() const { return m_count; }
    unsigned dropped() const { return m_dropped; }
    const GoalSnapshot& at(unsigned index) const { return m_samples[index]; }
private:
    std::array<GoalSnapshot,GoalTraceCapacity> m_samples{};
    unsigned m_count=0,m_dropped=0;
};
bool writeGoalTrace(std::ostream&,const GoalTrace&);
enum class State { Disabled, Setup, Warmup, Capturing, Finalizing, Complete, Failed };
enum Event : unsigned { None=0, Setup=1, Orders=2, Start=4, Stop=8 };
enum class Action { Overlay, Money, InstantBuild, Reveal, Group, Army, Capture, LargeGroup, OpposingArmy, Move, Battle, Count };
const char* name(State state);
struct BenchmarkResult
{
    State state=State::Failed;
    const char* reason="",*map="";
    unsigned mapCRC=0,seed=0,actualSeed=0,units=0,objects=0,startTick=0,endTick=0,startCRC=0,endCRC=0,startRNG=0,endRNG=0,internal=0,severe=0;
    std::uint64_t fingerprint=0,maxQueue=0,phaseErrors=0,completedTicks=0;
};
bool writeBenchmarkResult(std::ostream&,const PerformanceProfile::Recorder&,const PerformanceProfile::Metadata&,const char* stem,const BenchmarkResult&);
struct FailureResult
{
    const char* reason="";
    State state=State::Setup;
    unsigned tick=0,units=0,startTick=0,endTick=0,orders=0,warmup=0;
    SelectionStats selection;
    unsigned failedUnit=ArmySize;
    const char* check="unavailable";
    bool configured=false;
    bool gameRequested=false,profiling=false;
};
bool writeBenchmarkFailure(std::ostream&,const FailureResult&);
// Scenario-only allocation follows the ordinary menu/save-load ownership rules.
bool ensureScenarioGameInfo();
bool readyForScenarioStart(bool introComplete,bool moviePlaying,bool loadingMap,bool loadingSave,bool clearing);
struct StartupProgress
{
    const char* stage="uninitialized";
    State state=State::Setup;
    unsigned tick=0,units=0,orders=0,warmupTicks=0;
    bool mapRequested=false,mapLoaded=false,slotsReady=false,playersReady=false;
    bool introComplete=false,moviePlaying=false,logicReady=false,profilerRunning=false;
};
bool writeStartupProgress(std::ostream&,const StartupProgress&);
bool validScenario(const char* name);
bool allowAction(bool dev, bool scenario, bool offline, Action action);
inline bool canSpawn(unsigned existing,unsigned count) { return existing<=MaxDevUnits && count<=MaxDevUnits-existing; }
inline bool timedOut(std::uint64_t now,std::uint64_t deadline) { return now>deadline; }
class Controller
{
public:
    explicit Controller(bool enabled=false) : m_state(enabled?State::Setup:State::Disabled) {}
    State state() const { return m_state; }
    unsigned tick(unsigned frame);
    void prepared(unsigned frame) { if(m_state==State::Setup){m_origin=m_last=frame;m_state=State::Warmup;} }
    void finish(bool valid) { if(m_state==State::Finalizing)m_state=valid?State::Complete:State::Failed; }
    void fail() { if(m_state!=State::Disabled)m_state=State::Failed; }
    unsigned relative(unsigned frame) const { return frame>=m_origin?frame-m_origin:0; }
private:
    State m_state;
    unsigned m_origin=0,m_last=0;
};
// Engine adapter, gated before any allocation, clocks or game-state access.
bool configureScenario(const char* name);
bool scenarioEnabled();
void enableDevMode();
bool devModeEnabled();
void requestQuickGame();
bool quickGameEnabled();
bool configureDevPreset(const char* value);
// Explicit slot ownership predicate, also used by the interactive runtime.
bool validDevOwners(int humanIndex,bool humanActive,bool humanType,
                    int enemyIndex,bool enemyActive,bool enemyComputer);
bool interactiveOffline();
void beginInteractiveOuter();
void interactiveTick(unsigned frame,unsigned actions);
const char* interactiveStatus();
void shutdownInteractive();
void beginOuter();
void logicTick(unsigned frame);
bool consumeKey(unsigned key,unsigned state);
void drawOverlay();
void shutdown();
}
