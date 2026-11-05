// SPDX-License-Identifier: GPL-3.0-or-later
// See ScalabilityMultiplier.h.
#include "PreRTS.h"

#include "Common/ScalabilityMultiplier.h"
#include "Common/KindOf.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace ScalabilityMultiplier
{
namespace
{
unsigned s_copies = 0;
unsigned s_frame = 0;
bool s_parsed = false;
bool s_done = false;

unsigned injectClones(unsigned copies)
{
	std::vector<Object *> sources;
	for (Object *o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
	{
		const ThingTemplate *t = o->getTemplate();
		Player *owner = o->getControllingPlayer();
		if (!t || !owner || owner->getPlayerType() != PLAYER_COMPUTER) continue;
		if (!o->getAIUpdateInterface() || o->isEffectivelyDead() || o->getContainedBy()) continue;
		if (t->isKindOf(KINDOF_STRUCTURE) || t->isKindOf(KINDOF_AIRCRAFT) || t->isKindOf(KINDOF_PROJECTILE) || t->isKindOf(KINDOF_IMMOBILE)) continue;
		sources.push_back(o);
	}
	unsigned created = 0;
	Pathfinder *pathfinder = TheAI ? TheAI->pathfinder() : nullptr;
	if (!pathfinder) return 0;
	for (size_t i = 0; i < sources.size(); ++i)
	{
		Object *src = sources[i];
		for (unsigned k = 0; k < copies; ++k)
		{
			const float angle = static_cast<float>(i * 7 + k * 3) * 2.399963f;   // golden angle: a fixed spiral around the original
			const float radius = 14.0f + 9.0f * static_cast<float>(1 + k);
			Coord3D pos = *src->getPosition();
			pos.x += radius * cosf(angle);
			pos.y += radius * sinf(angle);
			if (!pathfinder->adjustDestination(src, src->getAIUpdateInterface()->getLocomotorSet(), &pos)) continue;
			pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
			Object *copy = TheThingFactory->newObject(src->getTemplate(), src->getTeam());
			if (!copy) continue;
			copy->setOrientation(src->getOrientation());
			copy->setPosition(&pos);
			++created;
		}
	}
	return created;
}
}

void onLogicUpdate()
{
	if (s_done) return;
	if (!s_parsed)
	{
		s_parsed = true;
		const char *v = getenv("GENERALS_SIM_CLONE");
		unsigned k = 0, f = 0;
		if (v && sscanf(v, "%u@%u", &k, &f) == 2 && k > 0) { s_copies = k; s_frame = f; }
		else s_done = true;
	}
	if (s_done || !TheGameLogic || TheGameLogic->isInShellGame() || !TheGameLogic->isInGame()) return;
	if (TheGameLogic->getFrame() != s_frame) return;
	s_done = true;
	const unsigned made = injectClones(s_copies);
	printf("GENERALS_SIM_CLONE: %u copies added at frame %u (objects now %u)\n", made, static_cast<unsigned>(TheGameLogic->getFrame()), static_cast<unsigned>(TheGameLogic->getObjectCount()));
	fflush(stdout);
}
}
