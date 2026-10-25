// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Frontend screen kit: the settings-screen geometry (rail, rows, scrolling, actions) at the whole resolution x scale matrix.
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "Common/ModernScreenLayout.h"

using namespace ModernScreen;

namespace
{
struct Surface { int w, h; };
const Surface kSurfaces[] = { { 1920, 1080 }, { 2560, 1440 }, { 3440, 1440 }, { 3840, 2160 }, { 5120, 1440 } };
const int kScales[] = { 75, 100, 150, 200, 300 };
const int kRowCounts[] = { 0, 1, 3, 7, 12, 30 };

Layout make(const Surface &s, int scale, int rows, int first = 0, int tabs = 6)
{
	Input in; in.surfaceW = s.w; in.surfaceH = s.h; in.uiPercent = scale; in.tabCount = tabs; in.rowCount = rows; in.firstRow = first;
	return compute(in);
}
}

TEST(ModernScreenLayout, EverythingIsInsideThePanelAndNothingOverlaps)
{
	for (const Surface &s : kSurfaces) for (int scale : kScales) for (int rows : kRowCounts)
	{
		const Layout L = make(s, scale, rows);
		const std::string what = std::to_string(s.w) + "x" + std::to_string(s.h) + " @" + std::to_string(scale) + " rows " + std::to_string(rows);
		const Rect surface{ 0, 0, s.w, s.h };
		ASSERT_GT(L.panel.w, 0) << what;
		EXPECT_TRUE(surface.contains(L.panel)) << what;
		EXPECT_TRUE(L.composition.contains(L.panel)) << what;
		std::vector<Rect> parts = { L.title, L.titleRule, L.rail, L.content, L.status, L.defaults, L.apply, L.back };
		for (const Rect &r : parts) { EXPECT_TRUE(L.panel.contains(r)) << what; EXPECT_GT(r.w, 0); EXPECT_GT(r.h, 0); }
		for (size_t i = 0; i < parts.size(); ++i) for (size_t j = i + 1; j < parts.size(); ++j)
			EXPECT_FALSE(parts[i].overlaps(parts[j])) << what << " parts " << i << "," << j;
		for (int i = 0; i < 6; ++i)
		{
			EXPECT_TRUE(L.rail.contains(L.tab[i])) << what;
			EXPECT_GE(L.tab[i].h, 20) << what;
			if (i) EXPECT_FALSE(L.tab[i].overlaps(L.tab[i - 1])) << what;
		}
		for (int i = 0; i < L.visibleRows; ++i)
		{
			EXPECT_TRUE(L.content.contains(L.row[i])) << what << " row " << i;
			EXPECT_TRUE(L.row[i].contains(L.label[i])) << what;
			EXPECT_TRUE(L.row[i].contains(L.control[i])) << what;
			EXPECT_FALSE(L.label[i].overlaps(L.control[i])) << what;
			EXPECT_GE(L.control[i].h, 18) << what; EXPECT_GE(L.control[i].w, 60) << what;
			if (i) EXPECT_FALSE(L.row[i].overlaps(L.row[i - 1])) << what;
		}
		for (const Rect *b : { &L.defaults, &L.apply, &L.back }) EXPECT_GE(b->h, 20) << what;
		EXPECT_LE(L.effectiveScale, scale) << what;
	}
}

TEST(ModernScreenLayout, AtLeastFourRowsAreVisibleAtEveryScale)
{
	for (const Surface &s : kSurfaces) for (int scale : kScales)
	{
		const Layout L = make(s, scale, 12);
		EXPECT_GE(L.visibleRows, 4) << s.w << "x" << s.h << " @" << scale;
		EXPECT_TRUE(L.scrollable == (L.rowCount > L.visibleRows));
	}
}

TEST(ModernScreenLayout, AnOversizedScaleIsReducedNotClipped)
{
	const Layout L = make({ 1920, 1080 }, 300, 7);
	EXPECT_TRUE(L.compact);
	EXPECT_LT(L.effectiveScale, 300);
	EXPECT_LE(L.panel.bottom(), 1080);
	const Layout fit = make({ 3840, 2160 }, 150, 7);
	EXPECT_FALSE(fit.compact);
	EXPECT_EQ(fit.effectiveScale, 150);
}

TEST(ModernScreenLayout, UltrawideKeepsAComposedPanelAndTheMapVisible)
{
	const Layout w169 = make({ 2560, 1440 }, 100, 7), w219 = make({ 3440, 1440 }, 100, 7), w329 = make({ 5120, 1440 }, 100, 7);
	EXPECT_EQ(w169.panel.w, w219.panel.w); EXPECT_EQ(w169.panel.w, w329.panel.w);
	EXPECT_EQ(w169.panel.h, w329.panel.h);
	EXPECT_LE(w329.panel.w * 2, 5120);                 // under half of a 32:9 surface
	EXPECT_EQ(w329.panel.x + w329.panel.w / 2, 5120 / 2);   // centred
}

