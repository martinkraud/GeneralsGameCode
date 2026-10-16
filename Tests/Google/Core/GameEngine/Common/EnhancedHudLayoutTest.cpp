/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// Enhanced gameplay HUD (USA prototype): the pure geometry, the persisted switch and the activation rules.

#include <gtest/gtest.h>

#include "Common/DisplayGeometry.h"
#include "Common/DisplaySettings.h"
#include "Common/EnhancedHud.h"
#include "Common/EnhancedHudImageFit.h"
#include "Common/EnhancedHudLayout.h"
#include "Common/OptionsPageLayout.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace EnhancedHudLayout;

namespace
{
struct Surface { int w, h; const char *name; };
const Surface kSurfaces[] = {
	{800, 600, "800x600"},     {1024, 768, "1024x768"},   {1280, 1024, "1280x1024"}, {1366, 768, "1366x768"},
	{1920, 1080, "1920x1080"}, {1920, 1200, "1920x1200"}, {2560, 1440, "2560x1440"}, {3440, 1440, "3440x1440"},
	{5120, 1440, "5120x1440"}, {2000, 777, "2000x777 (odd)"}, {1000, 1000, "1000x1000 (square)"},
};

double layoutScaleFor(int w, int h, int uiScalePercent = DisplayGeometry::kAutoUiScale)
{
	return DisplayGeometry::computeLayout({w, h}, uiScalePercent).scale();
}

Result compute(const Surface &s, int uiScalePercent = DisplayGeometry::kAutoUiScale)
{
	return EnhancedHudLayout::compute(s.w, s.h, layoutScaleFor(s.w, s.h, uiScalePercent));
}
}  // namespace

TEST(EnhancedHudLayout, IslandIsBottomFlushCentredAndInsideTheSurface)
{
	for (const Surface &s : kSurfaces)
	{
		const Result r = compute(s);
		ASSERT_TRUE(r.valid) << s.name;
		EXPECT_EQ(r.island.bottom(), s.h) << s.name;                       // flush: no gap under it
		EXPECT_GE(r.island.x, 0) << s.name;
		EXPECT_LE(r.island.right(), s.w) << s.name;
		EXPECT_LE(std::abs(r.island.x - (s.w - r.island.right())), 1) << s.name;   // horizontally centred
		EXPECT_GT(r.island.w, 0);
		EXPECT_GT(r.island.h, 0);
	}
}

TEST(EnhancedHudLayout, DoesNotStretchAcrossWideSurfaces)
{
	for (const Surface &s : kSurfaces)
	{
		const Result r = compute(s);
		const bool compact = s.w * 1000 <= s.h * kCompactAspectPermille;
		if (!compact)
		{
			EXPECT_LE(r.island.w * 100, s.w * kMaxWidthPercentWide + 100) << s.name;    // world visible on both sides
			EXPECT_GT(r.island.x, 0) << s.name;
			EXPECT_GT(s.w - r.island.right(), 0) << s.name;
		}
	}
	// 21:9 and 32:9 at the same height are bounded to the same physical island as 16:9 at that height
	const Result a = EnhancedHudLayout::compute(2560, 1440, layoutScaleFor(2560, 1440));
	const Result b = EnhancedHudLayout::compute(3440, 1440, layoutScaleFor(3440, 1440));
	const Result c = EnhancedHudLayout::compute(5120, 1440, layoutScaleFor(5120, 1440));
	// 2560 is bounded by the 82 % width cap (the wider surfaces are bounded by the UI scale): never larger
	EXPECT_LE(a.island.w, b.island.w);
	EXPECT_EQ(b.island.w, c.island.w);
	EXPECT_EQ(b.island.h, c.island.h);
	EXPECT_LE(a.island.h, c.island.h);
	EXPECT_LT(c.island.w * 100, 5120 * 50);   // far below half of a 32:9 surface
}

