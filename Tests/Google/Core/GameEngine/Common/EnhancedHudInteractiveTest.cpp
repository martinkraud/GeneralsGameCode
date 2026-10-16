/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// Enhanced gameplay HUD (USA prototype): interactive geometry.
//
// Runs the real placement mechanism (Common/EnhancedHudWindows.h, the code ControlBar::applyEnhancedLayout runs) on a
// mock window tree with exactly the GameWindow position/size/hit-test semantics and the ControlBar.wnd /
// GenPowersShortcutBarUS.wnd hierarchy and retail rectangles. A drawn rectangle must be the clickable rectangle.

#include <gtest/gtest.h>

#include "Common/DisplayGeometry.h"
#include "Common/EnhancedHudLayout.h"
#include "Common/EnhancedHudWindows.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace EnhancedHudLayout;

namespace
{

// Same semantics as GameWindow: regions are parent relative, winSetPosition keeps the size and does not move children,
// hit testing descends through the children and sums the ancestors' origins (GameWindow::winPointInChild).
struct Win
{
	std::string name;
	Win *parent = nullptr, *child = nullptr, *next = nullptr;
	int lox = 0, loy = 0, w = 0, h = 0;
	bool hidden = false;
	bool noInput = false;

	Win *winGetParent() { return parent; }
	Win *winGetChild() { return child; }
	Win *winGetNext() { return next; }
	int winGetPosition(int *x, int *y) { *x = lox; *y = loy; return 0; }
	int winSetPosition(int x, int y) { lox = x; loy = y; return 0; }
	int winGetSize(int *x, int *y) { *x = w; *y = h; return 0; }
	int winSetSize(int x, int y) { w = x; h = y; return 0; }
	int winGetScreenPosition(int *x, int *y)
	{
		*x = lox; *y = loy;
		for (Win *p = parent; p; p = p->parent) { *x += p->lox; *y += p->loy; }
		return 0;
	}
	void winHide(bool h2) { hidden = h2; }
	bool winIsHidden() { return hidden; }

