// SPDX-License-Identifier: GPL-3.0-or-later
// X64.52A: engine side of the Superweapons match rule. See GameLogic/SuperweaponRule.h and Common/SuperweaponLimit.h.
#include "PreRTS.h"

#include "Common/Player.h"
#include "Common/SuperweaponLimit.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/RebuildHoleBehavior.h"
#include "GameLogic/Object.h"
#include "GameLogic/SuperweaponRule.h"

namespace
{
void countSuperweapon(Object *obj, void *userData)
{
	if (obj->isEffectivelyDead())
		return;
	const ThingTemplate *tmpl = obj->getTemplate();
	if (!tmpl)
		return;
	unsigned *count = static_cast<unsigned *>(userData);
	if (tmpl->isSuperweaponStructure())
	{
		++*count;
		return;
	}
	if (obj->isKindOf(KINDOF_REBUILD_HOLE))
	{
		RebuildHoleBehaviorInterface *hole = RebuildHoleBehavior::getRebuildHoleBehaviorInterfaceFromObject(obj);
		const ThingTemplate *rebuild = hole ? hole->getRebuildTemplate() : nullptr;
		if (rebuild && rebuild->isSuperweaponStructure())
			++*count;
	}
}
}

namespace SuperweaponRule
{
unsigned countOwned(const Player *player)
{
	unsigned count = 0;
	if (player)
		player->iterateObjects(countSuperweapon, &count);
	return count;
}

bool blocksStart(const Player *player, const ThingTemplate *what)
{
	if (!what || !TheGameLogic)
		return false;
	const unsigned limit = TheGameLogic->getSuperweaponLimit();
	if (!SuperweaponLimit::isActive(limit) || !what->isSuperweaponStructure())
		return false;
	if (limit == SuperweaponLimit::kDisabled)
		return true;
	return SuperweaponLimit::blocks(limit, countOwned(player));
}
}
