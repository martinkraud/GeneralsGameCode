// SPDX-License-Identifier: GPL-3.0-or-later
// X64.49: opt-in measurement of camera scrolling cadence (GENERALS_CAMERA_PROBE=<existing output directory>).
//
// A MEASUREMENT aid for the "camera judder while holding the arrow keys" investigation. Off by default: every hook is one test of a plain bool.
// When on it
//   * applies the render-rate cap GENERALS_CAMERA_PROBE_FPS (e.g. 30/60/120/144/240) through the normal frame pacer,
//   * drives the REAL scroll state of the look-at translator (the same state the arrow keys / the screen edge set) in phases:
//     key hold (GENERALS_CAMERA_PROBE_HOLD seconds, default 3) - rest - screen-edge hold - rest, then asks the engine to quit,
//   * records one row per scroll step (S: wall clock, frame-pacer update time, offset applied) and one per drawn frame (D: wall clock, logic frame,
//     the view position and the position of the render camera that is used for the D3D11 matrices),
//   * writes camera-probe.csv and camera-probe-summary.txt (frame-time mean / p95 / p99 / worst / variance, camera step distribution,
//     distance covered, logic ticks observed) and prints the summary as a log checkpoint.
// It never changes a tick, a command, a path or an object. The contract test (test_camera_presentation_contract.py) pins the hooks.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <windows.h>

