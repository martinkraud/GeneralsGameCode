// SPDX-License-Identifier: GPL-3.0-or-later
// X64.42: opt-in attribution of the script engine's per-tick time (GENERALS_SIM_SCRIPTS=1). Everything is inert unless
// ScriptProfile::enable() is called (the headless benchmark runner does when the variable is set); the hooks in ScriptEngine.cpp are a
// single predictable branch on a plain bool otherwise. Output is bounded: per-script / per-condition-type / per-action-type tables and the
// worst N script ticks with their top contributors, never a per-frame log.
#pragma once

#include <cstdint>
#include <cstdio>

namespace ScriptProfile
{
extern bool g_enabled;
inline bool enabled() { return g_enabled; }
void enable();
void reset();   // clears every table and disables (tests)

std::uint64_t now();

// One ScriptEngine::update() call.
void beginFrame(unsigned frame);
void endFrame();

// A script evaluation (executeScript); nested (called through a subroutine) scripts are counted inclusive in their parent.
struct ScriptTimer
{
	ScriptTimer(const void *script, const char *name);
	~ScriptTimer();
	void *m_row; std::uint64_t m_start; std::uint64_t m_cond0, m_act0;
};

void conditionDone(int type, std::uint64_t start, bool result);
void sequentialDone(std::uint64_t start);

// RAII probes; all inert (a single bool test) when profiling is off.
struct FrameProbe { explicit FrameProbe(unsigned frame) { if (g_enabled) beginFrame(frame); } ~FrameProbe() { if (g_enabled) endFrame(); } };
struct ConditionProbe
{
	explicit ConditionProbe(int type) : m_type(type), m_start(g_enabled ? now() : 0) {}
	void finish(bool result) { if (g_enabled) conditionDone(m_type, m_start, result); }
	int m_type; std::uint64_t m_start;
};
struct ActionProbe { explicit ActionProbe(int type); ~ActionProbe(); int m_type; std::uint64_t m_start; };
struct SequentialProbe { SequentialProbe() : m_start(g_enabled ? now() : 0) {} ~SequentialProbe() { if (g_enabled) sequentialDone(m_start); } std::uint64_t m_start; };
bool conditionNameKnown(int type);
bool actionNameKnown(int type);
void setConditionName(int type, const char *name);
void setActionName(int type, const char *name);

// Free-form per-operation counters (e.g. objects scanned), also attributed to the frame.
void count(const char *what, std::uint64_t n);

void report(FILE *out);
}
