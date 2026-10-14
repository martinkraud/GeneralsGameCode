// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 FIX1: z-order helpers for gadgets that are created at runtime next to window-file controls.
//
// The window system draws a parent's children from the tail of the child list to the head, so the HEAD is the front.
// A window-file layout is created in file order, which places later (lower-on-screen, usually) controls in front
// of earlier ones; the original dropdowns rely on that to open over the controls below them. A gadget that is
// created at runtime lands at the very front, so a runtime-created neighbour of a dropdown paints OVER that
// dropdown's expanded list (first native review of x64-33-modern-display: dropdown contents behind other
// controls). These helpers place a runtime gadget deliberately instead of by creation time.
#pragma once

#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"

namespace GadgetLayering
{

// Moves `window` so it is drawn directly BEHIND `reference` (and below everything that is in front of `reference`).
// Both must share a parent. No-op when they do not.
inline void placeBehind(GameWindow *window, GameWindow *reference)
{
	if (!window || !reference || window == reference)
		return;
	GameWindow *parent = reference->winGetParent();
	if (!parent || window->winGetParent() != parent)
		return;
	TheWindowManager->unlinkChildWindow(window);
	GameWindow *next = reference->winGetNext();
	if (next)
		TheWindowManager->insertWindowAheadOf(window, next);
	else
		TheWindowManager->addWindowToParentAtEnd(window, parent);
}

// Moves `window` so it is drawn directly IN FRONT of `reference`.
inline void placeInFront(GameWindow *window, GameWindow *reference)
{
	if (!window || !reference || window == reference)
		return;
	GameWindow *parent = reference->winGetParent();
	if (!parent || window->winGetParent() != parent)
		return;
	TheWindowManager->unlinkChildWindow(window);
	TheWindowManager->insertWindowAheadOf(window, reference);
}

// True when `a` is drawn in front of `b` (same parent): used by tests and diagnostics.
inline bool isInFront(const GameWindow *a, const GameWindow *b)
{
	for (const GameWindow *w = a; w; w = const_cast<GameWindow *>(w)->winGetNext())
		if (w == b)
			return true;
	return false;
}

}  // namespace GadgetLayering
