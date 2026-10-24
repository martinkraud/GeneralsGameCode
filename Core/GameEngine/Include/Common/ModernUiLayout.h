// SPDX-License-Identifier: GPL-3.0-or-later
// UI.1 Modern Experience UI foundation: a pure, allocation-free responsive layout core (rows, columns, constraints, weights, scrolling, content-width clamp).
//
// Why: every Modern screen so far (Options categories, Match Options, the Enhanced HUD) has its own hand-written geometry header. That worked, but each new option meant
// hunting for room in a fixed 800x600 retail layout. This core lets a screen DECLARE a tree of containers and leaves with constraints; the solver places them. Adding a row
// never changes the rectangles of the rows before it, nothing is an absolute pixel, and the same declaration is valid at every resolution and UI scale.
//
// Scope of this header: geometry only. No engine types, no windows, no drawing, no input, no heap allocation (fixed capacity), deterministic, thread-safe (no statics).
// Wiring to GameWindow trees, focus order and drawing is the job of the UI.2+ milestones (docs/modernization/MODERN_UI_ARCHITECTURE.md).
//
// Units: node sizes are REFERENCE units (the 800x600 canvas every retail layout was authored on); solve() multiplies them by the UI scale (percent) and rounds.
//
//   container axis      Column: children top -> bottom        Row: children left -> right
//   preferred / min / max   per axis, 0 max = unbounded
//   weight              share of the free main-axis space (0 = keeps its preferred size)
//   cross alignment     Start / Center / Stretch on the other axis
//   scroll              a container whose content may exceed its rectangle: reported as scrollable instead of overflowing
#pragma once

#include <cstdint>
#include <type_traits>

namespace ModernUi
{
constexpr int kMaxNodes = 96;
constexpr int kNone = -1;

struct Size { int w = 0, h = 0; };
struct Rect
{
	int x = 0, y = 0, w = 0, h = 0;
	int right() const { return x + w; }
	int bottom() const { return y + h; }
	bool contains(const Rect &o) const { return o.x >= x && o.y >= y && o.right() <= right() && o.bottom() <= bottom(); }
	bool overlaps(const Rect &o) const { return x < o.right() && right() > o.x && y < o.bottom() && bottom() > o.y && w > 0 && h > 0 && o.w > 0 && o.h > 0; }
};
struct Insets { int l = 0, t = 0, r = 0, b = 0; };

enum class Axis : std::uint8_t { Column, Row };
enum class Cross : std::uint8_t { Start, Center, Stretch };

struct Node
{
	// ---- declared (reference units) ----
	Axis axis = Axis::Column;            // how THIS node lays out its children
	Cross cross = Cross::Stretch;        // how children sit on the other axis
	int prefW = 0, prefH = 0;            // preferred size (a container with children and prefW/H = 0 sizes to its content)
	int minW = 0, minH = 0;
	int maxW = 0, maxH = 0;              // 0 = unbounded
	int weight = 0;                      // main-axis flex share inside the parent
	Insets padding;
	int gap = 0;                         // between children
	bool scroll = false;                 // content may exceed the rectangle along the main axis
	// ---- tree ----
	int parent = kNone;
	int firstChild = kNone, lastChild = kNone, next = kNone;
};

struct Result
{
	Rect rect[kMaxNodes];                // solved physical rectangles (valid after solve())
	Size content[kMaxNodes];             // scaled content size of each container (what its children need)
	bool scrollable[kMaxNodes] = {};     // container.scroll && content larger than rect along the main axis
	bool overflow = false;               // a non-scrolling container could not hold its children even at their minimum sizes
};

class Tree
{
public:
	int count() const { return m_count; }
	const Node &node(int i) const { return m_nodes[i]; }
	Node &node(int i) { return m_nodes[i]; }

