// SPDX-License-Identifier: GPL-3.0-or-later
// Enhanced gameplay HUD (USA prototype): the shared window placement mechanism.
//
// Templated over a window type with the GameWindow position API (winGetParent/winGetChild/winGetNext,
// winGet/SetPosition, winGet/SetSize, winGetScreenPosition, winHide) so that the engine (GameWindow) and the unit tests
// (a mock with the same semantics as GameWindow::winPointInChild) run exactly the same code.
//
// Why this exists: GameWindow hit testing descends the window tree. A child can only be hit when the point is inside
// every ancestor's region, and positions are parent relative. Moving a window to a screen rectangle (placeAbsolute)
// keeps what is *drawn* correct whatever its ancestors are, but the interactive region is the intersection with all
// ancestors. ControlBar.wnd nests the command windows and buttons below `CenterBackground` (an 800 x 121 art
// container) which the layout does not place; left at its 800 px rectangle it clipped the scaled grid. After placement
// every container therefore has to contain its children (fitContainers) so the drawn and the interactive geometry agree.
#pragma once

#include "Common/EnhancedHudLayout.h"

#include <set>
#include <vector>

namespace EnhancedHudWindows
{

using EnhancedHudLayout::Rect;

template <class Win>
Rect screenRect(Win *win)
{
	int x = 0, y = 0, w = 0, h = 0;
	win->winGetScreenPosition(&x, &y);
	win->winGetSize(&w, &h);
	return {x, y, w, h};
}

// Moves/sizes a window so that its screen rectangle is `rect` (positions are parent relative).
template <class Win>
void placeAbsolute(Win *win, const Rect &rect)
{
	if (!win)
		return;
	int px = 0, py = 0;
	if (Win *parent = win->winGetParent())
		parent->winGetScreenPosition(&px, &py);
	win->winSetPosition(rect.x - px, rect.y - py);
	win->winSetSize(rect.w, rect.h);
}

inline Rect unionOf(const Rect &a, const Rect &b)
{
	const int x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
	const int x1 = std::max(a.right(), b.right()), y1 = std::max(a.bottom(), b.bottom());
	return {x0, y0, x1 - x0, y1 - y0};
}

// Post-order: afterwards every window below `master` (except `master`) contains all of its children on the screen.
// A container the layout placed keeps its rectangle (extended if needed); an unplaced container is replaced by the
// bounding box of its children (its retail rectangle was authored for the 800 x 600 band and is meaningless now).
// Children keep their screen rectangles: only the container region changes.
template <class Win, class Remember>
void fitContainers(Win *win, Win *master, const std::set<Win *> &placed, Remember &remember)
{
	for (Win *child = win->winGetChild(); child; child = child->winGetNext())
		fitContainers(child, master, placed, remember);
	if (win == master || win->winGetChild() == nullptr)
		return;

	bool any = false;
	Rect bounds;
	for (Win *child = win->winGetChild(); child; child = child->winGetNext())
	{
		const Rect r = screenRect(child);
		bounds = any ? unionOf(bounds, r) : r;
		any = true;
	}
	if (!any)
		return;

	const Rect own = screenRect(win);
	const Rect target = placed.count(win) ? unionOf(own, bounds) : bounds;
	if (target.x == own.x && target.y == own.y && target.w == own.w && target.h == own.h)
		return;

	remember(win);
	for (Win *child = win->winGetChild(); child; child = child->winGetNext())
		remember(child);

	// children are parent relative: keep their screen rectangles while the container moves
	const int dx = target.x - own.x, dy = target.y - own.y;
	int px = 0, py = 0;
	if (Win *parent = win->winGetParent())
		parent->winGetScreenPosition(&px, &py);
	win->winSetPosition(target.x - px, target.y - py);
	win->winSetSize(target.w, target.h);
	if (dx != 0 || dy != 0)
	{
		for (Win *child = win->winGetChild(); child; child = child->winGetNext())
		{
			int cx = 0, cy = 0;
			child->winGetPosition(&cx, &cy);
			child->winSetPosition(cx - dx, cy - dy);
		}
	}
}

template <class Win>
void collectSubtree(Win *win, std::set<Win *> &out)
{
	out.insert(win);
	for (Win *child = win->winGetChild(); child; child = child->winGetNext())
		collectSubtree(child, out);
}

// Scales the descendants of `win` around their parent origin by `factor` from their remembered originals
// (original(w, int[4]) -> {x, y, w, h}, parent relative; also remembers the window). Idempotent.
template <class Win, class Original>
void scaleSubtree(Win *win, double factor, Original &original)
{
	for (Win *child = win->winGetChild(); child; child = child->winGetNext())
	{
		int o[4] = {0, 0, 0, 0};
		original(child, o);
		child->winSetPosition(static_cast<int>(o[0] * factor + 0.5), static_cast<int>(o[1] * factor + 0.5));
		child->winSetSize(static_cast<int>(o[2] * factor + 0.5), static_cast<int>(o[3] * factor + 0.5));
		scaleSubtree(child, factor, original);
	}
}

// The special power shortcut column: the whole layout is moved to `column` and scaled uniformly by `factor`; rounding
// can leave a scaled child a pixel outside its scaled parent, so the (all placed) containers are extended afterwards.
template <class Win, class Original, class Remember>
void applyPowerColumn(Win *parent, const Rect &column, double factor, Original &&original, Remember &&remember)
{
	int o[4] = {0, 0, 0, 0};
	original(parent, o);
	parent->winSetPosition(column.x, column.y);
	parent->winSetSize(static_cast<int>(o[2] * factor + 0.5), static_cast<int>(o[3] * factor + 0.5));
	scaleSubtree(parent, factor, original);
	std::set<Win *> all;
	collectSubtree(parent, all);
	fitContainers(parent, parent, all, remember);
}

// The compact fallback of the Global Powers surface: the direct children of `parent` (slotOf(child) = 0..4, -1 for none)
// are laid out in a row with `row.w / 5` pitch instead of the retail column; every child keeps its retail size scaled by
// `factor` and its own subtree keeps its retail relationships. Same remember/fit rules as applyPowerColumn.
template <class Win, class Original, class Remember, class SlotOf>
void applyPowerRow(Win *parent, const Rect &row, double factor, Original &&original, Remember &&remember, SlotOf &&slotOf)
{
	int o[4] = {0, 0, 0, 0};
	original(parent, o);
	parent->winSetPosition(row.x, row.y);
	parent->winSetSize(row.w, row.h);
	const int pitch = row.w / EnhancedHudLayout::kPowerCount;
	for (Win *child = parent->winGetChild(); child; child = child->winGetNext())
	{
		int c[4] = {0, 0, 0, 0};
		original(child, c);
		const int slot = std::max(0, slotOf(child));
		child->winSetPosition(slot * pitch, 0);
		child->winSetSize(static_cast<int>(c[2] * factor + 0.5), static_cast<int>(c[3] * factor + 0.5));
		scaleSubtree(child, factor, original);
	}
	std::set<Win *> all;
	collectSubtree(parent, all);
	fitContainers(parent, parent, all, remember);
}

// Places every window of the layout tables below `master` (the island), then makes the tree consistent.
//   find(name)   -> window of ControlBar.wnd without the prefix, or nullptr
//   remember(w)  -> called before a window is changed (first call wins; used to restore Original exactly)
//   decorative(w)-> childless retail art windows (authored for the 800 px band) that have no Enhanced counterpart: they
//                   are hidden so they can neither draw nor take clicks outside the island
template <class Win, class Find, class Remember, class Decorative>
void apply(Win *master, const EnhancedHudLayout::Result &layout, Find &&find, Remember &&remember,
           Decorative &&decorative)
{
	std::set<Win *> placed;

	// Descendants of the command containers that have no table entry (beacon text, cancel-construction button, the
	// observer lists, the OCL timer ...) follow their container: they are moved with the same mapping as the grid.
	// Their retail screen rectangles must be read before anything moves.
	struct Extra { Win *win; Rect retail; };
	std::vector<Extra> extras;
	{
		std::set<Win *> tabled;
		int n = 0;
		for (const EnhancedHudLayout::NamedPlacement *p = EnhancedHudLayout::placements(n); n-- > 0; ++p)
			if (Win *w = find(p->window))
				tabled.insert(w);
		for (const EnhancedHudLayout::GroupWindow *g = EnhancedHudLayout::infoGroup(n); n-- > 0; ++g)
			if (Win *w = find(g->window))
				tabled.insert(w);
		for (const EnhancedHudLayout::GroupWindow *g = EnhancedHudLayout::commandGroup(n); n-- > 0; ++g)
			if (Win *w = find(g->window))
				tabled.insert(w);
		n = 0;
		for (const EnhancedHudLayout::GroupWindow *g = EnhancedHudLayout::commandGroup(n); n-- > 0; ++g)
		{
			Win *container = find(g->window);
			if (!container || std::strstr(g->window, "Button") != nullptr)
				continue;
			std::set<Win *> subtree;
			collectSubtree(container, subtree);
			for (Win *w : subtree)
				if (w != container && !tabled.count(w))
					extras.push_back({w, screenRect(w)});
		}
	}

	remember(master);
	master->winSetPosition(layout.island.x, layout.island.y);
	master->winSetSize(layout.island.w, layout.island.h);
	placed.insert(master);

	int count = 0;
	const EnhancedHudLayout::NamedPlacement *table = EnhancedHudLayout::placements(count);
	for (int i = 0; i < count; ++i)
	{
		Win *win = find(table[i].window);
		if (!win)
			continue;
		remember(win);
		if (table[i].hide)
		{
			win->winHide(true);
			continue;
		}
		placeAbsolute(win, EnhancedHudLayout::toPhysical(layout, table[i].dst));
		placed.insert(win);
	}

	// the info group and the command group: containers are listed before their children
	for (int g = 0; g < 2; ++g)
	{
		const EnhancedHudLayout::GroupWindow *group = g == 0 ? EnhancedHudLayout::infoGroup(count)
		                                                     : EnhancedHudLayout::commandGroup(count);
		for (int i = 0; i < count; ++i)
		{
			EnhancedHudLayout::Rect dst;
			bool hide = false;
			if (!EnhancedHudLayout::destinationFor(group[i].window, dst, hide))
				continue;
			Win *win = find(group[i].window);
			if (!win)
				continue;
			remember(win);
			placeAbsolute(win, EnhancedHudLayout::toPhysical(layout, dst));
			placed.insert(win);
		}
	}

	for (const Extra &e : extras)
	{
		remember(e.win);
		placeAbsolute(e.win, EnhancedHudLayout::toPhysical(layout, EnhancedHudLayout::commandMap(e.retail)));
		placed.insert(e.win);
	}

	for (Win *child = master->winGetChild(); child; child = child->winGetNext())
	{
		if (child->winGetChild() != nullptr || child->winIsHidden() || !decorative(child))
			continue;
		remember(child);
		child->winHide(true);
	}

	// containers the layout does not place (CenterBackground ...) must follow their children
	fitContainers(master, master, placed, remember);
}

}  // namespace EnhancedHudWindows
