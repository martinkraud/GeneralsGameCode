// SPDX-License-Identifier: GPL-3.0-or-later
// Enhanced gameplay HUD: the Battle Overview skin for the retail in-game player list (Diplomacy.wnd). Presentation only: the
// retail windows, texts, mute buttons and their logic are untouched; the skin is the root window's draw function.
#pragma once

#include "Lib/BaseType.h"

class GameWindow;

namespace EnhancedHudOverview
{
// Enhanced active: skin the overview window (idempotent). Otherwise: restore the retail look if it was skinned.
void attach( GameWindow *root );
// The window is going away (restore = FALSE) or must show the retail look again (restore = TRUE).
void forget( GameWindow *root, Bool restore );
// Enhanced shows the player table in skirmish (retail shows the briefing list there).
Bool showPlayerTable();
}
