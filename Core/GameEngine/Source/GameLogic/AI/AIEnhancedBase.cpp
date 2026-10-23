// SPDX-License-Identifier: GPL-3.0-or-later
// X64.52: Enhanced AI "Dynamic Base & Defence" - the shared (Generals + Zero Hour) implementation. Original never reaches any of it (every entry point is behind
// AIPlayer::m_enhanced). The pure decision rules are in Common/AIEnhancedBase.h (unit tested); this file only gathers game state and applies the decisions through the
// existing EA mechanisms: the build list (priority slots), buildBySupplies, buildAIBaseDefense and the normal dozer construction path.
//
// Determinism: all decisions are functions of (match seed, player index, observed game state); the logic RNG is never consumed; iteration follows the build list /
// object list order; threat memory is integer. State that outlives a tick lives in AIPlayer::m_eb and is saved with the AI player.
#include "PreRTS.h"

#include "Common/AIBehavior.h"
#include "Common/AIDiagnostics.h"
#include "Common/AIEnhancedBase.h"
#include "Common/AIEnhancedBaseXfer.h"
#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/AIPlayer.h"
#include "GameLogic/AISkirmishPlayer.h"
#include "GameLogic/Damage.h"
#include "GameLogic/SuperweaponRule.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/TerrainLogic.h"
#include "GameClient/TerrainVisual.h"
#include "Common/BuildAssistant.h"

#include <cmath>
#include <set>
#include <string>
#include <vector>

using namespace AIEnhancedBase;

namespace
{
Bool isEnhancedFillClass(const ThingTemplate *t)
{
	if (!t || !t->isKindOf(KINDOF_STRUCTURE)) return FALSE;
	if (t->isKindOf(KINDOF_COMMANDCENTER) || t->isKindOf(KINDOF_FS_SUPERWEAPON) || t->isKindOf(KINDOF_FS_BASE_DEFENSE) || t->isKindOf(KINDOF_FS_FAKE) || t->isKindOf(KINDOF_FS_POWER))
		return FALSE;
	// production and tech structures of the faction; supply centers are handled by the expansion rule, power by the power rule of the retail loop
	return t->isKindOf(KINDOF_FS_BARRACKS) || t->isKindOf(KINDOF_FS_WARFACTORY) || t->isKindOf(KINDOF_FS_AIRFIELD) || t->isKindOf(KINDOF_FS_FACTORY) ||
		t->isKindOf(KINDOF_FS_TECHNOLOGY) || t->isKindOf(KINDOF_FS_ADVANCED_TECH) || t->isKindOf(KINDOF_FS_STRATEGY_CENTER) || t->isKindOf(KINDOF_FS_INTERNET_CENTER) ||
		t->isKindOf(KINDOF_FS_BLACK_MARKET);
}

Bool isCriticalStructure(const ThingTemplate *t)
{
	return t && (t->isKindOf(KINDOF_COMMANDCENTER) || t->isKindOf(KINDOF_FS_SUPPLY_CENTER));
}

// a slot's structure exists (built, under construction or a rebuild hole standing in for it)
Bool slotHasObject(const BuildListInfo *info)
{
	return info->getObjectID() != INVALID_ID && TheGameLogic->findObjectByID(info->getObjectID()) != nullptr;
}
}

// ------------------------------------------------------------------------------------------------
void AIPlayer::enhancedBaseUpdate()
{
	if (!m_baseCenterSet) return;
	const UnsignedInt frame = TheGameLogic->getFrame();
	if (((frame + (UnsignedInt)m_player->getPlayerIndex() * 7u) % kScanInterval) != 0) return;
	enhancedScanThreats(frame);
	m_eb.threat.decay();
	enhancedThreatDefense(frame);
	enhancedFillIn(frame);
	enhancedExpansion(frame);
}

