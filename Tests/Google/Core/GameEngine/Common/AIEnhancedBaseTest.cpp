/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// X64.52: the pure rules of the Enhanced AI "Dynamic Base & Defence". They must be deterministic functions of their inputs (a replay or a network peer reproduces
// every Enhanced decision), bounded, and must never need the logic RNG.

#include <gtest/gtest.h>
#include <cmath>
#include <set>
#include "Common/AIEnhancedBase.h"
#include "Common/AIEnhancedBaseXfer.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include <cstdio>

using namespace AIEnhancedBase;

TEST(AIEnhancedBase, HashIsPinnedAndDeterministic)
{
	// pinned: a change of the hash would silently change every Enhanced decision of every replay
	EXPECT_EQ(mix32(0), 0x92ca2f0eu);
	EXPECT_EQ(mix32(1), 0x96a0f96bu);
	EXPECT_EQ(hash2(1, 2), 0x730386cdu);
	EXPECT_EQ(playerSeed(12345, 3), 0xc65717ffu);
	EXPECT_EQ(pick(7, 0x1A70u, 5), 1);
	EXPECT_NE(playerSeed(12345, 3), playerSeed(12345, 4));
	EXPECT_NE(playerSeed(12345, 3), playerSeed(12346, 3));
}

TEST(AIEnhancedBase, PickIsInRangeAndSpread)
{
	int bucket[5] = {};
	for (uint32_t seed = 1; seed <= 2000; ++seed)
	{
		const int v = pick(seed, 0x55u, 5);
		ASSERT_GE(v, 0);
		ASSERT_LT(v, 5);
		++bucket[v];
	}
	for (int b : bucket) EXPECT_GT(b, 280) << "each of five choices should come up about 400 times in 2000";
	EXPECT_EQ(pick(1, 1, 1), 0);
	EXPECT_EQ(pick(1, 1, 0), 0);
	for (uint32_t i = 0; i < 1000; ++i) { const float u = unit(i, 9); ASSERT_GE(u, 0.0f); ASSERT_LT(u, 1.0f); }
}

TEST(AIEnhancedBase, LayoutVariantStaysWithinTolerance)
{
	EXPECT_FLOAT_EQ(layoutVariantOffset(0), 0.0f);
	for (int v = 0; v < kLayoutVariants; ++v) EXPECT_LE(std::fabs(layoutVariantOffset(v)), kPi / 8.0f + 1e-5f);   // never more than 22.5 degrees
	// a clearly worse variant is never chosen
	const int legal[kLayoutVariants] = { 12, 12, 7, 11, 12 };
	for (uint32_t seed = 0; seed < 500; ++seed)
	{
		const int v = chooseLayoutVariant(seed, legal, 1);
		EXPECT_NE(v, 2) << "variant 2 keeps 5 fewer slots legal";
		EXPECT_GE(legal[v], 11);
	}
	// the choice is a pure function of the seed and varies with it
	std::set<int> seen;
	for (uint32_t seed = 0; seed < 200; ++seed) { seen.insert(chooseLayoutVariant(seed, legal, 1)); EXPECT_EQ(chooseLayoutVariant(seed, legal, 1), chooseLayoutVariant(seed, legal, 1)); }
	EXPECT_EQ(seen.size(), 4u);
	// a variant that was not evaluated (-1) is never chosen; nothing evaluated falls back to the retail layout
	const int unevaluated[kLayoutVariants] = { -1, -1, -1, -1, -1 };
	EXPECT_EQ(chooseLayoutVariant(5, unevaluated, 1), 0);
	const int partial[kLayoutVariants] = { 9, -1, -1, 9, -1 };
	for (uint32_t seed = 0; seed < 100; ++seed) { const int v = chooseLayoutVariant(seed, partial, 0); EXPECT_TRUE(v == 0 || v == 3); }
}

TEST(AIEnhancedBase, JitterNeverExceedsTheFreeSpace)
{
	EXPECT_FLOAT_EQ(jitterAllowance(-5.0f, 20.0f), 0.0f);
	EXPECT_FLOAT_EQ(jitterAllowance(0.0f, 20.0f), 0.0f);
	EXPECT_FLOAT_EQ(jitterAllowance(10.0f, 20.0f), 2.5f);   // a quarter of the slack: two neighbours that both move towards each other still keep half of their gap
	EXPECT_FLOAT_EQ(jitterAllowance(500.0f, 20.0f), 20.0f);   // capped
	for (float slack = 1.0f; slack < 300.0f; slack += 7.0f) EXPECT_LE(2.0f * jitterAllowance(slack, 20.0f), slack * 0.5f + 1e-4f);
	for (uint32_t key = 1; key < 300; ++key)
	{
		float dx, dy;
		jitterOffset(777, key, 12.0f, &dx, &dy);
		const float length = std::sqrt(dx * dx + dy * dy);
		EXPECT_LE(length, 12.0f + 1e-3f);
		EXPECT_GE(length, 6.0f - 1e-3f);
	}
	float dx = 1, dy = 1;
	jitterOffset(777, 1, 0.0f, &dx, &dy);
	EXPECT_EQ(dx, 0.0f); EXPECT_EQ(dy, 0.0f);
	float ax, ay, bx, by;
	jitterOffset(777, 5, 12.0f, &ax, &ay);
	jitterOffset(777, 5, 12.0f, &bx, &by);
	EXPECT_EQ(ax, bx); EXPECT_EQ(ay, by);
}

