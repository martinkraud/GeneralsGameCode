// SPDX-License-Identifier: GPL-3.0-or-later
// X64.51: opt-in AI diagnostics for the dynamic reproduction of the X64.50 findings (GENERALS_AI_DIAG=<output base path>).
//
// Off by default: every hook is one test of a cached bool and nothing is read from or written to the simulation. When on it only COUNTS and LOGS (std::map ordered
// output, no simulation RNG, no extra random draws; the few extra read-only queries it makes are listed at their call sites). Two scenario aids exist for the
// disposable test stages only and are inert unless their own variable is set:
//   GENERALS_AI_DIAG_KILL="<frame>:<Template>@<playerIndex>;..."  (playerIndex -1 = any AI player) destroys the first object of that template owned by that player at that logic frame through the
//        normal death path (so a GLA structure leaves its rebuild hole). The same schedule is applied in Original and Enhanced runs, so they stay comparable.
//   GENERALS_AI_DIAG_DUMP=<frame>[,<frame>...]  prints each skirmish AI's build list (template, automatic, priority, rebuilds, state) at the first AI update at or after those frames.
//   GENERALS_AI_DIAG_CENSUS=<frame>[,<frame>...]  (X64.52) writes every skirmish AI's structures (template, position, position relative to its base center) to <base>_census.txt.
// Output: <base>_ai.txt (counters + the first events), written when the process exits.
#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>
#include <string>
#include <vector>

