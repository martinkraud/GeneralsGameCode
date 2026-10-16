// SPDX-License-Identifier: GPL-3.0-or-later
// Enhanced gameplay HUD (USA prototype): the shared image-fitting policy. Pure geometry, no engine types.
//
// The retail control bar maps every bitmap onto the rectangle of its GameWindow. The Enhanced layout gives windows
// different rectangles (a window is both the hit region and, in the retail draw code, the image rectangle), so the same
// mapping would stretch the bitmap to whatever the new rectangle happens to be. Enhanced therefore separates the two:
// the window keeps the full interactive plate, the bitmap is drawn with an explicit policy:
//
//   AspectFit   scale uniformly to fit inside the window, centred, never distorted (letterbox where the shapes differ)
//   NativeSize  1:1 or a whole-number multiple, centred (tiny bitmaps that must not be blown up)
//   FrameArt    designed to cover its rectangle and scaled by the same uniform factor as the Original mapping, so its
//               proportions are exactly the Original ones (never re-stretched by Enhanced)
//   Gauge       end caps / repeating tiles whose cross extent is the variable one: tiles are scaled uniformly from the
//               gauge thickness, only the length varies (intentionally stretchable along its length)
//   Procedural  the retail bitmap is not used in Enhanced (too low resolution to scale, or no Enhanced counterpart);
//               the control keeps its window, commands and state
//   SelfFitting the draw code keeps the aspect itself (radar map)
//   TextOnly / Container: no bitmap involved
#pragma once

#include "Common/EnhancedHudLayout.h"

#include <cmath>
#include <cstring>

