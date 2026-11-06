// SPDX-License-Identifier: GPL-3.0-or-later
// X64.sc / X64.aq: opt-in characterisation of PartitionManager::getClosestObjects and of the AI scans that feed it (GENERALS_SIM_GCO=<output file>). Read-only: it counts what a query touched, who asked, what it
// returned and how much of the world changed since the same asker last asked; it never changes what a query returns. When the variable is not set, a query pays for a few local integer increments and one boolean test.
#pragma once

namespace SpatialQueryDiag
{
struct QueryStats
{
	unsigned cells = 0;        ///< partition cells opened
	unsigned cois = 0;         ///< object-in-cell entries looked at
	unsigned doneSkips = 0;    ///< entries skipped because the object was already seen in this query (multi-cell objects)
	unsigned distPass = 0;     ///< objects that were inside the current best distance
	unsigned filterPass = 0;   ///< ... and that the filters also accepted
	unsigned ringsDone = 0;    ///< last ring radius opened
	unsigned results = 0;      ///< objects returned (collect-all queries) or 1/0 (closest-object queries)
	unsigned resultId = 0;     ///< object id of the closest result (0 = none; for collect-all queries the count is compared instead)
};

extern bool g_on;
inline bool enabled() { return g_on; }
unsigned long long now();

/// One finished query. identity is the asking object (or null for a position query, then the position identifies the asker); filterTypes are the vtable pointers of the filters (a filter-set fingerprint).
void record(unsigned frame, unsigned worldObjects, const void *identity, float px, float py, float maxDist, int distanceType, bool collectsAll,
	const void *const *filterTypes, int filterCount, const QueryStats &stats, unsigned long long ticks);

/// The partition manager processed `n` objects whose cells changed this frame (a cheap measure of how much of the world moved).
void noteMoved(unsigned n);

/// A hunt-state attack-priority selection finished (AI::findClosestEnemy with attack info): candidates examined, the final effective priority, the table's maximum priority, the distance modifier,
/// the distance of the winner and of the farthest candidate, and how many candidates lay beyond the distance from which none could win.
void huntSelect(float range, unsigned candidates, int effectivePriority, int maxPriority, float distanceModifier, float winnerDist, float farthestDist, unsigned beyondBound, float boundDist);

/// Which stage of the exact staged hunt scan produced the answer (0..2 = bounded stage, 99 = fell back to the full scan); `verified` = also compared with the full scan (GENERALS_HUNT_VERIFY).
void huntStage(int stage, bool verified);

void report();   ///< writes the tables to the file named by GENERALS_SIM_GCO (no-op when not enabled)
}