namespace AIDiagnostics
{
struct State
{
	int enabled = -1;
	std::string base;
	std::map<std::string, long long> counters;
	std::vector<std::string> events;
	struct Kill { unsigned frame; std::string templateName; int player; bool done; };
	std::vector<Kill> kills;
	std::vector<unsigned> dumps;
	std::vector<unsigned> censuses;
	std::set<std::pair<unsigned, int>> fired;   // (dump / census frame, player) already written
	std::vector<std::string> censusLines;
	struct Defense { unsigned frame; int count; int player; bool done; };
	std::vector<Defense> defenses;
	// X64.52: GENERALS_AI_DIAG_ATTACK="<frame>:<count>:<angleDegrees>@<player>;..." - a scripted harassment of one base from a chosen direction: at <frame> an enemy
	// unit is placed 450 units from the base center at that compass angle (0 = +x, 90 = +y) and, <count> times at 15 frame intervals, deals real (1 point) damage to
	// the nearest base structure through the normal damage path. Identical in Original and Enhanced runs; it exists to prove that attack direction steers Enhanced defences.
	struct Attack { unsigned frame; int count; int angle; int player; bool done; int fired; unsigned source; unsigned next; };
	std::vector<Attack> attacks;
	~State()
	{
		if (enabled <= 0 || base.empty()) return;
		if (FILE *f = std::fopen((base + "_ai.txt").c_str(), "w"))
		{
			for (const auto &kv : counters) std::fprintf(f, "counter %s = %lld\n", kv.first.c_str(), kv.second);
			for (const std::string &e : events) std::fprintf(f, "%s\n", e.c_str());
			std::fclose(f);
		}
		if (!censusLines.empty())
			if (FILE *f = std::fopen((base + "_census.txt").c_str(), "w"))
			{
				for (const std::string &e : censusLines) std::fprintf(f, "%s\n", e.c_str());
				std::fclose(f);
			}
	}
};

inline State &state() { static State s; return s; }

inline bool enabled()
{
	State &s = state();
	if (s.enabled >= 0) return s.enabled > 0;
	const char *base = std::getenv("GENERALS_AI_DIAG");
	s.enabled = (base && *base) ? 1 : 0;
	if (s.enabled)
	{
		s.base = base;
		if (const char *kill = std::getenv("GENERALS_AI_DIAG_KILL"))
		{
			std::string text(kill);
			size_t pos = 0;
			while (pos < text.size())
			{
				size_t end = text.find(';', pos);
				if (end == std::string::npos) end = text.size();
				const std::string item = text.substr(pos, end - pos);
				const size_t colon = item.find(':'), at = item.find('@');
				if (colon != std::string::npos && at != std::string::npos && at > colon)
					s.kills.push_back({ (unsigned)std::strtoul(item.substr(0, colon).c_str(), nullptr, 10), item.substr(colon + 1, at - colon - 1), std::atoi(item.c_str() + at + 1), false });
				pos = end + 1;
			}
		}
		if (const char *defense = std::getenv("GENERALS_AI_DIAG_DEFENSE"))
		{
			// "<frame>:<count>@<playerIndex>;...": at that frame the AI is ordered to place <count> front and <count> flank base defences (a stress of AI-11)
			std::string text(defense);
			size_t pos = 0;
			while (pos < text.size())
			{
				size_t end = text.find(';', pos);
				if (end == std::string::npos) end = text.size();
				const std::string item = text.substr(pos, end - pos);
				const size_t colon = item.find(':'), at = item.find('@');
				if (colon != std::string::npos && at != std::string::npos && at > colon)
					s.defenses.push_back({ (unsigned)std::strtoul(item.substr(0, colon).c_str(), nullptr, 10), std::atoi(item.c_str() + colon + 1), std::atoi(item.c_str() + at + 1), false });
				pos = end + 1;
			}
		}
		if (const char *dump = std::getenv("GENERALS_AI_DIAG_DUMP"))
		{
			std::string text(dump);
			size_t pos = 0;
			while (pos < text.size())
			{
				size_t end = text.find(',', pos);
				if (end == std::string::npos) end = text.size();
				s.dumps.push_back((unsigned)std::strtoul(text.substr(pos, end - pos).c_str(), nullptr, 10));
				pos = end + 1;
			}
		}
		if (const char *attack = std::getenv("GENERALS_AI_DIAG_ATTACK"))
		{
			std::string text(attack);
			size_t pos = 0;
			while (pos < text.size())
			{
				size_t end = text.find(';', pos);
				if (end == std::string::npos) end = text.size();
				const std::string item = text.substr(pos, end - pos);
				const size_t c1 = item.find(':'), c2 = item.find(':', c1 == std::string::npos ? 0 : c1 + 1), at = item.find('@');
				if (c1 != std::string::npos && c2 != std::string::npos && at != std::string::npos && c1 < c2 && c2 < at)
					s.attacks.push_back({ (unsigned)std::strtoul(item.substr(0, c1).c_str(), nullptr, 10), std::atoi(item.c_str() + c1 + 1), std::atoi(item.c_str() + c2 + 1), std::atoi(item.c_str() + at + 1), false, 0, 0u, 0u });
				pos = end + 1;
			}
		}
		if (const char *census = std::getenv("GENERALS_AI_DIAG_CENSUS"))
		{
			std::string text(census);
			size_t pos = 0;
			while (pos < text.size())
			{
				size_t end = text.find(',', pos);
				if (end == std::string::npos) end = text.size();
				s.censuses.push_back((unsigned)std::strtoul(text.substr(pos, end - pos).c_str(), nullptr, 10));
				pos = end + 1;
			}
		}
	}
	return s.enabled > 0;
}

inline void count(const char *key, long long n = 1) { if (enabled()) state().counters[key] += n; }

inline void event(unsigned frame, const char *tag, const char *format, ...)
{
	if (!enabled()) return;
	State &s = state();
	++s.counters[std::string("events.") + tag];
	if (s.events.size() >= 4000) return;
	char body[480];
	va_list args; va_start(args, format);
	std::vsnprintf(body, sizeof body, format, args);
	va_end(args);
	char line[560];
	std::snprintf(line, sizeof line, "event frame=%u %s %s", frame, tag, body);
	s.events.push_back(line);
}

// the next scheduled kill that is due for this player at this frame, or nullptr
inline State::Kill *dueKill(unsigned frame, int playerIndex)
{
	if (!enabled()) return nullptr;
	for (State::Kill &k : state().kills)
		if (!k.done && (k.player == playerIndex || k.player < 0) && frame >= k.frame) return &k;   // player < 0: the first AI that finds such an object
	return nullptr;
}

inline State::Defense *dueDefense(unsigned frame, int playerIndex)
{
	if (!enabled()) return nullptr;
	for (State::Defense &d : state().defenses)
		if (!d.done && (d.player == playerIndex || d.player < 0) && frame >= d.frame) return &d;
	return nullptr;
}

inline State::Attack *dueAttack(unsigned frame, int playerIndex)
{
	if (!enabled()) return nullptr;
	for (State::Attack &a : state().attacks)
		if (!a.done && a.player == playerIndex && frame >= a.frame) return &a;
	return nullptr;
}

// X64.52 ablation (diagnostics runs only): GENERALS_AI_DIAG_X52_OFF="layout,jitter,threat,threatdef,fill,expand" disables those Enhanced base features so that the
// effect of each one can be measured alone. Unset (the shipped state) everything is on; the variable is not read at all unless GENERALS_AI_DIAG is set.
inline bool featureOff(const char *name)
{
	if (!enabled()) return false;
	static const std::string off = []{ const char *v = std::getenv("GENERALS_AI_DIAG_X52_OFF"); return std::string(v ? v : ""); }();
	if (off.empty()) return false;
	const std::string list = "," + off + ",", key = std::string(",") + name + ",";
	return list.find(key) != std::string::npos;
}

// true once per (scheduled frame, player): at the first AI update of that player at or after the scheduled frame
inline bool scheduledOnce(const std::vector<unsigned> &frames, unsigned frame, int playerIndex)
{
	State &s = state();
	for (unsigned f : frames)
		if (frame >= f && s.fired.insert({ f * 2u + (&frames == &s.censuses ? 1u : 0u), playerIndex }).second) return true;
	return false;
}
inline bool dumpDue(unsigned frame, int playerIndex)
{
	if (!enabled()) return false;
	return scheduledOnce(state().dumps, frame, playerIndex);
}
inline bool censusDue(unsigned frame, int playerIndex)
{
	if (!enabled()) return false;
	return scheduledOnce(state().censuses, frame, playerIndex);
}
inline void censusLine(const char *format, ...)
{
	if (!enabled()) return;
	char line[400];
	va_list args; va_start(args, format);
	std::vsnprintf(line, sizeof line, format, args);
	va_end(args);
	state().censusLines.push_back(line);
}
}
