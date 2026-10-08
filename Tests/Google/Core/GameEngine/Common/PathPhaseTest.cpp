/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include <gtest/gtest.h>
#include <sstream>
#include "Common/PerformanceProfile.h"
namespace
{
using namespace PerformanceProfile;
Tick phaseTime=0,phaseQueries=0;
Tick phaseClock(){++phaseQueries;return phaseTime;}
class PathPhaseProfiler : public testing::Test
{
	void SetUp() override {activeRecorder=nullptr;activePath=nullptr;activeIteration=nullptr;activePhase=nullptr;phaseTime=phaseQueries=0;}
	void TearDown() override {EXPECT_EQ(activePhase,nullptr);EXPECT_EQ(activeIteration,nullptr);activeRecorder=nullptr;activePath=nullptr;}
};
void skip63() {for(unsigned int i=0;i<63;++i){PathIteration iteration(phaseClock);PathPhase line(PathPhaseKind::Line);PathInsertion insertion;}}
TEST_F(PathPhaseProfiler, Samples64And128AndResetsForEachRelevantSearch)
{
	Recorder r(1,8);r.start(0);r.beginFrame(0,0,120,120);activeRecorder=&r;
	for(PathKind kind:{PathKind::Internal,PathKind::Ground,PathKind::Closest})
	{
		PathScope search(kind,phaseClock);phaseQueries=0;
		skip63();EXPECT_EQ(phaseQueries,0u);EXPECT_EQ(search.sample()->phases.selected,0u);
		{PathIteration iteration(phaseClock);PathPhase line(PathPhaseKind::Line);phaseTime+=10;}
		EXPECT_EQ(search.sample()->phases.selected,1u);EXPECT_EQ(phaseQueries,2u);
		skip63();{PathIteration iteration(phaseClock);PathPhase line(PathPhaseKind::Line);phaseTime+=10;}
		EXPECT_EQ(search.sample()->phases.iterations,128u);EXPECT_EQ(search.sample()->phases.selected,2u);
		EXPECT_EQ(search.sample()->phases.metrics[0].inclusive,20u);
	}
	activeRecorder=nullptr;r.endFrame(phaseTime,1);
}
TEST_F(PathPhaseProfiler, DisabledShallowAndLegacyDeepNeverReadPhaseClock)
{
	{PathIteration iteration(phaseClock);PathPhase line(PathPhaseKind::Line);PathInsertion insertion;}
	EXPECT_EQ(phaseQueries,0u);
	for(unsigned int capacity:{0u,8u})
	{
		Recorder r(1,capacity,false);r.start(0);r.beginFrame(0,0,120,120);activeRecorder=&r;
		{PathScope search(PathKind::Internal,phaseClock);phaseQueries=0;
		 for(unsigned int i=0;i<128;++i){PathIteration iteration(phaseClock);PathPhase line(PathPhaseKind::Line);PathInsertion insertion;}
		 EXPECT_EQ(phaseQueries,0u);if(search.enabled())EXPECT_EQ(search.sample()->phases.iterations,0u);}
		activeRecorder=nullptr;r.endFrame(100,1);
	}
}
TEST_F(PathPhaseProfiler, NestedLineExcludesItsTimeAndInsertionFromNeighbor)
{
	Recorder r(1,4);r.start(0);r.beginFrame(0,0,120,120);activeRecorder=&r;
	{
		PathScope search(PathKind::Internal,phaseClock);skip63();PathIteration iteration(phaseClock);
		phaseTime=10;
		{
			PathPhase neighbor(PathPhaseKind::Neighbor);phaseTime=20;
			{PathPhase line(PathPhaseKind::Line);phaseTime=30;{PathInsertion insertion;phaseTime=50;}phaseTime=70;}
			phaseTime=80;{PathInsertion insertion;phaseTime=90;}phaseTime=100;
		}
		const auto& phases=search.sample()->phases;
		EXPECT_EQ(phases.metrics[0].inclusive,50u);EXPECT_EQ(phases.metrics[0].insertion,20u);
		EXPECT_EQ(phases.metrics[1].inclusive,40u);EXPECT_EQ(phases.metrics[1].insertion,10u);
		EXPECT_EQ(phases.metrics[0].inserts,1u);EXPECT_EQ(phases.metrics[1].inserts,1u);
		EXPECT_EQ(phases.errors,0u);
		EXPECT_EQ(phases.metrics[0].inclusive-phases.metrics[0].insertion,30u);
		EXPECT_EQ(phases.metrics[1].inclusive-phases.metrics[1].insertion,30u);
	}
	activeRecorder=nullptr;r.endFrame(110,1);
}
TEST_F(PathPhaseProfiler, NestedSearchAndDroppedRecordsSuspendAndRestorePhaseState)
{
	Recorder r(1,1);r.start(0);r.beginFrame(0,0,120,120);activeRecorder=&r;
	{
		PathScope search(PathKind::Ground,phaseClock);skip63();PathIteration iteration(phaseClock);
		PathPhase neighbor(PathPhaseKind::Neighbor);auto* previous=activePhase;auto* allocation=r.paths().data();
		{PathScope dropped(PathKind::Internal,phaseClock);EXPECT_FALSE(dropped.enabled());
		 EXPECT_EQ(activeIteration,nullptr);EXPECT_EQ(activePhase,nullptr);skip63();
		 PathIteration nested(phaseClock);PathPhase line(PathPhaseKind::Line);PathInsertion insertion;}
		EXPECT_EQ(activePhase,previous);EXPECT_EQ(activeIteration,&iteration);
		{PathInsertion insertion;phaseTime=5;}
		EXPECT_EQ(r.paths().data(),allocation);
	}
	activeRecorder=nullptr;r.endFrame(10,1);
	EXPECT_EQ(r.droppedPaths(),1u);ASSERT_EQ(r.paths().size(),1u);
	EXPECT_EQ(r.paths()[0].phases.iterations,64u);EXPECT_EQ(r.paths()[0].phases.metrics[1].inserts,1u);
	r.stop();r.start(20);r.beginFrame(20,1,120,120);activeRecorder=&r;
	{PathScope search(PathKind::Internal,phaseClock);EXPECT_EQ(search.sample()->phases.iterations,0u);
	 EXPECT_EQ(search.sample()->phases.metrics[1].inclusive,0u);}
	activeRecorder=nullptr;r.endFrame(30,2);
}
TEST_F(PathPhaseProfiler, UnsupportedKindsAndUnsampledIterationsCannotInheritSelection)
{
	Recorder r(1,4);r.start(0);r.beginFrame(0,0,120,120);activeRecorder=&r;
	{PathScope search(PathKind::Hierarchical,phaseClock);phaseQueries=0;skip63();
	 PathIteration iteration(phaseClock);PathPhase line(PathPhaseKind::Line);PathInsertion insertion;
	 EXPECT_EQ(phaseQueries,0u);EXPECT_EQ(search.sample()->phases.iterations,0u);}
	{PathScope search(PathKind::Internal,phaseClock);skip63();PathIteration selected(phaseClock);
	 {PathIteration unsampled(phaseClock);EXPECT_EQ(activeIteration,nullptr);}
	 EXPECT_EQ(activeIteration,&selected);}
	activeRecorder=nullptr;r.endFrame(100,1);
}
TEST_F(PathPhaseProfiler, RegressedClocksAndOversizedInsertionAreBoundedAndReported)
{
	Recorder r(1,4);r.start(0);r.beginFrame(0,0,120,120);activeRecorder=&r;
	{PathScope search(PathKind::Internal,phaseClock);skip63();PathIteration iteration(phaseClock);
	 phaseTime=100;{PathPhase line(PathPhaseKind::Line);phaseTime=90;}
	 phaseTime=100;{PathPhase neighbor(PathPhaseKind::Neighbor);{PathInsertion insertion;phaseTime=200;}phaseTime=110;}
	 EXPECT_EQ(search.sample()->phases.errors,2u);
	 EXPECT_EQ(search.sample()->phases.metrics[0].inclusive,0u);
	 EXPECT_EQ(search.sample()->phases.metrics[1].inclusive,10u);EXPECT_EQ(search.sample()->phases.metrics[1].insertion,10u);}
	activeRecorder=nullptr;r.endFrame(120,1);
	Metadata meta;meta.frequency=1000;std::ostringstream summary,csv;reportPaths(r,meta,summary,csv);
	EXPECT_NE(summary.str().find("path_phase_errors=2"),std::string::npos);
	EXPECT_NE(summary.str().find("path_phase_stride=64"),std::string::npos);
	EXPECT_NE(csv.str().find("line_sample_inclusive_ms,line_sample_insertion_ms"),std::string::npos);
}
TEST_F(PathPhaseProfiler, EarlyReturnAndExceptionUnwindLeaveNoActivePhase)
{
	Recorder r(1,4);r.start(0);r.beginFrame(0,0,120,120);activeRecorder=&r;
	auto leave=[](){PathScope search(PathKind::Closest,phaseClock);skip63();PathIteration iteration(phaseClock);PathPhase neighbor(PathPhaseKind::Neighbor);phaseTime+=3;};
	leave();EXPECT_EQ(activePhase,nullptr);EXPECT_EQ(activeIteration,nullptr);
	try{PathScope search(PathKind::Ground,phaseClock);skip63();PathIteration iteration(phaseClock);PathPhase line(PathPhaseKind::Line);phaseTime+=7;throw 1;}catch(int){}
	EXPECT_EQ(activePhase,nullptr);EXPECT_EQ(activeIteration,nullptr);EXPECT_EQ(activePath,nullptr);
	EXPECT_EQ(r.paths()[0].phases.metrics[1].inclusive,3u);EXPECT_EQ(r.paths()[1].phases.metrics[0].inclusive,7u);
	activeRecorder=nullptr;r.endFrame(20,1);
}
}
