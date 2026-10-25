// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Frontend screen kit, part 1 (pure, engine-free): the geometry of a "settings screen" - a title, a left category rail, a scrollable column of label + control rows, a status line and a
// bottom action row (Defaults | Apply, Back). Options uses it; any screen that is "pages of rows" (Match Rules, a future Controls page) gets the same composition and the same guarantees:
//   * the panel is composed inside a centred 16:9 box (ultrawide keeps the shell map visible on both sides of it, it is never stretched);
//   * the UI scale is reduced (never clipped) until the rail, four rows and the action row fit the surface height;
//   * rows are generated from a count, never from absolute positions: adding a row changes no earlier rectangle; rows beyond the viewport scroll.
// Units: reference units * (effective scale / 100), rounded. Rectangles are absolute surface pixels.
#pragma once

#include <cstdint>

#include "Common/ModernTheme.h"

namespace ModernScreen
{
struct Rect
{
	int x = 0, y = 0, w = 0, h = 0;
	int right() const { return x + w; }
	int bottom() const { return y + h; }
	bool contains(const Rect &o) const { return o.x >= x && o.y >= y && o.right() <= right() && o.bottom() <= bottom(); }
	bool overlaps(const Rect &o) const { return w > 0 && h > 0 && o.w > 0 && o.h > 0 && x < o.right() && right() > o.x && y < o.bottom() && bottom() > o.y; }
	bool hit(int px, int py) const { return px >= x && px < right() && py >= y && py < bottom(); }
};

constexpr int kMaxTabs = 8;
constexpr int kMaxRows = 24;          // rows that can be on screen at once (the viewport); more rows scroll

struct Input
{
	int surfaceW = 0, surfaceH = 0;
	int uiPercent = 100;
	int tabCount = 0;
	int rowCount = 0;                 // rows of the active page
	int firstRow = 0;                 // scroll position (clamped)
	int minVisibleRows = 4;           // the scale is reduced until at least this many rows (or all of them, if fewer) are visible
};

struct Layout
{
	Rect composition, panel;
	Rect title, titleRule;
	Rect rail, tab[kMaxTabs];
	Rect content;                      // the row viewport
	Rect row[kMaxRows], label[kMaxRows], control[kMaxRows];   // the visible rows: index i is row (firstRow + i)
	int visibleRows = 0, firstRow = 0, rowCount = 0;
	bool scrollable = false;
	Rect scrollTrack, scrollThumb;     // only when scrollable
	Rect status;                       // dirty / help line
	Rect defaults, apply, back;
	int effectiveScale = 100;
	bool compact = false;
};

namespace detail
{
inline unsigned &computeCounter() { static unsigned n = 0; return n; }   // instrumentation: how many times any screen geometry was solved (it must grow with layout events, not with frames)
inline int sc(int ref, int percent) { return (ref * percent + 50) / 100; }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
}

inline unsigned computeCount() { return detail::computeCounter(); }

inline int maxFirstRow(int rowCount, int visibleRows) { return rowCount > visibleRows ? rowCount - visibleRows : 0; }
inline int clampFirstRow(int firstRow, int rowCount, int visibleRows) { return detail::clampi(firstRow, 0, maxFirstRow(rowCount, visibleRows)); }

// The row that must be scrolled into view for `row` to be visible (keeps the focused row on screen).
inline int firstRowToReveal(int row, int firstRow, int rowCount, int visibleRows)
{
	if (row < firstRow) firstRow = row;
	else if (row >= firstRow + visibleRows) firstRow = row - visibleRows + 1;
	return clampFirstRow(firstRow, rowCount, visibleRows);
}

inline Layout compute(const Input &in)
{
	using namespace detail;
	++computeCounter();
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	Layout L;
	if (in.surfaceW <= 0 || in.surfaceH <= 0) return L;
	int scale = in.uiPercent > 0 ? in.uiPercent : 100;
	const int tabs = clampi(in.tabCount, 0, kMaxTabs);

	int compW = static_cast<int>(in.surfaceH * ModernTheme::kCompositionMaxAspect + 0.5);
	if (compW > in.surfaceW) compW = in.surfaceW;
	L.composition = Rect{ (in.surfaceW - compW) / 2, 0, compW, in.surfaceH };

	for (int attempt = 0; attempt < 64; ++attempt)
	{
		const int pad = sc(24, scale), gap = sc(16, scale), edge = sc(m.edgeMargin, scale);
		const int titleH = sc(34, scale), ruleH = (sc(m.ruleThickness, scale) > 0 ? sc(m.ruleThickness, scale) : 1);
		const int tabH = sc(40, scale), tabGap = sc(4, scale);
		const int rowH = sc(m.rowHeight, scale), rowGap = sc(10, scale), pitch = rowH + rowGap;
		const int actionH = sc(m.actionHeight, scale), statusH = sc(22, scale);
		const int visibleMin = in.rowCount < in.minVisibleRows ? (in.rowCount > 0 ? in.rowCount : 1) : in.minVisibleRows;
		const int railNeed = tabs > 0 ? tabs * tabH + (tabs - 1) * tabGap : 0;
		const int bodyNeed = railNeed > visibleMin * pitch ? railNeed : visibleMin * pitch;
		const int needH = pad + titleH + gap / 2 + ruleH + gap + bodyNeed + gap + statusH + gap / 2 + actionH + pad;

		const int availH = in.surfaceH - 2 * edge;
		const int prefH = sc(560, scale);
		int panelH = prefH < availH ? prefH : availH;
		if (panelH < needH) panelH = needH;
		const bool fits = needH <= availH;
		if (!fits && scale > 30) { scale = scale * 92 / 100; continue; }

		int panelW = sc(m.contentMaxWidth * 80 / 100, scale);          // settings pages are a reading column, not a canvas
		if (panelW > compW - 2 * edge) panelW = compW - 2 * edge;
		const int minPanelW = sc(520, scale);
		if (panelW < minPanelW && minPanelW <= in.surfaceW - 2 * edge) panelW = minPanelW;
		L.panel = Rect{ L.composition.x + (compW - panelW) / 2, (in.surfaceH - panelH) / 2, panelW, panelH };
		const Rect &P = L.panel;

		L.title = Rect{ P.x + pad, P.y + pad, P.w - 2 * pad, titleH };
		L.titleRule = Rect{ P.x + pad, L.title.bottom() + gap / 2, P.w - 2 * pad, ruleH };

		const int bodyTop = L.titleRule.bottom() + gap;
		const int actionsTop = P.bottom() - pad - actionH;
		L.status = Rect{ P.x + pad, actionsTop - gap / 2 - statusH, P.w - 2 * pad, statusH };
		const int bodyBottom = L.status.y - gap / 2;

		int railW = sc(m.railWidth, scale);
		if (railW > (P.w - 2 * pad) / 3) railW = (P.w - 2 * pad) / 3;
		L.rail = Rect{ P.x + pad, bodyTop, railW, bodyBottom - bodyTop };
		for (int i = 0; i < tabs; ++i)
			L.tab[i] = Rect{ L.rail.x, L.rail.y + i * (tabH + tabGap), railW, tabH };

		const int contentX = L.rail.right() + gap * 3 / 2;
		L.content = Rect{ contentX, bodyTop, P.right() - pad - contentX, bodyBottom - bodyTop };
		int visible = L.content.h / pitch;
		if (visible < 1) visible = 1;
		if (visible > kMaxRows) visible = kMaxRows;
		L.rowCount = in.rowCount < 0 ? 0 : in.rowCount;
		L.scrollable = L.rowCount > visible;
		L.visibleRows = L.rowCount < visible ? L.rowCount : visible;
		L.firstRow = clampFirstRow(in.firstRow, L.rowCount, L.visibleRows);

		const int trackW = L.scrollable ? sc(8, scale) : 0;
		const int rowsW = L.content.w - (L.scrollable ? trackW + gap / 2 : 0);
		int labelW = rowsW * 44 / 100;
		int controlW = rowsW - labelW - gap;
		const int maxCtl = sc(m.controlMaxWidth, scale), minCtl = sc(m.controlMinWidth, scale);
		if (controlW > maxCtl) { controlW = maxCtl; }
		if (controlW < minCtl) { controlW = minCtl; labelW = rowsW - controlW - gap; if (labelW < 0) labelW = 0; }
		const int controlH = sc(m.controlHeight, scale) < rowH ? sc(m.controlHeight, scale) : rowH;
		for (int i = 0; i < L.visibleRows; ++i)
		{
			const int y = L.content.y + i * pitch;
			L.row[i] = Rect{ L.content.x, y, rowsW, rowH };
			L.label[i] = Rect{ L.content.x, y, labelW, rowH };
			L.control[i] = Rect{ L.content.x + labelW + gap, y + (rowH - controlH) / 2, controlW, controlH };
		}
		if (L.scrollable)
		{
			const int trackH = L.visibleRows * pitch - rowGap;
			L.scrollTrack = Rect{ L.content.right() - trackW, L.content.y, trackW, trackH };
			const int total = L.rowCount;
			int thumbH = trackH * L.visibleRows / total;
			const int minThumb = sc(24, scale);
			if (thumbH < minThumb) thumbH = minThumb;
			const int span = trackH - thumbH, maxFirst = maxFirstRow(L.rowCount, L.visibleRows);
			L.scrollThumb = Rect{ L.scrollTrack.x, L.scrollTrack.y + (maxFirst > 0 ? span * L.firstRow / maxFirst : 0), trackW, thumbH };
		}

		const int bw = sc(160, scale);
		L.back = Rect{ P.right() - pad - bw, actionsTop, bw, actionH };
		L.apply = Rect{ L.back.x - gap - bw, actionsTop, bw, actionH };
		L.defaults = Rect{ L.content.x, actionsTop, bw, actionH };
		L.effectiveScale = scale;
		L.compact = scale < (in.uiPercent > 0 ? in.uiPercent : 100);
		return L;
	}
	return L;
}
// ---- modal dialog (confirm / exit / warning) -----------------------------------------------------------------------------------------------------------------------
// A centred panel with a title, a wrapped body and up to three buttons right-aligned in one row (the primary action is the rightmost). The caller measures the body text at
// dialogInnerWidth() and passes its height: the panel grows with the text up to the surface height (a longer text is clipped by the caller's wrapping, never the buttons).
constexpr int kMaxDialogButtons = 3;

struct DialogInput
{
	int surfaceW = 0, surfaceH = 0, uiPercent = 100;
	int buttonCount = 1;
	int bodyHeight = 0;        // measured height of the wrapped body text (pixels, at the effective scale the caller got from dialogScale())
	int extraHeight = 0;       // an extra row under the body (a text entry), reference units; 0 = none
};

struct DialogLayout
{
	Rect panel, title, titleRule, body, extra, button[kMaxDialogButtons];
	int effectiveScale = 100;
};

inline int dialogScale(int surfaceW, int surfaceH, int uiPercent)
{
	using namespace detail;
	int scale = uiPercent > 0 ? uiPercent : 100;
	// the panel (520 x ~240 reference units) must fit with margins: reduce the scale instead of clipping
	while (scale > 30 && (sc(520, scale) > surfaceW - 2 * sc(24, scale) || sc(240, scale) > surfaceH - 2 * sc(24, scale))) scale = scale * 92 / 100;
	return scale;
}

inline int dialogPanelWidth(int surfaceW, int uiPercent)
{
	using namespace detail;
	int w = sc(520, uiPercent);
	if (w > surfaceW - 2 * sc(24, uiPercent)) w = surfaceW - 2 * sc(24, uiPercent);
	return w;
}

inline int dialogInnerWidth(int surfaceW, int uiPercent) { return dialogPanelWidth(surfaceW, uiPercent) - 2 * detail::sc(28, uiPercent); }

inline DialogLayout computeDialog(const DialogInput &in)
{
	using namespace detail;
	++computeCounter();
	DialogLayout L;
	if (in.surfaceW <= 0 || in.surfaceH <= 0) return L;
	const int scale = dialogScale(in.surfaceW, in.surfaceH, in.uiPercent);
	L.effectiveScale = scale;
	const int pad = sc(28, scale), gap = sc(16, scale), titleH = sc(34, scale), ruleH = sc(2, scale) > 0 ? sc(2, scale) : 1;
	const int btnH = sc(40, scale), btnW = sc(132, scale);
	const int buttons = clampi(in.buttonCount, 1, kMaxDialogButtons);
	int w = dialogPanelWidth(in.surfaceW, scale);
	const int minW = buttons * btnW + (buttons - 1) * gap + 2 * pad;
	if (w < minW && minW <= in.surfaceW) w = minW;
	int bodyH = in.bodyHeight > sc(24, scale) ? in.bodyHeight : sc(24, scale);
	const int extraH = in.extraHeight > 0 ? sc(in.extraHeight, scale) : 0;
	int h = pad + titleH + gap / 2 + ruleH + gap + bodyH + (extraH ? gap / 2 + extraH : 0) + gap * 3 / 2 + btnH + pad;
	const int maxH = in.surfaceH - 2 * sc(24, scale);
	if (h > maxH) { bodyH -= h - maxH; h = maxH; if (bodyH < sc(24, scale)) bodyH = sc(24, scale); }
	L.panel = Rect{ (in.surfaceW - w) / 2, (in.surfaceH - h) / 2, w, h };
	L.title = Rect{ L.panel.x + pad, L.panel.y + pad, w - 2 * pad, titleH };
	L.titleRule = Rect{ L.title.x, L.title.bottom() + gap / 2, w - 2 * pad, ruleH };
	L.body = Rect{ L.title.x, L.titleRule.bottom() + gap, w - 2 * pad, bodyH };
	if (extraH) L.extra = Rect{ L.title.x, L.body.bottom() + gap / 2, w - 2 * pad, extraH };
	const int by = L.panel.bottom() - pad - btnH;
	for (int i = 0; i < buttons; ++i)   // index 0 is the leftmost; the primary action is the last
		L.button[i] = Rect{ L.panel.right() - pad - (buttons - i) * btnW - (buttons - 1 - i) * gap, by, btnW, btnH };
	return L;
}
// ---- list browser (Load Game, Replays): a title, a list on the left, a column of actions on the right (the last action - Back - sits at the bottom of the column) -----------------------
constexpr int kMaxBrowserActions = 6;

struct BrowserInput
{
	int surfaceW = 0, surfaceH = 0, uiPercent = 100;
	int actionCount = 1;
};

struct BrowserLayout
{
	Rect composition, panel, title, titleRule, list, actionColumn, action[kMaxBrowserActions], separator;
	int actionCount = 0;
	int effectiveScale = 100;
	bool compact = false;
};

inline BrowserLayout computeBrowser(const BrowserInput &in)
{
	using namespace detail;
	++computeCounter();
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	BrowserLayout L;
	if (in.surfaceW <= 0 || in.surfaceH <= 0) return L;
	int scale = in.uiPercent > 0 ? in.uiPercent : 100;
	int compW = static_cast<int>(in.surfaceH * ModernTheme::kCompositionMaxAspect + 0.5);
	if (compW > in.surfaceW) compW = in.surfaceW;
	L.composition = Rect{ (in.surfaceW - compW) / 2, 0, compW, in.surfaceH };
	const int actions = clampi(in.actionCount, 1, kMaxBrowserActions);
	for (int attempt = 0; attempt < 64; ++attempt)
	{
		const int pad = sc(24, scale), gap = sc(16, scale), edge = sc(m.edgeMargin, scale);
		const int titleH = sc(34, scale), ruleH = sc(m.ruleThickness, scale) > 0 ? sc(m.ruleThickness, scale) : 1;
		const int btnH = sc(m.actionHeight + 6, scale), btnGap = sc(10, scale), colW = sc(232, scale);
		const int listMinH = sc(5 * 36, scale);
		const int columnNeed = actions * btnH + (actions - 1) * btnGap;
		const int bodyNeed = columnNeed > listMinH ? columnNeed : listMinH;
		const int needH = pad + titleH + gap / 2 + ruleH + gap + bodyNeed + pad;
		const int availH = in.surfaceH - 2 * edge;
		int panelH = sc(600, scale);
		if (panelH > availH) panelH = availH;
		if (panelH < needH) panelH = needH;
		int panelW = sc(980, scale);
		if (panelW > compW - 2 * edge) panelW = compW - 2 * edge;
		const int minW = colW + sc(360, scale) + 2 * pad + gap;
		if ((needH > availH || minW > compW - 2 * edge) && scale > 30) { scale = scale * 92 / 100; continue; }
		if (panelW < minW) panelW = minW;
		L.panel = Rect{ L.composition.x + (compW - panelW) / 2, (in.surfaceH - panelH) / 2, panelW, panelH };
		const Rect &P = L.panel;
		L.title = Rect{ P.x + pad, P.y + pad, P.w - 2 * pad, titleH };
		L.titleRule = Rect{ L.title.x, L.title.bottom() + gap / 2, L.title.w, ruleH };
		const int top = L.titleRule.bottom() + gap, bottom = P.bottom() - pad;
		L.actionColumn = Rect{ P.right() - pad - colW, top, colW, bottom - top };
		L.separator = Rect{ L.actionColumn.x - gap / 2 - 1, top, 1, bottom - top };
		L.list = Rect{ P.x + pad, top, L.actionColumn.x - gap - (P.x + pad), bottom - top };
		L.actionCount = actions;
		for (int i = 0; i < actions; ++i)
		{
			const bool last = i == actions - 1 && actions > 1;   // Back: anchored to the bottom of the column
			const int y = last ? bottom - btnH : top + i * (btnH + btnGap);
			L.action[i] = Rect{ L.actionColumn.x, y, colW, btnH };
		}
		L.effectiveScale = scale;
		L.compact = scale < (in.uiPercent > 0 ? in.uiPercent : 100);
		return L;
	}
	return L;
}
} // namespace ModernScreen
