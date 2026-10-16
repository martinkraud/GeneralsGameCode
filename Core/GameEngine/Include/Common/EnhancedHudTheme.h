// SPDX-License-Identifier: GPL-3.0-or-later
// Enhanced gameplay HUD: the faction presentation layer. Pure data, no engine types, no gameplay state.
//
// Every faction shares the same HUD core (EnhancedHudLayout: module roles, geometry, responsive rules; EnhancedHudWindows:
// placement and hit-test mechanism; EnhancedHudImageFit: bitmap policy; ControlBarEnhanced.cpp: the one renderer).
// A faction only supplies a Theme: colours, weights and a few decorative switches that the single renderer reads. A theme
// cannot change where a control is, what it does or what state it shows.
//
//   shared by all factions            may differ per faction (this file)
//   module roles + interaction model  surface/accent colours, bevel and outline weights
//   geometry / responsive layout      decorative geometry switches (corner brackets, caps, bevels)
//   command/XP/power behaviour        warning/ready/disabled colours
//   hit testing, image fitting        (later) textures, emblem, typography, popup styling
//
// Implemented: USA. China, GLA, Europe and Russia are recognised but not implemented yet: for them `implemented()` is
// false, Enhanced stays off (the Original control bar is used) and `themeFor()` returns the USA theme only as a safe default
// for tools/tests - no placeholder art is invented. Adding a faction = add a Theme constant, flip `implemented()`; no HUD
// logic is copied. Europe/Russia are not retail sides; they enter through `factionOfSide` when their side names exist.
#pragma once

#include <cstring>

namespace EnhancedHudTheme
{

enum class Faction { Usa, China, Gla, Europe, Russia, Unknown };

struct Rgba { unsigned char r, g, b, a; };

struct Theme
{
	const char *name;
	// console surfaces
	Rgba plate, plateBand, module, moduleLow, well, wellEdge;
	// lines and accents
	Rgba outline, accent, accentDim, bevelLight, bevelDark;
	// content
	Rgba icon, iconDim, rankLit, rankDim;
	// state treatments
	Rgba ready, warning, warningDark, warningInk;
	// presentation switches (decorative only)
	bool moduleCaps;          // short accent cap on a module's top edge
	bool bevelModules;        // light top/left, dark bottom/right edge on modules
	bool cornerBrackets;      // small bracket marks at the console corners
	int capPermille;          // cap length as a share of the module width (permille)
};

// USA Theme v1: modern military command console. Dark navy / gunmetal, restrained blue accent, recessed equipment bays.
inline const Theme &usa()
{
	static const Theme t = {
		"USA",
		/* plate     */ {12, 16, 24, 255}, /* plateBand */ {21, 27, 39, 255}, /* module */ {30, 38, 52, 255},
		/* moduleLow */ {20, 26, 38, 255}, /* well */ {9, 13, 20, 255},      /* wellEdge */ {44, 58, 82, 255},
		/* outline   */ {34, 56, 98, 255}, /* accent */ {72, 126, 208, 255}, /* accentDim */ {38, 64, 112, 255},
		/* bevelLight*/ {82, 100, 132, 255}, /* bevelDark */ {7, 9, 15, 255},
		/* icon      */ {198, 214, 240, 255}, /* iconDim */ {92, 104, 124, 255},
		/* rankLit   */ {236, 200, 88, 255},  /* rankDim */ {50, 58, 74, 255},
		/* ready     */ {96, 156, 236, 255},  /* warning */ {224, 24, 24, 255}, /* warningDark */ {70, 14, 14, 255},
		/* warningInk*/ {255, 255, 255, 255},
		true, true, true, 140,
	};
	return t;
}

// Maps a player template side name (ControlBarScheme side) to a faction. Retail ZH sides keep the faction prefix
// (e.g. AmericaAirForceGeneral). Europe/Russia are reserved for their future side names.
inline Faction factionOfSide(const char *side)
{
	if (!side) return Faction::Unknown;
	if (std::strncmp(side, "America", 7) == 0) return Faction::Usa;
	if (std::strncmp(side, "China", 5) == 0) return Faction::China;
	if (std::strncmp(side, "GLA", 3) == 0) return Faction::Gla;
	if (std::strncmp(side, "Europe", 6) == 0) return Faction::Europe;
	if (std::strncmp(side, "Russia", 6) == 0) return Faction::Russia;
	return Faction::Unknown;
}

// Only factions with a real theme get the Enhanced HUD; every other faction keeps the Original control bar.
inline bool implemented(Faction f) { return f == Faction::Usa; }

inline const Theme &themeFor(Faction f)
{
	(void)f;                 // future factions add their own constant here; until then the safe default is USA
	return usa();
}

}  // namespace EnhancedHudTheme
