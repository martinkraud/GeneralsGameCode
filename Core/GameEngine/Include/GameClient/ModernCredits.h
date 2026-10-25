// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience credits screen: the retail CreditsMenu.wnd scroller (CreditsManager draws the text) over the 3D shell map with a Modern scrim band behind the text column instead of the
// retail black screen. The text uses the Modern type roles and the title palette (see Credits.cpp). Classic: attach() returns false and nothing changes.
#pragma once

class GameWindow;

namespace ModernCredits
{
bool attach(GameWindow *creditsParent);   // after the retail Init created the layout
void update();                            // once per frame: follows the surface / UI scale
void forget();
bool active();
}
