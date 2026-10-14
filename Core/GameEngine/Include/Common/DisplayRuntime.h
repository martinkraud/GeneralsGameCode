// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33: the live display state of a Modern Experience launch. One place holds "what is the display right now":
// the window client / back buffer / logical display size, the UI layout derived from it and the settings that
// produced it. Everything that needs to know (window creation, display init, WND layout, camera projection, font
// scaling, mouse mapping, Options) asks here instead of assuming 800x600.
//
// `modern()` is false in Classic and in every diagnostic profile: those keep the accepted fixed 800x600 logical
// display, the review scale and the stretched swap chain untouched.
#pragma once

#include "Common/DisplayGeometry.h"
#include "Common/DisplaySettings.h"

namespace DisplayRuntime
{

struct State
{
	bool modern = false;
	DisplayConfig::Settings settings;                 // as persisted (requested)
	DisplayConfig::Resolved resolved;                 // as applied (effective)
	DisplayGeometry::Layout layout;                     // derived from the live client size
};

inline State &state()
{
	static State s;
	return s;
}

inline bool modern() { return state().modern; }

// Device-layer entry points registered by the presentation owner (W3DDisplay) so engine-level code (Options) can
// ask for a display change without depending on the device library.
using ApplyModeFn = bool (*)(unsigned width, unsigned height, bool borderless, int monitorIndex /* -1 = keep */);
using VsyncFn = void (*)(bool enabled);
inline ApplyModeFn &applyModeHook() { static ApplyModeFn fn = nullptr; return fn; }
inline VsyncFn &vsyncHook() { static VsyncFn fn = nullptr; return fn; }

inline const DisplayGeometry::Layout &layout() { return state().layout; }

// Called once the window/back buffer exists at `client` (or after a runtime change).
inline void setClient(DisplayGeometry::Size client)
{
	State &s = state();
	s.resolved.client = client;
	s.layout = DisplayGeometry::computeLayout(client, s.settings.uiScalePercent);
}

inline void setUiScalePercent(int percent)
{
	State &s = state();
	s.settings.uiScalePercent = percent;
	if (s.resolved.client.valid())
		s.layout = DisplayGeometry::computeLayout(s.resolved.client, percent);
}

// Effective UI scale as a factor (1.0 when not modern): fonts and ui art multiply by this.
inline double uiScale()
{
	return state().modern ? state().layout.scale() : 1.0;
}

// Scale of the 800x600 reference layout for code that positions things itself (HUD text, slider thumbs, window
// animations). Classic: the per-axis display/800 and display/600 ratios, exactly as before. Modern: the uniform UI
// scale, so nothing is stretched on a wide surface.
inline double referenceScaleX(int displayWidth)
{
	return state().modern ? state().layout.scale() : displayWidth / 800.0;
}

inline double referenceScaleY(int displayHeight)
{
	return state().modern ? state().layout.scale() : displayHeight / 600.0;
}

// Where the reference origin sits on the physical surface for centered content ((0,0) in Classic).
inline DisplayGeometry::Point referenceOrigin()
{
	if (!state().modern) return {0, 0};
	return {state().layout.canvas.x, state().layout.canvas.y};
}

// Control bar (bottom anchored): the bar art is authored in the scheme's creation resolution. Modern: one uniform
// factor (UI scale x 800/creation width) and the bottom-anchored canvas origin; Classic: the old per-axis ratio.
inline DisplayGeometry::Point controlBarOrigin()
{
	if (!state().modern) return {0, 0};
	return DisplayGeometry::anchorOrigin(state().layout, DisplayGeometry::Anchor::Bottom);
}

inline float controlBarMultiplier(int, int creationWidth, float classicRatio)
{
	if (!state().modern || creationWidth <= 0) return classicRatio;
	return static_cast<float>(state().layout.scale() * DisplayGeometry::kReferenceSize.width / creationWidth);
}

struct IntPoint { int x; int y; };
template <typename Coord>
inline Coord offsetByControlBarOrigin(Coord offset)
{
	const DisplayGeometry::Point origin = controlBarOrigin();
	offset.x += origin.x;
	offset.y += origin.y;
	return offset;
}

// The expected window client size for the "client must be exact" checks; {0,0} when the classic rules apply.
inline DisplayGeometry::Size expectedClient()
{
	return state().modern ? state().resolved.client : DisplayGeometry::Size{};
}

}  // namespace DisplayRuntime
