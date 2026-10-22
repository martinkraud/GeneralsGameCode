/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// X64.51: the AI Behaviour rule and the pure decision helpers of the Enhanced Phase-1 corrections. Every helper must return the RETAIL answer for Original (false) -
// that is the unit-level half of the "Original is unchanged" proof (the replay-fingerprint gate is the system-level half) - and the repaired answer for Enhanced.

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <set>
#include <utility>
#include <vector>
#include "Common/AIBehavior.h"

namespace
{
struct Template { const char *name; };

constexpr float kPi = 3.14159265358979f;
}

TEST(AIBehavior, OptionValueRoundTripAndStrictParsing)
{
	AIBehaviorMode mode = AI_BEHAVIOR_ORIGINAL;
	EXPECT_STREQ(AIBehavior::optionKey(), "AI");
	EXPECT_EQ(AIBehavior::optionValue(AI_BEHAVIOR_ENHANCED), 'E');
	EXPECT_EQ(AIBehavior::optionValue(AI_BEHAVIOR_ORIGINAL), 'O');
	EXPECT_TRUE(AIBehavior::parseOptionValue("E", mode)); EXPECT_EQ(mode, AI_BEHAVIOR_ENHANCED);
	EXPECT_TRUE(AIBehavior::parseOptionValue("O", mode)); EXPECT_EQ(mode, AI_BEHAVIOR_ORIGINAL);
	EXPECT_TRUE(AIBehavior::parseOptionValue("e", mode)); EXPECT_EQ(mode, AI_BEHAVIOR_ENHANCED);
	// anything else is rejected, never silently read as Original
	for (const char *bad : { "", "X", "EE", "Enhanced", "1", "0" })
	{
		mode = AI_BEHAVIOR_ENHANCED;
		EXPECT_FALSE(AIBehavior::parseOptionValue(bad, mode)) << bad;
		EXPECT_EQ(mode, AI_BEHAVIOR_ENHANCED) << "a rejected value must not modify the result: " << bad;
	}
	EXPECT_FALSE(AIBehavior::parseOptionValue(nullptr, mode));
	EXPECT_EQ(static_cast<int>(AI_BEHAVIOR_ORIGINAL), 0);   // zero-initialised memory is Original
}

TEST(AIBehavior, Ai01DozerCheckTemplate)
{
	const Template accumulated{ "accumulated" }, current{ "current" };
	EXPECT_EQ(AIBehavior::dozerCheckTemplate(false, &accumulated, &current), &accumulated);   // Original: the accumulated plan
	EXPECT_EQ(AIBehavior::dozerCheckTemplate(false, static_cast<const Template *>(nullptr), &current), nullptr);   // ... which is null for non-priority entries: the retail defect
	EXPECT_EQ(AIBehavior::dozerCheckTemplate(true, &accumulated, &current), &current);
	EXPECT_EQ(AIBehavior::dozerCheckTemplate(true, static_cast<const Template *>(nullptr), &current), &current);   // Enhanced: reachable
}

TEST(AIBehavior, Ai01SelectionModel)
{
	// Model of the selection loop of processBaseBuilding: entries in list order, `canMake(plan)` fails for a null plan (BuildAssistant::canMakeUnit returns
	// CANMAKE_NO_PREREQ for null). A non-priority automatic entry is selected only if the dozer check passes.
	struct Entry { const Template *plan; bool priority; bool automatic; bool buildable; };
	const Template power{ "power" }, barracks{ "barracks" };
	const auto select = [&](bool enhanced, const std::vector<Entry> &entries) -> const Template *
	{
		const Template *accumulated = nullptr;
		for (const Entry &e : entries)
		{
			if (e.priority && !accumulated) accumulated = e.plan;
			if (!e.automatic) continue;
			const Template *check = AIBehavior::dozerCheckTemplate(enhanced, accumulated, e.plan);
			if (!check) continue;                       // canMakeUnit(dozer, nullptr) != CANMAKE_OK
			if (e.buildable && !accumulated) accumulated = e.plan;
		}
		return accumulated;
	};
	const std::vector<Entry> automaticOnly = { { &power, false, true, true }, { &barracks, false, true, true } };
	EXPECT_EQ(select(false, automaticOnly), nullptr);          // Original: nothing is ever selected (dead branch)
	EXPECT_EQ(select(true, automaticOnly), &power);            // Enhanced: the first buildable automatic entry
	const std::vector<Entry> withPriority = { { &barracks, true, true, true }, { &power, false, true, true } };
	EXPECT_EQ(select(false, withPriority), &barracks);         // a priority plan still wins in both modes
	EXPECT_EQ(select(true, withPriority), &barracks);
	const std::vector<Entry> scriptGated = { { &power, false, false, true } };
	EXPECT_EQ(select(true, scriptGated), nullptr);             // AutomaticallyBuild = No stays script-gated in Enhanced
}

