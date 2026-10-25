// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Frontend screen kit: the Match Setup composition (Map | Players | Match Rules) at the resolution x scale matrix, and rule generation from a spec list.
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "Common/ModernMatchSetupLayout.h"

using namespace ModernMatchSetup;

namespace
{
struct Surface { int w, h; };
const Surface kSurfaces[] = { { 1920, 1080 }, { 2560, 1440 }, { 3440, 1440 }, { 3840, 2160 }, { 5120, 1440 } };
const int kScales[] = { 75, 100, 150, 200, 300 };

Layout make(const Surface &s, int scale, int rules, bool honors = true)
{
	Input in; in.surfaceW = s.w; in.surfaceH = s.h; in.uiPercent = scale; in.ruleCount = rules; in.honors = honors;
	return compute(in);
}
}

TEST(ModernMatchSetup, EveryPartIsInsideItsZoneAndNothingOverlaps)
{
	for (const Surface &s : kSurfaces) for (int scale : kScales) for (int rules : { 0, 3, 4, 7, kMaxRules })
	{
		const Layout L = make(s, scale, rules);
		const std::string what = std::to_string(s.w) + "x" + std::to_string(s.h) + " @" + std::to_string(scale) + " rules " + std::to_string(rules);
		ASSERT_GT(L.panel.w, 0) << what;
		const ModernScreen::Rect surface{ 0, 0, s.w, s.h };
		EXPECT_TRUE(surface.contains(L.panel)) << what;
		EXPECT_TRUE(L.composition.contains(L.panel)) << what;
		const ModernScreen::Rect zones[] = { L.mapZone, L.playersZone, L.rulesZone };
		for (const ModernScreen::Rect &z : zones) EXPECT_TRUE(L.panel.contains(z)) << what;
		EXPECT_FALSE(L.mapZone.overlaps(L.playersZone)) << what;
		EXPECT_FALSE(L.playersZone.overlaps(L.rulesZone)) << what;
		for (const ModernScreen::Rect &r : { L.mapHeading, L.mapPreview, L.mapName, L.selectMap }) EXPECT_TRUE(L.mapZone.contains(r)) << what << " map part";
		EXPECT_FALSE(L.mapPreview.overlaps(L.mapName) || L.mapName.overlaps(L.selectMap)) << what;
		EXPECT_TRUE(L.playersZone.contains(L.playersHeading)) << what;
		for (int c = 0; c < kColumns; ++c)
		{
			EXPECT_TRUE(L.playersZone.contains(L.columnHead[c])) << what;
			EXPECT_FALSE(L.columnHead[c].overlaps(L.playersHeading)) << what;
			if (c) EXPECT_FALSE(L.columnHead[c].overlaps(L.columnHead[c - 1])) << what;
		}
		for (int r = 0; r < kSlots; ++r) for (int c = 0; c < kColumns; ++c)
		{
			EXPECT_TRUE(L.playersZone.contains(L.cell[r][c])) << what << " cell " << r << "," << c;
			EXPECT_GE(L.cell[r][c].h, 20) << what; EXPECT_GE(L.cell[r][c].w, 40) << what;
			if (c) EXPECT_FALSE(L.cell[r][c].overlaps(L.cell[r][c - 1])) << what;
			if (r) EXPECT_FALSE(L.cell[r][c].overlaps(L.cell[r - 1][c])) << what;
			EXPECT_FALSE(L.cell[r][c].overlaps(L.columnHead[c])) << what;
		}
		ASSERT_EQ(L.ruleCount, rules) << what;
		EXPECT_TRUE(L.rulesZone.contains(L.rulesHeading)) << what;
		for (int i = 0; i < L.ruleCount; ++i)
		{
			EXPECT_TRUE(L.rulesZone.contains(L.ruleLabel[i]) && L.rulesZone.contains(L.ruleControl[i])) << what << " rule " << i;
			EXPECT_FALSE(L.ruleLabel[i].overlaps(L.ruleControl[i])) << what;
			EXPECT_GE(L.ruleControl[i].h, 18) << what;
			if (i) { EXPECT_FALSE(L.ruleControl[i - 1].overlaps(L.ruleLabel[i])) << what; EXPECT_FALSE(L.ruleLabel[i].overlaps(L.ruleLabel[i - 1])) << what; }
		}
		for (int i = 0; i < kHonorRows; ++i)
		{
			EXPECT_TRUE(L.rulesZone.contains(L.honorLabel[i]) && L.rulesZone.contains(L.honorValue[i])) << what << " honor " << i;
			EXPECT_FALSE(L.honorLabel[i].overlaps(L.honorValue[i])) << what;
			if (rules) EXPECT_FALSE(L.ruleControl[rules - 1].overlaps(L.honorLabel[i])) << what;
		}
		for (const ModernScreen::Rect &r : { L.back, L.start, L.status, L.title, L.titleRule }) EXPECT_TRUE(L.panel.contains(r)) << what;
		for (const ModernScreen::Rect *z : { &L.mapZone, &L.playersZone, &L.rulesZone }) { EXPECT_FALSE(z->overlaps(L.back)) << what; EXPECT_FALSE(z->overlaps(L.start)) << what; EXPECT_FALSE(z->overlaps(L.status)) << what; }
		EXPECT_FALSE(L.back.overlaps(L.start)) << what;
		EXPECT_GE(L.start.h, 20) << what; EXPECT_GE(L.back.h, 20) << what;
		EXPECT_LE(L.effectiveScale, scale) << what;
	}
}