TEST(EnhancedHudLayout, RegionsAndDividersPartitionTheIslandInOrder)
{
	for (const Surface &s : kSurfaces)
	{
		const Result r = compute(s);
		ASSERT_TRUE(r.valid);
		for (const Rect &f : {r.radarFrame, r.infoFrame, r.commandFrame, r.xpFrame})
		{
			EXPECT_FALSE(f.empty()) << s.name;
			EXPECT_TRUE(f.within(r.island)) << s.name;
		}
		EXPECT_LE(r.radarFrame.right(), r.infoFrame.x) << s.name;       // radar | info | command | xp (left to right)
		EXPECT_LE(r.infoFrame.right(), r.commandFrame.x) << s.name;
		EXPECT_LE(r.commandFrame.right(), r.xpFrame.x) << s.name;
		EXPECT_LT(r.dividerX[0], r.dividerX[1]);
		EXPECT_LT(r.dividerX[1], r.dividerX[2]);
	}
}

TEST(EnhancedHudLayout, EveryPlacedWindowIsInsideItsOwnRegion)
{
	for (const Surface &s : kSurfaces)
	{
		const Result r = compute(s);
		ASSERT_TRUE(r.valid);
		auto expectIn = [&](const char *window, const Rect &frame) {
			Rect dst; bool hide = false;
			ASSERT_TRUE(destinationFor(window, dst, hide)) << window;
			ASSERT_FALSE(hide) << window;
			const Rect p = toPhysical(r, dst);
			EXPECT_TRUE(p.within(frame)) << s.name << " " << window;
			EXPECT_TRUE(p.within(r.island)) << s.name << " " << window;
		};
		// radar region
		expectIn("LeftHUD", r.radarFrame);
		expectIn("WinUAttack", r.radarFrame);
		for (const char *w : {"ButtonOptions", "ButtonIdleWorker", "ButtonPlaceBeacon", "PopupCommunicator"})
			expectIn(w, r.radarFrame);
		// info region (selection, queue, upgrades)
		int n = 0;
		for (const GroupWindow *g = infoGroup(n); n-- > 0; ++g)
			expectIn(g->window, r.infoFrame);
		// command region (money/power header, containers, twelve buttons)
		expectIn("MoneyDisplay", r.commandFrame);
		expectIn("PowerWindow", r.commandFrame);
		n = 0;
		for (const GroupWindow *g = commandGroup(n); n-- > 0; ++g)
			expectIn(g->window, r.commandFrame);
		// XP region
		expectIn("ButtonGeneral", r.xpFrame);
		expectIn("ExpBarForeground", r.xpFrame);
		expectIn("GeneralsExp", r.xpFrame);
	}
}

TEST(EnhancedHudLayout, WindowsOfDifferentRegionsDoNotOverlap)
{
	const Result r = compute(kSurfaces[5]);   // 1920x1200
	struct Entry { const char *name; Rect rect; int group; };
	std::vector<Entry> entries;
	auto add = [&](const char *name, int group) {
		Rect dst; bool hide = false;
		ASSERT_TRUE(destinationFor(name, dst, hide));
		entries.push_back({name, toPhysical(r, dst), group});
	};
	add("LeftHUD", 0); add("ButtonOptions", 0); add("ButtonIdleWorker", 0); add("PopupCommunicator", 0); add("ButtonPlaceBeacon", 0);
	add("WinUnitSelected", 1);
	add("MoneyDisplay", 2); add("PowerWindow", 2);
	for (int i = 1; i <= 12; ++i)
	{
		char name[24];
		std::snprintf(name, sizeof(name), "ButtonCommand%02d", i);
		static std::vector<std::string> keep;
		keep.push_back(name);
		add(keep.back().c_str(), 2);
	}
	add("ButtonGeneral", 3); add("ExpBarForeground", 3);
	for (size_t i = 0; i < entries.size(); ++i)
		for (size_t j = i + 1; j < entries.size(); ++j)
			if (entries[i].group != entries[j].group)
				EXPECT_FALSE(entries[i].rect.intersects(entries[j].rect)) << entries[i].name << " vs " << entries[j].name;
	// the twelve command buttons never overlap each other
	for (size_t i = 0; i < entries.size(); ++i)
		for (size_t j = i + 1; j < entries.size(); ++j)
			if (std::strncmp(entries[i].name, "ButtonCommand", 13) == 0 && std::strncmp(entries[j].name, "ButtonCommand", 13) == 0)
				EXPECT_FALSE(entries[i].rect.intersects(entries[j].rect)) << entries[i].name << " vs " << entries[j].name;
}

