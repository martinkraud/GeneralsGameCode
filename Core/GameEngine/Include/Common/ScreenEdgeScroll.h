// SPDX-License-Identifier: GPL-3.0-or-later
// X64.52B: the pure rules of screen-edge camera scrolling (LookAtTranslator), so they can be unit tested without the engine.
#pragma once

namespace ScreenEdgeScroll
{
// Width of the edge band in pixels (a cursor within this many pixels of the display border scrolls).
constexpr int kEdgeSize = 3;

struct Hit
{
	bool left = false, right = false, top = false, bottom = false;
	bool any() const { return left || right || top || bottom; }
};

// Which borders the cursor (display coordinates) touches.
inline Hit hit(int x, int y, unsigned width, unsigned height)
{
	Hit h;
	h.left = x < kEdgeSize;
	h.top = y < kEdgeSize;
	h.right = width >= (unsigned)kEdgeSize && x >= (int)width - kEdgeSize;
	h.bottom = height >= (unsigned)kEdgeSize && y >= (int)height - kEdgeSize;
	return h;
}

// May the cursor scroll the camera at the screen edge?
//   enabledWindowed / enabledFullscreen: the two saved preferences (Options.ini ScreenEdgeScrollEnabledIn{Windowed,Fullscreen}App).
//   appIsWindowed: how the application currently presents.
//   singlePresentationMode: the modern AMD64 runtime always presents in a window, so there is no separate "fullscreen" state for the player to have chosen.
//     A "windowed = no" left in Options.ini by an earlier build (the original game and every Win32 build default it to no, and the Options screen writes the value
//     back) must not silently switch the edge off forever with no control to bring it back: there either preference being enabled enables the edge.
inline bool allowed(bool enabledWindowed, bool enabledFullscreen, bool appIsWindowed, bool singlePresentationMode)
{
	if (singlePresentationMode)
		return enabledWindowed || enabledFullscreen;
	return appIsWindowed ? enabledWindowed : enabledFullscreen;
}
}
