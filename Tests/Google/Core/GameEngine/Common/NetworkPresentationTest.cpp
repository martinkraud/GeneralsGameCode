/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// Multiplayer smoothness audit: the presentation clock fed with the lock-step (network) schedule, the pacing decision, and the frame-delivery statistics.
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <string>
#include "Common/FrameDeliveryStats.h"
#include "Common/PresentationClock.h"
#include "GameNetwork/NetworkPacing.h"

namespace
{
// Integer model of Network::timeForNewFrame() + Network::getTimingDiagnostics() on a QPC-style counter (10 MHz): at most one tick is released per render iteration, at
// the first iteration at or after the due time; the schedule advances by frameDelay (= freq / rate, truncated) and re-anchors when more than two periods behind. It
// offers (now - anchor, delay) in seconds, as Network::getTimingDiagnostics() does.
constexpr int64_t Freq = 10000000;
struct LockStepSchedule
{
	int64_t nextFrameTime = 0, anchor = 0, delay = 0;
	unsigned released = 0, resets = 0;
	bool commandsReady = true;          // false = other players' commands are late
	unsigned slugEvery = 0;             // >0: every N-th interval is 10% longer (self-slug)
	unsigned slugCounter = 0;

	bool tryRelease(int64_t now)
	{
		int64_t frameDelay = Freq / 30;
		if (slugEvery > 0 && ((++slugCounter) % slugEvery) == 0) frameDelay += frameDelay / 10;
		if (!commandsReady) return false;
		if (now >= nextFrameTime)
		{
			if (nextFrameTime + 2 * frameDelay < now) { nextFrameTime = now; anchor = now; ++resets; }
			else { anchor = nextFrameTime; nextFrameTime += frameDelay; }
			delay = frameDelay;
			++released;
			return true;
		}
		return false;
	}
	bool valid() const { return delay > 0; }
	float remainder(int64_t now) const { return static_cast<float>(now - anchor) / static_cast<float>(Freq); }
	float period() const { return static_cast<float>(delay) / static_cast<float>(Freq); }
};

struct RunResult
{
	unsigned frames = 0, ticks = 0, validFrames = 0, epochChanges = 0, alphaRegressions = 0;
	double maxAlphaJump = 0.0;
};

// Mirrors GameEngine::update() + FramePacer::update(): observe after the decision, then advance with the measured interval of the iteration that follows.
RunResult run(double renderHz, double seconds, double jitterFraction, unsigned slugEvery, double stallFrom, double stallTo, unsigned hitchEvery = 0)
{
	PresentationClock clock;
	LockStepSchedule schedule;
	schedule.slugEvery = slugEvery;
	RunResult r;
	int64_t now = 0;
	unsigned frame = 0;
	unsigned lastEpoch = clock.getTiming().epoch;
	float lastAlpha = 1.0f;
	bool lastValid = false;
	uint32_t rng = 12345;
	auto next = [&]() { rng = rng * 1664525u + 1013904223u; return static_cast<double>(rng >> 8) / static_cast<double>(1 << 24); };
	const double dt0 = 1.0 / renderHz;
	const int64_t end = static_cast<int64_t>(seconds * Freq);
	while (now < end)
	{
		const double seconds_now = static_cast<double>(now) / Freq;
		schedule.commandsReady = !(seconds_now >= stallFrom && seconds_now < stallTo);
		const bool completed = schedule.tryRelease(now);
		if (completed) { ++frame; ++r.ticks; }
		double nextDt = dt0 * (1.0 + jitterFraction * (next() * 2.0 - 1.0));
		if (hitchEvery > 0 && (r.frames % hitchEvery) == hitchEvery - 1) nextDt = 0.028;
		const int64_t endOfIteration = now + static_cast<int64_t>(nextDt * Freq);
		const bool stalling = !schedule.commandsReady && now >= schedule.nextFrameTime;   // Network::update(): commands late although the tick is due
		if (schedule.valid() && !stalling && nextDt < schedule.period())
		{
			// FramePacer::update(): read the schedule AFTER the limiter wait, hold at just below one period while the next tick waits for the next iteration
			const float rem = std::min(schedule.remainder(endOfIteration), schedule.period() * 0.9999f);
			clock.observeScheduler(rem, schedule.period());
		}
		clock.advance(frame, completed, 0.0f, true);
		const PresentationTiming &t = clock.getTiming();
		++r.frames;
		if (t.valid) ++r.validFrames;
		if (t.epoch != lastEpoch) { ++r.epochChanges; lastEpoch = t.epoch; }
		if (t.valid)
		{
			EXPECT_GE(t.alpha, 0.0f);
			EXPECT_LE(t.alpha, 1.0f);
			EXPECT_EQ(t.previousGeneration + 1, t.generation);
			if (lastValid && !completed && t.alpha + 1e-6f < lastAlpha) ++r.alphaRegressions;
			if (lastValid && !completed) r.maxAlphaJump = std::max(r.maxAlphaJump, static_cast<double>(t.alpha - lastAlpha));
		}
		lastAlpha = t.alpha;
		lastValid = t.valid;
		now = endOfIteration;
	}
	return r;
}
}