TEST(EnhancedHudLayout, HitRegionsAreWorldClickableBesideTheIsland)
{
	for (const Surface &s : kSurfaces)
	{
		const Result r = compute(s);
		ASSERT_TRUE(r.valid);
		if (r.island.x > 4)
		{
			EXPECT_FALSE(r.island.contains(r.island.x - 2, s.h - 2)) << s.name;      // left of the island: world
			EXPECT_FALSE(r.island.contains(r.island.right() + 1, s.h - 2)) << s.name;
		}
		EXPECT_TRUE(r.island.contains(s.w / 2, s.h - 1)) << s.name;                   // the island itself takes input
		EXPECT_FALSE(r.island.contains(s.w / 2, r.island.y - 1)) << s.name;            // above: world
	}
}

TEST(EnhancedHudLayout, UiScaleChangesTheIslandWithinBounds)
{
	const int surface = 5;   // 1920x1200
	Result previous;
	for (int percent : {50, 75, 100, 125, 150, 200})
	{
		const Result r = compute(kSurfaces[surface], percent);
		ASSERT_TRUE(r.valid) << percent;
		EXPECT_EQ(r.island.bottom(), 1200) << percent;
		EXPECT_LE(r.island.w * 100, 1920 * kMaxWidthPercentWide + 100) << percent;
		EXPECT_LE(r.island.h * 3, 1200 + 3) << percent;       // never taller than a third of the surface
		if (previous.valid)
			EXPECT_GE(r.island.w, previous.island.w) << percent;   // a larger UI scale never shrinks the island
		previous = r;
	}
	const Result small = compute(kSurfaces[5], 50), large = compute(kSurfaces[5], 150);
	EXPECT_LT(small.island.w, large.island.w);
}

TEST(EnhancedHudLayout, PowerColumnIsBesideTheIslandOrAboveItAtTheRightEdge)
{
	for (const Surface &s : kSurfaces)
	{
		const Result r = compute(s);
		ASSERT_TRUE(r.valid);
		EXPECT_GE(r.powerColumn.x, 0) << s.name;
		EXPECT_LE(r.powerColumn.right(), s.w) << s.name;
		EXPECT_GE(r.powerColumn.y, 0) << s.name;
		EXPECT_LE(r.powerColumn.bottom(), s.h) << s.name;
		EXPECT_FALSE(r.powerColumn.intersects(r.island)) << s.name;
		if (r.powerBesideIsland)
		{
			EXPECT_EQ(r.powerColumn.x, r.island.right()) << s.name;
			EXPECT_EQ(r.powerColumn.bottom(), s.h) << s.name;    // flush with the bottom like the island
		}
		else
		{
			EXPECT_EQ(r.powerColumn.right(), s.w) << s.name;
			EXPECT_LE(r.powerColumn.bottom(), r.island.y) << s.name;
		}
	}
	EXPECT_TRUE(compute(kSurfaces[8]).powerBesideIsland);    // 5120x1440
	EXPECT_FALSE(compute(kSurfaces[0]).powerBesideIsland);   // 800x600: no room beside
}

TEST(EnhancedHudLayout, OnlyUsaIsSupportedAndEverythingElseFallsBackToOriginal)
{
	EXPECT_TRUE(supportsSide("America"));
	EXPECT_TRUE(supportsSide("AmericaAirForceGeneral"));
	EXPECT_FALSE(supportsSide("AmericaSmall"));
	for (const char *side : {"China", "GLA", "ChinaSmall", "GLASmall", "Observer", "", "Russia", "Europe", "ChinaTankGeneral"})
		EXPECT_FALSE(supportsSide(side)) << side;
	EXPECT_FALSE(supportsSide(nullptr));

	// requested + modern + not classic + USA + a surface that fits
	EXPECT_TRUE(shouldBeActive(true, true, false, "America", 1920, 1200));
	EXPECT_FALSE(shouldBeActive(false, true, false, "America", 1920, 1200));    // Original chosen
	EXPECT_FALSE(shouldBeActive(true, false, false, "America", 1920, 1200));    // not the modern display
	EXPECT_FALSE(shouldBeActive(true, true, true, "America", 1920, 1200));      // -classic
	EXPECT_FALSE(shouldBeActive(true, true, false, "China", 1920, 1200));       // unsupported faction
	EXPECT_FALSE(shouldBeActive(true, true, false, "America", 320, 200));       // surface too small
}

