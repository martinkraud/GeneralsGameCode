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
#include "Common/EnhancedHudTheme.h"
#include "Common/EnhancedHudLayout.h"
#include "Common/EnhancedHudPanels.h"
#include "Common/OptionsPageLayout.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

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

TEST(EnhancedHudLayout, GlobalPowersAreAnIndependentSurfaceThatNeverTouchesTheIsland)
{
	for (const Surface &s : kSurfaces)
		for (int ui : {DisplayGeometry::kAutoUiScale, 75, 100, 150, 200})
		{
			const Result r = compute(s, ui);
			ASSERT_TRUE(r.valid) << s.name;
			// fully on the surface, frame included
			EXPECT_GE(r.powerFrame.x, 0) << s.name;
			EXPECT_GE(r.powerFrame.y, 0) << s.name;
			EXPECT_LE(r.powerFrame.right(), s.w) << s.name;
			EXPECT_LE(r.powerFrame.bottom(), s.h) << s.name;
			// clear space to the island (never touching), in both directions
			const int gap = scaledRef(kPowerGap, r.scale);
			EXPECT_FALSE(r.powerFrame.intersects(r.island)) << s.name;
			const bool clearX = r.powerFrame.x >= r.island.right() + gap || r.powerFrame.right() <= r.island.x - gap;
			const bool clearY = r.powerFrame.bottom() <= r.island.y - gap;
			EXPECT_TRUE(clearX || clearY) << s.name << " ui" << ui;
			// never bottom-flush like the island: it floats in the battlefield
			EXPECT_LT(r.powerFrame.bottom(), s.h) << s.name;
			// shape: five buttons in a column, or (compact surfaces) in a row
			if (r.powerHorizontal)
				EXPECT_GT(r.powerPanel.w, r.powerPanel.h) << s.name;
			else
				EXPECT_GT(r.powerPanel.h, r.powerPanel.w) << s.name;
		}
}

TEST(EnhancedHudLayout, GlobalPowersAreMountedFlushAgainstTheRightScreenEdge)
{
	// human preference: edge anchored on every non-compact surface, 21:9 and 32:9 included (trade-off: on extreme ultrawide
	// the powers are physically far from the centre of the action)
	for (const Surface &s : kSurfaces)
		for (int ui : {DisplayGeometry::kAutoUiScale, 75, 100, 150, 200})
		{
			const Result r = compute(s, ui);
			const bool compact = s.w * 1000 <= s.h * kCompactAspectPermille;
			if (compact)
				continue;
			ASSERT_FALSE(r.powerHorizontal) << s.name;
			EXPECT_EQ(r.powerFrame.right(), s.w) << s.name << " ui" << ui;            // the extent ends at the edge
			EXPECT_EQ(r.powerEdgeRight, s.w) << s.name;
			// the plates themselves end within a few pixels of the edge: "attached to the screen edge"
			const int m = std::max(2, int(std::lround(3 * r.scale)));
			const int plateRight = r.powerPanel.x + scaledRef(kPowerButtonX, r.scale) + scaledRef(kPowerButtonW, r.scale) + m;
			EXPECT_LE(std::abs(s.w - plateRight), 1) << s.name << " ui" << ui;               // nominal numbers agree within rounding
			EXPECT_LE(plateRight, s.w) << s.name;
		}
	// 32:9 keeps the edge anchor (no safe-region rule): far right of a 5120 px surface
	const Result wide = compute(kSurfaces[8]);
	EXPECT_EQ(wide.powerFrame.right(), 5120);
	EXPECT_FALSE(wide.powerFrame.intersects(wide.island));
}

