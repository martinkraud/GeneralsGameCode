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

// The state of one view's held scroll (X64.52B: moved out of View so the real logic is unit tested; View::applyPendingUserScroll is a thin wrapper).
// set(): the look-at translator hands over the offset it computed for fpsRatio (every client update while a key / the screen edge / RMB scroll is held).
// advance(): called once per client update before the camera transform is built; integrates the velocity for the wall-clock time since the previous advance(),
//            consumes it (a velocity moves the camera ONCE: calling advance() again without a new set() moves nothing) and reports the offset.
// clear(): release / forced stop.
struct Integrator
{
	bool has = false;
	float perBaseX = 0.0f, perBaseY = 0.0f;
	long long lastTicks = 0;

	void set(float offsetX, float offsetY, float fpsRatio)
	{
		if (fpsRatio <= 0.0f)
			return;
		perBaseX = offsetX / fpsRatio;
		perBaseY = offsetY / fpsRatio;
		has = true;
	}
	void clear() { has = false; }
	bool advance(long long nowTicks, long long ticksPerSecond, float &outX, float &outY)
	{
		const float seconds = stepSeconds(nowTicks, lastTicks, ticksPerSecond);
		lastTicks = nowTicks;
		outX = outY = 0.0f;
		if (!has)
			return false;
		has = false;
		outX = deltaForInterval(perBaseX, seconds);
		outY = deltaForInterval(perBaseY, seconds);
		return true;
	}
};
}