TEST(ModernMatchSetup, AddingARuleAddsACardAndMovesNothingElse)
{
	// "a synthetic rule needs no bespoke layout": the rule count is the only input; the cards before it keep their rectangles
	for (const Surface &s : kSurfaces)
	{
		const Layout a = make(s, 100, 3), b = make(s, 100, 4);
		if (a.effectiveScale != b.effectiveScale) continue;
		for (int i = 0; i < 3; ++i)
		{
			// relative to the zone: the panel is centred, so its top moves when the composition grows, the cards inside the zone do not
			EXPECT_EQ(a.ruleLabel[i].x - a.rulesZone.x, b.ruleLabel[i].x - b.rulesZone.x); EXPECT_EQ(a.ruleLabel[i].y - a.rulesZone.y, b.ruleLabel[i].y - b.rulesZone.y);
			EXPECT_EQ(a.ruleControl[i].y - a.rulesZone.y, b.ruleControl[i].y - b.rulesZone.y); EXPECT_EQ(a.ruleControl[i].w, b.ruleControl[i].w);
		}
		EXPECT_EQ(b.ruleCount, 4);
		EXPECT_GE(b.ruleControl[3].y, a.ruleControl[2].bottom());
		EXPECT_EQ(a.cell[7][3].x - a.playersZone.x, b.cell[7][3].x - b.playersZone.x);        // the players table does not move inside its zone
	}
	// more rules than the spec capacity are ignored, never overflow
	EXPECT_EQ(make({ 3840, 2160 }, 100, 99).ruleCount, kMaxRules);
}

TEST(ModernMatchSetup, UltrawideKeepsTheCompositionAndAtLeastTheMiddleOfTheMapVisible)
{
	const Layout w169 = make({ 2560, 1440 }, 100, 4), w219 = make({ 3440, 1440 }, 100, 4), w329 = make({ 5120, 1440 }, 100, 4);
	EXPECT_EQ(w169.panel.w, w219.panel.w); EXPECT_EQ(w169.panel.w, w329.panel.w);
	EXPECT_EQ(w329.panel.x + w329.panel.w / 2, 5120 / 2);
	EXPECT_GT(w329.panel.x, 0);
}

TEST(ModernMatchSetup, OversizedScalesAreReducedAndDeterministic)
{
	const Layout big = make({ 1920, 1080 }, 300, 4);
	EXPECT_TRUE(big.compact);
	EXPECT_LT(big.effectiveScale, 300);
	EXPECT_LE(big.panel.bottom(), 1080); EXPECT_LE(big.panel.right(), 1920);
	const Layout a = make({ 3440, 1440 }, 125, 5), b = make({ 3440, 1440 }, 125, 5);
	EXPECT_EQ(a.panel.x, b.panel.x); EXPECT_EQ(a.cell[3][2].w, b.cell[3][2].w); EXPECT_EQ(a.ruleControl[4].y, b.ruleControl[4].y);
	EXPECT_EQ(compute(Input{}).panel.w, 0);
}
