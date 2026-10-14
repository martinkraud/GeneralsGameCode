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

#include "Common/DisplaySettings.h"
#include "Common/DisplayRuntime.h"

using namespace DisplayConfig;
using DisplayGeometry::Size;
using DisplayModes::Monitor;

namespace
{
Monitor makeMonitor(int x, int w, int h, bool primary, int hz = 60, int workInset = 40)
{
	Monitor m;
	m.bounds = {x, 0, w, h};
	m.work = {x, 0, w, h - workInset};
	m.primary = primary;
	m.desktop.width = w; m.desktop.height = h; m.desktop.refreshHz = hz;
	return m;
}

Environment env1080()
{
	Environment e;
	e.monitors.push_back(makeMonitor(0, 1920, 1080, true, 144));
	return e;
}
}

TEST(DisplaySettings, ClassicIgnoresStoredValues)
{
	KeyValues kv;
	store(Settings{true, 1, {2560, 1440}, 144, WindowMode::Borderless, true, 200, true}, kv);
	const Settings s = load(kv, true);
	EXPECT_EQ(s, classicDefaults());
	EXPECT_EQ(s.resolution, (Size{800, 600}));
	EXPECT_EQ(s.uiScalePercent, 100);
	EXPECT_FALSE(s.vsync);
	EXPECT_FALSE(s.smoothMotion);
	EXPECT_EQ(s.mode, WindowMode::Windowed);
}

TEST(DisplaySettings, LegacyPreferencesMigrateToModernAutomaticDefaults)
{
	KeyValues legacy = {{"Resolution", "800 600"}, {"FrameRateLimit", "120"}, {"FPSLimit", "yes"},
	                    {"MusicVolume", "55"}, {"SomethingUnknown", "x"}};
	const Settings s = load(legacy, false);
	EXPECT_FALSE(s.explicitSettings);
	EXPECT_FALSE(s.resolution.valid());                 // automatic
	EXPECT_EQ(s.mode, WindowMode::Windowed);
	EXPECT_EQ(s.uiScalePercent, 0);
	EXPECT_FALSE(s.vsync);
	EXPECT_FALSE(s.smoothMotion);
	EXPECT_EQ(s.monitor, 0);
}

TEST(DisplaySettings, RoundTripAndUnknownKeysSurvive)
{
	KeyValues kv = {{"Resolution", "800 600"}, {"MusicVolume", "55"}, {"Zebra", "stripes"}};
	Settings want;
	want.explicitSettings = true;
	want.monitor = 1; want.resolution = {3440, 1440}; want.mode = WindowMode::Borderless;
	want.vsync = true; want.uiScalePercent = 150; want.smoothMotion = true; want.refreshHz = 100;
	store(want, kv);
	EXPECT_EQ(kv["MusicVolume"], "55");
	EXPECT_EQ(kv["Zebra"], "stripes");
	EXPECT_EQ(kv["Resolution"], "800 600");              // legacy key untouched
	EXPECT_EQ(load(kv, false), want);
	store(Settings{true, 0, {}, 0, WindowMode::Windowed, false, 0, false}, kv);
	EXPECT_EQ(kv["UIScale"], "Auto");
	EXPECT_EQ(load(kv, false).uiScalePercent, 0);
}

TEST(DisplaySettings, InvalidValuesFallBackSafely)
{
	KeyValues kv = {{"DisplayVersion", "1"}, {"DisplayMonitor", "-3"}, {"DisplayWidth", "99999"}, {"DisplayHeight", "700"},
	                {"DisplayMode", "Sideways"}, {"UIScale", "7"}, {"VSync", "maybe"}, {"RefreshRate", "-5"}};
	const Settings s = load(kv, false);
	EXPECT_TRUE(s.explicitSettings);
	EXPECT_EQ(s.monitor, 0);
	EXPECT_FALSE(s.resolution.valid());
	EXPECT_EQ(s.mode, WindowMode::Windowed);
	EXPECT_EQ(s.uiScalePercent, 0);
	EXPECT_FALSE(s.vsync);
	EXPECT_EQ(s.refreshHz, 0);
	KeyValues tiny = {{"DisplayVersion", "1"}, {"DisplayWidth", "320"}, {"DisplayHeight", "240"}};
	EXPECT_FALSE(load(tiny, false).resolution.valid());
	KeyValues junk = {{"DisplayVersion", "abc"}, {"DisplayMode", "Borderless"}};
	EXPECT_FALSE(load(junk, false).explicitSettings);
	EXPECT_EQ(load(junk, false).mode, WindowMode::Windowed);
}

