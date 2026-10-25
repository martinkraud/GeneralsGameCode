// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 FIX2: geometry of the Modern Experience Options categories (pure, testable).
//
// The retail Options screen is one dense 800x600 page. Modern settings do not fit it: native review of FIX1 showed
// text leaving its controls and a crowded Video block. Options is now organised in categories; every category is a
// column of label + control rows in one content rectangle below a row of tabs. All numbers are reference units
// multiplied by the live UI scale; a page never grows with the surface (a settings panel does not need to span the whole
// surface) and nothing here knows a monitor.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>

namespace OptionsPageLayout
{

enum class Category { Display, Graphics, Audio, Controls, Gameplay, Modern, Network, Count };   // Modern: the Modern Experience features (UI.3 category)
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

// Modern Options is its own composition (FIX4): a centred panel whose width is derived from what its tab strip
// needs, not the 2003 frame. Reference units, panel-relative. The frame itself is a plain filled/bordered window
// (the retail OptionsMenuParentOld is a colour rectangle with a gold border), so any width is safe.
constexpr int kPanelRefW = 760;      // preferred width (a deliberately wide modern dialog); grows to kPanelMaxW if captions need more
constexpr int kPanelMaxW = 780;      // canvas is 800 wide
constexpr int kPanelRefH = 568;
constexpr int kPanelMargin = 36;
constexpr int kTabPad = 14;          // horizontal padding inside a tab (each side)
constexpr int kTabGap = 14;          // gap between tabs
constexpr int kTabMinW = 72;
// panel-relative reference rectangles at the preferred width
constexpr Rect kTabStrip = {36, 66, 688, 30};
constexpr Rect kContent = {36, 108, 688, 380};

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
	{"ModernModeLabel", "ModernMode", ControlKind::Combo},
	{"ModernMonitorLabel", "ModernMonitor", ControlKind::Combo},
	{"ResolutionLabel", "ComboBoxResolution", ControlKind::Combo},
	{"ModernUiScaleLabel", "ModernUiScale", ControlKind::Combo},
	{"ModernVsyncLabel", "ModernVsync", ControlKind::Combo},
	{"FrameRateLimitLabel", "ComboBoxFrameRateLimit", ControlKind::Combo},
};
// Gamma is intentionally not offered: the D3D11 backend has no gamma stage (D3D11Backend::Set_Gamma is Unsupported) and the retail
// path never touches the desktop in windowed presentation, so the slider would change nothing (SETTINGS_COMPLETION_AUDIT.md, F).
constexpr PageItem kGraphicsItems[] = {
	{"DetailLabel", "ComboBoxDetail", ControlKind::Combo},
	{"ModernRenderScaleLabel", "ModernRenderScale", ControlKind::Combo},      // X64.37 v3: internal 3D resolution, live everywhere
};
constexpr PageItem kAudioItems[] = {
	{"SFXVolumeLabel", "SliderSFXVolume", ControlKind::Slider},
	{"MusicVolumeLabel", "SliderMusicVolume", ControlKind::Slider},
	{"VoiceVolumeLabel", "SliderVoiceVolume", ControlKind::Slider},
};
constexpr PageItem kControlsItems[] = {
	{nullptr, "CheckAlternateMouse", ControlKind::Check},
	{"ScrollSpeedLabel", "SliderScrollSpeed", ControlKind::Slider},
	{nullptr, "CheckBoxDrawAnchor", ControlKind::Check},     // retail options that Modern Options used to hide: all have live handlers
	{nullptr, "CheckBoxMoveAnchor", ControlKind::Check},
};
// Checkboxes first, dropdowns last: a dropdown opens downwards over the rows below it. (The language filter checkbox is not offered:
// its text token GUI:LanguageFilter does not exist in the Generals vocabulary and nothing in Generals reads the preference.)
constexpr PageItem kGameplayItems[] = {
	{nullptr, "CheckBoxUseCamera", ControlKind::Check},
	{nullptr, "CheckBoxSaveCamera", ControlKind::Check},
};
// Modern Experience: what the Modern Engine adds on top of the retail game (UI.3 groups it in its own category).
constexpr PageItem kModernItems[] = {
	{"ModernMotionLabel", "ModernMotion", ControlKind::Combo},
	{"ModernInterfaceLabel", "ModernInterface", ControlKind::Combo},   // Original | Enhanced (USA, China, GLA)
};