TEST(AIBehavior, Ai02HoleBookkeeping)
{
	EXPECT_FALSE(AIBehavior::refreshBuildingAfterHoleLink(false));
	EXPECT_FALSE(AIBehavior::stopHoleScanAtFirstMatch(false));
	EXPECT_TRUE(AIBehavior::refreshBuildingAfterHoleLink(true));
	EXPECT_TRUE(AIBehavior::stopHoleScanAtFirstMatch(true));
}

TEST(AIBehavior, Ai04FineSearchCoversTheIntendedRegion)
{
	// The 11 x 11 refinement visits (x, y) with offset index (x - 5) on X and fineSearchYIndex(x, y) - 5 on Y.
	const auto visit = [](bool enhanced)
	{
		std::set<std::pair<int, int>> cells;
		for (int x = 0; x < 11; ++x)
			for (int y = 0; y < 11; ++y)
				cells.insert({ x - 5, AIBehavior::fineSearchYIndex(enhanced, x, y) - 5 });
		return cells;
	};
	const auto original = visit(false);
	const auto enhanced = visit(true);
	EXPECT_EQ(original.size(), 11u);                           // retail Zero Hour: the diagonal only (the typo)
	for (const auto &c : original) EXPECT_EQ(c.first, c.second);
	EXPECT_EQ(enhanced.size(), 121u);                          // intended: the whole 2D region
	EXPECT_TRUE(enhanced.count({ -5, 5 }) && enhanced.count({ 5, -5 }) && enhanced.count({ 0, 0 }));
	for (const auto &c : original) EXPECT_TRUE(enhanced.count(c)) << "the repaired region contains everything the retail loop examined";
	// Generals never had the typo: its loop uses the y index for y, which is the Enhanced rule, and Generals' Original code does not call the helper.
}

TEST(AIBehavior, Ai05RallyOffset)
{
	EXPECT_FALSE(AIBehavior::applyRallyOffset(false, true));
	EXPECT_FALSE(AIBehavior::applyRallyOffset(true, false));
	EXPECT_TRUE(AIBehavior::applyRallyOffset(true, true));
}

TEST(AIBehavior, Ai08RetaliationPredicate)
{
	// Original: any OTHER AI that targets me favours the candidate (the defect); Enhanced: only the candidate targeting me does, once.
	EXPECT_TRUE(AIBehavior::retaliationBonusApplies(false, true, false));
	EXPECT_FALSE(AIBehavior::retaliationBonusApplies(false, false, true));
	EXPECT_FALSE(AIBehavior::retaliationBonusApplies(true, true, false));
	EXPECT_TRUE(AIBehavior::retaliationBonusApplies(true, false, true));
	EXPECT_FALSE(AIBehavior::retaliationBonusOncePerCandidate(false));
	EXPECT_TRUE(AIBehavior::retaliationBonusOncePerCandidate(true));
}