TEST(DisplaySettings, KeysAreCaseInsensitive)
{
	KeyValues kv = {{"displayversion", "1"}, {"DISPLAYMODE", "borderless"}, {"vsync", "YES"}};
	const Settings s = load(kv, false);
	EXPECT_EQ(s.mode, WindowMode::Borderless);
	EXPECT_TRUE(s.vsync);
}

TEST(DisplaySettings, AutoWindowedSizesAreOrdinaryForEveryFamily)
{
	auto autoFor = [](int w, int h) {
		Monitor m = makeMonitor(0, w, h, true);
		return autoWindowedSize(m, 16, 39);
	};
	EXPECT_EQ(autoFor(1920, 1080), (Size{1600, 900}));
	EXPECT_EQ(autoFor(2560, 1440), (Size{1920, 1080}));
	EXPECT_EQ(autoFor(3440, 1440), (Size{2560, 1080}));      // 21:9 family
	EXPECT_EQ(autoFor(5120, 1440), (Size{3840, 1080}));      // 32:9 family
	EXPECT_EQ(autoFor(3840, 2160), (Size{2560, 1440}));
	for (Size monitor : {Size{1920, 1080}, Size{2560, 1440}, Size{3440, 1440}, Size{3840, 2160}, Size{5120, 1440},
	                     Size{2560, 1080}, Size{3840, 1080}, Size{1024, 768}, Size{1280, 720}, Size{1920, 1200}, Size{1111, 777}})
	{
		Monitor m = makeMonitor(0, monitor.width, monitor.height, true);
		const Size s = autoWindowedSize(m, 16, 39);
		EXPECT_GE(s.width, 800);
		EXPECT_GE(s.height, 600);
		if (monitor.width >= 1024)
		{
			EXPECT_LE(s.width + 16, m.work.width);
			EXPECT_LE(s.height + 39, m.work.height);
		}
	}
}

TEST(DisplaySettings, ResolveWindowedBorderlessExclusiveAndFallbacks)
{
	Environment e = env1080();
	e.monitors.push_back(makeMonitor(1920, 3440, 1440, false, 100));

	Settings s;
	s.resolution = {1280, 720};
	Resolved r = resolve(s, e);
	EXPECT_EQ(r.mode, WindowMode::Windowed);
	EXPECT_EQ(r.client, (Size{1280, 720}));
	EXPECT_TRUE(r.notes.empty());
	EXPECT_EQ(r.layout.world.width, 1280);

	s.mode = WindowMode::Borderless;
	r = resolve(s, e);
	EXPECT_EQ(r.client, (Size{1920, 1080}));
	EXPECT_EQ(r.refreshHz, 144);

	s.monitor = 1;
	r = resolve(s, e);
	EXPECT_EQ(r.client, (Size{3440, 1440}));
	EXPECT_EQ(r.monitorBounds.x, 1920);
	EXPECT_TRUE(r.layout.isWide());

	s.mode = WindowMode::Exclusive;                   // deferred: reported as borderless
	r = resolve(s, e);
	EXPECT_EQ(r.mode, WindowMode::Borderless);
	EXPECT_FALSE(r.notes.empty());

	s.mode = WindowMode::Windowed;
	s.monitor = 7;                                    // unplugged monitor
	s.resolution = {5120, 1440};                      // does not fit primary
	r = resolve(s, e);
	EXPECT_EQ(r.monitorIndex, 0);
	EXPECT_LE(r.client.width + 16, 1920);
	EXPECT_GE(r.client.width, 800);

	Environment none;
	r = resolve(Settings{}, none);
	EXPECT_TRUE(r.client.valid());
	EXPECT_FALSE(r.notes.empty());
}

