// SPDX-License-Identifier: GPL-3.0-or-later
// X64.51: the stored preference of the "AI Behaviour" selector of the Skirmish setup screen (Original / Enhanced), shared by both titles. The dropdown itself is built by
// GameClient/MatchOptionControls.h (X64.52A).
//
// The selector describes HOW the computer opponents play (the game rule AIBehaviorMode of Common/AIBehavior.h), never how strong they are: difficulty stays the
// per-slot Easy / Medium / Brutal choice. It is part of the Modern Experience (hidden with -classic: the rule is then Original). The value lives in the GameInfo of the
// match, so it follows the same synchronised path as every other match rule (options string, replay header, save).
#pragma once

#include "Common/AIBehavior.h"

namespace AIBehaviorOptions
{
// The stored preference value ("Original" / "Enhanced"); anything else is rejected.
const char *preferenceValue(AIBehaviorMode mode);
bool parsePreference(const char *text, AIBehaviorMode &mode);
}
