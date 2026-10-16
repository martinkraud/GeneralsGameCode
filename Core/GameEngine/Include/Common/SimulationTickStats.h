// SPDX-License-Identifier: GPL-3.0-or-later
// X64.38: distribution statistics for authoritative simulation ticks (pure, engine-free, deterministic).
//
// Averages hide the failures that matter for a 30 TPS lockstep simulation: one 80 ms tick among 100 ms of 5 ms ticks is a visible hitch
// in single player and a stall for every peer in multiplayer. This module reports the whole distribution and the real-time
// consequences of it:
//
//   * mean, median, p90, p95, p99, p99.9, worst;
//   * counts above 16.67 / 25 / 33.333 / 50 / 66.67 ms (half, 3/4, one, 1.5 and 2 tick periods);
//   * a real-time model: the authoritative clock issues a tick every 33.333 ms; a tick that starts late (a backlog) is a CATCH-UP
//     tick, a tick that finishes after its own deadline (start of the slot + one period) is a MISSED DEADLINE; the model also yields
//     the largest backlog and the sustained TPS the sequence could hold in real time (<= 30).
//
// The model assumes the engine runs ticks back to back as soon as they are due (the FramePacer behaviour); it is derived from measured
// tick durations and is labelled as such everywhere it is printed - it is not a measurement of a paced run.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace SimulationTickStats
{
constexpr double kPeriodMs = 1000.0 / 30.0;                 // 33.333... ms: the authoritative tick period
constexpr double kThresholdsMs[5] = {16.67, 25.0, kPeriodMs, 50.0, 66.67};
constexpr int kThresholdCount = 5;

struct Summary
{
	size_t count = 0;
	double mean = 0, median = 0, p90 = 0, p95 = 0, p99 = 0, p999 = 0, worst = 0, best = 0, total = 0;
	size_t above[kThresholdCount] = {0, 0, 0, 0, 0};        // strictly greater than the threshold
	// real-time model (see above)
	size_t missedDeadlines = 0;                             // finished after the end of its own slot
	size_t catchUpTicks = 0;                                // started after the start of its own slot
	size_t longestCatchUpRun = 0;                           // consecutive catch-up ticks
	double maxBacklogMs = 0;                                // largest (start - scheduled start)
	double modelWallMs = 0;                                 // finish of the last tick on the model clock
	double sustainedTps = 0;                                // min(30, count / model wall seconds)
	double uncappedTps = 0;                                 // count / total busy seconds (how fast the ticks run back to back)
};

// Percentile by the nearest-rank method on a sorted copy (p in [0,100]); empty input gives 0.
inline double percentile(std::vector<double> values, double p)
{
	if (values.empty()) return 0.0;
	std::sort(values.begin(), values.end());
	if (p <= 0.0) return values.front();
	if (p >= 100.0) return values.back();
	size_t rank = static_cast<size_t>(std::ceil(p / 100.0 * static_cast<double>(values.size())));
	if (rank < 1) rank = 1;
	if (rank > values.size()) rank = values.size();
	return values[rank - 1];
}

inline Summary summarize(const std::vector<double> &tickMs)
{
	Summary s;
	s.count = tickMs.size();
	if (tickMs.empty()) return s;
	std::vector<double> sorted = tickMs;
	std::sort(sorted.begin(), sorted.end());
	for (double v : sorted) s.total += v;
	s.mean = s.total / static_cast<double>(s.count);
	s.best = sorted.front();
	s.worst = sorted.back();
	s.median = percentile(sorted, 50.0);
	s.p90 = percentile(sorted, 90.0);
	s.p95 = percentile(sorted, 95.0);
	s.p99 = percentile(sorted, 99.0);
	s.p999 = percentile(sorted, 99.9);
	for (double v : tickMs)
		for (int i = 0; i < kThresholdCount; ++i)
			if (v > kThresholdsMs[i]) ++s.above[i];
	// the real-time model, in execution order
	double finish = 0.0;
	size_t run = 0;
	for (size_t i = 0; i < tickMs.size(); ++i)
	{
		const double scheduled = static_cast<double>(i) * kPeriodMs;
		const double start = finish > scheduled ? finish : scheduled;
		const double backlog = start - scheduled;
		if (backlog > 1e-9)
		{
			++s.catchUpTicks;
			++run;
			if (run > s.longestCatchUpRun) s.longestCatchUpRun = run;
		}
		else
			run = 0;
		if (backlog > s.maxBacklogMs) s.maxBacklogMs = backlog;
		finish = start + tickMs[i];
		if (finish > scheduled + kPeriodMs + 1e-9) ++s.missedDeadlines;
	}
	s.modelWallMs = finish;
	const double wallSeconds = finish / 1000.0;
	s.sustainedTps = wallSeconds > 0.0 ? std::min(30.0, static_cast<double>(s.count) / wallSeconds) : 0.0;
	s.uncappedTps = s.total > 0.0 ? static_cast<double>(s.count) / (s.total / 1000.0) : 0.0;
	return s;
}
}  // namespace SimulationTickStats