	// Adds a node below `parent` (kNone for the root, only once). Returns its index or kNone when the tree is full.
	int add(int parent, const Node &n)
	{
		if (m_count >= kMaxNodes) return kNone;
		const int i = m_count++;
		m_nodes[i] = n;
		m_nodes[i].parent = parent;
		m_nodes[i].firstChild = m_nodes[i].lastChild = m_nodes[i].next = kNone;
		if (parent != kNone)
		{
			Node &p = m_nodes[parent];
			if (p.lastChild == kNone) p.firstChild = i; else m_nodes[p.lastChild].next = i;
			p.lastChild = i;
		}
		return i;
	}

private:
	Node m_nodes[kMaxNodes];
	int m_count = 0;
};

namespace detail
{
inline int scaled(int reference, int scalePercent) { return (reference * scalePercent + (reference >= 0 ? 50 : -50)) / 100; }
inline int clampTo(int v, int lo, int hi) { if (hi > 0 && v > hi) v = hi; return v < lo ? lo : v; }

struct Measure { int pw, ph, minw, minh; };

// bottom-up preferred / minimum sizes in physical pixels
inline Measure measure(const Tree &t, int i, int s, Measure *cache)
{
	const Node &n = t.node(i);
	const int padW = scaled(n.padding.l + n.padding.r, s), padH = scaled(n.padding.t + n.padding.b, s);
	int cw = 0, ch = 0, cminw = 0, cminh = 0, kids = 0;
	for (int c = n.firstChild; c != kNone; c = t.node(c).next)
	{
		const Measure m = measure(t, c, s, cache);
		const int g = kids ? scaled(n.gap, s) : 0;
		if (n.axis == Axis::Column) { ch += m.ph + g; cminh += m.minh + g; if (m.pw > cw) cw = m.pw; if (m.minw > cminw) cminw = m.minw; }
		else                        { cw += m.pw + g; cminw += m.minw + g; if (m.ph > ch) ch = m.ph; if (m.minh > cminh) cminh = m.minh; }
		++kids;
	}
	Measure r;
	r.pw = clampTo(n.prefW ? scaled(n.prefW, s) : cw + padW, scaled(n.minW, s), n.maxW ? scaled(n.maxW, s) : 0);
	r.ph = clampTo(n.prefH ? scaled(n.prefH, s) : ch + padH, scaled(n.minH, s), n.maxH ? scaled(n.maxH, s) : 0);
	// minimum: what the children need at their minimum (a scrolling container only needs its declared minimum along its main axis)
	const int declMinW = scaled(n.minW, s), declMinH = scaled(n.minH, s);
	r.minw = (n.scroll && n.axis == Axis::Row) ? declMinW : (cminw + padW > declMinW ? cminw + padW : declMinW);
	r.minh = (n.scroll && n.axis == Axis::Column) ? declMinH : (cminh + padH > declMinH ? cminh + padH : declMinH);
	if (r.minw > r.pw) r.pw = r.minw;
	if (r.minh > r.ph) r.ph = r.minh;
	cache[i] = r;
	return r;
}

inline void arrange(const Tree &t, int i, const Rect &rect, int s, const Measure *cache, Result &out)
{
	const Node &n = t.node(i);
	out.rect[i] = rect;
	Rect inner{ rect.x + scaled(n.padding.l, s), rect.y + scaled(n.padding.t, s),
		rect.w - scaled(n.padding.l + n.padding.r, s), rect.h - scaled(n.padding.t + n.padding.b, s) };
	if (inner.w < 0) inner.w = 0;
	if (inner.h < 0) inner.h = 0;
	const bool column = n.axis == Axis::Column;
	const int mainAvail = column ? inner.h : inner.w;
	const int gap = scaled(n.gap, s);

	int kids = 0, preferredSum = 0, minSum = 0, weightSum = 0;
	for (int c = n.firstChild; c != kNone; c = t.node(c).next)
	{
		const Measure &m = cache[c];
		preferredSum += column ? m.ph : m.pw;
		minSum += column ? m.minh : m.minw;
		weightSum += t.node(c).weight;
		++kids;
	}
	const int gaps = kids > 1 ? gap * (kids - 1) : 0;
	out.content[i] = column ? Size{ inner.w, preferredSum + gaps } : Size{ preferredSum + gaps, inner.h };

	// main-axis sizes: start from preferred; grow weighted children into free space; shrink everything toward its minimum when short
	int sizes[kMaxNodes];
	int idx[kMaxNodes];
	int k = 0;
	for (int c = n.firstChild; c != kNone; c = t.node(c).next) { idx[k] = c; sizes[k] = column ? cache[c].ph : cache[c].pw; ++k; }
	int free_ = mainAvail - (preferredSum + gaps);
	if (free_ > 0 && weightSum > 0)
	{
		// distribute by weight, honouring each child's maximum; repeat while someone hit a cap and space remains
		bool capped[kMaxNodes] = {};
		for (int pass = 0; pass < 4 && free_ > 0; ++pass)
		{
			int ws = 0;
			for (int j = 0; j < k; ++j) if (!capped[j]) ws += t.node(idx[j]).weight;
			if (!ws) break;
			int given = 0;
			for (int j = 0; j < k; ++j)
			{
				const Node &cn = t.node(idx[j]);
				if (capped[j] || !cn.weight) continue;
				int add = free_ * cn.weight / ws;
				const int cap = column ? (cn.maxH ? scaled(cn.maxH, s) : 0) : (cn.maxW ? scaled(cn.maxW, s) : 0);
				if (cap && sizes[j] + add >= cap) { add = cap - sizes[j]; capped[j] = true; }
				sizes[j] += add; given += add;
			}
			free_ -= given;
			if (!given) break;
		}
	}
	else if (free_ < 0)
	{
		const bool scrollMain = n.scroll;
		if (!scrollMain)
		{
			int need = -free_;
			// shrink in proportion to the room above each child's minimum
			int room = 0;
			for (int j = 0; j < k; ++j) room += sizes[j] - (column ? cache[idx[j]].minh : cache[idx[j]].minw);
			if (room > 0)
			{
				const int take = need < room ? need : room;
				int taken = 0;
				for (int j = 0; j < k; ++j)
				{
					const int r = sizes[j] - (column ? cache[idx[j]].minh : cache[idx[j]].minw);
					const int d = r > 0 ? take * r / room : 0;
					sizes[j] -= d; taken += d;
				}
				for (int j = 0; j < k && taken < take; ++j)   // rounding remainder
				{
					const int r = sizes[j] - (column ? cache[idx[j]].minh : cache[idx[j]].minw);
					if (r > 0) { --sizes[j]; ++taken; }
				}
				need -= taken;
			}
			if (need > 0) out.overflow = true;
		}
	}

	int pos = column ? inner.y : inner.x;
	int used = 0;
	for (int j = 0; j < k; ++j)
	{
		const int c = idx[j];
		const Node &cn = t.node(c);
		const Measure &m = cache[c];
		const int crossAvail = column ? inner.w : inner.h;
		int crossSize = column ? m.pw : m.ph;
		if (n.cross == Cross::Stretch) crossSize = crossAvail;
		const int crossMax = column ? (cn.maxW ? scaled(cn.maxW, s) : 0) : (cn.maxH ? scaled(cn.maxH, s) : 0);
		if (crossMax && crossSize > crossMax) crossSize = crossMax;
		if (crossSize > crossAvail && !n.scroll) crossSize = crossAvail;
		int crossPos = column ? inner.x : inner.y;
		if (n.cross == Cross::Center) crossPos += (crossAvail - crossSize) / 2;
		Rect r = column ? Rect{ crossPos, pos, crossSize, sizes[j] } : Rect{ pos, crossPos, sizes[j], crossSize };
		arrange(t, c, r, s, cache, out);
		pos += sizes[j] + gap;
		used += sizes[j] + (j ? gap : 0);
	}
	out.scrollable[i] = n.scroll && used > mainAvail;
}
} // namespace detail

// Solves the tree into `out`. Root rectangle = `available`. Returns false on an empty tree.
inline bool solve(const Tree &tree, const Rect &available, int uiScalePercent, Result &out)
{
	if (tree.count() <= 0) return false;
	out.overflow = false;
	for (int i = 0; i < tree.count(); ++i) { out.scrollable[i] = false; out.content[i] = Size{}; }
	detail::Measure cache[kMaxNodes];
	detail::measure(tree, 0, uiScalePercent > 0 ? uiScalePercent : 100, cache);
	detail::arrange(tree, 0, available, uiScalePercent > 0 ? uiScalePercent : 100, cache, out);
	return true;
}

// The content rectangle of a screen on a surface of any shape: at most `maxContentWidthRef` reference units wide (times the UI scale), centred, inside a safe margin.
// A 5120x1440 surface therefore gets the same content width as a 2560x1440 one; backgrounds and decoration may still use the whole surface.
inline Rect contentRect(const Size &surface, int maxContentWidthRef, int uiScalePercent, int safeMarginRef = 16)
{
	const int s = uiScalePercent > 0 ? uiScalePercent : 100;
	const int margin = detail::scaled(safeMarginRef, s);
	int w = surface.w - 2 * margin;
	const int cap = detail::scaled(maxContentWidthRef, s);
	if (cap > 0 && w > cap) w = cap;
	if (w < 0) w = 0;
	int h = surface.h - 2 * margin;
	if (h < 0) h = 0;
	return Rect{ (surface.w - w) / 2, margin, w, h };
}

// ---- validation (used by tests and by the layout audit; never needed at run time) ----
enum Issue : unsigned { IssueNone = 0, IssueOverlap = 1, IssueOutsideParent = 2, IssueBelowMinimum = 4, IssueOverflow = 8 };

inline unsigned validate(const Tree &t, const Result &r)
{
	unsigned issues = r.overflow ? IssueOverflow : IssueNone;
	for (int i = 0; i < t.count(); ++i)
	{
		const Node &n = t.node(i);
		for (int c = n.firstChild; c != kNone; c = t.node(c).next)
		{
			if (!n.scroll && !r.rect[i].contains(r.rect[c])) issues |= IssueOutsideParent;
			for (int d = t.node(c).next; d != kNone; d = t.node(d).next)
				if (r.rect[c].overlaps(r.rect[d])) issues |= IssueOverlap;
		}
	}
	return issues;
}

static_assert(std::is_trivially_copyable<Node>::value && std::is_trivially_copyable<Tree>::value && std::is_trivially_copyable<Result>::value,
	"the layout core must stay allocation free: plain data only");
} // namespace ModernUi