// ---- the lock-step schedule drives the clock like the offline accumulator does --------------------------------------------------------------------------------

class NetworkPresentation : public testing::TestWithParam<int> {};

TEST_P(NetworkPresentation, InterpolatesBetweenLockStepTicks)
{
	const RunResult r = run(GetParam(), 120.0, 0.0, 0, 1e9, 1e9);
	SCOPED_TRACE(GetParam());
	EXPECT_NEAR(r.ticks, 120 * 30, 3);                                  // authoritative cadence unchanged: 30 ticks/s
	EXPECT_GT(r.validFrames, r.frames * 99 / 100);                      // interpolation is available (the defect: 0%)
	EXPECT_EQ(r.epochChanges, 0u);                                      // no history resets in a steady match
	EXPECT_EQ(r.alphaRegressions, 0u);                                  // alpha never moves backwards between ticks
	EXPECT_LT(r.maxAlphaJump, 1.0 / (GetParam() / 30.0) + 0.05);        // a step per frame is about one render interval of a tick
}
INSTANTIATE_TEST_SUITE_P(RenderRates, NetworkPresentation, testing::Values(60, 75, 90, 120, 144, 165, 240));

TEST(NetworkPresentation, RenderJitterKeepsAlphaMonotonic)
{
	for (double hz : {60.0, 144.0, 240.0})
	{
		const RunResult r = run(hz, 60.0, 0.4, 0, 1e9, 1e9);
		SCOPED_TRACE(hz);
		EXPECT_EQ(r.alphaRegressions, 0u);
		EXPECT_GT(r.validFrames, r.frames * 95 / 100);
		EXPECT_NEAR(r.ticks, 60 * 30, 3);
	}
}

TEST(NetworkPresentation, RenderHitchesNeverMoveAlphaBackwards)
{
	// a 28 ms frame every 97th iteration: predicting the next alpha from the last interval used to step back by up to half a tick after such a frame
	for (double hz : {60.0, 120.0, 240.0})
	{
		const RunResult r = run(hz, 60.0, 0.2, 0, 1e9, 1e9, 97);
		SCOPED_TRACE(hz);
		EXPECT_EQ(r.alphaRegressions, 0u);
		EXPECT_GT(r.validFrames, r.frames * 90 / 100);
		EXPECT_NEAR(r.ticks, 60 * 30, 3);
	}
}

TEST(NetworkPresentation, SelfSlugDoesNotResetInterpolationHistory)
{
	// every 4th interval 10% longer: the period changes between 33.3 and 36.7 ms but the history stays continuous
	const RunResult r = run(120.0, 60.0, 0.0, 4, 1e9, 1e9);
	EXPECT_EQ(r.epochChanges, 0u);
	EXPECT_GT(r.validFrames, r.frames * 99 / 100);
	EXPECT_EQ(r.alphaRegressions, 0u);
}

