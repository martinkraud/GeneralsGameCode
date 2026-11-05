// SPDX-License-Identifier: GPL-3.0-or-later
// See FrameDeliveryTelemetry.h. Measurement only.
#include "PreRTS.h"

#include "Common/FrameDeliveryTelemetry.h"
#include "Common/FrameDeliveryStats.h"
#include "Common/FramePacer.h"
#include "Common/PerformanceProfile.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/NetworkInterface.h"

#include <psapi.h>
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
	// hitch recorder: frames at or above hitchMs get a row with the phase durations and the deltas of the renderer / process counters
	FILE *hitches = nullptr;
	double hitchMs = 40.0;
	long long lastMark = 0;
	double phaseMs[(unsigned)FrameDeliveryTelemetry::Phase::Count] = {0};
	FrameDeliveryTelemetry::BackendSampler sampler = nullptr;
	FrameDeliveryTelemetry::BackendCounters prevBackend;
	unsigned long long prevFaults = 0, prevThreadCpu100ns = 0;
	unsigned hitchRows = 0;
	double tickMs = 0.0;
	// GENERALS_FRAMEDELIVERY_PROFILE=1: a PerformanceProfile recorder spans every in-game outer iteration so a hitch row can list the exclusive time per category
	bool profile = false, recorderActive = false;
	PerformanceProfile::Recorder rec{4, 0, false};
	FILE *hitchCategories = nullptr;
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
	PROCESS_MEMORY_COUNTERS_EX pmc = {};
	pmc.cb = sizeof(pmc);
	GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof(pmc));
	DWORD handleCount = 0;
	GetProcessHandleCount(GetCurrentProcess(), &handleCount);
	const double procWs = (double)pmc.WorkingSetSize / 1048576.0, procPrivate = (double)pmc.PrivateUsage / 1048576.0;
	const unsigned long procHandles = (unsigned long)handleCount;
	fprintf(s.windows, "%u,%.1f,%s,%u,%.2f,%.3f,%.3f,%.3f,%.3f,%u,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%.1f,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%.1f,%.1f,%lu\n",
		s.windowIndex, s.windowStartMs / 1000.0, network ? "net" : "off", w.frames(), w.renderFps(), iv.p50, iv.p95, iv.p99, iv.max, w.hitches(),
		w.tickRate(), sp.p50, sp.p95, sp.p99, sp.max, td.p50, td.p95, td.p99,
		w.framesWithTicks(0), w.framesWithTicks(1), w.framesWithTicks(2), w.validPercent(), w.frozenFrames(), w.epochChanges(), w.alphaRegressions(),
		w.stalledFrames(), network ? net.frameRate : 0, network ? net.runAhead : 0, network ? net.cushion : 0,
		network ? net.selfSlugs - s.netStart.selfSlugs : 0, network ? net.schedulerResets - s.netStart.schedulerResets : 0,
		network ? net.released - s.netStart.released : 0, 0u, procWs, procPrivate, procHandles);
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
	s.hitches = fopen((stem + "-hitches.csv").c_str(), "wb");
	if (s.hitches)
		fprintf(s.hitches, "t_ms,interval_ms,predraw_ms,draw_ms,postdraw_ms,wait_ms,tick_ms,thread_cpu_ms,present_ms,submit_ms,upload_ms,"
			"tex_created,buf_created,tex_destroyed,tex_updates,draws,page_faults,working_set_mb,private_mb,handles,ticks\n");
	const char *profileEnv = getenv("GENERALS_FRAMEDELIVERY_PROFILE");
	if (profileEnv && *profileEnv == '1')
	{
		s.profile = true;
		s.hitchCategories = fopen((stem + "-hitch-categories.csv").c_str(), "wb");
		if (s.hitchCategories)
			fprintf(s.hitchCategories, "t_ms,interval_ms,category,exclusive_ms,inclusive_ms,calls\n");
	}
	const char *hitchEnv = getenv("GENERALS_FRAMEDELIVERY_HITCH_MS");
	if (hitchEnv && atof(hitchEnv) >= 5.0)
		s.hitchMs = atof(hitchEnv);
	const char *win = getenv("GENERALS_FRAMEDELIVERY_WINDOW");
	if (win && atof(win) > 0.5)
		s.windowSeconds = atof(win);
	LARGE_INTEGER f;
	QueryPerformanceFrequency(&f);
	s.freq = (double)f.QuadPart;
	s.origin = now();
	s.lastMark = s.origin;
	g_state = 1;
	return true;
}

void setBackendSampler(BackendSampler sampler) { s.sampler = sampler; }

void mark(Phase endedPhase)
{
	if (!enabled())
		return;
	const long long t = now();
	s.phaseMs[(unsigned)endedPhase] += (double)(t - s.lastMark) * 1000.0 / s.freq;
	s.lastMark = t;
}

