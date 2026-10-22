// SPDX-License-Identifier: GPL-3.0-or-later
// X64.51: opt-in AI diagnostics for the dynamic reproduction of the X64.50 findings (GENERALS_AI_DIAG=<output base path>).
//
// Off by default: every hook is one test of a cached bool and nothing is read from or written to the simulation. When on it only COUNTS and LOGS (std::map ordered
// output, no simulation RNG, no extra random draws; the few extra read-only queries it makes are listed at their call sites). Two scenario aids exist for the
// disposable test stages only and are inert unless their own variable is set:
//   GENERALS_AI_DIAG_KILL="<frame>:<Template>@<playerIndex>;..."  (playerIndex -1 = any AI player) destroys the first object of that template owned by that player at that logic frame through the
//        normal death path (so a GLA structure leaves its rebuild hole). The same schedule is applied in Original and Enhanced runs, so they stay comparable.
//   GENERALS_AI_DIAG_DUMP=<frame>[,<frame>...]  prints each skirmish AI's build list (template, automatic, priority, rebuilds, state) at those frames.
// Output: <base>_ai.txt (counters + the first events), written when the process exits.
#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
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
	struct Defense { unsigned frame; int count; int player; bool done; };
	std::vector<Defense> defenses;
	~State()
	{
		if (enabled <= 0 || base.empty()) return;
		if (FILE *f = std::fopen((base + "_ai.txt").c_str(), "w"))
		{
			for (const auto &kv : counters) std::fprintf(f, "counter %s = %lld\n", kv.first.c_str(), kv.second);
			for (const std::string &e : events) std::fprintf(f, "%s\n", e.c_str());
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
	}
	return s.enabled > 0;
}

inline void count(const char *key, long long n = 1) { if (enabled()) state().counters[key] += n; }

inline void event(unsigned frame, const char *tag, const char *format, ...)
{
	if (!enabled()) return;
	State &s = state();
	++s.counters[std::string("events.") + tag];
	if (s.events.size() >= 400) return;
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

inline bool dumpDue(unsigned frame)
{
	if (!enabled()) return false;
	for (unsigned f : state().dumps) if (f == frame) return true;
	return false;
}
}