TEST(NetworkPresentation, StallInvalidatesAndRecovers)
{
	// commands late for 400 ms in the middle of half a minute
	const RunResult r = run(120.0, 30.0, 0.0, 0, 10.0, 10.4);
	EXPECT_GE(r.epochChanges, 1u);                                      // the stall is visible as an invalidation, never as extrapolation
	EXPECT_GT(r.validFrames, r.frames * 95 / 100);                      // and interpolation resumes afterwards
	EXPECT_EQ(r.alphaRegressions, 0u);
}

TEST(NetworkPresentation, SlowRenderNeverInterpolates)
{
	// a 30 fps renderer cannot interpolate (render interval >= tick period): history stays invalid, alpha stays 1, the tick cadence is untouched
	const RunResult r = run(30.0, 30.0, 0.0, 0, 1e9, 1e9);
	EXPECT_EQ(r.validFrames, 0u);
	EXPECT_NEAR(r.ticks, 30 * 30, 3);
}

// ---- pacing decision -------------------------------------------------------------------------------------------------------------------------------------------

TEST(NetworkPacing, LogicRateNeverExceedsTheAuthoritativeRate)
{
	// a 120 fps render cap used to be the ceiling: two players at 60 / 144 fps were asked for 60 ticks/s while the slowest was held at 30
	EXPECT_EQ(NetworkPacing::negotiatedLogicRate(60, 30, 5, 30), 30);
	EXPECT_EQ(NetworkPacing::negotiatedLogicRate(144, 30, 5, 30), 30);
	EXPECT_EQ(NetworkPacing::negotiatedLogicRate(31, 30, 5, 30), 30);
}

TEST(NetworkPacing, SlowMachineStillLowersTheRate)
{
	EXPECT_EQ(NetworkPacing::negotiatedLogicRate(20, 30, 5, 30), 20);
	EXPECT_EQ(NetworkPacing::negotiatedLogicRate(3, 30, 5, 30), 5);       // floor
}

TEST(NetworkPacing, WithinTenPercentBelowKeepsTheCurrentRate)
{
	EXPECT_EQ(NetworkPacing::negotiatedLogicRate(27, 30, 5, 30), 30);
	EXPECT_EQ(NetworkPacing::negotiatedLogicRate(26, 30, 5, 30), 26);
	EXPECT_EQ(NetworkPacing::negotiatedLogicRate(29, 30, 5, 30), 30);
}

TEST(NetworkPacing, LowRenderCapStillLimitsTheRate)
{
	EXPECT_EQ(NetworkPacing::negotiatedLogicRate(60, 30, 5, 24), 24);
}

// ---- frame-delivery statistics ----------------------------------------------------------------------------------------------------------------------------

TEST(FrameDeliveryStats, PercentilesAreNearestRank)
{
	std::vector<float> v;
	for (int i = 1; i <= 100; ++i) v.push_back(static_cast<float>(i));
	const FrameDelivery::Percentiles p = FrameDelivery::summarize(v);
	EXPECT_EQ(p.count, 100u);
	EXPECT_DOUBLE_EQ(p.p50, 50.0);
	EXPECT_DOUBLE_EQ(p.p95, 95.0);
	EXPECT_DOUBLE_EQ(p.p99, 99.0);
	EXPECT_DOUBLE_EQ(p.max, 100.0);
	EXPECT_NEAR(p.mean, 50.5, 1e-9);
	EXPECT_EQ(FrameDelivery::summarize(std::vector<float>()).count, 0u);
}

