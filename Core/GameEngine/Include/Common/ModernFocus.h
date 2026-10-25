// SPDX-License-Identifier: GPL-3.0-or-later
// UI.2 Modern Experience focus / navigation model (pure, engine-free, allocation-free, deterministic).
//
// Retail has no keyboard navigation model (docs/modernization/MODERN_UI_ARCHAEOLOGY.md section 5). This is the model the Modern screens use. It knows nothing about windows, pixels of a
// particular resolution or input devices: a screen hands it an ordered list of items (id, rectangle, enabled, visible, optional explicit neighbours) and feeds it abstract commands
// (next / previous / directional / activate / back / point). Mouse, keyboard and - later - a controller all map onto the same commands, so adding a device does not touch the model.
//
//   * deterministic focus order = the order the screen listed its items (the tab order);
//   * disabled or hidden items are never focusable and are skipped by every command;
//   * directional moves use the explicit neighbour when the screen declared one, otherwise a geometric rule on the item centres (no fixed coordinates);
//   * a Navigator keeps a stack of pages (screens inside a screen): entering a page remembers the focused item of the page it leaves, back() restores it.
#pragma once

#include <cstdint>
#include <type_traits>

namespace ModernFocus
{
constexpr int kMaxItems = 24;
constexpr int kMaxPages = 8;
constexpr int kNone = -1;      // no item / no page / "use the geometric rule"
constexpr int kBlocked = -2;   // explicit neighbour value: do not move in this direction

enum class Direction : std::uint8_t { Up = 0, Down = 1, Left = 2, Right = 3 };

struct Rect
{
	int x = 0, y = 0, w = 0, h = 0;
	int centerX() const { return x + w / 2; }
	int centerY() const { return y + h / 2; }
	bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct Item
{
	int id = 0;                       // screen-defined, unique inside the ring, >= 0
	Rect rect;
	bool enabled = true;
	bool visible = true;
	int nav[4] = { kNone, kNone, kNone, kNone };   // explicit neighbour item ids per Direction (kNone = geometric rule, kBlocked = stop)
	bool focusable() const { return enabled && visible && rect.w > 0 && rect.h > 0; }
};

class Ring
{
public:
	void clear() { m_count = 0; m_focus = kNone; }
	// Replaces the items. Keeps the focus on the same item id when it is still focusable, otherwise drops it (the screen then calls focusFirst() / restore()).
	void setItems(const Item *items, int count)
	{
		const int keepId = focusedId();
		m_count = count < 0 ? 0 : (count > kMaxItems ? kMaxItems : count);
		for (int i = 0; i < m_count; ++i) m_items[i] = items[i];
		m_focus = kNone;
		if (keepId != kNone) focusId(keepId);
	}
	int count() const { return m_count; }
	const Item &item(int index) const { return m_items[index]; }
	int focusedIndex() const { return m_focus; }
	int focusedId() const { return m_focus == kNone ? kNone : m_items[m_focus].id; }
	bool hasFocus() const { return m_focus != kNone; }
	int indexOf(int id) const { for (int i = 0; i < m_count; ++i) if (m_items[i].id == id) return i; return kNone; }

	bool focusId(int id)
	{
		const int i = indexOf(id);
		if (i == kNone || !m_items[i].focusable()) return false;
		m_focus = i;
		return true;
	}
	bool focusFirst()
	{
		for (int i = 0; i < m_count; ++i) if (m_items[i].focusable()) { m_focus = i; return true; }
		m_focus = kNone;
		return false;
	}
	bool focusLast()
	{
		for (int i = m_count - 1; i >= 0; --i) if (m_items[i].focusable()) { m_focus = i; return true; }
		m_focus = kNone;
		return false;
	}
	// Tab order. Wraps. With no focus, next() takes the first and previous() the last focusable item.
	bool next() { return step(+1); }
	bool previous() { return step(-1); }

	bool move(Direction d)
	{
		if (m_focus == kNone) return d == Direction::Up ? focusLast() : focusFirst();
		const Item &cur = m_items[m_focus];
		const int explicitId = cur.nav[static_cast<int>(d)];
		if (explicitId == kBlocked) return false;
		if (explicitId != kNone && focusId(explicitId)) return true;   // an unusable explicit target falls back to the geometric rule
		const int best = nearest(d);
		if (best != kNone) { m_focus = best; return true; }
		// menus are columns: moving past either end wraps to the other end
		if (d == Direction::Down || d == Direction::Up) return wrapVertical(d);
		return false;
	}

	// Pointer position: the focusable item under it, or kNone. Hover = focus is the caller's policy: focusPoint() applies it.
	int hit(int px, int py) const
	{
		for (int i = 0; i < m_count; ++i) if (m_items[i].focusable() && m_items[i].rect.contains(px, py)) return m_items[i].id;
		return kNone;
	}
	bool focusPoint(int px, int py) { const int id = hit(px, py); return id != kNone && focusId(id); }