namespace EnhancedHudImageFit
{

using EnhancedHudLayout::Rect;

enum class Class { AspectFit, NativeSize, FrameArt, Gauge, Procedural, SelfFitting, TextOnly, Container };

// Uniform fit of an `imgW x imgH` bitmap into `area`, centred. Returns an empty rect for degenerate input.
inline Rect containFit(const Rect &area, int imgW, int imgH)
{
	if (area.w <= 0 || area.h <= 0 || imgW <= 0 || imgH <= 0)
		return {};
	const double s = std::min(static_cast<double>(area.w) / imgW, static_cast<double>(area.h) / imgH);
	const int w = std::max(1, static_cast<int>(std::lround(imgW * s)));
	const int h = std::max(1, static_cast<int>(std::lround(imgH * s)));
	return {area.x + (area.w - w) / 2, area.y + (area.h - h) / 2, w, h};
}

// 1:1 (or the largest whole-number multiple that fits), centred; falls back to a uniform shrink when the bitmap is larger
// than the area.
inline Rect nativeFit(const Rect &area, int imgW, int imgH)
{
	if (area.w <= 0 || area.h <= 0 || imgW <= 0 || imgH <= 0)
		return {};
	const double contain = std::min(static_cast<double>(area.w) / imgW, static_cast<double>(area.h) / imgH);
	if (contain < 1.0)
		return containFit(area, imgW, imgH);
	const int k = std::max(1, static_cast<int>(std::floor(contain)));
	const int w = imgW * k, h = imgH * k;
	return {area.x + (area.w - w) / 2, area.y + (area.h - h) / 2, w, h};
}

inline Rect fit(Class cls, const Rect &area, int imgW, int imgH)
{
	switch (cls)
	{
		case Class::AspectFit: return containFit(area, imgW, imgH);
		case Class::NativeSize: return nativeFit(area, imgW, imgH);
		default: return area;       // FrameArt / Gauge are drawn by their own code against the window rectangle
	}
}

// Uniform scale of a gauge's tiles: the cross extent of the gauge window divided by the tile's native cross extent.
inline double gaugeScale(int windowCross, int imageCross)
{
	return (windowCross > 0 && imageCross > 0) ? static_cast<double>(windowCross) / imageCross : 1.0;
}

struct Policy
{
	const char *window;      // ControlBar.wnd name without prefix (or "Power:<name>" for GenPowersShortcutBar*.wnd)
	Class cls;
	const char *why;
};

// Every image-backed or art-bearing window the Enhanced USA layout places. A test requires that every window of the
// layout tables appears here: new windows must be classified, not silently stretched.
inline const Policy *policies(int &count)
{
	static const Policy table[] = {
		{"LeftHUD",              Class::SelfFitting, "TheRadar->draw keeps the map aspect inside the window"},
		{"WinUAttack",           Class::Procedural,  "retail SUUAttackI/H are 22x24 px; the warning lamp is drawn procedurally, state = window enable"},
		{"ButtonOptions",        Class::Procedural,  "30x22 px bitmaps: procedural plate + glyph"},
		{"ButtonIdleWorker",     Class::Procedural,  "30x22 px bitmaps: procedural plate + glyph"},
		{"ButtonPlaceBeacon",    Class::Procedural,  "30x22 px bitmaps: procedural plate + glyph"},
		{"PopupCommunicator",    Class::Procedural,  "30x22 px bitmaps: procedural plate + glyph"},
		{"MoneyDisplay",         Class::TextOnly,    "static text inside the cash panel"},
		{"PowerWindow",          Class::Gauge,       "repeating PowerPoint tiles: thickness scales the tiles, only the length varies"},
		{"ButtonGeneral",        Class::AspectFit,   "SUGeneral/H/P are 65x29 px (three stars): fitted, never squeezed into the button plate"},
		{"ExpBarForeground",     Class::Procedural,  "retail strip (21x107 px, stars baked in) replaced by trough + rank ladder"},
		{"GeneralsExp",          Class::Gauge,       "GenExpBar tiles: width scales the tiles, only the filled length varies"},
		{"ButtonSmall",          Class::Container,   "hidden: no Enhanced counterpart"},
		{"ButtonMedium",         Class::Container,   "hidden: no Enhanced counterpart"},
		{"ButtonLarge",          Class::Container,   "hidden: no Enhanced counterpart"},
		{"BackgroundMarker",     Class::Container,   "no-input draw marker, no image"},
		{"ForegroundMarker",     Class::Container,   "no-input draw marker, no image"},
		{"OnTopDraw",            Class::Container,   "no-input draw marker, no image"},
		{"RightHUD",             Class::Container,   "container"},
		{"ProductionQueueWindow", Class::Container,  "container"},
		{"WinUnitSelected",      Class::Container,   "container"},
		{"CameoWindow",          Class::AspectFit,   "unit/building cameo portrait"},
		{"UnitUpgrade1",         Class::AspectFit,   "upgrade icon"},
		{"UnitUpgrade2",         Class::AspectFit,   "upgrade icon"},
		{"UnitUpgrade3",         Class::AspectFit,   "upgrade icon"},
		{"UnitUpgrade4",         Class::AspectFit,   "upgrade icon"},
		{"UnitUpgrade5",         Class::AspectFit,   "upgrade icon"},
		{"ButtonQueue01",        Class::AspectFit,   "production queue cameo"},
		{"ButtonQueue02",        Class::AspectFit,   "production queue cameo"},
		{"ButtonQueue03",        Class::AspectFit,   "production queue cameo"},
		{"ButtonQueue04",        Class::AspectFit,   "production queue cameo"},
		{"ButtonQueue05",        Class::AspectFit,   "production queue cameo"},
		{"ButtonQueue06",        Class::AspectFit,   "production queue cameo"},
		{"ButtonQueue07",        Class::AspectFit,   "production queue cameo"},
		{"ButtonQueue08",        Class::AspectFit,   "production queue cameo"},
		{"ButtonQueue09",        Class::AspectFit,   "production queue cameo"},
		{"CommandWindow",        Class::Container,   "container"},
		{"BeaconWindow",         Class::Container,   "container"},
		{"UnderConstructionWindow", Class::Container, "container"},
		{"ObserverPlayerInfoWindow", Class::Container, "container"},
		{"ObserverPlayerListWindow", Class::Container, "container"},
		{"OCLTimerWindow",       Class::Container,   "container"},
		{"ButtonCommand01",      Class::AspectFit,   "command/build icon"},
		{"ButtonCommand02",      Class::AspectFit,   "command/build icon"},
		{"ButtonCommand03",      Class::AspectFit,   "command/build icon"},
		{"ButtonCommand04",      Class::AspectFit,   "command/build icon"},
		{"ButtonCommand05",      Class::AspectFit,   "command/build icon"},
		{"ButtonCommand06",      Class::AspectFit,   "command/build icon"},
		{"ButtonCommand07",      Class::AspectFit,   "command/build icon"},
		{"ButtonCommand08",      Class::AspectFit,   "command/build icon"},
		{"ButtonCommand09",      Class::AspectFit,   "command/build icon"},
		{"ButtonCommand10",      Class::AspectFit,   "command/build icon"},
		{"ButtonCommand11",      Class::AspectFit,   "command/build icon"},
		{"ButtonCommand12",      Class::AspectFit,   "command/build icon"},
		// GenPowersShortcutBarUS.wnd
		{"Power:ButtonCommand",  Class::AspectFit,   "special power icon (cooldown overlay still covers the whole plate)"},
		{"Power:ButtonParent",   Class::FrameArt,    "button frame art, scaled uniformly with its column"},
	};
	count = static_cast<int>(sizeof(table) / sizeof(table[0]));
	return table;
}

inline const Policy *find(const char *window)
{
	int n = 0;
	for (const Policy *p = policies(n); n-- > 0; ++p)
		if (std::strcmp(p->window, window) == 0)
			return p;
	return nullptr;
}

}  // namespace EnhancedHudImageFit
