/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include "Common/PerformanceProfile.h"
#include "Common/DeveloperHarness.h"
namespace
{
volatile PerformanceProfile::Tick profileBenchmarkSink=0;
// Includes harness call cost; prevents the disabled branch from being folded out.
__declspec(noinline) void benchmarkScope()
{ PerformanceProfile::Scope scope(PerformanceProfile::Category::AIObject); }
__declspec(noinline) void benchmarkPathScope()
{ PerformanceProfile::PathScope scope(PerformanceProfile::PathKind::Internal); }
__declspec(noinline) void benchmarkPathCount()
{ PerformanceProfile::pathCount(PerformanceProfile::PathWork::HeadPops); }
__declspec(noinline) void benchmarkPhaseInsert(bool hooks)
{
	if(hooks){PerformanceProfile::PathInsertion insertion;PerformanceProfile::pathCount(PerformanceProfile::PathWork::OpenInserts);PerformanceProfile::pathCount(PerformanceProfile::PathWork::ForwardHops,100);}
	else {PerformanceProfile::pathCount(PerformanceProfile::PathWork::OpenInserts);PerformanceProfile::pathCount(PerformanceProfile::PathWork::ForwardHops,100);}
}
__declspec(noinline) void benchmarkPhaseExpansion(bool hooks)
{
	PerformanceProfile::pathCount(PerformanceProfile::PathWork::InfoAttempts,32);
	for(unsigned int i=0;i<8;++i)benchmarkPhaseInsert(hooks);
}
__declspec(noinline) void benchmarkPhaseIteration(bool hooks)
{
	PerformanceProfile::pathCount(PerformanceProfile::PathWork::HeadPops);
	if(hooks)
	{
		PerformanceProfile::PathIteration iteration;
		PerformanceProfile::PathPhase neighbor(PerformanceProfile::PathPhaseKind::Neighbor);
		{PerformanceProfile::PathPhase line(PerformanceProfile::PathPhaseKind::Line);benchmarkPhaseExpansion(hooks);}
		benchmarkPhaseExpansion(hooks);
	}
	else {benchmarkPhaseExpansion(hooks);benchmarkPhaseExpansion(hooks);}
}
TEST(PerformanceProfileBenchmark, DISABLED_SampledPhaseOverhead)
{
#ifdef _DEBUG
	GTEST_SKIP()<<"Release measurements only";
#endif
	using namespace PerformanceProfile;
	constexpr unsigned int iterations=262144;
	std::printf("PHASE_MEMORY,sample,%zu,capacity,%u,buffer_bytes,%zu\n",sizeof(PathSample),MaxPaths,sizeof(PathSample)*MaxPaths);
	// Mode 2 omits new hooks; mode 3 runs hooks but suppresses sampled clocks.
	const char* names[]={"disabled_with_hooks","aggregate_with_hooks","legacy_deep_no_hooks","deep_unsampled_hooks","deep_sampled_64","disabled_no_hooks","aggregate_no_hooks"};
	for(unsigned int mode=0;mode<7;++mode)
	{
		std::vector<double> times;
		for(unsigned int repeat=0;repeat<7;++repeat)
		{
			Recorder r(1,mode>=2 && mode<=4?8:0,mode==4);r.start(0);r.beginFrame(0,0,120,120);
			activeRecorder=mode!=0 && mode!=5?&r:nullptr;
			{
				Scope aggregate(Category::PathSearch);PathScope search(PathKind::Internal);
				const auto start=std::chrono::steady_clock::now();
				for(unsigned int i=0;i<iterations;++i)benchmarkPhaseIteration(mode!=2 && mode<5);
				times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
				if(mode==4){EXPECT_EQ(search.sample()->phases.selected,iterations/PathPhaseStride);EXPECT_EQ(search.sample()->phases.errors,0u);}
			}
			activeRecorder=nullptr;r.endFrame(now(),0);profileBenchmarkSink=r.paths().size();
		}
		std::sort(times.begin(),times.end());
		std::printf("PHASE_BENCH,%s,%u,7,%.6f,%.3f\n",names[mode],iterations,times[3],times[3]*1000000/iterations);
	}
}
TEST(PerformanceProfileBenchmark, DISABLED_DeveloperHarnessOverhead)
{
    constexpr unsigned iterations=1000000;
    for(unsigned mode=0;mode<3;++mode)
    {
        DeveloperTools::Controller controller(mode==2);if(mode==2)controller.prepared(0);
        std::vector<double> times;
        for(unsigned repeat=0;repeat<7;++repeat)
        {
            const auto start=std::chrono::steady_clock::now();
            for(unsigned i=0;i<iterations;++i)
            {
                if(mode==1){if(DeveloperTools::scenarioEnabled() || DeveloperTools::devModeEnabled())DeveloperTools::beginOuter();DeveloperTools::drawOverlay();}
                if(mode==2){if(i%500==0){controller=DeveloperTools::Controller(true);controller.prepared(i);}profileBenchmarkSink=controller.tick(i);}
                else profileBenchmarkSink=i;
            }
            times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        std::sort(times.begin(),times.end());std::printf("DEV_BENCH,%u,%.3f,ns_per_iteration\n",mode,times[3]*1000000/iterations);
    }
}
TEST(PerformanceProfileBenchmark, DISABLED_PathDetailOverhead)
{
#ifdef _DEBUG
	GTEST_SKIP()<<"Release measurements only";
#endif
	using namespace PerformanceProfile;
	std::printf("PATH_MEMORY,sample,%zu,capacity,%u,buffer_bytes,%zu\n",sizeof(PathSample),MaxPaths,sizeof(PathSample)*MaxPaths);
	for(unsigned int op=0;op<4;++op)
	{
		const unsigned int iterations=op<2?16384:500000;
		std::vector<double> times;
		for(unsigned int repeat=0;repeat<5;++repeat)
		{
			Recorder r(1,MaxPaths);r.start(0);r.beginFrame(0,0,120,120);
			activeRecorder=op==1||op==3?&r:nullptr;
			{
				PathScope parent(PathKind::Request);
				const auto start=std::chrono::steady_clock::now();
				for(unsigned int i=0;i<iterations;++i)
					if(op<2)benchmarkPathScope();else benchmarkPathCount();
				times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
			}
			activeRecorder=nullptr;r.endFrame(now(),0);profileBenchmarkSink=r.paths().size();
		}
		std::sort(times.begin(),times.end());
		const char* names[]={"disabled_path_scope","enabled_path_scope","disabled_node_counter","enabled_node_counter"};
		std::printf("PATH_BENCH,%s,%u,5,%.6f,%.3f\n",names[op],iterations,times[2],times[2]*1000000/iterations);
	}
}
TEST(PerformanceProfileBenchmark, DISABLED_ReleaseOverhead)
{
#ifdef _DEBUG
	GTEST_SKIP()<<"Release measurements only";
#endif
	using namespace PerformanceProfile;
	constexpr unsigned int iterations=500000;
	std::printf("PROFILE_MEMORY,frame,%zu,capacity,%u,buffer_bytes,%zu\n",sizeof(Frame),MaxFrames,sizeof(Frame)*MaxFrames);
	std::printf("PROFILE_BENCH,operation,iterations,repeats,median_ms,ns_per_operation\n");
	for(unsigned int op=0;op<5;++op)
	{
		std::vector<double> times;
		for(int repeat=0;repeat<5;++repeat)
		{
			Recorder r(1);r.start(0);r.beginFrame(0,0,120,120);activeRecorder=op==1||op==4?&r:nullptr;
			Tick checksum=0;
			const auto start=std::chrono::steady_clock::now();
			for(unsigned int i=0;i<iterations;++i)
			{
				if(op==0||op==1)benchmarkScope();
				else if(op==2)checksum+=now();
				else if(op==3){r.push(Category::AIObject,Tick(i)*10);r.pop(Tick(i)*10+5);}
				else count(Counter::UpdateModules);
			}
			const auto elapsed=std::chrono::steady_clock::now()-start;
			activeRecorder=nullptr;r.endFrame(Tick(iterations)*10,0);
			profileBenchmarkSink=checksum+r.frames()[0].metrics[static_cast<unsigned int>(Category::AIObject)].calls+r.frames()[0].counters[static_cast<unsigned int>(Counter::UpdateModules)];
			times.push_back(std::chrono::duration<double,std::milli>(elapsed).count());
		}
		std::sort(times.begin(),times.end());
		const char* names[]={"disabled_scope","enabled_qpc_scope","qpc_query","synthetic_push_pop","enabled_counter"};
		std::printf("PROFILE_BENCH,%s,%u,5,%.6f,%.3f\n",names[op],iterations,times[2],times[2]*1000000/iterations);
	}
	activeRecorder=nullptr;
}
}