TEST(AIEnhancedBase, ThreatSectorsAndDecay)
{
	EXPECT_EQ(ThreatMap::sectorOf(1, 0), 0);
	EXPECT_EQ(ThreatMap::sectorOf(0, 1), 4);
	EXPECT_EQ(ThreatMap::sectorOf(-1, 0), 8);
	EXPECT_EQ(ThreatMap::sectorOf(0, -1), 12);
	EXPECT_EQ(ThreatMap::sectorOf(1, -0.001f), 15);
	ThreatMap map;
	EXPECT_EQ(map.dominant(1), -1);                         // nothing observed
	for (int i = 0; i < 5; ++i) map.add(4);
	EXPECT_EQ(map.dominant(kThreatMinEvents), -1) << "five observations are below the reaction threshold of six";
	map.add(4);
	EXPECT_EQ(map.dominant(kThreatMinEvents), 4);
	for (int i = 0; i < 10; ++i) map.add(3);              // a neighbouring sector pulls the smoothed maximum towards it but cannot overtake the heavier sector
	EXPECT_EQ(map.dominant(kThreatMinEvents), 3);
	// integer decay: monotone, reaches exactly zero, never negative
	int previous = map.total();
	for (int step = 0; step < 4000 && map.total() > 0; ++step)
	{
		map.decay();
		ASSERT_LE(map.total(), previous);
		ASSERT_GE(map.total(), 0);
		previous = map.total();
	}
	EXPECT_EQ(map.total(), 0);
	// half-life of about three minutes: after 60 s a quarter-ish of the weight is gone, after 10 minutes most of it
	ThreatMap h;
	h.add(0, 40);
	const int start = h.weight[0];
	for (int i = 0; i < 60; ++i) h.decay();
	EXPECT_LT(h.weight[0], start);
	EXPECT_GT(h.weight[0], start * 7 / 10);
	for (int i = 0; i < 540; ++i) h.decay();
	EXPECT_LT(h.weight[0], start / 4);
	// the weight is capped, so one long siege cannot hold the memory for hours
	ThreatMap capped;
	for (int i = 0; i < 1000; ++i) capped.add(2);
	EXPECT_EQ(capped.weight[2], 60 * ThreatMap::kEvent);
}

TEST(AIEnhancedBase, DirectionChangeNeedsMoreThanFortyFiveDegrees)
{
	EXPECT_NEAR(angleDifference(0.1f, 0.1f), 0.0f, 1e-6f);
	EXPECT_NEAR(angleDifference(kPi - 0.1f, -kPi + 0.1f), 0.2f, 1e-5f);   // wraps around
	EXPECT_LT(angleDifference(0.0f, kPi / 8.0f), kArcRecenterRadians);
	EXPECT_GT(angleDifference(0.0f, kPi / 3.0f), kArcRecenterRadians);
}

TEST(AIEnhancedBase, RecoveryIsBoundedAndNeedsAComfortableTreasury)
{
	const int wealthy = 7000;
	EXPECT_TRUE(recoveryAllowed(true, false, 6000, 1000, wealthy, 0, 10000, 0));
	EXPECT_FALSE(recoveryAllowed(false, false, 60000, 1000, wealthy, 0, 10000, 0)) << "only critical structures";
	EXPECT_FALSE(recoveryAllowed(true, true, 60000, 1000, wealthy, 0, 10000, 0)) << "retail allowance still available: nothing to grant";
	EXPECT_FALSE(recoveryAllowed(true, false, 4000, 1000, wealthy, 0, 10000, 0)) << "needs the cost plus half the wealthy reserve";
	EXPECT_FALSE(recoveryAllowed(true, false, 60000, 1000, wealthy, kRecoveryGrantCap, 10000, 0)) << "global cap";
	EXPECT_FALSE(recoveryAllowed(true, false, 60000, 1000, wealthy, 0, 100 + kRecoveryCooldown - 1, 100)) << "cool-down";
	EXPECT_TRUE(recoveryAllowed(true, false, 60000, 1000, wealthy, 0, 100 + kRecoveryCooldown, 100));
}

TEST(AIEnhancedBase, ExpansionTargetsGrowWithTime)
{
	EXPECT_EQ(desiredSupplyCenters(0), 2);
	EXPECT_EQ(desiredSupplyCenters(30 * 60 * 10 - 1), 2);
	EXPECT_EQ(desiredSupplyCenters(30 * 60 * 10), 3);
	EXPECT_GT(kExpansionStart, 30u * 60u * 3u) << "the scripted opening builds the first economy";
	EXPECT_LE(kExpansionCap, 3);
	EXPECT_LE(kThreatDefenseCap, 8);
}

