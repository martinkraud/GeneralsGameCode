// SPDX-License-Identifier: GPL-3.0-or-later
// See SpatialQueryDiag.h. Aggregates per caller (first three frames above getClosestObjects), per search radius and per world size; counts repeats within a frame and, per asker and caller, how often the
// answer is identical to the previous one and how much of the world changed in between.
#include "PreRTS.h"

#include "Common/SpatialQueryDiag.h"
#include "Common/SimSampler.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace SpatialQueryDiag
{
namespace
{
const char *outputPath() { const char *v = getenv("GENERALS_SIM_GCO"); return (v && *v) ? v : nullptr; }

struct Agg
{
	unsigned long long calls = 0, ticks = 0, cells = 0, cois = 0, doneSkips = 0, distPass = 0, filterPass = 0, repeats = 0, rings = 0, collectAll = 0;
	unsigned long long empty = 0, emptyTicks = 0, results = 0;
	// rescans: a later query by the same asker through the same caller
	unsigned long long rescans = 0, identical = 0, emptyAgain = 0, emptyAgainTicks = 0, intervalSum = 0;
	unsigned long long movedZero = 0, movedLt100 = 0, movedLt1000 = 0, movedMore = 0;
	unsigned long long gap1 = 0, gap2to10 = 0, gap11to30 = 0, gap31to120 = 0, gapMore = 0;
	unsigned long long askers = 0;
};
struct CallerKey
{
	void *f[3];
	bool operator<(const CallerKey &o) const { for (int i = 0; i < 3; ++i) if (f[i] != o.f[i]) return f[i] < o.f[i]; return false; }
};
struct Last { unsigned frame = 0; unsigned id = 0; unsigned results = 0; unsigned long long moved = 0; };

std::map<CallerKey, Agg> s_callers;
std::map<unsigned, Agg> s_byWorld;     // world object count / 100
std::map<unsigned, Agg> s_byRadius;    // radius bucket
std::map<unsigned, Agg> s_byTime;      // game minute
Agg s_total;
unsigned s_frame = 0xFFFFFFFFu;
std::unordered_map<unsigned long long, unsigned> s_seen;
std::unordered_map<unsigned long long, Last> s_last;   // (caller, asker) -> previous answer
unsigned long long s_frames = 0;
unsigned long long s_movedTotal = 0;

// hunt selection
struct Hunt
{
	unsigned long long scans = 0, candidates = 0, beyond = 0, canPrune = 0, eAtLeast2 = 0, farthestSum = 0, boundSum = 0, prunableBeyond = 0;
	double winnerDistSum = 0, farthestDistSum = 0, boundDistSum = 0;
	std::map<int, unsigned long long> byE;
	std::map<int, unsigned long long> byMaxP;
	float modifier = 0;
} s_hunt, s_huntWide;   // narrow scans (mood targets, small ranges) and wide scans (hunt: range >= 2000)

unsigned long long mix(unsigned long long h, unsigned long long v) { h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2); return h; }
unsigned radiusBucket(float d) { if (d <= 50.f) return 50; if (d <= 100.f) return 100; if (d <= 200.f) return 200; if (d <= 400.f) return 400; if (d <= 800.f) return 800; if (d < 1.0e5f) return 1600; return 99999; }
void add(Agg &a, const QueryStats &s, unsigned long long ticks, bool repeat, bool all)
{
	++a.calls; a.ticks += ticks; a.cells += s.cells; a.cois += s.cois; a.doneSkips += s.doneSkips; a.distPass += s.distPass; a.filterPass += s.filterPass; a.rings += s.ringsDone; a.results += s.results;
	if (repeat) ++a.repeats;
	if (all) ++a.collectAll;
	if (s.results == 0) { ++a.empty; a.emptyTicks += ticks; }
}
}

bool g_on = (getenv("GENERALS_SIM_GCO") != nullptr && *getenv("GENERALS_SIM_GCO") != 0);

unsigned long long now()
{
	LARGE_INTEGER c;
	QueryPerformanceCounter(&c);
	return static_cast<unsigned long long>(c.QuadPart);
}

