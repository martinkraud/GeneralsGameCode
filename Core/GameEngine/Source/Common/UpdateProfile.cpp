// SPDX-License-Identifier: GPL-3.0-or-later
// See UpdateProfile.h.
#include "PreRTS.h"

#include "Common/UpdateProfile.h"
#include "Common/SimSampler.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace UpdateProfile
{
namespace
{
struct Agg { unsigned long long calls = 0, ticks = 0; };
std::unordered_map<const void *, Agg> s_modules;
Agg s_states[64];
Agg s_parts[4];
const char *outputPath() { const char *v = getenv("GENERALS_SIM_UPDPROF"); return (v && *v) ? v : nullptr; }
}

bool g_on = (getenv("GENERALS_SIM_UPDPROF") != nullptr && *getenv("GENERALS_SIM_UPDPROF") != 0);

unsigned long long now()
{
	LARGE_INTEGER c;
	QueryPerformanceCounter(&c);
	return static_cast<unsigned long long>(c.QuadPart);
}

void noteModule(const void *vtable, unsigned long long ticks) { Agg &a = s_modules[vtable]; ++a.calls; a.ticks += ticks; }
void addAiState(int stateId, unsigned long long ticks) { if (stateId < 0 || stateId >= 64) stateId = 63; ++s_states[stateId].calls; s_states[stateId].ticks += ticks; }
void addAiPart(int part, unsigned long long ticks) { if (part < 0 || part > 3) return; ++s_parts[part].calls; s_parts[part].ticks += ticks; }

void report()
{
	const char *path = outputPath();
	if (!path || s_modules.empty()) return;
	LARGE_INTEGER f; QueryPerformanceFrequency(&f);
	const double freq = static_cast<double>(f.QuadPart);
	auto ms = [&](unsigned long long t) { return 1000.0 * static_cast<double>(t) / freq; };
	FILE *out = fopen(path, "wb");
	if (!out) return;
	unsigned long long totalTicks = 0, totalCalls = 0;
	for (auto &e : s_modules) { totalTicks += e.second.ticks; totalCalls += e.second.calls; }
	fprintf(out, "update-module profile: %llu update calls, %.1f ms inclusive (timer overhead of about 40 ns per call included)\n\n", totalCalls, ms(totalTicks));
	fprintf(out, "--- update modules by inclusive time ---\n");
	std::vector<std::pair<unsigned long long, const void *>> order;
	for (auto &e : s_modules) order.push_back(std::make_pair(e.second.ticks, e.first));
	std::sort(order.begin(), order.end(), [](const std::pair<unsigned long long, const void *> &a, const std::pair<unsigned long long, const void *> &b) { return a.first > b.first; });
	for (size_t i = 0; i < order.size() && i < 45; ++i)
	{
		const Agg &a = s_modules[order[i].second];
		fprintf(out, "%6.2f%%  %9.1f ms  %10llu calls  %7.2f us/call  %s\n", 100.0 * a.ticks / static_cast<double>(totalTicks ? totalTicks : 1), ms(a.ticks), a.calls, 1000.0 * ms(a.ticks) / static_cast<double>(a.calls ? a.calls : 1),
			SimSampler::describeAddress(reinterpret_cast<unsigned long long>(order[i].second)).c_str());
	}
	fprintf(out, "\n--- AIUpdateInterface::update parts ---\n");
	static const char *partNames[4] = { "whole update", "state machine", "turret AI", "locomotor" };
	for (int i = 0; i < 4; ++i)
		fprintf(out, "%-14s %9.1f ms %10llu calls %7.2f us/call\n", partNames[i], ms(s_parts[i].ticks), s_parts[i].calls, 1000.0 * ms(s_parts[i].ticks) / static_cast<double>(s_parts[i].calls ? s_parts[i].calls : 1));
	fprintf(out, "\n--- state machine time by the AI state the object was in (AIStateType id) ---\n");
	std::vector<std::pair<unsigned long long, int>> st;
	for (int i = 0; i < 64; ++i) if (s_states[i].calls) st.push_back(std::make_pair(s_states[i].ticks, i));
	std::sort(st.begin(), st.end(), [](const std::pair<unsigned long long, int> &a, const std::pair<unsigned long long, int> &b) { return a.first > b.first; });
	for (auto &e : st)
		fprintf(out, "state %2d  %9.1f ms %10llu calls %7.2f us/call\n", e.second, ms(s_states[e.second].ticks), s_states[e.second].calls, 1000.0 * ms(s_states[e.second].ticks) / static_cast<double>(s_states[e.second].calls ? s_states[e.second].calls : 1));
	fclose(out);
}
}