namespace
{
unsigned long long threadCpu100ns()
{
	FILETIME create, exitTime, kernel, user;
	if (!GetThreadTimes(GetCurrentThread(), &create, &exitTime, &kernel, &user))
		return 0;
	return ((unsigned long long)kernel.dwHighDateTime << 32 | kernel.dwLowDateTime) + ((unsigned long long)user.dwHighDateTime << 32 | user.dwLowDateTime);
}

void recordHitch(float intervalMs, unsigned ticks)
{
	BackendCounters b;
	if (s.sampler)
		s.sampler(b);
	PROCESS_MEMORY_COUNTERS_EX pmc = {};
	pmc.cb = sizeof(pmc);
	GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof(pmc));
	DWORD handles = 0;
	GetProcessHandleCount(GetCurrentProcess(), &handles);
	const unsigned long long cpu = threadCpu100ns();
	const double tps = (double)b.ticksPerSecond / 1000.0;   // ticks per ms
	if (s.hitches)
	{
		fprintf(s.hitches, "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.3f,%.3f,%.3f,%llu,%llu,%llu,%llu,%llu,%llu,%.1f,%.1f,%lu,%u\n",
			ms(now()), intervalMs, s.phaseMs[0], s.phaseMs[1], s.phaseMs[2], s.phaseMs[3], s.tickMs,
			(double)(cpu - s.prevThreadCpu100ns) / 10000.0,
			(double)(b.presentTicks - s.prevBackend.presentTicks) / tps, (double)(b.submitTicks - s.prevBackend.submitTicks) / tps, (double)(b.uploadTicks - s.prevBackend.uploadTicks) / tps,
			b.textureCreations - s.prevBackend.textureCreations, b.bufferCreations - s.prevBackend.bufferCreations, b.texturesDestroyed - s.prevBackend.texturesDestroyed,
			b.textureUpdates - s.prevBackend.textureUpdates, b.draws - s.prevBackend.draws,
			(unsigned long long)pmc.PageFaultCount - s.prevFaults, (double)pmc.WorkingSetSize / 1048576.0, (double)pmc.PrivateUsage / 1048576.0, (unsigned long)handles, ticks);
		fflush(s.hitches);
		++s.hitchRows;
	}
}

void snapshotCounters()
{
	if (s.sampler)
		s.sampler(s.prevBackend);
	PROCESS_MEMORY_COUNTERS pmc = {};
	pmc.cb = sizeof(pmc);
	GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
	s.prevFaults = pmc.PageFaultCount;
	s.prevThreadCpu100ns = threadCpu100ns();
}
}

namespace
{
void stopRecorder()
{
	if (s.recorderActive)
	{
		s.rec.endFrame(PerformanceProfile::now(), TheGameLogic ? TheGameLogic->getFrame() : 0);
		PerformanceProfile::activeRecorder = nullptr;
		s.recorderActive = false;
	}
}

void startRecorder()
{
	if (s.profile && PerformanceProfile::activeRecorder == nullptr)
	{
		const PerformanceProfile::Tick t = PerformanceProfile::now();
		s.rec.start(t);
		s.rec.beginFrame(t, TheGameLogic ? TheGameLogic->getFrame() : 0, 30, 30);
		PerformanceProfile::activeRecorder = &s.rec;
		s.recorderActive = true;
	}
}

void recordCategories(float intervalMs)
{
	if (!s.hitchCategories || s.rec.frames().empty())
		return;
	const PerformanceProfile::Frame &f = s.rec.frames().back();
	const double perMs = 1000.0 / (double)PerformanceProfile::frequency();
	for (unsigned c = 0; c < PerformanceProfile::CategoryCount; ++c)
	{
		const PerformanceProfile::Metric &m = f.metrics[c];
		if (m.calls == 0 || (double)m.exclusive * perMs < 0.5)
			continue;
		fprintf(s.hitchCategories, "%.3f,%.3f,%s,%.3f,%.3f,%llu\n", ms(now()), intervalMs, PerformanceProfile::name((PerformanceProfile::Category)c),
			(double)m.exclusive * perMs, (double)m.inclusive * perMs, (unsigned long long)m.calls);
	}
	fflush(s.hitchCategories);
}
}

void frame(float intervalSeconds)
{
	if (!enabled())
		return;
	mark(Phase::Wait);
	stopRecorder();
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
		for (double &v : s.phaseMs) v = 0.0;
		s.tickMs = 0.0;
		snapshotCounters();
		s.lastMark = now();
		s.ticksThisFrame = 0;
		startRecorder();
		return;
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
	if (f.intervalMs >= s.hitchMs)
	{
		recordHitch(f.intervalMs, f.ticks);
		recordCategories(f.intervalMs);
	}
	snapshotCounters();
	for (double &v : s.phaseMs) v = 0.0;
	s.tickMs = 0.0;
	s.lastMark = now();
	s.ticksThisFrame = 0;
	startRecorder();
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
	s.tickMs += (double)(end - m_start) * 1000.0 / s.freq;
	++s.ticksThisFrame;
}
}