TEST(DisplaySettings, UiScaleResolvesThroughTheSameLayoutAtEveryReviewSize)
{
	for (Size size : {Size{800, 600}, Size{1024, 768}, Size{1280, 720}, Size{1920, 1080}, Size{2560, 1440},
	                  Size{2560, 1080}, Size{3440, 1440}, Size{3840, 2160}, Size{3840, 1080}, Size{5120, 1440}, Size{1111, 777}})
	{
		Environment e;
		e.monitors.push_back(makeMonitor(0, size.width, size.height, true));
		Settings s;
		s.mode = WindowMode::Borderless;
		for (int scale : {0, 75, 100, 150, 200})
		{
			s.uiScalePercent = scale;
			const Resolved r = resolve(s, e);
			EXPECT_EQ(r.client, size);
			EXPECT_EQ(r.layout.world.width, size.width);   // the world always spans the whole surface
			EXPECT_LE(r.layout.canvas.right(), size.width);
			EXPECT_LE(r.layout.canvas.bottom(), size.height);
			EXPECT_GE(r.layout.uiScalePercent, 50);
		}
	}
}

TEST(DisplaySettings, SizeChoicesAreRealAndFit)
{
	Monitor m = makeMonitor(0, 3440, 1440, true, 100);
	m.modes = DisplayModes::filterModes({[]{ DisplayModes::Mode x; x.width = 3440; x.height = 1440; x.refreshHz = 100; return x; }(),
	                                     []{ DisplayModes::Mode x; x.width = 2560; x.height = 1080; x.refreshHz = 100; return x; }()});
	const auto windowed = sizeChoices(m, WindowMode::Windowed, 16, 39, {1111, 777});
	ASSERT_FALSE(windowed.empty());
	bool hasCurrent = false, hasWide = false;
	for (const Size &s : windowed)
	{
		EXPECT_LE(s.width + 16, m.work.width);
		EXPECT_LE(s.height + 39, m.work.height);
		hasCurrent |= s == Size{1111, 777};
		hasWide |= s == Size{2560, 1080};
	}
	EXPECT_TRUE(hasCurrent);
	EXPECT_TRUE(hasWide);
	for (size_t i = 1; i < windowed.size(); ++i)
		EXPECT_TRUE(windowed[i - 1].width < windowed[i].width ||
		            (windowed[i - 1].width == windowed[i].width && windowed[i - 1].height < windowed[i].height));
	const auto borderless = sizeChoices(m, WindowMode::Borderless, 16, 39);
	ASSERT_EQ(borderless.size(), 1u);
	EXPECT_EQ(borderless[0], (Size{3440, 1440}));
}

namespace
{
struct ModernScope
{
	DisplayRuntime::State saved;
	explicit ModernScope(DisplayGeometry::Size client, int uiScale = 0) : saved(DisplayRuntime::state())
	{
		DisplayRuntime::State &s = DisplayRuntime::state();
		s.modern = true;
		s.settings = DisplayConfig::Settings{};
		s.settings.uiScalePercent = uiScale;
		DisplayRuntime::setClient(client);
	}
	~ModernScope() { DisplayRuntime::state() = saved; }
};
}

TEST(DisplayRuntime, ClassicKeepsTheOriginalPerAxisRatios)
{
	EXPECT_FALSE(DisplayRuntime::modern());
	EXPECT_DOUBLE_EQ(DisplayRuntime::referenceScaleX(1024), 1024 / 800.0);
	EXPECT_DOUBLE_EQ(DisplayRuntime::referenceScaleY(768), 768 / 600.0);
	EXPECT_EQ(DisplayRuntime::controlBarOrigin().x, 0);
	EXPECT_EQ(DisplayRuntime::controlBarOrigin().y, 0);
	EXPECT_FLOAT_EQ(DisplayRuntime::controlBarMultiplier(1024, 800, 1.28f), 1.28f);
	EXPECT_FALSE(DisplayRuntime::expectedClient().valid());
	EXPECT_DOUBLE_EQ(DisplayRuntime::uiScale(), 1.0);
}