TEST(AIEnhancedBase, StateStartsNeutral)
{
	State s;
	EXPECT_EQ(s.layoutVariant, 0);
	EXPECT_EQ(s.arcSector, -1);
	EXPECT_EQ(s.threat.total(), 0);
	EXPECT_EQ(s.recoveryGrants, 0);
}

TEST(AIEnhancedBase, StateRoundTripsThroughSaveAndLoad)
{
	State saved;
	saved.seed = playerSeed(424242, 5);
	saved.layoutVariant = 3;
	saved.recoveryGrants = 7;
	saved.threatDefenses = 2;
	saved.expansions = 1;
	saved.expansionFailures = 2;
	saved.arcSector = 9;
	saved.lastThreatScan = 12345;
	saved.lastThreatDefense = 11000;
	saved.lastExpansion = 9000;
	saved.lastRecovery = 8000;
	for (int i = 0; i < 40; ++i) saved.threat.add(i % 5 == 0 ? 6 : 7);
	saved.threat.decay();
	const char *file = "x64_52_ai_state_roundtrip.tmp";
	{
		XferSave out;
		out.open(file);
		xferState(&out, saved);
		out.close();
	}
	State loaded;
	{
		XferLoad in;
		in.open(file);
		xferState(&in, loaded);
		in.close();
	}
	std::remove(file);
	EXPECT_EQ(loaded.seed, saved.seed);
	EXPECT_EQ(loaded.layoutVariant, saved.layoutVariant);
	EXPECT_EQ(loaded.recoveryGrants, saved.recoveryGrants);
	EXPECT_EQ(loaded.threatDefenses, saved.threatDefenses);
	EXPECT_EQ(loaded.expansions, saved.expansions);
	EXPECT_EQ(loaded.expansionFailures, saved.expansionFailures);
	EXPECT_EQ(loaded.arcSector, saved.arcSector);
	EXPECT_EQ(loaded.lastThreatScan, saved.lastThreatScan);
	EXPECT_EQ(loaded.lastThreatDefense, saved.lastThreatDefense);
	EXPECT_EQ(loaded.lastExpansion, saved.lastExpansion);
	EXPECT_EQ(loaded.lastRecovery, saved.lastRecovery);
	for (int i = 0; i < ThreatMap::kSectors; ++i) EXPECT_EQ(loaded.threat.weight[i], saved.threat.weight[i]);
	// the decisions after a load equal the decisions without one
	EXPECT_EQ(loaded.threat.dominant(kThreatMinEvents), saved.threat.dominant(kThreatMinEvents));
}

TEST(AIEnhancedBase, PlacementFailuresAreRememberedForThreeMinutes)
{
	State s;
	const uint32_t airfield = placementKey("AmericaAirfield", 1200.0f, 3400.0f);
	const uint32_t power = placementKey("AmericaPowerPlant", 1200.0f, 3400.0f);
	EXPECT_NE(airfield, power);
	EXPECT_NE(airfield, placementKey("AmericaAirfield", 1240.0f, 3400.0f));
	EXPECT_EQ(airfield, placementKey("AmericaAirfield", 1200.0f, 3400.0f));
	EXPECT_EQ(airfield, placementKey("AmericaAirfield", 1200.4f, 3400.4f)) << "the key tolerates sub-cell noise";
	EXPECT_EQ(fnv1a("a"), 0xE40C292Cu);   // pinned: the key must not change between builds (it is saved)
	EXPECT_FALSE(placementRecentlyFailed(s, airfield, 5000));
	notePlacementFailure(s, airfield, 5000);
	EXPECT_TRUE(placementRecentlyFailed(s, airfield, 5000));
	EXPECT_TRUE(placementRecentlyFailed(s, airfield, 5000 + kPlacementFailureWindow - 1));
	EXPECT_FALSE(placementRecentlyFailed(s, airfield, 5000 + kPlacementFailureWindow));
	EXPECT_FALSE(placementRecentlyFailed(s, power, 5001));
	// a second failure of the same slot refreshes it instead of using another entry
	notePlacementFailure(s, airfield, 9000);
	EXPECT_TRUE(placementRecentlyFailed(s, airfield, 9000 + kPlacementFailureWindow - 1));
	// the memory has room for State::kFailSlots slots; the oldest is overwritten first
	State t;
	for (int i = 0; i < State::kFailSlots + 2; ++i) notePlacementFailure(t, placementKey("X", 10.0f * i, 0.0f), 100 + i);
	EXPECT_FALSE(placementRecentlyFailed(t, placementKey("X", 0.0f, 0.0f), 200));
	EXPECT_TRUE(placementRecentlyFailed(t, placementKey("X", 10.0f * (State::kFailSlots + 1), 0.0f), 200));
	// frame 0 is stored as 1 so that an entry never looks empty
	State z;
	notePlacementFailure(z, airfield, 0);
	EXPECT_TRUE(placementRecentlyFailed(z, airfield, 1));
}

TEST(AIEnhancedBase, StuckOrderWindowIsShorterThanTheFailureWindow)
{
	EXPECT_EQ(kStuckOrderFrames, 30u * 45u);
	EXPECT_LT(kStuckOrderFrames, kPlacementFailureWindow);
	EXPECT_LE(State::kFailSlots, 16);
}