// ------------------------------------------------------------------------------------------------
/** Where did damage to my base structures come from? Reads only the structures of my own build list (a few dozen objects, once per second). */
void AIPlayer::enhancedScanThreats(UnsignedInt frame)
{
	if (AIDiagnostics::featureOff("threat")) return;
	const UnsignedInt since = m_eb.lastThreatScan;
	m_eb.lastThreatScan = frame;
	for (BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext())
	{
		if (info->getObjectID() == INVALID_ID) continue;
		Object *obj = TheGameLogic->findObjectByID(info->getObjectID());
		if (!obj || obj->getControllingPlayer() != m_player) continue;
		BodyModuleInterface *body = obj->getBodyModule();
		if (!body) continue;
		const UnsignedInt *stamp = body->getLastDamageTimestamp();
		if (!stamp || *stamp <= since || *stamp > frame) continue;
		const DamageInfo *damage = body->getLastDamageInfo();
		if (!damage || damage->in.m_sourceID == INVALID_ID || damage->out.m_actualDamageDealt <= 0.0f) continue;
		Object *source = TheGameLogic->findObjectByID(damage->in.m_sourceID);
		if (!source || m_player->getRelationship(source->getTeam()) != ENEMIES) continue;
		const Coord3D *at = source->getPosition();
		const float dx = at->x - m_baseCenter.x, dy = at->y - m_baseCenter.y;
		if (dx * dx + dy * dy < 1.0f) continue;
		const int sector = ThreatMap::sectorOf(dx, dy);
		m_eb.threat.add(sector);
		AIDiagnostics::count("x52.threat.observations");
		if (AIDiagnostics::enabled())
		{
			char key[48]; std::snprintf(key, sizeof key, "x52.threat.sector.%02d", sector);
			AIDiagnostics::count(key);
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** The direction (radians, from the base center) the base is currently being attacked from, when it has been attacked enough to react. */
Bool AIPlayer::enhancedThreatDirection(Real *angle) const
{
	const int sector = m_eb.threat.dominant(kThreatMinEvents);
	if (sector < 0) return FALSE;
	float sx = 0.0f, sy = 0.0f;
	for (int k = -1; k <= 1; ++k)
	{
		const int s = (sector + k + ThreatMap::kSectors) % ThreatMap::kSectors;
		const float w = (float)m_eb.threat.weight[s];
		sx += w * std::cos(ThreatMap::sectorCenterAngle(s));
		sy += w * std::sin(ThreatMap::sectorCenterAngle(s));
	}
	if (sx == 0.0f && sy == 0.0f) return FALSE;
	*angle = std::atan2(sy, sx);
	return TRUE;
}

// ------------------------------------------------------------------------------------------------
/** A base that keeps being hit from one side asks for one more base defence there (through the existing, threat-steered buildAIBaseDefense). Bounded. */
void AIPlayer::enhancedThreatDefense(UnsignedInt frame)
{
	if (!isSkirmishAI() || m_eb.threatDefenses >= kThreatDefenseCap || AIDiagnostics::featureOff("threatdef")) return;
	if (frame < m_eb.lastThreatDefense + kThreatDefenseCooldown) return;
	if (m_eb.threat.dominant(kThreatDefenseMinEvents) < 0) return;
	// not while a defence order of mine is still waiting to be built, and only with comfortable money (the order competes with unit production)
	const AISideInfo *side = TheAI->getAiData()->m_sideInfo;
	while (side && side->m_side != m_player->getSide()) side = side->m_next;
	if (!side) return;
	const ThingTemplate *defense = TheThingFactory->findTemplate(side->m_baseDefenseStructure1);
	if (!defense) return;
	Int pending = 0;
	for (BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext())
		if (info->isPriorityBuild() && !slotHasObject(info) && info->getTemplateName() == side->m_baseDefenseStructure1 && info->getNumRebuilds() != 0) ++pending;
	if (pending > 0) return;
	if (m_player->getMoney()->countMoney() < defense->calcCostToBuild(m_player) * 2) return;
	m_eb.lastThreatDefense = frame;
	++m_eb.threatDefenses;
	AIDiagnostics::count("x52.threat.defense_ordered");
	Real angle = 0.0f;
	enhancedThreatDirection(&angle);
	AIDiagnostics::event(frame, "x52.threat.defense_ordered", "player=%d threat_deg=%d n=%d", m_player->getPlayerIndex(), (int)(angle * 180.0f / kPi), m_eb.threatDefenses);
	buildAIBaseDefense(false);
}

// ------------------------------------------------------------------------------------------------
/** Spend a real surplus on the base the retail script has not got to yet: rebuild-exhausted critical structures and the next unbuilt production / tech slot. */
void AIPlayer::enhancedFillIn(UnsignedInt frame)
{
	if (!isSkirmishAI() || !m_player->getCanBuildBase() || AIDiagnostics::featureOff("fill")) return;
	if (frame < kFillInStart || frame < m_eb.lastRecovery + kRecoveryCooldown) return;
	const Int money = m_player->getMoney()->countMoney();
	const Int wealthy = TheAI->getAiData()->m_resourcesWealthy;
	if (money < wealthy / 2) return;   // cheap early exit: nothing below can pass
	AIDiagnostics::count("x52.fill.evaluated");

	std::vector<BuildListInfo *> recovery, fill;
	Bool pendingOrder = FALSE;
	for (BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext())
	{
		if (info->getTemplateName().isEmpty() || slotHasObject(info)) continue;
		const ThingTemplate *tmpl = TheThingFactory->findTemplate(info->getTemplateName());
		if (!tmpl) continue;
		// a destroyed slot waiting for its retail rebuild delay is not mine to touch
		if (info->getObjectTimestamp() > 0) continue;
		// a slot the retail loop cannot act on right now (unsafe place, or its placement search just came up empty) neither blocks nor needs me
		if (!isLocationSafe(info->getLocation(), tmpl) || enhancedPlacementRecentlyFailed(info->getLocation(), tmpl)) continue;
		if (info->isPriorityBuild() && info->isBuildable()) { pendingOrder = TRUE; continue; }   // a retail order is waiting to be built
		if (!info->isBuildable())
		{
			if (isCriticalStructure(tmpl) || info->isSupplyBuilding()) recovery.push_back(info);   // allowance spent: the Command Center / a supply center
		}
		else if (isEnhancedFillClass(tmpl) && !info->isAutomaticBuild())   // automatic entries are handled by the repaired automatic path (AI-01)
		{
			fill.push_back(info);
		}
	}
	// Recovering the economy / the anchor never waits for other orders; adding production or tech only does when nothing else is pending (one thing at a time).
	std::vector<BuildListInfo *> &candidates = !recovery.empty() ? recovery : fill;
	if (&candidates == &fill && pendingOrder) { AIDiagnostics::count("x52.fill.blocked_by_pending_order"); return; }
	if (candidates.empty()) { AIDiagnostics::count("x52.fill.no_candidate"); return; }
	BuildListInfo *chosen = candidates[(size_t)pick(m_eb.seed, 0xF111u + frame / (30u * 60u), (int)candidates.size())];
	const ThingTemplate *tmpl = TheThingFactory->findTemplate(chosen->getTemplateName());
	const Int cost = tmpl->calcCostToBuild(m_player);
	if (money < cost + wealthy / 2) return;   // a comfortable treasury for both fill-in and recovery
	Object *dozer = findDozer(chosen->getLocation());
	if (!dozer || TheBuildAssistant->canMakeUnit(dozer, tmpl) != CANMAKE_OK) return;
	Coord3D where;
	if (!enhancedFindPlaceable(chosen->getLocation(), tmpl, chosen->getAngle(), dozer, &where, 8))
	{
		enhancedNotePlacementFailure(chosen->getLocation(), tmpl);   // not worth activating: the construction path would only run its long fallback search
		AIDiagnostics::count("x52.fill.skipped_unplaceable");
		return;
	}
	if (where.x != chosen->getLocation()->x || where.y != chosen->getLocation()->y)
	{
		where.z = chosen->getLocation()->z;
		chosen->setLocation(where);   // the retail construction path then finds the slot legal at once instead of running its fallback search
		AIDiagnostics::count("x52.fill.slot_relocated");
	}
	if (chosen->isBuildable())
	{
		chosen->markPriorityBuild();
		AIDiagnostics::count("x52.fill.slot_activated");
		AIDiagnostics::event(frame, "x52.fill.slot_activated", "player=%d entry=%s money=%d", m_player->getPlayerIndex(), chosen->getTemplateName().str(), (int)money);
	}
	else
	{
		if (!recoveryAllowed(TRUE, FALSE, money, cost, wealthy, m_eb.recoveryGrants, frame, m_eb.lastRecovery)) return;
		chosen->incrementNumRebuilds();
		chosen->markPriorityBuild();
		++m_eb.recoveryGrants;
		AIDiagnostics::count("x52.recovery.rebuild_granted");
		AIDiagnostics::event(frame, "x52.recovery.rebuild_granted", "player=%d entry=%s grants=%d money=%d", m_player->getPlayerIndex(), chosen->getTemplateName().str(), m_eb.recoveryGrants, (int)money);
	}
	m_eb.lastRecovery = frame;
	m_buildDelay = 0;   // look at the build list again right away (the retail loop does the actual construction)
}

// ------------------------------------------------------------------------------------------------
/** Expand the economy to another supply source when the base has fewer supply centers than it should by now (existing buildBySupplies does the placement). */
void AIPlayer::enhancedExpansion(UnsignedInt frame)
{
	if (!isSkirmishAI() || frame < kExpansionStart || AIDiagnostics::featureOff("expand")) return;
	if (m_eb.expansions >= kExpansionCap || m_eb.expansionFailures >= kExpansionFailureCap) return;
	if (frame < m_eb.lastExpansion + kExpansionCooldown) return;
	// supply centers I own and a template to expand with
	Int centers = 0;
	AsciiString centerName;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() != m_player || !obj->isKindOf(KINDOF_STRUCTURE) || obj->isKindOf(KINDOF_REBUILD_HOLE)) continue;
		// a supply center is whatever docks supply trucks (the FS_SUPPLY_CENTER tag does not exist in every data set)
		static const NameKeyType key_centerUpdate = NAMEKEY("SupplyCenterDockUpdate");
		if (!obj->findUpdateModule(key_centerUpdate)) continue;
		++centers;
		if (centerName.isEmpty()) centerName = obj->getTemplate()->getName();
	}
	if (centerName.isEmpty() || centers >= desiredSupplyCenters(frame)) return;
	for (BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext())   // an expansion (or a retail supply center) is already on order
		if (info->getTemplateName() == centerName && !slotHasObject(info) && info->isPriorityBuild() && info->getNumRebuilds() != 0) return;
	const ThingTemplate *tmpl = TheThingFactory->findTemplate(centerName);
	if (!tmpl || m_player->getMoney()->countMoney() < tmpl->calcCostToBuild(m_player) + 300) return;
	m_eb.lastExpansion = frame;
	// Which supply source? The retail chooser (findSupplyCenter: nearest to the base with enough supplies, not already served, not closer to the enemy than to us).
	// Where? The retail rule of buildBySupplies - a few cells from the dock, towards the base - but with the cheap near probe instead of its 400 unit wiggle search.
	Object *warehouse = findSupplyCenter(1500);
	if (!warehouse)
	{
		++m_eb.expansionFailures;
		AIDiagnostics::count("x52.expand.no_source");
		return;
	}
	Coord3D location = *warehouse->getPosition();
	Coord2D offset;
	offset.x = location.x - m_baseCenter.x;
	offset.y = location.y - m_baseCenter.y;
	offset.normalize();
	const Real radius = 3 * PATHFIND_CELL_SIZE_F;
	location.x -= offset.x * radius;
	location.y -= offset.y * radius;
	const Real angle = tmpl->getPlacementViewAngle();
	Coord3D where;
	if (!enhancedFindPlaceable(&location, tmpl, angle, nullptr, &where, 10))
	{
		++m_eb.expansionFailures;
		AIDiagnostics::count("x52.expand.no_site");
		return;
	}
	if (!isLocationSafe(&where, tmpl))
	{
		++m_eb.expansionFailures;
		AIDiagnostics::count("x52.expand.unsafe_site");
		return;
	}
	where.z = 0;   // all build list locations are ground relative
	m_player->addToPriorityBuildList(centerName, &where, angle);
	m_curWarehouseID = warehouse->getID();
	++m_eb.expansions;
	AIDiagnostics::count("x52.expand.ordered");
	AIDiagnostics::event(frame, "x52.expand.ordered", "player=%d template=%s at=%.0f,%.0f source=%.0f,%.0f centers=%d", m_player->getPlayerIndex(), centerName.str(), where.x, where.y,
		warehouse->getPosition()->x, warehouse->getPosition()->y, (int)centers);
}

// ------------------------------------------------------------------------------------------------
Bool AIPlayer::enhancedPlacementRecentlyFailed(const Coord3D *at, const ThingTemplate *tmpl)
{
	if (AIDiagnostics::featureOff("placememo")) return FALSE;
	return placementRecentlyFailed(m_eb, placementKey(tmpl->getName().str(), at->x, at->y), TheGameLogic->getFrame());
}

void AIPlayer::enhancedNotePlacementFailure(const Coord3D *at, const ThingTemplate *tmpl)
{
	notePlacementFailure(m_eb, placementKey(tmpl->getName().str(), at->x, at->y), TheGameLogic->getFrame());
	AIDiagnostics::count("x52.place.failure_remembered");
}

// The retail loop starts only the FIRST priority order of the build list; an order that cannot start (enemy units on the site, no free dozer, unreachable) blocks every
// order behind it for as long as it stays first. Enhanced lets such an order step aside for three minutes once it has been stuck for 45 seconds.
void AIPlayer::enhancedNoteStartFailure(const BuildListInfo *info, const ThingTemplate *tmpl)
{
	const UnsignedInt frame = TheGameLogic->getFrame();
	const uint32_t key = placementKey(tmpl->getName().str(), info->getLocation()->x, info->getLocation()->y);
	if (m_eb.stuckKey != key)
	{
		m_eb.stuckKey = key;
		m_eb.stuckSince = frame ? frame : 1u;
		return;
	}
	if (frame >= m_eb.stuckSince + kStuckOrderFrames)
	{
		notePlacementFailure(m_eb, key, frame);
		m_eb.stuckKey = 0;
		AIDiagnostics::count("x52.order.stuck_stepped_aside");
		AIDiagnostics::event(frame, "x52.order.stuck_stepped_aside", "player=%d entry=%s at=%.0f,%.0f", m_player->getPlayerIndex(), tmpl->getName().str(), info->getLocation()->x, info->getLocation()->y);
	}
}

void AIPlayer::enhancedNoteStartSuccess()
{
	m_eb.stuckKey = 0;
}

// Exact-or-near placement probe: the retail legality test (path, terrain, overlap) at the position and at up to `rings` rings of 8 neighbours 2 cells apart.
// At most 1 + 8 * rings checks (the retail fallback search is hundreds to thousands). Returns the first legal position, nearest first.
Bool AIPlayer::enhancedFindPlaceable(const Coord3D *at, const ThingTemplate *tmpl, Real angle, Object *builder, Coord3D *out, Int rings)
{
	static const Int dx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 }, dy[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
	Bool ok = FALSE;
	for (Int ring = 0; ring <= rings && !ok; ++ring)
	{
		const Real reach = 2 * ring * PATHFIND_CELL_SIZE_F;
		for (Int d = 0; d < (ring == 0 ? 1 : 8) && !ok; ++d)
		{
			Coord3D pos = *at;
			pos.x += dx[d] * reach;
			pos.y += dy[d] * reach;
			Coord3D test = pos;
			test.z += TheTerrainLogic->getGroundHeight(pos.x, pos.y);
			ok = TheBuildAssistant->isLocationLegalToBuild(&test, tmpl, angle, BuildAssistant::CLEAR_PATH | BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP, builder, m_player) == LBC_OK;
			if (ok) *out = pos;
		}
	}
	TheTerrainVisual->removeAllBibs();
	return ok;
}

// ------------------------------------------------------------------------------------------------
void AIPlayer::enhancedXferState(Xfer *xfer)
{
	AIEnhancedBase::xferState(xfer, m_eb);
}

// ================================================================================================
// Base layout variation (Skirmish AI, once, when the base list is placed)
// ================================================================================================
namespace
{
// Terrain legality only: at placement time the destroyed start Command Center is still in the world and would make every neighbouring slot look blocked; object
// overlap is resolved by the normal construction path (it places or relocates each structure when it is actually built).
struct LayoutProbe
{
	Player *player;
	Bool legal(const ThingTemplate *tmpl, Coord3D pos, Real angle, const Region3D &bounds, Int *code = nullptr) const
	{
		if (code) *code = -1;
		if (pos.x < bounds.lo.x || pos.x > bounds.hi.x || pos.y < bounds.lo.y || pos.y > bounds.hi.y) return FALSE;
		pos.z += TheTerrainLogic->getGroundHeight(pos.x, pos.y);
		const LegalBuildCode result = TheBuildAssistant->isLocationLegalToBuild(&pos, tmpl, angle, BuildAssistant::TERRAIN_RESTRICTIONS, nullptr, player);
		TheTerrainVisual->removeAllBibs();   // isLocationLegalToBuild adds bib feedback
		if (code) *code = (Int)result;
		return result == LBC_OK;
	}
};
}

Real AISkirmishPlayer::enhancedLayoutOffset(BuildListInfo *list, const Coord3D &buildPos, const Coord3D &startPos, Real baseAngle)
{
	if (AIDiagnostics::featureOff("layout")) return 0.0f;
	Region3D bounds;
	TheTerrainLogic->getMaximumPathfindExtent(&bounds);
	LayoutProbe probe = { m_player };
	int legal[kLayoutVariants];
	for (int v = 0; v < kLayoutVariants; ++v)
	{
		const Real angle = baseAngle + layoutVariantOffset(v);
		const Real s = sin(angle), c = cos(angle);
		int count = 0;
		for (BuildListInfo *cur = list; cur; cur = cur->getNext())
		{
			const ThingTemplate *tmpl = TheThingFactory->findTemplate(cur->getTemplateName());
			if (!tmpl || tmpl->isKindOf(KINDOF_COMMANDCENTER)) continue;
			Coord3D pos = *cur->getLocation();
			pos.x -= buildPos.x;
			pos.y -= buildPos.y;
			const Real nx = pos.x * c - pos.y * s;
			const Real ny = pos.y * c + pos.x * s;
			pos.x = nx + startPos.x;
			pos.y = ny + startPos.y;
			Int code = 0;
			if (probe.legal(tmpl, pos, cur->getAngle(), bounds, &code)) ++count;
			if (v == 0 && AIDiagnostics::enabled()) AIDiagnostics::event(TheGameLogic->getFrame(), "x52.layout.slot", "player=%d entry=%s code=%d", m_player->getPlayerIndex(), cur->getTemplateName().str(), (int)code);
		}
		legal[v] = count;
	}
	m_eb.layoutVariant = chooseLayoutVariant(m_eb.seed, legal, 1);
	AIDiagnostics::event(TheGameLogic->getFrame(), "x52.layout.variant", "player=%d side=%s variant=%d offset_deg=%d legal=%d,%d,%d,%d,%d", m_player->getPlayerIndex(), m_player->getSide().str(),
		m_eb.layoutVariant, (int)(layoutVariantOffset(m_eb.layoutVariant) * 180.0f / kPi), legal[0], legal[1], legal[2], legal[3], legal[4]);
	return layoutVariantOffset(m_eb.layoutVariant);
}

void AISkirmishPlayer::enhancedJitterSlots(BuildListInfo *list)
{
	if (AIDiagnostics::featureOff("jitter")) return;
	struct Slot { BuildListInfo *info; const ThingTemplate *tmpl; Real radius; Bool movable; };
	std::vector<Slot> slots;
	for (BuildListInfo *cur = list; cur; cur = cur->getNext())
	{
		const ThingTemplate *tmpl = TheThingFactory->findTemplate(cur->getTemplateName());
		if (!tmpl) continue;
		Slot slot = { cur, tmpl, tmpl->getTemplateGeometryInfo().getBoundingCircleRadius(),
			// the Command Center is the anchor and supply centers sit next to their docks: neither moves
			!tmpl->isKindOf(KINDOF_COMMANDCENTER) && !tmpl->isKindOf(KINDOF_FS_SUPPLY_CENTER) && !tmpl->isKindOf(KINDOF_CASH_GENERATOR) };
		slots.push_back(slot);
	}
	Region3D bounds;
	TheTerrainLogic->getMaximumPathfindExtent(&bounds);
	LayoutProbe probe = { m_player };
	int moved = 0;
	for (size_t i = 0; i < slots.size(); ++i)
	{
		if (!slots[i].movable) continue;
		const Coord3D here = *slots[i].info->getLocation();
		Real slack = 1.0e9f;
		for (size_t j = 0; j < slots.size(); ++j)
		{
			if (j == i) continue;
			const Coord3D *other = slots[j].info->getLocation();
			const Real dx = other->x - here.x, dy = other->y - here.y;
			const Real gap = sqrt(dx * dx + dy * dy) - slots[i].radius - slots[j].radius;
			if (gap < slack) slack = gap;
		}
		const Real allowance = jitterAllowance(slack, 2 * PATHFIND_CELL_SIZE_F);
		if (allowance <= 0.0f) continue;
		Real jx, jy;
		jitterOffset(m_eb.seed, (uint32_t)i + 1u, allowance, &jx, &jy);
		Coord3D moved3 = here;
		moved3.x += jx;
		moved3.y += jy;
		const Bool hereLegal = probe.legal(slots[i].tmpl, here, slots[i].info->getAngle(), bounds);
		if (hereLegal && !probe.legal(slots[i].tmpl, moved3, slots[i].info->getAngle(), bounds)) continue;
		slots[i].info->setLocation(moved3);
		++moved;
	}
	AIDiagnostics::event(TheGameLogic->getFrame(), "x52.layout.jitter", "player=%d slots=%d moved=%d", m_player->getPlayerIndex(), (int)slots.size(), moved);
}

// ================================================================================================
// Diagnostics scenario aid (GENERALS_AI_DIAG_ATTACK): a scripted, repeatable harassment from a chosen direction. Applied identically to Original and Enhanced runs.
// ================================================================================================
void AIPlayer::diagnosticAttackAid()
{
	const UnsignedInt frame = TheGameLogic->getFrame();
	AIDiagnostics::State::Attack *attack = AIDiagnostics::dueAttack(frame, m_player->getPlayerIndex());
	if (!attack || !m_baseCenterSet || frame < attack->next) return;
	Object *source = attack->source ? TheGameLogic->findObjectByID((ObjectID)attack->source) : nullptr;
	if (!source)
	{
		if (attack->fired > 0) { attack->done = true; return; }   // the harasser died: the aid is over
		for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		{
			if (obj->getControllingPlayer() == m_player || obj->isKindOf(KINDOF_STRUCTURE) || !obj->getAI()) continue;
			if (m_player->getRelationship(obj->getTeam()) != ENEMIES) continue;
			source = obj;
			break;
		}
		if (!source) { attack->next = frame + 90; return; }
		attack->source = (unsigned)source->getID();
	}
	const float radians = (float)attack->angle * kPi / 180.0f;
	Coord3D at = m_baseCenter;
	at.x += std::cos(radians) * 450.0f;
	at.y += std::sin(radians) * 450.0f;
	at.z = TheTerrainLogic->getGroundHeight(at.x, at.y);
	source->setPosition(&at);
	// nearest structure of the base to the harasser
	Object *target = nullptr;
	float best = 1.0e30f;
	for (BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext())
	{
		Object *obj = info->getObjectID() != INVALID_ID ? TheGameLogic->findObjectByID(info->getObjectID()) : nullptr;
		if (!obj || obj->getControllingPlayer() != m_player) continue;
		const float dx = obj->getPosition()->x - at.x, dy = obj->getPosition()->y - at.y;
		if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; target = obj; }
	}
	if (target)
	{
		DamageInfo damage;
		damage.in.m_sourceID = source->getID();
		damage.in.m_damageType = DAMAGE_SMALL_ARMS;
		damage.in.m_deathType = DEATH_NORMAL;
		damage.in.m_amount = 1.0f;
		target->attemptDamage(&damage);
		++attack->fired;
	}
	attack->next = frame + 15;
	if (attack->fired >= attack->count) attack->done = true;
	AIDiagnostics::count("scenario.attack.hits");
}

