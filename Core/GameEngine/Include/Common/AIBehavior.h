// SPDX-License-Identifier: GPL-3.0-or-later
// X64.51: the AI Behaviour rule (Original / Enhanced) and the small pure decision helpers of the Enhanced Phase-1 corrections.
//
// Original  = the retail-compatible AI, unchanged (what the game always did; replays and saves of it stay byte-identical).
// Enhanced  = the same AI pipeline with proven EA defects repaired (docs/modernization/X64_51_ENHANCED_AI_FOUNDATION.md). It is NOT a difficulty and NOT a new AI.
//
// The rule is a GAME RULE, never a local toggle: it lives in GameInfo, travels in the game options string (key "AI", written ONLY when Enhanced so every Original
// options string / replay header is byte-identical to before), is recorded in replay headers, saved with skirmish saves and per AI player, and every AI player of a
// match follows the same value. Everything below is pure (no engine state, no RNG, no time): each function returns the retail answer when `enhanced` is false.
#pragma once

#include <cmath>

enum AIBehaviorMode
{
	AI_BEHAVIOR_ORIGINAL = 0,
	AI_BEHAVIOR_ENHANCED = 1
};

namespace AIBehavior
{
// Options-string key / values ("AI=E2"). Original is never written; "AI=O" is accepted when read.
//
// Enhanced is a behaviour REVISION, written into every Enhanced replay header and options string: the Enhanced AI evolves from milestone to milestone, so a replay or
// a match recorded with another revision must be refused instead of silently playing with different decisions (a retail / Original replay is unaffected).
//   revision 1: X64.51 (written as "E")      revision 2: X64.52 Dynamic Base & Defence (written as "E2")
constexpr int kEnhancedRevision = 2;
inline const char *optionKey() { return "AI"; }
inline const char *optionValue(AIBehaviorMode mode) { return mode == AI_BEHAVIOR_ENHANCED ? "E2" : "O"; }
inline bool parseOptionValue(const char *text, AIBehaviorMode &mode)
{
	if (!text || !text[0]) return false;
	if ((text[0] == 'O' || text[0] == 'o') && !text[1]) { mode = AI_BEHAVIOR_ORIGINAL; return true; }
	if (text[0] == 'E' || text[0] == 'e')
	{
		int revision = 0, digits = 0;
		for (const char *c = text + 1; *c; ++c, ++digits)
		{
			if (*c < '0' || *c > '9' || digits > 3) return false;
			revision = revision * 10 + (*c - '0');
		}
		if (digits == 0) revision = 1;   // the X64.51 spelling: a different revision of the Enhanced AI
		if (revision != kEnhancedRevision) return false;
		mode = AI_BEHAVIOR_ENHANCED;
		return true;
	}
	return false;
}

// AI-01 (processBaseBuilding): the template the dozer-capability check is run against. Retail passes the plan accumulated so far (null for every
// non-priority entry), which makes the "first buildable automatic entry" branch unreachable; Enhanced checks the entry being examined.
template <class T> inline const T *dozerCheckTemplate(bool enhanced, const T *accumulatedPlan, const T *currentEntry)
{
	return enhanced ? currentEntry : accumulatedPlan;
}

// AI-02 (processBaseBuilding): after a destroyed GLA structure's slot is linked to its rebuild hole, retail leaves the loop's building pointer stale
// (null) and goes on to place a second structure; Enhanced re-reads the slot's object (the hole) so the slot counts as built until the hole acts.
inline bool refreshBuildingAfterHoleLink(bool enhanced) { return enhanced; }
inline bool stopHoleScanAtFirstMatch(bool enhanced) { return enhanced; }

// AI-04 (Zero Hour computeSuperweaponTarget fine search): index used for the Y offset of grid cell (x, y). Retail Zero Hour used x for both axes
// (an 11 point diagonal); Generals always used y. Enhanced uses y (the full 11 x 11 region).
inline int fineSearchYIndex(bool enhanced, int x, int y) { return enhanced ? y : x; }

// AI-05 (build-list rally point offset): retail evaluated `gotOffset;` (no effect), so the offset was never applied.
inline bool applyRallyOffset(bool enhanced, bool offsetPresent) { return enhanced && offsetPresent; }

// AI-08 (acquireEnemy retaliation preference): retail tests, for every other AI, whether THAT AI targets me, and then favours the candidate; Enhanced
// favours the candidate only if the CANDIDATE targets me, once.
inline bool retaliationBonusApplies(bool enhanced, bool someOtherAiTargetsMe, bool candidateAiTargetsMe)
{
	return enhanced ? candidateAiTargetsMe : someOtherAiTargetsMe;
}
inline bool retaliationBonusOncePerCandidate(bool enhanced) { return enhanced; }

// AI-11 (buildAIBaseDefenseStructure): the +-60 degree arc limit. Retail bounds only the positive (left) side and gives up without advancing the left/right
// alternation, so once the left arc is full nothing is built again; Enhanced bounds both sides, advances the alternation and gives up only when every arc is full.
inline bool defenseArcExhausted(bool enhanced, float angle, float limit) { return enhanced ? std::fabs(angle) > limit : angle > limit; }
inline bool advanceDefenseCounterWhenExhausted(bool enhanced) { return enhanced; }
inline int defenseArcsPerCall(bool flank) { return flank ? 4 : 2; }   // front: left / right; flank: (flank, backdoor) x (left, right)

// AI-13 (isSupplySourceAttacked): the attack-memory window in logic frames (retail 10 FRAMES; the intent was 10 seconds; upstream's repaired value is gated
// by !RETAIL_COMPATIBLE_CRC at compile time, Enhanced selects it at run time).
inline int supplyAttackScanWindowFrames(bool enhanced, int logicFramesPerSecond) { return enhanced ? 10 * logicFramesPerSecond : 10; }
}
