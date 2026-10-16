/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ReplaySimulation.h"

#include "Common/GameEngine.h"
#include "Common/LocalFileSystem.h"
#include "Common/Recorder.h"
#include "Common/WorkerProcess.h"
#include "GameLogic/GameLogic.h"
#include "GameClient/GameClient.h"
#include "Common/PerformanceProfile.h"
#include "GameLogic/AIPathfind.h"
#include "Common/SimulationTickStats.h"
#include "Common/DeterminismFingerprint.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>


Bool ReplaySimulation::s_isRunning = false;
UnsignedInt ReplaySimulation::s_replayIndex = 0;
UnsignedInt ReplaySimulation::s_replayCount = 0;

namespace
{
int countProcessesRunning(const std::vector<WorkerProcess>& processes)
{
	int numProcessesRunning = 0;
	size_t i = 0;
	for (; i < processes.size(); ++i)
	{
		if (processes[i].isRunning())
			++numProcessesRunning;
	}
	return numProcessesRunning;
}
} // namespace

namespace
{
const char *envText(const char *name)
{
	const char *v = getenv(name);
	return (v && *v) ? v : nullptr;
}

// X64.38: opt-in measurement of the headless replay simulation (GENERALS_SIM_BENCH=<output base path>). Nothing here runs unless that
// variable is set, and nothing here changes what the simulation does: each GameLogic::UPDATE() is timed as a whole and attributed to
// the PerformanceProfile phase categories, and the determinism fingerprint (state CRC + object count at fixed logic frames) is taken
// between ticks. Options: GENERALS_SIM_FP_INTERVAL (frames between fingerprint checkpoints, default 30; 0 disables),
// GENERALS_SIM_IGNORE_CRC (keep simulating past a replay CRC mismatch; the mismatch is still reported).
class SimulationBench
{
public:
	// GENERALS_SIM_PATHS=1: per-search trace (PathScope samples of every path kind); =2 additionally samples the neighbour / line phases of
	// the A* iterations. Off by default: it adds clock reads to every search, so timing runs and trace runs are kept apart.
	static unsigned int pathCapacity() { const char *v = envText("GENERALS_SIM_PATHS"); return v ? PerformanceProfile::MaxPaths : 0u; }
	static bool pathPhases() { const char *v = envText("GENERALS_SIM_PATHS"); return v && atoi(v) >= 2; }
	SimulationBench() : m_base(envText("GENERALS_SIM_BENCH")), m_recorder(PerformanceProfile::MaxFrames, pathCapacity(), pathPhases()) {}
	bool enabled() const { return m_base != nullptr; }
	bool ignoreCrc() const { return envText("GENERALS_SIM_IGNORE_CRC") != nullptr; }

	void begin(const char *replay)
	{
		m_replay = replay;
		const char *interval = envText("GENERALS_SIM_FP_INTERVAL");
		m_fpInterval = interval ? static_cast<UnsignedInt>(atoi(interval)) : 30u;
		m_freq = static_cast<double>(PerformanceProfile::frequency());
		m_previous = PerformanceProfile::activeRecorder;
		PerformanceProfile::activeRecorder = &m_recorder;
		m_recorder.start(PerformanceProfile::now());
		m_wallStart = PerformanceProfile::now();
	}

	// One timed, attributed simulation tick followed (outside the measured region) by the fingerprint checkpoint when due.
	void tick()
	{
		const UnsignedInt before = TheGameLogic->getFrame();
		const PerformanceProfile::Tick t0 = PerformanceProfile::now();
		m_recorder.beginFrame(t0, before, LOGICFRAMES_PER_SECOND, LOGICFRAMES_PER_SECOND);
		TheGameLogic->UPDATE();
		const PerformanceProfile::Tick t1 = PerformanceProfile::now();
		const UnsignedInt after = TheGameLogic->getFrame();
		m_recorder.endFrame(t1, after);
		m_pendingMs.push_back(static_cast<double>(t1 - t0) * 1000.0 / m_freq);
		if (m_recorder.frames().size() >= 1024 || m_recorder.paths().size() >= 20000) harvest();
		if (m_fpInterval != 0 && after != before && after % m_fpInterval == 0)
		{
			DeterminismFingerprint::Checkpoint c;
			c.frame = after;
			c.crc = TheGameLogic->getCRC(CRC_RECALC);
			c.objects = TheGameLogic->getObjectCount();
			m_trace.push_back(c);
		}
	}

	void finish(int errors)
	{
		harvest();
		PerformanceProfile::activeRecorder = m_previous;
		report(errors);
	}

private:
	typedef std::array<float, PerformanceProfile::CategoryCount> Row;

	// One compact record per PathScope sample (every path kind). Times in milliseconds.
	struct PathRec
	{
		PerformanceProfile::PathKind kind;
		PerformanceProfile::PathKind request;
		PerformanceProfile::PathOutcome outcome;
		unsigned int frame, object, surfaces;
		int layer, radius, human, crusher;
		bool zoneRejected;
		float inclusiveMs, exclusiveMs;
		float fromX, fromY, toX, toY;
		unsigned long long work[PerformanceProfile::PathWorkCount];
		unsigned long long id, parent;
		unsigned long long iterations, selected, neighborCalls, lineCalls;
		float neighborMs, lineMs;       // sampled (1 in 64 iterations) - scale by iterations / selected for an estimate
		float neighborInsertMs, lineInsertMs;   // part of the sampled phase time spent inside open-list insertion
		unsigned long long neighborInserts, lineInserts;
		int queueBefore, queueAfter;
	};

