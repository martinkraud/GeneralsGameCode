// SPDX-License-Identifier: GPL-3.0-or-later
// See SpatialQueryDiag.h. Aggregates per caller (first three frames above getClosestObjects), per search radius and per world size; counts repeats within a frame.
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
};
struct CallerKey
{
	void *f[3];
	bool operator<(const CallerKey &o) const { for (int i = 0; i < 3; ++i) if (f[i] != o.f[i]) return f[i] < o.f[i]; return false; }
};

std::map<CallerKey, Agg> s_callers;
std::map<unsigned, Agg> s_byWorld;     // world object count / 100
std::map<unsigned, Agg> s_byRadius;    // radius bucket
Agg s_total;
unsigned s_frame = 0xFFFFFFFFu;
std::unordered_map<unsigned long long, unsigned> s_seen;
unsigned long long s_frames = 0;

unsigned long long mix(unsigned long long h, unsigned long long v) { h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2); return h; }
unsigned radiusBucket(float d) { if (d <= 50.f) return 50; if (d <= 100.f) return 100; if (d <= 200.f) return 200; if (d <= 400.f) return 400; if (d <= 800.f) return 800; if (d < 1.0e5f) return 1600; return 99999; }
void add(Agg &a, const QueryStats &s, unsigned long long ticks, bool repeat, bool all)
{
	++a.calls; a.ticks += ticks; a.cells += s.cells; a.cois += s.cois; a.doneSkips += s.doneSkips; a.distPass += s.distPass; a.filterPass += s.filterPass; a.rings += s.ringsDone;
	if (repeat) ++a.repeats;
	if (all) ++a.collectAll;
}
}

bool g_on = (getenv("GENERALS_SIM_GCO") != nullptr && *getenv("GENERALS_SIM_GCO") != 0);

unsigned long long now()
{
	LARGE_INTEGER c;
	QueryPerformanceCounter(&c);
	return static_cast<unsigned long long>(c.QuadPart);
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

	add(s_callers[key], stats, ticks, repeat, collectsAll);
	add(s_byWorld[worldObjects / 100], stats, ticks, repeat, collectsAll);
	add(s_byRadius[radiusBucket(maxDist)], stats, ticks, repeat, collectsAll);
	add(s_total, stats, ticks, repeat, collectsAll);
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
		fprintf(out, "%-22s calls %10llu | total %9.1f ms | %7.2f us/call | cells %7.1f cois %8.1f (done-skips %6.1f) distOK %6.1f filterOK %6.2f rings %5.1f | repeat-in-frame %5.1f%% | collect-all %5.1f%%\n",
			name, a.calls, ms(a.ticks), 1000.0 * ms(a.ticks) / n, a.cells / n, a.cois / n, a.doneSkips / n, a.distPass / n, a.filterPass / n, a.rings / n, 100.0 * a.repeats / n, 100.0 * a.collectAll / n);
	};
	fprintf(out, "getClosestObjects characterisation: %llu simulation frames with queries, %.1f queries per frame, %.3f ms per frame (timer overhead included)\n", s_frames, static_cast<double>(s_total.calls) / (s_frames ? s_frames : 1), ms(s_total.ticks) / (s_frames ? s_frames : 1));
	row("TOTAL", s_total);
	fprintf(out, "\n--- by search radius (world units; 99999 = unbounded) ---\n");
	for (auto &e : s_byRadius) { char n[48]; snprintf(n, sizeof n, "radius <= %u", e.first); row(n, e.second); }
	fprintf(out, "\n--- by world object count (bucket = count / 100) ---\n");
	for (auto &e : s_byWorld) { char n[48]; snprintf(n, sizeof n, "objects %u00..", e.first); row(n, e.second); }
	fprintf(out, "\n--- by caller (three frames above getClosestObjects), most expensive first ---\n");
	std::vector<std::pair<unsigned long long, const CallerKey *>> order;
	for (auto &e : s_callers) order.push_back(std::make_pair(e.second.ticks, &e.first));
	std::sort(order.begin(), order.end(), [](const std::pair<unsigned long long, const CallerKey *> &a, const std::pair<unsigned long long, const CallerKey *> &b) { return a.first > b.first; });
	for (size_t i = 0; i < order.size() && i < 40; ++i)
	{
		const CallerKey &k = *order[i].second;
		const Agg &a = s_callers[k];
		fprintf(out, "\n#%zu  %.2f%% of query time\n", i + 1, 100.0 * a.ticks / static_cast<double>(s_total.ticks ? s_total.ticks : 1));
		row("   ", a);
		for (int j = 0; j < 3; ++j)
			if (k.f[j]) fprintf(out, "   <- %s\n", SimSampler::describeAddress(reinterpret_cast<unsigned long long>(k.f[j])).c_str());
	}
	fclose(out);
}
}
