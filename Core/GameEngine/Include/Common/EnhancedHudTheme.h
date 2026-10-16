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

// Edge profile of a surface: square, or a machined 45 degree cut of `ref` reference units on the outer corners.
enum class Corner { Square, Chamfer };

struct Theme
{
	const char *name;
	// ---- colour ---------------------------------------------------------------------------------------------------
	// console surfaces
	Rgba plate, plateBand, module, moduleLow, well, wellEdge;
	// lines and accents
	Rgba outline, accent, accentDim, bevelLight, bevelDark;
	// content
	Rgba icon, iconDim, rankLit, rankDim;
	// state treatments
	Rgba ready, warning, warningDark, warningInk;
	// ---- shape and material language (not colour): what makes a faction look different when recoloured is not enough ----
	Corner shellCorner;       // outer console top corners
	int shellCornerRef;       // size in reference units
	Corner moduleCorner;      // module corners
	int moduleCornerRef;
	Corner plateCorner;       // interactive plates: command cells, utility keys, Global Powers plates
	int plateCornerRef;
	bool moduleCaps;          // short accent cap on a module's top edge
	int capPermille;          // cap length as a share of the module width (permille)
	bool bevelModules;        // light top/left, dark bottom/right edge on modules
	bool cornerBrackets;      // bracket marks on the console and bezel corners
	bool fasteners;           // small screws at the shell corners, on the radar bezel and the lamp housing
	bool seams;               // short machined seams in the gaps between modules
	bool vents;               // small groups of vent lines in module headers
	bool tickMarks;           // instrumentation ticks (XP gauge, header rail, radar bezel)
	bool stateRails;          // thin accent rail on plates that shows ready / not ready
	bool railBreaks;          // the shell's accent rail is drawn in segments with technical breaks
	int bezelRef;             // radar bezel thickness (reference units)
	int wellDepthRef;         // inner shadow thickness of recessed wells (reference units)
};

// True when two themes differ in more than colour (shape / material language). Used by tests and tools to keep a new
// faction from being a recolour of another one.
inline bool differsBeyondColour(const Theme &a, const Theme &b)
{
	return a.shellCorner != b.shellCorner || a.shellCornerRef != b.shellCornerRef || a.moduleCorner != b.moduleCorner ||
	       a.moduleCornerRef != b.moduleCornerRef || a.plateCorner != b.plateCorner || a.plateCornerRef != b.plateCornerRef ||
	       a.moduleCaps != b.moduleCaps || a.capPermille != b.capPermille || a.bevelModules != b.bevelModules ||
	       a.cornerBrackets != b.cornerBrackets || a.fasteners != b.fasteners || a.seams != b.seams || a.vents != b.vents ||
	       a.tickMarks != b.tickMarks || a.stateRails != b.stateRails || a.railBreaks != b.railBreaks ||
	       a.bezelRef != b.bezelRef || a.wellDepthRef != b.wellDepthRef;
}

// USA Theme v1: modern military command console. Precision-manufactured: dark navy / gunmetal, machined (chamfered)
// corners, recessed equipment bays, a few purposeful fasteners and seams, restrained blue instrumentation.
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
		/* shell   */ Corner::Chamfer, 9, /* module */ Corner::Chamfer, 4, /* plate */ Corner::Chamfer, 3,
		/* caps    */ true, 140, /* bevel */ true, /* brackets */ true, /* fasteners */ true, /* seams */ true,
		/* vents   */ true, /* ticks */ true, /* stateRails */ true, /* railBreaks */ true,
		/* bezel   */ 4, /* wellDepth */ 2,
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