void noteMoved(unsigned n) { s_movedTotal += n; }

namespace { unsigned long long s_stageCount[4] = {0, 0, 0, 0}, s_stageFallback = 0, s_stageVerified = 0; }
void huntStage(int stage, bool verified)
{
	if (stage >= 0 && stage < 4) ++s_stageCount[stage]; else ++s_stageFallback;
	if (verified) ++s_stageVerified;
}

void huntSelect(float range, unsigned candidates, int effectivePriority, int maxPriority, float distanceModifier, float winnerDist, float farthestDist, unsigned beyondBound, float boundDist)
{
	Hunt &h = range >= 2000.0f ? s_huntWide : s_hunt;
	++h.scans; h.candidates += candidates; h.beyond += beyondBound; h.modifier = distanceModifier;
	++h.byE[effectivePriority]; ++h.byMaxP[maxPriority];
	if (effectivePriority >= 2) ++h.eAtLeast2;
	h.winnerDistSum += winnerDist; h.farthestDistSum += farthestDist; h.boundDistSum += boundDist;
	if (beyondBound > 0) { ++h.canPrune; }
}

void record(unsigned frame, unsigned worldObjects, const void *identity, float px, float py, float maxDist, int distanceType, bool collectsAll,
	const void *const *filterTypes, int filterCount, const QueryStats &stats, unsigned long long ticks)
{
	if (frame != s_frame) { s_frame = frame; s_seen.clear(); ++s_frames; }
	unsigned long long h = 1469598103934665603ull;
	h = mix(h, reinterpret_cast<unsigned long long>(identity));
	if (!identity) { unsigned a, b; memcpy(&a, &px, 4); memcpy(&b, &py, 4); h = mix(h, a); h = mix(h, b); }
	unsigned md; memcpy(&md, &maxDist, 4);
	h = mix(h, md); h = mix(h, static_cast<unsigned>(distanceType)); h = mix(h, collectsAll ? 1u : 0u);
	for (int i = 0; i < filterCount; ++i) h = mix(h, reinterpret_cast<unsigned long long>(filterTypes[i]));
	const bool repeat = ++s_seen[h] > 1;

	CallerKey key = {};
	void *frames[6] = {};
	const USHORT got = RtlCaptureStackBackTrace(2, 5, frames, nullptr);   // skip record() and getClosestObjects()
	for (int i = 0; i < 3 && i < got; ++i) key.f[i] = frames[i];

	Agg &caller = s_callers[key];
	add(caller, stats, ticks, repeat, collectsAll);
	add(s_byWorld[worldObjects / 100], stats, ticks, repeat, collectsAll);
	add(s_byRadius[radiusBucket(maxDist)], stats, ticks, repeat, collectsAll);
	add(s_byTime[frame / 1800], stats, ticks, repeat, collectsAll);
	add(s_total, stats, ticks, repeat, collectsAll);

	// rescan statistics: the same asker (object, or the guarded position) asking through the same caller again
	unsigned long long askerKey = mix(reinterpret_cast<unsigned long long>(identity), 7);
	if (!identity) { unsigned a, b; memcpy(&a, &px, 4); memcpy(&b, &py, 4); askerKey = mix(mix(askerKey, a), b); }
	askerKey = mix(askerKey, reinterpret_cast<unsigned long long>(key.f[0]));
	askerKey = mix(askerKey, reinterpret_cast<unsigned long long>(key.f[1]));
	auto it = s_last.find(askerKey);
	if (it == s_last.end())
	{
		++caller.askers;
	}
	else
	{
		const Last &prev = it->second;
		++caller.rescans;
		const unsigned gap = frame - prev.frame;
		caller.intervalSum += gap;
		if (gap <= 1) ++caller.gap1; else if (gap <= 10) ++caller.gap2to10; else if (gap <= 30) ++caller.gap11to30; else if (gap <= 120) ++caller.gap31to120; else ++caller.gapMore;
		const bool same = collectsAll ? (prev.results == stats.results) : (prev.id == stats.resultId);
		if (same) ++caller.identical;
		if (same && stats.results == 0 && prev.results == 0) { ++caller.emptyAgain; caller.emptyAgainTicks += ticks; }
		const unsigned long long moved = s_movedTotal - prev.moved;
		if (moved == 0) ++caller.movedZero; else if (moved < 100) ++caller.movedLt100; else if (moved < 1000) ++caller.movedLt1000; else ++caller.movedMore;
	}
	Last &l = s_last[askerKey];
	l.frame = frame; l.id = stats.resultId; l.results = stats.results; l.moved = s_movedTotal;
}

