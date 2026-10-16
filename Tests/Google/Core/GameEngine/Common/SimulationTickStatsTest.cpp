// SPDX-License-Identifier: GPL-3.0-or-later
// X64.38: distribution statistics and the derived real-time model for simulation ticks (pure; no engine, no clock).

#include <gtest/gtest.h>

#include "Common/SimulationTickStats.h"

using namespace SimulationTickStats;

TEST(SimulationTickStats, EmptyInputIsAllZero)
{
	const Summary s = summarize({});
	EXPECT_EQ(s.count, 0u);
	EXPECT_EQ(s.worst, 0.0);
	EXPECT_EQ(s.sustainedTps, 0.0);
	EXPECT_EQ(percentile({}, 99.0), 0.0);
}

TEST(SimulationTickStats, NearestRankPercentiles)
{
	std::vector<double> v;
	for (int i = 1; i <= 100; ++i) v.push_back(static_cast<double>(i));
	const Summary s = summarize(v);
	EXPECT_EQ(s.count, 100u);
	EXPECT_DOUBLE_EQ(s.median, 50.0);
	EXPECT_DOUBLE_EQ(s.p90, 90.0);
	EXPECT_DOUBLE_EQ(s.p95, 95.0);
	EXPECT_DOUBLE_EQ(s.p99, 99.0);
	EXPECT_DOUBLE_EQ(s.worst, 100.0);
	EXPECT_DOUBLE_EQ(s.best, 1.0);
	EXPECT_DOUBLE_EQ(s.mean, 50.5);
}

TEST(SimulationTickStats, OrderDoesNotChangeTheDistribution)
{
	const std::vector<double> a = {5, 80, 5, 5, 40, 5, 5, 12, 5, 5};
	std::vector<double> b(a.rbegin(), a.rend());
	const Summary sa = summarize(a), sb = summarize(b);
	EXPECT_DOUBLE_EQ(sa.p95, sb.p95);
	EXPECT_DOUBLE_EQ(sa.median, sb.median);
	for (int i = 0; i < kThresholdCount; ++i) EXPECT_EQ(sa.above[i], sb.above[i]);
}

TEST(SimulationTickStats, ThresholdCountsAreStrictlyGreater)
{
	const Summary s = summarize({16.67, 16.68, 25.0, 25.1, kPeriodMs, 50.0, 66.67, 70.0});
	EXPECT_EQ(s.above[0], 7u);   // > 16.67
	EXPECT_EQ(s.above[1], 5u);   // > 25
	EXPECT_EQ(s.above[2], 3u);   // > 33.333 (50, 66.67, 70)
	EXPECT_EQ(s.above[3], 2u);   // > 50
	EXPECT_EQ(s.above[4], 1u);   // > 66.67
}

TEST(SimulationTickStats, FastTicksHoldThirtyTpsWithNoMissesOrCatchUp)
{
	const Summary s = summarize(std::vector<double>(300, 8.0));
	EXPECT_EQ(s.missedDeadlines, 0u);
	EXPECT_EQ(s.catchUpTicks, 0u);
	EXPECT_EQ(s.maxBacklogMs, 0.0);
	EXPECT_DOUBLE_EQ(s.sustainedTps, 30.0);
	EXPECT_NEAR(s.uncappedTps, 125.0, 1e-9);
}

TEST(SimulationTickStats, OneLongTickMissesItsDeadlineAndCausesCatchUp)
{
	std::vector<double> v(60, 5.0);
	v[10] = 100.0;                       // 100 ms: three periods; the following ticks start late until the backlog drains
	const Summary s = summarize(v);
	// tick 10 ends 66.7 ms after its slot; ticks 11..13 start late (backlog 66.7 -> 38.3 -> 10) and the first two still end after their
	// own slot; tick 14 is on time again
	EXPECT_EQ(s.missedDeadlines, 3u);
	EXPECT_EQ(s.catchUpTicks, 3u);
	EXPECT_NEAR(s.maxBacklogMs, 100.0 - kPeriodMs, 1e-6);
	EXPECT_EQ(s.longestCatchUpRun, 3u);
	EXPECT_DOUBLE_EQ(s.worst, 100.0);
}

TEST(SimulationTickStats, SustainedOverloadIsBelowThirtyTps)
{
	const Summary s = summarize(std::vector<double>(300, 50.0));
	EXPECT_NEAR(s.sustainedTps, 20.0, 0.2);
	EXPECT_GT(s.missedDeadlines, 250u);
	EXPECT_GT(s.maxBacklogMs, 1000.0);
}