TEST(EnhancedHudLayout, RuntimeSwitchFallsBackAndNeverInventsALayout)
{
	EnhancedHud::deactivate();
	EnhancedHud::setRequested(false);
	EXPECT_FALSE(EnhancedHud::update(true, false, "America", 1920, 1200, 2.0));
	EXPECT_FALSE(EnhancedHud::active());
	EnhancedHud::setRequested(true);
	EXPECT_TRUE(EnhancedHud::update(true, false, "America", 1920, 1200, 2.0));
	EXPECT_TRUE(EnhancedHud::active());
	EXPECT_EQ(EnhancedHud::layout().island.bottom(), 1200);
	EXPECT_FALSE(EnhancedHud::update(true, false, "GLA", 1920, 1200, 2.0));    // unsupported: Original, no fake layout
	EXPECT_FALSE(EnhancedHud::active());
	EXPECT_FALSE(EnhancedHud::layout().valid);
	EnhancedHud::setRequested(false);
	EnhancedHud::deactivate();
}

TEST(EnhancedHudLayout, TheInterfaceChoiceIsPersistedAndDefaultsToOriginal)
{
	using namespace DisplayConfig;
	KeyValues kv;
	Settings defaults = load(kv, false);
	EXPECT_FALSE(defaults.enhancedHud);                            // Original by default

	Settings s;
	s.enhancedHud = true;
	KeyValues out;
	store(s, out);
	EXPECT_EQ(out["InterfaceStyle"], "Enhanced");
	KeyValues file = out;
	file["DisplayVersion"] = "1";
	EXPECT_TRUE(load(file, false).enhancedHud);
	EXPECT_FALSE(load(file, true).enhancedHud);                    // Classic ignores stored values entirely

	file["InterfaceStyle"] = "Original";
	EXPECT_FALSE(load(file, false).enhancedHud);
	file["InterfaceStyle"] = "something else";                     // unknown value: Original, never a surprise
	EXPECT_FALSE(load(file, false).enhancedHud);
	file["InterfaceStyle"] = "enhanced";                           // case-insensitive
	EXPECT_TRUE(load(file, false).enhancedHud);

	Settings a, b;
	b.enhancedHud = true;
	EXPECT_FALSE(a == b);
	// other keys in the file are untouched by the round trip
	KeyValues withOthers;
	withOthers["Resolution"] = "800 600";
	withOthers["SomethingElse"] = "x";
	store(s, withOthers);
	EXPECT_EQ(withOthers["Resolution"], "800 600");
	EXPECT_EQ(withOthers["SomethingElse"], "x");
}

TEST(EnhancedHudLayout, TheInterfaceChoiceLivesOnTheGameplayPage)
{
	using namespace OptionsPageLayout;
	EXPECT_EQ(ownerOf("ModernInterface"), static_cast<int>(Category::Gameplay));
	EXPECT_EQ(ownerOf("ModernInterfaceLabel"), static_cast<int>(Category::Gameplay));
	EXPECT_EQ(ownerOf("ModernMotion"), static_cast<int>(Category::Gameplay));
}

TEST(EnhancedHudLayout, WindowTableCoversTheOriginalFunctionalSurface)
{
	// the functionality the prototype must keep: radar, command grid (12), queue (9), upgrades (5), cameo, XP bar and
	// General button, money, power, and the four utility buttons
	auto has = [](const char *name) { Rect r; bool hide = false; return destinationFor(name, r, hide); };
	for (const char *w : {"LeftHUD", "CommandWindow", "ButtonCommand01", "ButtonCommand12", "ButtonQueue01", "ButtonQueue09",
	                      "UnitUpgrade1", "UnitUpgrade5", "CameoWindow", "WinUnitSelected", "GeneralsExp", "ExpBarForeground",
	                      "ButtonGeneral", "MoneyDisplay", "PowerWindow", "ButtonOptions", "ButtonIdleWorker", "ButtonPlaceBeacon",
	                      "PopupCommunicator"})
		EXPECT_TRUE(has(w)) << w;
	Rect r; bool hide = false;
	EXPECT_TRUE(destinationFor("ButtonSmall", r, hide));
	EXPECT_TRUE(hide);                                               // the retail 800 px size toggles have no equivalent
	EXPECT_FALSE(destinationFor("NoSuchWindow", r, hide));
}

