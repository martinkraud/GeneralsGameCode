/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// UI.2: the Modern focus / navigation model (Common/ModernFocus.h).

#include <gtest/gtest.h>
#include <vector>
#include "Common/ModernFocus.h"

using namespace ModernFocus;

namespace
{
// a menu column: ids 10..14 top to bottom
std::vector<Item> column(int n, int firstId = 10)
{
	std::vector<Item> v;
	for (int i = 0; i < n; ++i) { Item it; it.id = firstId + i; it.rect = { 100, 100 + i * 50, 200, 40 }; v.push_back(it); }
	return v;
}
Ring ringOf(const std::vector<Item> &items) { Ring r; r.setItems(items.data(), (int)items.size()); return r; }
}

TEST(ModernFocus, TabOrderIsTheListingOrderAndWraps)
{
	Ring r = ringOf(column(4));
	EXPECT_FALSE(r.hasFocus());
	EXPECT_TRUE(r.next());     EXPECT_EQ(r.focusedId(), 10);
	EXPECT_TRUE(r.next());     EXPECT_EQ(r.focusedId(), 11);
	r.focusId(13);
	EXPECT_TRUE(r.next());     EXPECT_EQ(r.focusedId(), 10);   // wrap
	EXPECT_TRUE(r.previous()); EXPECT_EQ(r.focusedId(), 13);   // wrap back
}

TEST(ModernFocus, DisabledAndHiddenItemsNeverEnterTraversal)
{
	std::vector<Item> v = column(5);
	v[1].enabled = false; v[3].visible = false;
	Ring r = ringOf(v);
	r.focusFirst();
	std::vector<int> seen;
	for (int i = 0; i < 6; ++i) { seen.push_back(r.focusedId()); r.next(); }
	for (int id : seen) { EXPECT_NE(id, 11); EXPECT_NE(id, 13); }
	EXPECT_FALSE(r.focusId(11));
	EXPECT_FALSE(r.focusId(13));
	r.focusId(12);
	r.move(Direction::Up);   EXPECT_EQ(r.focusedId(), 10);   // skips the disabled 11
	r.move(Direction::Down); EXPECT_EQ(r.focusedId(), 12);
	r.move(Direction::Down); EXPECT_EQ(r.focusedId(), 14);   // skips the hidden 13
	EXPECT_EQ(r.hit(150, 100 + 1 * 50 + 10), kNone);          // pointer over the disabled item
	EXPECT_EQ(r.hit(150, 100 + 3 * 50 + 10), kNone);          // and over the hidden one
}

TEST(ModernFocus, NothingFocusableGivesNoFocusAndNoActivation)
{
	std::vector<Item> v = column(3);
	for (Item &i : v) i.enabled = false;
	Ring r = ringOf(v);
	EXPECT_FALSE(r.focusFirst());
	EXPECT_FALSE(r.next());
	EXPECT_FALSE(r.move(Direction::Down));
	EXPECT_EQ(r.activate(), kNone);
}

TEST(ModernFocus, DirectionalMovesInAColumnWrapVertically)
{
	Ring r = ringOf(column(4));
	r.focusFirst();
	EXPECT_TRUE(r.move(Direction::Down));  EXPECT_EQ(r.focusedId(), 11);
	EXPECT_TRUE(r.move(Direction::Up));    EXPECT_EQ(r.focusedId(), 10);
	EXPECT_TRUE(r.move(Direction::Up));    EXPECT_EQ(r.focusedId(), 13);   // wraps to the bottom
	EXPECT_TRUE(r.move(Direction::Down));  EXPECT_EQ(r.focusedId(), 10);   // and back to the top
	EXPECT_FALSE(r.move(Direction::Left)); EXPECT_EQ(r.focusedId(), 10);   // nothing to the left: stays
	EXPECT_FALSE(r.move(Direction::Right));
}

TEST(ModernFocus, GeometricRuleIsNotTiedToFixedCoordinates)
{
	// the same 2x2 grid at two different scales and offsets gives the same moves
	for (int scale : { 1, 3 })
		for (int offset : { 0, 500 })
		{
			std::vector<Item> v(4);
			for (int i = 0; i < 4; ++i) { v[i].id = i; v[i].rect = { offset + (i % 2) * 120 * scale, offset + (i / 2) * 60 * scale, 100 * scale, 40 * scale }; }
			Ring r = ringOf(v);
			r.focusId(0);
			r.move(Direction::Right); EXPECT_EQ(r.focusedId(), 1);
			r.move(Direction::Down);  EXPECT_EQ(r.focusedId(), 3);
			r.move(Direction::Left);  EXPECT_EQ(r.focusedId(), 2);
			r.move(Direction::Up);    EXPECT_EQ(r.focusedId(), 0);
		}
}