// Retail checkboxes that Modern Options restored. In the retail layout they are children of the Options root, not of the panel that
// hosts the page's other controls; a sibling of the host panel that overlaps an open dropdown takes its clicks (X64.37 native finding:
// "Enhanced" could not be selected). They are moved into the host panel of their page so every control of a page shares one parent.
struct HostedControl { const char *control; const char *hostPanel; };
constexpr HostedControl kReparented[] = {
	{"CheckBoxUseCamera", "VideoParent"},
	{"CheckBoxSaveCamera", "VideoParent"},
	{"CheckBoxDrawAnchor", "ScrollParent"},
	{"CheckBoxMoveAnchor", "ScrollParent"},
};

// Controls that cannot change while a match is running (they need the main menu): they are disabled AND their label says so.
// Lifecycle reasons (SETTINGS_COMPLETION_AUDIT.md section 10): resolution, mode, monitor and UI scale change the geometry the in-game
// windows were created with (ControlBar, ReplayControl, popup message ...); Unit Motion has per-object snapshots that start at match
// load; Detail is a load-time LOD. The Interface (HUD) and Render Scale are live in a match.
constexpr const char *kMainMenuOnly[] = {"ComboBoxResolution", "ModernMode", "ModernMonitor", "ModernUiScale", "ModernMotion",
                                         "ComboBoxDetail"};
inline bool mainMenuOnly(const char *control)
{
	for (const char *name : kMainMenuOnly)
		if (control && std::strcmp(name, control) == 0) return true;
	return false;
}

// The Resolution control: selectable only in a windowed main-menu session; borderless runs at the desktop size, which the control
// shows read-only with the reason in its label.
enum class ResolutionReason { None, DesktopSize, MainMenuOnly };
struct ResolutionControlState { bool enabled; ResolutionReason reason; };
inline ResolutionControlState resolutionControl(bool borderless, bool inMatch)
{
	if (inMatch) return {false, ResolutionReason::MainMenuOnly};
	if (borderless) return {false, ResolutionReason::DesktopSize};
	return {true, ResolutionReason::None};
}

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
	case Category::Modern: return {kModernItems, static_cast<int>(sizeof(kModernItems) / sizeof(kModernItems[0]))};
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
// Real font metrics differ a little between the measuring string and the drawn one (hinting, drop shadow): every
// caption is budgeted 6 % plus 2 reference units more than measured, so a glyph can never reach the tab border.
inline int safeCaptionWidth(int measured, double scale)
{
	return measured + measured * 6 / 100 + std::max(scaled(2, scale), 1);
}

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
	const int pad = scaled(kTabPad, scale), minW = scaled(kTabMinW, scale), gap = std::max(scaled(kTabGap, scale), 2);
	int want[kCategoryCount];
	int total = gap * (kCategoryCount - 1);
	for (int i = 0; i < kCategoryCount; ++i)
	{
		want[i] = std::max(safeCaptionWidth(measured[i], scale) + 2 * pad, minW);
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

// Rows are 24 reference units tall on a 44 unit pitch. Label column 38 % of the content width, control column
// next to it (at most 300 units): stable columns with room for "Borderless Fullscreen" or a long resolution string.
inline Page computePageIn(const Rect &content, double scale, int rowCount)
{
	Page p;
	p.content = content;
	p.rowCount = std::min(std::max(rowCount, 0), kMaxRows);
	const int pad = scaled(24, scale), rowH = scaled(24, scale), pitch = scaled(44, scale), top = scaled(20, scale);
	const int labelW = content.w * 38 / 100;
	const int controlX = pad + labelW + scaled(24, scale);
	const int controlW = std::min(scaled(300, scale), content.w - controlX - pad);
	for (int i = 0; i < p.rowCount; ++i)
	{
		const int y = content.y + top + pitch * i;
		p.rows[i].label = {content.x + pad, y, labelW, rowH};
		p.rows[i].control = {content.x + controlX, y, controlW, rowH};
	}
	p.fits = p.rowCount == 0 || p.rows[p.rowCount - 1].control.bottom() <= content.bottom();
	return p;
}

inline Page computePage(double scale, int rowCount, int panelX, int panelY)
{
	const Rect content = {panelX + scaled(kContent.x, scale), panelY + scaled(kContent.y, scale),
	                      scaled(kContent.w, scale), scaled(kContent.h, scale)};
	return computePageIn(content, scale, rowCount);
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
	case Category::Modern: return "GUI:OptionsTabModern";
	default: return "GUI:OptionsTabNetwork";
	}
}

