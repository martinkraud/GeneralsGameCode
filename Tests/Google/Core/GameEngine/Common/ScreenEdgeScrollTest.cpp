/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// X64.52B: screen-edge camera scrolling. The X64.52A human test found it dead: a "ScreenEdgeScrollEnabledInWindowedApp = no" saved by an earlier build won over
// the modern runtime's default although the modern runtime can only present in a window. These tests would have failed on that candidate.

#include <gtest/gtest.h>
#include "Common/ScreenEdgeScroll.h"

using namespace ScreenEdgeScroll;

TEST(ScreenEdgeScroll, ModernRuntimeScrollsAtTheEdgeUnlessBothPreferencesAreOff)
{
	// (windowed pref, fullscreen pref) -> allowed; the modern runtime presents in a window and has a single presentation mode
	EXPECT_TRUE(allowed(true, true, true, true));
	EXPECT_TRUE(allowed(false, true, true, true));      // the stale "windowed = no" of the X64.52A test: must NOT disable the edge
	EXPECT_TRUE(allowed(true, false, true, true));
	EXPECT_FALSE(allowed(false, false, true, true));    // both explicitly off: the only way to switch it off
}

TEST(ScreenEdgeScroll, ClassicRuntimeKeepsTheRetailPerModeSemantics)
{
	EXPECT_FALSE(allowed(false, true, true, false));    // windowed Win32 build with windowed = no: off
	EXPECT_TRUE(allowed(true, false, true, false));
	EXPECT_TRUE(allowed(false, true, false, false));    // fullscreen follows the fullscreen preference
	EXPECT_FALSE(allowed(true, false, false, false));
}

TEST(ScreenEdgeScroll, EdgeBandAtEveryDisplayConfiguration)
{
	struct Size { unsigned w, h; };
	for (Size s : { Size{ 800, 600 }, Size{ 1400, 1050 }, Size{ 1920, 1080 }, Size{ 1920, 1200 }, Size{ 2560, 1440 }, Size{ 3440, 1440 }, Size{ 5120, 1440 } })
	{
		const int w = (int)s.w, h = (int)s.h;
		// the four borders (cursor limited to the display: 0 .. size-1)
		EXPECT_TRUE(hit(0, h / 2, s.w, s.h).left) << w;
		EXPECT_TRUE(hit(kEdgeSize - 1, h / 2, s.w, s.h).left);
		EXPECT_TRUE(hit(w - 1, h / 2, s.w, s.h).right) << w;
		EXPECT_TRUE(hit(w - kEdgeSize, h / 2, s.w, s.h).right);
		EXPECT_TRUE(hit(w / 2, 0, s.w, s.h).top);
		EXPECT_TRUE(hit(w / 2, h - 1, s.w, s.h).bottom) << h;
		// the centre and the first pixel inside the band are not the edge
		EXPECT_FALSE(hit(w / 2, h / 2, s.w, s.h).any());
		EXPECT_FALSE(hit(kEdgeSize, h / 2, s.w, s.h).any());
		EXPECT_FALSE(hit(w - kEdgeSize - 1, h / 2, s.w, s.h).any());
		EXPECT_FALSE(hit(w / 2, kEdgeSize, s.w, s.h).any());
		EXPECT_FALSE(hit(w / 2, h - kEdgeSize - 1, s.w, s.h).any());
		// corners scroll both ways
		const Hit corner = hit(w - 1, h - 1, s.w, s.h);
		EXPECT_TRUE(corner.right && corner.bottom && !corner.left && !corner.top);
	}
}

TEST(ScreenEdgeScroll, DegenerateDisplaySizesDoNotMisfireOnTheFarBorder)
{
	EXPECT_FALSE(hit(0, 0, 0, 0).right);
	EXPECT_FALSE(hit(0, 0, 0, 0).bottom);
	EXPECT_TRUE(hit(0, 0, 0, 0).left);     // x < 3 holds at the origin by definition
}
