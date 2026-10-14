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

#include "Common/DisplayGeometry.h"
#include "Common/DisplayModes.h"

using namespace DisplayGeometry;

namespace
{
const Size kSizes[] = {
	{800, 600}, {1024, 768}, {1280, 720}, {1920, 1080}, {2560, 1440}, {3440, 1440}, {3840, 2160},
	{5120, 1440}, {2560, 1080}, {3840, 1080}, {1366, 768}, {1000, 700}, {1777, 999},
};
}

TEST(DisplayGeometry, LayoutIsCenteredAndWorldIsWholeSurface)
{
	for (const Size &size : kSizes)
	{
		const Layout l = computeLayout(size);
		EXPECT_EQ(l.physical, size);
		EXPECT_EQ((l.world == Rect{0, 0, size.width, size.height}), true);
		EXPECT_GE(l.canvas.x, 0);
		EXPECT_GE(l.canvas.y, 0);
		EXPECT_LE(l.canvas.right(), size.width);
		EXPECT_LE(l.canvas.bottom(), size.height);
		EXPECT_EQ(l.canvas.x, (size.width - l.canvas.width) / 2);
		EXPECT_EQ(l.canvas.y, (size.height - l.canvas.height) / 2);
	}
}

TEST(DisplayGeometry, AutoScaleFitsAndSnapsToQuarterSteps)
{
	EXPECT_EQ(autoUiScalePercent({800, 600}), 100);
	EXPECT_EQ(autoUiScalePercent({1280, 720}), 100);    // height 720/600 = 120 -> 100
	EXPECT_EQ(autoUiScalePercent({1920, 1080}), 175);   // 180 -> 175
	EXPECT_EQ(autoUiScalePercent({2560, 1440}), 225);   // 240 -> 225
	EXPECT_EQ(autoUiScalePercent({3840, 2160}), 350);   // 360 -> 350
	EXPECT_EQ(autoUiScalePercent({5120, 1440}), 225);   // height bound, not width
	EXPECT_EQ(autoUiScalePercent({640, 480}), 80);      // below native: exact fit
}

TEST(DisplayGeometry, RequestedScaleIsClampedToWhatFits)
{
	EXPECT_EQ(resolveUiScalePercent({1920, 1080}, 200), 180);
	EXPECT_EQ(resolveUiScalePercent({1920, 1080}, 150), 150);
	EXPECT_EQ(resolveUiScalePercent({1920, 1080}, 10), 50);
	EXPECT_EQ(resolveUiScalePercent({1920, 1080}, 0), 175);
	EXPECT_EQ(resolveUiScalePercent({800, 600}, 300), 100);
}

TEST(DisplayGeometry, UltrawideCanvasIsNotStretched)
{
	for (const Size &size : {Size{3440, 1440}, Size{5120, 1440}, Size{3840, 1080}})
	{
		const Layout l = computeLayout(size);
		EXPECT_TRUE(l.isWide());
		// Canvas keeps the 4:3 art aspect.
		EXPECT_EQ(l.canvas.width * 3, l.canvas.height * 4);
		EXPECT_LT(l.canvas.width, size.width);
		EXPECT_EQ(l.world.width, size.width);
	}
	EXPECT_FALSE(computeLayout({1024, 768}).isWide());
}

TEST(DisplayGeometry, ReferenceToPhysicalRoundTrips)
{
	for (const Size &size : kSizes)
	{
		for (int percent : {0, 100, 150, 200})
		{
			const Layout l = computeLayout(size, percent);
			for (Anchor a : {Anchor::Center, Anchor::Right, Anchor::Bottom, Anchor::BottomRight, Anchor::TopLeft})
			{
				for (Point p : {Point{0, 0}, Point{400, 300}, Point{799, 599}, Point{123, 456}})
				{
					const Point phys = referenceToPhysical(l, p, a);
					const Point back = physicalToReference(l, phys, a);
					EXPECT_EQ(back, p) << size.width << "x" << size.height << " " << l.uiScalePercent;
				}
			}
		}
	}
}

TEST(DisplayGeometry, AnchorsKeepEdgeDistances)
{
	const Layout l = computeLayout({3440, 1440});
	const int scale = l.uiScalePercent;
	const Rect rightBar = referenceToPhysical(l, Rect{752, 100, 48, 300}, Anchor::Right);
	EXPECT_EQ(rightBar.right(), l.physical.width);
	const Rect bottomBar = referenceToPhysical(l, Rect{0, 416, 800, 184}, Anchor::Bottom);
	EXPECT_EQ(bottomBar.bottom(), l.physical.height);
	EXPECT_EQ(bottomBar.height, scaled(184, scale));
	const Rect corner = referenceToPhysical(l, Rect{600, 400, 200, 200}, Anchor::BottomRight);
	EXPECT_EQ(corner.right(), l.physical.width);
	EXPECT_EQ(corner.bottom(), l.physical.height);
	const Rect topLeft = referenceToPhysical(l, Rect{0, 0, 100, 50}, Anchor::TopLeft);
	EXPECT_EQ(topLeft.x, 0);
	EXPECT_EQ(topLeft.y, 0);
}

TEST(DisplayGeometry, AdjacentRectsNeverGapOrOverlap)
{
	const Layout l = computeLayout({1920, 1080}, 175);
	for (int x = 0; x < 790; x += 7)
	{
		const Rect a = referenceToPhysical(l, Rect{x, 10, 7, 20});
		const Rect b = referenceToPhysical(l, Rect{x + 7, 10, 7, 20});
		EXPECT_EQ(a.right(), b.x);
	}
}