	Win *pointInChild(int x, int y)
	{
		for (Win *c = child; c; c = c->next)
		{
			int ox = c->lox, oy = c->loy;
			for (Win *p = c->parent; p; p = p->parent) { ox += p->lox; oy += p->loy; }
			if (x >= ox && x <= ox + c->w && y >= oy && y <= oy + c->h && !c->hidden)
				return c->pointInChild(x, y);
		}
		return this;
	}
};

struct Def { const char *name; int parent, x, y, w, h; bool hidden; };

// ControlBar.wnd (retail, screen rectangles at 800 x 600). Order = file order.
const Def kBar[] = {
	{"ControlBarParent", -1, 0, 416, 800, 184, false},
	{"Munkee", 0, 0, 414, 800, 186, false},
	{"BackgroundMarker", 0, 8, 595, 5, 5, false},
	{"", 0, 10, 459, 790, 141, false},
	{"", 0, 0, 421, 186, 179, false},
	{"", 0, 614, 428, 186, 172, false},
	{"CenterBackground", 0, 0, 479, 800, 121, false},
	{"BeaconWindow", 6, 198, 492, 408, 100, true},
	{"ButtonDeleteBeacon", 7, 252, 520, 56, 48, false},
	{"EditBeaconText", 7, 312, 544, 232, 25, false},
	{"StaticTextBeaconLabel", 7, 312, 520, 285, 24, false},
	{"ButtonClearBeaconText", 7, 544, 544, 56, 24, false},
	{"CommandWindow", 6, 198, 492, 408, 100, true},
	{"ButtonCommand12", 12, 546, 546, 53, 45, false},
	{"ButtonCommand11", 12, 546, 493, 53, 45, false},
	{"ButtonCommand10", 12, 486, 546, 53, 45, false},
	{"ButtonCommand09", 12, 486, 493, 53, 45, false},
	{"ButtonCommand08", 12, 426, 546, 53, 45, false},
	{"ButtonCommand07", 12, 426, 493, 53, 45, false},
	{"ButtonCommand06", 12, 366, 546, 53, 45, false},
	{"ButtonCommand05", 12, 366, 493, 53, 45, false},
	{"ButtonCommand04", 12, 306, 546, 53, 45, false},
	{"ButtonCommand03", 12, 306, 493, 53, 45, false},
	{"ButtonCommand02", 12, 246, 546, 53, 45, false},
	{"ButtonCommand01", 12, 246, 493, 53, 45, false},
	{"UnderConstructionWindow", 6, 198, 492, 408, 100, true},
	{"ButtonCancelConstruction", 25, 259, 519, 56, 48, false},
	{"UnderConstructionDesc", 25, 323, 519, 276, 48, false},
	{"ObserverPlayerInfoWindow", 6, 198, 492, 408, 100, false},
	{"WinGeneralPortrait", 28, 268, 496, 108, 96, true},
	{"WinFlag", 28, 328, 496, 160, 96, false},
	{"ButtonCancel", 28, 540, 567, 66, 23, false},
	{"StaticTextObsBuildings", 28, 315, 528, 91, 32, false},
	{"StaticTextObsUnits", 28, 247, 528, 70, 32, false},
	{"StaticTextObsUnitsLost", 28, 488, 528, 68, 32, false},
	{"StaticTextObsUnitsKilled", 28, 410, 528, 85, 32, false},
	{"StaticTextPlayerName", 28, 252, 492, 314, 36, false},
	{"StaticTextNumberOfUnitsLost", 28, 488, 560, 68, 28, false},
	{"StaticTextNumberOfUnitsKilled", 28, 412, 560, 76, 28, false},
	{"StaticTextNumberOfBuildings", 28, 321, 560, 80, 28, false},
	{"StaticTextNumberOfUnits", 28, 251, 560, 60, 28, false},
	{"ObserverPlayerListWindow", 6, 198, 492, 408, 100, true},
	{"ButtonPlayer7", 41, 426, 568, 20, 20, false},
	{"ButtonPlayer6", 41, 426, 544, 20, 20, false},
	{"ButtonPlayer5", 41, 426, 520, 20, 20, false},
	{"ButtonPlayer4", 41, 426, 496, 20, 20, false},
	{"ButtonPlayer3", 41, 242, 568, 20, 20, false},
	{"ButtonPlayer2", 41, 242, 544, 20, 20, false},
	{"ButtonPlayer1", 41, 242, 520, 20, 20, false},
	{"ButtonPlayer0", 41, 242, 496, 20, 20, false},
	{"StaticTextPlayer7", 41, 448, 566, 154, 26, false},
	{"StaticTextPlayer6", 41, 448, 542, 154, 26, false},
	{"StaticTextPlayer5", 41, 448, 518, 154, 26, false},
	{"StaticTextPlayer4", 41, 448, 494, 154, 26, false},
	{"StaticTextPlayer3", 41, 264, 566, 158, 26, false},
	{"StaticTextPlayer2", 41, 264, 544, 158, 24, false},
	{"StaticTextPlayer1", 41, 264, 518, 158, 26, false},
	{"StaticTextPlayer0", 41, 264, 494, 158, 28, false},
	{"OCLTimerWindow", 6, 200, 491, 408, 100, true},
	{"OCLTimerSellButton", 58, 546, 539, 56, 48, false},
	{"OCLTimerProgressBar", 58, 248, 516, 292, 56, false},
	{"OCLTimerStaticText", 58, 256, 524, 276, 40, false},
	{"LeftHUD", 0, 7, 443, 167, 152, false},
	{"GeneralsExp", 0, 769, 503, 13, 87, false},
	{"ExpBarForeground", 0, 766, 484, 21, 107, false},
	{"RightHUD", 0, 621, 483, 139, 109, false},
	{"ProductionQueueWindow", 65, 621, 483, 141, 109, false},
	{"ButtonQueue09", 66, 715, 555, 46, 36, false},
	{"ButtonQueue08", 66, 668, 555, 45, 36, false},
	{"ButtonQueue07", 66, 622, 555, 44, 36, false},
	{"ButtonQueue06", 66, 715, 519, 46, 34, false},
	{"ButtonQueue05", 66, 668, 519, 45, 34, false},
	{"ButtonQueue04", 66, 622, 519, 44, 34, false},
	{"ButtonQueue03", 66, 715, 484, 46, 33, false},
	{"ButtonQueue02", 66, 668, 484, 45, 33, false},
	{"ButtonQueue01", 66, 622, 484, 44, 33, false},
	{"WinUnitSelected", 65, 621, 483, 141, 109, false},
	{"CameoWindow", 76, 622, 484, 91, 69, false},
	{"UnitUpgrade1", 76, 715, 484, 46, 33, false},
	{"UnitUpgrade2", 76, 715, 519, 46, 34, false},
	{"UnitUpgrade3", 76, 715, 555, 46, 36, false},
	{"UnitUpgrade4", 76, 668, 555, 45, 36, false},
	{"UnitUpgrade5", 76, 622, 555, 44, 36, false},
	{"ButtonIdleWorker", 0, 206, 519, 30, 22, false},
	{"ButtonPlaceBeacon", 0, 206, 543, 30, 22, false},
	{"PopupCommunicator", 0, 206, 567, 30, 22, false},
	{"ButtonOptions", 0, 206, 495, 30, 22, false},
	{"ForegroundMarker", 0, 0, 595, 5, 5, false},
	{"ButtonGeneral", 0, 736, 433, 56, 41, false},
	{"MoneyDisplay", 0, 360, 437, 79, 19, false},
	{"PowerWindow", 0, 261, 473, 283, 7, false},
	{"ButtonSmall", 0, 592, 436, 31, 21, true},
	{"ButtonLarge", 0, 666, 445, 48, 28, false},
	{"ButtonMedium", 0, 625, 439, 31, 21, true},
	{"WinUAttack", 0, 77, 416, 45, 45, false},
	{"OnTopDraw", 0, 3, 593, 31, 7, false},
};

// GenPowersShortcutBarUS.wnd
const Def kPowers[] = {
	{"Parent", -1, 740, 159, 60, 270, false},
	{"ButtonParent1", 0, 740, 159, 59, 56, false}, {"ButtonCommand1", 1, 743, 168, 45, 38, false},
	{"ButtonParent2", 0, 740, 208, 59, 56, false}, {"ButtonCommand2", 3, 743, 217, 45, 38, false},
	{"ButtonParent3", 0, 740, 257, 59, 56, false}, {"ButtonCommand3", 5, 743, 266, 45, 38, false},
	{"ButtonParent4", 0, 740, 306, 59, 56, false}, {"ButtonCommand4", 7, 743, 315, 45, 38, false},
	{"ButtonParent5", 0, 740, 355, 59, 56, false}, {"ButtonCommand5", 9, 743, 364, 45, 38, false},
};

struct Tree
{
	std::vector<std::unique_ptr<Win>> wins;
	std::map<std::string, Win *> byName;
	Win *master() { return wins.front().get(); }
	Win *find(const std::string &n) const { auto it = byName.find(n); return it == byName.end() ? nullptr : it->second; }