// ---- visual/layout refinement (native review 2): cash panel, General/XP region, radar controls, power column ------

namespace
{
Rect physical(const Result &r, const char *window)
{
	Rect dst;
	bool hide = false;
	EXPECT_TRUE(destinationFor(window, dst, hide)) << window;
	return toPhysical(r, dst);
}
}  // namespace

TEST(EnhancedHudLayout, CashHasItsOwnPanelInsideTheCommandFrame)
{
	for (const Surface &s : kSurfaces)
		for (int ui : {DisplayGeometry::kAutoUiScale, 75, 100, 150, 200})
		{
			const Result r = compute(s, ui);
			ASSERT_TRUE(r.valid) << s.name;
			EXPECT_TRUE(r.cashFrame.within(r.commandFrame)) << s.name;
			const Rect money = physical(r, "MoneyDisplay");
			EXPECT_TRUE(money.within(r.cashFrame)) << s.name << " ui" << ui;
			// readable: at least 120 reference units wide and 20 high, never smaller than the retail text window
			EXPECT_GE(kCashFrame.w, 120);
			EXPECT_GE(money.h, 12) << s.name;
			// the cash panel does not touch the power bar or the command grid
			EXPECT_FALSE(r.cashFrame.intersects(physical(r, "PowerWindow"))) << s.name;
			EXPECT_FALSE(r.cashFrame.intersects(physical(r, "ButtonCommand01"))) << s.name;
			EXPECT_FALSE(r.cashFrame.intersects(physical(r, "ButtonCommand11"))) << s.name;
		}
}

TEST(EnhancedHudLayout, ThousandsGroupingIsPresentationOnly)
{
	wchar_t out[32];
	groupThousands(0, out, 32);        EXPECT_STREQ(out, L"0");
	groupThousands(999, out, 32);      EXPECT_STREQ(out, L"999");
	groupThousands(1000, out, 32);     EXPECT_STREQ(out, L"1,000");
	groupThousands(45200, out, 32);    EXPECT_STREQ(out, L"45,200");
	groupThousands(1234567, out, 32);  EXPECT_STREQ(out, L"1,234,567");
	groupThousands(4294967295ull, out, 32);  EXPECT_STREQ(out, L"4,294,967,295");
	wchar_t tiny[4];
	EXPECT_EQ(groupThousands(123456, tiny, 4), 3);   // never overruns the buffer
}

TEST(EnhancedHudLayout, GeneralRegionHasRoomForTheButtonBarAndReadableStars)
{
	for (const Surface &s : kSurfaces)
		for (int ui : {DisplayGeometry::kAutoUiScale, 75, 100, 150, 200})
		{
			const Result r = compute(s, ui);
			ASSERT_TRUE(r.valid) << s.name;
			const Rect general = physical(r, "ButtonGeneral");
			const Rect bar = physical(r, "ExpBarForeground");
			const Rect fill = physical(r, "GeneralsExp");
			EXPECT_TRUE(general.within(r.xpFrame)) << s.name;
			EXPECT_TRUE(bar.within(r.xpFrame)) << s.name;
			EXPECT_TRUE(fill.within(bar)) << s.name;
			EXPECT_FALSE(general.intersects(bar)) << s.name;
			// the retail strip was 21 units wide for the stars AND the bar; the bar alone now has at least that
			EXPECT_GE(kXpFrame.w, 140);
			Rect previous;
			for (int i = 0; i < 4; ++i)
			{
				const Rect slot = toPhysical(r, rankSlot(i));
				const Rect star = toPhysical(r, rankStar(i));
				EXPECT_TRUE(slot.within(r.xpFrame)) << s.name << " slot " << i;
				EXPECT_TRUE(star.within(slot)) << s.name << " star " << i;
				// the star is square (never squeezed) and at a natural size
				EXPECT_LE(std::abs(star.w - star.h), 1) << s.name << " star " << i;
				EXPECT_GE(rankStar(i).w, 16);
				EXPECT_FALSE(slot.intersects(bar)) << s.name;
				EXPECT_FALSE(slot.intersects(general)) << s.name;
				EXPECT_FALSE(slot.intersects(fill)) << s.name;
				if (i > 0)
				{
					EXPECT_FALSE(slot.intersects(previous)) << s.name << " slot " << i;     // plates never overlap
					EXPECT_LT(slot.y, previous.y);                                         // rank 2 at the bottom
				}
				previous = slot;
			}
			EXPECT_EQ(rankSlotCount(5), 4);
			EXPECT_EQ(rankSlotCount(1), 0);
			EXPECT_EQ(rankSlotCount(8), 4);
		}
}

