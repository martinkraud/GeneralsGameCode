// SPDX-License-Identifier: GPL-3.0-or-later
// Enhanced gameplay HUD (USA prototype): the pure geometry. No engine types, fully testable.
//
// The Enhanced HUD is a bounded, horizontally centred island that is flush with the bottom edge of the surface:
//
//        world                                                        world
//             +-------------------------------------------------------+
//             | RADAR + utility | UNIT INFO | MONEY/POWER + COMMANDS | XP |
//   [powers]  +-------------------------------------------------------+
//   ===================================== surface bottom ===============
//
// * It is designed on a 1000 x 180 reference island and scaled uniformly (never anisotropically). The scale is the
//   modern UI scale, reduced when needed so the island never exceeds `maxWidthFraction` of the surface width
//   (compact 4:3 surfaces may use the whole width; wider surfaces keep the world visible and clickable on both sides).
// * Nothing here depends on the surface width beyond that cap: on 21:9 / 32:9 the island stays the same physical size.
// * The Global Powers are an independent surface (a column in the gameplay safe region, a row on compact surfaces); it
//   never touches the island and is drawn as individual plates only.
// * Faction independent: the layout knows nothing about colours or art (EnhancedHudTheme supplies those).
// * Every existing control-bar window is *moved* (a table of reference rectangles from ControlBar.wnd mapped into the
//   island); no command logic is duplicated. Windows the Enhanced layout does not place keep their identity and are
//   simply not repositioned.
#pragma once

