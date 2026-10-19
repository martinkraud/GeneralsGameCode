/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

// X64.49: the arithmetic of user camera scrolling in presentation time (View::applyPendingUserScroll).
// Pure functions: no timer, no engine state, nothing serialized or simulated.
namespace PresentationScroll
{
// The historic base frame rate of the game (GameCommon.h BaseFps): scroll speeds are tuned per 30 Hz frame.
constexpr float BaseFps = 30.0f;
// A stall never turns into a jump: at most this much wall-clock time is integrated in one step (the frame pacer's 5 fps floor).
constexpr float MaxStepSeconds = 0.2f;

// Seconds to integrate for one update. The first call (no previous counter), a counter that did not advance and an invalid frequency give 0 (no jump).
inline float stepSeconds(long long nowTicks, long long lastTicks, long long ticksPerSecond)
{
	if (lastTicks <= 0 || nowTicks <= lastTicks || ticksPerSecond <= 0)
		return 0.0f;
	const float seconds = static_cast<float>(static_cast<double>(nowTicks - lastTicks) / static_cast<double>(ticksPerSecond));
	return seconds > MaxStepSeconds ? MaxStepSeconds : seconds;
}

// Scroll delta for an interval: the velocity is stored per base frame (delta at fpsRatio 1), so it scales with time, never with the frame count.
inline float deltaForInterval(float perBaseFrame, float seconds)
{
	return perBaseFrame * BaseFps * seconds;
}
}
