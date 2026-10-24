/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// UI.1: the Modern UI layout core (Common/ModernUiLayout.h) at the resolution / UI-scale matrix of the project, with a declarative "match rules" panel as the
// proof of extensibility (three real rules + any number of future ones, no pixel offsets).

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include "Common/ModernUiLayout.h"

using namespace ModernUi;

namespace
{
struct Panel
{
	Tree tree;
	int root = kNone, header = kNone, rules = kNone, footer = kNone;
	std::vector<int> label, control;
};

// root column: header, a column of setting rows (label + control), footer row with two buttons
Panel makePanel(int ruleCount)
{
	Panel p;
	Node root; root.axis = Axis::Column; root.padding = { 12, 12, 12, 12 }; root.gap = 8;
	p.root = p.tree.add(kNone, root);
	Node header; header.prefH = 24; header.minH = 20;
	p.header = p.tree.add(p.root, header);
	Node rules; rules.axis = Axis::Column; rules.gap = 4; rules.weight = 1; rules.scroll = true; rules.minH = 60;
	p.rules = p.tree.add(p.root, rules);
	for (int i = 0; i < ruleCount; ++i)
	{
		Node row; row.axis = Axis::Row; row.gap = 8; row.prefH = 28; row.minH = 24; row.cross = Cross::Stretch;
		const int r = p.tree.add(p.rules, row);
		Node label; label.prefW = 140; label.minW = 100;
		Node control; control.prefW = 160; control.minW = 120; control.maxW = 260; control.weight = 1;
		p.label.push_back(p.tree.add(r, label));
		p.control.push_back(p.tree.add(r, control));
	}
	Node footer; footer.axis = Axis::Row; footer.gap = 8; footer.prefH = 30; footer.minH = 26;
	p.footer = p.tree.add(p.root, footer);
	Node button; button.prefW = 100; button.minW = 80; button.weight = 1; button.maxW = 160;
	p.tree.add(p.footer, button);
	p.tree.add(p.footer, button);
	return p;
}

const Size kSurfaces[] = { { 1920, 1080 }, { 2560, 1440 }, { 3440, 1440 }, { 3840, 2160 }, { 5120, 1440 } };
const int kScales[] = { 75, 100, 125, 150, 200, 300 };
}

TEST(ModernUiLayout, ColumnStacksWithGapAndPadding)
{
	Tree t;
	Node root; root.axis = Axis::Column; root.padding = { 10, 10, 10, 10 }; root.gap = 5;
	const int r = t.add(kNone, root);
	Node leaf; leaf.prefH = 20; leaf.prefW = 50;
	const int a = t.add(r, leaf), b = t.add(r, leaf);
	Result out;
	ASSERT_TRUE(solve(t, { 0, 0, 200, 200 }, 100, out));
	EXPECT_EQ(out.rect[a].y, 10); EXPECT_EQ(out.rect[a].h, 20);
	EXPECT_EQ(out.rect[b].y, 10 + 20 + 5);
	EXPECT_EQ(out.rect[a].x, 10); EXPECT_EQ(out.rect[a].w, 180);      // Stretch on the cross axis
	EXPECT_EQ(validate(t, out), IssueNone);
}

TEST(ModernUiLayout, WeightsShareFreeSpaceAndHonourMaximums)
{
	Tree t;
	Node root; root.axis = Axis::Row;
	const int r = t.add(kNone, root);
	Node fixed; fixed.prefW = 100;
	Node a; a.weight = 1; Node b; b.weight = 1; b.maxW = 120;
	const int f = t.add(r, fixed), x = t.add(r, a), y = t.add(r, b);
	Result out;
	solve(t, { 0, 0, 600, 40 }, 100, out);
	EXPECT_EQ(out.rect[f].w, 100);
	EXPECT_EQ(out.rect[y].w, 120);                                    // capped
	EXPECT_EQ(out.rect[x].w, 600 - 100 - 120);                        // takes what the capped one could not use
	EXPECT_EQ(validate(t, out), IssueNone);
}

TEST(ModernUiLayout, ShrinksTowardMinimumsAndReportsOverflowBelowThem)
{
	Tree t;
	Node root; root.axis = Axis::Row; root.gap = 0;
	const int r = t.add(kNone, root);
	Node c; c.prefW = 200; c.minW = 100;
	const int a = t.add(r, c), b = t.add(r, c);
	Result out;
	solve(t, { 0, 0, 300, 40 }, 100, out);                           // 400 preferred, 300 available: both shrink, none below 100
	EXPECT_EQ(out.rect[a].w + out.rect[b].w, 300);
	EXPECT_GE(out.rect[a].w, 100); EXPECT_GE(out.rect[b].w, 100);
	EXPECT_FALSE(out.overflow);
	solve(t, { 0, 0, 150, 40 }, 100, out);                           // below 2 x 100: cannot hold the children
	EXPECT_TRUE(out.overflow);
	EXPECT_NE(validate(t, out) & IssueOverflow, 0u);
}

TEST(ModernUiLayout, ScrollContainerReportsInsteadOfOverflowing)
{
	Tree t;
	Node root; root.axis = Axis::Column; root.scroll = true;
	const int r = t.add(kNone, root);
	Node row; row.prefH = 30;
	for (int i = 0; i < 20; ++i) t.add(r, row);
	Result out;
	solve(t, { 0, 0, 100, 200 }, 100, out);
	EXPECT_TRUE(out.scrollable[r]);
	EXPECT_FALSE(out.overflow);
	EXPECT_EQ(out.content[r].h, 600);                                // what a scroll bar needs to know
	solve(t, { 0, 0, 100, 700 }, 100, out);
	EXPECT_FALSE(out.scrollable[r]);
}

