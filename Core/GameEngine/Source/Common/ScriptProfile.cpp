// SPDX-License-Identifier: GPL-3.0-or-later
#include "PreRTS.h"

#include "Common/ScriptProfile.h"
#include "Common/PerformanceProfile.h"

#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace ScriptProfile
{
bool g_enabled = false;

namespace
{
constexpr int kMaxType = 1024;
constexpr size_t kWorstFrames = 24;
constexpr size_t kTopPerFrame = 8;

struct Row
{
	std::string name;
	unsigned long long calls = 0, totalNs = 0, maxNs = 0, condNs = 0, actNs = 0;
};
struct TypeRow
{
	std::string name;
	unsigned long long calls = 0, trueCount = 0, totalNs = 0, maxNs = 0;
};
struct FrameEntry { const Row *row; std::uint64_t ns; };
struct FrameRec
{
	unsigned frame = 0;
	std::uint64_t totalNs = 0, scriptsNs = 0, sequentialNs = 0;
	unsigned scriptsRun = 0;
	std::vector<std::pair<std::string, std::uint64_t>> top, topCond, topAct;
	std::map<std::string, std::uint64_t> counters;
};

std::unordered_map<const void *, Row> s_rows;
TypeRow s_cond[kMaxType], s_act[kMaxType];
std::uint64_t s_condNsTotal = 0, s_actNsTotal = 0;
std::map<std::string, std::uint64_t> s_counters, s_frameCounters;
std::vector<FrameRec> s_worst;
std::vector<double> s_frameMs;
unsigned s_frame = 0;
std::uint64_t s_frameStart = 0, s_frameScripts = 0, s_frameSeq = 0;
unsigned s_frameScriptsRun = 0;
int s_depth = 0;
std::vector<FrameEntry> s_frameEntries;
std::map<int, std::uint64_t> s_frameCondNs, s_frameActNs;
unsigned long long s_mod300Ns = 0, s_otherNs = 0, s_mod300Frames = 0, s_otherFrames = 0;
std::uint64_t s_seqTotal = 0;
int s_actDepth = 0;

double ms(unsigned long long ns) { return static_cast<double>(ns) * 1000.0 / static_cast<double>(PerformanceProfile::frequency()); }
}

void enable() { g_enabled = true; }
void reset()
{
	g_enabled = false;
	s_rows.clear();
	for (int i = 0; i < kMaxType; ++i) { s_cond[i] = TypeRow(); s_act[i] = TypeRow(); }
	s_condNsTotal = s_actNsTotal = 0; s_counters.clear(); s_frameCounters.clear(); s_worst.clear(); s_frameMs.clear();
	s_frame = 0; s_frameStart = s_frameScripts = s_frameSeq = 0; s_frameScriptsRun = 0; s_depth = 0; s_actDepth = 0;
	s_frameEntries.clear(); s_frameCondNs.clear(); s_frameActNs.clear();
	s_mod300Ns = s_otherNs = s_mod300Frames = s_otherFrames = 0; s_seqTotal = 0;
}
std::uint64_t now() { return static_cast<std::uint64_t>(PerformanceProfile::now()); }

void beginFrame(unsigned frame)
{
	s_frame = frame; s_frameScripts = 0; s_frameSeq = 0; s_frameScriptsRun = 0; s_depth = 0;
	s_frameEntries.clear(); s_frameCondNs.clear(); s_frameActNs.clear(); s_frameCounters.clear();
	s_frameStart = now();
}

void endFrame()
{
	const std::uint64_t total = now() - s_frameStart;
	s_frameMs.push_back(ms(total));
	if (s_frame % 300 == 0) { s_mod300Ns += total; ++s_mod300Frames; } else { s_otherNs += total; ++s_otherFrames; }
	if (s_worst.size() >= kWorstFrames && total <= s_worst.back().totalNs) return;
	FrameRec rec;
	rec.frame = s_frame; rec.totalNs = total; rec.scriptsNs = s_frameScripts; rec.sequentialNs = s_frameSeq; rec.scriptsRun = s_frameScriptsRun;
	std::sort(s_frameEntries.begin(), s_frameEntries.end(), [](const FrameEntry &a, const FrameEntry &b) { return a.ns > b.ns; });
	for (size_t i = 0; i < s_frameEntries.size() && i < kTopPerFrame; ++i)
		rec.top.emplace_back(s_frameEntries[i].row->name, s_frameEntries[i].ns);
	std::vector<std::pair<std::uint64_t, int>> c, a;
	for (auto &kv : s_frameCondNs) c.emplace_back(kv.second, kv.first);
	for (auto &kv : s_frameActNs) a.emplace_back(kv.second, kv.first);
	std::sort(c.rbegin(), c.rend()); std::sort(a.rbegin(), a.rend());
	for (size_t i = 0; i < c.size() && i < 4; ++i) rec.topCond.emplace_back(s_cond[c[i].second].name, c[i].first);
	for (size_t i = 0; i < a.size() && i < 4; ++i) rec.topAct.emplace_back(s_act[a[i].second].name, a[i].first);
	rec.counters = s_frameCounters;
	s_worst.push_back(std::move(rec));
	std::sort(s_worst.begin(), s_worst.end(), [](const FrameRec &x, const FrameRec &y) { return x.totalNs > y.totalNs; });
	if (s_worst.size() > kWorstFrames) s_worst.pop_back();
}

ScriptTimer::ScriptTimer(const void *script, const char *name)
{
	m_row = nullptr;
	if (!g_enabled) return;
	Row &r = s_rows[script];
	if (r.name.empty()) r.name = name ? name : "?";
	m_row = &r; m_cond0 = s_condNsTotal; m_act0 = s_actNsTotal;
	++s_depth;
	m_start = now();
}

ScriptTimer::~ScriptTimer()
{
	if (!m_row) return;
	const std::uint64_t ns = now() - m_start;
	--s_depth;
	Row &r = *static_cast<Row *>(m_row);
	++r.calls; r.totalNs += ns; r.maxNs = std::max<unsigned long long>(r.maxNs, ns);
	r.condNs += s_condNsTotal - m_cond0; r.actNs += s_actNsTotal - m_act0;
	if (s_depth == 0)
	{
		s_frameScripts += ns; ++s_frameScriptsRun;
		s_frameEntries.push_back({&r, ns});
	}
}

void conditionDone(int type, std::uint64_t start, bool result)
{
	const std::uint64_t ns = now() - start;
	if (type < 0 || type >= kMaxType) return;
	TypeRow &t = s_cond[type];
	++t.calls; t.totalNs += ns; t.maxNs = std::max<unsigned long long>(t.maxNs, ns); if (result) ++t.trueCount;
	s_frameCondNs[type] += ns;
	s_condNsTotal += ns;
}

ActionProbe::ActionProbe(int type) : m_type(type), m_start(0) { if (g_enabled) { ++s_actDepth; m_start = now(); } }
ActionProbe::~ActionProbe()
{
	if (!g_enabled || !m_start) return;
	const std::uint64_t ns = now() - m_start;
	--s_actDepth;
	if (m_type < 0 || m_type >= kMaxType) return;
	TypeRow &t = s_act[m_type];
	++t.calls; t.totalNs += ns; t.maxNs = std::max<unsigned long long>(t.maxNs, ns);
	s_frameActNs[m_type] += ns;
	if (s_actDepth == 0) s_actNsTotal += ns;   // nested actions are inside their parent action's time
}

void sequentialDone(std::uint64_t start) { const std::uint64_t ns = now() - start; s_frameSeq += ns; s_seqTotal += ns; }
bool conditionNameKnown(int type) { return type >= 0 && type < kMaxType && !s_cond[type].name.empty(); }
bool actionNameKnown(int type) { return type >= 0 && type < kMaxType && !s_act[type].name.empty(); }
void setConditionName(int type, const char *name) { if (type >= 0 && type < kMaxType) s_cond[type].name = name ? name : "?"; }
void setActionName(int type, const char *name) { if (type >= 0 && type < kMaxType) s_act[type].name = name ? name : "?"; }
void count(const char *what, std::uint64_t n) { s_counters[what] += n; s_frameCounters[what] += n; }

void report(FILE *out)
{
	if (!out || !g_enabled) return;
	fprintf(out, "SCRIPT PROFILE (GENERALS_SIM_SCRIPTS; clock reads included in every figure; times in ms)\n");
	std::vector<double> v = s_frameMs;
	double total = 0; for (double x : v) total += x;
	std::sort(v.begin(), v.end());
	auto pct = [&](double p) { return v.empty() ? 0.0 : v[std::min(v.size() - 1, static_cast<size_t>(p * static_cast<double>(v.size())))]; };
	fprintf(out, "script updates: %zu | total %.1f | mean %.4f | p99 %.3f | p99.9 %.3f | worst %.3f\n", v.size(), total, v.empty() ? 0.0 : total / static_cast<double>(v.size()), pct(0.99), pct(0.999), v.empty() ? 0.0 : v.back());
	size_t over20 = 0, over33 = 0, over50 = 0, over667 = 0;
	for (double x : v) { over20 += x > 20.0; over33 += x > 33.333; over50 += x > 50.0; over667 += x > 66.67; }
	fprintf(out, "script updates > 20 ms: %zu | > 33.333: %zu | > 50: %zu | > 66.67: %zu\n", over20, over33, over50, over667);
	fprintf(out, "frames divisible by 300: %llu frames, %.1f ms total (%.3f ms mean); other frames: %llu, %.1f ms (%.4f ms mean)\n",
		s_mod300Frames, ms(s_mod300Ns), s_mod300Frames ? ms(s_mod300Ns) / static_cast<double>(s_mod300Frames) : 0.0, s_otherFrames, ms(s_otherNs), s_otherFrames ? ms(s_otherNs) / static_cast<double>(s_otherFrames) : 0.0);
	fprintf(out, "sequential scripts total %.1f ms\n", ms(s_seqTotal));

	std::vector<const Row *> rows; for (auto &kv : s_rows) rows.push_back(&kv.second);
	std::sort(rows.begin(), rows.end(), [](const Row *a, const Row *b) { return a->totalNs > b->totalNs; });
	fprintf(out, "\ntop scripts by cumulative time (inclusive of nested):  name | calls | total | mean | max | cond | actions\n");
	for (size_t i = 0; i < rows.size() && i < 25; ++i)
		fprintf(out, "  %-48s %9llu %9.1f %9.4f %8.3f %9.1f %9.1f\n", rows[i]->name.c_str(), rows[i]->calls, ms(rows[i]->totalNs), ms(rows[i]->totalNs) / static_cast<double>(rows[i]->calls), ms(rows[i]->maxNs), ms(rows[i]->condNs), ms(rows[i]->actNs));
	std::sort(rows.begin(), rows.end(), [](const Row *a, const Row *b) { return a->maxNs > b->maxNs; });
	fprintf(out, "\ntop scripts by worst single evaluation:\n");
	for (size_t i = 0; i < rows.size() && i < 12; ++i)
		fprintf(out, "  %-48s calls %9llu max %8.3f total %9.1f\n", rows[i]->name.c_str(), rows[i]->calls, ms(rows[i]->maxNs), ms(rows[i]->totalNs));

	auto dump = [&](const char *title, TypeRow *t) {
		std::vector<int> idx; for (int i = 0; i < kMaxType; ++i) if (t[i].calls) idx.push_back(i);
		std::sort(idx.begin(), idx.end(), [&](int a, int b) { return t[a].totalNs > t[b].totalNs; });
		fprintf(out, "\n%s by cumulative time: type name | calls | total | mean | max\n", title);
		for (size_t i = 0; i < idx.size() && i < 14; ++i)
			fprintf(out, "  %3d %-44s %10llu %9.1f %9.5f %8.3f\n", idx[i], t[idx[i]].name.c_str(), t[idx[i]].calls, ms(t[idx[i]].totalNs), ms(t[idx[i]].totalNs) / static_cast<double>(t[idx[i]].calls), ms(t[idx[i]].maxNs));
		std::sort(idx.begin(), idx.end(), [&](int a, int b) { return t[a].maxNs > t[b].maxNs; });
		fprintf(out, "%s by worst single call:\n", title);
		for (size_t i = 0; i < idx.size() && i < 8; ++i)
			fprintf(out, "  %3d %-44s calls %10llu max %8.3f total %9.1f\n", idx[i], t[idx[i]].name.c_str(), t[idx[i]].calls, ms(t[idx[i]].maxNs), ms(t[idx[i]].totalNs));
	};
	dump("conditions", s_cond);
	dump("actions", s_act);

	if (!s_counters.empty())
	{
		fprintf(out, "\ncounters:\n");
		for (auto &kv : s_counters) fprintf(out, "  %-40s %llu\n", kv.first.c_str(), static_cast<unsigned long long>(kv.second));
	}
	fprintf(out, "\nworst %zu script updates:\n", s_worst.size());
	for (const FrameRec &f : s_worst)
	{
		fprintf(out, "  frame %6u (mod300=%3u) total %8.3f ms | top-level scripts %8.3f ms (%u run) | sequential %.3f ms\n", f.frame, f.frame % 300, ms(f.totalNs), ms(f.scriptsNs), f.scriptsRun, ms(f.sequentialNs));
		for (auto &p : f.top) fprintf(out, "      script %-46s %8.3f\n", p.first.c_str(), ms(p.second));
		for (auto &p : f.topCond) fprintf(out, "      cond   %-46s %8.3f\n", p.first.c_str(), ms(p.second));
		for (auto &p : f.topAct) fprintf(out, "      action %-46s %8.3f\n", p.first.c_str(), ms(p.second));
		for (auto &p : f.counters) fprintf(out, "      count  %-46s %llu\n", p.first.c_str(), static_cast<unsigned long long>(p.second));
	}
}
}
