// SPDX-License-Identifier: GPL-3.0-or-later
// Pure decision of the logic frame rate the packet router asks a lock-step match to run at. Pacing only: the rate is applied at the same execution frame on
// every machine and changes when ticks are RELEASED, never what a tick computes.
#pragma once

namespace NetworkPacing
{
// minDisplayFps: slowest player's average DISPLAY frame rate; currentRate: the rate in force; minRate: floor; maxRate: ceiling.
//
// The ceiling must be the authoritative 30 ticks/s (callers pass min(render cap, LOGICFRAMES_PER_SECOND)). The render cap is a PRESENTATION setting (30..240); using it as the
// ceiling let a fast machine be asked for e.g. 60 ticks/s while the slowest player is held at 30 (and ticks cannot be released before that player's commands arrive), which
// shows up as bursts of ticks followed by stalls.
inline int negotiatedLogicRate(int minDisplayFps, int currentRate, int minRate, int maxRate)
{
	// within 10% below the rate in force: keep it (avoid hunting around the target)
	if (minDisplayFps >= ((currentRate * 9) / 10) && minDisplayFps < currentRate)
		minDisplayFps = currentRate;
	if (minDisplayFps > maxRate) minDisplayFps = maxRate;
	if (minDisplayFps < minRate) minDisplayFps = minRate;
	return minDisplayFps;
}
}