TEST(ModernFocus, ExplicitNeighboursOverrideGeometryAndCanBlock)
{
	std::vector<Item> v = column(3);
	v[0].nav[(int)Direction::Down] = 12;                 // explicit: from 10, Down goes to 12
	v[2].nav[(int)Direction::Down] = kBlocked;           // explicit: 12 does not wrap
	Ring r = ringOf(v);
	r.focusId(10);
	EXPECT_TRUE(r.move(Direction::Down));  EXPECT_EQ(r.focusedId(), 12);
	EXPECT_FALSE(r.move(Direction::Down)); EXPECT_EQ(r.focusedId(), 12);
	// an unusable explicit target falls back to the geometric rule
	v[1].enabled = true; v[2].enabled = false;
	Ring r2 = ringOf(v);
	r2.focusId(10);
	EXPECT_TRUE(r2.move(Direction::Down)); EXPECT_EQ(r2.focusedId(), 11);
}

TEST(ModernFocus, PointerHoverFocusesTheItemUnderIt)
{
	Ring r = ringOf(column(4));
	EXPECT_EQ(r.hit(150, 100 + 2 * 50 + 5), 12);
	EXPECT_EQ(r.hit(150, 100 + 2 * 50 + 45), kNone);     // the gap between items
	EXPECT_EQ(r.hit(50, 120), kNone);                    // left of the column
	EXPECT_TRUE(r.focusPoint(150, 100 + 3 * 50 + 5));
	EXPECT_EQ(r.focusedId(), 13);
	EXPECT_EQ(r.activate(), 13);
	EXPECT_FALSE(r.focusPoint(0, 0));
	EXPECT_EQ(r.focusedId(), 13);                        // leaving all items keeps the focus
}

TEST(ModernFocus, SetItemsKeepsTheFocusedItemWhenStillFocusable)
{
	std::vector<Item> v = column(4);
	Ring r = ringOf(v);
	r.focusId(12);
	v[0].rect.y += 7;                                    // a relayout moved things
	r.setItems(v.data(), (int)v.size());
	EXPECT_EQ(r.focusedId(), 12);
	v[2].enabled = false;                                // the focused item became disabled
	r.setItems(v.data(), (int)v.size());
	EXPECT_FALSE(r.hasFocus());
	EXPECT_TRUE(r.focusFirst());
}

TEST(ModernFocus, NavigatorRestoresFocusWhenReturningFromAChildPage)
{
	Navigator nav;
	nav.reset(0);
	std::vector<Item> root = column(5);
	nav.ring.setItems(root.data(), (int)root.size());
	nav.ring.focusId(12);                                // the user is on the third entry

	EXPECT_TRUE(nav.enter(1));                           // opens a child page
	EXPECT_EQ(nav.page(), 1);
	std::vector<Item> child = column(3, 100);
	nav.ring.setItems(child.data(), (int)child.size());
	nav.ring.focusFirst();
	EXPECT_TRUE(nav.enter(2));                           // and a grandchild
	std::vector<Item> grand = column(2, 200);
	nav.ring.setItems(grand.data(), (int)grand.size());
	nav.ring.focusFirst();
	nav.ring.next();                                     // second item focused

	EXPECT_TRUE(nav.back());                             // back to the child page
	nav.ring.setItems(child.data(), (int)child.size());
	EXPECT_TRUE(nav.restoreFocus());
	EXPECT_EQ(nav.ring.focusedId(), 100);                // the child page's own first item

	EXPECT_TRUE(nav.back());                             // back to the root
	nav.ring.setItems(root.data(), (int)root.size());
	EXPECT_TRUE(nav.restoreFocus());
	EXPECT_EQ(nav.ring.focusedId(), 12);                 // the entry the user left from
	EXPECT_TRUE(nav.atRoot());
	EXPECT_FALSE(nav.back());                            // Escape at the root does nothing
}

TEST(ModernFocus, RestoreFallsBackToTheFirstFocusableItem)
{
	Navigator nav;
	nav.reset(0);
	std::vector<Item> root = column(3);
	nav.ring.setItems(root.data(), (int)root.size());
	nav.ring.focusId(11);
	nav.enter(1);
	nav.back();
	root[1].enabled = false;                             // the remembered entry is no longer available
	nav.ring.setItems(root.data(), (int)root.size());
	EXPECT_TRUE(nav.restoreFocus());
	EXPECT_EQ(nav.ring.focusedId(), 10);
}

TEST(ModernFocus, NavigatorDepthIsBounded)
{
	Navigator nav;
	nav.reset(0);
	int entered = 0;
	for (int i = 0; i < 20; ++i) if (nav.enter(i + 1)) ++entered;
	EXPECT_EQ(entered, kMaxPages - 1);
	EXPECT_EQ(nav.depth(), kMaxPages);
}

TEST(ModernFocus, TheSameCommandSequenceAlwaysGivesTheSameFocus)
{
	for (int run = 0; run < 3; ++run)
	{
		std::vector<Item> v = column(6);
		v[2].enabled = false;
		Ring r = ringOf(v);
		r.focusFirst();
		const Direction seq[] = { Direction::Down, Direction::Down, Direction::Down, Direction::Up, Direction::Down, Direction::Down, Direction::Down, Direction::Down };
		std::vector<int> trace;
		for (Direction d : seq) { r.move(d); trace.push_back(r.focusedId()); }
		const std::vector<int> expect = { 11, 13, 14, 13, 14, 15, 10, 11 };
		EXPECT_EQ(trace, expect);
	}
}
