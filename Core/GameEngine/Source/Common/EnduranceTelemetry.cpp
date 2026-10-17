// SPDX-License-Identifier: GPL-3.0-or-later
// X64.44: see EnduranceTelemetry.h. Measurement only; read-only with respect to the simulation.
#include "PreRTS.h"

#include "Common/EnduranceTelemetry.h"
#include "Common/PerformanceProfile.h"
#include "Common/SimulationTickStats.h"
#include "GameLogic/Pathfinder/PathFailureDiag.h"

#include "Common/GlobalData.h"
#include "Common/FramePacer.h"
#include "Common/KindOf.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "gitinfo.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>
#include <intrin.h>

namespace EnduranceTelemetry
{
int g_state = -1;

namespace
{
using PerformanceProfile::Tick;
constexpr unsigned kIntervalFrames = 300;      // 10 s of game time at Normal speed
constexpr unsigned kMaxPlayers = 12;     // player slots 0..11 (slot 0 neutral, 1 civilian, then the Skirmish players and observers)
constexpr size_t kTickBufferBytes = 64 * 1024;
using PerformanceProfile::Category;
// Simulation-relevant phases (exclusive time per tick). Render / client categories are not recorded: only GameLogic::update runs inside the telemetry scope.
const Category kPhases[] = { Category::Logic, Category::ObjectUpdates, Category::AIGlobal, Category::AIPlayer, Category::AIObject, Category::Movement, Category::PathQueue,
	Category::PathRequest, Category::PathSearch, Category::PathBuild, Category::WeaponFire, Category::WeaponStore, Category::Missile, Category::ZoneCalc, Category::LogicScripts,
	Category::LogicTerrain, Category::LogicCrc, Category::LogicRecorder, Category::LogicCommands, Category::LogicBuildAssist, Category::LogicPartition, Category::LogicDestroy,
	Category::LogicStores, Category::LogicDisabledLoop, Category::ZoneLabel, Category::ZoneRelabel, Category::ZoneBlocks, Category::ZoneHierarchy, Category::ZoneFlatten };
constexpr size_t kPhaseCount = sizeof(kPhases) / sizeof(kPhases[0]);
const double kBuckets[] = { 1.0, 2.0, 5.0, 10.0, 16.67, 25.0, 33.333, 50.0, 66.67, 100.0 };   // upper edges; one more bucket above the last
constexpr size_t kBucketCount = sizeof(kBuckets) / sizeof(kBuckets[0]) + 1;

struct Session
{
	bool active = false;
	HANDLE intervalsFile = INVALID_HANDLE_VALUE, ticksFile = INVALID_HANDLE_VALUE, samplesFile = INVALID_HANDLE_VALUE;
	std::string tickBuffer;
	std::string directory, stem;
	Tick startTick = 0, intervalStartTick = 0;
	unsigned startFrame = 0, lastFrame = 0;
	double freq = 1.0;
	PerformanceProfile::Recorder rec{ 4, 0, false };
	// interval accumulators
	std::vector<float> ms;
	double catMs[kPhaseCount] = { 0 };
	unsigned long long counters[PerformanceProfile::CounterCount] = { 0 };
	unsigned long long zoneCalls = 0;
	unsigned long long buckets[kBucketCount] = { 0 };
	unsigned long long engineFrames = 0, pausedFrames = 0, nonTickUpdates = 0, intervalEngineFrames = 0, intervalPausedFrames = 0;
	double intervalDiagMs = 0, totalDiagMs = 0;
	PathFailureDiag::LiteCounters prevLite;
	std::vector<float> allMs;            // every completed tick of the session (for the summary)
	std::vector<float> allMsFrame;
	unsigned long long rows = 0;
};
Session s_session;
bool s_atexitRegistered = false;

double msFrom(Tick a, Tick b) { return static_cast<double>(b - a) * 1000.0 / s_session.freq; }

void writeAll(HANDLE h, const std::string &text)
{
	if (h == INVALID_HANDLE_VALUE || text.empty()) return;
	DWORD written = 0;
	WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);   // deliberately no FlushFileBuffers (X64.42)
}

std::string hardwareText()
{
	int regs[4] = { 0, 0, 0, 0 };
	char brand[49] = {};
	for (unsigned i = 0; i < 3; ++i) { __cpuid(regs, static_cast<int>(0x80000002u + i)); memcpy(brand + i * 16, regs, 16); }
	SYSTEM_INFO si{};
	GetNativeSystemInfo(&si);
	MEMORYSTATUSEX mem{};
	mem.dwLength = sizeof(mem);
	GlobalMemoryStatusEx(&mem);
	char text[400];
	snprintf(text, sizeof(text), "cpu=%s; logical_cores=%u; ram_mb=%llu", brand, si.dwNumberOfProcessors, static_cast<unsigned long long>(mem.ullTotalPhys >> 20));
	return text;
}

const char *playerTypeName(int t) { return t == PLAYER_HUMAN ? "human" : (t == PLAYER_COMPUTER ? "computer" : "?"); }

void appendf(std::string &out, const char *format, ...)
{
	char line[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(line, sizeof(line), format, args);
	va_end(args);
	out += line;
}

HANDLE openFile(const std::string &path)
{
	return CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

void writeSummary(const char *reason);

void endSession(const char *reason)
{
	Session &s = s_session;
	if (!s.active) return;
	writeAll(s.ticksFile, s.tickBuffer);
	s.tickBuffer.clear();
	std::string rest;
	PathFailureDiag::drainSampleCsv(rest);
	writeAll(s.samplesFile, rest);
	writeSummary(reason);
	if (s.intervalsFile != INVALID_HANDLE_VALUE) CloseHandle(s.intervalsFile);
	if (s.ticksFile != INVALID_HANDLE_VALUE) CloseHandle(s.ticksFile);
	if (s.samplesFile != INVALID_HANDLE_VALUE) CloseHandle(s.samplesFile);
	s.intervalsFile = s.ticksFile = s.samplesFile = INVALID_HANDLE_VALUE;
	s.active = false;
}

void atExitHandler() { endSession("process exit"); }

void startSession()
{
	Session &s = s_session;
	s = Session();
	s.freq = static_cast<double>(PerformanceProfile::frequency());
	const char *dirEnv = getenv("GENERALS_ENDURANCE");
	std::string dir = dirEnv ? dirEnv : ".";
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	const std::time_t t = std::time(nullptr);
	std::tm utc{};
	gmtime_s(&utc, &t);
	char stamp[40];
	std::strftime(stamp, sizeof(stamp), "%Y%m%dT%H%M%SZ", &utc);
	s.directory = dir;
	s.stem = dir + "\\endurance-" + stamp;
	s.intervalsFile = openFile(s.stem + "-intervals.csv");
	s.ticksFile = openFile(s.stem + "-ticks.csv");
	s.samplesFile = openFile(s.stem + "-pathfail.csv");
	if (s.intervalsFile == INVALID_HANDLE_VALUE) { g_state = 0; return; }   // cannot write: stay off, never fail the game
	s.active = true;
	s.startTick = s.intervalStartTick = PerformanceProfile::now();
	s.startFrame = s.lastFrame = TheGameLogic->getFrame();
	PathFailureDiag::enableLite();
	s.prevLite = PathFailureDiag::lite();
	if (!s_atexitRegistered) { std::atexit(atExitHandler); s_atexitRegistered = true; }

	// metadata (key=value text)
	std::string meta;
	appendf(meta, "schema=1\nstarted_utc=%s\n", stamp);
#if RTS_ZEROHOUR
	appendf(meta, "title=Zero Hour\n");
#else
	appendf(meta, "title=Generals\n");
#endif
	appendf(meta, "git=%s\ndirty=%d\nbuild=MSVC %d; %s %s\nhardware=%s\n", GitSHA1, GitUncommittedChanges ? 1 : 0, static_cast<int>(_MSC_FULL_VER), __DATE__, __TIME__, hardwareText().c_str());
	appendf(meta, "map=%s\ngame_mode=%d\n", TheGlobalData ? TheGlobalData->m_mapName.str() : "?", static_cast<int>(TheGameLogic->getGameMode()));
	appendf(meta, "logic_time_scale_fps=%d\nrender_fps_limit=%d\ninterval_frames=%u\n", TheFramePacer ? TheFramePacer->getLogicTimeScaleFps() : -1, TheFramePacer ? TheFramePacer->getFramesPerSecondLimit() : -1, kIntervalFrames);
	appendf(meta, "tick_buckets_ms_upper_edges=");
	for (size_t i = 0; i < kBucketCount - 1; ++i) appendf(meta, "%s%.3f", i ? "," : "", kBuckets[i]);
	appendf(meta, ",inf\n");
	if (ThePlayerList)
	{
		for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
		{
			Player *p = ThePlayerList->getNthPlayer(i);
			if (!p) continue;
			appendf(meta, "player%d=side:%s;type:%s;difficulty:%d\n", i, p->getSide().str(), playerTypeName(static_cast<int>(p->getPlayerType())), static_cast<int>(p->getPlayerDifficulty()));
		}
	}
	appendf(meta, "start_frame=%u\n", s.startFrame);
	const std::string metaPath = s.stem + "-meta.txt";
	HANDLE h = openFile(metaPath);
	writeAll(h, meta);
	if (h != INVALID_HANDLE_VALUE) CloseHandle(h);

	// CSV headers
	std::string header = "session_s,frame,game_s,wall_s,interval_ticks,interval_wall_s,achieved_tps,lag_s,engine_frames,paused_frames,non_tick_updates,"
		"tick_mean_ms,tick_p50_ms,tick_p95_ms,tick_p99_ms,tick_max_ms,gt16_67,gt25,gt33_333,gt50,gt66_67,gt100";
	for (size_t i = 0; i < kBucketCount; ++i) appendf(header, ",bucket%zu", i);
	header += ",objects_total,units_total,structures_total,stationary_nonidle_units,moving_units,idle_units";
	for (unsigned p = 0; p < kMaxPlayers; ++p) appendf(header, ",p%u_units,p%u_structs,p%u_stationary_nonidle", p, p, p);
	for (size_t c = 0; c < kPhaseCount; ++c) appendf(header, ",ms_%s", PerformanceProfile::name(kPhases[c]));
	header += ",update_modules,queued_paths,queue_cells,objects_visited,zone_calc_calls";
	header += ",path_searches,path_ok,path_fail,path_fail_ge5ms,path_fail_ms,path_fail_worst_ms,path_fail_pops,path_analyses,fr_cliff,fr_struct,fr_ally_busy,fr_enemy_fixed,fr_offmap,blocker_units,zone_recalc_observed,map_mutations";
	header += ",analysis_ms_excluded,scan_ms\n";
	writeAll(s.intervalsFile, header);
	writeAll(s.ticksFile, "frame,tick_ms,start_ms\n");
	writeAll(s.samplesFile, std::string(PathFailureDiag::sampleCsvHeader()));
}

double percentile(std::vector<float> &sorted, double p)
{
	if (sorted.empty()) return 0.0;
	const size_t idx = std::min(sorted.size() - 1, static_cast<size_t>(p * static_cast<double>(sorted.size())));
	return sorted[idx];
}

struct WorldScan
{
	unsigned long long objects = 0, units = 0, structures = 0, stationaryNonIdle = 0, moving = 0, idle = 0;
	unsigned long long pUnits[kMaxPlayers] = { 0 }, pStructs[kMaxPlayers] = { 0 }, pStationary[kMaxPlayers] = { 0 };
};

// Read-only walk over the object list.
void scanWorld(WorldScan &w)
{
	for (Object *o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
	{
		++w.objects;
		if (o->isEffectivelyDead()) continue;
		Player *owner = o->getControllingPlayer();
		const int idx = owner ? owner->getPlayerIndex() : -1;
		const bool structure = o->isKindOf(KINDOF_STRUCTURE);
		if (structure)
		{
			++w.structures;
			if (idx >= 0 && idx < static_cast<int>(kMaxPlayers)) ++w.pStructs[idx];
			continue;
		}
		if (!(o->isKindOf(KINDOF_INFANTRY) || o->isKindOf(KINDOF_VEHICLE) || o->isKindOf(KINDOF_AIRCRAFT))) continue;
		++w.units;
		if (idx >= 0 && idx < static_cast<int>(kMaxPlayers)) ++w.pUnits[idx];
		AIUpdateInterface *ai = o->getAIUpdateInterface();
		if (!ai) continue;
		if (ai->isIdle()) { ++w.idle; continue; }
		if (ai->isMoving()) { ++w.moving; continue; }
		++w.stationaryNonIdle;
		if (idx >= 0 && idx < static_cast<int>(kMaxPlayers)) ++w.pStationary[idx];
	}
}

void writeInterval(Tick now, unsigned frameAfter)
{
	Session &s = s_session;
	std::vector<float> sorted = s.ms;
	std::sort(sorted.begin(), sorted.end());
	double sum = 0;
	for (float v : sorted) sum += v;
	const double wallS = msFrom(s.startTick, now) / 1000.0;
	const double intervalWall = msFrom(s.intervalStartTick, now) / 1000.0;
	const double gameS = static_cast<double>(frameAfter - s.startFrame) / 30.0;
	const unsigned ticks = static_cast<unsigned>(sorted.size());
	size_t gt1667 = 0, gt25 = 0, gt33 = 0, gt50 = 0, gt667 = 0, gt100 = 0;
	for (float v : sorted) { gt1667 += v > 16.67f; gt25 += v > 25.0f; gt33 += v > 33.333f; gt50 += v > 50.0f; gt667 += v > 66.67f; gt100 += v > 100.0f; }

	const Tick scanStart = PerformanceProfile::now();
	WorldScan w;
	scanWorld(w);
	const double scanMs = msFrom(scanStart, PerformanceProfile::now());

	const PathFailureDiag::LiteCounters &now_ = PathFailureDiag::lite();
	const PathFailureDiag::LiteCounters &prev = s.prevLite;
	std::string row;
	appendf(row, "%.1f,%u,%.1f,%.1f,%u,%.3f,%.3f,%.3f,%llu,%llu,%llu,", wallS, frameAfter, gameS, wallS, ticks, intervalWall, intervalWall > 0 ? ticks / intervalWall : 0.0, wallS - gameS,
		s.intervalEngineFrames, s.intervalPausedFrames, s.nonTickUpdates);
	appendf(row, "%.4f,%.4f,%.4f,%.4f,%.4f,%zu,%zu,%zu,%zu,%zu,%zu", ticks ? sum / ticks : 0.0, percentile(sorted, 0.5), percentile(sorted, 0.95), percentile(sorted, 0.99), sorted.empty() ? 0.0 : sorted.back(),
		gt1667, gt25, gt33, gt50, gt667, gt100);
	for (size_t i = 0; i < kBucketCount; ++i) appendf(row, ",%llu", s.buckets[i]);
	appendf(row, ",%llu,%llu,%llu,%llu,%llu,%llu", w.objects, w.units, w.structures, w.stationaryNonIdle, w.moving, w.idle);
	for (unsigned p = 0; p < kMaxPlayers; ++p) appendf(row, ",%llu,%llu,%llu", w.pUnits[p], w.pStructs[p], w.pStationary[p]);
	for (size_t c = 0; c < kPhaseCount; ++c) appendf(row, ",%.3f", s.catMs[c]);
	appendf(row, ",%llu,%llu,%llu,%llu,%llu", s.counters[static_cast<unsigned>(PerformanceProfile::Counter::UpdateModules)], s.counters[static_cast<unsigned>(PerformanceProfile::Counter::QueuedPaths)],
		s.counters[static_cast<unsigned>(PerformanceProfile::Counter::QueueCells)], s.counters[static_cast<unsigned>(PerformanceProfile::Counter::ObjectsVisited)], s.zoneCalls);
	appendf(row, ",%llu,%llu,%llu,%llu,%.3f,%.3f,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu", now_.searches - prev.searches, now_.successes - prev.successes, now_.failures - prev.failures,
		now_.expensiveFailures - prev.expensiveFailures, now_.failureMs - prev.failureMs, now_.worstFailureMs, now_.failurePops - prev.failurePops, now_.analyses - prev.analyses,
		now_.frontierCliff - prev.frontierCliff, now_.frontierStructure - prev.frontierStructure, now_.frontierAllyBusy - prev.frontierAllyBusy, now_.frontierEnemyFixed - prev.frontierEnemyFixed,
		now_.frontierOffMap - prev.frontierOffMap, now_.blockerUnits - prev.blockerUnits, now_.zoneRecalcs - prev.zoneRecalcs, now_.mutations - prev.mutations);
	appendf(row, ",%.3f,%.3f\n", now_.analysisMs - prev.analysisMs, scanMs);
	writeAll(s.intervalsFile, row);
	// failure samples (rate-limited analyses) and the buffered tick list go out with the row; still no flush
	std::string samples;
	PathFailureDiag::drainSampleCsv(samples);
	writeAll(s.samplesFile, samples);
	writeAll(s.ticksFile, s.tickBuffer);
	s.tickBuffer.clear();

	++s.rows;
	s.totalDiagMs += scanMs + (now_.analysisMs - prev.analysisMs);
	// reset the interval; PathFailureDiag::worstFailureMs is a running maximum, reported as such
	s.prevLite = now_;
	s.prevLite.worstFailureMs = 0;
	s.ms.clear();
	std::fill(std::begin(s.catMs), std::end(s.catMs), 0.0);
	std::fill(std::begin(s.counters), std::end(s.counters), 0ULL);
	std::fill(std::begin(s.buckets), std::end(s.buckets), 0ULL);
	s.zoneCalls = 0;
	s.intervalEngineFrames = s.intervalPausedFrames = 0;
	s.intervalStartTick = PerformanceProfile::now();
}

void writeSummary(const char *reason)
{
	Session &s = s_session;
	using namespace SimulationTickStats;
	std::vector<double> v(s.allMs.begin(), s.allMs.end());
	const Summary sum = summarize(v);
	std::string text;
	appendf(text, "ENDURANCE SESSION SUMMARY (%s)\nrows=%llu ticks=%zu first_frame=%u last_frame=%u\n", reason, s.rows, s.allMs.size(), s.startFrame, s.lastFrame);
	appendf(text, "tick ms: mean %.4f median %.4f p90 %.4f p95 %.4f p99 %.4f p99.9 %.4f worst %.3f\n", sum.mean, sum.median, sum.p90, sum.p95, sum.p99, sum.p999, sum.worst);
	appendf(text, "ticks above 16.67/25/33.333/50/66.67 ms: %zu / %zu / %zu / %zu / %zu\n", sum.above[0], sum.above[1], sum.above[2], sum.above[3], sum.above[4]);
	appendf(text, "derived (back-to-back model): missed deadlines %zu, catch-up ticks %zu, longest catch-up run %zu, max backlog %.1f ms\n", sum.missedDeadlines, sum.catchUpTicks, sum.longestCatchUpRun, sum.maxBacklogMs);
	appendf(text, "total diagnostic time (world scans + failure analyses), excluded from tick figures: %.1f ms\n", s.totalDiagMs);
	HANDLE h = openFile(s.stem + "-summary.txt");
	writeAll(h, text);
	if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
}   // namespace

bool initState()
{
	g_state = (getenv("GENERALS_ENDURANCE") && *getenv("GENERALS_ENDURANCE")) ? 1 : 0;
	if (g_state > 0) PathFailureDiag::enableLite();
	return g_state > 0;
}

void engineFrame()
{
	if (!enabled() || !s_session.active) return;
	++s_session.engineFrames;
	++s_session.intervalEngineFrames;
	if (TheGameLogic && TheGameLogic->isGamePaused()) { ++s_session.pausedFrames; ++s_session.intervalPausedFrames; }
}

LogicTick::LogicTick() : m_active(false), m_ownsRecorder(false), m_start(0), m_frameBefore(0), m_analysisBefore(0), m_previousRecorder(nullptr)
{
	if (!enabled() || !TheGameLogic) return;
	const GameMode mode = TheGameLogic->getGameMode();
	const bool inGame = mode != GAME_SHELL && mode != GAME_NONE;
	const unsigned frame = TheGameLogic->getFrame();
	if (s_session.active && (!inGame || frame < s_session.lastFrame)) endSession(inGame ? "new game" : "left the game");
	if (!inGame || frame == 0) return;          // the first update loads the map: not a tick
	if (!s_session.active) { startSession(); if (!s_session.active) return; }
	m_active = true;
	m_frameBefore = frame;
	m_analysisBefore = PathFailureDiag::lite().analysisMs;
	m_start = PerformanceProfile::now();
	if (PerformanceProfile::activeRecorder == nullptr)
	{
		m_ownsRecorder = true;
		s_session.rec.start(m_start);
		s_session.rec.beginFrame(m_start, frame, 30, 30);
		PerformanceProfile::activeRecorder = &s_session.rec;
	}
}

LogicTick::~LogicTick()
{
	if (!m_active) return;
	Session &s = s_session;
	const Tick end = PerformanceProfile::now();
	const unsigned frameAfter = TheGameLogic ? TheGameLogic->getFrame() : m_frameBefore;
	if (m_ownsRecorder)
	{
		s.rec.endFrame(end, frameAfter);
		PerformanceProfile::activeRecorder = nullptr;
	}
	if (frameAfter == m_frameBefore) { ++s.nonTickUpdates; return; }     // paused / frozen / waiting: not a completed tick
	// The update that leaves the game (exit to the shell) resets the frame counter: it is the world teardown, not a simulation tick (X64.44 closeout:
	// without this guard it was recorded as the last tick and the summary reported last_frame=1).
	if (frameAfter < m_frameBefore) { ++s.nonTickUpdates; return; }
	const double analysisMs = PathFailureDiag::lite().analysisMs - m_analysisBefore;
	double tickMs = msFrom(m_start, end) - analysisMs;
	if (tickMs < 0) tickMs = 0;
	if (m_ownsRecorder && !s.rec.frames().empty())
	{
		const PerformanceProfile::Frame &f = s.rec.frames().back();
		for (size_t c = 0; c < kPhaseCount; ++c)
		{
			double ms = static_cast<double>(f.metrics[static_cast<unsigned>(kPhases[c])].exclusive) * 1000.0 / s.freq;
			if (kPhases[c] == Category::PathSearch) ms = std::max(0.0, ms - analysisMs);   // the failure analysis runs inside the search scope
			s.catMs[c] += ms;
		}
		for (unsigned c = 0; c < PerformanceProfile::CounterCount; ++c) s.counters[c] += f.counters[c];
		s.zoneCalls += f.metrics[static_cast<unsigned>(Category::ZoneCalc)].calls;
	}
	s.ms.push_back(static_cast<float>(tickMs));
	s.allMs.push_back(static_cast<float>(tickMs));
	size_t b = 0;
	while (b < kBucketCount - 1 && tickMs > kBuckets[b]) ++b;
	++s.buckets[b];
	s.lastFrame = frameAfter;
	char line[96];
	snprintf(line, sizeof(line), "%u,%.3f,%.3f\n", m_frameBefore, tickMs, msFrom(s.startTick, m_start));
	s.tickBuffer += line;
	if (s.tickBuffer.size() >= kTickBufferBytes) { writeAll(s.ticksFile, s.tickBuffer); s.tickBuffer.clear(); }
	if (frameAfter % kIntervalFrames == 0) writeInterval(end, frameAfter);
}
}