TEST(AIBehavior, Ai11DefenseArcModel)
{
	const float limit = kPi / 3.0f;
	EXPECT_TRUE(AIBehavior::defenseArcExhausted(false, limit + 0.01f, limit));
	EXPECT_FALSE(AIBehavior::defenseArcExhausted(false, -3.0f, limit));       // retail: the negative side is never bounded
	EXPECT_TRUE(AIBehavior::defenseArcExhausted(true, -limit - 0.01f, limit));
	EXPECT_FALSE(AIBehavior::defenseArcExhausted(true, limit - 0.01f, limit));
	EXPECT_FALSE(AIBehavior::advanceDefenseCounterWhenExhausted(false));
	EXPECT_TRUE(AIBehavior::advanceDefenseCounterWhenExhausted(true));
	EXPECT_EQ(AIBehavior::defenseArcsPerCall(false), 2);
	EXPECT_EQ(AIBehavior::defenseArcsPerCall(true), 4);

	// Model of the FRONT arc bookkeeping of buildAIBaseDefenseStructure (spacing `step`; `buildable(angle)` stands for the terrain / overlap legality test).
	struct Outcome { int placed; float widest; bool terminated; };
	const auto placeFront = [&](bool enhanced, int calls, float step, bool (*buildable)(float))
	{
		float left = 0.0f, right = 0.0f; int counter = 0; Outcome out{ 0, 0.0f, true };
		for (int call = 0; call < calls; ++call)
		{
			int exhausted = 0, guard = 0;
			for (;;)
			{
				if (++guard > 1000) { out.terminated = false; return out; }
				float angle;
				if (counter & 1) { right -= step; angle = right; } else { angle = left; left += step; }
				if (AIBehavior::defenseArcExhausted(enhanced, angle, limit))
				{
					if (!AIBehavior::advanceDefenseCounterWhenExhausted(enhanced)) break;     // retail: give up, the alternation stays where it is
					++counter;
					if (++exhausted >= AIBehavior::defenseArcsPerCall(false)) break;
					continue;
				}
				exhausted = 0;
				++counter;
				if (!buildable(angle)) continue;                                              // retail loop: try the next candidate
				++out.placed; out.widest = std::max(out.widest, std::fabs(angle));
				break;
			}
		}
		return out;
	};
	const float step = 0.2f;                         // about six slots per side inside +-60 degrees
	const auto everythingBuildable = [](float) { return true; };
	const Outcome retailOpen = placeFront(false, 200, step, everythingBuildable);
	const Outcome repairedOpen = placeFront(true, 200, step, everythingBuildable);
	EXPECT_TRUE(retailOpen.terminated);
	EXPECT_TRUE(repairedOpen.terminated);
	// the retail right side is unbounded: defences are placed OUTSIDE the intended +-60 degree sector; the repaired rule never does
	EXPECT_GT(retailOpen.widest, limit);
	EXPECT_LE(repairedOpen.widest, limit);
	// the retail stall: once the positive (left) arc has been passed the retail loop gives up WITHOUT advancing the alternation, and every later call gives up
	// again. With the left positions blocked that happens after a few calls: more calls place nothing (the retail total is frozen) and the placements that
	// did happen include some outside the +-60 degree sector (the unbounded right side). The repaired rule stops growing too (both arcs are bounded and
	// used up) but never leaves the sector.
	const auto leftBlocked = [](float angle) { return angle <= 0.0f; };
	const Outcome retailBlocked = placeFront(false, 200, step, leftBlocked);
	const Outcome repairedBlocked = placeFront(true, 200, step, leftBlocked);
	EXPECT_TRUE(retailBlocked.terminated);
	EXPECT_TRUE(repairedBlocked.terminated);
	EXPECT_EQ(placeFront(false, 20, step, leftBlocked).placed, retailBlocked.placed);          // retail: frozen
	EXPECT_EQ(placeFront(true, 40, step, leftBlocked).placed, repairedBlocked.placed);         // repaired: bounded by the arcs
	EXPECT_GT(retailBlocked.widest, limit);
	EXPECT_LE(repairedBlocked.widest, limit);
	EXPECT_GE(repairedBlocked.placed, 3);                                                       // the free arc is really used
}

TEST(AIBehavior, Ai13SupplyAttackWindow)
{
	EXPECT_EQ(AIBehavior::supplyAttackScanWindowFrames(false, 30), 10);        // retail: 10 FRAMES
	EXPECT_EQ(AIBehavior::supplyAttackScanWindowFrames(true, 30), 300);        // intended: 10 SECONDS
}