	// Loader behaviour: screen rectangle -> parent relative; GameWindowManager::addWindowToParent puts a new child at the
	// HEAD of its parent's list (later windows are hit first).
	template <size_t N>
	void build(const Def (&defs)[N])
	{
		for (size_t i = 0; i < N; ++i)
		{
			std::unique_ptr<Win> w(new Win);
			w->name = defs[i].name;
			w->w = defs[i].w; w->h = defs[i].h; w->hidden = defs[i].hidden;
			int px = 0, py = 0;
			if (defs[i].parent >= 0)
			{
				Win *p = wins[defs[i].parent].get();
				p->winGetScreenPosition(&px, &py);
				w->parent = p;
				w->next = p->child;
				p->child = w.get();
			}
			w->lox = defs[i].x - px; w->loy = defs[i].y - py;
			if (!w->name.empty() && !byName.count(w->name))
				byName[w->name] = w.get();
			wins.push_back(std::move(w));
		}
		for (auto &w : wins)
			if (w->name == "Munkee" || w->name == "BackgroundMarker" || w->name == "ForegroundMarker" || w->name == "OnTopDraw")
				w->noInput = true;
	}
};

// The loader creates every window at the modern layout scale before the Enhanced layout sees it.
void scaleLoaded(Tree &t, double ls)
{
	for (auto &w : t.wins)
	{
		w->lox = static_cast<int>(std::lround(w->lox * ls));
		w->loy = static_cast<int>(std::lround(w->loy * ls));
		w->w = static_cast<int>(std::lround(w->w * ls));
		w->h = static_cast<int>(std::lround(w->h * ls));
	}
}

double layoutScaleFor(int w, int h, int ui) { return DisplayGeometry::computeLayout({w, h}, ui).scale(); }

struct Surface { int w, h; };
const Surface kSurfaces[] = {{800, 600}, {1024, 768}, {1280, 1024}, {1366, 768}, {1920, 1080},
                             {1920, 1200}, {2560, 1440}, {3440, 1440}, {5120, 1440}};
const int kUiScales[] = {DisplayGeometry::kAutoUiScale, 75, 100, 150, 200};

const char *kCommandContexts[] = {"CommandWindow", "BeaconWindow", "UnderConstructionWindow", "ObserverPlayerInfoWindow",
                                  "ObserverPlayerListWindow", "OCLTimerWindow"};
const char *kUnitContexts[] = {"WinUnitSelected", "ProductionQueueWindow"};

bool isDecorative(Win *w) { return w->name.empty() || w->name == "Munkee" || w->name == "CenterBackground"; }

void applyEnhanced(Tree &t, const Result &layout)
{
	std::set<Win *> remembered;
	EnhancedHudWindows::apply<Win>(t.master(), layout,
		[&t](const char *n) { return t.find(n); },
		[&remembered](Win *w) { remembered.insert(w); },
		[](Win *w) { return isDecorative(w); });
}

void showContext(Tree &t, const char *command, const char *unit)
{
	for (const char *c : kCommandContexts)
		t.find(c)->hidden = std::strcmp(c, command) != 0;
	for (const char *u : kUnitContexts)
		t.find(u)->hidden = std::strcmp(u, unit) != 0;
	t.find("RightHUD")->hidden = false;
	t.find("CenterBackground")->hidden = false;
}

// Input path of GameWindowManager::getWindowUnderCursor for a single top-level window.
Win *hitTest(Win *top, int x, int y)
{
	if (top->hidden || x < top->lox || x > top->lox + top->w || y < top->loy || y > top->loy + top->h)
		return nullptr;
	return top->pointInChild(x, y);
}

Rect screen(Win *w) { return EnhancedHudWindows::screenRect(w); }

// Visible = this window and all its ancestors are visible.
bool visible(Win *w)
{
	for (; w; w = w->parent)
		if (w->hidden)
			return false;
	return true;
}

void collect(Win *w, std::vector<Win *> &out)
{
	out.push_back(w);
	for (Win *c = w->child; c; c = c->next)
		collect(c, out);
}

struct Probe { int x, y; const char *what; };
std::vector<Probe> insideProbes(const Rect &r)
{
	const int cx = r.x + r.w / 2, cy = r.y + r.h / 2, l = r.x + 1, t = r.y + 1, rr = r.x + r.w - 2, b = r.y + r.h - 2;
	return {{cx, cy, "centre"},      {l, t, "top-left"},    {rr, t, "top-right"}, {l, b, "bottom-left"},
	        {rr, b, "bottom-right"}, {cx, t, "top edge"},   {cx, b, "bottom edge"}, {l, cy, "left edge"},
	        {rr, cy, "right edge"}};
}

// The invariant: every point inside the screen rectangle of a visible, input-taking leaf window hits that window
// (unless another input window also covers the point), and the points just outside never do.
void expectInteractiveEqualsDrawn(Win *top, const std::string &where)
{
	std::vector<Win *> all;
	collect(top, all);
	std::vector<Win *> leaves;
	for (Win *w : all)
		if (w != top && w->child == nullptr && visible(w) && !w->noInput && !w->name.empty())
			leaves.push_back(w);
	for (Win *leaf : leaves)
	{
		const Rect r = screen(leaf);
		if (r.w < 4 || r.h < 4)
			continue;
		for (const Probe &p : insideProbes(r))
		{
			bool shared = false;
			for (Win *other : leaves)
			{
				// GameWindow hit testing is inclusive of the right and bottom edge
				const Rect o = screen(other);
				if (other != leaf && p.x >= o.x && p.x <= o.right() && p.y >= o.y && p.y <= o.bottom())
					shared = true;
			}
			if (shared)
				continue;
			Win *hit = hitTest(top, p.x, p.y);
			EXPECT_EQ(hit, leaf) << where << ": " << leaf->name << " " << p.what << " (" << p.x << "," << p.y
			                     << ") hit " << (hit ? hit->name : "nothing") << " rect " << r.x << "," << r.y << " "
			                     << r.w << "x" << r.h;
		}
		const Probe outside[] = {{r.x + r.w / 2, r.y - 3, "above"}, {r.x + r.w / 2, r.y + r.h + 3, "below"},
		                         {r.x - 3, r.y + r.h / 2, "left"}, {r.x + r.w + 3, r.y + r.h / 2, "right"}};
		for (const Probe &p : outside)
		{
			EXPECT_NE(hitTest(top, p.x, p.y), leaf) << where << ": " << leaf->name << " reacts " << p.what << " its rectangle";
		}
	}
}

}  // namespace