	void harvest()
	{
		const std::vector<PerformanceProfile::Frame> &frames = m_recorder.frames();
		for (size_t i = 0; i < frames.size() && i < m_pendingMs.size(); ++i)
		{
			const PerformanceProfile::Frame &f = frames[i];
			if (f.logicAfter == f.logicBefore) continue;       // not a completed tick (paused, frozen time, game start)
			if (f.logicBefore == 0) { m_startupMs = m_pendingMs[i]; continue; }   // the first update loads the map: reported, not a tick
			m_tickMs.push_back(m_pendingMs[i]);
			m_tickFrame.push_back(f.logicBefore);
			Row row;
			for (unsigned int c = 0; c < PerformanceProfile::CategoryCount; ++c)
				row[c] = static_cast<float>(static_cast<double>(f.metrics[c].exclusive) * 1000.0 / m_freq);
			m_rows.push_back(row);
			for (unsigned int c = 0; c < PerformanceProfile::CategoryCount; ++c) m_callTotals[c] += f.metrics[c].calls;
			m_calls.push_back(f.counters[static_cast<unsigned int>(PerformanceProfile::Counter::UpdateModules)]);
			m_pathsQueued += f.counters[static_cast<unsigned int>(PerformanceProfile::Counter::QueuedPaths)];
			m_queueCells += f.counters[static_cast<unsigned int>(PerformanceProfile::Counter::QueueCells)];
			m_visited += f.counters[static_cast<unsigned int>(PerformanceProfile::Counter::ObjectsVisited)];
		}
		harvestPaths();
		m_pendingMs.clear();
		m_recorder.start(PerformanceProfile::now());
	}

	static void line(FILE *file, const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		vfprintf(file, format, args);
		va_end(args);
	}

	void report(int errors)
	{
		using namespace SimulationTickStats;
		const Summary s = summarize(m_tickMs);
		const double wallSeconds = static_cast<double>(PerformanceProfile::now() - m_wallStart) / m_freq;
		SYSTEM_INFO info;
		GetSystemInfo(&info);
		const std::string base = m_base;
		FILE *out[2] = {stdout, fopen((base + "_summary.txt").c_str(), "w")};
		for (FILE *f : out)
		{
			if (!f) continue;
			line(f, "X64.38 simulation benchmark (headless replay; serial reference path)\n");
			line(f, "replay: %s\n", m_replay.c_str());
			line(f, "logical processors: %u | ticks measured: %zu | mismatch/errors: %d | harness wall: %.2f s | map-load update (excluded): %.1f ms\n", info.dwNumberOfProcessors, s.count, errors, wallSeconds, m_startupMs);
			line(f, "tick ms: mean %.3f | median %.3f | p90 %.3f | p95 %.3f | p99 %.3f | p99.9 %.3f | worst %.3f | best %.3f\n", s.mean, s.median, s.p90, s.p95, s.p99, s.p999, s.worst, s.best);
			line(f, "ticks above 16.67 ms: %zu | 25 ms: %zu | 33.333 ms: %zu | 50 ms: %zu | 66.67 ms: %zu\n", s.above[0], s.above[1], s.above[2], s.above[3], s.above[4]);
			line(f, "uncapped throughput: %.1f ticks/s (%.2fx real time at 30 TPS)\n", s.uncappedTps, s.uncappedTps / 30.0);
			line(f, "real-time model (DERIVED from the measured tick durations, ticks run back to back as they become due; not a paced run):\n");
			line(f, "  missed deadlines: %zu | catch-up ticks: %zu | longest catch-up run: %zu | max backlog: %.2f ms | sustained TPS: %.2f\n", s.missedDeadlines, s.catchUpTicks, s.longestCatchUpRun, s.maxBacklogMs, s.sustainedTps);
			line(f, "counters: update modules invoked %llu | queued paths %llu | queue cells %llu | disabled-loop object visits %llu\n", m_sumCalls(), m_pathsQueued, m_queueCells, m_visited);
		}
		// phase attribution
		struct Attr { unsigned int category; double total, p50, p95, p99, worst, worstShare; unsigned long long calls; };
		std::vector<Attr> attrs;
		const double p99Threshold = s.p99;
		double worstTotal = 0.0;
		for (size_t i = 0; i < m_tickMs.size(); ++i) if (m_tickMs[i] >= p99Threshold) worstTotal += m_tickMs[i];
		for (unsigned int c = 0; c < PerformanceProfile::CategoryCount; ++c)
		{
			std::vector<double> v;
			v.reserve(m_rows.size());
			double total = 0.0, inWorst = 0.0;
			for (size_t i = 0; i < m_rows.size(); ++i)
			{
				v.push_back(m_rows[i][c]);
				total += m_rows[i][c];
				if (m_tickMs[i] >= p99Threshold) inWorst += m_rows[i][c];
			}
			if (total <= 0.0) continue;
			Attr a;
			a.category = c;
			a.calls = m_callTotals[c];
			a.total = total;
			a.p50 = percentile(v, 50.0);
			a.p95 = percentile(v, 95.0);
			a.p99 = percentile(v, 99.0);
			a.worst = percentile(v, 100.0);
			a.worstShare = worstTotal > 0.0 ? 100.0 * inWorst / worstTotal : 0.0;
			attrs.push_back(a);
		}
		std::sort(attrs.begin(), attrs.end(), [](const Attr &a, const Attr &b) { return a.total > b.total; });
		for (FILE *f : out)
		{
			if (!f) continue;
			line(f, "phase attribution (EXCLUSIVE ms per tick; nested scopes are subtracted from their parent: ZoneCalc is inside path_queue, logic_* phases inside game_logic_update):\n");
			line(f, "  %-28s %10s %7s %8s %8s %8s %8s %12s %10s\n", "phase", "total_ms", "share%", "p50", "p95", "p99", "max", "share_of_p99+%", "calls");
			const double grand = s.total > 0.0 ? s.total : 1.0;
			for (const Attr &a : attrs)
				line(f, "  %-28s %10.1f %7.2f %8.4f %8.4f %8.4f %8.4f %12.2f %10llu\n", PerformanceProfile::name(static_cast<PerformanceProfile::Category>(a.category)), a.total, 100.0 * a.total / grand, a.p50, a.p95, a.p99, a.worst, a.worstShare, a.calls);
		}
		// determinism fingerprint
		for (FILE *f : out)
		{
			if (!f) continue;
			line(f, "determinism fingerprint: %zu checkpoints every %u frames | digest %s\n", m_trace.size(), m_fpInterval, DeterminismFingerprint::formatDigest(DeterminismFingerprint::digest(m_trace)).c_str());
		}
		for (FILE *f : out)
		{
			if (!f) continue;
			const char *zoneMode = envText("GENERALS_ZONE_VERIFY") ? "VERIFY (candidate vs legacy oracle)" : (envText("GENERALS_ZONE_LEGACY") ? "LEGACY (original merge primitives)" : "default (minimum-root union-find)");
			line(f, "zone calculation mode: %s", zoneMode);
			if (envText("GENERALS_ZONE_VERIFY"))
				line(f, " | %u recalculations compared, %u mismatches", PathfindZoneManager::verifyComparisons(), PathfindZoneManager::verifyMismatches());
			line(f, "\n");
		}
		for (FILE *f : out)
		{
			if (!f) continue;
			line(f, "open list mode: %s | inserts answered by the index %u | inserts that walked %u (%llu hops)", Pathfinder::openListModeName(), Pathfinder::openListIndexedInserts(), Pathfinder::openListWalkedInserts(), Pathfinder::openListWalkedHops());
			if (envText("GENERALS_PATH_VERIFY")) line(f, " | %u insertions verified against the original walk", Pathfinder::openListVerifiedInserts());
			line(f, "\n");
			if (envText("GENERALS_PATH_TRACE_HASH")) line(f, "open list evolution digest: %016llX\n", Pathfinder::openListTraceDigest());
			line(f, "line walk mode: %s | memo hits %llu", Pathfinder::lineModeName(), Pathfinder::lineMemoHits());
			if (envText("GENERALS_LINE_VERIFY")) line(f, " | %llu memo hits re-verified with the full checks", Pathfinder::lineMemoVerified());
			line(f, "\n");
			if (envText("GENERALS_LINE_TRACE_HASH")) line(f, "line decision digest: %016llX\n", Pathfinder::lineTraceDigest());
		}
		if (m_recorder.deepPaths()) reportPaths(out, base);
		if (FILE *f = fopen((base + "_fingerprint.txt").c_str(), "w"))
		{
			for (const DeterminismFingerprint::Checkpoint &c : m_trace) line(f, "%s\n", DeterminismFingerprint::format(c).c_str());
			fclose(f);
		}
		if (FILE *f = fopen((base + "_ticks.csv").c_str(), "w"))
		{
			line(f, "tick,frame,ms");
			for (const Attr &a : attrs) line(f, ",%s", PerformanceProfile::name(static_cast<PerformanceProfile::Category>(a.category)));
			line(f, "\n");
			for (size_t i = 0; i < m_tickMs.size(); ++i)
			{
				line(f, "%zu,%u,%.4f", i, m_tickFrame[i], m_tickMs[i]);
				for (const Attr &a : attrs) line(f, ",%.4f", m_rows[i][a.category]);
				line(f, "\n");
			}
			fclose(f);
		}
		fflush(stdout);
		if (out[1]) fclose(out[1]);
	}