TEST(EnhancedHudLayout, GlobalPowersUseARowOnCompactSurfacesAndAColumnOtherwise)
{
	for (const Surface &s : kSurfaces)
	{
		const bool compact = s.w * 1000 <= s.h * kCompactAspectPermille;
		const Result r = compute(s);
		EXPECT_EQ(r.powerHorizontal, compact) << s.name;
		if (compact)
		{
			// the row sits on top of the island, aligned to its right end, never wider than the island
			EXPECT_LE(r.powerFrame.bottom(), r.island.y) << s.name;
			EXPECT_LE(r.powerFrame.right(), r.island.right()) << s.name;
			EXPECT_GE(r.powerFrame.x, r.island.x) << s.name;
			EXPECT_EQ(r.powerPanel.w, kPowerCount * scaledRef(kPowerColumnW, r.scale)) << s.name;
		}
	}
	// determinism
	const Result a = compute(kSurfaces[0]), b = compute(kSurfaces[0]);
	EXPECT_EQ(a.powerPanel.x, b.powerPanel.x);
	EXPECT_EQ(a.powerPanel.y, b.powerPanel.y);
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
			EXPECT_GE(kXpFrame.w, 140);
			for (int i = 0; i < 4; ++i)
			{
				const Rect slot = toPhysical(r, rankSlot(i));
				const Rect star = toPhysical(r, rankStar(i));
				EXPECT_TRUE(slot.within(r.xpFrame)) << s.name << " slot " << i;
				EXPECT_TRUE(star.within(slot)) << s.name << " star " << i;
				// the star is square (never squeezed) and at a natural size, centred in its cell
				EXPECT_LE(std::abs(star.w - star.h), 1) << s.name << " star " << i;
				EXPECT_GE(rankStar(i).w, 24);
				EXPECT_LE(std::abs((star.x - slot.x) - (slot.right() - star.right())), 1);
				EXPECT_LE(std::abs((star.y - slot.y) - (slot.bottom() - star.bottom())), 1);
				EXPECT_FALSE(slot.intersects(bar)) << s.name;
				EXPECT_FALSE(slot.intersects(general)) << s.name;
				EXPECT_FALSE(slot.intersects(fill)) << s.name;
				for (int j = 0; j < i; ++j)
					EXPECT_FALSE(slot.intersects(toPhysical(r, rankSlot(j)))) << s.name << " cells " << i << "," << j;
			}
			// one module: the General button is above the rank grid, the XP trough is to the left of both
			EXPECT_LT(general.bottom(), toPhysical(r, rankSlot(0)).y) << s.name;
			EXPECT_LE(bar.right(), general.x) << s.name;
			EXPECT_LE(bar.right(), toPhysical(r, rankSlot(0)).x) << s.name;
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

TEST(EnhancedHudLayout, CommandBayHasTwelveFixedSlotsInsideTheCommandFrame)
{
	for (const Surface &s : kSurfaces)
		for (int ui : {DisplayGeometry::kAutoUiScale, 75, 100, 150, 200})
		{
			const Result r = compute(s, ui);
			Rect cells[12];
			for (int i = 0; i < 12; ++i)
			{
				char name[24];
				std::snprintf(name, sizeof(name), "ButtonCommand%02d", i + 1);
				cells[i] = physical(r, name);
				EXPECT_TRUE(cells[i].within(r.commandFrame)) << name << " " << s.name;
				EXPECT_FALSE(cells[i].intersects(r.cashFrame)) << name << " " << s.name;
				for (int j = 0; j < i; ++j)
					EXPECT_FALSE(cells[i].intersects(cells[j])) << i << "," << j << " " << s.name;
			}
			// the slots keep the retail indices: 01/02 = first column top/bottom, 03/04 = second column, ...
			for (int col = 0; col < 6; ++col)
			{
				EXPECT_EQ(cells[col * 2].x, cells[col * 2 + 1].x) << s.name;
				EXPECT_LT(cells[col * 2].y, cells[col * 2 + 1].y) << s.name;
				if (col > 0)
					EXPECT_LT(cells[(col - 1) * 2].x, cells[col * 2].x) << s.name;
			}
			// the whole bay sits below the resource header, inside the frame, with a visible margin all round
			const int pad = std::max(2, int(2 * r.scale));
			EXPECT_GE(cells[0].y - r.cashFrame.bottom(), pad) << s.name;
		}
}

TEST(EnhancedHudLayout, InfoModuleBlockIsInsideItsFrameWithRoomForTheWell)
{
	for (const Surface &s : kSurfaces)
	{
		const Result r = compute(s);
		const Rect block = physical(r, "WinUnitSelected");
		EXPECT_TRUE(block.within(r.infoFrame)) << s.name;
		const int g = std::max(1, int(4 * r.scale + 0.5) / 2);
		EXPECT_GE(block.x - r.infoFrame.x, g) << s.name;
		EXPECT_GE(block.y - r.infoFrame.y, g) << s.name;
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

TEST(EnhancedHudLayout, TranslatedIslandKeepsItsInternalGeometryAndLeavesGlobalPowersAlone)
{
	// the control bar slides on hide/show: the frame is drawn at the live master position
	for (const Surface &s : kSurfaces)
	{
		const Result r = compute(s);
		const Result lowered = translated(r, 0, r.island.h);       // fully below the bottom edge (hidden stage)
		EXPECT_EQ(lowered.island.y, r.island.y + r.island.h) << s.name;
		EXPECT_EQ(lowered.island.x, r.island.x) << s.name;
		EXPECT_EQ(lowered.commandFrame.y - lowered.island.y, r.commandFrame.y - r.island.y) << s.name;
		EXPECT_EQ(lowered.radarFrame.x - lowered.island.x, r.radarFrame.x - r.island.x) << s.name;
		// mapped reference rectangles follow the island
		const Rect a = toPhysical(r, {409, 44, 53, 45}), b = toPhysical(lowered, {409, 44, 53, 45});
		EXPECT_EQ(b.y - a.y, r.island.h) << s.name;
		EXPECT_EQ(b.x, a.x) << s.name;
		// the Global Powers surface is independent of the island's own movement
		EXPECT_EQ(lowered.powerPanel.x, r.powerPanel.x) << s.name;
		EXPECT_EQ(lowered.powerPanel.y, r.powerPanel.y) << s.name;
		const Result same = translated(r, 0, 0);
		EXPECT_EQ(same.island.y, r.island.y);
	}
}

// ---- faction theme layer (refine 4) ---------------------------------------------------------------------------------

namespace
{
double luminance(const EnhancedHudTheme::Rgba &c) { return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b; }
}  // namespace

TEST(EnhancedHudTheme, FactionsAreRecognisedAndOnlyUsaIsImplemented)
{
	using namespace EnhancedHudTheme;
	EXPECT_EQ(factionOfSide("America"), Faction::Usa);
	EXPECT_EQ(factionOfSide("AmericaAirForceGeneral"), Faction::Usa);
	EXPECT_EQ(factionOfSide("China"), Faction::China);
	EXPECT_EQ(factionOfSide("ChinaTankGeneral"), Faction::China);
	EXPECT_EQ(factionOfSide("GLA"), Faction::Gla);
	EXPECT_EQ(factionOfSide("GLAToxinGeneral"), Faction::Gla);
	EXPECT_EQ(factionOfSide("Europe"), Faction::Europe);     // reserved: not a retail side
	EXPECT_EQ(factionOfSide("Russia"), Faction::Russia);
	EXPECT_EQ(factionOfSide("Observer"), Faction::Unknown);
	EXPECT_EQ(factionOfSide(nullptr), Faction::Unknown);
	EXPECT_TRUE(implemented(Faction::Usa));
	for (Faction f : {Faction::China, Faction::Gla, Faction::Europe, Faction::Russia, Faction::Unknown})
		EXPECT_FALSE(implemented(f));
	// unimplemented factions never get an Enhanced HUD (Original is used), whatever they are called
	for (const char *side : {"China", "ChinaInfantryGeneral", "GLA", "GLAStealthGeneral", "Europe", "Russia", "Observer"})
		EXPECT_FALSE(supportsSide(side)) << side;
	EXPECT_TRUE(supportsSide("America"));
	EXPECT_FALSE(supportsSide("AmericaSmall"));
}

TEST(EnhancedHudTheme, ThemeIsPresentationDataOnly)
{
	using namespace EnhancedHudTheme;
	// plain data: copyable, standard layout (nothing a faction could use to change gameplay)
	static_assert(std::is_trivially_copyable<Theme>::value, "theme must be plain data");
	static_assert(std::is_standard_layout<Theme>::value, "theme must be plain data");
	for (Faction f : {Faction::Usa, Faction::China, Faction::Gla, Faction::Europe, Faction::Russia, Faction::Unknown})
		EXPECT_STREQ(themeFor(f).name, "USA");           // safe default until a faction implements its own theme
	EXPECT_STREQ(usa().name, "USA");
}

TEST(EnhancedHudTheme, UsaThemeKeepsAReadableHierarchy)
{
	const EnhancedHudTheme::Theme &t = EnhancedHudTheme::usa();
	// depth hierarchy (darkest last): control (raised) > chassis > module (recessed) > module header > well
	EXPECT_LT(luminance(t.plate), 70.0);                       // still a dark console
	EXPECT_LT(luminance(t.well), luminance(t.moduleLow));
	EXPECT_LT(luminance(t.moduleLow), luminance(t.module));
	EXPECT_LT(luminance(t.module), luminance(t.plate));        // modules are recessed into the chassis
	EXPECT_GT(luminance(t.control), luminance(t.plate));       // interactive plates are raised above it
	EXPECT_GT(luminance(t.control), luminance(t.controlLow));
	// content is readable on every surface
	for (const EnhancedHudTheme::Rgba *surface : {&t.plate, &t.module, &t.moduleLow, &t.well, &t.control})
	{
		EXPECT_GT(luminance(t.icon) - luminance(*surface), 100.0);
		EXPECT_GT(luminance(t.rankLit) - luminance(*surface), 100.0);
	}
	// the warning is the strongest colour of the theme, the accent is restrained blue
	EXPECT_GT(t.warning.r, 200);
	EXPECT_LT(t.warning.g, 60);
	EXPECT_GT(t.warning.r, t.accent.r + 100);
	EXPECT_GT(t.accent.b, t.accent.r);
	EXPECT_LT(luminance(t.accentDim), luminance(t.accent));
	EXPECT_LT(luminance(t.warningDark), luminance(t.warning));
	EXPECT_TRUE(t.bevelModules && t.brushedChassis && t.junctionRibs && t.moduleLeds);
}

TEST(EnhancedHudTheme, LiveStateFollowsTheSideAndNeverChangesLayout)
{
	EnhancedHud::setRequested(true);
	ASSERT_TRUE(EnhancedHud::update(true, false, "America", 1920, 1200, 2.0));
	EXPECT_EQ(EnhancedHud::faction(), EnhancedHudTheme::Faction::Usa);
	const Result usaLayout = EnhancedHud::layout();
	// the layout is faction independent: the same surface and scale give the same geometry
	const Result again = EnhancedHudLayout::compute(1920, 1200, 2.0);
	EXPECT_EQ(usaLayout.island.x, again.island.x);
	EXPECT_EQ(usaLayout.island.w, again.island.w);
	EXPECT_EQ(usaLayout.powerPanel.x, again.powerPanel.x);
	EXPECT_FALSE(EnhancedHud::update(true, false, "China", 1920, 1200, 2.0));
	EXPECT_EQ(EnhancedHud::faction(), EnhancedHudTheme::Faction::China);
	EXPECT_FALSE(EnhancedHud::active());
	EnhancedHud::setRequested(false);
	EnhancedHud::update(true, false, "America", 1920, 1200, 2.0);
}

// ---- radar content geometry -----------------------------------------------------------------------------------------

namespace
{
// Same arithmetic as Radar::findDrawPositions (the radar keeps the map aspect inside its window).
void radarDrawArea(int width, int height, double mapW, double mapH, int &ulx, int &uly, int &lrx, int &lry)
{
	const double ratioW = mapW / width, ratioH = mapH / height;
	if (ratioW >= ratioH)
	{
		const double ry = mapH / ratioW;
		ulx = 0;
		uly = static_cast<int>((height - ry) / 2.0);
		lrx = static_cast<int>(mapW / ratioW);
		lry = height - uly;
	}
	else
	{
		const double rx = mapW / ratioH;
		ulx = static_cast<int>((width - rx) / 2.0);
		uly = 0;
		lrx = width - ulx;
		lry = static_cast<int>(mapH / ratioH);
	}
}
const double kMapShapes[][2] = {{1024, 1024}, {2048, 1024}, {1024, 2048}, {1536, 1152}, {600, 1000}, {4096, 800}};
}  // namespace

TEST(EnhancedHudLayout, RetailRadarWindowShowsBlackBarsForMapsThatAreNotItsShape)
{
	// root cause of the black side bars: the retail radar window is 167 x 152 (1.10:1); W3DRadar::draw keeps the map aspect
	// and paints the rest of the window black. A square map therefore leaves bars left and right.
	const Result r = compute(kSurfaces[5]);
	const Rect slot = physical(r, "LeftHUD");
	int ulx, uly, lrx, lry;
	radarDrawArea(slot.w, slot.h, 1024, 1024, ulx, uly, lrx, lry);
	EXPECT_GT(ulx, 4) << "left bar width";
	EXPECT_GT(slot.w - lrx, 4) << "right bar width";
	EXPECT_EQ(uly, 0);
}

TEST(EnhancedHudLayout, FittedRadarWindowFillsItselfForEveryMapShapeWithoutDistortion)
{
	using namespace EnhancedHudImageFit;
	for (const Surface &s : kSurfaces)
		for (int ui : {DisplayGeometry::kAutoUiScale, 75, 100, 150, 200})
		{
			const Result r = compute(s, ui);
			const Rect slot = physical(r, "LeftHUD");
			for (const auto &shape : kMapShapes)
			{
				const Rect f = containFit(slot, static_cast<int>(shape[0]), static_cast<int>(shape[1]));
				ASSERT_FALSE(f.empty());
				EXPECT_TRUE(f.within(slot)) << s.name;
				// the map keeps its aspect (to the rounding of one pixel per side)
				EXPECT_NEAR(double(f.w) / f.h, shape[0] / shape[1], (1.0 / std::min(f.w, f.h)) * (shape[0] / shape[1] + 1.0) + 1e-9);
				// centred in the module's radar slot, touching it on the limiting side
				EXPECT_LE(std::abs((f.x - slot.x) - (slot.right() - f.right())), 1);
				EXPECT_LE(std::abs((f.y - slot.y) - (slot.bottom() - f.bottom())), 1);
				EXPECT_TRUE(f.w == slot.w || f.h == slot.h);
				// the radar's own draw logic inside the fitted window leaves no black bar (at most rounding)
				int ulx, uly, lrx, lry;
				radarDrawArea(f.w, f.h, shape[0], shape[1], ulx, uly, lrx, lry);
				EXPECT_LE(ulx, 1) << s.name << " " << shape[0] << "x" << shape[1];
				EXPECT_LE(uly, 1) << s.name;
				EXPECT_LE(f.w - lrx, 2) << s.name;
				EXPECT_LE(f.h - lry, 2) << s.name;
			}
		}
}

TEST(EnhancedHudLayout, PowerPlateIsTheButtonPlusAnEvenMargin)
{
	for (double scale : {0.5, 0.87, 1.0, 1.57, 2.25})
	{
		const Rect button = {100, 200, 45, 38};
		const Rect plate = powerPlateRect(button, scale);
		EXPECT_TRUE(button.within(plate));
		const int m = button.x - plate.x;
		EXPECT_GE(m, 2);
		EXPECT_EQ(button.y - plate.y, m);
		EXPECT_EQ(plate.right() - button.right(), m);
		EXPECT_EQ(plate.bottom() - button.bottom(), m);
	}
}

TEST(EnhancedHudLayout, AcceptedMajorGeometryIsFrozen)
{
	// the refine4 framework: any change to these numbers is a layout redesign and must be a deliberate decision
	EXPECT_EQ(kIslandW, 1000);
	EXPECT_EQ(kIslandH, 180);
	EXPECT_EQ(kRadarFrame.x, 8);    EXPECT_EQ(kRadarFrame.w, 216);
	EXPECT_EQ(kInfoFrame.x, 230);   EXPECT_EQ(kInfoFrame.w, 165);
	EXPECT_EQ(kCommandFrame.x, 401); EXPECT_EQ(kCommandFrame.w, 444);
	EXPECT_EQ(kXpFrame.x, 851);     EXPECT_EQ(kXpFrame.w, 141);
	EXPECT_EQ(kCashFrame.x, 409);   EXPECT_EQ(kCashFrame.w, 152);
	EXPECT_EQ(kCommandScalePermille, 1200);
	EXPECT_EQ(kInfoScalePermille, 1100);
	EXPECT_EQ(kMaxWidthPercentWide, 82);
	EXPECT_EQ(kCompactAspectPermille, 1450);
	EXPECT_EQ(kRankGridX, 898);
	EXPECT_EQ(kRankCell, 42);
	EXPECT_EQ(rankSlotCount(5), 4);
	// slot count and ordering of the command bay
	Rect first, last;
	bool hide = false;
	ASSERT_TRUE(destinationFor("ButtonCommand01", first, hide));
	ASSERT_TRUE(destinationFor("ButtonCommand12", last, hide));
	EXPECT_EQ(first.x, 409);
	EXPECT_EQ(first.y, 44);
	EXPECT_GT(last.x, first.x);
	EXPECT_GT(last.y, first.y);
	int n = 0;
	int buttons = 0;
	for (const GroupWindow *g = commandGroup(n); n-- > 0; ++g)
		if (std::strncmp(g->window, "ButtonCommand", 13) == 0)
			++buttons;
	EXPECT_EQ(buttons, 12);
	// physical island on the reference surfaces (centred, bottom flush, bounded)
	struct Case { int w, h, islandW, islandH; } cases[] = {{1920, 1200, 1574, 283}, {3440, 1440, 2250, 405}, {5120, 1440, 2250, 405}};
	for (const Case &k : cases)
	{
		const Result r = EnhancedHudLayout::compute(k.w, k.h, layoutScaleFor(k.w, k.h, DisplayGeometry::kAutoUiScale));
		EXPECT_EQ(r.island.w, k.islandW) << k.w << "x" << k.h;
		EXPECT_EQ(r.island.h, k.islandH) << k.w << "x" << k.h;
		EXPECT_EQ(r.island.bottom(), k.h);
		EXPECT_EQ(r.island.x, (k.w - r.island.w) / 2);
	}
}

TEST(EnhancedHudTheme, ThemeCanExpressShapeAndMaterialDifferencesBeyondColour)
{
	using namespace EnhancedHudTheme;
	Theme recolour = usa();
	recolour.accent = {200, 40, 40, 255};
	recolour.module = {60, 20, 20, 255};
	EXPECT_FALSE(differsBeyondColour(usa(), recolour));        // a pure recolour is detected as such
	Theme heavy = recolour;
	heavy.shellCorner = Corner::Square;
	heavy.moduleCornerRef = 0;
	heavy.moduleCorner = Corner::Square;
	heavy.fasteners = true;
	heavy.seams = false;
	heavy.bezelRef = 7;
	heavy.wellDepthRef = 4;
	EXPECT_TRUE(differsBeyondColour(usa(), heavy));            // a different construction language is
	// the USA language itself: machined corners, fasteners, seams, vents, ticks, state rails, broken accent rail
	const Theme &t = usa();
	EXPECT_EQ(t.shellCorner, Corner::Chamfer);
	EXPECT_EQ(t.moduleCorner, Corner::Chamfer);
	EXPECT_EQ(t.plateCorner, Corner::Chamfer);
	EXPECT_TRUE(t.fasteners && t.seams && t.tickMarks && t.stateRails);
	EXPECT_TRUE(t.vents);                           // grilles only on the end cap and two module headers (see renderer)
	EXPECT_GT(t.bezelRef, t.wellDepthRef);        // the radar bezel is a stronger surround than an ordinary well
	// shape parameters are small, bounded reference units (a theme cannot reshape the layout)
	EXPECT_LE(t.shellCornerRef, 16);
	EXPECT_LE(t.moduleCornerRef, 8);
	EXPECT_LE(t.plateCornerRef, 6);
	EXPECT_LE(t.bezelRef, 8);
}

// ---- General milestones, popup and Battle Overview models (refine 7) ----------------------------------------------------

TEST(EnhancedHudPanels, FourCellsAreRankMilestonesWithRealMeaning)
{
	using namespace EnhancedHudPanels;
	// Generals: 5 rank levels -> milestones at ranks 2, 3, 4, 5
	EXPECT_EQ(milestoneCount(5), 4);
	for (int i = 0; i < 4; ++i)
		EXPECT_EQ(milestoneRank(5, i, 4), i + 2);
	// Zero Hour style: 10 levels -> quartiles of the ladder, ending at the top rank
	EXPECT_EQ(milestoneCount(10), 4);
	int previous = 1;
	for (int i = 0; i < 4; ++i)
	{
		const int rank = milestoneRank(10, i, 4);
		EXPECT_GT(rank, previous);
		previous = rank;
	}
	EXPECT_EQ(milestoneRank(10, 3, 4), 10);
	EXPECT_EQ(milestoneCount(1), 0);
	EXPECT_EQ(milestoneCount(2), 1);
	EXPECT_EQ(milestoneCount(3), 2);
	// exactly one "next" cell, earned before it, locked after it, for every rank
	for (int levels : {2, 3, 5, 8, 10})
		for (int rank = 1; rank <= levels; ++rank)
		{
			const int count = milestoneCount(levels);
			int earned = 0, next = 0, locked = 0;
			for (int i = 0; i < count; ++i)
			{
				const Milestone m = milestoneState(rank, levels, i, count);
				earned += m == Milestone::Earned;
				next += m == Milestone::Next;
				locked += m == Milestone::Locked;
			}
			EXPECT_LE(next, 1) << levels << "/" << rank;
			EXPECT_EQ(earned + next + locked, count);
			if (rank == levels)
				EXPECT_EQ(earned, count) << levels;               // top rank: everything earned
			if (rank == 1)
				EXPECT_EQ(earned, 0) << levels;                   // fresh General: nothing earned
		}
}

TEST(EnhancedHudPanels, MilestoneProgressIsTheRealSkillPointFraction)
{
	using namespace EnhancedHudPanels;
	const auto needed = [](int level) { return 100 * (level - 1); };       // cumulative skill points per level
	// 5 levels: milestone 0 is rank 2 (needs 100), milestone 1 is rank 3 (needs 200) ...
	EXPECT_EQ(milestoneProgressPermille(0, 5, 0, 4, needed), 0);
	EXPECT_EQ(milestoneProgressPermille(50, 5, 0, 4, needed), 500);
	EXPECT_EQ(milestoneProgressPermille(100, 5, 0, 4, needed), 1000);
	EXPECT_EQ(milestoneProgressPermille(150, 5, 1, 4, needed), 500);
	EXPECT_EQ(milestoneProgressPermille(99999, 5, 1, 4, needed), 1000);
	EXPECT_EQ(milestoneProgressPermille(-5, 5, 1, 4, needed), 0);
	// degenerate tables never divide by zero
	EXPECT_EQ(milestoneProgressPermille(10, 5, 0, 4, [](int) { return 7; }), 0);
	// General Points pips: zero means none, more than the pips saturates
	EXPECT_EQ(pointPips(0, 5), 0);
	EXPECT_EQ(pointPips(-3, 5), 0);
	EXPECT_EQ(pointPips(2, 5), 2);
	EXPECT_EQ(pointPips(40, 5), 5);
}

TEST(EnhancedHudPanels, PowerStateMapsTheRetailWindowFlags)
{
	using namespace EnhancedHudPanels;
	EXPECT_EQ(powerState(true, false), PowerState::Available);
	EXPECT_EQ(powerState(true, true), PowerState::Available);       // enabled always wins (purchasable)
	EXPECT_EQ(powerState(false, true), PowerState::Purchased);      // disabled + ALWAYS_COLOR = player->hasScience
	EXPECT_EQ(powerState(false, false), PowerState::Locked);
}

TEST(EnhancedHudPanels, PopupSectionsHoldEveryRetailWindowOfGeneralsExpPoints)
{
	using namespace EnhancedHudPanels;
	struct Item { const char *name; Rect r; int section; };      // retail rectangles relative to the parent (296 x 432)
	const Item items[] = {
		{"StaticTextTitle", {8, 6, 160, 26}, 0},          {"StaticTextLevel", {176, 6, 113, 26}, 0},
		{"Experience label", {8, 38, 160, 16}, 0},        {"ExpBarHousing", {8, 54, 167, 18}, 0},
		{"ProgressBarExperience", {11, 59, 161, 8}, 0},   {"RemainingSkillPoints", {174, 36, 84, 36}, 0},
		{"RankPointsAvailable", {262, 40, 23, 31}, 0},
		{"Rank1 header", {0, 76, 296, 24}, 1},            {"ButtonRank1Number0", {16, 104, 56, 48}, 1},
		{"ButtonRank1Number1", {84, 104, 56, 48}, 1},     {"ButtonRank1Number2", {152, 104, 56, 48}, 1},
		{"Rank3 header", {0, 160, 296, 24}, 2},           {"ButtonRank3Number0", {16, 188, 56, 48}, 2},
		{"ButtonRank3Number2", {16, 292, 56, 48}, 2},     {"ButtonRank3Number9", {220, 188, 56, 48}, 2},
		{"ButtonRank3Number11", {220, 293, 56, 48}, 2},   {"Rank8 header", {0, 348, 296, 24}, 3},
		{"ButtonRank8Number0", {16, 376, 56, 48}, 3},     {"ButtonExit", {198, 392, 88, 32}, 3},
	};
	const Rect sections[4] = {kPopupHeader, kPopupRank1, kPopupRank3, kPopupRank8};
	for (const Item &it : items)
	{
		EXPECT_TRUE(it.r.within(sections[it.section])) << it.name;
		for (int s = 0; s < 4; ++s)
			if (s != it.section)
				EXPECT_FALSE(it.r.intersects(sections[s])) << it.name << " vs section " << s;
		EXPECT_TRUE(it.r.within(Rect{0, 0, kPopupW, kPopupH})) << it.name;
	}
	// the sections tile the popup top to bottom without gaps or overlap
	EXPECT_EQ(sections[0].bottom(), sections[1].y);
	EXPECT_EQ(sections[1].bottom(), sections[2].y);
	EXPECT_EQ(sections[2].bottom(), sections[3].y);
	EXPECT_EQ(sections[3].bottom(), kPopupH);
	// scaling keeps containment
	for (double u : {0.87, 1.0, 1.57, 2.25})
		for (const Item &it : items)
			EXPECT_TRUE(popupScaled(it.r, u).within(popupScaled(Rect{0, 0, kPopupW, kPopupH}, u) ) ) << it.name << " " << u;
}

TEST(EnhancedHudPanels, BattleOverviewShowsOnlyDataThatExistsAndIsSafe)
{
	using namespace EnhancedHudPanels;
	int n = 0;
	const OverviewTabInfo *tabs = overviewTabs(n);
	ASSERT_EQ(n, 4);
	EXPECT_TRUE(tabs[0].available);
	EXPECT_STREQ(tabs[0].label, "PLAYERS");
	EXPECT_EQ(availableOverviewTabs(), 1);                    // no fake ARMY / ECONOMY / COMBAT statistics
	for (int i = 1; i < n; ++i)
	{
		EXPECT_FALSE(tabs[i].available) << tabs[i].label;
		EXPECT_GT(std::strlen(tabs[i].reason), 10u) << tabs[i].label;   // every missing tab documents why
	}
	// the retail rows (8 slots) all fit the table module
	const Rect table = kOverviewTable;
	for (int row = 0; row < kOverviewRows; ++row)
	{
		const Rect r = {8, kOverviewRowY0 + row * kOverviewRowPitch, 408, 24};
		EXPECT_TRUE(r.within(table)) << row;
	}
	EXPECT_TRUE(kOverviewFooter.y >= table.bottom());
	EXPECT_TRUE(kOverviewFooter.bottom() <= kOverviewH);
	// the retail Done button (324,366 92x27) sits in the footer
	EXPECT_TRUE((Rect{324, 366, 92, 27}).within(kOverviewFooter));
}

TEST(EnhancedHudPanels, GameTimeAndMapNameAreFormattedFromSharedData)
{
	using namespace EnhancedHudPanels;
	wchar_t out[32];
	formatGameTime(0, 30, out, 32);                    EXPECT_STREQ(out, L"00:00:00");
	formatGameTime(30 * 61, 30, out, 32);              EXPECT_STREQ(out, L"00:01:01");
	formatGameTime(30LL * 3600 * 2 + 30 * 75, 30, out, 32);   EXPECT_STREQ(out, L"02:01:15");
	formatGameTime(-5, 30, out, 32);                   EXPECT_STREQ(out, L"00:00:00");
	EXPECT_EQ(formatGameTime(100, 0, out, 32), 0);     // invalid rate
	mapDisplayName("Maps\\Tournament Desert\\Tournament Desert.map", out, 32);   EXPECT_STREQ(out, L"Tournament Desert");
	mapDisplayName("Maps/Alpine Assault/Alpine Assault.map", out, 32);             EXPECT_STREQ(out, L"Alpine Assault");
	mapDisplayName("plain", out, 32);                                              EXPECT_STREQ(out, L"plain");
	mapDisplayName(nullptr, out, 32);                                              EXPECT_STREQ(out, L"");
	wchar_t tiny[4];
	EXPECT_EQ(mapDisplayName("Maps\\Long Name Here.map", tiny, 4), 3);              // never overruns
}