// ---- the panel: everything panel-relative derives from the measured captions ------------------------------------
struct PanelGeometry
{
	Rect panel;           // the frame
	Rect container;       // the see-thru parent that owns the page windows (hit testing): the frame inflated
	Rect title, line, content;
	TabStrip tabs;
	Rect defaults, accept, back;   // bottom buttons
	Rect footer;
	bool captionsFit = false;      // every caption (with its safety budget and padding) fits its tab
};

// canvasCenterX/canvasTopY: the centre line and top of the 800x600 reference canvas on the surface.
inline PanelGeometry computePanel(const int measured[kCategoryCount], double scale, int canvasCenterX, int canvasTopY)
{
	PanelGeometry g;
	const int pad = scaled(kTabPad, scale), gap = std::max(scaled(kTabGap, scale), 2), margin = scaled(kPanelMargin, scale);
	int need = gap * (kCategoryCount - 1);
	for (int i = 0; i < kCategoryCount; ++i)
		need += std::max(safeCaptionWidth(measured[i], scale) + 2 * pad, scaled(kTabMinW, scale));
	int width = std::max(need + 2 * margin, scaled(kPanelRefW, scale));
	width = std::min(width, scaled(kPanelMaxW, scale));
	const int height = scaled(kPanelRefH, scale);
	g.panel = {canvasCenterX - width / 2, canvasTopY + scaled(600 - kPanelRefH, scale) / 2, width, height};
	const int inner = width - 2 * margin;
	g.title = {g.panel.x + margin, g.panel.y + scaled(16, scale), inner, scaled(34, scale)};
	g.line = {g.panel.x + margin, g.panel.y + scaled(54, scale), inner, std::max(scaled(2, scale), 1)};
	g.content = {g.panel.x + margin, g.panel.y + scaled(108, scale), inner, scaled(380, scale)};
	g.tabs = computeTabStrip(measured, scale, g.panel.x + margin, g.panel.y + scaled(66, scale), inner, scaled(30, scale));
	g.captionsFit = g.tabs.padded;
	const int bw = scaled(156, scale), bh = scaled(32, scale), by = g.content.bottom() + scaled(14, scale);
	g.back = {g.panel.right() - margin - bw, by, bw, bh};
	g.accept = {g.back.x - scaled(16, scale) - bw, by, bw, bh};
	g.defaults = {g.panel.x + margin, by, bw, bh};
	g.footer = {g.panel.x + margin, by + bh + scaled(8, scale), inner, scaled(20, scale)};
	const int inflate = scaled(8, scale);
	g.container = {g.panel.x - inflate, g.panel.y - inflate, g.panel.w + 2 * inflate, g.panel.h + 2 * inflate};
	return g;
}

// The retail Advanced Display panel (485 x 492 reference units) while it is open: centred horizontally in the Modern panel and below
// the title rule, so it never touches the title or the tab strip (which are hidden while it is open). Clamped inside the panel.
inline Rect advancedPopupRect(const PanelGeometry &g, int popupW, int popupH, double scale)
{
	Rect r;
	r.w = popupW;
	r.h = popupH;
	r.x = g.panel.x + (g.panel.w - popupW) / 2;
	const int minY = g.line.bottom() + scaled(6, scale), maxBottom = g.panel.bottom() - scaled(8, scale);
	r.y = minY;
	if (r.y + popupH > maxBottom)
		r.y = maxBottom - popupH;
	return r;
}

}  // namespace OptionsPageLayout