TEST(EnhancedHudInteractive, RetailTreeIsWhatTheTestAssumes)
{
	Tree t;
	t.build(kBar);
	// the command windows and the 12 buttons live below the unplaced CenterBackground container
	ASSERT_NE(t.find("CenterBackground"), nullptr);
	EXPECT_EQ(t.find("CommandWindow")->parent, t.find("CenterBackground"));
	EXPECT_EQ(t.find("ButtonCommand01")->parent, t.find("CommandWindow"));
	EXPECT_EQ(t.find("WinUnitSelected")->parent, t.find("RightHUD"));
	// Original (retail) hit testing: a click on the middle of ButtonCommand01 hits it
	showContext(t, "CommandWindow", "WinUnitSelected");
	const Rect r = screen(t.find("ButtonCommand01"));
	EXPECT_EQ(hitTest(t.master(), r.x + r.w / 2, r.y + r.h / 2), t.find("ButtonCommand01"));
}

TEST(EnhancedHudInteractive, EveryVisibleWindowIsClickableWhereItIsDrawn)
{
	for (const Surface &s : kSurfaces)
		for (int ui : kUiScales)
		{
			const double ls = layoutScaleFor(s.w, s.h, ui);
			const Result layout = EnhancedHudLayout::compute(s.w, s.h, ls);
			if (!layout.valid)
				continue;
			for (const char *cc : kCommandContexts)
				for (const char *uc : kUnitContexts)
				{
					Tree t;
					t.build(kBar);
					applyEnhanced(t, layout);
					showContext(t, cc, uc);
					expectInteractiveEqualsDrawn(t.master(), std::to_string(s.w) + "x" + std::to_string(s.h) + " ui" +
					                             std::to_string(ui) + " " + cc + "/" + uc);
				}
		}
}