namespace CameraProbe
{
enum Phase { Idle = 0, Settle, KeyHold, RestAfterKey, EdgeHold, RestAfterEdge, Finished };

struct Row { char kind; int phase; double ms; unsigned logicFrame; double a, b, c, d, e; };

struct State
{
	int enabled = -1;
	std::string dir;
	int fps = 0;
	double holdSeconds = 3.0;
	unsigned delayFrames = 90;
	Phase phase = Idle;
	double phaseStartMs = 0.0;
	bool appliedFps = false;
	bool legacyScroll = false;   // GENERALS_CAMERA_PROBE_LEGACY=1: measure the former post-draw, previous-interval scroll step for a before/after comparison
	bool wroteOutput = false;
	std::vector<Row> rows;
	LARGE_INTEGER freq{}, origin{};
};

inline State &state() { static State s; return s; }

inline double nowMs()
{
	State &s = state();
	LARGE_INTEGER t; QueryPerformanceCounter(&t);
	return 1000.0 * (double)(t.QuadPart - s.origin.QuadPart) / (double)s.freq.QuadPart;
}

inline bool enabled()
{
	State &s = state();
	if (s.enabled >= 0) return s.enabled > 0;
	const char *dir = std::getenv("GENERALS_CAMERA_PROBE");
	s.enabled = (dir && *dir) ? 1 : 0;
	if (s.enabled)
	{
		s.dir = dir;
		const char *legacy = std::getenv("GENERALS_CAMERA_PROBE_LEGACY");
		s.legacyScroll = legacy && *legacy == '1';
		const char *fps = std::getenv("GENERALS_CAMERA_PROBE_FPS");
		s.fps = fps ? std::atoi(fps) : 0;
		const char *hold = std::getenv("GENERALS_CAMERA_PROBE_HOLD");
		if (hold && std::atof(hold) > 0.2) s.holdSeconds = std::atof(hold);
		const char *delay = std::getenv("GENERALS_CAMERA_PROBE_DELAY");
		if (delay && std::atoi(delay) > 0) s.delayFrames = (unsigned)std::atoi(delay);
		QueryPerformanceFrequency(&s.freq); QueryPerformanceCounter(&s.origin);
		s.rows.reserve(1 << 16);
	}
	return s.enabled > 0;
}

inline double percentile(std::vector<double> v, double p)
{
	if (v.empty()) return 0.0;
	std::sort(v.begin(), v.end());
	const double pos = p * (double)(v.size() - 1);
	const size_t lo = (size_t)pos;
	const size_t hi = std::min(v.size() - 1, lo + 1);
	return v[lo] + (v[hi] - v[lo]) * (pos - (double)lo);
}

struct Dist { size_t n = 0; double mean = 0, p50 = 0, p95 = 0, p99 = 0, worst = 0, stddev = 0; };
inline Dist describe(const std::vector<double> &v)
{
	Dist d; d.n = v.size();
	if (v.empty()) return d;
	double sum = 0; for (double x : v) sum += x;
	d.mean = sum / (double)v.size();
	double var = 0; for (double x : v) var += (x - d.mean) * (x - d.mean);
	d.stddev = std::sqrt(var / (double)v.size());
	d.p50 = percentile(v, 0.5); d.p95 = percentile(v, 0.95); d.p99 = percentile(v, 0.99);
	d.worst = *std::max_element(v.begin(), v.end());
	return d;
}

// Number of distinct values of a sequence collapsed to runs: how many consecutive drawn frames showed the SAME camera position (stale frames).
inline void writeOutput()
{
	State &s = state();
	if (s.wroteOutput) return;
	s.wroteOutput = true;
	const std::string csv = s.dir + "\\camera-probe.csv";
	if (FILE *f = std::fopen(csv.c_str(), "w"))
	{
		std::fprintf(f, "kind,phase,ms,logic_frame,a,b,c,d,e\n");
		for (const Row &r : s.rows)
			std::fprintf(f, "%c,%d,%.4f,%u,%.5f,%.5f,%.5f,%.5f,%.5f\n", r.kind, r.phase, r.ms, r.logicFrame, r.a, r.b, r.c, r.d, r.e);
		std::fclose(f);
	}
	std::string text;
	char line[512];
	std::snprintf(line, sizeof line, "target_fps=%d hold_seconds=%.2f rows=%zu\n", s.fps, s.holdSeconds, s.rows.size()); text += line;
	for (int ph : { (int)KeyHold, (int)EdgeHold })
	{
		std::vector<double> frameMs, presentMs, stepDist, stepPerSec, presentSpeed, scrollOffset, scrollUpdateMs;
		const Row *prev = nullptr, *first = nullptr, *last = nullptr, *lastD = nullptr, *prevP = nullptr;
		unsigned stale = 0, drawn = 0, ticksObserved = 0, scrollSteps = 0;
		double firstX = 0, firstY = 0, lastX = 0, lastY = 0;
		std::vector<double> perTickMove;                       // distance moved between the first drawn frame of consecutive logic ticks
		double tickStartX = 0, tickStartY = 0; unsigned tickStartFrame = 0; bool haveTick = false;
		double prevPx = 0, prevPy = 0; bool havePrevPos = false;
		for (const Row &r : s.rows)
		{
			if (r.phase != ph) continue;
			if (r.kind == 'S') { ++scrollSteps; scrollOffset.push_back(std::hypot(r.b, r.c)); scrollUpdateMs.push_back(r.a); continue; }
			if (r.kind == 'P')
			{
				// the frame handed to the swap chain: interval between presents, and the world speed the viewer sees = camera distance / present interval
				if (prevP && lastD)
				{
					const double dt = r.ms - prevP->ms;
					presentMs.push_back(dt);
					if (havePrevPos && dt > 0) presentSpeed.push_back(std::hypot(lastD->d - prevPx, lastD->e - prevPy) / (dt / 1000.0));
				}
				if (lastD) { prevPx = lastD->d; prevPy = lastD->e; havePrevPos = true; }
				prevP = &r;
				continue;
			}
			if (r.kind != 'D') continue;
			lastD = &r;
			++drawn;
			if (!first) { first = &r; firstX = r.d; firstY = r.e; }
			last = &r; lastX = r.d; lastY = r.e;
			if (prev)
			{
				frameMs.push_back(r.ms - prev->ms);
				const double dist = std::hypot(r.d - prev->d, r.e - prev->e);        // the render camera that feeds the matrices
				stepDist.push_back(dist);
				if (dist < 1e-4) ++stale;
				if (r.ms - prev->ms > 0) stepPerSec.push_back(dist / ((r.ms - prev->ms) / 1000.0));
			}
			if (!haveTick || r.logicFrame != tickStartFrame)
			{
				if (haveTick) perTickMove.push_back(std::hypot(r.d - tickStartX, r.e - tickStartY));
				tickStartX = r.d; tickStartY = r.e; tickStartFrame = r.logicFrame; haveTick = true; ++ticksObserved;
			}
			prev = &r;
		}
		const Dist pm = describe(presentMs), ps2 = describe(presentSpeed);
		const Dist ft = describe(frameMs), sd = describe(stepDist), sp = describe(stepPerSec), pt = describe(perTickMove);
		const double elapsed = (first && last) ? (last->ms - first->ms) / 1000.0 : 0.0;
		const double travelled = std::hypot(lastX - firstX, lastY - firstY);
		std::snprintf(line, sizeof line, "[%s] drawn=%u scroll_steps=%u logic_ticks=%u elapsed_s=%.3f distance=%.2f speed_units_per_s=%.2f\n",
			ph == KeyHold ? "key" : "edge", drawn, scrollSteps, ticksObserved, elapsed, travelled, elapsed > 0 ? travelled / elapsed : 0.0); text += line;
		std::snprintf(line, sizeof line, "  frame_ms: n=%zu mean=%.3f p50=%.3f p95=%.3f p99=%.3f worst=%.3f stddev=%.3f var=%.4f\n", ft.n, ft.mean, ft.p50, ft.p95, ft.p99, ft.worst, ft.stddev, ft.stddev * ft.stddev); text += line;
		std::snprintf(line, sizeof line, "  present_interval_ms: n=%zu mean=%.3f p50=%.3f p95=%.3f p99=%.3f worst=%.3f stddev=%.3f var=%.4f\n", pm.n, pm.mean, pm.p50, pm.p95, pm.p99, pm.worst, pm.stddev, pm.stddev * pm.stddev); text += line;
		std::snprintf(line, sizeof line, "  seen_world_speed_units_per_s (camera step / present interval): mean=%.2f p95=%.2f p99=%.2f stddev=%.2f cv=%.4f\n", ps2.mean, ps2.p95, ps2.p99, ps2.stddev, ps2.mean > 0 ? ps2.stddev / ps2.mean : 0.0); text += line;
		std::snprintf(line, sizeof line, "  camera_step_per_frame: mean=%.4f p50=%.4f p95=%.4f p99=%.4f max=%.4f stddev=%.4f stale_frames=%u\n", sd.mean, sd.p50, sd.p95, sd.p99, sd.worst, sd.stddev, stale); text += line;
		std::snprintf(line, sizeof line, "  camera_speed_units_per_s (per draw interval): mean=%.2f p95=%.2f p99=%.2f stddev=%.2f cv=%.4f\n", sp.mean, sp.p95, sp.p99, sp.stddev, sp.mean > 0 ? sp.stddev / sp.mean : 0.0); text += line;
		std::snprintf(line, sizeof line, "  camera_move_per_logic_tick: n=%zu mean=%.4f p95=%.4f max=%.4f stddev=%.4f\n", pt.n, pt.mean, pt.p95, pt.worst, pt.stddev); text += line;
	}
	const std::string txt = s.dir + "\\camera-probe-summary.txt";
	if (FILE *f = std::fopen(txt.c_str(), "w")) { std::fputs(text.c_str(), f); std::fclose(f); }
}
}
