// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 FIX2 Options categories (Modern Experience): DISPLAY / GRAPHICS / AUDIO / CONTROLS / GAMEPLAY / NETWORK.
// Built from the packaged OptionsMenu.wnd controls at runtime: the retail panels are re-arranged as pages, new
// gadgets share their draw data. Nothing is replaced on disk and Classic keeps the retail single page.
#pragma once

class GameWindow;

namespace OptionsTabs
{
// Creates the tab strip and shows the first page. Returns false (retail page untouched) in Classic, in diagnostic
// profiles and when the layout is not the retail Options layout.
bool create();

// Tab button routing from OptionsMenuSystem: true when `controlId` was one of the tabs.
bool onButton(int controlId);

// The Options screen is closing; windows belong to the layout.
void forget();

// Index of the category currently shown (-1 when tabs are not active).
int activeCategory();
}