TEST(EnhancedHudInteractive, TwelveCommandButtonsHitExactlyTheirLayoutRectangle)
{
	for (const Surface &s : kSurfaces)
		for (int ui : kUiScales)
		{
			const Result layout = EnhancedHudLayout::compute(s.w, s.h, layoutScaleFor(s.w, s.h, ui));
			if (!layout.valid)
				continue;
			Tree t;
			t.build(kBar);
			applyEnhanced(t, layout);
			showContext(t, "CommandWindow", "WinUnitSelected");
			int n = 0;
			const GroupWindow *cmd = commandGroup(n);
			int hits = 0;
			for (int i = 0; i < n; ++i)
			{
				if (std::strncmp(cmd[i].window, "ButtonCommand", 13) != 0)
					continue;
				Rect dst; bool hide = false;
				ASSERT_TRUE(destinationFor(cmd[i].window, dst, hide));
				const Rect expect = toPhysical(layout, dst);
				Win *b = t.find(cmd[i].window);
				ASSERT_NE(b, nullptr);
				const Rect got = screen(b);
				EXPECT_EQ(got.x, expect.x) << cmd[i].window;
				EXPECT_EQ(got.y, expect.y) << cmd[i].window;
				EXPECT_EQ(got.w, expect.w) << cmd[i].window;
				EXPECT_EQ(got.h, expect.h) << cmd[i].window;
				for (const Probe &p : insideProbes(expect))
					EXPECT_EQ(hitTest(t.master(), p.x, p.y), b)
						<< cmd[i].window << " " << p.what << " at " << s.w << "x" << s.h << " ui" << ui;
				// immediately above the top edge / below the bottom edge is another window or the island, never this one
				EXPECT_NE(hitTest(t.master(), expect.x + expect.w / 2, expect.y - 2), b) << cmd[i].window;
				EXPECT_NE(hitTest(t.master(), expect.x + expect.w / 2, expect.y + expect.h + 2), b) << cmd[i].window;
				++hits;
			}
			EXPECT_EQ(hits, 12);
		}
}

TEST(EnhancedHudInteractive, RadarGeneralXpAndUtilityButtonsAreInteractive)
{
	const Surface s = {1920, 1200};
	const Result layout = EnhancedHudLayout::compute(s.w, s.h, layoutScaleFor(s.w, s.h, DisplayGeometry::kAutoUiScale));
	Tree t;
	t.build(kBar);
	applyEnhanced(t, layout);
	showContext(t, "CommandWindow", "WinUnitSelected");
	for (const char *n : {"LeftHUD", "ButtonGeneral", "ExpBarForeground", "ButtonOptions", "ButtonIdleWorker",
	                      "ButtonPlaceBeacon", "PopupCommunicator", "MoneyDisplay"})
	{
		Win *w = t.find(n);
		ASSERT_NE(w, nullptr) << n;
		Rect dst; bool hide = false;
		ASSERT_TRUE(destinationFor(n, dst, hide)) << n;
		const Rect r = toPhysical(layout, dst);
		EXPECT_EQ(screen(w).x, r.x) << n;
		EXPECT_EQ(screen(w).y, r.y) << n;
		// radar: probe away from the attack indicator that is drawn on top of its corner
		const bool radar = std::strcmp(n, "LeftHUD") == 0;
		const int px = radar ? r.x + r.w - 3 : r.x + r.w / 2;
		const int py = radar ? r.y + r.h - 3 : r.y + 2;
		EXPECT_EQ(hitTest(t.master(), px, py), w) << n;
	}
	// the world beside and above the island is not the HUD
	EXPECT_EQ(hitTest(t.master(), layout.island.x - 3, layout.island.y + layout.island.h / 2), nullptr);
	EXPECT_EQ(hitTest(t.master(), layout.island.x + layout.island.w / 2, layout.island.y - 3), nullptr);
}

