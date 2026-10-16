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
// Implemented: USA (reference), China. GLA, Europe and Russia are recognised but not implemented yet: for them `implemented()` is
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
	// raised control surfaces and chassis texture
	Rgba control, controlLow, chassisLine;
	// ---- shape and material language (not colour): what makes a faction look different when recoloured is not enough ----
	Corner shellCorner;       // outer console top corners
	int shellCornerRef;       // size in reference units
	Corner moduleCorner;      // module corners
	int moduleCornerRef;
	Corner plateCorner;       // interactive plates: command cells, utility keys, Global Powers plates
	int plateCornerRef;
	bool bevelModules;        // inner shadow top/left and a lit lip bottom/right on recessed modules
	bool fasteners;           // small screws at the shell corners, on the radar bezel and the lamp housing
	bool seams;               // short machined seams in the gaps between modules
	bool vents;               // small groups of vent lines in module headers
	bool tickMarks;           // instrumentation ticks (XP gauge, header rail, radar bezel)
	bool stateRails;          // thin accent rail on plates that shows ready / not ready
	int bezelRef;             // radar bezel thickness (reference units)
	int wellDepthRef;         // inner shadow thickness of recessed wells (reference units)
	bool brushedChassis;      // fine horizontal grain on the chassis (material texture)
	bool junctionRibs;        // raised structural ribs between modules: the modules read as windows in one chassis
	bool moduleLeds;          // small illuminated segment group in each module header
	Corner windowCorner;      // outer corner profile of the General Powers popup and the Battle Overview
	int windowCornerRef;      // its size in reference units
	// ---- heavy construction vocabulary (all false = the USA precision-machined look) -------------------------------------
	bool heavyBevel;          // plates carry a second, inner bevel line: thicker structural edges
	bool rivets;              // domed rivet / bolt heads along ribs, girders and bezels (instead of slotted screws only)
	bool cornerCaps;          // raised square caps on the shell's top corners and on the General Powers / overview corners
	bool moduleBrackets;      // reinforcing brackets on the corners of recessed modules
	bool girder;              // a riveted structural girder band across the top of the chassis
};

// True when two themes differ in more than colour (shape / material language). Used by tests and tools to keep a new
// faction from being a recolour of another one.
inline bool differsBeyondColour(const Theme &a, const Theme &b)
{
	return a.shellCorner != b.shellCorner || a.shellCornerRef != b.shellCornerRef || a.moduleCorner != b.moduleCorner ||
	       a.moduleCornerRef != b.moduleCornerRef || a.plateCorner != b.plateCorner || a.plateCornerRef != b.plateCornerRef ||
	       a.bevelModules != b.bevelModules || a.fasteners != b.fasteners || a.seams != b.seams || a.vents != b.vents ||
	       a.tickMarks != b.tickMarks || a.stateRails != b.stateRails ||
	       a.bezelRef != b.bezelRef || a.wellDepthRef != b.wellDepthRef || a.brushedChassis != b.brushedChassis ||
	       a.junctionRibs != b.junctionRibs || a.moduleLeds != b.moduleLeds ||
	       a.windowCorner != b.windowCorner || a.windowCornerRef != b.windowCornerRef || a.heavyBevel != b.heavyBevel ||
	       a.rivets != b.rivets || a.cornerCaps != b.cornerCaps || a.moduleBrackets != b.moduleBrackets || a.girder != b.girder;
}

