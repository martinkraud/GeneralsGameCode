// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 FIX2: geometry of the Modern Experience Options categories (pure, testable).
//
// The retail Options screen is one dense 800x600 page. Modern settings do not fit it: native review of FIX1 showed
// text leaving its controls and a crowded Video block. Options is now organised in categories; every category is a
// column of label + control rows in one content rectangle below a row of tabs. All numbers are reference units
// multiplied by the live UI scale; a page never grows with the surface (a settings panel does not need to be 5120 px
// wide) and nothing here knows a monitor.
#pragma once

#include <algorithm>
#include <cmath>

namespace OptionsPageLayout
{

enum class Category { Display, Graphics, Audio, Controls, Gameplay, Network, Count };
constexpr int kCategoryCount = static_cast<int>(Category::Count);
constexpr int kMaxRows = 10;

struct Rect
{
	int x = 0, y = 0, w = 0, h = 0;
	int right() const { return x + w; }
	int bottom() const { return y + h; }
	bool intersects(const Rect &o) const { return x < o.right() && o.x < right() && y < o.bottom() && o.y < bottom(); }
	bool inside(const Rect &o) const { return x >= o.x && y >= o.y && right() <= o.right() && bottom() <= o.bottom(); }
};

// Reference rectangles on the Options canvas (the retail 800x600 frame).
constexpr Rect kTabStrip = {151, 64, 485, 28};
constexpr Rect kContent = {151, 104, 485, 408};      // above the Defaults / Accept / Cancel buttons at y 528

inline int scaled(int reference, double scale) { return static_cast<int>(std::floor(reference * scale + 0.5)); }

struct Row
{
	Rect label;       // caption (left column)
	Rect control;     // combo / slider / checkbox (right column; a checkbox spans both columns)
};

struct Page
{
	Rect content;
	int rowCount = 0;
	Row rows[kMaxRows];
	bool fits = false;
};

struct Tabs
{
	Rect strip;
	Rect tab[kCategoryCount];
};

// Rows are 24 reference units tall on a 44 unit pitch: 8 rows fit with room for the dropdown of the last row to
// open inside the content area. Label column 200, control column 225, 20 unit margins.
inline Page computePage(double scale, int rowCount, int originX, int originY)
{
	Page p;
	p.content = {originX + scaled(kContent.x, scale), originY + scaled(kContent.y, scale),
	             scaled(kContent.w, scale), scaled(kContent.h, scale)};
	p.rowCount = std::min(std::max(rowCount, 0), kMaxRows);
	const int pad = scaled(20, scale), labelW = scaled(200, scale), controlX = scaled(240, scale);
	const int controlW = scaled(225, scale), rowH = scaled(24, scale), pitch = scaled(44, scale), top = scaled(20, scale);
	for (int i = 0; i < p.rowCount; ++i)
	{
		const int y = p.content.y + top + pitch * i;
		p.rows[i].label = {p.content.x + pad, y, labelW, rowH};
		p.rows[i].control = {p.content.x + controlX, y, controlW, rowH};
	}
	// a dropdown of the last row opens downward by up to ~6 entries of (row height): it must stay inside the content
	const int lastBottom = p.rowCount ? p.rows[p.rowCount - 1].control.bottom() + rowH * 6 : p.content.y;
	p.fits = p.rowCount == 0 || p.rows[p.rowCount - 1].control.bottom() <= p.content.bottom();
	(void)lastBottom;
	return p;
}

inline Tabs computeTabs(double scale, int originX, int originY)
{
	Tabs t;
	t.strip = {originX + scaled(kTabStrip.x, scale), originY + scaled(kTabStrip.y, scale),
	           scaled(kTabStrip.w, scale), scaled(kTabStrip.h, scale)};
	const int gap = std::max(scaled(3, scale), 1);
	const int total = t.strip.w - gap * (kCategoryCount - 1);
	const int each = total / kCategoryCount;
	int x = t.strip.x;
	for (int i = 0; i < kCategoryCount; ++i)
	{
		const int w = (i == kCategoryCount - 1) ? t.strip.right() - x : each;
		t.tab[i] = {x, t.strip.y, w, t.strip.h};
		x += w + gap;
	}
	return t;
}

// Tab captions are short, upper-case words of the original Generals vocabulary.
inline const char *categoryTextKey(Category c)
{
	switch (c)
	{
	case Category::Display: return "GUI:OptionsTabDisplay";
	case Category::Graphics: return "GUI:OptionsTabGraphics";
	case Category::Audio: return "GUI:OptionsTabAudio";
	case Category::Controls: return "GUI:OptionsTabControls";
	case Category::Gameplay: return "GUI:OptionsTabGameplay";
	default: return "GUI:OptionsTabNetwork";
	}
}

}  // namespace OptionsPageLayout
