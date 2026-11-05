// SPDX-License-Identifier: GPL-3.0-or-later
// Multiplayer smoothness audit: opt-in measurement of what the player actually receives (GENERALS_FRAMEDELIVERY=<output directory>).
//
// Records per render iteration: the limiter-paced interval, completed logic ticks, the presentation clock (valid / alpha / epoch) and, in network games,
// stall state and the scheduler's own view (frame rate, run-ahead, cushion, self-slug and reset counters, the schedule remainder / period the Network
// would offer to the presentation clock). One CSV row per window (default 5 s, GENERALS_FRAMEDELIVERY_WINDOW=<seconds>) with p50 / p95 / p99 / max
// of the render interval, the tick spacing and the tick duration; GENERALS_FRAMEDELIVERY_FRAMES=1 additionally writes every iteration.
// MEASUREMENT ONLY: it never changes a tick, a command, a timer or an interpolation input. Off: every hook is one test of a plain bool.
#pragma once

namespace FrameDeliveryTelemetry
{
extern int g_state;                 // -1 = not read yet, 0 = off, 1 = on
bool initState();
inline bool enabled() { return g_state > 0 ? true : (g_state == 0 ? false : initState()); }

// Called once at the end of FramePacer::update() with the measured interval. Inert when off.
void frame(float intervalSeconds);

// RAII around one TheGameLogic->UPDATE() call. Inert when off.
class Tick
{
public:
	Tick();
	~Tick();
	Tick(const Tick &) = delete;
	Tick &operator=(const Tick &) = delete;
private:
	bool m_active;
	long long m_start;
};
}