TEST(DisplayRuntime, ModernScalesUniformlyAndAnchorsTheControlBarToTheBottom)
{
	for (Size size : {Size{1920, 1080}, Size{2560, 1080}, Size{3440, 1440}, Size{5120, 1440}, Size{3840, 2160}, Size{1280, 720}})
	{
		ModernScope scope(size);
		const double scale = DisplayRuntime::uiScale();
		EXPECT_DOUBLE_EQ(DisplayRuntime::referenceScaleX(size.width), scale);
		EXPECT_DOUBLE_EQ(DisplayRuntime::referenceScaleY(size.height), scale);   // never anisotropic
		EXPECT_EQ(DisplayRuntime::expectedClient(), size);
		const DisplayGeometry::Point origin = DisplayRuntime::controlBarOrigin();
		const DisplayGeometry::Layout &layout = DisplayRuntime::layout();
		EXPECT_EQ(origin.y + DisplayGeometry::scaled(600, layout.uiScalePercent), size.height);   // bottom edge
		EXPECT_EQ(origin.x, layout.canvas.x);                                                    // centered horizontally
		// 1024x768 control bar art maps onto the same 800-wide reference block.
		EXPECT_NEAR(DisplayRuntime::controlBarMultiplier(size.width, 1024, 0.0f), scale * 800.0 / 1024.0, 1e-5);
	}
}

TEST(DisplayRuntime, HitTestingUsesTheSameTransformAsDrawing)
{
	using DisplayGeometry::Anchor;
	using DisplayGeometry::Point;
	using DisplayGeometry::Rect;
	for (Size size : {Size{800, 600}, Size{1024, 768}, Size{1920, 1080}, Size{3440, 1440}, Size{5120, 1440}, Size{1111, 777}})
	{
		ModernScope scope(size);
		const DisplayGeometry::Layout &layout = DisplayRuntime::layout();
		struct Control { Rect reference; };
		const Control controls[] = {
			{{4, 425, 180, 170}},     // radar
			{{216, 428, 34, 34}},     // a command button
			{{752, 120, 48, 41}},     // shortcut sidebar slot
			{{0, 0, 800, 600}},       // full-screen menu parent
		};
		for (const Control &c : controls)
		{
			const Anchor anchor = DisplayGeometry::inferAnchor(c.reference);
			const Rect physical = DisplayGeometry::referenceToPhysical(layout, c.reference, anchor);
			const Point center = {physical.x + physical.width / 2, physical.y + physical.height / 2};
			EXPECT_TRUE(physical.contains(center));
			// A click on the drawn centre converts back to the centre of the reference rectangle (within one scaled pixel).
			const Point back = DisplayGeometry::physicalToReference(layout, center, anchor);
			EXPECT_NEAR(back.x, c.reference.x + c.reference.width / 2.0, 1.0 + 1.0 / layout.scale());
			EXPECT_NEAR(back.y, c.reference.y + c.reference.height / 2.0, 1.0 + 1.0 / layout.scale());
			// The pixel just outside every edge is outside.
			EXPECT_FALSE(physical.contains({physical.x - 1, center.y}));
			EXPECT_FALSE(physical.contains({physical.right(), center.y}));
		}
		// Edge scrolling works on the physical surface: its extremes are always inside the world viewport.
		EXPECT_TRUE(layout.world.contains({0, 0}));
		EXPECT_TRUE(layout.world.contains({size.width - 1, size.height - 1}));
		EXPECT_FALSE(layout.world.contains({size.width, size.height - 1}));
	}
}

TEST(DisplayRuntime, ShortcutSidebarAndRadarStayOnTheirEdgesOnUltrawide)
{
	using DisplayGeometry::Anchor;
	using DisplayGeometry::Rect;
	ModernScope scope({5120, 1440});
	const DisplayGeometry::Layout &layout = DisplayRuntime::layout();
	const Rect sidebar = DisplayGeometry::referenceToPhysical(layout, Rect{752, 120, 48, 41}, Anchor::Right);
	EXPECT_EQ(sidebar.right(), 5120);                      // hugging the right edge, not drifting to x=800*scale
	const Rect radar = DisplayGeometry::referenceToPhysical(layout, Rect{4, 425, 180, 170}, Anchor::Bottom);
	EXPECT_LT(radar.x, 5120 / 2);
	EXPECT_LE(radar.bottom(), 1440);
	EXPECT_GT(radar.x, 0);                                  // inside the centered canvas, left part of the bar
}

