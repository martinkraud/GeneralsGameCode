/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include "Common/PresentationClock.h"

namespace
{
const float Period = 1.0f / 30.0f;

// Independent scheduler fixture: use the engine's existing float accumulator
// policy, then compare presentation time with a double-precision elapsed oracle.
void checkCadence(int fps, bool jitter)
{
	PresentationClock clock;
	float remainder = 0.0f;
	unsigned int frame = 0;
	double elapsed = 0.0;
	int validCount = 0;
	double maxTimeError = 0.0;
	int lastCompletion = -1;
	int shortestGap = fps;
	int longestGap = 0;
	const float jitterFactors[] = {0.71f, 1.23f, 0.86f, 1.20f};
	float lastDelta = 1.0f / fps;
	for (int i = 0; i < fps * 120; ++i)
	{
		const float nextDelta = (1.0f / fps) * (jitter ? jitterFactors[i % 4] : 1.0f);
		bool completed;
		if (fps == 30)
		{
			// The production 30-cap branch accepts immediately; no accumulator.
			completed = true;
			++frame;
			clock.observeScheduler(0.0f, 0.0f);
		}
		else
		{
			remainder += std::min(lastDelta, Period);
			completed = remainder >= Period;
			if (completed)
			{
				remainder -= Period;
				++frame;
			}
			clock.observeScheduler(remainder, Period);
		}
		elapsed += lastDelta;
		if (completed)
		{
			if (lastCompletion >= 0)
			{
				shortestGap = std::min(shortestGap, i - lastCompletion);
				longestGap = std::max(longestGap, i - lastCompletion);
			}
			lastCompletion = i;
		}
		clock.advance(frame, completed, nextDelta, true);
		const PresentationTiming& timing = clock.getTiming();
		ASSERT_EQ(timing.generation, frame);
		if (fps == 30)
		{
			ASSERT_FALSE(timing.valid);
			ASSERT_EQ(timing.alpha, 1.0f);
		}
		else if (timing.valid)
		{
			++validCount;
			ASSERT_EQ(timing.previousGeneration + 1, timing.generation);
			const double displayedTime = (double(timing.previousGeneration) + double(timing.alpha)) * double(Period);
			const double expectedTime = std::min(elapsed + nextDelta - double(Period), frame * double(Period));
			maxTimeError = std::max(maxTimeError, std::abs(displayedTime - expectedTime));
			ASSERT_NEAR(displayedTime, expectedTime, 0.0001) << "iteration " << i;
			ASSERT_NEAR(timing.alpha, std::min(1.0f, (remainder + nextDelta) / Period), 0.000001);
		}
		lastDelta = nextDelta;
	}
	if (fps != 30)
	{
		EXPECT_GT(validCount, fps * 100);
		EXPECT_NEAR(frame, elapsed / Period, 1.01);
	}
	if (!jitter)
	{
		testing::Test::RecordProperty("RenderFps", fps);
		testing::Test::RecordProperty("Iterations", fps * 120);
		testing::Test::RecordProperty("CompletedFrames", frame);
		testing::Test::RecordProperty("ShortestAcceptanceGap", shortestGap);
		testing::Test::RecordProperty("LongestAcceptanceGap", longestGap);
		testing::Test::RecordProperty("MaxTimeErrorNanoseconds", static_cast<int>(maxTimeError * 1e9));
		EXPECT_EQ(shortestGap, fps / 30);
		EXPECT_EQ(longestGap, (fps + 29) / 30);
	}
}

class PresentationCadence : public testing::TestWithParam<int> {};
TEST_P(PresentationCadence, RetainsSchedulerRemainderForTwoMinutes)
{
	checkCadence(GetParam(), false);
}
INSTANTIATE_TEST_SUITE_P(RenderCaps, PresentationCadence, testing::Values(30, 60, 120, 144, 165, 240));

TEST(PresentationClock, JitterUsesElapsedTimeNotRenderCount)
{
	const int rates[] = {60, 120, 144, 165, 240};
	for (int fps : rates)
	{
		SCOPED_TRACE(fps);
		checkCadence(fps, true);
	}
}

void prime(PresentationClock& clock)
{
	clock.observeScheduler(0.0f, Period);
	clock.advance(10, true, Period / 4, true);
	EXPECT_FALSE(clock.getTiming().valid);
	clock.observeScheduler(Period / 8, Period);
	clock.advance(11, true, Period / 4, true);
	ASSERT_TRUE(clock.getTiming().valid);
}

TEST(PresentationClock, NonIntegerCompletionKeepsResidualInsteadOfResettingPhase)
{
	const int rates[] = {144, 165};
	for (int fps : rates)
	{
		PresentationClock clock;
		prime(clock);
		const float delta = 1.0f / fps;
		const float residual = (fps == 144 ? 5 : 6) * delta - Period;
		clock.observeScheduler(residual, Period);
		clock.advance(12, true, delta, true);
		EXPECT_NEAR(clock.getTiming().alpha, (residual + delta) / Period, 0.000001);
		EXPECT_GT(clock.getTiming().alpha, delta / Period + 0.01f);
	}
}

TEST(PresentationClock, ResetSameFrameAndResumeRequireFreshCompletedPair)
{
	PresentationClock clock;
	prime(clock);
	const unsigned int epoch = clock.getTiming().epoch;
	clock.reset();
	EXPECT_NE(clock.getTiming().epoch, epoch);
	EXPECT_FALSE(clock.getTiming().valid);
	clock.observeScheduler(0.0f, Period);
	clock.advance(11, false, Period / 4, true);
	EXPECT_FALSE(clock.getTiming().valid);
	prime(clock); // Even the same frame values now belong to a new epoch.
}

TEST(PresentationClock, PauseFreezeHaltOrFastPolicySnapsAndReprimes)
{
	PresentationClock clock;
	prime(clock);
	const unsigned int epoch = clock.getTiming().epoch;
	clock.observeScheduler(Period / 8, Period);
	clock.advance(11, false, Period / 4, false);
	EXPECT_FALSE(clock.getTiming().valid);
	EXPECT_EQ(clock.getTiming().alpha, 1.0f);
	EXPECT_NE(clock.getTiming().epoch, epoch);
	const unsigned int pausedEpoch = clock.getTiming().epoch;
	clock.observeScheduler(Period / 8, Period);
	clock.advance(11, false, Period / 4, false);
	EXPECT_EQ(clock.getTiming().epoch, pausedEpoch); // No epoch churn while paused.
	prime(clock);
}

TEST(PresentationClock, NetworkAcceptedWaitingAndVariableArrivalAlwaysSnap)
{
	PresentationClock clock;
	prime(clock);
	const unsigned int frames[] = {12, 12, 12, 13, 14, 14};
	unsigned int prior = 11;
	for (unsigned int frame : frames)
	{
		clock.observeScheduler(0.0f, 0.0f); // Network readiness has no offline remainder.
		clock.advance(frame, frame != prior, Period / 4, false);
		EXPECT_FALSE(clock.getTiming().valid);
		EXPECT_EQ(clock.getTiming().generation, frame);
		EXPECT_EQ(clock.getTiming().previousGeneration, frame);
		EXPECT_EQ(clock.getTiming().alpha, 1.0f);
		prior = frame;
	}
}

TEST(PresentationClock, StallAndBelowThirtyFpsCannotExtrapolate)
{
	const float deltas[] = {Period, 1.0f / 15, 5.0f};
	for (float delta : deltas)
	{
		PresentationClock clock;
		prime(clock);
		clock.observeScheduler(Period / 2, Period);
		clock.advance(12, true, delta, true);
		EXPECT_FALSE(clock.getTiming().valid);
		EXPECT_EQ(clock.getTiming().alpha, 1.0f);
		prime(clock);
	}
}

TEST(PresentationClock, MissingObservationAndGenerationJumpsInvalidate)
{
	PresentationClock clock;
	prime(clock);
	clock.advance(11, false, Period / 4, true);
	EXPECT_FALSE(clock.getTiming().valid);
	prime(clock);
	clock.observeScheduler(0.0f, Period);
	clock.advance(15, true, Period / 4, true);
	EXPECT_FALSE(clock.getTiming().valid);
	clock.observeScheduler(0.0f, Period);
	clock.advance(16, true, Period / 4, true);
	EXPECT_TRUE(clock.getTiming().valid);
	clock.observeScheduler(0.0f, Period);
	clock.advance(5, false, Period / 4, true); // Unannounced load/reset.
	EXPECT_FALSE(clock.getTiming().valid);
}

TEST(PresentationClock, SkippedRenderingDoesNotChangeClockAndAlphaSaturates)
{
	PresentationClock clock;
	prime(clock);
	clock.observeScheduler(Period * 0.9f, Period);
	clock.advance(11, false, Period / 4, true);
	EXPECT_TRUE(clock.getTiming().valid);
	EXPECT_EQ(clock.getTiming().alpha, 1.0f);
	// No draw calls at all: scheduler observations and completions keep advancing.
	for (unsigned int frame = 12; frame < 20; ++frame)
	{
		clock.observeScheduler(Period / 8, Period);
		clock.advance(frame, true, Period / 4, true);
		EXPECT_TRUE(clock.getTiming().valid);
		EXPECT_EQ(clock.getTiming().generation, frame);
	}
}

TEST(PresentationClock, InvalidNumericInputsCannotPublishValidTiming)
{
	PresentationClock clock;
	prime(clock);
	clock.observeScheduler(-0.1f, Period);
	clock.advance(12, true, Period / 4, true);
	EXPECT_FALSE(clock.getTiming().valid);
	prime(clock);
	clock.observeScheduler(0.0f, Period);
	clock.advance(12, true, std::numeric_limits<float>::quiet_NaN(), true);
	EXPECT_FALSE(clock.getTiming().valid);
	prime(clock);
	clock.observeScheduler(Period, Period);
	clock.advance(12, true, Period / 4, true);
	EXPECT_FALSE(clock.getTiming().valid);
}

TEST(PresentationClock, RepeatedCompletionAndFrameWrapStartNewEpoch)
{
	PresentationClock clock;
	prime(clock);
	unsigned int epoch = clock.getTiming().epoch;
	clock.observeScheduler(0.0f, Period);
	clock.advance(11, true, Period / 4, true);
	EXPECT_FALSE(clock.getTiming().valid);
	EXPECT_NE(clock.getTiming().epoch, epoch);
	const unsigned int last = std::numeric_limits<unsigned int>::max();
	clock.reset();
	clock.observeScheduler(0.0f, Period);
	clock.advance(last - 1, true, Period / 4, true);
	clock.observeScheduler(0.0f, Period);
	clock.advance(last, true, Period / 4, true);
	EXPECT_TRUE(clock.getTiming().valid);
	epoch = clock.getTiming().epoch;
	clock.observeScheduler(0.0f, Period);
	clock.advance(0, true, Period / 4, true);
	EXPECT_FALSE(clock.getTiming().valid);
	EXPECT_NE(clock.getTiming().epoch, epoch);
}
}