TEST(EnhancedHudInteractive, NoInputMarkersDoNotSwallowClicksOverControls)
{
	// GameWindowManager discards a click whose topmost window has WIN_STATUS_NO_INPUT, so the retail draw markers must
	// not sit on any control.
	for (const Surface &s : kSurfaces)
		for (int ui : kUiScales)
		{
			const Result layout = EnhancedHudLayout::compute(s.w, s.h, layoutScaleFor(s.w, s.h, ui));
			if (!layout.valid)
				continue;
			Tree t;
			t.build(kBar);
			applyEnhanced(t, layout);
			showContext(t, "CommandWindow", "WinUnitSelected");
			std::vector<Win *> all;
			collect(t.master(), all);
			for (Win *m : all)
			{
				if (!m->noInput || !visible(m))
					continue;
				const Rect mr = screen(m);
				for (Win *o : all)
				{
					if (o == m || o->noInput || !visible(o) || o == t.master() || o->name.empty() || o->child != nullptr)
						continue;
					const Rect r = screen(o);
					const bool overlap = mr.x <= r.right() && r.x <= mr.right() && mr.y <= r.bottom() && r.y <= mr.bottom();
					EXPECT_FALSE(overlap) << m->name << " overlaps " << o->name << " at " << s.w << "x" << s.h << " ui" << ui;
				}
			}
		}
}

TEST(EnhancedHudInteractive, EveryContainerContainsItsChildren)
{
	for (const Surface &s : kSurfaces)
		for (int ui : kUiScales)
		{
			const Result layout = EnhancedHudLayout::compute(s.w, s.h, layoutScaleFor(s.w, s.h, ui));
			if (!layout.valid)
				continue;
			Tree t;
			t.build(kBar);
			applyEnhanced(t, layout);
			std::vector<Win *> all;
			collect(t.master(), all);
			for (Win *w : all)
			{
				if (w == t.master() || w->child == nullptr)
					continue;
				const Rect pr = screen(w);
				for (Win *c = w->child; c; c = c->next)
					EXPECT_TRUE(screen(c).within(pr)) << c->name << " in " << w->name << " at " << s.w << "x" << s.h;
			}
			// the unplaced retail container now follows its children, it no longer clips them
			const Rect cb = screen(t.find("CenterBackground"));
			EXPECT_TRUE(screen(t.find("CommandWindow")).within(cb));
			EXPECT_TRUE(cb.within(Rect{layout.island.x, layout.island.y, layout.island.w, layout.island.h}))
				<< s.w << "x" << s.h;
		}
}

TEST(EnhancedHudInteractive, TheRetailRectangleOfCenterBackgroundIsNoLongerTheInteractiveRegion)
{
	// the defect: placing the windows (drawn correctly) without fitting their unplaced container left the 800 x 121
	// retail rectangle as the clickable region, so the upper part of the scaled top row (and everything right of 800 px
	// at larger scales) did not register. Prove the harness sees it, then that the shipped mechanism does not have it.
	int surfacesWithOldDefect = 0;
	for (const Surface &s : kSurfaces)
	{
		const Result layout = EnhancedHudLayout::compute(s.w, s.h, layoutScaleFor(s.w, s.h, DisplayGeometry::kAutoUiScale));
		if (!layout.valid)
			continue;
		Tree old;
		old.build(kBar);
		old.master()->winSetPosition(layout.island.x, layout.island.y);
		old.master()->winSetSize(layout.island.w, layout.island.h);
		int n = 0;
		const GroupWindow *group = commandGroup(n);
		for (int i = 0; i < n; ++i)
		{
			Rect dst; bool hide = false;
			if (Win *w = old.find(group[i].window))
				if (destinationFor(group[i].window, dst, hide))
					EnhancedHudWindows::placeAbsolute(w, toPhysical(layout, dst));
		}
		showContext(old, "CommandWindow", "WinUnitSelected");
		bool broken = false;
		for (int i = 1; i <= 12 && !broken; ++i)
		{
			char name[32];
			std::snprintf(name, sizeof(name), "ButtonCommand%02d", i);
			Win *b = old.find(name);
			for (const Probe &p : insideProbes(screen(b)))
				if (hitTest(old.master(), p.x, p.y) != b)
					broken = true;
		}
		if (broken)
			++surfacesWithOldDefect;

		Tree fixed;
		fixed.build(kBar);
		applyEnhanced(fixed, layout);
		showContext(fixed, "CommandWindow", "WinUnitSelected");
		for (int i = 1; i <= 12; ++i)
		{
			char name[32];
			std::snprintf(name, sizeof(name), "ButtonCommand%02d", i);
			Win *b = fixed.find(name);
			for (const Probe &p : insideProbes(screen(b)))
				EXPECT_EQ(hitTest(fixed.master(), p.x, p.y), b) << name << " " << p.what << " " << s.w << "x" << s.h;
		}
	}
	EXPECT_GT(surfacesWithOldDefect, 0);
}

