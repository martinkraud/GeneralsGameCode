// SPDX-License-Identifier: GPL-3.0-or-later
// Enhanced gameplay HUD (USA prototype): the live switch. `requested` is the user's Options > Gameplay > Interface
// choice (persisted by the Modern Experience settings); `active` is whether the Enhanced presentation is in use *right
// now* (requested, modern display, supported faction, a surface that can hold the island). Original is the default and
// the fallback for everything else; nothing else in the engine changes when `active` is false.
#pragma once

#include "Common/EnhancedHudLayout.h"

namespace EnhancedHud
{

// Shared Enhanced HUD Framework version. v1 is FROZEN (see docs/modernization/ENHANCED_HUD_FRAMEWORK_V1.md): the geometry,
// responsive rules, window ownership, hit-test mechanism and state vocabulary shared by every faction. Factions supply a Theme
// (EnhancedHudTheme) and nothing else; changing the frozen contract requires a deliberate version bump, a documented reason and
// a new native review.
constexpr int kFrameworkVersion = 1;

struct State
{
	bool requested = false;
	bool active = false;
	// presentation cost counters (read by the developer diagnostics only): frames drawn and the nanoseconds the frame draw took
	unsigned long long drawFrames = 0, drawNanos = 0;
	EnhancedHudLayout::Result layout;
	EnhancedHudTheme::Faction faction = EnhancedHudTheme::Faction::Usa;
};

inline State &state()
{
	static State s;
	return s;
}

inline void setRequested(bool requested) { state().requested = requested; }
inline bool requested() { return state().requested; }
inline bool active() { return state().active; }
inline const EnhancedHudLayout::Result &layout() { return state().layout; }
inline EnhancedHudTheme::Faction faction() { return state().faction; }
// The presentation theme of the faction the HUD is currently built for (read by the single renderer).
inline const EnhancedHudTheme::Theme &theme() { return EnhancedHudTheme::themeFor(state().faction); }

// Re-evaluates the decision for the given side/surface and recomputes the layout. Returns the new `active`.
// `commandGrid` is the control bar's command grid (12 slots in Generals, 14 in Zero Hour); it is selected before anything is laid out.
inline bool update(bool modernDisplay, bool classic, const char *side, int surfaceW, int surfaceH, double layoutScale,
                   EnhancedHudLayout::CommandGrid commandGrid = EnhancedHudLayout::CommandGrid::Standard)
{
	EnhancedHudLayout::setCommandGrid(commandGrid);
	State &s = state();
	s.layout = EnhancedHudLayout::Result();
	s.active = false;
	s.faction = EnhancedHudTheme::factionOfSide(side);
	if (!EnhancedHudLayout::shouldBeActive(s.requested, modernDisplay, classic, side, surfaceW, surfaceH))
		return false;
	s.layout = EnhancedHudLayout::compute(surfaceW, surfaceH, layoutScale);
	s.active = s.layout.valid;
	return s.active;
}

inline void recordDraw(unsigned long long nanos) { State &s = state(); ++s.drawFrames; s.drawNanos += nanos; }

inline void deactivate()
{
	State &s = state();
	s.active = false;
	s.layout = EnhancedHudLayout::Result();
}

}  // namespace EnhancedHud