#include "Common/MatchOptionsLayout.h"

TEST(MatchOptionsLayout, RowsAreOnTheGridRhythmAndLeaveTheHonorsBlockAlone)
{
	for (int percent : {75, 100, 125, 150, 175, 200, 250, 300})
	{
		const double s = percent / 100.0;
		const int hx = MatchOptionsLayout::scaled(57, s), hy = MatchOptionsLayout::scaled(315, s);
		const MatchOptionsLayout::Layout l =
			MatchOptionsLayout::compute(hx, hy, MatchOptionsLayout::scaled(156, s), MatchOptionsLayout::scaled(24, s), s);
		const int rowH = MatchOptionsLayout::scaled(24, s);
		for (int i = 0; i < MatchOptionsLayout::kRowCapacity; ++i)
		{
			EXPECT_EQ(l.label[i].h, rowH);
			EXPECT_EQ(l.control[i].y, l.label[i].y);
			EXPECT_LE(l.label[i].right(), l.control[i].x) << percent;
			EXPECT_LE(l.control[i].right(), hx + MatchOptionsLayout::scaled(240, s)) << percent;   // stays in the left column
			if (i)
				EXPECT_EQ(l.label[i].y, l.label[i - 1].bottom());
		}
		// Both rows end exactly where the shifted honors block begins (Zero Hour's 7 px move).
		const int honorsTopAfterShift = l.honorsBlockBefore.y + 1 + l.honorsShift;
		EXPECT_NEAR(l.label[MatchOptionsLayout::kRowCapacity - 1].bottom(), honorsTopAfterShift, 2) << percent;
		// The retired speed block is entirely above the honors block and below the header top.
		EXPECT_GE(l.remnantBlock.y, hy);
		EXPECT_LE(l.remnantBlock.bottom(), hy + MatchOptionsLayout::scaled(69, s)) << percent;   // first honors row (retail y 384)
	}
}

#include "Common/HudComposition.h"
#include "Common/OptionsPageLayout.h"

TEST(HudComposition, CoreIsCentredScaledAndNeverStretched)
{
	using DisplayGeometry::Anchor;
	for (Size size : {Size{800, 600}, Size{1024, 768}, Size{1280, 720}, Size{1920, 1080}, Size{2560, 1440}, Size{2560, 1080},
	                  Size{3440, 1440}, Size{3840, 2160}, Size{3840, 1080}, Size{5120, 1440}, Size{1111, 777}, Size{1777, 999}})
	{
		for (int requested : {0, 100, 150})
		{
			const DisplayGeometry::Layout layout = DisplayGeometry::computeLayout(size, requested);
			const DisplayGeometry::Rect band = DisplayGeometry::referenceToPhysical(layout, DisplayGeometry::Rect{0, 408, 800, 191}, Anchor::Bottom);
			const int backing = DisplayGeometry::scaled(120, layout.uiScalePercent);
			const HudComposition::Composition c = HudComposition::compute(
				band.x, band.y, band.width, band.height, size.width, size.height, 800, 599, 800, 600, backing);
			ASSERT_TRUE(c.qualifies);
			// the core never grows with the surface: exactly 800 reference units x the UI scale
			EXPECT_EQ(c.core.w, DisplayGeometry::scaled(800, layout.uiScalePercent)) << size.width;
			// centred horizontally, touching the bottom edge, never outside the surface
			EXPECT_GE(c.core.x, 0); EXPECT_LE(c.core.right(), size.width);
			EXPECT_LE(c.core.bottom(), size.height); EXPECT_GE(c.core.y, 0);
			EXPECT_LE(std::abs((c.core.x) - (size.width - c.core.right())), 1);
			// backing sits under the core's lower part, inside the core, flush with the bottom edge
			EXPECT_EQ(c.backing.x, c.core.x); EXPECT_EQ(c.backing.w, c.core.w);
			EXPECT_EQ(c.backing.bottom(), size.height);
			EXPECT_GE(c.backing.y, c.core.y);
			// world beside the core, left + core + right tile the width exactly
			EXPECT_EQ(c.worldLeft.w + c.core.w + c.worldRight.w, size.width);
			// regions for timers/notices stay inside the core's width and above it
			EXPECT_EQ(c.timerBlock.x, c.core.x); EXPECT_EQ(c.timerBlock.w, c.core.w);
			EXPECT_LE(c.timerBlock.bottom(), c.core.y);
			EXPECT_LE(c.noticeBlock.bottom(), c.timerBlock.y);
			// input: only the core is an HUD input region; every world pixel beside it is not
			EXPECT_FALSE(c.core.contains(c.core.x - 1, c.core.y + 1));
			EXPECT_FALSE(c.core.contains(c.core.right(), c.core.y + 1));
		}
	}
}

