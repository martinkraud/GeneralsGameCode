/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "PreRTS.h"
#include "Common/GroundTranslation.h"
#include "Common/GroundTranslationPolicy.h"

namespace GroundTranslation
{
	static bool enabled = false;
	bool isEnabled() { return enabled; }
	void setEnabled(bool value) { enabled = value; }
	const KindOfMaskType& excludedKinds()
	{
		static const KindOfMaskType excluded = []() {
			KindOfMaskType result;
			const KindOfType kinds[] = {KINDOF_AIRCRAFT, KINDOF_PROJECTILE, KINDOF_STRUCTURE,
				KINDOF_IMMOBILE, KINDOF_DOZER, KINDOF_HARVESTER, KINDOF_TRANSPORT,
				KINDOF_BOAT, KINDOF_CLIFF_JUMPER, KINDOF_DRONE, KINDOF_MOB_NEXUS,
				KINDOF_SPAWNS_ARE_THE_WEAPONS, KINDOF_PORTABLE_STRUCTURE, KINDOF_INERT};
			for (KindOfType kind : kinds) result.set(kind);
			return result;
		}();
		return excluded;
	}
	bool supportsMovement(AIStateType state)
	{
		switch (state)
		{
			case AI_IDLE: case AI_MOVE_TO: case AI_WAIT:
			case AI_ATTACK_POSITION: case AI_ATTACK_OBJECT: case AI_FORCE_ATTACK_OBJECT:
			case AI_ATTACK_AND_FOLLOW_OBJECT: case AI_ATTACK_MOVE_TO:
			case AI_GUARD:
#if RTS_ZEROHOUR
			case AI_GUARD_RETALIATE:
#endif
			case AI_MOVE_OUT_OF_THE_WAY: case AI_MOVE_AND_TIGHTEN:
				return true;
			default: return false; // Waypoints, entry/exit/docking and bespoke states fail closed.
		}
	}
	bool canInterpolate(const Eligibility& c)
	{
		return isEnabled() && c.boundVisibleAlive && c.ordinaryGroundKind && !c.blocked
			&& c.ordinaryAI && c.groundLocomotor && supportsMovement(c.movement)
			&& c.drawModuleCount != 0 && c.allDrawModulesSupported;
	}
}