TEST(EnhancedHudInteractive, SpecialPowerShortcutButtonsAreClickableWhereDrawn)
{
	for (const Surface &s : kSurfaces)
		for (int ui : kUiScales)
		{
			const double ls = layoutScaleFor(s.w, s.h, ui);
			const Result layout = EnhancedHudLayout::compute(s.w, s.h, ls);
			if (!layout.valid)
				continue;
			Tree t;
			t.build(kPowers);
			scaleLoaded(t, ls);
			// originals recorded before anything moves (the engine's remembered Original store)
			std::map<Win *, std::vector<int>> orig;
			auto original = [&orig](Win *w, int *out)
			{
				auto it = orig.find(w);
				if (it == orig.end())
					it = orig.insert({w, {w->lox, w->loy, w->w, w->h}}).first;
				for (int i = 0; i < 4; ++i)
					out[i] = it->second[i];
			};
			auto remember = [&original](Win *w) { int o[4]; original(w, o); };
			if (layout.powerHorizontal)
				EnhancedHudWindows::applyPowerRow<Win>(t.master(), layout.powerPanel, layout.scale / ls, original, remember,
					[](Win *w) { return w->name.rfind("ButtonParent", 0) == 0 ? w->name[12] - '1' : -1; });
			else
				EnhancedHudWindows::applyPowerColumn<Win>(t.master(), layout.powerPanel, layout.scale / ls, original, remember);
			EXPECT_EQ(t.master()->lox, layout.powerPanel.x);
			EXPECT_EQ(t.master()->loy, layout.powerPanel.y);
			for (int i = 1; i <= 5; ++i)
			{
				Win *b = t.find("ButtonCommand" + std::to_string(i));
				ASSERT_NE(b, nullptr);
				const Rect r = screen(b);
				if (r.w < 4 || r.h < 4)
					continue;
				for (const Probe &p : insideProbes(r))
					EXPECT_EQ(hitTest(t.master(), p.x, p.y), b)
						<< "power " << i << " " << p.what << " at " << s.w << "x" << s.h << " ui" << ui;
			}
			std::vector<Win *> all;
			collect(t.master(), all);
			for (Win *w : all)
				for (Win *c = w->child; c; c = c->next)
					EXPECT_TRUE(screen(c).within(screen(w))) << c->name;
		}
}

TEST(EnhancedHudInteractive, RestoringTheRememberedWindowsRecoversTheRetailGeometry)
{
	const Result layout = EnhancedHudLayout::compute(1920, 1200, layoutScaleFor(1920, 1200, DisplayGeometry::kAutoUiScale));
	Tree t;
	t.build(kBar);
	struct Saved { int x, y, w, h; bool hidden; };
	std::map<Win *, Saved> saved;
	EnhancedHudWindows::apply<Win>(t.master(), layout, [&t](const char *n) { return t.find(n); },
		[&saved](Win *w) { if (!saved.count(w)) saved[w] = {w->lox, w->loy, w->w, w->h, w->hidden}; },
		[](Win *w) { return isDecorative(w); });
	// everything the mechanism changed was remembered first: restoring them yields the retail tree
	Tree retail;
	retail.build(kBar);
	for (auto &kv : saved)
	{
		kv.first->lox = kv.second.x; kv.first->loy = kv.second.y;
		kv.first->w = kv.second.w;   kv.first->h = kv.second.h;
		kv.first->hidden = kv.second.hidden;
	}
	for (size_t i = 0; i < t.wins.size(); ++i)
	{
		EXPECT_EQ(t.wins[i]->lox, retail.wins[i]->lox) << t.wins[i]->name;
		EXPECT_EQ(t.wins[i]->loy, retail.wins[i]->loy) << t.wins[i]->name;
		EXPECT_EQ(t.wins[i]->w, retail.wins[i]->w) << t.wins[i]->name;
		EXPECT_EQ(t.wins[i]->h, retail.wins[i]->h) << t.wins[i]->name;
		EXPECT_EQ(t.wins[i]->hidden, retail.wins[i]->hidden) << t.wins[i]->name;
	}
}

