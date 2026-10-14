// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 FIX1: scale-aware geometry of the runtime-built Options > Display Options panel (pure, testable).
//
// Native review of x64-33-modern-display showed overlapping controls: the panel metrics were fixed pixel numbers
// while the window system had already scaled the panel, the fonts and the control heights by the UI scale. Every
// number here is a reference (800x600) unit multiplied by the scale, derived from the panel's own size, so the
// layout is the same picture at every supported scale.
#pragma once

#include <algorithm>
#include <cmath>

namespace OptionsPanelLayout
{

constexpr int kMaxRows = 8;

struct Rect { int x = 0, y = 0, w = 0, h = 0; int right() const { return x + w; } int bottom() const { return y + h; } };

struct Panel
{
	int panelW = 0, panelH = 0;
	Rect title;
	int rowCount = 0;
	Rect label[kMaxRows];     // left column caption
	Rect control[kMaxRows];   // right column control
	Rect note;                // explanatory text under the last row
	int buttonZoneTop = 0;    // the accept/back buttons start here (panel-relative)
	bool fits = false;        // every row and the note end above the button zone
};

inline int scaled(int reference, double scale) { return static_cast<int>(std::floor(reference * scale + 0.5)); }

// `controlHeight` is the live control height (a 24 reference-pixel combo box times the UI scale).
// `buttonZoneTop` is the panel-relative y of the top of the accept/back buttons.
inline Panel compute(int panelW, int panelH, int controlHeight, double scale, int rowCount, int buttonZoneTop)
{
	Panel p;
	p.panelW = panelW;
	p.panelH = panelH;
	p.rowCount = std::min(std::max(rowCount, 0), kMaxRows);
	p.buttonZoneTop = buttonZoneTop;
	const int pad = scaled(24, scale);
	const int titleTop = scaled(14, scale);
	const int rowTop = scaled(58, scale);
	const int gap = std::max(scaled(14, scale), 4);
	const int pitch = std::max(controlHeight + gap, scaled(52, scale));
	const int half = panelW / 2;
	p.title = {pad, titleTop, panelW - 2 * pad, scaled(30, scale)};
	for (int i = 0; i < p.rowCount; ++i)
	{
		const int y = rowTop + pitch * i;
		p.label[i] = {pad, y, half - pad - gap / 2, controlHeight};
		p.control[i] = {half, y, half - pad, controlHeight};
	}
	const int noteTop = rowTop + pitch * p.rowCount + gap;
	p.note = {pad, noteTop, panelW - 2 * pad, controlHeight * 2};
	p.fits = p.note.bottom() <= buttonZoneTop;
	return p;
}

}  // namespace OptionsPanelLayout