// USA Theme (Design2): a precision-manufactured command console. Depth hierarchy, darkest last:
//   level 0 chassis   - mid gunmetal with a fine grain and raised structural ribs between the modules
//   level 1 module    - recessed navy panels set into the chassis
//   level 2 well      - near-black equipment bays and displays
//   level 3 control   - raised, lighter machined plates (the interactive things)
//   level 4 content   - icons, LEDs, rank, state
// Blue is illumination (LEDs, hover, ready), not an outline colour. Red is reserved for the attack warning, gold for rank.
inline const Theme &usa()
{
	static const Theme t = {
		"USA",
		/* plate     */ {32, 40, 54, 255}, /* plateBand */ {44, 54, 72, 255}, /* module */ {21, 27, 40, 255},
		/* moduleLow */ {15, 20, 31, 255}, /* well */ {8, 11, 18, 255},      /* wellEdge */ {50, 64, 90, 255},
		/* outline   */ {22, 32, 54, 255}, /* accent */ {72, 126, 208, 255}, /* accentDim */ {36, 60, 106, 255},
		/* bevelLight*/ {92, 110, 142, 255}, /* bevelDark */ {5, 7, 12, 255},
		/* icon      */ {198, 214, 240, 255}, /* iconDim */ {92, 104, 124, 255},
		/* rankLit   */ {236, 200, 88, 255},  /* rankDim */ {50, 58, 74, 255},
		/* ready     */ {96, 156, 236, 255},  /* warning */ {224, 24, 24, 255}, /* warningDark */ {70, 14, 14, 255},
		/* warningInk*/ {255, 255, 255, 255},
		/* control   */ {46, 58, 80, 255}, /* controlLow */ {31, 39, 55, 255}, /* chassisLine */ {37, 46, 62, 255},
		/* shell   */ Corner::Chamfer, 14, /* module */ Corner::Chamfer, 4, /* plate */ Corner::Chamfer, 3,
		/* bevel   */ true, /* fasteners */ true, /* seams */ true, /* vents */ true, /* ticks */ true,
		/* stateRails */ true,
		/* bezel   */ 5, /* wellDepth */ 2,
		/* grain   */ true, /* ribs */ true, /* moduleLeds */ true,
		/* popup/overview window corner */ Corner::Chamfer, 8,
		/* heavyBevel, rivets, cornerCaps, moduleBrackets, girder */ false, false, false, false, false,
	};
	return t;
}

// China Theme: a heavy military-industrial console. Same regions and behaviour as every faction; the identity comes from the
// construction, not the colour. Square, reinforced shoulders (raised corner caps, a riveted girder across the top, brackets on
// the recessed modules), double-edged structural plates, domed rivets instead of slotted screws, no ventilation grilles, no
// brushed grain (flat plate steel), deeper wells. Dark steel with a warm, slightly olive cast; the faction accent is a restrained
// brick red; available / hover is red-orange, the warning (attack, insufficient power) is an amber lamp, rank stays gold.
inline const Theme &china()
{
	static const Theme t = {
		"China",
		/* plate     */ {40, 41, 38, 255}, /* plateBand */ {58, 58, 53, 255}, /* module */ {26, 27, 25, 255},
		/* moduleLow */ {19, 20, 18, 255}, /* well */ {9, 9, 8, 255},        /* wellEdge */ {74, 70, 62, 255},
		/* outline   */ {30, 28, 25, 255}, /* accent */ {196, 62, 44, 255}, /* accentDim */ {104, 38, 30, 255},
		/* bevelLight*/ {124, 119, 108, 255}, /* bevelDark */ {6, 6, 5, 255},
		/* icon      */ {224, 220, 206, 255}, /* iconDim */ {118, 112, 100, 255},
		/* rankLit   */ {232, 192, 84, 255},  /* rankDim */ {66, 62, 50, 255},
		/* ready     */ {238, 106, 74, 255},   /* warning */ {244, 158, 22, 255}, /* warningDark */ {86, 50, 8, 255},
		/* warningInk*/ {26, 16, 2, 255},
		/* control   */ {60, 60, 55, 255}, /* controlLow */ {41, 41, 37, 255}, /* chassisLine */ {47, 48, 44, 255},
		/* shell   */ Corner::Square, 0, /* module */ Corner::Square, 0, /* plate */ Corner::Square, 0,
		/* bevel   */ true, /* fasteners */ true, /* seams */ true, /* vents */ false, /* ticks */ true,
		/* stateRails */ true,
		/* bezel   */ 5, /* wellDepth */ 3,
		/* grain   */ false, /* ribs */ true, /* moduleLeds */ true,
		/* popup/overview window corner */ Corner::Square, 0,
		/* heavyBevel, rivets, cornerCaps, moduleBrackets, girder */ true, true, true, true, true,
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
inline bool implemented(Faction f) { return f == Faction::Usa || f == Faction::China; }

inline const Theme &themeFor(Faction f)
{
	if (f == Faction::China) return china();
	return usa();            // future factions add their own constant here; until then the safe default is USA
}

}  // namespace EnhancedHudTheme
