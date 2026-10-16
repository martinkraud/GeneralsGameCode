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
	SimulationBench() : m_base(envText("GENERALS_SIM_BENCH")) {}
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
		if (m_recorder.frames().size() >= 1024) harvest();
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
