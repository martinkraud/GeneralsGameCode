/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once
#include "Common/KindOf.h"
#include "GameLogic/AIStateMachine.h"
#include "GameLogic/LocomotorSet.h"

namespace GroundTranslation
{
	const KindOfMaskType& excludedKinds();
	bool supportsMovement(AIStateType state);
	inline bool supportsOrientationMovement(AIStateType state, bool infantry, bool aimingOrFiring)
	{
		if (!supportsMovement(state)) return false;
		if (!infantry) return true;
		if (aimingOrFiring) return false;
		return state == AI_IDLE || state == AI_MOVE_TO || state == AI_WAIT
			|| state == AI_MOVE_OUT_OF_THE_WAY || state == AI_MOVE_AND_TIGHTEN;
	}
	inline bool supportsSurfaces(unsigned int surfaces)
	{
		return (surfaces & LOCOMOTORSURFACE_GROUND) != 0
			&& (surfaces & ~(LOCOMOTORSURFACE_GROUND | LOCOMOTORSURFACE_RUBBLE)) == 0;
	}

	// The runtime ThingTemplate and test mask adapter expose these native queries.
	// No template names or mutable world state participate in this policy.
	template<class KindView> bool supportsKinds(const KindView& kinds)
	{
		return (kinds.isKindOf(KINDOF_INFANTRY) || kinds.isKindOf(KINDOF_VEHICLE))
			&& !kinds.isAnyKindOf(excludedKinds());
	}

	struct Eligibility
	{
		bool boundVisibleAlive;
		bool ordinaryGroundKind;
		bool blocked; // Containment, airborne, disabled, ability/deploy or script ownership.
		bool ordinaryAI;
		bool groundLocomotor;
		AIStateType movement;
		unsigned int drawModuleCount;
		bool allDrawModulesSupported;
		void addDrawModule(bool supported)
		{
			++drawModuleCount;
			allDrawModulesSupported = allDrawModulesSupported && supported;
		}
	};
	bool canInterpolate(const Eligibility& capabilities);
}
