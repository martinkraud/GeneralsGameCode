// SPDX-License-Identifier: GPL-3.0-or-later
// X64.51: the "AI Behaviour" selector of the Skirmish setup screen (Original / Enhanced), shared by both titles.
//
// The selector describes HOW the computer opponents play (the game rule AIBehaviorMode of Common/AIBehavior.h), never how strong they are: difficulty stays the
// per-slot Easy / Medium / Brutal choice. It is part of the Modern Experience (hidden with -classic: the rule is then Original). The value lives in the GameInfo of the
// match, so it follows the same synchronised path as every other match rule (options string, replay header, save).
#pragma once

#include "Common/AIBehavior.h"
#include "Common/NameKeyGenerator.h"

class GameWindow;

namespace AIBehaviorOptions
{
struct Rect { int x; int y; int w; int h; };

// Creates the label and the combo box inside `parent` (once; a second call returns the existing combo). `appearance` is an existing retail combo box whose look is copied,
// `labelDonor` an existing static text whose look is copied (may be null). Returns the combo box, or null when it could not be built.
GameWindow *create(GameWindow *parent, const Rect &label, const Rect &control, GameWindow *appearance, GameWindow *labelDonor, NameKeyType labelId, NameKeyType comboId);

// Selects the entry of `mode` (no message is sent) / reads the selected mode (Original when nothing is selected).
void select(GameWindow *combo, AIBehaviorMode mode);
AIBehaviorMode selected(GameWindow *combo);

// The stored preference value ("Original" / "Enhanced"); anything else is rejected.
const char *preferenceValue(AIBehaviorMode mode);
bool parsePreference(const char *text, AIBehaviorMode &mode);
}