	// Enter / confirm: the id to activate, or kNone when nothing focusable has the focus.
	int activate() const { return (m_focus != kNone && m_items[m_focus].focusable()) ? m_items[m_focus].id : kNone; }

private:
	bool step(int dir)
	{
		if (m_count == 0) return false;
		int i = m_focus == kNone ? (dir > 0 ? -1 : 0) : m_focus;
		for (int n = 0; n < m_count; ++n)
		{
			i = (i + dir + m_count) % m_count;
			if (m_items[i].focusable()) { m_focus = i; return true; }
		}
		return false;
	}
	// geometric rule: nearest centre in the direction; the primary distance counts once, the sideways offset twice; ties go to the lower index
	int nearest(Direction d) const
	{
		const Item &cur = m_items[m_focus];
		const int cx = cur.rect.centerX(), cy = cur.rect.centerY();
		int best = kNone;
		long long bestScore = 0;
		for (int i = 0; i < m_count; ++i)
		{
			if (i == m_focus || !m_items[i].focusable()) continue;
			const int dx = m_items[i].rect.centerX() - cx, dy = m_items[i].rect.centerY() - cy;
			long long primary = 0, side = 0;
			switch (d)
			{
			case Direction::Up:    primary = -dy; side = dx < 0 ? -dx : dx; break;
			case Direction::Down:  primary = dy;  side = dx < 0 ? -dx : dx; break;
			case Direction::Left:  primary = -dx; side = dy < 0 ? -dy : dy; break;
			case Direction::Right: primary = dx;  side = dy < 0 ? -dy : dy; break;
			}
			if (primary <= 0) continue;
			const long long score = primary + 2 * side;
			if (best == kNone || score < bestScore) { best = i; bestScore = score; }
		}
		return best;
	}
	bool wrapVertical(Direction d)
	{
		// the focusable item that is furthest in the opposite direction (top for Down, bottom for Up); lower index wins ties
		int pick = kNone;
		for (int i = 0; i < m_count; ++i)
		{
			if (!m_items[i].focusable()) continue;
			if (pick == kNone) { pick = i; continue; }
			const int cy = m_items[i].rect.centerY(), py = m_items[pick].rect.centerY();
			if (d == Direction::Down ? cy < py : cy > py) pick = i;
		}
		if (pick == kNone || pick == m_focus) return false;
		m_focus = pick;
		return true;
	}

	Item m_items[kMaxItems];
	int m_count = 0;
	int m_focus = kNone;
};

// A stack of pages (a main menu inside a main menu). The screen owns what a page contains; the navigator owns where the user is and what was focused on the way in.
class Navigator
{
public:
	void reset(int rootPage)
	{
		m_depth = 1;
		m_pages[0] = rootPage;
		m_saved[0] = kNone;
		m_pendingRestore = kNone;
		ring.clear();
	}
	int depth() const { return m_depth; }
	int page() const { return m_depth > 0 ? m_pages[m_depth - 1] : kNone; }
	bool atRoot() const { return m_depth <= 1; }

	// Enter a child page: remembers the focused item of the page being left.
	bool enter(int pageId)
	{
		if (m_depth <= 0 || m_depth >= kMaxPages) return false;
		m_saved[m_depth - 1] = ring.focusedId();
		m_pages[m_depth] = pageId;
		m_saved[m_depth] = kNone;
		++m_depth;
		m_pendingRestore = kNone;
		ring.clear();
		return true;
	}
	// Leave the current page (Escape / Back). The caller rebuilds the ring of the page it returns to, then calls restoreFocus().
	bool back()
	{
		if (m_depth <= 1) return false;
		--m_depth;
		m_pendingRestore = m_saved[m_depth - 1];
		ring.clear();
		return true;
	}
	// After the screen has set the page's items: focus the remembered item when it is still focusable, otherwise the first focusable one.
	bool restoreFocus()
	{
		const int want = m_pendingRestore;
		m_pendingRestore = kNone;
		if (want != kNone && ring.focusId(want)) return true;
		return ring.focusFirst();
	}
	// Remembered focus of the page below the current one (what back() will restore). Exposed for tests / persistence across screen re-creation.
	int savedFocus(int depthIndex) const { return (depthIndex >= 0 && depthIndex < m_depth) ? m_saved[depthIndex] : kNone; }

	Ring ring;

private:
	int m_pages[kMaxPages] = {};
	int m_saved[kMaxPages] = {};
	int m_depth = 0;
	int m_pendingRestore = kNone;
};

static_assert(std::is_trivially_copyable<Item>::value && std::is_trivially_copyable<Ring>::value && std::is_trivially_copyable<Navigator>::value, "plain data only");
} // namespace ModernFocus
