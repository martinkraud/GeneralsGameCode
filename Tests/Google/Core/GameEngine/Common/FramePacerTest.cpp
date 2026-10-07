/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <gtest/gtest.h>

#include "Common/FramePacer.h"
#include "GameNetwork/NetworkInterface.h"

// Inject deterministic elapsed time instead of sleeping or starting engine subsystems.
class TestFramePacer : public FramePacer
{
public:
	void setRenderTiming(Int fps)
	{
		m_maxFPS = fps;
		m_updateTime = 1.0f / fps;
	}
};

TEST(FramePacer, NormalOfflineRateDoesNotFollowRenderCap)
{
	ASSERT_EQ(TheNetwork, nullptr);
	TestFramePacer pacer;
	const Int renderRates[] = {30, 60, 120, 144, 240, 480};
	for (const Int fps : renderRates)
	{
		SCOPED_TRACE(fps);
		pacer.setRenderTiming(fps);
		EXPECT_TRUE(pacer.isLogicTimeScaleEnabled());
		EXPECT_EQ(pacer.getActualLogicTimeScaleFps(), 30);
		EXPECT_FLOAT_EQ(pacer.getActualLogicTimeScaleRatio(), 1.0f);
		EXPECT_FLOAT_EQ(pacer.getActualLogicTimeScaleOverFpsRatio(), 30.0f / fps);
		EXPECT_NEAR(pacer.getLogicTimeStepSeconds(), 1.0f / fps, 0.000001f);
	}
}

TEST(FramePacer, FrozenAndHaltedQueriesPreserveTheirIndependentOverrides)
{
	ASSERT_EQ(TheNetwork, nullptr);
	FramePacer pacer;
	pacer.setTimeFrozen(TRUE);
	EXPECT_EQ(pacer.getActualLogicTimeScaleFps(), 0);
	EXPECT_EQ(pacer.getActualLogicTimeScaleFps(FramePacer::IgnoreFrozenTime), 30);
	pacer.setGameHalted(TRUE);
	EXPECT_EQ(pacer.getActualLogicTimeScaleFps(FramePacer::IgnoreFrozenTime), 0);
	EXPECT_EQ(pacer.getActualLogicTimeScaleFps(FramePacer::IgnoreHaltedGame), 0);
	EXPECT_EQ(pacer.getActualLogicTimeScaleFps(FramePacer::IgnoreFrozenTime | FramePacer::IgnoreHaltedGame), 30);
	pacer.setTimeFrozen(FALSE);
	pacer.setGameHalted(FALSE);
	EXPECT_EQ(pacer.getActualLogicTimeScaleFps(), 30);
}

TEST(FramePacer, ExplicitSpeedAndLegacyControlsRemainAvailable)
{
	ASSERT_EQ(TheNetwork, nullptr);
	TestFramePacer pacer;
	pacer.setRenderTiming(120);
	pacer.setLogicTimeScaleFps(60);
	EXPECT_EQ(pacer.getActualLogicTimeScaleFps(), 60);
	EXPECT_FLOAT_EQ(pacer.getActualLogicTimeScaleRatio(), 2.0f);
	EXPECT_FLOAT_EQ(pacer.getActualLogicTimeScaleOverFpsRatio(), 0.5f);
	pacer.enableLogicTimeScale(FALSE);
	EXPECT_EQ(pacer.getActualLogicTimeScaleFps(), RenderFpsPreset::UncappedFpsValue);
	pacer.enableLogicTimeScale(TRUE);
	EXPECT_EQ(pacer.getActualLogicTimeScaleFps(), 60);
}

TEST(FramePacer, SlowRenderingDoesNotExtrapolateVisualTime)
{
	ASSERT_EQ(TheNetwork, nullptr);
	TestFramePacer pacer;
	pacer.setRenderTiming(15);
	EXPECT_EQ(pacer.getActualLogicTimeScaleFps(), 30);
	EXPECT_FLOAT_EQ(pacer.getActualLogicTimeScaleOverFpsRatio(), 1.0f);
	EXPECT_FLOAT_EQ(pacer.getLogicTimeStepSeconds(), SECONDS_PER_LOGICFRAME_REAL);
}