	void harvestPaths()
	{
		if (!m_recorder.deepPaths()) return;
		m_droppedPaths += m_recorder.droppedPaths();
		for (const PerformanceProfile::PathSample &ps : m_recorder.paths())
		{
			if (ps.logic == 0) continue;           // the map-load update
			PathRec r;
			r.kind = ps.kind; r.request = ps.request; r.outcome = ps.outcome;
			r.frame = ps.logic; r.object = ps.object; r.surfaces = ps.surfaces;
			r.layer = ps.layer; r.radius = ps.radius; r.human = ps.human; r.crusher = ps.crusher;
			r.zoneRejected = ps.zoneRejected;
			r.inclusiveMs = static_cast<float>(static_cast<double>(ps.inclusive) * 1000.0 / m_freq);
			r.exclusiveMs = static_cast<float>(static_cast<double>(ps.exclusive) * 1000.0 / m_freq);
			r.fromX = ps.fromX; r.fromY = ps.fromY; r.toX = ps.toX; r.toY = ps.toY;
			for (unsigned int w = 0; w < PerformanceProfile::PathWorkCount; ++w) r.work[w] = ps.work[w];
			r.id = ps.id; r.parent = ps.parent;
			r.iterations = ps.phases.iterations; r.selected = ps.phases.selected;
			r.neighborCalls = ps.phases.metrics[static_cast<unsigned int>(PerformanceProfile::PathPhaseKind::Neighbor)].calls;
			r.lineCalls = ps.phases.metrics[static_cast<unsigned int>(PerformanceProfile::PathPhaseKind::Line)].calls;
			r.neighborMs = static_cast<float>(static_cast<double>(ps.phases.metrics[static_cast<unsigned int>(PerformanceProfile::PathPhaseKind::Neighbor)].inclusive) * 1000.0 / m_freq);
			r.lineMs = static_cast<float>(static_cast<double>(ps.phases.metrics[static_cast<unsigned int>(PerformanceProfile::PathPhaseKind::Line)].inclusive) * 1000.0 / m_freq);
			{
				const PerformanceProfile::PathPhaseMetric &nm = ps.phases.metrics[static_cast<unsigned int>(PerformanceProfile::PathPhaseKind::Neighbor)];
				const PerformanceProfile::PathPhaseMetric &lm = ps.phases.metrics[static_cast<unsigned int>(PerformanceProfile::PathPhaseKind::Line)];
				r.neighborInsertMs = static_cast<float>(static_cast<double>(nm.insertion) * 1000.0 / m_freq);
				r.lineInsertMs = static_cast<float>(static_cast<double>(lm.insertion) * 1000.0 / m_freq);
				r.neighborInserts = nm.inserts; r.lineInserts = lm.inserts;
			}
			r.queueBefore = ps.queueBefore; r.queueAfter = ps.queueAfter;
			m_pathRecs.push_back(r);
		}
	}

