/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include <gtest/gtest.h>
#include <limits>
#include "Common/GroundTranslation.h"
#include "Common/GroundTranslationPolicy.h"

namespace
{
struct KindView
{
	KindOfMaskType mask;
	bool isKindOf(KindOfType kind) const { return mask.test(kind); }
	bool isAnyKindOf(const KindOfMaskType& kinds) const { return mask.anyIntersectionWith(kinds); }
};
GroundTranslation::Eligibility ordinary()
{
	GroundTranslation::Eligibility c = {true, true, false, true, true, AI_MOVE_TO, 0, true};
	c.addDrawModule(GroundTranslation::supportsRootDraw(true, false));
	return c;
}
class GroundPolicy : public testing::Test
{
	void SetUp() override { GroundTranslation::setEnabled(true); }
	void TearDown() override { GroundTranslation::setEnabled(false); }
};
TEST_F(GroundPolicy, OriginalSixAndRenamedCompatibleUnitsUseCapabilities)
{
	// Names are diagnostic labels only: the production API cannot inspect one.
	const char* labels[] = {"AmericaInfantryRanger", "ChinaInfantryRedguard", "GLAInfantryRebel",
		"AmericaTankCrusader", "ChinaTankBattleMaster", "GLATankScorpion", "Mod_RenamedInfantry", "RenamedCar"};
	for (unsigned int i = 0; i < 8; ++i)
	{
		SCOPED_TRACE(labels[i]);
		KindView view;
		view.mask.set(i < 3 || i == 6 ? KINDOF_INFANTRY : KINDOF_VEHICLE);
		auto c = ordinary();
		c.ordinaryGroundKind = GroundTranslation::supportsKinds(view);
		EXPECT_TRUE(GroundTranslation::canInterpolate(c));
	}
}
TEST_F(GroundPolicy, ForbiddenKindsRejectEvenWithInfantryOrVehicle)
{
	const KindOfType excluded[] = {KINDOF_AIRCRAFT, KINDOF_PROJECTILE, KINDOF_STRUCTURE,
		KINDOF_IMMOBILE, KINDOF_DOZER, KINDOF_HARVESTER, KINDOF_TRANSPORT, KINDOF_BOAT,
		KINDOF_CLIFF_JUMPER, KINDOF_DRONE, KINDOF_MOB_NEXUS, KINDOF_PORTABLE_STRUCTURE,
		KINDOF_SPAWNS_ARE_THE_WEAPONS, KINDOF_INERT};
	for (KindOfType kind : excluded)
	{
		KindView view;
		view.mask.set(KINDOF_VEHICLE); view.mask.set(kind);
		EXPECT_FALSE(GroundTranslation::supportsKinds(view)) << int(kind);
	}
	EXPECT_FALSE(GroundTranslation::supportsKinds(KindView()));
}
TEST_F(GroundPolicy, BlockedOrMissingRuntimeCapabilitiesReject)
{
	EXPECT_TRUE(GroundTranslation::supportsSurfaces(LOCOMOTORSURFACE_GROUND));
	EXPECT_TRUE(GroundTranslation::supportsSurfaces(LOCOMOTORSURFACE_GROUND | LOCOMOTORSURFACE_RUBBLE));
	EXPECT_FALSE(GroundTranslation::supportsSurfaces(0));
	EXPECT_FALSE(GroundTranslation::supportsSurfaces(LOCOMOTORSURFACE_GROUND | LOCOMOTORSURFACE_WATER));
	EXPECT_FALSE(GroundTranslation::supportsSurfaces(LOCOMOTORSURFACE_GROUND | LOCOMOTORSURFACE_CLIFF));
	EXPECT_FALSE(GroundTranslation::supportsSurfaces(LOCOMOTORSURFACE_AIR));
	auto c = ordinary(); c.boundVisibleAlive = false;
	EXPECT_FALSE(GroundTranslation::canInterpolate(c));
	c = ordinary(); c.blocked = true; // Contained/airborne/parachuting/script/deploy notifications.
	EXPECT_FALSE(GroundTranslation::canInterpolate(c));
	c = ordinary(); c.ordinaryAI = false;
	EXPECT_FALSE(GroundTranslation::canInterpolate(c));
	c = ordinary(); c.groundLocomotor = false;
	EXPECT_FALSE(GroundTranslation::canInterpolate(c));
}
TEST_F(GroundPolicy, MissingUnsupportedMixedAndAttachedDrawModulesFailClosed)
{
	auto c = ordinary(); c.drawModuleCount = 0;
	EXPECT_FALSE(GroundTranslation::canInterpolate(c));
	c = ordinary(); c.allDrawModulesSupported = true; c.drawModuleCount = 0;
	c.addDrawModule(GroundTranslation::supportsRootDraw(false, false));
	EXPECT_FALSE(GroundTranslation::canInterpolate(c));
	c = ordinary(); c.addDrawModule(GroundTranslation::supportsRootDraw(false, false));
	EXPECT_FALSE(GroundTranslation::canInterpolate(c));
	c = ordinary(); c.addDrawModule(GroundTranslation::supportsRootDraw(true, true));
	EXPECT_FALSE(GroundTranslation::canInterpolate(c));
}
TEST_F(GroundPolicy, OrdinaryCombatFormationAndYieldStatesQualify)
{
	const AIStateType states[] = {AI_IDLE, AI_MOVE_TO, AI_WAIT, AI_ATTACK_POSITION, AI_ATTACK_OBJECT,
		AI_FORCE_ATTACK_OBJECT, AI_ATTACK_AND_FOLLOW_OBJECT, AI_ATTACK_MOVE_TO, AI_GUARD,

#if RTS_ZEROHOUR
		AI_GUARD_RETALIATE,
#endif
		AI_MOVE_OUT_OF_THE_WAY, AI_MOVE_AND_TIGHTEN};
	for (AIStateType state : states) { auto c = ordinary(); c.movement = state; EXPECT_TRUE(GroundTranslation::canInterpolate(c)); }
}
TEST_F(GroundPolicy, SemanticAndUnauditedMovementStatesReject)
{
	const AIStateType states[] = {AI_DOCK, AI_ENTER, AI_EXIT,
#if RTS_ZEROHOUR
		AI_EXIT_INSTANTLY,
#endif
		AI_RAPPEL_INTO,
		AI_COMBATDROP, AI_GUARD_TUNNEL_NETWORK, AI_GET_REPAIRED, AI_BUSY, AI_MOVE_AND_DELETE,
		AI_MOVE_AND_EVACUATE, AI_FOLLOW_WAYPOINT_PATH_AS_TEAM, AI_FOLLOW_PATH, AI_HUNT, AI_PANIC};
	for (AIStateType state : states) { auto c = ordinary(); c.movement = state; EXPECT_FALSE(GroundTranslation::canInterpolate(c)); }
	EXPECT_FALSE(GroundTranslation::supportsMovement(static_cast<AIStateType>(-1)));
}
TEST_F(GroundPolicy, GateOffOverridesAllCapabilities)
{
	GroundTranslation::setEnabled(false);
	EXPECT_FALSE(GroundTranslation::canInterpolate(ordinary()));
}
TEST_F(GroundPolicy, NanAndReadOnlyHudSelectionPosition)
{
	GroundTranslationHistory history;
	history.capture(Vector3(0, 0, 0), 10, 1);
	history.capture(Vector3(2, 4, 6), 11, 1);
	const Vector3 canonical(2, 4, 6);
	PresentationTiming t = {10, 11, 1, 0.5f, true};
	const Vector3 presented = history.getPresentationPosition(canonical, t, true);
	EXPECT_EQ(presented, Vector3(1, 2, 3));
	EXPECT_EQ(canonical, Vector3(2, 4, 6));
	t.alpha = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(history.getPresentationPosition(canonical, t, true), canonical);
	EXPECT_EQ(history.getPresentationPosition(canonical, t, false), canonical);
}
}