TEST(FrameDeliveryStats, WindowCountsHitchesTicksAndValidity)
{
	FrameDelivery::Window w;
	for (int i = 0; i < 100; ++i)
	{
		FrameDelivery::FrameSample f;
		f.intervalMs = (i == 50) ? 40.0f : 8.333f;
		f.ticks = (i % 4 == 0) ? 1u : 0u;
		f.alphaValid = i >= 10;
		f.alpha = f.alphaValid ? static_cast<float>(i % 4) / 4.0f : 1.0f;
		f.epoch = 7;
		f.stalled = (i == 60);
		w.addFrame(f);
		if (f.ticks) w.addTick(i * 8.333, 0.5);
	}
	EXPECT_EQ(w.frames(), 100u);
	EXPECT_EQ(w.framesWithTicks(1), 25u);
	EXPECT_EQ(w.framesWithTicks(0), 75u);
	EXPECT_EQ(w.hitches(), 1u);
	EXPECT_EQ(w.stalledFrames(), 1u);
	EXPECT_NEAR(w.validPercent(), 90.0, 1e-9);
	EXPECT_EQ(w.epochChanges(), 0u);
	EXPECT_EQ(w.alphaRegressions(), 0u);
	EXPECT_NEAR(w.tickSpacing().p50, 4 * 8.333, 1e-3);
	EXPECT_EQ(w.ticks(), 25u);
}

TEST(FrameDeliveryStats, AlphaRegressionWithoutTickIsReported)
{
	FrameDelivery::Window w;
	FrameDelivery::FrameSample f;
	f.intervalMs = 8.0f;
	f.alphaValid = true;
	f.epoch = 1;
	f.alpha = 0.5f;
	w.addFrame(f);
	f.alpha = 0.3f;                         // moves backwards without a tick
	w.addFrame(f);
	EXPECT_EQ(w.alphaRegressions(), 1u);
}

// ---- LAN join compatibility (Public Alpha gate) -------------------------------------------------------------------------------------------------------------------------------------------
#include "GameNetwork/LanJoinCompat.h"

TEST(LanJoinCompat, OnlyTheSameBuildWithTheSameDataMayJoin)
{
	using LanJoinCompat::Identity;
	const Identity host = {0x1234ABCDu, 0x00C0FFEEu};
	EXPECT_TRUE(LanJoinCompat::compatible(host, host));
	EXPECT_TRUE(LanJoinCompat::compatible(host, Identity{0x1234ABCDu, 0x00C0FFEEu}));
	EXPECT_FALSE(LanJoinCompat::compatible(host, Identity{0x1234ABCEu, 0x00C0FFEEu}));   // a different executable (another build, or the retail one)
	EXPECT_FALSE(LanJoinCompat::compatible(host, Identity{0x1234ABCDu, 0x00C0FFEFu}));   // the same executable with different game data (INI / overlay)
	EXPECT_FALSE(LanJoinCompat::compatible(host, Identity{0u, 0u}));                      // a client that never computed its identity
	EXPECT_TRUE(LanJoinCompat::compatible(Identity{}, Identity{}));                        // symmetric by construction
	EXPECT_EQ(LanJoinCompat::compatible(host, Identity{1, 2}), LanJoinCompat::compatible(Identity{1, 2}, host));
}

TEST(LanJoinCompat, EnforcedInBothTitlesOnNative64BitAndKeepsTheUpstreamRuleOnlyFor32BitZeroHour)
{
	EXPECT_TRUE(LanJoinCompat::enforced(false, false));   // retail Generals always enforced it
	EXPECT_TRUE(LanJoinCompat::enforced(false, true));
	EXPECT_TRUE(LanJoinCompat::enforced(true, true));     // Zero Hour, native 64-bit: now enforced
	EXPECT_FALSE(LanJoinCompat::enforced(true, false));   // upstream 32-bit Zero Hour: unchanged
}

TEST(LanJoinCompat, TheRefusalTextSaysWhatToDo)
{
	const std::wstring text = LanJoinCompat::modernDenyText();
	EXPECT_NE(text.find(L"different version"), std::wstring::npos);
	EXPECT_NE(text.find(L"same"), std::wstring::npos);
	EXPECT_LT(text.size(), 200u);
}
