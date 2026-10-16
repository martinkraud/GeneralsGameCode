// SPDX-License-Identifier: GPL-3.0-or-later
// X64.38: the worker-count policy and the parallelise-or-not threshold (pure).

#include <gtest/gtest.h>

#include "Common/SimulationWorkerPlan.h"

using namespace SimulationWorkerPlan;

static Topology topo(int logical, int physical) { Topology t; t.logicalProcessors = logical; t.physicalCores = physical; return t; }

TEST(SimulationWorkerPlan, AutoIsSerialOnSmallMachines)
{
	EXPECT_EQ(autoWorkers(topo(2, 2)), 0);
	EXPECT_EQ(autoWorkers(topo(4, 4)), 0);
	EXPECT_EQ(autoWorkers(topo(8, 4)), 0);     // 4 cores with SMT is still a 4-core machine
	EXPECT_EQ(autoWorkers(topo(1, 1)), 0);
}

TEST(SimulationWorkerPlan, AutoLeavesTwoCoresAndCapsAtSix)
{
	EXPECT_EQ(autoWorkers(topo(12, 6)), 4);
	EXPECT_EQ(autoWorkers(topo(16, 8)), 6);
	EXPECT_EQ(autoWorkers(topo(32, 16)), 6);
	EXPECT_EQ(autoWorkers(topo(64, 32)), 6);
	EXPECT_EQ(autoWorkers(topo(10, 5)), 3);
}

TEST(SimulationWorkerPlan, AutoCountsCoresNotThreadsAndMakesNoHybridAssumption)
{
	// a 20-thread hybrid desktop CPU reports 14 cores (8 P with SMT + 6 E): every core counts once
	EXPECT_EQ(autoWorkers(topo(20, 14)), 6);
	// SMT must not double the count
	EXPECT_EQ(autoWorkers(topo(16, 8)), autoWorkers(topo(8, 8)));
}

TEST(SimulationWorkerPlan, ExplicitSettingsAreHonouredButBounded)
{
	EXPECT_EQ(workersFor(0, topo(16, 8)), 0);
	EXPECT_EQ(workersFor(1, topo(16, 8)), 1);
	EXPECT_EQ(workersFor(8, topo(16, 8)), 8);
	EXPECT_EQ(workersFor(8, topo(4, 2)), 3);      // never more than logical processors - 1
	EXPECT_EQ(workersFor(4, topo(1, 1)), 0);
	EXPECT_EQ(workersFor(kAuto, topo(16, 8)), 6);
}

TEST(SimulationWorkerPlan, UnknownSettingsFallBackToAuto)
{
	EXPECT_EQ(workersFor(5, topo(16, 8)), autoWorkers(topo(16, 8)));
	EXPECT_EQ(workersFor(99, topo(16, 8)), autoWorkers(topo(16, 8)));
	EXPECT_EQ(workersFor(-7, topo(16, 8)), autoWorkers(topo(16, 8)));
}

TEST(SimulationWorkerPlan, SmallWorkStaysSerial)
{
	EXPECT_FALSE(worthParallelising(50.0, 4, 20.0));        // 50 us of work, 20 us to hand it over: no
	EXPECT_FALSE(worthParallelising(1000.0, 0, 20.0));      // no workers
	EXPECT_FALSE(worthParallelising(0.0, 4, 20.0));
	EXPECT_TRUE(worthParallelising(2000.0, 4, 20.0));       // 2 ms of work: yes
}

TEST(SimulationWorkerPlan, MoreWorkersNeverMakeWorkWorthItThatWasNotWorthItWithTheOverhead)
{
	// the saved time is bounded by serialCost: even infinite workers cannot beat the overhead on tiny work
	EXPECT_FALSE(worthParallelising(30.0, 64, 20.0));
}
