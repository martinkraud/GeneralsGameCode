// SPDX-License-Identifier: GPL-3.0-or-later
// Enhanced gameplay HUD (USA prototype): the live switch. `requested` is the user's Options > Gameplay > Interface
// choice (persisted by the Modern Experience settings); `active` is whether the Enhanced presentation is in use *right
// now* (requested, modern display, supported faction, a surface that can hold the island). Original is the default and
// the fallback for everything else; nothing else in the engine changes when `active` is false.
#pragma once

#include "Common/EnhancedHudLayout.h"

namespace EnhancedHud
{

struct State
{
	bool requested = false;
	bool active = false;
	EnhancedHudLayout::Result layout;
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

// Re-evaluates the decision for the given side/surface and recomputes the layout. Returns the new `active`.
inline bool update(bool modernDisplay, bool classic, const char *side, int surfaceW, int surfaceH, double layoutScale)
{
	State &s = state();
	s.layout = EnhancedHudLayout::Result();
	s.active = false;
	if (!EnhancedHudLayout::shouldBeActive(s.requested, modernDisplay, classic, side, surfaceW, surfaceH))
		return false;
	s.layout = EnhancedHudLayout::compute(surfaceW, surfaceH, layoutScale);
	s.active = s.layout.valid;
	return s.active;
}

inline void deactivate()
{
	State &s = state();
	s.active = false;
	s.layout = EnhancedHudLayout::Result();
}

}  // namespace EnhancedHud