TEST(HudComposition, SmallArtAndTopBannersDoNotQualify)
{
	EXPECT_FALSE(HudComposition::compute(100, 400, 180, 190, 1920, 1080, 180, 599, 800, 600, 120).qualifies);
	EXPECT_FALSE(HudComposition::compute(0, 0, 800, 60, 1920, 1080, 800, 60, 800, 600, 120).qualifies);
}

TEST(HudComposition, HudAnchoredFractionsKeepTheirDistanceFromTheBar)
{
	for (Size size : {Size{1280, 720}, Size{1920, 1080}, Size{2560, 1080}, Size{5120, 1440}})
	{
		ModernScope scope(size);
		const DisplayGeometry::Point origin = DisplayRuntime::controlBarOrigin();
		const double s = DisplayRuntime::uiScale();
		const int x = DisplayRuntime::hudX(0.7, size.width);
		const int y = DisplayRuntime::hudY(0.7, size.height);
		EXPECT_EQ(x, origin.x + static_cast<int>(0.7 * 800 * s));
		EXPECT_EQ(y, origin.y + static_cast<int>(0.7 * 600 * s));
		EXPECT_LT(x, origin.x + static_cast<int>(800 * s));
	}
	EXPECT_EQ(DisplayRuntime::hudX(0.7, 1000), 700);   // Classic: fraction of the surface, as before
}

TEST(OptionsPageLayout, PagesFitAndNeverGrowWithTheSurface)
{
	using namespace OptionsPageLayout;
	for (int percent : {75, 100, 125, 150, 175, 200, 250, 300})
	{
		const double s = percent / 100.0;
		const int ox = 20, oy = 10;
		const Tabs tabs = computeTabs(s, ox, oy);
		// tabs tile the strip with no overlap and stay inside it
		for (int i = 0; i < kCategoryCount; ++i)
		{
			EXPECT_TRUE(tabs.tab[i].inside(tabs.strip)) << percent;
			if (i) EXPECT_FALSE(tabs.tab[i].intersects(tabs.tab[i - 1])) << percent;
			EXPECT_GE(tabs.tab[i].w, scaled(60, s)) << "tab too narrow for its caption at " << percent;
		}
		for (int rows = 1; rows <= 8; ++rows)
		{
			const Page p = computePage(s, rows, ox, oy);
			EXPECT_TRUE(p.fits) << rows << " rows at " << percent;
			EXPECT_GE(p.content.y, tabs.strip.bottom());     // content starts below the tabs
			for (int i = 0; i < p.rowCount; ++i)
			{
				EXPECT_TRUE(p.rows[i].label.inside(p.content));
				EXPECT_TRUE(p.rows[i].control.inside(p.content));
				EXPECT_LE(p.rows[i].label.right(), p.rows[i].control.x);       // columns never overlap
				if (i) EXPECT_GE(p.rows[i].control.y, p.rows[i - 1].control.bottom());
			}
		}
		// the content width is a reference width x scale: it does not depend on any surface
		EXPECT_EQ(computePage(s, 1, 0, 0).content.w, scaled(kContent.w, s));
	}
}