// ================================================================================================
// X64.52A diagnostics scenario aid (GENERALS_AI_DIAG_SW): superweapon rule set-ups and probes. Identical in every run of a comparison; inert without the variable.
// ================================================================================================
void AIPlayer::diagnosticSuperweaponAid()
{
	const UnsignedInt frame = TheGameLogic->getFrame();
	const Int playerIndex = m_player->getPlayerIndex();
	{
		static std::set<int> reported;   // the money every player starts with (Starting Cash regression checks): once per player, at its first AI update
		if (reported.insert(playerIndex).second)
			AIDiagnostics::event(frame, "start.money", "player=%d side=%s human=%d money=%d", playerIndex, m_player->getSide().str(), m_player->getPlayerType() == PLAYER_HUMAN ? 1 : 0, (int)m_player->getMoney()->countMoney());
	}
	if (frame % 30 == 0)
		AIDiagnostics::maxCount((std::string("sw.peak_owned.p") + std::to_string(playerIndex)).c_str(), SuperweaponRule::countOwned(m_player));
	AIDiagnostics::State::SwAid *aid = AIDiagnostics::dueSwAid(frame);
	if (!aid)
		return;
	aid->done = TRUE;
	if (aid->verb == "money")   // Starting Cash regression checks: the money of any player (the human slot has no AIPlayer to report it)
	{
		if (Player *p = ThePlayerList ? ThePlayerList->getNthPlayer(aid->player) : nullptr)
			AIDiagnostics::event(frame, "sw.money", "player=%d side=%s human=%d money=%d", aid->player, p->getSide().str(), p->getPlayerType() == PLAYER_HUMAN ? 1 : 0, (int)p->getMoney()->countMoney());
		return;
	}
	Player *who = ThePlayerList ? ThePlayerList->getNthPlayer(aid->player) : nullptr;   // the aid names the player it acts on (the human slot has no AIPlayer)
	if (!who)
	{
		AIDiagnostics::event(frame, "sw.aid_unknown_player", "player=%d", aid->player);
		return;
	}
	const Int aidPlayer = aid->player;
	const ThingTemplate *tmpl = TheThingFactory->findTemplate(aid->templateName.c_str());
	if (!tmpl)
	{
		AIDiagnostics::event(frame, "sw.aid_unknown_template", "player=%d template=%s", aidPlayer, aid->templateName.c_str());
		return;
	}
	// the player's Command Center (position for new things) and first dozer
	Object *center = nullptr, *dozer = nullptr;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() != who)
			continue;
		if (!center && obj->isKindOf(KINDOF_COMMANDCENTER)) center = obj;
		if (!dozer && obj->isKindOf(KINDOF_DOZER) && obj->getAI() && obj->getAI()->getDozerAIInterface()) dozer = obj;
	}
	if (aid->verb == "give")
	{
		if (!center) return;
		Coord3D at = *center->getPosition();
		at.x += 260.0f + 40.0f * (float)SuperweaponRule::countOwned(who);
		at.y += 260.0f;
		Object *made = TheThingFactory->newObject(tmpl, who->getDefaultTeam());
		if (made)
		{
			made->setPosition(&at);
			made->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
		}
		AIDiagnostics::event(frame, "sw.give", "player=%d template=%s owned_after=%u", aidPlayer, aid->templateName.c_str(), SuperweaponRule::countOwned(who));
	}
	else if (aid->verb == "kill")
	{
		Object *target = nullptr;
		for (Object *obj = TheGameLogic->getFirstObject(); obj && !target; obj = obj->getNextObject())
			if (obj->getControllingPlayer() == who && obj->getTemplate()->isEquivalentTo(tmpl) && !obj->isEffectivelyDead())
				target = obj;
		if (target)
			target->kill();   // the normal death path (a GLA structure leaves its rebuild hole)
		AIDiagnostics::event(frame, "sw.kill", "player=%d template=%s killed=%d", aidPlayer, aid->templateName.c_str(), target ? 1 : 0);
	}
	else if (aid->verb == "capture")
	{
		Object *target = nullptr;
		for (Object *obj = TheGameLogic->getFirstObject(); obj && !target; obj = obj->getNextObject())
			if (obj->getControllingPlayer() && obj->getControllingPlayer()->getPlayerIndex() == aid->from && obj->getTemplate()->isEquivalentTo(tmpl) && !obj->isEffectivelyDead())
				target = obj;
		if (target)
			target->setTeam(who->getDefaultTeam());   // Object::setTeam -> onCapture: the central point of every capture
		AIDiagnostics::event(frame, "sw.capture", "player=%d template=%s from=%d captured=%d owned_after=%u", aidPlayer, aid->templateName.c_str(), aid->from, target ? 1 : 0, SuperweaponRule::countOwned(who));
	}
	else if (aid->verb == "probe" || aid->verb == "build")
	{
		if (!dozer)
		{
			AIDiagnostics::event(frame, "sw.probe_no_dozer", "player=%d", aidPlayer);
			return;
		}
		const CanMakeType result = TheBuildAssistant->canMakeUnit(dozer, tmpl);
		AIDiagnostics::event(frame, "sw.probe", "player=%d template=%s owned=%u limit=%u canMakeUnit=%d (%s)", aidPlayer, aid->templateName.c_str(), SuperweaponRule::countOwned(who),
			(unsigned)TheGameLogic->getSuperweaponLimit(), (int)result, result == CANMAKE_OK ? "OK" : result == CANMAKE_MAXED_OUT_FOR_PLAYER ? "MAXED_OUT_FOR_PLAYER" : result == CANMAKE_NO_PREREQ ? "NO_PREREQ" : result == CANMAKE_NO_MONEY ? "NO_MONEY" : "other");
		if (aid->verb == "build")
		{
			// the call a human's construct command ends in (DozerAIUpdate::construct): it re-checks canMakeUnit for a human player and refuses; the site is created only if the rule allows it
			DozerAIInterface *dozerAI = dozer->getAI()->getDozerAIInterface();
			// try a ring of sites around the base (the first legal one gets the construction site); a refusal by the rule fails at the first attempt, and at every other
			const Coord3D base = center ? *center->getPosition() : *dozer->getPosition();
			const Object *site = nullptr;
			for (Int ring = 1; ring <= 6 && !site; ++ring)
				for (Int step = 0; step < 12 && !site; ++step)
				{
					Coord3D at = base;
					at.x += std::cos(step * kPi / 6.0f) * 220.0f * (float)ring;
					at.y += std::sin(step * kPi / 6.0f) * 220.0f * (float)ring;
					at.z = TheTerrainLogic->getGroundHeight(at.x, at.y);
					site = dozerAI->construct(tmpl, &at, tmpl->getPlacementViewAngle(), who, FALSE);
				}
			AIDiagnostics::event(frame, "sw.build", "player=%d template=%s site_created=%d owned_after=%u", aidPlayer, aid->templateName.c_str(), site ? 1 : 0, SuperweaponRule::countOwned(who));
			AIDiagnostics::count(site ? "sw.build.site_created" : "sw.build.refused");
		}
	}
}
