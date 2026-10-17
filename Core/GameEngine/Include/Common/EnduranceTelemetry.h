// SPDX-License-Identifier: GPL-3.0-or-later
// X64.44: opt-in time series of a real, long-running Skirmish (GENERALS_ENDURANCE=<output directory>).
//
// Purpose: find out how the simulation behaves as a natural match grows (objects, AI armies, congestion, path failures) and when - if ever - it stops
// sustaining the Normal 30 logic ticks per second. It is a MEASUREMENT tool: it never changes a tick, a command, a path or an object.
//
// Off by default: every hook is one test of a plain bool. When on, it
//   * times every completed logic tick (outside the measured region it only appends to in-memory buffers),
//   * attributes the tick to the existing PerformanceProfile phase categories (unless another recorder is already active),
//   * every 300 logic frames (10 s of game time at Normal speed) writes one CSV row: tick distribution, achieved tick rate, wall-clock lag, world scale
//     (objects, units / structures per player, stationary non-idle units), phase costs, path-search and failure statistics (PathFailureDiag lite mode),
//   * writes every logic tick (frame, duration, start offset) to a second file so exact percentiles and the real-time backlog can be rebuilt offline.
// Output goes through WriteFile on buffers and is NEVER flushed to disk synchronously (X64.42: FlushFileBuffers on the logic thread caused 10-85 ms stalls);
// the OS keeps the data across a process crash. The time spent in the read-only world scan and in the rate-limited failure analysis is measured and
// subtracted from the tick it fell into.
#pragma once

namespace EnduranceTelemetry
{
extern int g_state;                 // -1 = not read yet, 0 = off, 1 = on
bool initState();
inline bool enabled() { return g_state > 0 ? true : (g_state == 0 ? false : initState()); }

// Call once per GameEngine::update (one render frame): counts render frames and paused frames. Inert when off.
void engineFrame();

// RAII around one TheGameLogic->UPDATE() call. Inert when off.
class LogicTick
{
public:
	LogicTick();
	~LogicTick();
	LogicTick(const LogicTick &) = delete;
	LogicTick &operator=(const LogicTick &) = delete;
private:
	bool m_active;
	bool m_ownsRecorder;
	unsigned long long m_start;
	unsigned m_frameBefore;
	double m_analysisBefore;
	void *m_previousRecorder;
};
}