TEST(EnhancedHudLayout, RadarUtilityControlsAreInsideTheRadarFrameAndClearOfTheRadar)
{
	for (const Surface &s : kSurfaces)
		for (int ui : {DisplayGeometry::kAutoUiScale, 75, 100, 150, 200})
		{
			const Result r = compute(s, ui);
			const Rect radar = physical(r, "LeftHUD");
			EXPECT_TRUE(radar.within(r.radarFrame)) << s.name;
			Rect prev;
			bool first = true;
			for (const char *n : {"ButtonOptions", "ButtonIdleWorker", "ButtonPlaceBeacon", "PopupCommunicator"})
			{
				const Rect b = physical(r, n);
				EXPECT_TRUE(b.within(r.radarFrame)) << n << " " << s.name;
				EXPECT_FALSE(b.intersects(radar)) << n << " " << s.name;
				EXPECT_GE(kRadarFrame.w, 200);
				if (!first)
					EXPECT_FALSE(b.intersects(prev)) << n;
				prev = b;
				first = false;
			}
		}
}

TEST(EnhancedHudLayout, SpecialPowerColumnIsAttachedToTheIsland)
{
	for (const Surface &s : kSurfaces)
		for (int ui : {DisplayGeometry::kAutoUiScale, 100, 200})
		{
			const Result r = compute(s, ui);
			ASSERT_TRUE(r.valid);
			if (r.powerBesideIsland)
			{
				EXPECT_EQ(r.powerColumn.x, r.island.right()) << s.name;       // touching the island's right end
				EXPECT_EQ(r.powerColumn.bottom(), r.island.bottom()) << s.name;
			}
			else
			{
				EXPECT_EQ(r.powerColumn.bottom(), r.island.y) << s.name;      // sitting on the island's top edge
				EXPECT_EQ(r.powerColumn.right(), r.island.right()) << s.name; // flush with its right end
			}
			EXPECT_GE(r.powerColumn.x, 0);
		}
}

TEST(EnhancedHudLayout, RebalancedRegionsLeaveNoLargeEmptyCentre)
{
	const Result r = compute(kSurfaces[5]);
	// the command grid fills most of its frame (it was 396/440 wide and 98/164 high before the rebalance)
	const Rect first = physical(r, "ButtonCommand01"), last = physical(r, "ButtonCommand12");
	const double usedW = double(last.right() - first.x) / r.commandFrame.w;
	const double usedH = double(last.bottom() - first.y) / r.commandFrame.h;
	EXPECT_GE(usedW, 0.90);
	EXPECT_GE(usedH, 0.70);
	// the selection block fills most of the info frame
	const Rect info = physical(r, "WinUnitSelected");
	EXPECT_GE(double(info.w) / r.infoFrame.w, 0.90);
	EXPECT_GE(double(info.h) / r.infoFrame.h, 0.70);
}

// ---- image fitting (native review 3): reused retail bitmaps keep their proportions ----------------------------------

