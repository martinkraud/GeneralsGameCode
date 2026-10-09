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
Tick timestamp=0,queries=0;
Tick fakeClock(){++queries;return timestamp;}
class PathProfiler : public testing::Test
{
	void SetUp() override {activeRecorder=nullptr;activePath=nullptr;timestamp=queries=0;}
	void TearDown() override {activeRecorder=nullptr;activePath=nullptr;}
};
TEST_F(PathProfiler, DisabledAndShallowDoNotReadClockOrAllocateRecords)
{
	{PathScope s(PathKind::Internal,fakeClock);pathCount(PathWork::HeadPops);EXPECT_FALSE(s.enabled());}
	Recorder r(1);r.start(0);r.beginFrame(0,1,120,120);activeRecorder=&r;
	{PathScope s(PathKind::Internal,fakeClock);EXPECT_FALSE(s.enabled());}
	EXPECT_EQ(queries,0u);EXPECT_TRUE(r.paths().empty());EXPECT_FALSE(r.pathsFull());
	activeRecorder=nullptr;r.endFrame(100,1);
}
TEST_F(PathProfiler, IndividualNestedIntervalsAreLinkedAndExclusive)
{
	Recorder r(1,8);r.start(0);r.beginFrame(0,70,120,120);activeRecorder=&r;
	{
		timestamp=10;PathScope request(PathKind::Request,fakeClock);
		request.sample()->object=42;request.sample()->layer=3;request.sample()->surfaces=7;
		timestamp=20;
		{PathScope search(PathKind::Hierarchical,fakeClock);pathCount(PathWork::HeadPops,8);search.result(true);timestamp=50;}
		timestamp=60;
		{PathScope search(PathKind::Internal,fakeClock);pathCount(PathWork::HeadPops,12);search.result(false);timestamp=90;}
		request.result(false);timestamp=100;
	}
	activeRecorder=nullptr;r.endFrame(110,71);
	ASSERT_EQ(r.paths().size(),3u);const auto& p=r.paths();
	EXPECT_EQ(p[0].inclusive,90u);EXPECT_EQ(p[0].exclusive,30u);
	EXPECT_EQ(p[1].inclusive,30u);EXPECT_EQ(p[2].inclusive,30u);
	EXPECT_EQ(p[1].parent,p[0].id);EXPECT_EQ(p[2].parent,p[0].id);
	EXPECT_EQ(p[1].request,PathKind::Request);EXPECT_EQ(p[1].object,42u);EXPECT_EQ(p[1].layer,3);
	EXPECT_EQ(p[1].logic,70u);EXPECT_EQ(p[1].outer,0u);
	EXPECT_EQ(p[0].work[0],20u);EXPECT_EQ(p[1].work[0],8u);EXPECT_EQ(p[2].work[0],12u);
	EXPECT_EQ(p[1].outcome,PathOutcome::ReturnedPath);EXPECT_EQ(p[2].outcome,PathOutcome::NullAfterWork);
	EXPECT_EQ(queries,6u);EXPECT_EQ(activePath,nullptr);
}
TEST_F(PathProfiler, BoundedStorageDropsRecordsWithoutGrowingOrMisattributingWork)
{
	Recorder r(1,1);r.start(0);r.beginFrame(0,0,120,120);activeRecorder=&r;
	{
		PathScope parent(PathKind::Ground,fakeClock);const auto* allocation=r.paths().data();
		{PathScope dropped(PathKind::Hierarchical,fakeClock);EXPECT_FALSE(dropped.enabled());pathCount(PathWork::HeadPops,99);}
		EXPECT_EQ(activePath,&parent);pathCount(PathWork::HeadPops,1);
		EXPECT_EQ(r.paths().data(),allocation);
	}
	activeRecorder=nullptr;r.endFrame(100,1);
	EXPECT_EQ(r.paths().size(),1u);EXPECT_TRUE(r.pathsFull());EXPECT_EQ(r.droppedPaths(),1u);
	EXPECT_EQ(r.paths()[0].work[0],1u);
}
TEST_F(PathProfiler, OutcomesDistinguishEarlyNullWorkNullAndClosestReturn)
{
	Recorder r(1,3);r.start(0);r.beginFrame(0,0,120,120);activeRecorder=&r;
	{PathScope s(PathKind::Internal,fakeClock);s.result(false);}
	{PathScope s(PathKind::Internal,fakeClock);pathCount(PathWork::HeadPops);s.result(false);}
	{PathScope s(PathKind::Closest,fakeClock);s.closest();s.result(true);}
	activeRecorder=nullptr;r.endFrame(100,1);
	EXPECT_EQ(r.paths()[0].outcome,PathOutcome::NullBeforeWork);
	EXPECT_EQ(r.paths()[1].outcome,PathOutcome::NullAfterWork);
	EXPECT_EQ(r.paths()[2].outcome,PathOutcome::ReturnedClosest);
}
TEST_F(PathProfiler, PhaseCountersPropagateAndScopeUnwinds)
{
	Recorder r(1,3);r.start(0);r.beginFrame(0,0,120,120);activeRecorder=&r;
	try
	{
		PathScope search(PathKind::Internal,fakeClock);timestamp=10;
		{PathScope phase(PathKind::Cleanup,fakeClock);pathCount(PathWork::CleanedCells,19);timestamp=30;}
		timestamp=40;throw 1;
	}
	catch(int){}
	EXPECT_EQ(activePath,nullptr);activeRecorder=nullptr;r.endFrame(100,1);
	EXPECT_EQ(r.paths()[0].work[static_cast<unsigned int>(PathWork::CleanedCells)],19u);
	EXPECT_EQ(r.paths()[0].inclusive,40u);EXPECT_EQ(r.paths()[0].exclusive,20u);
}
TEST_F(PathProfiler, ResetRetainsBoundAndCannotInvalidateLiveRecords)
{
	Recorder r(1,2);r.start(0);r.beginFrame(0,0,120,120);activeRecorder=&r;
	{PathScope s(PathKind::Queue,fakeClock);s.sample()->queueBefore=7;s.sample()->queueAfter=3;r.start(99);r.stop();}
	activeRecorder=nullptr;r.endFrame(100,1);r.stop();
	EXPECT_EQ(r.paths()[0].queueBefore,7);EXPECT_EQ(r.paths()[0].queueAfter,3);
	r.start(200);EXPECT_TRUE(r.paths().empty());EXPECT_EQ(r.droppedPaths(),0u);
}
TEST_F(PathProfiler, IndividualCSVAndStatisticsHandleEmptyAndPopulatedCaptures)
{
	for(bool populated:{false,true})
	{
		Recorder r(1,3);r.start(0);r.beginFrame(0,8,120,120);activeRecorder=&r;
		if(populated){timestamp=10;{PathScope s(PathKind::Hierarchical,fakeClock);pathCount(PathWork::HeadPops,50);s.result(false);timestamp=110;}}
		activeRecorder=nullptr;r.endFrame(120,9);r.stop();
		Metadata meta;meta.frequency=1000;std::ostringstream summary,csv;reportPaths(r,meta,summary,csv);
		EXPECT_NE(csv.str().find("parent_id,outer_index,logic_before"),std::string::npos);
		EXPECT_NE(csv.str().find("forward_hops_inclusive_count"),std::string::npos);
		EXPECT_NE(summary.str().find(populated?"hierarchical samples=1 total_ms=100":"hierarchical samples=0 total_ms=0"),std::string::npos);
		std::istringstream rows(csv.str());std::string row,field;
		while(std::getline(rows,row))
		{
			std::istringstream fields(row);
			while(std::getline(fields,field,',')){EXPECT_NE(field,"nan");EXPECT_NE(field,"inf");EXPECT_NE(field,"-inf");}
		}
		if(populated)EXPECT_NE(csv.str().find("null_after_work"),std::string::npos);
	}
}
TEST_F(PathProfiler, SourceCategoryMaskAndRepeatedSearchesStayDistinct)
{
	Recorder r(1,3);r.start(0);r.beginFrame(0,8,120,120);r.push(Category::AIPlayer,0);activeRecorder=&r;
	{PathScope s(PathKind::Ground,fakeClock);timestamp=10;}
	{PathScope s(PathKind::Ground,fakeClock);timestamp=30;}
	activeRecorder=nullptr;r.pop(40);r.endFrame(50,9);
	ASSERT_EQ(r.paths().size(),2u);EXPECT_NE(r.paths()[0].id,r.paths()[1].id);
	EXPECT_NE(r.paths()[0].sourceCategories&(Tick(1)<<static_cast<unsigned int>(Category::AIPlayer)),0u);
	EXPECT_EQ(r.paths()[0].inclusive,10u);EXPECT_EQ(r.paths()[1].inclusive,20u);
}
TEST_F(PathProfiler, CoordinateReportRoundTripsFractionalAndIntegerBoundaryValues)
{
    Recorder r(1,1);r.start(0);r.beginFrame(0,91,120,120);
    auto* sample=r.beginPath(PathKind::Internal,0,0);
    sample->fromX=1090.5f;sample->fromY=1023.99994f;
    sample->toX=3870.5f;sample->toY=1100.5f;
    r.endFrame(1,92);
    Metadata meta;meta.frequency=1000;std::ostringstream summary,csv;
    reportPaths(r,meta,summary,csv);
    std::istringstream input(csv.str());std::string line,field;
    std::getline(input,line);std::getline(input,line);std::istringstream row(line);
    for(int i=0;i<16;++i)std::getline(row,field,',');
    for(float expected:{sample->fromX,sample->fromY,sample->toX,sample->toY})
    {
        std::getline(row,field,',');std::istringstream value(field);value.imbue(std::locale::classic());
        float actual=0;value>>actual;ASSERT_TRUE(value);EXPECT_EQ(actual,expected);
        EXPECT_EQ(static_cast<std::uint64_t>(actual),static_cast<std::uint64_t>(expected));
    }
}

}