TEST(DisplayGeometry, InferAnchorFromReferenceRect)
{
	EXPECT_EQ(inferAnchor(Rect{752, 100, 48, 300}), Anchor::Right);
	EXPECT_EQ(inferAnchor(Rect{0, 416, 800, 184}), Anchor::Bottom);
	EXPECT_EQ(inferAnchor(Rect{600, 400, 200, 200}), Anchor::BottomRight);
	EXPECT_EQ(inferAnchor(Rect{100, 100, 200, 200}), Anchor::Center);
}

TEST(DisplayGeometry, HorPlusPreservesVerticalFov)
{
	const double hfov = 1.0471975512;   // 60 degrees at 4:3
	const double vfov43 = verticalFovForAspect(hfov, 4.0 / 3.0);
	for (const Size &size : kSizes)
	{
		const double aspect = static_cast<double>(size.width) / size.height;
		const double h = horizontalFovForAspect(hfov, aspect);
		EXPECT_NEAR(verticalFovForAspect(h, aspect), vfov43, 1e-9);
		EXPECT_GE(h, hfov - 1e-9) << "wider surfaces never see less than 4:3";
	}
	EXPECT_NEAR(horizontalFovForAspect(hfov, 4.0 / 3.0), hfov, 1e-12);
}

TEST(DisplayModes, FilterDedupesCanonicalizesAndDropsJunk)
{
	using namespace DisplayModes;
	std::vector<Mode> raw;
	auto add = [&](int w, int h, int hz, int bpp = 32, bool il = false) {
		Mode m; m.width = w; m.height = h; m.refreshHz = hz; m.bitsPerPixel = bpp; m.interlaced = il;
		raw.push_back(m);
	};
	add(1920, 1080, 59);
	add(1920, 1080, 60);
	add(1920, 1080, 60, 16);     // 16 bpp dropped
	add(1920, 1080, 144);
	add(1920, 1080, 120, 32, true);   // interlaced dropped
	add(640, 480, 60);           // below minimum dropped
	add(2560, 1440, 165);
	add(2560, 1440, 164);
	add(800, 600, 0);
	const std::vector<Mode> modes = filterModes(raw);
	ASSERT_EQ(modes.size(), 4u);
	EXPECT_EQ(resolutionsOf(modes).size(), 3u);
	EXPECT_EQ(refreshRatesFor(modes, {1920, 1080}), (std::vector<int>{60, 144}));
	EXPECT_EQ(refreshRatesFor(modes, {2560, 1440}), (std::vector<int>{165}));
	EXPECT_EQ(bestRefreshFor(modes, {1920, 1080}), 144);
	EXPECT_EQ(bestRefreshFor(modes, {1234, 567}), 0);
	EXPECT_TRUE(offers(modes, {2560, 1440}, 165));
	EXPECT_FALSE(offers(modes, {2560, 1440}, 144));    // no fake modes
	EXPECT_EQ(nearestResolution(modes, {1900, 1000}), (Size{1920, 1080}));
	EXPECT_EQ(nearestResolution({}, {1900, 1000}), Size{});
}

TEST(DisplayModes, LabelsAndFamilies)
{
	using namespace DisplayModes;
	EXPECT_EQ(resolutionLabel({1920, 1080}), "1920x1080");
	EXPECT_EQ(refreshLabel(144), "144 Hz");
	EXPECT_EQ(refreshLabel(0), "Auto");
	EXPECT_STREQ(aspectFamily({800, 600}), "4:3");
	EXPECT_STREQ(aspectFamily({1920, 1080}), "16:9");
	EXPECT_STREQ(aspectFamily({2560, 1080}), "21:9");
	EXPECT_STREQ(aspectFamily({3440, 1440}), "21:9");
	EXPECT_STREQ(aspectFamily({5120, 1440}), "32:9");
	EXPECT_STREQ(aspectFamily({1920, 1200}), "16:10");
}

TEST(DisplayGeometry, LayoutFamiliesAndAnchorPolicy)
{
	EXPECT_EQ(layoutKindForFile("Window\\ControlBar.wnd"), LayoutKind::Hud);
	EXPECT_EQ(layoutKindForFile("ControlBarPopupDescription.wnd"), LayoutKind::Hud);
	EXPECT_EQ(layoutKindForFile("Window\\GenPowersShortcutBarUS.wnd"), LayoutKind::Hud);
	EXPECT_EQ(layoutKindForFile("window/ReplayControl.wnd"), LayoutKind::Hud);
	EXPECT_EQ(layoutKindForFile("Window\\InGameChat.wnd"), LayoutKind::Hud);
	EXPECT_EQ(layoutKindForFile("Window\\Menus\\MainMenu.wnd"), LayoutKind::Frontend);
	EXPECT_EQ(layoutKindForFile("Window\\Menus\\SkirmishGameOptionsMenu.wnd"), LayoutKind::Frontend);
	EXPECT_EQ(layoutKindForFile(nullptr), LayoutKind::Frontend);
	// Frontend: everything on the centered canvas, whatever it touches.
	EXPECT_EQ(anchorFor(LayoutKind::Frontend, Rect{595, 546, 200, 49}), Anchor::Center);
	// Hud: one vertical origin (the bottom) for the bar, its popup, the chat line and the sidebar; the sidebar
	// additionally hugs the right edge.
	EXPECT_EQ(anchorFor(LayoutKind::Hud, Rect{0, 416, 800, 184}), Anchor::Bottom);
	EXPECT_EQ(anchorFor(LayoutKind::Hud, Rect{8, 376, 648, 40}), Anchor::Bottom);
	EXPECT_EQ(anchorFor(LayoutKind::Hud, Rect{740, 159, 60, 270}), Anchor::BottomRight);
}