	static const char *kindName(PerformanceProfile::PathKind k)
	{
		static const char *names[] = {"request", "internal", "ground", "hierarchical", "closest", "attack", "safe", "patch", "moveaway",
			"queue", "dispatch", "reconstruction", "cleanup", "zones", "zoneflags", "obstacle"};
		const unsigned int i = static_cast<unsigned int>(k);
		return i < sizeof(names) / sizeof(*names) ? names[i] : "?";
	}
	static const char *outcomeName(PerformanceProfile::PathOutcome o)
	{
		static const char *names[] = {"observed", "path", "closest", "null_before_work", "null_after_work"};
		const unsigned int i = static_cast<unsigned int>(o);
		return i < sizeof(names) / sizeof(*names) ? names[i] : "?";
	}
	static double workOf(const PathRec &r, PerformanceProfile::PathWork w) { return static_cast<double>(r.work[static_cast<unsigned int>(w)]); }

	// Pearson correlation of two samples (0 when degenerate).
	static double correlation(const std::vector<double> &a, const std::vector<double> &b)
	{
		const size_t n = a.size();
		if (n < 3) return 0.0;
		double ma = 0, mb = 0;
		for (size_t i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
		ma /= static_cast<double>(n); mb /= static_cast<double>(n);
		double sab = 0, saa = 0, sbb = 0;
		for (size_t i = 0; i < n; ++i) { sab += (a[i] - ma) * (b[i] - mb); saa += (a[i] - ma) * (a[i] - ma); sbb += (b[i] - mb) * (b[i] - mb); }
		return (saa > 0 && sbb > 0) ? sab / std::sqrt(saa * sbb) : 0.0;
	}

	void reportPaths(FILE *const *out, const std::string &base)
	{
		using namespace SimulationTickStats;
		using PerformanceProfile::PathKind;
		using PerformanceProfile::PathWork;
		// ---- per kind
		for (FILE *f : {out[0], out[1]})
		{
			if (!f) continue;
			line(f, "path trace: %zu samples (dropped %llu) | all times in ms, INCLUSIVE of nested samples" "%s", m_pathRecs.size(), m_droppedPaths, "\n");
			line(f, "  %-15s %8s %10s %8s %8s %8s %8s %9s %7s %7s %7s %7s %7s %7s\n", "kind", "count", "total_ms", "mean", "p95", "p99", "p99.9", "max", ">5ms", ">10ms", ">25ms", ">50ms", ">100ms", ">200ms");
		}
		for (unsigned int k = 0; k < 16; ++k)
		{
			std::vector<double> v;
			double total = 0;
			for (const PathRec &r : m_pathRecs) if (static_cast<unsigned int>(r.kind) == k) { v.push_back(r.inclusiveMs); total += r.inclusiveMs; }
			if (v.empty()) continue;
			size_t over[6] = {0, 0, 0, 0, 0, 0};
			const double th[6] = {5, 10, 25, 50, 100, 200};
			for (double x : v) for (int t = 0; t < 6; ++t) if (x > th[t]) ++over[t];
			for (FILE *f : {out[0], out[1]})
			{
				if (!f) continue;
				line(f, "  %-15s %8zu %10.1f %8.4f %8.4f %8.4f %8.4f %9.3f %7zu %7zu %7zu %7zu %7zu %7zu\n", kindName(static_cast<PathKind>(k)), v.size(), total, total / static_cast<double>(v.size()),
					percentile(v, 95.0), percentile(v, 99.0), percentile(v, 99.9), percentile(v, 100.0), over[0], over[1], over[2], over[3], over[4], over[5]);
			}
		}
		// ---- the searches proper: internal (A*), ground, hierarchical, closest, attack, safe, patch, moveaway
		std::vector<const PathRec *> searches;
		for (const PathRec &r : m_pathRecs)
			if (r.kind == PathKind::Internal || r.kind == PathKind::Ground || r.kind == PathKind::Hierarchical || r.kind == PathKind::Closest ||
				r.kind == PathKind::Attack || r.kind == PathKind::Safe || r.kind == PathKind::Patch || r.kind == PathKind::MoveAway)
				searches.push_back(&r);
		std::vector<const PathRec *> sorted = searches;
		std::sort(sorted.begin(), sorted.end(), [](const PathRec *a, const PathRec *b) { return a->inclusiveMs > b->inclusiveMs; });
		for (FILE *f : {out[0], out[1]})
		{
			if (!f) continue;
			line(f, "top 25 slowest search samples (internal / ground / hierarchical / closest / attack / safe / patch / moveaway):\n");
			line(f, "  %-12s %7s %-9s %-6s %-12s %7s %7s %8s %9s %9s %9s %9s %7s %7s %7s %9s %10s %s\n", "kind", "ms", "frame", "obj", "outcome", "pops", "opens", "info_new", "info_fail", "fwd_hops", "rev_hops", "cleaned", "layer", "human", "crusher", "line_walks", "callbacks", "from->to (world)");
			size_t shown = 0;
			for (const PathRec *r : sorted)
			{
				if (++shown > 25) break;
				line(f, "  %-12s %7.2f %-9u %-6u %-12s %7.0f %7.0f %8.0f %9.0f %9.0f %9.0f %9.0f %7d %7d %7d %9.0f %10.0f (%.0f,%.0f)->(%.0f,%.0f)\n", kindName(r->kind), r->inclusiveMs, r->frame, r->object, outcomeName(r->outcome),
					workOf(*r, PathWork::HeadPops), workOf(*r, PathWork::OpenInserts), workOf(*r, PathWork::InfoNew), workOf(*r, PathWork::InfoFailed), workOf(*r, PathWork::ForwardHops), workOf(*r, PathWork::ReverseHops), workOf(*r, PathWork::CleanedCells),
					r->layer, r->human, r->crusher, workOf(*r, PathWork::LineCalls), workOf(*r, PathWork::LineCallbacks), r->fromX, r->fromY, r->toX, r->toY);
			}
		}
		// ---- work distributions and correlations for the A* kernel (kind internal)
		std::vector<double> ms, pops, opens, fwd, rev, infoNew, infoFail, cleaned;
		for (const PathRec &r : m_pathRecs)
		{
			if (r.kind != PathKind::Internal) continue;
			ms.push_back(r.inclusiveMs);
			pops.push_back(workOf(r, PathWork::HeadPops)); opens.push_back(workOf(r, PathWork::OpenInserts)); fwd.push_back(workOf(r, PathWork::ForwardHops)); rev.push_back(workOf(r, PathWork::ReverseHops));
			infoNew.push_back(workOf(r, PathWork::InfoNew)); infoFail.push_back(workOf(r, PathWork::InfoFailed)); cleaned.push_back(workOf(r, PathWork::CleanedCells));
		}
		for (FILE *f : {out[0], out[1]})
		{
			if (!f || ms.empty()) continue;
			line(f, "internal A* kernel (%zu searches): work distribution p50 / p95 / p99 / p99.9 / max\n", ms.size());
			const struct { const char *name; const std::vector<double> *v; } rows[] = {{"head pops (cells expanded)", &pops}, {"open-list inserts", &opens}, {"forward insertion hops", &fwd}, {"reverse insertion hops", &rev}, {"cell infos allocated", &infoNew}, {"cell info alloc failures", &infoFail}, {"cells cleaned", &cleaned}};
			for (const auto &row : rows)
				line(f, "  %-28s %10.0f %10.0f %10.0f %10.0f %12.0f\n", row.name, percentile(*row.v, 50.0), percentile(*row.v, 95.0), percentile(*row.v, 99.0), percentile(*row.v, 99.9), percentile(*row.v, 100.0));
			double totalFwd = 0, totalOpens = 0, totalMs = 0;
			for (double x : fwd) totalFwd += x;
			for (double x : opens) totalOpens += x;
			for (double x : ms) totalMs += x;
			line(f, "  mean forward hops per open-list insert: %.1f | total hops %.3g | total ms %.1f\n", totalOpens > 0 ? totalFwd / totalOpens : 0.0, totalFwd, totalMs);
			line(f, "  correlation of search ms with: head pops %.3f | opens %.3f | forward hops %.3f | reverse hops %.3f | info allocated %.3f | cleaned %.3f\n",
				correlation(ms, pops), correlation(ms, opens), correlation(ms, fwd), correlation(ms, rev), correlation(ms, infoNew), correlation(ms, cleaned));
			// share of search time by expansion-count bucket
			const double edges[] = {0, 100, 500, 2000, 5000, 20000, 1e18};
			line(f, "  by cells expanded: bucket | searches | total ms | share of search time | max ms | mean hops/insert\n");
			for (int b = 0; b < 6; ++b)
			{
				size_t n = 0; double sum = 0, mx = 0, hf = 0, ho = 0;
				for (size_t i = 0; i < ms.size(); ++i) if (pops[i] >= edges[b] && pops[i] < edges[b + 1]) { ++n; sum += ms[i]; if (ms[i] > mx) mx = ms[i]; hf += fwd[i]; ho += opens[i]; }
				line(f, "    [%.0f, %.0f) | %zu | %.1f | %.1f%% | %.2f | %.1f\n", edges[b], b == 5 ? -1.0 : edges[b + 1], n, sum, totalMs > 0 ? 100.0 * sum / totalMs : 0.0, mx, ho > 0 ? hf / ho : 0.0);
			}
			// outcome / class split of the internal searches
			size_t byOutcome[5] = {0, 0, 0, 0, 0}; double outcomeMs[5] = {0, 0, 0, 0, 0}; size_t zoneRej = 0, human = 0, computer = 0; double humanMs = 0, computerMs = 0, rejMs = 0;
			for (const PathRec &r : m_pathRecs)
			{
				if (r.kind != PathKind::Internal) continue;
				const unsigned int o = static_cast<unsigned int>(r.outcome);
				if (o < 5) { ++byOutcome[o]; outcomeMs[o] += r.inclusiveMs; }
				if (r.zoneRejected) { ++zoneRej; rejMs += r.inclusiveMs; }
				if (r.human == 1) { ++human; humanMs += r.inclusiveMs; } else { ++computer; computerMs += r.inclusiveMs; }
			}
			line(f, "  outcome split (count / total ms): path %zu / %.1f | closest %zu / %.1f | null before work %zu / %.1f | null after work (exhausted the open list) %zu / %.1f | zone-rejected %zu / %.1f\n",
				byOutcome[1], outcomeMs[1], byOutcome[2], outcomeMs[2], byOutcome[3], outcomeMs[3], byOutcome[4], outcomeMs[4], zoneRej, rejMs);
			line(f, "  requester: human %zu / %.1f ms | computer %zu / %.1f ms\n", human, humanMs, computer, computerMs);
		}
		// ---- the line phase (examineNeighboringCells -> iterateCellsAlongLine -> examineCellsCallback), internal A* searches only
		{
			double totCalls = 0, totCb = 0, totKept = 0, totUpd = 0, totPops = 0, tValid = 0, tMove = 0, tUpd = 0;
			double stops[8] = {0, 0, 0, 0, 0, 0, 0, 0};   // invalid, zone, pinched, extent, movement, fixed, cliff, alloc
			std::vector<double> cbPerCall;
			double bigCalls = 0, bigCb = 0, bigPops = 0, bigKept = 0, bigMs = 0;
			for (const PathRec &r : m_pathRecs)
			{
				if (r.kind != PathKind::Internal) continue;
				const double calls = workOf(r, PathWork::LineCalls), cb = workOf(r, PathWork::LineCallbacks), pops = workOf(r, PathWork::HeadPops);
				totCalls += calls; totCb += cb; totPops += pops;
				totKept += workOf(r, PathWork::LineKept); totUpd += workOf(r, PathWork::LineUpdated);
				stops[0] += workOf(r, PathWork::LineStopInvalid); stops[1] += workOf(r, PathWork::LineStopZone); stops[2] += workOf(r, PathWork::LineStopPinched);
				stops[3] += workOf(r, PathWork::LineStopExtent); stops[4] += workOf(r, PathWork::LineStopMovement); stops[5] += workOf(r, PathWork::LineStopFixed);
				stops[6] += workOf(r, PathWork::LineStopCliff); stops[7] += workOf(r, PathWork::LineStopAlloc);
				tValid += workOf(r, PathWork::LineTicksValid); tMove += workOf(r, PathWork::LineTicksMovement); tUpd += workOf(r, PathWork::LineTicksUpdate);
				if (calls > 0) cbPerCall.push_back(cb / calls);
				if (pops >= 20000) { bigCalls += calls; bigCb += cb; bigPops += pops; bigKept += workOf(r, PathWork::LineKept); bigMs += r.inclusiveMs; }
			}
			const double ms = 1000.0 / m_freq;
			for (FILE *f : {out[0], out[1]})
			{
				if (!f) continue;
				line(f, "line phase (internal A*): %.0f line walks for %.0f expansions | %.0f callbacks = %.2f per walk (p50 %.1f / p95 %.1f / p99 %.1f / max %.0f) | %.2f callbacks per expansion" "%s",
					totCalls, totPops, totCb, totCalls > 0 ? totCb / totCalls : 0.0, percentile(cbPerCall, 50.0), percentile(cbPerCall, 95.0), percentile(cbPerCall, 99.0), percentile(cbPerCall, 100.0), totPops > 0 ? totCb / totPops : 0.0, "\n");
				line(f, "  callback outcomes: kept (already on a list at <= cost, walk continues) %.0f (%.1f%%) | updated / inserted %.0f (%.1f%%) | walks stopped: invalid %.0f, zone %.0f, pinched %.0f, extent %.0f, movement %.0f, fixed %.0f, cliff %.0f, alloc %.0f\n",
					totKept, totCb > 0 ? 100.0 * totKept / totCb : 0.0, totUpd, totCb > 0 ? 100.0 * totUpd / totCb : 0.0, stops[0], stops[1], stops[2], stops[3], stops[4], stops[5], stops[6], stops[7]);
				line(f, "  searches expanding >= 20000 cells: %.0f callbacks for %.0f expansions in %.1f ms (%.2f callbacks / expansion, %.1f%% kept, %.3f us per callback)\n",
					bigCb, bigPops, bigMs, bigPops > 0 ? bigCb / bigPops : 0.0, bigCb > 0 ? 100.0 * bigKept / bigCb : 0.0, bigCb > 0 ? 1000.0 * bigMs / bigCb : 0.0);
				if (tValid + tMove + tUpd > 0)
					line(f, "  line timing (GENERALS_SIM_PATHS=3, clock overhead included): validity checks %.1f ms | checkForMovement %.1f ms | cost / list update %.1f ms\n", tValid * ms, tMove * ms, tUpd * ms);
			}
		}
		// ---- request wrappers: fallback split
		{
			size_t reqs = 0, fallbacks = 0, fbFailed = 0; double reqMs = 0, fbMs = 0, nofbMs = 0;
			for (const PathRec &r : m_pathRecs)
			{
				if (r.kind != PathKind::Request) continue;
				++reqs; reqMs += r.inclusiveMs;
				if (workOf(r, PathWork::HierarchyFallbacks) > 0) { ++fallbacks; fbMs += r.inclusiveMs; if (r.outcome != PerformanceProfile::PathOutcome::ReturnedPath) ++fbFailed; }
				else nofbMs += r.inclusiveMs;
			}
			for (FILE *f : {out[0], out[1]})
			{
				if (!f) continue;
				line(f, "findPath requests: %zu (%.1f ms) | with hierarchical fallback: %zu (%.1f ms, %zu returned no path) | without: %zu (%.1f ms)\n", reqs, reqMs, fallbacks, fbMs, fbFailed, reqs - fallbacks, nofbMs);
			}
		}
		// ---- csv of every search sample >= 1 ms
		if (FILE *f = fopen((base + "_paths.csv").c_str(), "w"))
		{
			line(f, "frame,kind,id,parent,inclusive_ms,exclusive_ms,outcome,object,layer,radius,human,crusher,zone_rejected,pops,opens,info_new,info_fail,fwd_hops,rev_hops,cleaned,block_zone_queries,hier_fallbacks,iterations,sampled_iterations,neighbor_ms_sampled,line_ms_sampled,neighbor_insert_ms_sampled,line_insert_ms_sampled,neighbor_inserts_sampled,line_inserts_sampled,queue_before,queue_after,from_x,from_y,to_x,to_y\n");
			for (const PathRec &r : m_pathRecs)
			{
				if (r.inclusiveMs < 1.0f) continue;
				line(f, "%u,%s,%llu,%llu,%.4f,%.4f,%s,%u,%d,%d,%d,%d,%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%llu,%llu,%.4f,%.4f,%.4f,%.4f,%llu,%llu,%d,%d,%.1f,%.1f,%.1f,%.1f\n", r.frame, kindName(r.kind), r.id, r.parent, r.inclusiveMs, r.exclusiveMs, outcomeName(r.outcome), r.object, r.layer, r.radius, r.human, r.crusher, r.zoneRejected ? 1 : 0,
					workOf(r, PathWork::HeadPops), workOf(r, PathWork::OpenInserts), workOf(r, PathWork::InfoNew), workOf(r, PathWork::InfoFailed), workOf(r, PathWork::ForwardHops), workOf(r, PathWork::ReverseHops), workOf(r, PathWork::CleanedCells), workOf(r, PathWork::BlockZoneQueries), workOf(r, PathWork::HierarchyFallbacks),
					r.iterations, r.selected, r.neighborMs, r.lineMs, r.neighborInsertMs, r.lineInsertMs, r.neighborInserts, r.lineInserts, r.queueBefore, r.queueAfter, r.fromX, r.fromY, r.toX, r.toY);
			}
			fclose(f);
		}
	}

	unsigned long long m_sumCalls() const
	{
		unsigned long long sum = 0;
		for (unsigned long long v : m_calls) sum += v;
		return sum;
	}

	const char *m_base;
	std::string m_replay;
	UnsignedInt m_fpInterval = 30;
	double m_freq = 1.0;
	PerformanceProfile::Recorder m_recorder;
	PerformanceProfile::Recorder *m_previous = nullptr;
	PerformanceProfile::Tick m_wallStart = 0;
	std::vector<double> m_pendingMs, m_tickMs;
	std::vector<UnsignedInt> m_tickFrame;
	std::vector<Row> m_rows;
	std::vector<unsigned long long> m_calls;
	double m_startupMs = 0.0;
	std::array<unsigned long long, PerformanceProfile::CategoryCount> m_callTotals{};
	std::vector<PathRec> m_pathRecs;
	unsigned long long m_droppedPaths = 0;
	unsigned long long m_pathsQueued = 0, m_queueCells = 0, m_visited = 0;
	std::vector<DeterminismFingerprint::Checkpoint> m_trace;
};
} // namespace

int ReplaySimulation::simulateReplaysInThisProcess(const std::vector<AsciiString> &filenames)
{
	int numErrors = 0;

	if (!TheGlobalData->m_headless)
	{
		s_isRunning = true;
		s_replayIndex = 0;
		s_replayCount = static_cast<UnsignedInt>(filenames.size());

		// If we are not in headless mode, we need to run the replay in the engine.
		for (; s_replayIndex < s_replayCount; ++s_replayIndex)
		{
			TheRecorder->playbackFile(filenames[s_replayIndex]);
			TheGameEngine->execute();
			if (TheRecorder->sawCRCMismatch())
				numErrors++;
			if (!s_isRunning)
				break;
			TheGameEngine->setQuitting(FALSE);
		}
		s_isRunning = false;
		s_replayIndex = 0;
		s_replayCount = 0;
		return numErrors != 0 ? 1 : 0;
	}
	// Note that we use printf here because this is run from cmd.
	DWORD totalStartTimeMillis = GetTickCount();
	for (size_t i = 0; i < filenames.size(); i++)
	{
		const AsciiString &filename = filenames[i];
		printf("Simulating Replay \"%s\"\n", filename.str());
		fflush(stdout);
		DWORD startTimeMillis = GetTickCount();
		if (TheRecorder->simulateReplay(filename))
		{
			SimulationBench bench;
			if (bench.enabled()) bench.begin(filename.str());
			bool mismatchSeen = false;
			UnsignedInt totalTimeSec = TheRecorder->getPlaybackFrameCount() / LOGICFRAMES_PER_SECOND;
			while (TheRecorder->isPlaybackInProgress())
			{
				const int progressFrameInterval = 10*60*LOGICFRAMES_PER_SECOND;
				if (TheGameLogic->getFrame() != 0 && TheGameLogic->getFrame() % progressFrameInterval == 0)
				{
					// Print progress report
					UnsignedInt gameTimeSec = TheGameLogic->getFrame() / LOGICFRAMES_PER_SECOND;
					UnsignedInt realTimeSec = (GetTickCount()-startTimeMillis) / 1000;
					printf("Elapsed Time: %02d:%02d Game Time: %02d:%02d/%02d:%02d\n",
							realTimeSec/60, realTimeSec%60, gameTimeSec/60, gameTimeSec%60, totalTimeSec/60, totalTimeSec%60);
					fflush(stdout);
				}
				if (bench.enabled())
					bench.tick();
				else
					TheGameLogic->UPDATE();
				if (TheRecorder->sawCRCMismatch())
				{
					if (!mismatchSeen) numErrors++;
					mismatchSeen = true;
					// X64.38: GENERALS_SIM_IGNORE_CRC keeps a benchmark running past a mismatch (the engine pauses the game on one).
					if (!bench.ignoreCrc()) break;
					if (TheGameLogic->isGamePaused()) TheGameLogic->setGamePaused(FALSE, FALSE, FALSE);
				}
			}
			if (bench.enabled()) bench.finish(numErrors);
			UnsignedInt gameTimeSec = TheGameLogic->getFrame() / LOGICFRAMES_PER_SECOND;
			UnsignedInt realTimeSec = (GetTickCount()-startTimeMillis) / 1000;
			printf("Elapsed Time: %02d:%02d Game Time: %02d:%02d/%02d:%02d\n",
					realTimeSec/60, realTimeSec%60, gameTimeSec/60, gameTimeSec%60, totalTimeSec/60, totalTimeSec%60);
			fflush(stdout);
		}
		else
		{
			printf("Cannot open replay\n");
			numErrors++;
		}
	}
	if (filenames.size() > 1)
	{
		printf("Simulation of all replays completed. Errors occurred: %d\n", numErrors);

		UnsignedInt realTime = (GetTickCount()-totalStartTimeMillis) / 1000;
		printf("Total Time: %d:%02d:%02d\n", realTime/60/60, realTime/60%60, realTime%60);
		fflush(stdout);
	}

	return numErrors != 0 ? 1 : 0;
}

int ReplaySimulation::simulateReplaysInWorkerProcesses(const std::vector<AsciiString> &filenames, int maxProcesses)
{
	DWORD totalStartTimeMillis = GetTickCount();

	WideChar exePath[1024];
	GetModuleFileNameW(nullptr, exePath, ARRAY_SIZE(exePath));

	std::vector<WorkerProcess> processes;
	int filenamePositionStarted = 0;
	int filenamePositionDone = 0;
	int numErrors = 0;

	while (true)
	{
		int i;
		for (i = 0; i < processes.size(); i++)
			processes[i].update();

		// Get result of finished processes and print output in order
		while (!processes.empty())
		{
			if (!processes[0].isDone())
				break;
			AsciiString stdOutput = processes[0].getStdOutput();
			printf("%d/%d %s", filenamePositionDone+1, (int)filenames.size(), stdOutput.str());
			DWORD exitcode = processes[0].getExitCode();
			if (exitcode != 0)
				printf("Error!\n");
			fflush(stdout);
			numErrors += exitcode == 0 ? 0 : 1;
			processes.erase(processes.begin());
			filenamePositionDone++;
		}

		int numProcessesRunning = countProcessesRunning(processes);

		// Add new processes when we are below the limit and there are replays left
		while (numProcessesRunning < maxProcesses && filenamePositionStarted < filenames.size())
		{
			UnicodeString filenameWide;
			filenameWide.translate(filenames[filenamePositionStarted]);
			UnicodeString command;
			command.format(L"\"%s\"%s%s -replay \"%s\"",
				exePath,
				TheGlobalData->m_windowed ? L" -win" : L"",
				TheGlobalData->m_headless ? L" -headless" : L"",
				filenameWide.str());

			processes.push_back(WorkerProcess());
			processes.back().startProcess(command);

			filenamePositionStarted++;
			numProcessesRunning++;
		}

		if (processes.empty())
			break;

		// Don't waste CPU here, our workers need every bit of CPU time they can get
		Sleep(100);
	}

	DEBUG_ASSERTCRASH(filenamePositionStarted == filenames.size(), ("inconsistent file position 1"));
	DEBUG_ASSERTCRASH(filenamePositionDone == filenames.size(), ("inconsistent file position 2"));

	printf("Simulation of all replays completed. Errors occurred: %d\n", numErrors);

	UnsignedInt realTime = (GetTickCount()-totalStartTimeMillis) / 1000;
	printf("Total Wall Time: %d:%02d:%02d\n", realTime/60/60, realTime/60%60, realTime%60);
	fflush(stdout);

	return numErrors != 0 ? 1 : 0;
}

std::vector<AsciiString> ReplaySimulation::resolveFilenameWildcards(const std::vector<AsciiString> &filenames)
{
	// If some filename contains wildcards, search for actual filenames.
	// Note that we cannot do this in parseReplay because we require TheLocalFileSystem initialized.
	std::vector<AsciiString> filenamesResolved;
	for (std::vector<AsciiString>::const_iterator filename = filenames.begin(); filename != filenames.end(); ++filename)
	{
		if (filename->find('*') || filename->find('?'))
		{
			AsciiString dir1 = TheRecorder->getReplayDir();
			AsciiString dir2 = *filename;
			AsciiString wildcard = *filename;
			{
				int len = dir2.getLength();
				while (len)
				{
					char c = dir2.getCharAt(len-1);
					if (c == '/' || c == '\\')
					{
						wildcard.set(wildcard.str()+dir2.getLength());
						break;
					}
					dir2.removeLastChar();
					len--;
				}
			}

			FilenameList files;
			TheLocalFileSystem->getFileListInDirectory(dir2.str(), dir1.str(), wildcard, files, FALSE);
			for (FilenameList::iterator it = files.begin(); it != files.end(); ++it)
			{
				AsciiString file;
				file.set(it->str() + dir1.getLength());
				filenamesResolved.push_back(file);
			}
		}
		else
			filenamesResolved.push_back(*filename);
	}
	return filenamesResolved;
}

int ReplaySimulation::simulateReplays(const std::vector<AsciiString> &filenames, int maxProcesses)
{
	std::vector<AsciiString> filenamesResolved = resolveFilenameWildcards(filenames);
	if (maxProcesses == SIMULATE_REPLAYS_SEQUENTIAL)
		return simulateReplaysInThisProcess(filenamesResolved);
	else
		return simulateReplaysInWorkerProcesses(filenamesResolved, maxProcesses);
}
