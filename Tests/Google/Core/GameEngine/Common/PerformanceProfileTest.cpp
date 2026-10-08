/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include <gtest/gtest.h>
#include <sstream>
#include <numeric>
#include "Common/PerformanceProfile.h"
namespace
{
using namespace PerformanceProfile;
Tick fakeTime=0, queries=0;
Tick fakeClock(){++queries;return fakeTime;}
unsigned int index(Category c){return static_cast<unsigned int>(c);}
class PerformanceProfiler : public testing::Test
{
	void SetUp() override {activeRecorder=nullptr;queries=fakeTime=0;}
	void TearDown() override {activeRecorder=nullptr;}
};
TEST_F(PerformanceProfiler, DisabledDoesNotQueryClockOrCollect)
{
	Recorder r(2);
	{ Scope scope(Category::Logic,fakeClock);count(Counter::CompletedTicks); }
	EXPECT_EQ(queries,0u);EXPECT_TRUE(r.frames().empty());
	EXPECT_FALSE(r.beginFrame(0,0,120,120));
}
TEST_F(PerformanceProfiler, NestedInclusiveExclusiveAndRAII)
{
	Recorder r(2);r.start(0);ASSERT_TRUE(r.beginFrame(0,0,120,120));activeRecorder=&r;
	fakeTime=10;
	{ Scope logic(Category::Logic,fakeClock);
		fakeTime=20; {Scope ai(Category::AIObject,fakeClock);fakeTime=40;}
		fakeTime=70;count(Counter::CompletedTicks);
	}
	activeRecorder=nullptr;r.endFrame(100,1);
	const auto& f=r.frames()[0];
	EXPECT_EQ(f.metrics[index(Category::Outer)].inclusive,100u);
	EXPECT_EQ(f.metrics[index(Category::Outer)].exclusive,40u);
	EXPECT_EQ(f.metrics[index(Category::Logic)].inclusive,60u);
	EXPECT_EQ(f.metrics[index(Category::Logic)].exclusive,40u);
	EXPECT_EQ(f.metrics[index(Category::AIObject)].exclusive,20u);
	EXPECT_EQ(queries,4u);EXPECT_EQ(r.errors(),0u);
}
TEST_F(PerformanceProfiler, SameCategoryRecursionDoesNotDoubleCountInclusive)
{
	Recorder r(1);r.start(0);r.beginFrame(0,0,120,120);
	r.push(Category::PathSearch,10);r.push(Category::PathSearch,20);r.pop(40);r.pop(70);r.endFrame(100,0);
	const auto m=r.frames()[0].metrics[index(Category::PathSearch)];
	EXPECT_EQ(m.calls,2u);EXPECT_EQ(m.inclusive,60u);EXPECT_EQ(m.exclusive,60u);
}
TEST_F(PerformanceProfiler, BoundedCaptureDoesNotGrowAndZeroCapacitySafe)
{
	Recorder r(2);r.start(0);r.beginFrame(0,0,120,120);r.endFrame(10,0);
	const auto* allocation=r.frames().data();
	r.beginFrame(10,0,120,120);r.endFrame(20,0);
	EXPECT_TRUE(r.full());EXPECT_FALSE(r.beginFrame(20,0,120,120));
	EXPECT_EQ(r.frames().size(),2u);EXPECT_EQ(r.frames().data(),allocation);
	Recorder empty(0);empty.start(0);EXPECT_FALSE(empty.beginFrame(0,0,120,120));
}
TEST_F(PerformanceProfiler, StartStopResetDoNotInvalidateLiveScope)
{
	Recorder r(2);r.start(10);r.beginFrame(10,0,60,60);
	r.stop();r.start(100);EXPECT_TRUE(r.running());EXPECT_EQ(r.started(),10u);
	r.endFrame(20,1);r.stop();EXPECT_FALSE(r.running());
	EXPECT_FALSE(r.beginFrame(30,1,60,60));r.start(40);
	EXPECT_TRUE(r.frames().empty());EXPECT_EQ(r.elapsed(),0u);EXPECT_EQ(r.started(),40u);
}
TEST_F(PerformanceProfiler, NearestRankPercentilesAndEmptyStatistics)
{
	std::vector<Tick> values(100);std::iota(values.begin(),values.end(),1);
	const auto s=statistics(values);
	EXPECT_EQ(s.count,100u);EXPECT_EQ(s.total,5050u);EXPECT_DOUBLE_EQ(s.mean,50.5);
	EXPECT_EQ(s.minimum,1u);EXPECT_EQ(s.maximum,100u);EXPECT_EQ(s.p50,50u);EXPECT_EQ(s.p95,95u);EXPECT_EQ(s.p99,99u);
	EXPECT_EQ(statistics({}).count,0u);EXPECT_DOUBLE_EQ(statistics({}).mean,0);
	EXPECT_EQ(statistics({9}).p99,9u);
}
TEST_F(PerformanceProfiler, TopNSlowestAndCompletedLogicFilterStable)
{
	Recorder r(5);r.start(0);
	for(unsigned int i=0;i<4;++i)
	{
		r.beginFrame(i*100,0,120,120);r.push(Category::Logic,i*100+1);
		if(i!=2)r.add(Counter::CompletedTicks);
		r.pop(i*100+10*(i+1));r.endFrame(i*100+50*(i+1),i);
	}
	EXPECT_EQ(r.slowest(Category::Outer,2),(std::vector<std::size_t>{3,2}));
	EXPECT_EQ(r.slowest(Category::Logic,2,true),(std::vector<std::size_t>{3,1}));
	EXPECT_TRUE(r.slowest(Category::Outer,0).empty());
}
TEST_F(PerformanceProfiler, CountersAndNoLogicFramesRemainDistinct)
{
	Recorder r(2);r.start(0);r.beginFrame(0,7,120,120);r.add(Counter::UpdateModules,4);r.add(Counter::UpdateModules,5);r.endFrame(10,7);
	EXPECT_EQ(r.frames()[0].counters[static_cast<unsigned int>(Counter::UpdateModules)],9u);
	EXPECT_EQ(r.frames()[0].counters[0],0u);
	r.beginFrame(10,7,120,60);r.add(Counter::CompletedTicks);r.endFrame(20,8);
	EXPECT_EQ(r.frames()[1].counters[0],1u);EXPECT_EQ(r.frames()[1].effectiveFps,60);
}
TEST_F(PerformanceProfiler, ReportsMetadataStatisticsSlowRowsAndEmptyCapture)
{
	for(bool populated:{false,true})
	{
		Recorder r(1);r.start(0);
		if(populated){r.beginFrame(0,1,120,120);r.push(Category::Logic,10);r.add(Counter::CompletedTicks);r.pop(60);r.endFrame(100,2);}
		r.stop();Metadata m;m.title="synthetic";m.git="abc";m.frequency=1000;m.interpolation=true;
		m.label="test-window";m.startedUtc="20261008T000000Z";
		std::ostringstream summary,frames,categories,slow;report(r,m,summary,frames,categories,slow);
		EXPECT_NE(summary.str().find("git=abc"),std::string::npos);
		EXPECT_NE(summary.str().find("capture_label=test-window"),std::string::npos);
		EXPECT_NE(summary.str().find("capture_started_utc=20261008T000000Z"),std::string::npos);
		EXPECT_NE(summary.str().find(populated?"completed_logic_ticks=1":"completed_logic_ticks=0"),std::string::npos);
		EXPECT_NE(categories.str().find("p95_ms,p99_ms"),std::string::npos);
		EXPECT_EQ(categories.str().find(",nan"),std::string::npos);EXPECT_EQ(categories.str().find(",inf"),std::string::npos);
		if(populated){EXPECT_NE(slow.str().find("completed_logic_frame,1,0"),std::string::npos);EXPECT_NE(frames.str().find("0,0,1,2,120,120"),std::string::npos);}
	}
}
TEST_F(PerformanceProfiler, OverflowAndRegressedClockAreReportedWithoutUnderflow)
{
	Recorder r(1);r.start(0);r.beginFrame(0,0,120,120);
	for(unsigned int i=1;i<MaxDepth;++i)ASSERT_TRUE(r.push(Category::AIObject,10));
	EXPECT_FALSE(r.push(Category::AIObject,10));
	for(unsigned int i=1;i<MaxDepth;++i)r.pop(9);
	r.endFrame(20,0);EXPECT_GT(r.errors(),0u);
	EXPECT_EQ(r.frames()[0].metrics[index(Category::AIObject)].inclusive,0u);
}
}