TEST(ModernUiLayout, UiScaleScalesEveryDimensionUniformly)
{
	Panel p = makePanel(3);
	Result a, b;
	solve(p.tree, { 0, 0, 1000, 1000 }, 100, a);
	solve(p.tree, { 0, 0, 2000, 2000 }, 200, b);
	for (int i : { p.header, p.footer })
		EXPECT_EQ(b.rect[i].h, 2 * a.rect[i].h) << i;
	EXPECT_EQ(b.rect[p.label[0]].w, 2 * a.rect[p.label[0]].w);
}

TEST(ModernUiLayout, MatchRulesPanelHasNoDefectAtTheResolutionAndScaleMatrix)
{
	for (const Size &surface : kSurfaces)
		for (int scale : kScales)
			for (int rules : { 3, 6, 12 })
			{
				Panel p = makePanel(rules);
				const Rect content = contentRect(surface, 640, scale);
				Result out;
				ASSERT_TRUE(solve(p.tree, content, scale, out));
				const unsigned issues = validate(p.tree, out);
				EXPECT_EQ(issues & (IssueOverlap | IssueOutsideParent), 0u) << surface.w << "x" << surface.h << " @" << scale << " rules " << rules;
				EXPECT_EQ(issues & IssueOverflow, 0u) << surface.w << "x" << surface.h << " @" << scale << " rules " << rules;
				// the panel never exceeds its content rectangle nor the surface
				EXPECT_TRUE(content.contains(out.rect[p.root]));
				EXPECT_LE(content.right(), surface.w); EXPECT_LE(content.bottom(), surface.h);
				for (int i = 0; i < rules; ++i)
				{
					EXPECT_GE(out.rect[p.control[i]].w, 120 * scale / 100 - 1);           // the control keeps its minimum width
					EXPECT_GE(out.rect[p.label[i]].w, 100 * scale / 100 - 1);
					EXPECT_LE(out.rect[p.control[i]].w, 260 * scale / 100 + 1);           // and never stretches to the surface
				}
			}
}

TEST(ModernUiLayout, UltrawideKeepsTheContentWidthInsteadOfStretching)
{
	for (int scale : kScales)
	{
		const Rect wide = contentRect({ 5120, 1440 }, 640, scale);
		const Rect uw = contentRect({ 3440, 1440 }, 640, scale);
		const Rect normal = contentRect({ 2560, 1440 }, 640, scale);
		if (640 * scale / 100 + 2 * (16 * scale / 100) <= 2560)                           // the cap is the limit (not the surface) at this scale
		{
			EXPECT_EQ(wide.w, normal.w) << scale;
			EXPECT_EQ(uw.w, normal.w) << scale;
		}
		EXPECT_EQ(wide.x + wide.w / 2, 5120 / 2) << scale;                                // centred
	}
}

TEST(ModernUiLayout, AddingARuleNeverMovesTheRulesBeforeIt)
{
	for (int scale : { 100, 200 })
	{
		const Rect content = contentRect({ 2560, 1440 }, 640, scale);
		Panel few = makePanel(3), many = makePanel(9);
		Result a, b;
		solve(few.tree, content, scale, a);
		solve(many.tree, content, scale, b);
		for (int i = 0; i < 3; ++i)
		{
			EXPECT_EQ(a.rect[few.control[i]].y, b.rect[many.control[i]].y) << i;
			EXPECT_EQ(a.rect[few.control[i]].h, b.rect[many.control[i]].h) << i;
			EXPECT_EQ(a.rect[few.control[i]].x, b.rect[many.control[i]].x) << i;
		}
	}
}

TEST(ModernUiLayout, TooManyRulesScrollAtSmallSurfacesInsteadOfOverlapping)
{
	Panel p = makePanel(40);
	const Rect content = contentRect({ 1920, 1080 }, 640, 200);
	Result out;
	solve(p.tree, content, 200, out);
	EXPECT_TRUE(out.scrollable[p.rules]);
	EXPECT_EQ(validate(p.tree, out) & IssueOverlap, 0u);
	EXPECT_GT(out.content[p.rules].h, out.rect[p.rules].h);
}

TEST(ModernUiLayout, SolveIsDeterministicAndPure)
{
	Panel p = makePanel(8);
	Result a, b;
	solve(p.tree, { 100, 50, 900, 700 }, 125, a);
	solve(p.tree, { 100, 50, 900, 700 }, 125, b);
	for (int i = 0; i < p.tree.count(); ++i)
	{
		EXPECT_EQ(a.rect[i].x, b.rect[i].x); EXPECT_EQ(a.rect[i].y, b.rect[i].y);
		EXPECT_EQ(a.rect[i].w, b.rect[i].w); EXPECT_EQ(a.rect[i].h, b.rect[i].h);
	}
	EXPECT_EQ(a.rect[p.root].x, 100);                                                       // the root is placed where it was offered
}

TEST(ModernUiLayout, PlainDataNoHeap)
{
	EXPECT_TRUE(std::is_trivially_copyable<Tree>::value);
	EXPECT_TRUE(std::is_trivially_copyable<Result>::value);
	EXPECT_LE(sizeof(Tree), 16u * 1024u);
	EXPECT_LE(sizeof(Result), 8u * 1024u);
	Panel p = makePanel(1000);                                                              // capacity is bounded: add() refuses instead of allocating
	EXPECT_EQ(p.tree.count(), kMaxNodes);
}

TEST(ModernUiLayout, SolvingAFullMatchPanelIsCheap)
{
	Panel p = makePanel(20);
	Result out;
	const Rect content = contentRect({ 3440, 1440 }, 640, 150);
	const auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < 2000; ++i) solve(p.tree, content, 150, out);
	const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 2000.0;
	EXPECT_LT(us, 100.0) << "per solve (microseconds); a screen re-solves only when the surface, the UI scale or its content changes";
}
