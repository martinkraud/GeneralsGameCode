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

using namespace DisplaySettings;
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
