// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 FIX1: geometry of the Skirmish "Match Options" block (pure, testable).
//
// Zero Hour's Skirmish screen evolved the Generals one by putting match rules on a row under the player grid
// (Game Speed, Starting Money, Limit Superweapons; the Battle Honors statistics moved 7 px down to make room).
// Generals keeps its own left column: the retired Game Speed header becomes the "Match Options" header and the rows
// below it are label + control pairs on the grid's 24 px rhythm. Two rows fit exactly where the retail Slower/Faster
// captions were, once the honors statistics sit 7 reference px lower, which is also what Zero Hour did:
//
//   header  (57,315) 156x24   "Match Options"
//   row 0   (57,339)          Starting Cash (100 wide)  [ combo, 130 wide ]
//   row 1   (57,363)          reserved: match rules the player explicitly chooses (e.g. a superweapon limit)
//   honors  statistics block starts at y = 387 (retail 380)
//
// Every number is a reference unit multiplied by `scale` (the live ratio of the header's height to 24).
#pragma once

#include <cmath>

namespace MatchOptionsLayout
{

constexpr int kRowCapacity = 2;

struct Rect { int x = 0, y = 0, w = 0, h = 0; int right() const { return x + w; } int bottom() const { return y + h; } };

struct Layout
{
	Rect header;
	Rect label[kRowCapacity];
	Rect control[kRowCapacity];
	int honorsShift = 0;            // move the Battle Honors statistics block down by this much
	Rect honorsBlockBefore;         // where that block sits before the shift (to find its windows)
	Rect remnantBlock;              // the retired Game Speed slider/caption block (to hide leftovers)
};

inline int scaled(int reference, double scale) { return static_cast<int>(std::floor(reference * scale + 0.5)); }

inline Layout compute(int headerX, int headerY, int headerW, int headerH, double scale)
{
	Layout l;
	l.header = {headerX, headerY, headerW, headerH};
	const int rowH = scaled(24, scale);
	// "Starting Cash" must stay on one line: Zero Hour gave its caption 93 units; the label column is 100 here.
	const int labelW = scaled(100, scale);
	const int gap = scaled(2, scale);
	const int controlW = scaled(130, scale);
	for (int i = 0; i < kRowCapacity; ++i)
	{
		const int y = headerY + headerH + rowH * i;
		l.label[i] = {headerX, y, labelW, rowH};
		l.control[i] = {headerX + labelW + gap, y, controlW, rowH};
	}
	l.honorsShift = scaled(7, scale);
	// Retail statistics panel: x 58..291, y 380..493 for a header at (57,315).
	l.honorsBlockBefore = {headerX, headerY + scaled(65, scale) - 1, scaled(240, scale), scaled(115, scale)};
	// Retired Game Speed block: header column plus the Faster caption 75 px to its right, from the slider's top
	// (header y + 25) down to 12 px below the slider (header y + 52).
	l.remnantBlock = {headerX - scaled(4, scale), headerY + scaled(25, scale),
	                  headerW + scaled(140, scale) + scaled(4, scale), scaled(41, scale)};
	return l;
}

}  // namespace MatchOptionsLayout
