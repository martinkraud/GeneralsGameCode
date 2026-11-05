// SPDX-License-Identifier: GPL-3.0-or-later
// See FrameDeliveryTelemetry.h. Measurement only.
#include "PreRTS.h"

#include "Common/FrameDeliveryTelemetry.h"
#include "Common/FrameDeliveryStats.h"
#include "Common/FramePacer.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/NetworkInterface.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace FrameDeliveryTelemetry
{
int g_state = -1;

namespace
{
struct Session
{
	FILE *windows = nullptr;
	FILE *frames = nullptr;
	double freq = 1.0;
	long long origin = 0;
	double windowSeconds = 5.0;
	FrameDelivery::Window window;
	double windowStartMs = 0.0;
	unsigned windowIndex = 0;
	bool wasInGame = false;
	unsigned ticksThisFrame = 0;
	std::string frameBuffer;
	NetworkTimingDiagnostics netStart;
	bool netStartValid = false;
	unsigned lastEpochSeen = 0;
	double lastNetRemainder = 0.0;
};
Session s;

long long now()
{
	LARGE_INTEGER v;
	QueryPerformanceCounter(&v);
	return v.QuadPart;
}

double ms(long long t) { return (double)(t - s.origin) * 1000.0 / s.freq; }

void closeWindow(bool network, const NetworkTimingDiagnostics &net)
{
	const FrameDelivery::Window &w = s.window;
	if (w.frames() == 0 || !s.windows)
		return;
	const FrameDelivery::Percentiles iv = w.interval(), sp = w.tickSpacing(), td = w.tickDuration();
	fprintf(s.windows, "%u,%.1f,%s,%u,%.2f,%.3f,%.3f,%.3f,%.3f,%u,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%.1f,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
		s.windowIndex, s.windowStartMs / 1000.0, network ? "net" : "off", w.frames(), w.renderFps(), iv.p50, iv.p95, iv.p99, iv.max, w.hitches(),
		w.tickRate(), sp.p50, sp.p95, sp.p99, sp.max, td.p50, td.p95, td.p99,
		w.framesWithTicks(0), w.framesWithTicks(1), w.framesWithTicks(2), w.validPercent(), w.frozenFrames(), w.epochChanges(), w.alphaRegressions(),
		w.stalledFrames(), network ? net.frameRate : 0, network ? net.runAhead : 0, network ? net.cushion : 0,
		network ? net.selfSlugs - s.netStart.selfSlugs : 0, network ? net.schedulerResets - s.netStart.schedulerResets : 0,
		network ? net.released - s.netStart.released : 0, 0u);
	if (s.frames && !s.frameBuffer.empty())
	{
		fwrite(s.frameBuffer.data(), 1, s.frameBuffer.size(), s.frames);
		s.frameBuffer.clear();
	}
	if (s.frames) fflush(s.frames);
	fflush(s.windows);
	++s.windowIndex;
}
}

bool initState()
{
	const char *dir = getenv("GENERALS_FRAMEDELIVERY");
	if (!dir || !*dir)
	{
		g_state = 0;
		return false;
	}
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	const std::string stem = std::string(dir) + "/frame-delivery-" + std::to_string((unsigned)GetCurrentProcessId());
	s.windows = fopen((stem + "-windows.csv").c_str(), "wb");
	if (!s.windows)
	{
		g_state = 0;
		return false;
	}
	fprintf(s.windows, "window,start_s,mode,frames,render_fps,int_p50_ms,int_p95_ms,int_p99_ms,int_max_ms,hitches,tick_rate,spacing_p50_ms,spacing_p95_ms,spacing_p99_ms,spacing_max_ms,"
		"tickdur_p50_ms,tickdur_p95_ms,tickdur_p99_ms,frames_0tick,frames_1tick,frames_2plus,alpha_valid_pct,frozen_frames,epoch_changes,alpha_regressions,stalled_frames,"
		"net_framerate,net_runahead,net_cushion,net_selfslugs,net_resets,net_released,reserved\n");
	const char *perFrame = getenv("GENERALS_FRAMEDELIVERY_FRAMES");
	if (perFrame && *perFrame == '1')
	{
		s.frames = fopen((stem + "-frames.csv").c_str(), "wb");
		if (s.frames)
			fprintf(s.frames, "t_ms,interval_ms,ticks,valid,alpha,epoch,stalled,net_remainder_ms,net_period_ms\n");
	}
	const char *win = getenv("GENERALS_FRAMEDELIVERY_WINDOW");
	if (win && atof(win) > 0.5)
		s.windowSeconds = atof(win);
	LARGE_INTEGER f;
	QueryPerformanceFrequency(&f);
	s.freq = (double)f.QuadPart;
	s.origin = now();
	g_state = 1;
	return true;
}

void frame(float intervalSeconds)
{
	if (!enabled())
		return;
	const bool inGame = TheGameLogic != nullptr && TheGameLogic->isInGame() && !TheGameLogic->isInShellGame() && !TheGameLogic->isGamePaused();
	NetworkTimingDiagnostics net;
	const bool network = TheNetwork != nullptr;
	if (network)
		TheNetwork->getTimingDiagnostics(net);
	if (!inGame)
	{
		if (s.wasInGame)
			closeWindow(network, net);
		s.window.reset();
		s.wasInGame = false;
		s.ticksThisFrame = 0;
		return;
	}
	if (!s.wasInGame)
	{
		s.window.reset();
		s.windowStartMs = ms(now());
		s.netStart = net;
		s.wasInGame = true;
	}
	const PresentationTiming &t = TheFramePacer->getPresentationTiming();
	FrameDelivery::FrameSample f;
	f.intervalMs = intervalSeconds * 1000.0f;
	f.ticks = s.ticksThisFrame;
	f.alphaValid = t.valid;
	f.alpha = t.alpha;
	f.epoch = t.epoch;
	f.stalled = network && net.stalling;
	s.window.addFrame(f);
	if (s.frames)
	{
		char line[192];
		const int n = snprintf(line, sizeof(line), "%.3f,%.3f,%u,%d,%.4f,%u,%d,%.3f,%.3f\n", ms(now()), f.intervalMs, f.ticks, f.alphaValid ? 1 : 0, f.alpha, f.epoch,
			f.stalled ? 1 : 0, net.scheduleValid ? net.scheduleRemainder * 1000.0f : -1.0f, net.scheduleValid ? net.schedulePeriod * 1000.0f : -1.0f);
		if (n > 0)
			s.frameBuffer.append(line, (size_t)n);
	}
	s.ticksThisFrame = 0;
	if (s.window.timeMs() >= s.windowSeconds * 1000.0)
	{
		closeWindow(network, net);
		s.window.reset();
		s.windowStartMs = ms(now());
		s.netStart = net;
	}
}

Tick::Tick() : m_active(enabled()), m_start(0)
{
	if (m_active)
		m_start = now();
}

Tick::~Tick()
{
	if (!m_active)
		return;
	const long long end = now();
	s.window.addTick(ms(m_start), (double)(end - m_start) * 1000.0 / s.freq);
	++s.ticksThisFrame;
}
}