TEST(ModernScreenLayout, AddingARowNeverMovesTheEarlierOnes)
{
	for (const Surface &s : kSurfaces) for (int scale : kScales)
	{
		const Layout a = make(s, scale, 3), b = make(s, scale, 4);
		if (a.scrollable || b.scrollable) continue;
		for (int i = 0; i < 3; ++i)
		{
			EXPECT_EQ(a.label[i].x, b.label[i].x); EXPECT_EQ(a.label[i].y, b.label[i].y);
			EXPECT_EQ(a.control[i].x, b.control[i].x); EXPECT_EQ(a.control[i].w, b.control[i].w);
		}
		EXPECT_EQ(b.visibleRows, 4);
	}
}

TEST(ModernScreenLayout, ScrollingClampsRevealsAndMovesTheThumb)
{
	EXPECT_EQ(clampFirstRow(-3, 12, 5), 0);
	EXPECT_EQ(clampFirstRow(99, 12, 5), 7);
	EXPECT_EQ(clampFirstRow(4, 3, 5), 0);
	EXPECT_EQ(firstRowToReveal(9, 0, 12, 5), 5);
	EXPECT_EQ(firstRowToReveal(2, 6, 12, 5), 2);
	EXPECT_EQ(firstRowToReveal(3, 1, 12, 5), 1);
	const Layout top = make({ 2560, 1440 }, 150, 30, 0), bottom = make({ 2560, 1440 }, 150, 30, 999);
	ASSERT_TRUE(top.scrollable);
	EXPECT_EQ(top.firstRow, 0);
	EXPECT_EQ(bottom.firstRow, 30 - bottom.visibleRows);
	EXPECT_LT(top.scrollThumb.y, bottom.scrollThumb.y);
	EXPECT_TRUE(top.scrollTrack.contains(top.scrollThumb)); EXPECT_TRUE(bottom.scrollTrack.contains(bottom.scrollThumb));
	EXPECT_EQ(bottom.scrollThumb.bottom(), bottom.scrollTrack.bottom());
	EXPECT_FALSE(make({ 2560, 1440 }, 100, 3).scrollable);
	for (int i = 0; i < top.visibleRows; ++i) EXPECT_FALSE(top.scrollTrack.overlaps(top.row[i]));
}

TEST(ModernScreenLayout, DeterministicAndCheap)
{
	const Layout a = make({ 3440, 1440 }, 125, 9), b = make({ 3440, 1440 }, 125, 9);
	EXPECT_EQ(a.panel.x, b.panel.x); EXPECT_EQ(a.panel.h, b.panel.h); EXPECT_EQ(a.control[2].x, b.control[2].x);
	EXPECT_EQ(compute(Input{}).panel.w, 0);   // an empty surface is not an error
}

TEST(ModernDialogLayout, FitsEverySurfaceAtEveryScaleWithReachableButtons)
{
	for (const Surface &s : kSurfaces) for (int scale : kScales) for (int buttons = 1; buttons <= kMaxDialogButtons; ++buttons) for (int body : { 0, 40, 160, 4000 })
	{
		DialogInput in; in.surfaceW = s.w; in.surfaceH = s.h; in.uiPercent = scale; in.buttonCount = buttons; in.bodyHeight = body;
		const DialogLayout L = computeDialog(in);
		const std::string what = std::to_string(s.w) + "x" + std::to_string(s.h) + " @" + std::to_string(scale) + " buttons " + std::to_string(buttons) + " body " + std::to_string(body);
		const Rect surface{ 0, 0, s.w, s.h };
		ASSERT_GT(L.panel.w, 0) << what;
		EXPECT_TRUE(surface.contains(L.panel)) << what;
		EXPECT_TRUE(L.panel.contains(L.title) && L.panel.contains(L.titleRule) && L.panel.contains(L.body)) << what;
		EXPECT_FALSE(L.title.overlaps(L.body)) << what;
		for (int i = 0; i < buttons; ++i)
		{
			EXPECT_TRUE(L.panel.contains(L.button[i])) << what << " button " << i;
			EXPECT_GE(L.button[i].h, 20) << what;
			EXPECT_FALSE(L.button[i].overlaps(L.body)) << what << " button " << i;
			if (i) { EXPECT_FALSE(L.button[i].overlaps(L.button[i - 1])) << what; EXPECT_GT(L.button[i].x, L.button[i - 1].x) << what; }
		}
		EXPECT_LE(L.panel.right() - L.button[buttons - 1].right(), L.panel.w / 4) << what;   // the primary action is the rightmost, close to the panel edge
		EXPECT_LE(std::abs(L.panel.x + L.panel.w / 2 - s.w / 2), 1) << what;
	}
}

TEST(ModernDialogLayout, GrowsWithTheBodyAndStaysCentred)
{
	DialogInput a; a.surfaceW = 2560; a.surfaceH = 1440; a.uiPercent = 100; a.buttonCount = 2; a.bodyHeight = 30;
	DialogInput b = a; b.bodyHeight = 200;
	const DialogLayout la = computeDialog(a), lb = computeDialog(b);
	EXPECT_GT(lb.panel.h, la.panel.h);
	EXPECT_EQ(la.panel.w, lb.panel.w);
	EXPECT_LE(std::abs((la.panel.y + la.panel.h / 2) - (lb.panel.y + lb.panel.h / 2)), 1);
}
