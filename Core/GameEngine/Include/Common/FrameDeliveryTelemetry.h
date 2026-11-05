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

// Phase marks inside one outer iteration (GameEngine::update / FramePacer::update). Each mark closes the phase that ends there; a hitch row lists the phase
// durations so a long frame is attributed to a phase. Inert when off.
enum class Phase : unsigned { PreDraw, Draw, PostDraw, Wait, Count };   // before the display draw (audio, client update, scripts) / the display draw / messages + network + logic / limiter wait
void mark(Phase endedPhase);
inline void markIf(Phase p) { if (g_state != 0) mark(p); }

// Cumulative renderer counters, supplied by the display host (the backend lives outside GameEngine). Differences between two frames attribute a hitch
// to Present (GPU / VSync), resource creation, uploads or draw submission. Measurement only.
struct BackendCounters
{
	unsigned long long presentTicks = 0, submitTicks = 0, uploadTicks = 0, ticksPerSecond = 1;
	unsigned long long textureCreations = 0, bufferCreations = 0, texturesDestroyed = 0, textureUpdates = 0, draws = 0;
};
typedef void (*BackendSampler)(BackendCounters &out);
void setBackendSampler(BackendSampler sampler);

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