namespace
{
struct Source { const char *what; int w, h; };
// native sizes of the retail bitmaps involved (INI MappedImage Coords) and typical cameo/icon shapes
const Source kSources[] = {
	{"SUGeneral 3 stars", 65, 29}, {"SUUAttackI warning", 22, 24}, {"SUExpBar strip", 21, 107}, {"command cameo", 64, 48},
	{"power icon", 45, 38}, {"unit cameo", 91, 69}, {"square star", 18, 18}, {"wide gauge cap", 100, 10}, {"1x1", 1, 1},
};
}  // namespace

TEST(EnhancedHudImageFit, ContainPreservesAspectStaysInsideAndIsCentred)
{
	using namespace EnhancedHudImageFit;
	for (const Source &src : kSources)
		for (Rect area : {Rect{10, 20, 67, 49}, Rect{0, 0, 86, 38}, Rect{5, 5, 63, 54}, Rect{100, 100, 300, 20}, Rect{3, 4, 20, 200}})
		{
			const Rect r = containFit(area, src.w, src.h);
			ASSERT_FALSE(r.empty()) << src.what;
			EXPECT_TRUE(r.within(area)) << src.what;
			// aspect preserved up to the one-pixel rounding of each side
			EXPECT_NEAR(double(r.w) / r.h, double(src.w) / src.h, (1.0 / std::min(r.w, r.h)) * (double(src.w) / src.h + 1.0) + 1e-9) << src.what;
			// touches the limiting side and is centred on the other (within rounding)
			EXPECT_TRUE(r.w == area.w || r.h == area.h) << src.what;
			EXPECT_LE(std::abs((r.x - area.x) - (area.right() - r.right())), 1) << src.what;
			EXPECT_LE(std::abs((r.y - area.y) - (area.bottom() - r.bottom())), 1) << src.what;
		}
}

TEST(EnhancedHudImageFit, SquareAssetsStaySquare)
{
	using namespace EnhancedHudImageFit;
	for (Rect area : {Rect{0, 0, 67, 49}, Rect{0, 0, 20, 200}, Rect{7, 9, 300, 40}, Rect{0, 0, 17, 17}})
	{
		const Rect r = containFit(area, 18, 18);
		EXPECT_EQ(r.w, r.h);
		EXPECT_TRUE(r.within(area));
	}
}

TEST(EnhancedHudImageFit, NativeSizeIsOneToOneOrAWholeMultiple)
{
	using namespace EnhancedHudImageFit;
	const Rect big = nativeFit({0, 0, 100, 60}, 22, 24);       // 100/22 = 4.5, 60/24 = 2.5 -> x2
	EXPECT_EQ(big.w, 44);
	EXPECT_EQ(big.h, 48);
	EXPECT_EQ(big.x, 28);
	EXPECT_EQ(big.y, 6);
	const Rect exact = nativeFit({5, 5, 22, 24}, 22, 24);
	EXPECT_EQ(exact.w, 22);
	EXPECT_EQ(exact.h, 24);
	const Rect shrink = nativeFit({0, 0, 11, 12}, 22, 24);      // larger than the area: uniform shrink, never distorted
	EXPECT_EQ(shrink.w * 24, shrink.h * 22);
	EXPECT_TRUE(shrink.within({0, 0, 11, 12}));
	EXPECT_TRUE(containFit({0, 0, 0, 10}, 5, 5).empty());      // degenerate input
	EXPECT_TRUE(containFit({0, 0, 10, 10}, 0, 5).empty());
}

TEST(EnhancedHudImageFit, GaugeTilesScaleUniformlyFromTheCrossExtent)
{
	using namespace EnhancedHudImageFit;
	EXPECT_DOUBLE_EQ(gaugeScale(24, 12), 2.0);
	EXPECT_DOUBLE_EQ(gaugeScale(0, 12), 1.0);        // degenerate: Original behaviour
	EXPECT_DOUBLE_EQ(gaugeScale(10, 0), 1.0);
}

