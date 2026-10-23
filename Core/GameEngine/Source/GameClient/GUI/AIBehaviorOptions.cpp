// SPDX-License-Identifier: GPL-3.0-or-later
// X64.51: stored preference of the "AI Behaviour" selector. See AIBehaviorOptions.h.
#include "PreRTS.h"

#include "GameClient/AIBehaviorOptions.h"

namespace AIBehaviorOptions
{
const char *preferenceValue(AIBehaviorMode mode)
{
	return mode == AI_BEHAVIOR_ENHANCED ? "Enhanced" : "Original";
}

bool parsePreference(const char *text, AIBehaviorMode &mode)
{
	if (!text)
		return false;
	if (!_stricmp(text, "Enhanced")) { mode = AI_BEHAVIOR_ENHANCED; return true; }
	if (!_stricmp(text, "Original")) { mode = AI_BEHAVIOR_ORIGINAL; return true; }
	return false;
}
}