void report()
{
	const char *path = outputPath();
	if (!path || s_total.calls == 0) return;
	LARGE_INTEGER f; QueryPerformanceFrequency(&f);
	const unsigned long long freq = static_cast<unsigned long long>(f.QuadPart);
	FILE *out = fopen(path, "wb");
	if (!out) return;
	auto ms = [&](unsigned long long t) { return 1000.0 * static_cast<double>(t) / static_cast<double>(freq); };
	auto row = [&](const char *name, const Agg &a) {
		const double n = static_cast<double>(a.calls ? a.calls : 1);
		fprintf(out, "%-18s calls %9llu | %8.1f ms %6.2f us/call | cells %6.0f cois %6.0f (skips %5.0f) distOK %5.0f filterOK %5.1f rings %4.0f | empty %5.1f%% | in-frame repeat %4.1f%% | collect-all %5.1f%%\n",
			name, a.calls, ms(a.ticks), 1000.0 * ms(a.ticks) / n, a.cells / n, a.cois / n, a.doneSkips / n, a.distPass / n, a.filterPass / n, a.rings / n, 100.0 * a.empty / n, 100.0 * a.repeats / n, 100.0 * a.collectAll / n);
	};
	fprintf(out, "getClosestObjects characterisation: %llu simulation frames with queries, %.1f queries per frame, %.3f ms per frame (timer overhead included)\n", s_frames, static_cast<double>(s_total.calls) / (s_frames ? s_frames : 1), ms(s_total.ticks) / (s_frames ? s_frames : 1));
	row("TOTAL", s_total);
	fprintf(out, "empty answers: %llu of %llu queries cost %.1f ms (%.1f%% of the query time)\n", s_total.empty, s_total.calls, ms(s_total.emptyTicks), 100.0 * s_total.emptyTicks / static_cast<double>(s_total.ticks ? s_total.ticks : 1));
	fprintf(out, "\n--- by search radius (world units; 99999 = unbounded) ---\n");
	for (auto &e : s_byRadius) { char n[48]; snprintf(n, sizeof n, "radius <= %u", e.first); row(n, e.second); }
	fprintf(out, "\n--- by game minute ---\n");
	for (auto &e : s_byTime) { char n[48]; snprintf(n, sizeof n, "minute %u", e.first); row(n, e.second); }
	fprintf(out, "\n--- by world object count (bucket = count / 100) ---\n");
	for (auto &e : s_byWorld) { char n[48]; snprintf(n, sizeof n, "objects %u00..", e.first); row(n, e.second); }

	for (int pass = 0; pass < 2; ++pass)
	{
		const Hunt &H = pass ? s_huntWide : s_hunt;
		fprintf(out, "\n--- attack-priority selections (AI::findClosestEnemy with attack info), %s ---\n", pass ? "WIDE scans (range >= 2000: hunt state)" : "narrow scans (mood targets)");
		if (H.scans)
		{
			const double n = static_cast<double>(H.scans);
			fprintf(out, "scans %llu | candidates per scan %.1f | winner at %.0f, farthest candidate at %.0f world units (mean) | distance modifier %.1f\n", H.scans, H.candidates / n, H.winnerDistSum / n, H.farthestDistSum / n, H.modifier);
			fprintf(out, "scans whose final effective priority is >= 2 (the only case where a distance bound can end the search): %llu (%.1f%%)\n", H.eAtLeast2, 100.0 * H.eAtLeast2 / n);
			fprintf(out, "scans with candidates beyond the bound from which none can win: %llu (%.1f%%); candidates beyond it: %llu of %llu (%.1f%%); mean bound %.0f world units\n", H.canPrune, 100.0 * H.canPrune / n, H.beyond, H.candidates, 100.0 * H.beyond / static_cast<double>(H.candidates ? H.candidates : 1), H.boundDistSum / n);
			fprintf(out, "final effective priority histogram:"); for (auto &e : H.byE) fprintf(out, " %d:%llu", e.first, e.second); fprintf(out, "\n");
			fprintf(out, "table maximum priority histogram:"); for (auto &e : H.byMaxP) fprintf(out, " %d:%llu", e.first, e.second); fprintf(out, "\n");
		}
		else fprintf(out, "none\n");
	}

	fprintf(out, "\n--- staged hunt scan: answers by stage ---\nstage 0 (R = 3.5 modifier steps): %llu | stage 1 (7.5): %llu | stage 2 (14.5): %llu | fell back to the full scan: %llu | verified against the full scan: %llu\n",
		s_stageCount[0], s_stageCount[1], s_stageCount[2], s_stageFallback, s_stageVerified);

	fprintf(out, "\n--- by caller (three frames above getClosestObjects), most expensive first ---\n");
	std::vector<std::pair<unsigned long long, const CallerKey *>> order;
	for (auto &e : s_callers) order.push_back(std::make_pair(e.second.ticks, &e.first));
	std::sort(order.begin(), order.end(), [](const std::pair<unsigned long long, const CallerKey *> &a, const std::pair<unsigned long long, const CallerKey *> &b) { return a.first > b.first; });
	for (size_t i = 0; i < order.size() && i < 25; ++i)
	{
		const CallerKey &k = *order[i].second;
		const Agg &a = s_callers[k];
		fprintf(out, "\n#%zu  %.2f%% of query time\n", i + 1, 100.0 * a.ticks / static_cast<double>(s_total.ticks ? s_total.ticks : 1));
		row("   ", a);
		fprintf(out, "   askers %llu | rescans %llu: identical answer %.1f%%, empty-again %.1f%% (%.1f ms) | mean gap %.1f frames (1:%.0f%% 2-10:%.0f%% 11-30:%.0f%% 31-120:%.0f%% >120:%.0f%%) | world movers since the previous scan: 0:%.1f%% <100:%.1f%% <1000:%.1f%% more:%.1f%%\n",
			a.askers, a.rescans, 100.0 * a.identical / static_cast<double>(a.rescans ? a.rescans : 1), 100.0 * a.emptyAgain / static_cast<double>(a.rescans ? a.rescans : 1), ms(a.emptyAgainTicks),
			static_cast<double>(a.intervalSum) / static_cast<double>(a.rescans ? a.rescans : 1),
			100.0 * a.gap1 / static_cast<double>(a.rescans ? a.rescans : 1), 100.0 * a.gap2to10 / static_cast<double>(a.rescans ? a.rescans : 1), 100.0 * a.gap11to30 / static_cast<double>(a.rescans ? a.rescans : 1), 100.0 * a.gap31to120 / static_cast<double>(a.rescans ? a.rescans : 1), 100.0 * a.gapMore / static_cast<double>(a.rescans ? a.rescans : 1),
			100.0 * a.movedZero / static_cast<double>(a.rescans ? a.rescans : 1), 100.0 * a.movedLt100 / static_cast<double>(a.rescans ? a.rescans : 1), 100.0 * a.movedLt1000 / static_cast<double>(a.rescans ? a.rescans : 1), 100.0 * a.movedMore / static_cast<double>(a.rescans ? a.rescans : 1));
		for (int j = 0; j < 3; ++j)
			if (k.f[j]) fprintf(out, "   <- %s\n", SimSampler::describeAddress(reinterpret_cast<unsigned long long>(k.f[j])).c_str());
	}
	fclose(out);
}
}
