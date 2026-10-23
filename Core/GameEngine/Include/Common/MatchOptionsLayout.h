// SPDX-License-Identifier: GPL-3.0-or-later
// Geometry of the Skirmish "Match Options" controls (pure, testable). X64.33 FIX1 introduced the block; X64.52A completes it for THREE options
// (Starting Cash, AI Behaviour, Superweapons) so that nothing overlaps at any resolution or UI scale.
//
// Both titles lay the options out from the retail windows' OWN scaled coordinates, never from absolute pixels, so one set of rules holds at every window size and UI scale.
//
// Generals (one column, under the retired Game Speed header, which becomes the "Match Options" header). Reference units at UI scale 1.0 (the grid's row height is 24):
//
//   header   (57,315) 156x24      "Match Options"
//   row 0    Starting Cash   label 100 + gap 2 + combo 130, 24 high, flush under the header
//   row 1    AI Behaviour    (same columns, next 24)
//   row 2    Superweapons    (same columns, next 24)
//   gap      6
//   stats    the Battle Honors statistics panel (retail 113 high) is squeezed vertically so that it ends where it always ended (+7, Zero Hour's own shift):
//            its top is the third row's bottom + the gap, its rows keep their order and are 0.73 of their retail pitch (the text is 24 high, the pitch stays above 34 px at 1080p)
//
// Zero Hour (one band, in the place of the retired Game Speed slider, between the player grid and the statistics): three label + combo pairs of equal width over the grid's own
// left and right edges, one row of the grid's height:
//
//   Starting Cash [ combo ]   AI Behaviour [ combo ]   Superweapons [ combo ]
//
// Every number is a reference unit multiplied by `scale` (the live ratio of the grid row height to 24).
#pragma once

#include <cmath>

namespace MatchOptionsLayout
{

constexpr int kRowCapacity = 3;

struct Rect { int x = 0, y = 0, w = 0, h = 0; int right() const { return x + w; } int bottom() const { return y + h; } };

inline int scaled(int reference, double scale) { return static_cast<int>(std::floor(reference * scale + 0.5)); }

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Generals: one column of rows + the squeezed statistics panel
// ---------------------------------------------------------------------------------------------------------------------------------------------
struct Layout
{
	Rect header;
	Rect label[kRowCapacity];
	Rect control[kRowCapacity];
	Rect statsFind;           // where the statistics panel's windows are found (retail position, before any change)
	int statsOldTop = 0;      // retail top / bottom of the panel frame (parent coordinates of the header)
	int statsOldBottom = 0;
	int statsNewTop = 0;      // squeezed panel: starts below the last row, ends where it ended before
	int statsNewBottom = 0;
	Rect remnantBlock;        // the retired Game Speed slider/caption block (to hide leftovers)

	// the squeeze maps a retail y inside the panel to its new y; heights scale by the same factor
	double squeeze() const { return statsOldBottom > statsOldTop ? double(statsNewBottom - statsNewTop) / double(statsOldBottom - statsOldTop) : 1.0; }
	int mapY(int y) const { return statsNewTop + static_cast<int>(std::floor((y - statsOldTop) * squeeze() + 0.5)); }
	int mapH(int h) const { const int v = static_cast<int>(std::floor(h * squeeze() + 0.5)); return v < 1 ? 1 : v; }
};

inline Layout compute(int headerX, int headerY, int headerW, int headerH, double scale)
{
	Layout l;
	l.header = {headerX, headerY, headerW, headerH};
	const int rowH = scaled(24, scale);
	// "Starting Cash" and "Superweapons" must stay on one line: the label column is 100 units.
	const int labelW = scaled(100, scale);
	const int gap = scaled(2, scale);
	const int controlW = scaled(130, scale);
	for (int i = 0; i < kRowCapacity; ++i)
	{
		const int y = headerY + headerH + rowH * i;
		l.label[i] = {headerX, y, labelW, rowH};
		l.control[i] = {headerX + labelW + gap, y, controlW, rowH};
	}
	// Retail statistics panel: x 58..291, frame y 65..178 below the header's top (the frame is 113 high).
	l.statsFind = {headerX, headerY + scaled(65, scale) - 1, scaled(240, scale), scaled(115, scale)};
	l.statsOldTop = headerY + scaled(65, scale);
	l.statsOldBottom = headerY + scaled(65 + 113, scale);
	l.statsNewTop = l.label[kRowCapacity - 1].bottom() + scaled(6, scale);
	l.statsNewBottom = l.statsOldBottom + scaled(7, scale);
	// Retired Game Speed block: header column plus the Faster caption 75 px to its right, from the slider's top
	// (header y + 25) down to 12 px below the slider (header y + 52).
	l.remnantBlock = {headerX - scaled(4, scale), headerY + scaled(25, scale),
	                  headerW + scaled(140, scale) + scaled(4, scale), scaled(41, scale)};
	return l;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Zero Hour: one band of three label + combo pairs
// ---------------------------------------------------------------------------------------------------------------------------------------------
struct Band
{
	Rect label[kRowCapacity];
	Rect control[kRowCapacity];
};

// left / right: the grid's own left and right edges; y / rowH: the band's row (the retail Starting Cash row)
inline Band computeBand(int left, int right, int y, int rowH, double scale)
{
	Band b;
	const int gutter = scaled(24, scale);
	const int gap = scaled(4, scale);
	const int total = right - left;
	const int pair = (total - gutter * (kRowCapacity - 1)) / kRowCapacity;
	const int labelW = static_cast<int>(std::floor(pair * 0.46 + 0.5));
	for (int i = 0; i < kRowCapacity; ++i)
	{
		const int x = left + i * (pair + gutter);
		b.label[i] = {x, y, labelW, rowH};
		b.control[i] = {x + labelW + gap, y, pair - labelW - gap, rowH};
	}
	return b;
}

// Gadget geometry shared by every combo box of the panel: the entry field fills the combo except for a square-ish drop-down button (half the row height wide), the list
// opens directly below the combo with the combo's width.
struct ComboParts
{
	Rect entry;
	Rect button;
	Rect list;
};
inline ComboParts comboParts(const Rect &combo, int listRows)
{
	ComboParts p;
	const int buttonW = combo.h / 2;
	p.entry = {0, 0, combo.w - buttonW, combo.h};
	p.button = {combo.w - buttonW, 0, buttonW, combo.h};
	p.list = {0, combo.h, combo.w, combo.h * listRows};
	return p;
}

}  // namespace MatchOptionsLayout