TEST(EnhancedHudInteractive, GlobalPowersRowFallbackKeepsEveryButtonClickableAndOrdered)
{
	for (const Surface &s : kSurfaces)
		for (int ui : kUiScales)
		{
			const double ls = layoutScaleFor(s.w, s.h, ui);
			const Result layout = EnhancedHudLayout::compute(s.w, s.h, ls);
			if (!layout.valid)
				continue;
			// force the compact row regardless of what compute() chose
			const int pitch = scaledRef(kPowerColumnW, layout.scale);
			const Rect row = {20, 30, pitch * kPowerCount, scaledRef(kPowerCellH, layout.scale)};
			Tree t;
			t.build(kPowers);
			scaleLoaded(t, ls);
			std::map<Win *, std::vector<int>> orig;
			auto original = [&orig](Win *w, int *out)
			{
				auto it = orig.find(w);
				if (it == orig.end())
					it = orig.insert({w, {w->lox, w->loy, w->w, w->h}}).first;
				for (int i = 0; i < 4; ++i)
					out[i] = it->second[i];
			};
			EnhancedHudWindows::applyPowerRow<Win>(t.master(), row, layout.scale / ls, original,
				[&original](Win *w) { int o[4]; original(w, o); },
				[](Win *w) { return w->name.rfind("ButtonParent", 0) == 0 ? w->name[12] - '1' : -1; });
			Rect previous;
			for (int i = 1; i <= 5; ++i)
			{
				Win *b = t.find("ButtonCommand" + std::to_string(i));
				Win *frame = t.find("ButtonParent" + std::to_string(i));
				ASSERT_NE(b, nullptr);
				const Rect r = screen(b), fr = screen(frame);
				EXPECT_TRUE(r.within(fr)) << i;
				EXPECT_TRUE(fr.within(Rect{row.x, row.y, row.w + 2, row.h + 2})) << i << " " << s.w << "x" << s.h;
				if (i > 1)
				{
					EXPECT_GT(fr.x, previous.x);                  // left to right in slot order
					EXPECT_FALSE(fr.intersects(previous));
				}
				previous = fr;
				if (r.w < 4 || r.h < 4)
					continue;
				for (const Probe &p : insideProbes(r))
					EXPECT_EQ(hitTest(t.master(), p.x, p.y), b) << "row " << i << " " << p.what << " " << s.w << "x" << s.h << " ui" << ui;
			}
		}
}

TEST(EnhancedHudInteractive, SparseCommandContextsKeepRetailSlotsAndOnlyVisibleButtonsAreClickable)
{
	// a context with a few commands hides the other slot windows; the visible ones keep their slot (index) and full plate
	const int kSparse[][3] = {{1, 2, 0}, {1, 7, 12}, {4, 5, 11}};
	for (const Surface &s : kSurfaces)
		for (int ui : {DisplayGeometry::kAutoUiScale, 100, 200})
		{
			const Result layout = EnhancedHudLayout::compute(s.w, s.h, layoutScaleFor(s.w, s.h, ui));
			if (!layout.valid)
				continue;
			for (const auto &pick : kSparse)
			{
				Tree dense;
				dense.build(kBar);
				applyEnhanced(dense, layout);
				showContext(dense, "CommandWindow", "WinUnitSelected");
				Tree sparse;
				sparse.build(kBar);
				applyEnhanced(sparse, layout);
				showContext(sparse, "CommandWindow", "WinUnitSelected");
				for (int i = 1; i <= 12; ++i)
				{
					char name[32];
					std::snprintf(name, sizeof(name), "ButtonCommand%02d", i);
					const bool keep = i == pick[0] || i == pick[1] || i == pick[2];
					sparse.find(name)->hidden = !keep;
					const Rect a = screen(dense.find(name)), b = screen(sparse.find(name));
					// hiding other buttons never moves a slot
					EXPECT_EQ(a.x, b.x); EXPECT_EQ(a.y, b.y); EXPECT_EQ(a.w, b.w); EXPECT_EQ(a.h, b.h);
					Win *hit = hitTest(sparse.master(), a.x + a.w / 2, a.y + a.h / 2);
					if (keep)
					{
						for (const Probe &p : insideProbes(b))
							EXPECT_EQ(hitTest(sparse.master(), p.x, p.y), sparse.find(name)) << name << " " << p.what;
					}
					else
						EXPECT_NE(hit, sparse.find(name)) << name << " must not react while hidden";
				}
			}
		}
}