#include "Common/EnhancedHudTheme.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace EnhancedHudLayout
{

struct Rect
{
	int x = 0, y = 0, w = 0, h = 0;
	int right() const { return x + w; }
	int bottom() const { return y + h; }
	bool empty() const { return w <= 0 || h <= 0; }
	bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
	bool within(const Rect &o) const { return x >= o.x && y >= o.y && right() <= o.right() && bottom() <= o.bottom(); }
	bool intersects(const Rect &o) const { return x < o.right() && o.x < right() && y < o.bottom() && o.y < bottom(); }
};

// ---- reference design (island-local units) -------------------------------------------------------------------------
constexpr int kIslandW = 1000;
constexpr int kIslandH = 180;
constexpr int kPowerColumnW = 62;      // 59 wide shortcut buttons + 3 gap
constexpr int kPowerColumnH = 270;     // 5 buttons x 49 pitch + margins (GenPowersShortcutBarUS.wnd: 60 x 270)
constexpr int kPowerCount = 5;
constexpr int kPowerButtonX = 3, kPowerButtonY = 9, kPowerButtonW = 45, kPowerButtonH = 38;   // ButtonCommandN inside ButtonParentN
constexpr int kPowerPitchY = 49;       // retail vertical pitch of the five slots (GenPowersShortcutBarUS.wnd)
constexpr int kPowerCellH = 58;        // the horizontal fallback row: one 59 x 56 shortcut button plus a gap high
constexpr int kPowerPad = 6;           // frame padding around the Global Powers surface
constexpr int kPowerGap = 14;          // minimum clear space between that frame and the main island
constexpr int kPowerMargin = 10;       // minimum distance of that frame from the surface edge

// Regions (island-local reference units).
constexpr Rect kRadarFrame   = {  8, 8, 216, 164};
constexpr Rect kInfoFrame    = {230, 8, 165, 164};
constexpr Rect kCommandFrame = {401, 8, 444, 164};
constexpr Rect kXpFrame      = {851, 8, 141, 164};
constexpr int kDividerX[3]   = {227, 398, 848};
constexpr Rect kCashFrame    = {409, 10, 152,  26};   // dedicated money display panel (inside the command frame header)
constexpr int kCommandScalePermille = 1200;   // the 6 x 2 button grid is drawn 1.20 x
constexpr int kInfoScalePermille = 1100;      // the selection block is drawn 1.10 x

// Ratio of the surface width the island may take on non-compact surfaces.
constexpr int kMaxWidthPercentWide = 82;
// Surfaces at or below this aspect (4:3 family) may use the whole width.
constexpr int kCompactAspectPermille = 1450;

inline int scaledRef(int refUnits, double scale) { return static_cast<int>(std::lround(refUnits * scale)); }

struct Result
{
	bool valid = false;
	double scale = 1.0;          // island scale: physical pixels per reference unit
	Rect island;                 // physical pixels, bottom flush, centred
	Rect powerPanel;             // physical pixels: the special power shortcut parent window (independent of the island)
	Rect powerFrame;             // extent the Global Powers plates can occupy (all five slots, plus rounding tolerance); nothing is drawn behind it
	bool powerHorizontal = false;   // compact fallback: a row of five instead of a column
	bool powerAboveIsland = false;  // the panel is stacked above the island's footprint (otherwise beside it)
	int powerEdgeRight = -1;        // x the outer edge of every drawn Global Powers plate must end at (screen edge), or -1
	int powerPlateMargin = 0;       // margin of powerPlateRect in pixels at this scale
	// frame regions in physical pixels
	Rect radarFrame, infoFrame, commandFrame, xpFrame, cashFrame;
	int dividerX[3] = {0, 0, 0};
};

// `layoutScale` is the modern layout's uniform UI scale (DisplayGeometry::Layout::scale()).
inline Result compute(int surfaceW, int surfaceH, double layoutScale)
{
	Result r;
	if (surfaceW <= 0 || surfaceH <= 0 || !(layoutScale > 0.0))
		return r;
	const bool compact = surfaceW * 1000 <= surfaceH * kCompactAspectPermille;
	const double widthFraction = compact ? 1.0 : kMaxWidthPercentWide / 100.0;
	const double capByWidth = widthFraction * surfaceW / kIslandW;
	// never taller than a third of the surface either (tiny windows with huge UI scale)
	const double capByHeight = (surfaceH / 3.0) / kIslandH;
	r.scale = std::min(layoutScale, std::min(capByWidth, capByHeight));
	if (!(r.scale > 0.0))
		return r;
	const int w = std::min(scaledRef(kIslandW, r.scale), surfaceW);
	const int h = scaledRef(kIslandH, r.scale);
	r.island = {(surfaceW - w) / 2, surfaceH - h, w, h};     // flush with the bottom edge: bottom() == surfaceH
	const double s = static_cast<double>(w) / kIslandW;       // exact horizontal fit after rounding the width
	auto phys = [&](const Rect &ref) {
		return Rect{r.island.x + static_cast<int>(std::lround(ref.x * s)), r.island.y + static_cast<int>(std::lround(ref.y * s)),
		            static_cast<int>(std::lround(ref.w * s)), static_cast<int>(std::lround(ref.h * s))};
	};
	r.radarFrame = phys(kRadarFrame);
	r.infoFrame = phys(kInfoFrame);
	r.commandFrame = phys(kCommandFrame);
	r.xpFrame = phys(kXpFrame);
	r.cashFrame = phys(kCashFrame);
	for (int i = 0; i < 3; ++i)
		r.dividerX[i] = r.island.x + static_cast<int>(std::lround(kDividerX[i] * s));

	// Global Powers: an independent surface, never touching the main island. The plates are mounted flush against the RIGHT
	// SCREEN EDGE (human preference, also on 21:9 / 32:9: the trade-off is that on extreme ultrawide the powers are far from
	// the central action). The frame below is the extent the plates can occupy (all five slots) plus a rounding tolerance;
	// only visible slots draw a plate, so the visual group is never larger than its plates.
	const int mar = scaledRef(kPowerMargin, r.scale), gap = scaledRef(kPowerGap, r.scale);
	const int pm = std::max(2, static_cast<int>(std::lround(3 * r.scale)));         // plate margin (powerPlateRect)
	const int tol = std::max(3, static_cast<int>(std::lround(1.5 * r.scale)));      // rounding tolerance of the extent
	const int bx = scaledRef(kPowerButtonX, r.scale), bw = scaledRef(kPowerButtonW, r.scale);
	const int by = scaledRef(kPowerButtonY, r.scale), bh = scaledRef(kPowerButtonH, r.scale);
	const int pitchY = scaledRef(kPowerPitchY, r.scale), pitchX = scaledRef(kPowerColumnW, r.scale);
	const int colW = scaledRef(kPowerColumnW, r.scale), colH = scaledRef(kPowerColumnH, r.scale);
	// extent of the plates relative to the panel window origin: column
	const int cL = bx - pm - tol, cR = bx + bw + pm, cT = by - pm - tol, cB = (kPowerCount - 1) * pitchY + by + bh + pm + tol;
	const int cx = surfaceW - cR;                                                       // the plates end exactly at the screen edge
	const bool overlapsX = cx + cL < r.island.right() + gap && cx + cR > r.island.x - gap;
	const int yMax = (overlapsX ? r.island.y - gap : surfaceH - mar) - cB;
	const int yMin = mar - cT;
	if (!compact && yMax >= yMin && cx + cL >= 0)
	{
		const int cy = std::max(yMin, std::min(yMax, static_cast<int>(std::lround(surfaceH * 0.18)) - cT));
		r.powerPanel = {cx, cy, colW, colH};
		r.powerHorizontal = false;
		r.powerAboveIsland = overlapsX;
		r.powerFrame = {cx + cL, cy + cT, cR - cL, cB - cT};
		r.powerEdgeRight = surfaceW;
		r.powerPlateMargin = pm;
	}
	else
	{
		// compact surfaces (4:3 family: the island spans the whole width, a tall column would cover a large part of the
		// battlefield) and the rare case where a column cannot fit: a row of five above the island's right end
		const int rL = bx - pm - tol, rR = (kPowerCount - 1) * pitchX + bx + bw + pm + tol, rT = by - pm - tol, rB = by + bh + pm + tol;
		const int rowW = kPowerCount * pitchX, rowH = scaledRef(kPowerCellH, r.scale);
		const int frameRight = std::min(r.island.right(), surfaceW) - 1;
		const int rx = std::max(-rL + mar, frameRight - rR);
		const int ry = std::max(mar - rT, r.island.y - gap - rB);
		r.powerPanel = {rx, ry, rowW, rowH};
		r.powerHorizontal = true;
		r.powerAboveIsland = true;
		r.powerFrame = {rx + rL, ry + rT, rR - rL, rB - rT};
	}
	r.valid = true;
	return r;
}

// The island moved by (dx, dy) (the real control bar window slides on hide/show and in the low stage): every island-bound
// rectangle moves with it, the independent Global Powers surface does not. Presentation helper for the procedural frame.
inline Result translated(const Result &r, int dx, int dy)
{
	Result t = r;
	Rect *rects[] = {&t.island, &t.radarFrame, &t.infoFrame, &t.commandFrame, &t.xpFrame, &t.cashFrame};
	for (Rect *q : rects) { q->x += dx; q->y += dy; }
	for (int i = 0; i < 3; ++i) t.dividerX[i] += dx;
	return t;
}

// ---- window placement table ---------------------------------------------------------------------------------------
// `src` is the rectangle in ControlBar.wnd (800 x 600 reference); `dst` is island-local reference units. A group is
// moved by translation (and by `permille` scale around its origin for the command grid).
struct Placement
{
	const char *window;       // without the "ControlBar.wnd:" prefix
	Rect src;
	Rect dst;                 // island-local reference units
	bool hide;                // true: not placed, hidden (no Enhanced equivalent: the 800 px size toggles)
};

// Radar group: LeftHUD at (16,14); utility buttons in a column at its right edge.
// Info group: the 141 x 109 selection block scaled 1.10 to (235,34).
// Command group: the 6 x 2 grid scaled 1.20, first button's top-left at (409,44).
// Header: the cash panel and the power bar above the grid. XP group: General button, the vertical XP bar and the rank
// ladder (kRankSlot) with room to read.
inline Rect infoMap(const Rect &src)
{
	const int k = kInfoScalePermille;
	return {235 + ((src.x - 621) * k) / 1000, 34 + ((src.y - 483) * k) / 1000, (src.w * k) / 1000, (src.h * k) / 1000};
}
inline Rect commandMap(const Rect &src)
{
	const int k = kCommandScalePermille;
	return {409 + ((src.x - 246) * k) / 1000, 44 + ((src.y - 493) * k) / 1000, (src.w * k) / 1000, (src.h * k) / 1000};
}

struct NamedPlacement
{
	const char *window;
	Rect src;
	Rect dst;
	bool hide;
};

inline const NamedPlacement *placements(int &count)
{
	static const NamedPlacement table[] = {
		// radar group
		{"LeftHUD",              {  7, 443, 167, 152}, { 16,  14, 167, 152}, false},
		{"WinUAttack",           { 77, 416,  45,  45}, {188, 142,  32,  28}, false},
		{"ButtonOptions",        {206, 495,  30,  22}, {188,  20,  32,  26}, false},
		{"ButtonIdleWorker",     {206, 519,  30,  22}, {188,  50,  32,  26}, false},
		{"ButtonPlaceBeacon",    {206, 543,  30,  22}, {188,  80,  32,  26}, false},
		{"PopupCommunicator",    {206, 567,  30,  22}, {188, 110,  32,  26}, false},
		// header: money / power
		{"MoneyDisplay",         {360, 437,  79,  19}, {413,  12, 144,  22}, false},
		{"PowerWindow",          {261, 473, 283,   7}, {570,  17, 263,   7}, false},
		// XP group
		{"ButtonGeneral",        {736, 433,  56,  41}, {898,  14,  88,  39}, false},
		{"ExpBarForeground",     {766, 484,  21, 107}, {857,  16,  34, 148}, false},
		{"GeneralsExp",          {769, 503,  13,  87}, {862,  22,  24, 136}, false},
		// no Enhanced equivalent: the three size toggles of the 800 px bar
		{"ButtonSmall",          {592, 436,  31,  21}, {0, 0, 0, 0}, true},
		{"ButtonMedium",         {625, 439,  31,  21}, {0, 0, 0, 0}, true},
		{"ButtonLarge",          {666, 445,  48,  28}, {0, 0, 0, 0}, true},
		// retail draw markers: no input, but a no-input window under the cursor still swallows the click. They are kept
		// (their draw callbacks stay wired) as 1 x 1 windows in the island's top-left corner, clear of every control.
		{"BackgroundMarker",     {  8, 595,   5,   5}, {  0,  0,   1,   1}, false},
		{"ForegroundMarker",     {  0, 595,   5,   5}, {  0,  0,   1,   1}, false},
		{"OnTopDraw",            {  3, 593,  31,   7}, {  0,  0,   1,   1}, false},
	};
	count = static_cast<int>(sizeof(table) / sizeof(table[0]));
	return table;
}

// The General module (one piece): XP trough on the left, the General Powers entry button on top of the right column and
// below it the rank grid, 2 x 2 square cells (slot 0 = first promotion, reading left to right, top to bottom), each with
// a square star. Island-local reference units.
constexpr int kRankGridX = 898, kRankGridY = 66, kRankCell = 42, kRankPitch = 46, kRankStarSize = 28;
inline Rect rankSlot(int index)
{
	return {kRankGridX + (index % 2) * kRankPitch, kRankGridY + (index / 2) * kRankPitch, kRankCell, kRankCell};
}
// Star glyph box inside a slot: square and centred.
inline Rect rankStar(int index)
{
	const Rect s = rankSlot(index);
	return {s.x + (s.w - kRankStarSize) / 2, s.y + (s.h - kRankStarSize) / 2, kRankStarSize, kRankStarSize};
}
// Number of ladder slots for a rank table of `rankLevels` levels (level 1 has no star).
inline int rankSlotCount(int rankLevels) { return std::max(0, std::min(rankLevels - 1, 4)); }

// Containers and children of the info group (all are translated by infoMap).
struct GroupWindow { const char *window; Rect src; };
inline const GroupWindow *infoGroup(int &count)
{
	static const GroupWindow table[] = {
		{"RightHUD",              {621, 483, 141, 109}},
		{"ProductionQueueWindow", {621, 483, 141, 109}},
		{"WinUnitSelected",       {621, 483, 141, 109}},
		{"CameoWindow",           {622, 484,  91,  69}},
		{"UnitUpgrade1",          {715, 484,  46,  33}},
		{"UnitUpgrade2",          {715, 519,  46,  34}},
		{"UnitUpgrade3",          {715, 555,  46,  36}},
		{"UnitUpgrade4",          {668, 555,  45,  36}},
		{"UnitUpgrade5",          {622, 555,  44,  36}},
		{"ButtonQueue01",         {622, 484,  44,  33}},
		{"ButtonQueue02",         {668, 484,  45,  33}},
		{"ButtonQueue03",         {715, 484,  46,  33}},
		{"ButtonQueue04",         {622, 519,  44,  34}},
		{"ButtonQueue05",         {668, 519,  45,  34}},
		{"ButtonQueue06",         {715, 519,  46,  34}},
		{"ButtonQueue07",         {622, 555,  44,  36}},
		{"ButtonQueue08",         {668, 555,  45,  36}},
		{"ButtonQueue09",         {715, 555,  46,  36}},
	};
	count = static_cast<int>(sizeof(table) / sizeof(table[0]));
	return table;
}

// Containers (full 408 x 100 command area) and the twelve buttons of the command group (commandMap).
inline const GroupWindow *commandGroup(int &count)
{
	static const GroupWindow table[] = {
		{"CommandWindow",             {198, 492, 408, 100}},
		{"BeaconWindow",              {198, 492, 408, 100}},
		{"UnderConstructionWindow",   {198, 492, 408, 100}},
		{"ObserverPlayerInfoWindow",  {198, 492, 408, 100}},
		{"ObserverPlayerListWindow",  {198, 492, 408, 100}},
		{"OCLTimerWindow",            {200, 491, 408, 100}},
		{"ButtonCommand01",           {246, 493,  53,  45}},
		{"ButtonCommand02",           {246, 546,  53,  45}},
		{"ButtonCommand03",           {306, 493,  53,  45}},
		{"ButtonCommand04",           {306, 546,  53,  45}},
		{"ButtonCommand05",           {366, 493,  53,  45}},
		{"ButtonCommand06",           {366, 546,  53,  45}},
		{"ButtonCommand07",           {426, 493,  53,  45}},
		{"ButtonCommand08",           {426, 546,  53,  45}},
		{"ButtonCommand09",           {486, 493,  53,  45}},
		{"ButtonCommand10",           {486, 546,  53,  45}},
		{"ButtonCommand11",           {546, 493,  53,  45}},
		{"ButtonCommand12",           {546, 546,  53,  45}},
	};
	count = static_cast<int>(sizeof(table) / sizeof(table[0]));
	return table;
}

// Island-local reference rectangle of a placement of the three kinds above (for a given window name), or false.
inline bool destinationFor(const char *name, Rect &out, bool &hide)
{
	hide = false;
	int n = 0;
	for (const NamedPlacement *p = placements(n); n-- > 0; ++p)
		if (std::strcmp(p->window, name) == 0) { out = p->dst; hide = p->hide; return true; }
	n = 0;
	for (const GroupWindow *g = infoGroup(n); n-- > 0; ++g)
		if (std::strcmp(g->window, name) == 0) { out = infoMap(g->src); return true; }
	n = 0;
	for (const GroupWindow *g = commandGroup(n); n-- > 0; ++g)
		if (std::strcmp(g->window, name) == 0)
		{
			out = commandMap(g->src);
			// a container is the bounding box of the grid (the 408 x 100 retail area is wider than the frame)
			if (std::strstr(name, "Button") == nullptr)
				out = {commandMap(Rect{246, 493, 0, 0}).x - 6, commandMap(Rect{246, 493, 0, 0}).y - 6,
				       commandMap(Rect{546, 546, 53, 45}).right() - commandMap(Rect{246, 493, 0, 0}).x + 12,
				       commandMap(Rect{546, 546, 53, 45}).bottom() - commandMap(Rect{246, 493, 0, 0}).y + 12};
			return true;
		}
	return false;
}

// Island-local reference rect -> physical pixels.
inline Rect toPhysical(const Result &r, const Rect &local)
{
	const double s = r.island.w > 0 ? static_cast<double>(r.island.w) / kIslandW : r.scale;
	const int x0 = r.island.x + static_cast<int>(std::lround(local.x * s));
	const int y0 = r.island.y + static_cast<int>(std::lround(local.y * s));
	const int x1 = r.island.x + static_cast<int>(std::lround((local.x + local.w) * s));
	const int y1 = r.island.y + static_cast<int>(std::lround((local.y + local.h) * s));
	return {x0, y0, std::max(x1 - x0, local.w > 0 ? 1 : 0), std::max(y1 - y0, local.h > 0 ? 1 : 0)};
}

// Thousands grouping for the Enhanced cash panel ("45200" -> "45,200"); presentation only, the value is untouched.
inline int groupThousands(unsigned long long value, wchar_t *out, int capacity)
{
	if (!out || capacity < 2)
		return 0;
	wchar_t digits[32];
	int n = 0;
	do { digits[n++] = static_cast<wchar_t>(L'0' + value % 10); value /= 10; } while (value && n < 31);
	int len = 0;
	for (int i = n - 1; i >= 0 && len < capacity - 1; --i)
	{
		out[len++] = digits[i];
		if (i > 0 && i % 3 == 0 && len < capacity - 1)
			out[len++] = L',';
	}
	out[len] = 0;
	return len;
}

// Enhanced is available for factions that have an implemented theme (EnhancedHudTheme); every other side falls back to
// the Original presentation. Today that is the USA only. The layout itself is faction independent.
inline bool supportsSide(const char *side)
{
	if (!side) return false;
	if (!EnhancedHudTheme::implemented(EnhancedHudTheme::factionOfSide(side))) return false;
	return std::strstr(side, "Small") == nullptr;     // the squished retail variant keeps the Original bar
}

// The plate drawn behind one Global Powers button: the button rectangle plus a small even margin. The Global Powers
// surface is nothing but these plates (one per visible slot) - there is no enclosing background panel.
inline Rect powerPlateRect(const Rect &button, double scale)
{
	const int m = std::max(2, static_cast<int>(std::lround(3 * scale)));
	return {button.x - m, button.y - m, button.w + 2 * m, button.h + 2 * m};
}

// The whole decision as a function: Enhanced is shown only when requested, in the modern presentation, for a
// supported side, on a surface that can hold the island.
inline bool shouldBeActive(bool requested, bool modernDisplay, bool classic, const char *side, int surfaceW, int surfaceH)
{
	return requested && modernDisplay && !classic && supportsSide(side) && surfaceW >= 640 && surfaceH >= 480;
}

}  // namespace EnhancedHudLayout