TEST(EnhancedHudImageFit, EveryPlacedWindowIsClassified)
{
	using namespace EnhancedHudImageFit;
	int n = 0;
	for (const NamedPlacement *p = placements(n); n-- > 0; ++p)
		EXPECT_NE(find(p->window), nullptr) << p->window << " has no image-fit class";
	for (const GroupWindow *g = infoGroup(n); n-- > 0; ++g)
		EXPECT_NE(find(g->window), nullptr) << g->window;
	for (const GroupWindow *g = commandGroup(n); n-- > 0; ++g)
		EXPECT_NE(find(g->window), nullptr) << g->window;
	// classes the human review named
	EXPECT_EQ(find("ButtonGeneral")->cls, Class::AspectFit);
	EXPECT_EQ(find("WinUAttack")->cls, Class::Procedural);
	EXPECT_EQ(find("ExpBarForeground")->cls, Class::Procedural);
	EXPECT_EQ(find("GeneralsExp")->cls, Class::Gauge);
	EXPECT_EQ(find("PowerWindow")->cls, Class::Gauge);
	EXPECT_EQ(find("LeftHUD")->cls, Class::SelfFitting);
	EXPECT_EQ(find("ButtonCommand07")->cls, Class::AspectFit);
	EXPECT_EQ(find("Power:ButtonCommand")->cls, Class::AspectFit);
	// every non-container class documents why (nothing is silently exempt)
	n = 0;
	for (const Policy *p = policies(n); n-- > 0; ++p)
		EXPECT_GT(std::strlen(p->why), 3u) << p->window;
}

TEST(EnhancedHudImageFit, GeneralButtonPlateMatchesItsThreeStarBitmap)
{
	using namespace EnhancedHudImageFit;
	for (const Surface &s : kSurfaces)
		for (int ui : {DisplayGeometry::kAutoUiScale, 75, 100, 150, 200})
		{
			const Result r = compute(s, ui);
			const Rect plate = physical(r, "ButtonGeneral");
			EXPECT_TRUE(plate.within(r.xpFrame)) << s.name;
			const Rect img = fit(Class::AspectFit, plate, 65, 29);
			EXPECT_TRUE(img.within(plate)) << s.name;
			// the plate is almost the bitmap's own shape: at most ~12 % of its height is letterbox, the stars are not
			// squeezed into a tall box
			EXPECT_LE(plate.h - img.h, plate.h / 8 + 2) << s.name << " ui" << ui;
			EXPECT_NEAR(double(img.w) / img.h, 65.0 / 29.0, 0.12) << s.name;
		}
}

TEST(EnhancedHudImageFit, ButtonIconsStayInsideTheirFullHitPlates)
{
	using namespace EnhancedHudImageFit;
	for (const Surface &s : kSurfaces)
	{
		const Result r = compute(s);
		int n = 0;
		for (const GroupWindow *g = commandGroup(n); n-- > 0; ++g)
		{
			if (std::strncmp(g->window, "ButtonCommand", 13) != 0)
				continue;
			const Rect plate = physical(r, g->window);
			for (const Source &src : kSources)
			{
				const Rect img = fit(Class::AspectFit, plate, src.w, src.h);
				EXPECT_TRUE(img.within(plate)) << g->window << " " << src.what;
				// the interactive rectangle is still the whole plate; the icon never leaves it
				EXPECT_TRUE(plate.contains(img.x + img.w / 2, img.y + img.h / 2));
			}
		}
	}
}

TEST(EnhancedHudLayout, AttackWarningLampIsInsideTheRadarFrameClearOfRadarAndButtons)
{
	for (const Surface &s : kSurfaces)
		for (int ui : {DisplayGeometry::kAutoUiScale, 75, 100, 150, 200})
		{
			const Result r = compute(s, ui);
			const Rect lamp = physical(r, "WinUAttack");
			EXPECT_TRUE(lamp.within(r.radarFrame)) << s.name;
			EXPECT_FALSE(lamp.intersects(physical(r, "LeftHUD"))) << s.name;
			for (const char *n : {"ButtonOptions", "ButtonIdleWorker", "ButtonPlaceBeacon", "PopupCommunicator"})
				EXPECT_FALSE(lamp.intersects(physical(r, n))) << n << " " << s.name;
			// big enough to read at a glance: at least 30 reference units wide
			EXPECT_GE(kRadarFrame.w, 200);
			EXPECT_GE(lamp.w * 100, int(30 * r.scale * 100 * 0.95)) << s.name;
		}
}
