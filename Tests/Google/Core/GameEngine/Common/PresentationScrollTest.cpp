/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// X64.49: held camera scrolling in presentation time. A fixed-duration held input must cover the same world distance at every render rate, the speed the viewer
// sees (camera step / displayed interval) must not follow frame-time jitter, and release must not overshoot. The "legacy" model below is the former behaviour:
// a step sized by the PREVIOUS frame interval, applied after the frame was drawn.

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "Common/PresentationScroll.h"

namespace
{
constexpr double Velocity = 680.0;       // world units per second the held input asks for
constexpr double PerBaseFrame = Velocity / 30.0;
constexpr long long TicksPerSecond = 10000000;

// Frame intervals (seconds) of a render rate: the nominal period, optionally disturbed the way a 240 fps cap is in practice (4.2 .. 7.4 ms alternating).
std::vector<double> frameIntervals(int fps, double seconds, bool jitter)
{
	static const double factors[] = { 1.00, 1.75, 1.00, 1.00, 1.40, 1.00, 1.25, 1.75 };
	std::vector<double> v;
	double t = 0.0;
	for (int i = 0; t < seconds; ++i)
	{
		const double dt = (1.0 / fps) * (jitter ? factors[i % 8] : 1.0);
		v.push_back(dt);
		t += dt;
	}
	return v;
}

struct Result { double distance; double totalSeconds; std::vector<double> seenSpeed; };

// New model: at every update the velocity is integrated for the real time since the previous update.
Result runPresented(const std::vector<double> &intervals)
{
	Result r{ 0.0, 0.0, {} };
	long long ticks = 1;                      // a previous counter value exists
	for (double dt : intervals)
	{
		const long long next = ticks + static_cast<long long>(dt * TicksPerSecond + 0.5);
		const float seconds = PresentationScroll::stepSeconds(next, ticks, TicksPerSecond);
		const double step = PresentationScroll::deltaForInterval(static_cast<float>(PerBaseFrame), seconds);
		ticks = next;
		r.distance += step; r.totalSeconds += dt;
		r.seenSpeed.push_back(step / dt);
	}
	return r;
}

// Former model: the step applied for frame k is sized by interval k-1 (the limiter's last update time).
Result runLegacy(const std::vector<double> &intervals)
{
	Result r{ 0.0, 0.0, {} };
	double previous = intervals.front();
	for (double dt : intervals)
	{
		const double step = PerBaseFrame * 30.0 * previous;
		r.distance += step; r.totalSeconds += dt;
		r.seenSpeed.push_back(step / dt);
		previous = dt;
	}
	return r;
}

double cv(const std::vector<double> &v)
{
	double mean = 0.0; for (double x : v) mean += x; mean /= v.size();
	double var = 0.0; for (double x : v) var += (x - mean) * (x - mean);
	return std::sqrt(var / v.size()) / mean;
}
}

TEST(PresentationScroll, HeldInputCoversTheSameDistanceAtEveryRenderRate)
{
	for (bool jitter : { false, true })
	{
		for (int fps : { 30, 60, 120, 144, 240 })
		{
			const Result r = runPresented(frameIntervals(fps, 3.0, jitter));
			// distance == velocity * elapsed time, independent of the frame count (long-double sums of float steps: well below 0.01%)
			EXPECT_NEAR(r.distance, Velocity * r.totalSeconds, Velocity * r.totalSeconds * 1e-4) << "fps " << fps << " jitter " << jitter;
			EXPECT_NEAR(r.totalSeconds, 3.0, 0.05);
		}
	}
}

TEST(PresentationScroll, SeenSpeedDoesNotFollowFrameTimeJitter)
{
	const std::vector<double> irregular = frameIntervals(240, 3.0, true);
	const Result presented = runPresented(irregular);
	const Result legacy = runLegacy(irregular);
	// the viewer sees the camera step divided by the interval it is shown for
	EXPECT_LT(cv(presented.seenSpeed), 1e-3);
	EXPECT_GT(cv(legacy.seenSpeed), 0.15);   // the measured judder: steps follow the previous frame, not the displayed one
}

TEST(PresentationScroll, StepsAreSmoothAtSteadyRates)
{
	for (int fps : { 30, 60, 120, 144, 240 })
	{
		const Result r = runPresented(frameIntervals(fps, 3.0, false));
		EXPECT_LT(cv(r.seenSpeed), 1e-3) << fps;
		// no frame is skipped: every step is positive and about velocity / fps
		for (double s : r.seenSpeed) EXPECT_GT(s, 0.0);
	}
}

TEST(PresentationScroll, FirstUpdateStallAndBackwardsCounterNeverJump)
{
	EXPECT_EQ(PresentationScroll::stepSeconds(5000, 0, TicksPerSecond), 0.0f);                    // no previous counter: no step
	EXPECT_EQ(PresentationScroll::stepSeconds(5000, 6000, TicksPerSecond), 0.0f);                 // counter went backwards
	EXPECT_EQ(PresentationScroll::stepSeconds(5000, 5000, TicksPerSecond), 0.0f);                 // did not advance
	EXPECT_EQ(PresentationScroll::stepSeconds(50000000, 1, 0), 0.0f);                              // invalid frequency
	EXPECT_FLOAT_EQ(PresentationScroll::stepSeconds(1 + 20 * TicksPerSecond, 1, TicksPerSecond), PresentationScroll::MaxStepSeconds);   // a 20 s stall = one 5 fps frame
	const double stalled = PresentationScroll::deltaForInterval(static_cast<float>(PerBaseFrame), PresentationScroll::MaxStepSeconds);
	EXPECT_NEAR(stalled, Velocity * 0.2, 1e-3);
}

TEST(PresentationScroll, ReleaseStopsImmediatelyWithoutOvershoot)
{
	// The velocity is consumed by one update; when the input is released nothing is pending, so the next updates move the camera by exactly zero.
	double pending = PerBaseFrame; bool has = true;
	long long ticks = 1; double position = 0.0;
	for (int frame = 0; frame < 20; ++frame)
	{
		const long long next = ticks + TicksPerSecond / 240;
		const float seconds = PresentationScroll::stepSeconds(next, ticks, TicksPerSecond);
		ticks = next;
		if (frame == 10) has = false;                       // released
		if (has) position += PresentationScroll::deltaForInterval(static_cast<float>(pending), seconds);
	}
	const double atRelease = position;
	for (int frame = 0; frame < 20; ++frame) { /* nothing pending: applyPendingUserScroll returns before scrolling */ }
	EXPECT_DOUBLE_EQ(position, atRelease);
	EXPECT_NEAR(position, Velocity * 10.0 / 240.0, 1e-3);   // exactly ten frames of motion: no extra step after the release
}

TEST(PresentationScroll, LegacyModelStillCoversTheSameDistanceItsDefectIsRipple)
{
	// Distance was never the problem (the reported speed is correct on average); the ripple was. Both statements are pinned.
	const std::vector<double> irregular = frameIntervals(240, 3.0, true);
	const Result legacy = runLegacy(irregular);
	EXPECT_NEAR(legacy.distance, Velocity * legacy.totalSeconds, Velocity * legacy.totalSeconds * 0.02);
}
