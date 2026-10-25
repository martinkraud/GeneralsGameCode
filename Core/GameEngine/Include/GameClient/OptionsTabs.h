// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience Options (X64.33 categories, redesigned by the Modern Frontend milestone): a left category rail (Display / Graphics / Audio / Controls / Gameplay / Modern Experience / Network),
// a scrollable column of label + control rows generated from Common/OptionsPageLayout.h's page tables and laid out by Common/ModernScreenLayout.h, an Apply / Defaults / Cancel action row and a
// dirty-state line. Built from the packaged OptionsMenu.wnd controls at runtime: the retail gadgets keep their callbacks (they are skinned with the Modern component set and re-placed),
// nothing is replaced on disk and Classic keeps the retail single page untouched.
#pragma once

#include "Common/ModernScreenLayout.h"

class GameWindow;

namespace OptionsTabs
{
// Builds the Modern Options screen over the retail one and shows the first page. Returns false (retail page untouched) in Classic, in diagnostic profiles and when the layout is not the
// retail Options layout.
bool create();

// Tab button routing from OptionsMenuSystem: true when `controlId` was one of the tabs.
bool onButton(int controlId);

// Shows/hides the Advanced Display panel as a sub-screen of the Modern panel (title and tabs never collide with it).
void setAdvancedOpen(bool open);

// Once per frame from OptionsMenuUpdate: re-lays the screen out when the surface or the UI scale changed, tracks keyboard focus and keeps the dirty line / slider values current.
void update();

// Keyboard routing from OptionsMenuInput (GWM_CHAR): Page Up / Page Down switch category, Ctrl+Home / Ctrl+End jump to the first / last. True when consumed.
bool onKey(int key, int state);

// Mouse wheel routing (GWM_WHEEL_UP / GWM_WHEEL_DOWN): scrolls the rows of a page that does not fit. True when consumed.
bool onWheel(bool up);

// True when a setting differs from the value it had when the screen opened.
bool dirty();

// Leaving without applying: true when the caller may leave at once (nothing changed or not a Modern screen); false when a confirmation dialog was opened - `onDiscard` is called if the
// user chooses to discard.
bool requestLeave(void (*onDiscard)());

// The Options screen is closing; windows belong to the layout.
void forget();

// Index of the category currently shown (-1 when tabs are not active).
int activeCategory();

// ---- diagnostics for the audit (read-only) ----
const ModernScreen::Layout &layout();
int firstRow();
}
