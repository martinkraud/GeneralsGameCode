// SPDX-License-Identifier: GPL-3.0-or-later
// X64.43: diagnostic-only characterisation of A* searches (Pathfinder::internalFindPath) that exhaust the open list without reaching the goal.
//
// Off by default: enabled only when the environment variable GENERALS_SIM_PATHFAIL is set (the headless benchmark runner then prints the report). With the
// switch off every hook is one predictable branch on a plain bool. With it on the code only READS pathfinder state (cells, lists, objects); it never
// changes a search, a path, a cell or an object, and it consumes no random numbers. Its own cost is paid only by searches that FAIL after work (a few
// hundred per 72 000 ticks) plus a signature update for every search that reaches the main loop; timing runs and diagnostic runs are kept apart.
#pragma once

#include <cstddef>
#include <cstdio>
#include <string>

class Pathfinder;
class Object;
class PathfindCell;
class LocomotorSet;
struct Coord3D;

namespace PathFailureDiag
{
extern int g_state;                 // -1 = not read yet, 0 = off, 1 = full characterisation (GENERALS_SIM_PATHFAIL), 2 = lite endurance mode (EnduranceTelemetry)
extern unsigned g_mutations;        // pathfinder cell-state mutations (unit position / goal / terrain type / obstacle changes)
bool initState();
inline bool enabled() { return g_state > 0 ? true : (g_state == 0 ? false : initState()); }
inline void noteMapMutation() { if (g_state > 0) ++g_mutations; }

// The hierarchical pre-search result of the request that is about to run its A* (1 = found a hierarchical path, 0 = fell back to "all zones passable").
void setHierarchical(int found);
// A zone recalculation happened (the pathfinder topology generation changed).
void noteZoneRecalculation();

struct Search
{
	bool active = false;
	bool done = false;
	unsigned long long startTick = 0;
	unsigned pops = 0;
	unsigned long long signature = 0;
	// copies of the request, valid when active
	Pathfinder *pf = nullptr;
	Object *obj = nullptr;
	const LocomotorSet *loco = nullptr;
	const Coord3D *from = nullptr, *rawTo = nullptr, *to = nullptr;
	PathfindCell *startCell = nullptr, *goalCell = nullptr;
	int zone1 = 0, zone2 = 0, radius = 0;
	bool center = true, human = true, crusher = false, tunneling = false;
	int layer = 0, destLayer = 0;
	unsigned ignoreObstacle = 0;

	Search() { }
	void begin(Pathfinder *p, Object *o, const LocomotorSet *l, const Coord3D *f, const Coord3D *raw, const Coord3D *adj, PathfindCell *start, PathfindCell *goal,
		int z1, int z2, int rad, bool ctr, bool isHuman, bool isCrusher, bool tunnel, int startLayer, int goalLayer, unsigned ignoreId);
	void pop() { if (active) ++pops; }
	void succeeded();
	// Call BEFORE the failure cleanup: the closed list and the cell infos still describe the exhausted search.
	void failed(int cellsAllocated);
};

void report(FILE *out);
void writeCsv(FILE *out);

// X64.44 lite mode (used by EnduranceTelemetry; never combined with the full mode): cheap counters for every search plus a rate-limited analysis of an
// expensive failure (>= 5 ms, at most one per 300 logic frames). The analysis time is measured so the telemetry can subtract it from the tick it ran in.
struct LiteCounters
{
	unsigned long long searches = 0, successes = 0, failures = 0, expensiveFailures = 0, analyses = 0, failurePops = 0, zoneRecalcs = 0, mutations = 0;
	unsigned long long frontierCliff = 0, frontierStructure = 0, frontierAllyBusy = 0, frontierEnemyFixed = 0, frontierOffMap = 0, blockerUnits = 0, analysedClosedCells = 0;
	double failureMs = 0, successMs = 0, worstFailureMs = 0, analysisMs = 0;
};
void enableLite();                       // switches the diagnostic to lite mode unless the full mode is already on
const LiteCounters &lite();
const char *sampleCsvHeader();
size_t drainSampleCsv(std::string &out);  // appends the CSV rows of the analysed expensive failures since the last call
}
