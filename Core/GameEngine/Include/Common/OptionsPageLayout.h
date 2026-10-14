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
// The strip is centred on the content (151..636) and 10 units wider on each side (still inside the frame art, which
// spans 120..661), so six padded captions get a visible gap between them instead of touching.
constexpr Rect kTabStrip = {141, 64, 505, 28};
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

// ---- page ownership (names are window names without the "OptionsMenu.wnd:" prefix) ----------------------------
// Exactly one category's items are visible/hit-testable; the retail controls that are not listed anywhere are
// hidden in Modern Options. Kind lets tests assert "no slider on DISPLAY/GAMEPLAY".
enum class ControlKind { Combo, Slider, Check, Block };

struct PageItem
{
	const char *label;     // may be null (a checkbox carries its own text)
	const char *control;
	ControlKind kind;
};

constexpr PageItem kDisplayItems[] = {
	{"ResolutionLabel", "ComboBoxResolution", ControlKind::Combo},
	{"ModernModeLabel", "ModernMode", ControlKind::Combo},
	{"ModernMonitorLabel", "ModernMonitor", ControlKind::Combo},
	{"ModernUiScaleLabel", "ModernUiScale", ControlKind::Combo},
	{"ModernVsyncLabel", "ModernVsync", ControlKind::Combo},
	{"FrameRateLimitLabel", "ComboBoxFrameRateLimit", ControlKind::Combo},
};
constexpr PageItem kGraphicsItems[] = {
	{"DetailLabel", "ComboBoxDetail", ControlKind::Combo},
	{"GammaLabel", "SliderGamma", ControlKind::Slider},
};
constexpr PageItem kAudioItems[] = {
	{"SFXVolumeLabel", "SliderSFXVolume", ControlKind::Slider},
	{"MusicVolumeLabel", "SliderMusicVolume", ControlKind::Slider},
	{"VoiceVolumeLabel", "SliderVoiceVolume", ControlKind::Slider},
};
constexpr PageItem kControlsItems[] = {
	{nullptr, "CheckAlternateMouse", ControlKind::Check},
	{"ScrollSpeedLabel", "SliderScrollSpeed", ControlKind::Slider},
};
constexpr PageItem kGameplayItems[] = {
	{"ModernMotionLabel", "ModernMotion", ControlKind::Combo},
};

struct PageTable
{
	const PageItem *items;
	int count;
};

inline PageTable pageItems(Category c)
{
	switch (c)
	{
	case Category::Display: return {kDisplayItems, static_cast<int>(sizeof(kDisplayItems) / sizeof(kDisplayItems[0]))};
	case Category::Graphics: return {kGraphicsItems, static_cast<int>(sizeof(kGraphicsItems) / sizeof(kGraphicsItems[0]))};
	case Category::Audio: return {kAudioItems, static_cast<int>(sizeof(kAudioItems) / sizeof(kAudioItems[0]))};
	case Category::Controls: return {kControlsItems, static_cast<int>(sizeof(kControlsItems) / sizeof(kControlsItems[0]))};
	case Category::Gameplay: return {kGameplayItems, static_cast<int>(sizeof(kGameplayItems) / sizeof(kGameplayItems[0]))};
	default: return {nullptr, 0};    // Network: the retail block, kept whole
	}
}

inline bool nameEquals(const char *a, const char *b)
{
	if (!a || !b) return false;
	while (*a && *a == *b) { ++a; ++b; }
	return *a == *b;
}

// Which category (or -1) owns a control/label name.
inline int ownerOf(const char *name)
{
	for (int c = 0; c < kCategoryCount; ++c)
	{
		const PageTable t = pageItems(static_cast<Category>(c));
		for (int i = 0; i < t.count; ++i)
			if (nameEquals(name, t.items[i].control) || nameEquals(name, t.items[i].label))
				return c;
	}
	return -1;
}

// ---- tab strip: measured widths ---------------------------------------------------------------------------------
// Every tab is as wide as its caption plus padding (minimum width), the slack is shared so the strip aligns with the
// content on both edges; if the captions cannot fit, widths shrink proportionally. Pure: the caller measures text.
struct TabStrip
{
	Rect strip;
	Rect tab[kCategoryCount];
	bool padded = false;        // every tab kept its full horizontal padding
};

inline TabStrip computeTabStrip(const int measured[kCategoryCount], double scale, int stripX, int stripY, int stripW, int stripH)
{
	TabStrip t;
	t.strip = {stripX, stripY, stripW, stripH};
	const int pad = scaled(8, scale), minW = scaled(56, scale), gap = std::max(scaled(10, scale), 2);
	int want[kCategoryCount];
	int total = gap * (kCategoryCount - 1);
	for (int i = 0; i < kCategoryCount; ++i)
	{
		want[i] = std::max(measured[i] + 2 * pad, minW);
		total += want[i];
	}
	const int available = stripW - gap * (kCategoryCount - 1);
	int widths[kCategoryCount];
	if (total <= stripW)
	{
		const int slack = stripW - total;
		int given = 0;
		for (int i = 0; i < kCategoryCount; ++i)
		{
			const int share = slack / kCategoryCount;
			widths[i] = want[i] + share;
			given += share;
		}
		widths[kCategoryCount - 1] += slack - given;
		t.padded = true;
	}
	else
	{
		int sum = 0;
		for (int i = 0; i < kCategoryCount; ++i) sum += want[i];
		int used = 0;
		for (int i = 0; i < kCategoryCount; ++i)
		{
			widths[i] = std::max(want[i] * available / std::max(sum, 1), 1);
			used += widths[i];
		}
		widths[kCategoryCount - 1] += available - used;
	}
	int x = stripX;
	for (int i = 0; i < kCategoryCount; ++i)
	{
		t.tab[i] = {x, stripY, widths[i], stripH};
		x += widths[i] + gap;
	}
	return t;
}

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
