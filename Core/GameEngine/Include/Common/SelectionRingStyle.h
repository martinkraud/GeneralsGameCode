// SPDX-License-Identifier: GPL-3.0-or-later
// Selection ring (the ground circle drawn around selected units and structures): pure, engine-free presentation rules.
//
// The ring is drawn by W3DView::drawNativeWorldOverlays as a 2D polyline on the terrain around every selected drawable. It is local presentation only: it reads no simulation state
// that it does not already display, writes nothing, takes no part in picking or selection (those use the drawable's geometry, never this ring) and is never part of a replay, a save or
// a network message.
//
//   * Classic (and any non-Modern run): the retail ring, exactly as before (opaque 2 px line, green own / red enemy / white other), and the Modern preference is ignored.
//   * Modern, Selection Ring On (default): the same ring, restrained: a softer, less saturated colour of the same family, translucent, and a thinner line.
//   * Modern, Selection Ring Off: the ring is not drawn. The health bar, the selected-object panel and the command UI are unaffected.
#pragma once

#include <algorithm>

namespace SelectionRing
{
enum class Relation { Own, Enemy, Other };

struct Style
{
	bool draw = true;
	unsigned char r = 255, g = 255, b = 255, a = 255;
	float width = 2.0f;
};

// The local Modern preference (Options > Modern Experience > Selection Ring). Default On; written by the options code and at start-up, read once per frame by the renderer.
inline bool &preferenceRef() { static bool on = true; return on; }
inline void setPreference(bool on) { preferenceRef() = on; }
inline bool preference() { return preferenceRef(); }
// Developer aid for the before / after captures only (GENERALS_AUTOSELECT probe): draws the retail ring in a Modern session so both can be compared at the same size. Never set by the game.
inline bool &forceRetailRef() { static bool on = false; return on; }

// The retail ring (Classic). Colours and width are the values the native world overlay always used.
inline Style retailStyle(Relation relation)
{
	Style s;
	s.width = 2.0f;
	s.a = 255;
	switch (relation)
	{
		case Relation::Own: s.r = 40; s.g = 255; s.b = 40; break;
		case Relation::Enemy: s.r = 255; s.g = 40; s.b = 40; break;
		default: s.r = 255; s.g = 255; s.b = 255; break;
	}
	return s;
}

// The restrained Modern ring: lower saturation and brightness than the retail primaries (still unmistakably green / red / white, readable on bright desert and dark terrain),
// translucent so the terrain and the unit stay visible through it, and a thinner line that follows the surface height so it does not look heavy on tall displays.
constexpr unsigned char kModernAlpha = 190;
inline float modernWidth(int surfaceHeight)
{
	const float w = 1.5f * static_cast<float>(surfaceHeight > 0 ? surfaceHeight : 1080) / 1080.0f;
	return std::min(3.0f, std::max(1.5f, w));
}
inline Style modernStyle(Relation relation, int surfaceHeight)
{
	Style s;
	s.width = modernWidth(surfaceHeight);
	s.a = kModernAlpha;
	switch (relation)
	{
		case Relation::Own: s.r = 120; s.g = 214; s.b = 128; break;
		case Relation::Enemy: s.r = 228; s.g = 104; s.b = 96; break;
		default: s.r = 226; s.g = 226; s.b = 226; break;
	}
	return s;
}

// `modernActive` = the Modern Experience is running and Classic is not.
inline Style styleFor(bool modernActive, bool preferenceOn, Relation relation, int surfaceHeight)
{
	if (!modernActive || forceRetailRef())
		return retailStyle(relation);
	if (!preferenceOn)
	{
		Style off = modernStyle(relation, surfaceHeight);
		off.draw = false;
		return off;
	}
	return modernStyle(relation, surfaceHeight);
}
} // namespace SelectionRing
