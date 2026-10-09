/* Copyright 2026 TheSuperHackers. SPDX-License-Identifier: GPL-3.0-or-later */
#include <gtest/gtest.h>
#include "Common/DeveloperHarness.h"
#include "Common/PerformanceProfile.h"
#include <filesystem>
#include <chrono>
#include <sstream>
#include <array>
#include "GameClient/KeyDefs.h"
#include "Common/GlobalData.h"
#include "Common/CommandLine.h"
#include "GameNetwork/GameInfo.h"
using namespace DeveloperTools;
TEST(DeveloperHarness, DefaultOffAndExactScenarioGate)
{
    EXPECT_FALSE(devModeEnabled());EXPECT_FALSE(scenarioEnabled());
    Controller c;EXPECT_EQ(c.state(),State::Disabled);EXPECT_EQ(c.tick(100),0u);
    EXPECT_FALSE(validScenario(nullptr));EXPECT_FALSE(validScenario(""));EXPECT_FALSE(validScenario("pathfinding-heavy-extra"));EXPECT_TRUE(validScenario("pathfinding-heavy"));
}
namespace { unsigned observedReports=0;bool observedWritten=false;
void observeReport(const PerformanceProfile::Recorder& r,const PerformanceProfile::Metadata& meta,const char*,bool written)
{++observedReports;observedWritten=written;EXPECT_EQ(r.frames().size(),1u);EXPECT_EQ(meta.mode,"test_stop");} }
TEST(DeveloperHarness, RuntimeCaptureControlsRejectActiveFrameAndWriteReports)
{
    using namespace PerformanceProfile;
    const auto directory=std::filesystem::temp_directory_path()/("generals-harness-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    PerformanceProfile::shutdown();EXPECT_FALSE(configure(nullptr));EXPECT_FALSE(configure("relative"));
    ASSERT_TRUE(configure(directory.string().c_str()));enablePathDetails();setAutomaticControl(true);setReportObserver(observeReport);
    ASSERT_TRUE(startCapture("test"));EXPECT_FALSE(startCapture("duplicate"));
    beginOuterFrame(0,120,120);ASSERT_NE(activeRecorder,nullptr);EXPECT_FALSE(stopCapture("inside_frame"));
    count(Counter::CompletedTicks);endOuterFrame(1);EXPECT_TRUE(stopCapture("test_stop"));
    EXPECT_EQ(observedReports,1u);EXPECT_TRUE(observedWritten);EXPECT_FALSE(runtimeStatus().running);EXPECT_EQ(runtimeStatus().frames,1u);
    PerformanceProfile::shutdown();EXPECT_FALSE(configured());
    unsigned files=0;for(const auto& entry:std::filesystem::directory_iterator(directory)){ASSERT_TRUE(entry.is_regular_file());std::filesystem::remove(entry.path());++files;}
    EXPECT_EQ(files,5u);std::filesystem::remove(directory);
}
TEST(DeveloperHarness, TickBoundariesAndNoRepeatedFrameActions)
{
    Controller c(true);EXPECT_EQ(c.tick(7),Event::Setup);c.prepared(7);
    EXPECT_EQ(c.tick(7),0u);EXPECT_EQ(c.tick(96),0u);
    EXPECT_EQ(c.tick(97),Event::Start|Event::Orders);EXPECT_EQ(c.state(),State::Capturing);
    EXPECT_EQ(c.tick(97),0u);EXPECT_EQ(c.tick(127),Event::None);
    EXPECT_EQ(c.tick(396),0u);EXPECT_EQ(c.tick(397),Event::Stop);EXPECT_EQ(c.state(),State::Finalizing);
    EXPECT_EQ(c.tick(398),0u);c.finish(true);EXPECT_EQ(c.state(),State::Complete);
}
TEST(DeveloperHarness, FailureResetAndFinalizeRejection)
{
    Controller c(true);c.prepared(20);c.tick(30);c.tick(29);EXPECT_EQ(c.state(),State::Failed);EXPECT_EQ(c.tick(999),0u);
    c=Controller(true);c.prepared(0);c.tick(WarmupTicks);c.tick(WarmupTicks+CaptureTicks);c.finish(false);EXPECT_EQ(c.state(),State::Failed);
    c=Controller(true);c.fail();EXPECT_EQ(c.tick(0),0u);
}
TEST(DeveloperHarness, TimeoutAndSpawnBoundsDoNotWrap)
{
    EXPECT_FALSE(timedOut(100,100));EXPECT_TRUE(timedOut(101,100));
    EXPECT_TRUE(canSpawn(0,MaxDevUnits));EXPECT_FALSE(canSpawn(MaxDevUnits,1));EXPECT_FALSE(canSpawn(~0u,1));EXPECT_FALSE(canSpawn(0,~0u));
}
TEST(DeveloperHarness, MetadataWriterIncludesIdentityBoundsAndEscapes)
{
    PerformanceProfile::Recorder recorder(1,8);recorder.start(0);recorder.beginFrame(0,90,120,120);recorder.endFrame(1,91);
    PerformanceProfile::Metadata meta;meta.git="fixture";meta.title="Zero Hour";
    BenchmarkResult result;result.state=State::Complete;result.map="Maps\\test.map";result.units=96;result.startTick=90;result.endTick=390;
    std::ostringstream out;ASSERT_TRUE(writeBenchmarkResult(out,recorder,meta,"fixture",result));
    const auto value=out.str();EXPECT_NE(value.find("\"status\":\"complete\""),std::string::npos);
    EXPECT_NE(value.find("Maps\\\\test.map"),std::string::npos);EXPECT_NE(value.find("\"capture_ticks\":300"),std::string::npos);
    EXPECT_NE(value.find("\"requested_fps\":120"),std::string::npos);EXPECT_NE(value.find("\"path_fingerprint\":\"0\""),std::string::npos);
    out.setstate(std::ios::badbit);EXPECT_FALSE(writeBenchmarkResult(out,recorder,meta,"fixture",result));
}
TEST(DeveloperHarness, ControlSequenceIndependentOfRenderCalls)
{
    Controller a(true),b(true);a.prepared(1);b.prepared(1);
    for(unsigned tick=2;tick<500;++tick){auto event=a.tick(tick);EXPECT_EQ(b.tick(tick),event);for(int render=0;render<9;++render)EXPECT_EQ(b.tick(tick),0u);}
    EXPECT_EQ(a.state(),b.state());
}
TEST(DeveloperHarness, RejectsAllActionsWithoutGateAndDuringScenario)
{
    for(unsigned i=0;i<static_cast<unsigned>(Action::Count);++i)
    {
        auto action=static_cast<Action>(i);EXPECT_FALSE(allowAction(false,false,true,action));EXPECT_FALSE(allowAction(true,true,true,action));
        EXPECT_EQ(allowAction(true,false,false,action),action==Action::Overlay);EXPECT_TRUE(allowAction(true,false,true,action));
    }
    EXPECT_FALSE(allowAction(true,false,true,Action::Count));
}
TEST(DeveloperHarness, RuntimeGatesAndChordReleaseAreExplicit)
{
    EXPECT_FALSE(consumeKey(KEY_F5,KEY_STATE_UP));
    EXPECT_FALSE(configureScenario("invalid"));EXPECT_TRUE(scenarioEnabled());DeveloperTools::shutdown();
    EXPECT_TRUE(configureScenario("pathfinding-heavy"));EXPECT_TRUE(scenarioEnabled());DeveloperTools::shutdown();
    enableDevMode();EXPECT_TRUE(devModeEnabled());EXPECT_FALSE(consumeKey(KEY_F5,KEY_STATE_DOWN));
    EXPECT_TRUE(consumeKey(KEY_F5,KEY_STATE_DOWN|KEY_STATE_CONTROL|KEY_STATE_ALT|KEY_STATE_SHIFT));
    EXPECT_TRUE(consumeKey(KEY_F5,KEY_STATE_UP)); // Release after modifiers is still consumed.
    EXPECT_FALSE(consumeKey(KEY_F5,KEY_STATE_UP));DeveloperTools::shutdown();EXPECT_FALSE(devModeEnabled());
}
TEST(DeveloperHarness, ProfilerControlProducesBoundedCompleteTickWindow)
{
    using namespace PerformanceProfile;
    Controller c(true);c.prepared(0);Recorder recorder(CaptureTicks+1,8);
    unsigned captured=0;
    for(unsigned tick=1;tick<=WarmupTicks+CaptureTicks;++tick)
    {
        if(recorder.running()){ASSERT_TRUE(recorder.beginFrame(tick,tick-1,120,120));recorder.add(Counter::CompletedTicks);recorder.endFrame(tick+1,tick);++captured;}
        auto event=c.tick(tick);if(event&Event::Start)recorder.start(tick);if(event&Event::Stop)recorder.stop();
    }
    EXPECT_EQ(captured,CaptureTicks);EXPECT_EQ(recorder.frames().size(),CaptureTicks);EXPECT_FALSE(recorder.running());EXPECT_EQ(recorder.errors(),0u);
    EXPECT_LE(ArmySize,MaxDevUnits);
}

TEST(DeveloperHarness, AutomaticScenarioAllocatesMenuOwnedSkirmishInfo)
{
    ASSERT_EQ(TheSkirmishGameInfo,nullptr);ASSERT_NE(TheWritableGlobalData,nullptr);
    struct Cleanup
    {
        ~Cleanup(){delete TheSkirmishGameInfo;TheSkirmishGameInfo=nullptr;DeveloperTools::shutdown();}
    } cleanup;
    EXPECT_FALSE(ensureScenarioGameInfo());EXPECT_EQ(TheSkirmishGameInfo,nullptr);
    EXPECT_FALSE(configureScenario("invalid"));EXPECT_FALSE(ensureScenarioGameInfo());
    ASSERT_TRUE(configureScenario("pathfinding-heavy"));ASSERT_TRUE(ensureScenarioGameInfo());
    auto* info=TheSkirmishGameInfo;ASSERT_NE(info,nullptr);
    info->init();info->setLocalIP(0);info->enterGame();
    EXPECT_TRUE(info->isInGame());EXPECT_EQ(info->getNumPlayers(),0);
    GameSlot slot;slot.setState(SLOT_PLAYER,UnicodeString(L"Regression"));info->setSlot(1,slot);
    ASSERT_TRUE(ensureScenarioGameInfo());EXPECT_EQ(TheSkirmishGameInfo,info);
    EXPECT_TRUE(info->getSlot(1)->isHuman()); // Reuse must not erase an existing slot setup.
}
TEST(DeveloperHarness, FailureMarkerRecordsStateTickAndProgress)
{
    FailureResult result;result.reason="missing \"map\"";result.tick=7;
    std::ostringstream out;ASSERT_TRUE(writeBenchmarkFailure(out,result));
    EXPECT_NE(out.str().find("\"failure_state\":\"setup\""),std::string::npos);
    EXPECT_NE(out.str().find("\"logic_tick\":7"),std::string::npos);
    EXPECT_NE(out.str().find("\"spawned_units\":0"),std::string::npos);
    EXPECT_NE(out.str().find("\"game_requested\":0"),std::string::npos);
    EXPECT_NE(out.str().find("missing \\\"map\\\""),std::string::npos);
    out.setstate(std::ios::badbit);EXPECT_FALSE(writeBenchmarkFailure(out,result));
}

TEST(DeveloperHarness, StartupWaitsForWholeIntroAndAllLoadingTransitions)
{
    // Regression: a prepared skirmish with a playing Sizzle movie cannot request MSG_NEW_GAME.
    EXPECT_FALSE(readyForScenarioStart(false,true,false,false,false));
    EXPECT_FALSE(readyForScenarioStart(false,false,false,false,false)); // Between intro stages.
    EXPECT_FALSE(readyForScenarioStart(true,true,false,false,false));
    EXPECT_FALSE(readyForScenarioStart(true,false,true,false,false));
    EXPECT_FALSE(readyForScenarioStart(true,false,false,true,false));
    EXPECT_FALSE(readyForScenarioStart(true,false,false,false,true));
    for(unsigned render=0;render<1000;++render)EXPECT_FALSE(readyForScenarioStart(false,false,false,false,false));
    EXPECT_TRUE(readyForScenarioStart(true,false,false,false,false));
}
TEST(DeveloperHarness, StartupCheckpointRecordsBoundedProgressAndEscapes)
{
    StartupProgress p;p.stage="waiting \"intro\"";p.moviePlaying=true;
    std::ostringstream out;ASSERT_TRUE(writeStartupProgress(out,p));
    EXPECT_NE(out.str().find("\"map_requested\":0"),std::string::npos);
    EXPECT_NE(out.str().find("\"movie_playing\":1"),std::string::npos);
    EXPECT_NE(out.str().find("waiting \\\"intro\\\""),std::string::npos);
    EXPECT_LT(out.str().size(),1024u);
    out.setstate(std::ios::badbit);EXPECT_FALSE(writeStartupProgress(out,p));
}

namespace {
bool connectedBanks(WorkloadPair& pair,void*)
{
    // Deterministic test terrain: one blocked candidate and two disconnected starts.
    return pair.start.x!=480 && pair.start.x!=520 && pair.start.x!=560;
}
bool allBlocked(WorkloadPair&,void*){return false;}
bool relocateGoal(WorkloadPair& pair,void*){pair.goal.x+=100;return true;}
}
TEST(DeveloperHarness, ReachableGoalSelectionIsBoundedSeparatedAndDeterministic)
{
    std::array<WorkloadPair,ArmySize> first{},second{};unsigned a=0,b=0;
    for(unsigned i=0;i<ArmySize;++i)
    {
        ASSERT_TRUE(selectWorkloadPair(a,first.data(),i,first[i],connectedBanks,nullptr));
        ASSERT_TRUE(selectWorkloadPair(b,second.data(),i,second[i],connectedBanks,nullptr));
        EXPECT_EQ(first[i].start.x,second[i].start.x);EXPECT_EQ(first[i].start.y,second[i].start.y);
        EXPECT_EQ(first[i].goal.x,second[i].goal.x);EXPECT_EQ(first[i].goal.y,second[i].goal.y);
        EXPECT_TRUE(connectedBanks(first[i],nullptr));EXPECT_FLOAT_EQ(first[i].goal.y-first[i].start.y,1500);
        for(unsigned j=0;j<i;++j)
        {
            const float dx=first[i].goal.x-first[j].goal.x,dy=first[i].goal.y-first[j].goal.y;
            EXPECT_GE(dx*dx+dy*dy,1600);
        }
    }
    unsigned cursor=0;WorkloadPair pair;
    EXPECT_FALSE(selectWorkloadPair(cursor,nullptr,0,pair,allBlocked,nullptr));EXPECT_EQ(cursor,WorkloadCandidates);
    cursor=0;EXPECT_FALSE(selectWorkloadPair(cursor,nullptr,0,pair,relocateGoal,nullptr));
    EXPECT_FALSE(workloadCandidate(WorkloadCandidates,pair));
}
TEST(DeveloperHarness, WorkloadOrdersOnlyOnceAtCaptureStart)
{
    Controller controller(true);controller.prepared(1);unsigned orders=0;
    for(unsigned tick=2;tick<=WarmupTicks+CaptureTicks+1;++tick)
    {
        const auto event=controller.tick(tick);
        if(event&Event::Orders){++orders;EXPECT_EQ(tick,1+WarmupTicks);EXPECT_TRUE(event&Event::Start);}
    }
    EXPECT_EQ(orders,1u);EXPECT_EQ(OrderPeriod,0u);
}

TEST(DeveloperHarness, FailureAfter76OrdersRetainsFirstRejectedCheck)
{
    Controller controller(true);controller.prepared(1);
    EXPECT_EQ(controller.tick(91),Event::Orders|Event::Start);
    FailureResult result;result.state=controller.state();result.tick=91;result.units=96;
    result.orders=76;result.warmup=90;result.failedUnit=76;result.check="goal_relocated_after_warmup";
    result.reason="workload_goal_changed_or_unreachable";result.configured=true;
    result.selection={120,96,10,4,10};controller.fail();
    std::ostringstream out;ASSERT_TRUE(writeBenchmarkFailure(out,result));
    EXPECT_NE(out.str().find("\"orders_issued\":76"),std::string::npos);
    EXPECT_NE(out.str().find("\"failure_state\":\"capturing\""),std::string::npos);
    EXPECT_NE(out.str().find("\"failure_check\":\"goal_relocated_after_warmup\""),std::string::npos);
    EXPECT_NE(out.str().find("\"pairs_accepted\":96"),std::string::npos);
    EXPECT_NE(out.str().find("\"profiler_running_at_failure\":0"),std::string::npos);
    // The check name is a synthetic fixture; Fix4 did not record which branch failed.
}
TEST(DeveloperHarness, SelectionAccountingIsBoundedAndConservesCandidates)
{
    SelectionStats stats;unsigned cursor=0;WorkloadPair result;
    EXPECT_FALSE(selectWorkloadPair(cursor,nullptr,0,result,allBlocked,nullptr,&stats));
    EXPECT_EQ(stats.generated,WorkloadCandidates);EXPECT_EQ(stats.validatorRejected,WorkloadCandidates);
    EXPECT_EQ(stats.accepted+stats.validatorRejected+stats.relocated+stats.spacing,stats.generated);
    stats={};cursor=0;
    EXPECT_FALSE(selectWorkloadPair(cursor,nullptr,0,result,relocateGoal,nullptr,&stats));
    EXPECT_EQ(stats.relocated,WorkloadCandidates);EXPECT_EQ(stats.accepted,0u);
}
TEST(DeveloperHarness, CommandLineStartupIntroFlags)
{
    // Run this test alone with -skipIntro, -devMode -skipIntro, or a scenario.
    bool skip=false,scenarioArg=false,quick=false,devArg=false;
    for(int i=1;i<__argc;++i){if(_stricmp(__argv[i],"-skipIntro")==0)skip=true;
        if(_stricmp(__argv[i],"-performanceScenario")==0)scenarioArg=true;
        if(_stricmp(__argv[i],"-quickGame")==0)quick=true;
        if(_stricmp(__argv[i],"-devMode")==0)devArg=true;}
    if(skip || scenarioArg || (quick && devArg))
    {
        EXPECT_FALSE(TheGlobalData->m_playIntro);EXPECT_FALSE(TheGlobalData->m_playSizzle);
        // Simulate a later GameData INI overwriting startup movie flags.
        TheWritableGlobalData->m_playIntro=TRUE;TheWritableGlobalData->m_playSizzle=TRUE;
        CommandLine::parseCommandLineForEngineInit();
        EXPECT_FALSE(TheGlobalData->m_playIntro);EXPECT_FALSE(TheGlobalData->m_playSizzle);
    }
    else {EXPECT_FALSE(scenarioEnabled());EXPECT_TRUE(TheGlobalData->m_playIntro);EXPECT_TRUE(TheGlobalData->m_playSizzle);}
    EXPECT_EQ(scenarioEnabled(),scenarioArg);
    EXPECT_EQ(quickGameEnabled(),quick && devArg && !scenarioArg);
    if(quickGameEnabled())EXPECT_FALSE(TheGlobalData->m_shellMapOn);
}

TEST(DeveloperHarness, GoalTraceBoundsAndOrderBoundaryMetadata)
{
    GoalTrace trace;GoalSnapshot sample;sample.stage="before_orders";sample.tick=91;sample.object=293;
    sample.goal={600.5f,3080.5f};sample.radius=7;
    sample.cells[12].goalID=292;sample.cells[12].flags=1;
    ASSERT_TRUE(trace.append(sample));sample.stage="after_order";sample.orders=76;
    for(unsigned i=1;i<GoalTraceCapacity;++i)ASSERT_TRUE(trace.append(sample));
    EXPECT_FALSE(trace.append(sample));EXPECT_EQ(trace.dropped(),1u);EXPECT_EQ(trace.size(),GoalTraceCapacity);
    EXPECT_EQ(trace.at(0).orders,0u);EXPECT_EQ(trace.at(1).orders,76u);
    std::ostringstream out;ASSERT_TRUE(writeGoalTrace(out,trace));
    EXPECT_NE(out.str().find("\"goal_id\":292"),std::string::npos);
    EXPECT_NE(out.str().find("600.5,3080.5"),std::string::npos);
    EXPECT_NE(out.str().find("\"dropped\":1"),std::string::npos);
    EXPECT_LT(out.str().size(),1024u*1024u);
    out.setstate(std::ios::badbit);EXPECT_FALSE(writeGoalTrace(out,trace));
}
TEST(DeveloperHarness, QuickGameRequiresDevAndRejectsScenarioAndNeutralOwners)
{
    DeveloperTools::shutdown();requestQuickGame();EXPECT_FALSE(quickGameEnabled());
    enableDevMode();EXPECT_TRUE(quickGameEnabled());configureScenario("pathfinding-heavy");EXPECT_FALSE(quickGameEnabled());
    DeveloperTools::shutdown();EXPECT_FALSE(quickGameEnabled());EXPECT_FALSE(interactiveOffline());
    EXPECT_TRUE(validDevOwners(1,true,true,2,true,true));
    EXPECT_FALSE(validDevOwners(1,true,true,0,true,true));
    EXPECT_FALSE(validDevOwners(0,true,true,2,true,true));
    EXPECT_FALSE(validDevOwners(1,true,true,1,true,true));
    EXPECT_FALSE(validDevOwners(1,false,true,2,true,true));
    EXPECT_FALSE(validDevOwners(1,true,false,2,true,true));
    EXPECT_FALSE(validDevOwners(1,true,true,2,false,true));
    EXPECT_FALSE(validDevOwners(1,true,true,2,true,false));
    EXPECT_FALSE(configureDevPreset(nullptr));EXPECT_FALSE(configureDevPreset("pathfinding"));
    EXPECT_TRUE(configureDevPreset("battle"));DeveloperTools::shutdown();
}
