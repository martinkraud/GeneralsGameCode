/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "PreRTS.h"
#include "Common/GroundTranslation.h"
#include <cstring>

namespace GroundTranslation
{
	static bool enabled = false;
	bool isEnabled() { return enabled; }
	void setEnabled(bool value) { enabled = value; }
	bool supportsTemplate(const char* name)
	{
		// Deliberately no inheritance/general/faction-variant matching. Changed
		// module/state eligibility is checked separately, even for these names.
		static const char* const names[] = {
			"AmericaInfantryRanger", "ChinaInfantryRedguard", "GLAInfantryRebel",
			"AmericaTankCrusader", "ChinaTankBattleMaster", "GLATankScorpion"
		};
		for (unsigned int i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
			if (name != nullptr && std::strcmp(name, names[i]) == 0)
				return true;
		return false;
	}
}
