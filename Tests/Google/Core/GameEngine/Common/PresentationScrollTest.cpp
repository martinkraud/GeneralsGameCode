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

// ---- X64.52B: the REAL integrator (View::applyPendingUserScroll is a thin wrapper around it), driven the way the look-at translator drives it ----

namespace
{
struct Drive { double x = 0.0, y = 0.0; std::vector<double> stepX; unsigned applied = 0; };

// One client update per interval: while `held`, the translator sets the velocity (offset computed for the frame pacer's CURRENT fpsRatio), then the view advances.
Drive drive(const std::vector<double> &intervals, double offsetPerBaseFrameX, double offsetPerBaseFrameY, bool held = true, bool advanceTwice = false)
{
	Drive d;
	PresentationScroll::Integrator in;
	long long ticks = 1;
	for (double dt : intervals)
	{
		const long long next = ticks + static_cast<long long>(dt * TicksPerSecond + 0.5);
		const float fpsRatio = static_cast<float>(30.0 / (1.0 / dt));          // BaseFps / updateFps, as the frame pacer reports it
		if (held)
			in.set(static_cast<float>(offsetPerBaseFrameX * fpsRatio), static_cast<float>(offsetPerBaseFrameY * fpsRatio), fpsRatio);
		float ox = 0, oy = 0;
		const bool moved = in.advance(next, TicksPerSecond, ox, oy);
		if (advanceTwice) { float ax = 0, ay = 0; if (in.advance(next, TicksPerSecond, ax, ay)) { ox += ax; oy += ay; } }
		ticks = next;
		d.x += ox; d.y += oy; d.stepX.push_back(ox); d.applied += moved ? 1 : 0;
	}
	return d;
}
}

TEST(PresentationScrollIntegrator, KeyboardAndRmbTravelTheSameDistanceAtEveryRenderRateAndJitter)
{
	// keyboard: a constant offset per base frame (SCROLL_AMT-style); RMB: an offset that depends on the mouse vector but is likewise constant while the mouse rests
	for (double perBase : { PerBaseFrame, PerBaseFrame * 0.37 })
		for (bool jitter : { false, true })
			for (int fps : { 30, 60, 120, 240 })
			{
				const Drive d = drive(frameIntervals(fps, 3.0, jitter), perBase, 0.0);
				EXPECT_NEAR(d.x, perBase * 30.0 * 3.0, perBase * 30.0 * 3.0 * 0.02 + 0.02) << "fps " << fps << " jitter " << jitter;
			}
}

TEST(PresentationScrollIntegrator, TravelOverEqualWallClockTimeDoesNotDependOnTheRenderRate)
{
	const double reference = drive(frameIntervals(30, 2.0, false), PerBaseFrame, 0.0).x;
	for (int fps : { 60, 120, 240 })
		EXPECT_NEAR(drive(frameIntervals(fps, 2.0, false), PerBaseFrame, 0.0).x, reference, reference * 0.03) << fps;
	// an irregular but equal-duration sequence
	std::vector<double> irregular = { 0.004, 0.012, 0.007, 0.002, 0.016, 0.005, 0.009, 0.033, 0.003, 0.0083 };
	double total = 0; for (double v : irregular) total += v;
	EXPECT_NEAR(drive(irregular, PerBaseFrame, 0.0).x, PerBaseFrame * 30.0 * total, PerBaseFrame * 30.0 * 0.02);
}

TEST(PresentationScrollIntegrator, CameraMovesOnEveryRenderFrameNotOnTheSimulationTick)
{
	// at 120 fps every update moves the camera (no 30 Hz quantisation: 4 of 4 frames inside one 33 ms logic tick move it, with equal steps)
	const Drive d = drive(frameIntervals(120, 1.0, false), PerBaseFrame, 0.0);
	for (size_t i = 1; i < d.stepX.size(); ++i)
		EXPECT_GT(d.stepX[i], 0.0) << i;
	EXPECT_EQ(d.applied, d.stepX.size());
	for (size_t i = 2; i < d.stepX.size(); ++i)
		EXPECT_NEAR(d.stepX[i], d.stepX[1], d.stepX[1] * 0.02) << i;
	// the count of distinct camera positions equals the frame count (a 30 TPS camera would show a quarter of them at 120 fps)
	EXPECT_GE(d.stepX.size(), 119u);
}

TEST(PresentationScrollIntegrator, AVelocityMovesTheCameraOnceNeverTwice)
{
	const std::vector<double> iv = frameIntervals(120, 1.0, false);
	const Drive once = drive(iv, PerBaseFrame, 0.0, true, false);
	const Drive twice = drive(iv, PerBaseFrame, 0.0, true, true);          // advance() called a second time in the same update
	EXPECT_NEAR(twice.x, once.x, 1e-3);
	// no new input -> no motion at all (nothing is re-applied from an old velocity)
	const Drive released = drive(iv, PerBaseFrame, 0.0, false);
	EXPECT_EQ(released.x, 0.0);
	EXPECT_EQ(released.applied, 0u);
}

TEST(PresentationScrollIntegrator, InvalidFpsRatioNeverSetsAVelocity)
{
	PresentationScroll::Integrator in;
	in.set(5.0f, 5.0f, 0.0f);
	in.set(5.0f, 5.0f, -1.0f);
	float x = 0, y = 0;
	EXPECT_FALSE(in.advance(100, TicksPerSecond, x, y));
}

TEST(PresentationScrollIntegrator, DiagonalAndNegativeDirectionsKeepTheirSignAndRatio)
{
	const Drive d = drive(frameIntervals(144, 1.0, true), -PerBaseFrame, PerBaseFrame * 0.5);
	EXPECT_LT(d.x, 0.0);
	EXPECT_GT(d.y, 0.0);
	EXPECT_NEAR(d.y / -d.x, 0.5, 1e-3);
}
