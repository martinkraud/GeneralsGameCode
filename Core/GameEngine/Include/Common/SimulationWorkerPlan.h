// SPDX-License-Identifier: GPL-3.0-or-later
// X64.38: how many simulation workers to use, and when a piece of work is worth handing to them (pure, engine-free).
//
// The authoritative simulation is a lockstep, order-sensitive program; helper threads are only ever an accelerator for pieces of work that
// are proven order-independent (see X64_38_CPU_SCALABILITY.md). This module only decides COUNTS and THRESHOLDS; it never touches game
// state. The serial path (0 workers) is always valid and is the determinism oracle.
//
//   * The setting is 0 (serial), 1, 2, 3, 4, 6, 8 or Auto.
//   * Auto is SMT-aware and makes no assumption about hybrid CPUs: it counts PHYSICAL cores as reported by the OS (a P-core and an E-core
//     are both one core), leaves two cores for the simulation thread's neighbours (renderer, audio, driver, OS) and never exceeds 6.
//     Machines with 4 or fewer physical cores run serial: a worker there would compete with the main thread and the renderer.
//   * An explicit setting is honoured but never exceeds the logical processor count minus one (the main thread keeps its own).
//   * Work is parallelised only when its measured serial cost clearly exceeds the hand-off overhead, so small workloads stay serial and
//     negative scaling is impossible by construction.
#pragma once

namespace SimulationWorkerPlan
{
constexpr int kAuto = -1;
constexpr int kChoices[] = {0, 1, 2, 3, 4, 6, 8};
constexpr int kMaxAutoWorkers = 6;
constexpr int kMaxWorkers = 8;

struct Topology
{
	int logicalProcessors = 1;    // hardware threads (SMT counted)
	int physicalCores = 1;        // cores (a hybrid CPU's P and E cores are one each)
};

inline int clampTopology(int value) { return value < 1 ? 1 : value; }

// Auto policy: physical cores minus two (render + OS/audio), at least 0, at most kMaxAutoWorkers; <= 4 physical cores => serial.
inline int autoWorkers(const Topology &t)
{
	const int physical = clampTopology(t.physicalCores);
	if (physical <= 4) return 0;
	int workers = physical - 2;
	if (workers > kMaxAutoWorkers) workers = kMaxAutoWorkers;
	const int logicalLimit = clampTopology(t.logicalProcessors) - 1;
	if (workers > logicalLimit) workers = logicalLimit < 0 ? 0 : logicalLimit;
	return workers;
}

// The worker count for a setting (kAuto or one of kChoices; anything else is treated as Auto).
inline int workersFor(int setting, const Topology &t)
{
	bool known = setting == kAuto;
	for (int c : kChoices) known = known || setting == c;
	if (!known) setting = kAuto;
	if (setting == kAuto) return autoWorkers(t);
	int workers = setting > kMaxWorkers ? kMaxWorkers : setting;
	const int logicalLimit = clampTopology(t.logicalProcessors) - 1;
	if (workers > logicalLimit) workers = logicalLimit < 0 ? 0 : logicalLimit;
	return workers;
}

// A piece of work is handed to the workers only when the time it would save clearly exceeds the cost of handing it over:
//   serialCost (microseconds, measured/estimated) * (1 - 1/(workers+1)) > overhead * safetyFactor
// where overhead is the measured per-job hand-off cost (wake + join). Anything else stays on the calling thread.
inline bool worthParallelising(double serialMicros, int workers, double handoffMicros, double safetyFactor = 3.0)
{
	if (workers <= 0 || serialMicros <= 0.0) return false;
	const double saved = serialMicros * (1.0 - 1.0 / static_cast<double>(workers + 1));
	return saved > handoffMicros * safetyFactor;
}
}  // namespace SimulationWorkerPlan
