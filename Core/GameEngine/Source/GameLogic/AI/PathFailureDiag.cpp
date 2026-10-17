// SPDX-License-Identifier: GPL-3.0-or-later
// X64.43: see PathFailureDiag.h. Diagnostic only; reads pathfinder state, never writes it.
#include "PreRTS.h"

#include "GameLogic/Pathfinder/PathFailureDiag.h"
#include "Common/PerformanceProfile.h"

#include "GameLogic/AIPathfind.h"
#include "Common/Player.h"
#include "Common/Team.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Friend of Pathfinder (declared in AIPathfind.h): the only place that reads its private lists and map.
struct PathFailureDiagAccess
{
	static void analyze(PathFailureDiag::Search &s, int cellsAllocated);
};

namespace PathFailureDiag
{
namespace
{
int s_hierarchical = -1;
unsigned s_zoneRecalcs = 0;
unsigned long long s_searches = 0, s_successes = 0, s_failures = 0;
double s_failMs = 0, s_successMs = 0;

enum Reason { R_TERRAIN_CLIFF, R_TERRAIN_WATER, R_TERRAIN_RUBBLE, R_TERRAIN_OBSTACLE, R_TERRAIN_OTHER, R_OFF_MAP, R_ALLY_NO_AI, R_ALLY_NOT_IDLE, R_ENEMY_FIXED, R_PASSES, R_COUNT };
const char *reasonName(int r)
{
	static const char *n[R_COUNT] = { "terrain: cliff", "terrain: water", "terrain: rubble", "terrain: structure (obstacle)", "terrain: other impassable", "footprint off map",
		"ally without AI in footprint", "ALLY FIXED NON-IDLE unit in footprint", "ENEMY FIXED unit in footprint (cannot crush)", "passes (diagonal rule / other)" };
	return n[r];
}

struct FailRec
{
	unsigned frame = 0, object = 0;
	std::string tmpl, team, goalObstacle;
	int player = -1, human = 0, aiState = -1, cmd = -1, attackPath = 0, hier = -1;
	unsigned surfaces = 0;
	int crusher = 0, radius = 0, center = 1, layer = 0, destLayer = 0;
	float fx = 0, fy = 0, rx = 0, ry = 0, ax = 0, ay = 0;
	int sx = 0, sy = 0, gx = 0, gy = 0, zone1 = 0, zone2 = 0;
	unsigned pops = 0, cells = 0, closed = 0;
	double ms = 0;
	unsigned long long sig = 0;
	unsigned repeat = 0, sigPrevFrame = 0, zoneRecalcsSincePrev = 0, mutationsSincePrev = 0;
	int goalType = -1, goalFlags = -1, goalPinched = 0, goalReachedNeighbours = 0, goalValidNeighbours = 0, goalNeighbours = 0;
	double nearestClosed = -1;
	int nearestX = 0, nearestY = 0, bboxW = 0, bboxH = 0;
	unsigned frontier[R_COUNT] = { 0 };
	unsigned blockerCells = 0, blockerUnits = 0;
	int staticBfs = -1;   // 1 goal reachable by terrain rules from the start (so the failure is dynamic), 0 not reachable, -1 not evaluated
	unsigned bfsCells = 0;
	float goalToEdgeMin = 0;
};

struct SigStat
{
	unsigned count = 0, fails = 0, firstFrame = 0, lastFrame = 0, flips = 0, zoneRecalcAtLast = 0, zoneRecalcAtFirst = 0, mutationsAtLast = 0;
	int lastResult = -1;
};

std::vector<FailRec> s_records;
std::unordered_map<unsigned long long, SigStat> s_sigs;
std::map<std::string, unsigned> s_blockerTemplates, s_blockerSides;
std::map<std::string, unsigned> s_blockerRel;

unsigned long long mix(unsigned long long h, unsigned long long v)
{
	h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
	return h;
}
double nowMs()
{
	return static_cast<double>(PerformanceProfile::now()) * 1000.0 / static_cast<double>(PerformanceProfile::frequency());
}
}

int g_state = -1;
unsigned g_mutations = 0;
bool initState()
{
	g_state = getenv("GENERALS_SIM_PATHFAIL") ? 1 : 0;
	return g_state > 0;
}
void setHierarchical(int found) { s_hierarchical = found; }
void noteZoneRecalculation() { ++s_zoneRecalcs; }

void Search::begin(Pathfinder *p, Object *o, const LocomotorSet *l, const Coord3D *f, const Coord3D *raw, const Coord3D *adj, PathfindCell *start, PathfindCell *goal,
	int z1, int z2, int rad, bool ctr, bool isHuman, bool isCrusher, bool tunnel, int startLayer, int goalLayer, unsigned ignoreId)
{
	if (!enabled()) return;
	active = true;
	pf = p; obj = o; loco = l; from = f; rawTo = raw; to = adj; startCell = start; goalCell = goal;
	zone1 = z1; zone2 = z2; radius = rad; center = ctr; human = isHuman; crusher = isCrusher; tunneling = tunnel;
	layer = startLayer; destLayer = goalLayer; ignoreObstacle = ignoreId;
	startTick = PerformanceProfile::now();
	// request signature: everything the search outcome depends on in the STATIC part of its input (cells, class of mover, flags); not unit positions.
	unsigned long long h = 1469598103934665603ULL;
	h = mix(h, static_cast<unsigned long long>(start->getXIndex())); h = mix(h, static_cast<unsigned long long>(start->getYIndex()));
	h = mix(h, static_cast<unsigned long long>(goal->getXIndex())); h = mix(h, static_cast<unsigned long long>(goal->getYIndex()));
	h = mix(h, static_cast<unsigned long long>(l->getValidSurfaces())); h = mix(h, isCrusher ? 1 : 0); h = mix(h, static_cast<unsigned long long>(rad));
	h = mix(h, ctr ? 1 : 0); h = mix(h, isHuman ? 1 : 0); h = mix(h, static_cast<unsigned long long>(startLayer)); h = mix(h, static_cast<unsigned long long>(goalLayer));
	h = mix(h, ignoreId); h = mix(h, tunnel ? 1 : 0);
	signature = h;
	++s_searches;
}

void Search::succeeded()
{
	if (!active || done) return;
	done = true;
	++s_successes;
	s_successMs += static_cast<double>(PerformanceProfile::now() - startTick) * 1000.0 / static_cast<double>(PerformanceProfile::frequency());
	SigStat &st = s_sigs[signature];
	if (st.count == 0) { st.firstFrame = TheGameLogic->getFrame(); st.zoneRecalcAtFirst = s_zoneRecalcs; }
	if (st.lastResult == 0) ++st.flips;
	++st.count; st.lastResult = 1; st.lastFrame = TheGameLogic->getFrame(); st.zoneRecalcAtLast = s_zoneRecalcs; st.mutationsAtLast = g_mutations;
}

void Search::failed(int cellsAllocated)
{
	if (!active || done) return;
	done = true;
	PathFailureDiagAccess::analyze(*this, cellsAllocated);
}

}   // namespace PathFailureDiag

void PathFailureDiagAccess::analyze(PathFailureDiag::Search &s, int cellsAllocated)
{
	using namespace PathFailureDiag;
	Pathfinder &pf = *s.pf;
	FailRec r;
	r.ms = static_cast<double>(PerformanceProfile::now() - s.startTick) * 1000.0 / static_cast<double>(PerformanceProfile::frequency());
	++s_failures; s_failMs += r.ms;
	r.frame = TheGameLogic->getFrame();
	r.pops = s.pops; r.cells = static_cast<unsigned>(cellsAllocated); r.sig = s.signature; r.hier = s_hierarchical;
	r.zone1 = s.zone1; r.zone2 = s.zone2; r.radius = s.radius; r.center = s.center; r.human = s.human; r.crusher = s.crusher;
	r.layer = s.layer; r.destLayer = s.destLayer; r.surfaces = static_cast<unsigned>(s.loco->getValidSurfaces());
	r.fx = s.from->x; r.fy = s.from->y; r.rx = s.rawTo->x; r.ry = s.rawTo->y; r.ax = s.to->x; r.ay = s.to->y;
	r.sx = s.startCell->getXIndex(); r.sy = s.startCell->getYIndex(); r.gx = s.goalCell->getXIndex(); r.gy = s.goalCell->getYIndex();
	if (s.obj)
	{
		r.object = s.obj->getID();
		r.tmpl = s.obj->getTemplate()->getName().str();
		if (s.obj->getControllingPlayer()) r.player = s.obj->getControllingPlayer()->getPlayerIndex();
		if (s.obj->getTeam() && s.obj->getTeam()->getPrototype()) r.team = s.obj->getTeam()->getPrototype()->getName().str();
		if (AIUpdateInterface *ai = s.obj->getAIUpdateInterface())
		{
			r.aiState = static_cast<int>(ai->getAIStateType());
			r.cmd = static_cast<int>(ai->getLastCommandSource());
			r.attackPath = ai->isAttackPath() ? 1 : 0;
		}
	}
	// repetition
	{
		SigStat &st = s_sigs[s.signature];
		if (st.count == 0) { st.firstFrame = r.frame; st.zoneRecalcAtFirst = s_zoneRecalcs; }
		r.repeat = st.count;
		r.sigPrevFrame = st.lastFrame;
		r.zoneRecalcsSincePrev = st.count ? s_zoneRecalcs - st.zoneRecalcAtLast : 0;
		r.mutationsSincePrev = st.count ? g_mutations - st.mutationsAtLast : 0;
		if (st.lastResult == 1) ++st.flips;
		++st.count; ++st.fails; st.lastResult = 0; st.lastFrame = r.frame; st.zoneRecalcAtLast = s_zoneRecalcs; st.mutationsAtLast = g_mutations;
	}

	const LocomotorSurfaceTypeMask surfaces = s.loco->getValidSurfaces();
	// goal cell facts
	PathfindCell *goal = s.goalCell;
	r.goalType = static_cast<int>(goal->getType());
	r.goalFlags = static_cast<int>(goal->getFlags());
	r.goalPinched = goal->getPinched() ? 1 : 0;
	if (goal->getObstacleID() != INVALID_ID)
	{
		Object *ob = TheGameLogic->findObjectByID(goal->getObstacleID());
		r.goalObstacle = ob ? ob->getTemplate()->getName().str() : "(gone)";
	}
	static const int dx8[8] = { 1, 0, -1, 0, 1, -1, -1, 1 };
	static const int dy8[8] = { 0, 1, 0, -1, 1, 1, -1, -1 };
	for (int i = 0; i < 8; ++i)
	{
		PathfindCell *n = pf.getCell(goal->getLayer(), r.gx + dx8[i], r.gy + dy8[i]);
		if (!n) continue;
		++r.goalNeighbours;
		if (pf.validMovementPosition(s.crusher, surfaces, n, goal)) ++r.goalValidNeighbours;
		if (n->hasInfo() && (n->getOpen() || n->getClosed())) ++r.goalReachedNeighbours;
	}
	// explored region: closed list
	int minX = 1 << 30, minY = 1 << 30, maxX = -(1 << 30), maxY = -(1 << 30);
	double best = 1e30;
	std::unordered_set<unsigned> blockerIds;
	std::vector<unsigned char> seen;   // closed cells for the frontier pass
	for (PathfindCell *c = pf.m_closedList.getHead(); c; c = c->getNextOpen())
	{
		++r.closed;
		const int cx = c->getXIndex(), cy = c->getYIndex();
		minX = std::min(minX, cx); maxX = std::max(maxX, cx); minY = std::min(minY, cy); maxY = std::max(maxY, cy);
		const double d = std::sqrt(static_cast<double>((cx - r.gx) * (cx - r.gx) + (cy - r.gy) * (cy - r.gy)));
		if (d < best) { best = d; r.nearestX = cx; r.nearestY = cy; }
		// frontier: neighbours of an expanded cell that were never reached, and why
		for (int i = 0; i < 8; ++i)
		{
			PathfindCell *n = pf.getCell(c->getLayer(), cx + dx8[i], cy + dy8[i]);
			if (!n) continue;
			if (n->hasInfo() && (n->getOpen() || n->getClosed())) continue;
			int reason = R_PASSES;
			if (!pf.validMovementPosition(s.crusher, surfaces, n, c))
			{
				switch (n->getType())
				{
					case PathfindCell::CELL_CLIFF: reason = R_TERRAIN_CLIFF; break;
					case PathfindCell::CELL_WATER: reason = R_TERRAIN_WATER; break;
					case PathfindCell::CELL_RUBBLE: reason = R_TERRAIN_RUBBLE; break;
					case PathfindCell::CELL_OBSTACLE: reason = R_TERRAIN_OBSTACLE; break;
					default: reason = R_TERRAIN_OTHER; break;
				}
			}
			else if (s.obj)
			{
				// diagnostic replica of Pathfinder::checkForMovement (reason codes only)
				const int numAbove = s.radius + (s.center ? 1 : 0);
				ObjectID ignoreId = s.obj->getAIUpdateInterface() ? s.obj->getAIUpdateInterface()->getIgnoredObstacleID() : INVALID_ID;
				bool decided = false;
				for (int a = cx + dx8[i] - s.radius; a < cx + dx8[i] + numAbove && !decided; ++a)
				{
					for (int b = cy + dy8[i] - s.radius; b < cy + dy8[i] + numAbove && !decided; ++b)
					{
						PathfindCell *fc = pf.getCell(c->getLayer(), a, b);
						if (!fc) { reason = R_OFF_MAP; decided = true; break; }
						const PathfindCell::CellFlags fl = fc->getFlags();
						if (fl == PathfindCell::NO_UNITS) continue;
						const ObjectID posUnit = fc->getPosUnit();
						if (posUnit == s.obj->getID() || posUnit == ignoreId) continue;
						if (fl != PathfindCell::UNIT_PRESENT_FIXED) continue;   // moving units are only considered with considerTransient (false for these frontiers)
						Object *unit = TheGameLogic->findObjectByID(posUnit);
						if (!unit) continue;
						if (s.obj->getRelationship(unit) == ALLIES)
						{
							if (!unit->getAIUpdateInterface()) { reason = R_ALLY_NO_AI; }
							else if (!unit->getAIUpdateInterface()->isIdle()) { reason = R_ALLY_NOT_IDLE; }
							else continue;
						}
						else
						{
							if (!s.obj->canCrushOrSquish(unit, TEST_CRUSH_OR_SQUISH)) reason = R_ENEMY_FIXED; else continue;
						}
						decided = true;
						if (blockerIds.insert(posUnit).second)
						{
							++s_blockerTemplates[unit->getTemplate()->getName().str()];
							++s_blockerRel[reason == R_ENEMY_FIXED ? "enemy" : "ally"];
							if (unit->getControllingPlayer()) ++s_blockerSides[unit->getControllingPlayer()->getSide().str()];
						}
					}
				}
			}
			++r.frontier[reason];
		}
	}
	r.blockerUnits = static_cast<unsigned>(blockerIds.size());
	r.blockerCells = r.frontier[R_ALLY_NOT_IDLE] + r.frontier[R_ALLY_NO_AI] + r.frontier[R_ENEMY_FIXED];
	if (r.closed) { r.nearestClosed = best; r.bboxW = maxX - minX + 1; r.bboxH = maxY - minY + 1; }
	// distance of the goal to the map border (cells)
	{
		const int lx = r.gx - pf.m_extent.lo.x, hx = pf.m_extent.hi.x - r.gx, ly = r.gy - pf.m_extent.lo.y, hy = pf.m_extent.hi.y - r.gy;
		r.goalToEdgeMin = static_cast<float>(std::min(std::min(lx, hx), std::min(ly, hy)));
	}
	// static (terrain-only) reachability of the goal from the start on the ground layer: separates "the terrain says no" from "units said no"
	if (s.startCell->getLayer() == LAYER_GROUND && goal->getLayer() == LAYER_GROUND)
	{
		const int w = pf.m_extent.hi.x - pf.m_extent.lo.x + 1, h = pf.m_extent.hi.y - pf.m_extent.lo.y + 1;
		if (w > 0 && h > 0)
		{
			std::vector<unsigned char> vis(static_cast<size_t>(w) * h, 0);
			std::vector<int> queue;
			auto idx = [&](int x, int y) { return static_cast<size_t>(y - pf.m_extent.lo.y) * w + (x - pf.m_extent.lo.x); };
			queue.push_back(r.sx); queue.push_back(r.sy);
			vis[idx(r.sx, r.sy)] = 1;
			bool reached = false;
			for (size_t head = 0; head < queue.size() && !reached; head += 2)
			{
				const int x = queue[head], y = queue[head + 1];
				bool open[4] = { false, false, false, false };
				for (int i = 0; i < 8; ++i)
				{
					const int nx = x + dx8[i], ny = y + dy8[i];
					if (nx < pf.m_extent.lo.x || nx > pf.m_extent.hi.x || ny < pf.m_extent.lo.y || ny > pf.m_extent.hi.y) continue;
					PathfindCell *n = pf.getCell(LAYER_GROUND, nx, ny);
					if (!n) continue;
					const bool ok = pf.validMovementPosition(s.crusher, surfaces, n, nullptr);
					if (i < 4) open[i] = ok;
					if (!ok) continue;
					if (i >= 4)
					{
						static const int adjA[4] = { 0, 1, 2, 3 }, adjB[4] = { 1, 2, 3, 0 };
						if (!open[adjA[i - 4]] && !open[adjB[i - 4]]) continue;
					}
					if (vis[idx(nx, ny)]) continue;
					vis[idx(nx, ny)] = 1;
					++r.bfsCells;
					queue.push_back(nx); queue.push_back(ny);
					if (nx == r.gx && ny == r.gy) { reached = true; break; }
				}
			}
			r.staticBfs = reached ? 1 : 0;
		}
	}
	s_records.push_back(r);
}

namespace PathFailureDiag
{
namespace
{
const char *aiStateName(int s)
{
	static const char *n[] = { "IDLE", "MOVE_TO", "FOLLOW_WAYPOINT_PATH_AS_TEAM", "FOLLOW_WAYPOINT_PATH_AS_INDIVIDUALS", "FOLLOW_WAYPOINT_PATH_AS_TEAM_EXACT", "FOLLOW_WAYPOINT_PATH_AS_INDIVIDUALS_EXACT",
		"FOLLOW_PATH", "FOLLOW_EXITPRODUCTION_PATH", "WAIT", "ATTACK_POSITION", "ATTACK_OBJECT", "FORCE_ATTACK_OBJECT", "ATTACK_AND_FOLLOW_OBJECT", "DEAD", "DOCK", "ENTER", "GUARD", "HUNT",
		"WANDER", "PANIC", "ATTACK_SQUAD", "GUARD_TUNNEL_NETWORK", "GET_REPAIRED", "MOVE_OUT_OF_THE_WAY", "MOVE_AND_TIGHTEN", "MOVE_AND_EVACUATE", "MOVE_AND_EVACUATE_AND_EXIT", "MOVE_AND_DELETE",
		"ATTACK_AREA", "HACK_INTERNET", "ATTACK_MOVE_TO", "ATTACKFOLLOW_WAYPOINT_PATH_AS_INDIVIDUALS", "ATTACKFOLLOW_WAYPOINT_PATH_AS_TEAM", "FACE_OBJECT", "FACE_POSITION", "RAPPEL_INTO",
		"COMBATDROP", "EXIT", "PICK_UP_CRATE", "MOVE_AWAY_FROM_REPULSORS", "WANDER_IN_PLACE", "BUSY" };
	return (s >= 0 && s < static_cast<int>(sizeof(n) / sizeof(n[0]))) ? n[s] : "?";
}
const char *cmdName(int c) { static const char *n[] = { "PLAYER", "SCRIPT", "AI", "DOZER", "DEFAULT_SWITCH_WEAPON" }; return (c >= 0 && c < 5) ? n[c] : "?"; }
const char *cellTypeName(int t) { static const char *n[] = { "clear", "water", "cliff", "rubble", "obstacle(structure)", "bridge-impassable", "impassable" }; return (t >= 0 && t < 7) ? n[t] : "?"; }
const char *flagName(int f) { static const char *n[] = { "no units", "unit goal", "unit moving", "unit FIXED", "?", "goal+other moving" }; return (f >= 0 && f < 6) ? n[f] : "?"; }
}

void report(FILE *out)
{
	if (!out || !enabled()) return;
	fprintf(out, "FAILING A* SEARCH CHARACTERISATION (GENERALS_SIM_PATHFAIL; diagnostic, read-only)\n");
	fprintf(out, "searches that reached the main loop: %llu | succeeded %llu | exhausted the open list %llu (%.2f ms total, mean %.3f ms) | success time %.1f ms\n",
		s_searches, s_successes, s_failures, s_failMs, s_failures ? s_failMs / static_cast<double>(s_failures) : 0.0, s_successMs);
	fprintf(out, "zone recalculations observed: %u\n", s_zoneRecalcs);
	std::vector<double> ms; double totalMs = 0;
	for (const FailRec &r : s_records) { ms.push_back(r.ms); totalMs += r.ms; }
	std::sort(ms.begin(), ms.end());
	auto pct = [&](double p) { return ms.empty() ? 0.0 : ms[std::min(ms.size() - 1, static_cast<size_t>(p * static_cast<double>(ms.size())))]; };
	fprintf(out, "failure time distribution (ms, diagnostic overhead included in the largest ones): p50 %.3f p90 %.3f p99 %.3f max %.3f\n", pct(0.5), pct(0.9), pct(0.99), ms.empty() ? 0.0 : ms.back());
	for (double th : { 1.0, 5.0, 10.0, 20.0, 33.333, 50.0 })
	{
		size_t n = 0; double t = 0;
		for (const FailRec &r : s_records) if (r.ms >= th) { ++n; t += r.ms; }
		fprintf(out, "  failures >= %6.3f ms: %4zu  (%.1f ms total = %.1f%% of all failing time)\n", th, n, t, totalMs > 0 ? 100.0 * t / totalMs : 0.0);
	}
	const double kExpensive = 5.0;
	std::vector<const FailRec *> exp;
	for (const FailRec &r : s_records) if (r.ms >= kExpensive) exp.push_back(&r);
	fprintf(out, "\nEXPENSIVE FAILURES (>= %.1f ms): %zu\n", kExpensive, exp.size());

	auto bucket = [&](const char *title, auto key) {
		std::map<std::string, std::pair<unsigned, double>> m;
		for (const FailRec *r : exp) { auto &e = m[key(*r)]; ++e.first; e.second += r->ms; }
		std::vector<std::pair<std::string, std::pair<unsigned, double>>> v(m.begin(), m.end());
		std::sort(v.begin(), v.end(), [](auto &a, auto &b) { return a.second.second > b.second.second; });
		fprintf(out, "\n%s:\n", title);
		for (size_t i = 0; i < v.size() && i < 14; ++i) fprintf(out, "  %-58s %5u  %9.1f ms\n", v[i].first.c_str(), v[i].second.first, v[i].second.second);
	};
	bucket("by hierarchical pre-search (1 found a hierarchical path, 0 fell back to all-zones-passable)", [](const FailRec &r) { return std::to_string(r.hier); });
	bucket("by requester AI state", [](const FailRec &r) { return std::string(aiStateName(r.aiState)); });
	bucket("by command source / attack path", [](const FailRec &r) { return std::string(cmdName(r.cmd)) + (r.attackPath ? " attack-path" : ""); });
	bucket("by requester template", [](const FailRec &r) { return r.tmpl; });
	bucket("by requesting team prototype", [](const FailRec &r) { return r.team; });
	bucket("by player index", [](const FailRec &r) { return std::to_string(r.player) + (r.human ? " (human rules)" : " (computer rules)"); });
	bucket("by goal cell type", [](const FailRec &r) { return std::string(cellTypeName(r.goalType)); });
	bucket("by goal cell unit flags", [](const FailRec &r) { return std::string(flagName(r.goalFlags)); });
	bucket("by goal neighbours reached by the search (of 8)", [](const FailRec &r) { return std::to_string(r.goalReachedNeighbours) + " reached, " + std::to_string(r.goalValidNeighbours) + " terrain-valid"; });
	bucket("by zone test (zone1 == zone2 always here, else the search is rejected before work)", [](const FailRec &r) { return r.zone1 == r.zone2 ? std::string("same zone") : std::string("different zones"); });
	bucket("by static terrain-only reachability (1 = terrain connects start and goal under the search's cell rules, 0 = it does not)", [](const FailRec &r) { return std::to_string(r.staticBfs); });
	bucket("by blocking-unit presence on the exhausted region's frontier", [](const FailRec &r) { return r.blockerUnits ? std::string("blocked by units") : std::string("terrain/map only"); });
	bucket("by repeat index of the same request signature (0 = first time)", [](const FailRec &r) { return std::to_string(std::min(r.repeat, 10u)); });

	// frontier totals
	unsigned long long ft[R_COUNT] = { 0 };
	for (const FailRec *r : exp) for (int i = 0; i < R_COUNT; ++i) ft[i] += r->frontier[i];
	fprintf(out, "\nFRONTIER of the exhausted region (neighbour cells of expanded cells that were never entered), summed over the expensive failures:\n");
	for (int i = 0; i < R_COUNT; ++i) fprintf(out, "  %-52s %12llu\n", reasonName(i), ft[i]);
	fprintf(out, "\nblocking units found on frontiers (distinct per failure) by template:\n");
	{
		std::vector<std::pair<std::string, unsigned>> v(s_blockerTemplates.begin(), s_blockerTemplates.end());
		std::sort(v.begin(), v.end(), [](auto &a, auto &b) { return a.second > b.second; });
		for (size_t i = 0; i < v.size() && i < 12; ++i) fprintf(out, "  %-40s %6u\n", v[i].first.c_str(), v[i].second);
		for (auto &kv : s_blockerRel) fprintf(out, "  relationship to the mover: %-10s %6u\n", kv.first.c_str(), kv.second);
		for (auto &kv : s_blockerSides) fprintf(out, "  blocker side: %-24s %6u\n", kv.first.c_str(), kv.second);
	}

	// repetition
	unsigned sigsTotal = static_cast<unsigned>(s_sigs.size()), sigsFailed = 0, sigsRepeatFail = 0, sigsAllFail = 0, sigsFlip = 0, sigsFailRepeatTopologyStable = 0;
	unsigned long long repeatFails = 0;
	for (auto &kv : s_sigs)
	{
		const SigStat &st = kv.second;
		if (st.fails) { ++sigsFailed; if (st.fails > 1) { ++sigsRepeatFail; repeatFails += st.fails - 1; } if (st.fails == st.count) ++sigsAllFail; if (st.flips) ++sigsFlip; }
	}
	for (const FailRec &r : s_records) if (r.repeat > 0 && r.zoneRecalcsSincePrev == 0) ++sigsFailRepeatTopologyStable;
	unsigned repeatsTotal = 0, repeatsNoMutation = 0; unsigned long long mutMin = ~0ULL, mutSum = 0;
	for (const FailRec &r : s_records) if (r.repeat > 0) { ++repeatsTotal; if (r.mutationsSincePrev == 0) ++repeatsNoMutation; mutSum += r.mutationsSincePrev; mutMin = std::min<unsigned long long>(mutMin, r.mutationsSincePrev); }
	unsigned expRepeat = 0; double expRepeatMs = 0;
	for (const FailRec *r : exp) if (r->repeat > 0) { ++expRepeat; expRepeatMs += r->ms; }
	fprintf(out, "\nREPETITION (signature = start cell, goal cell, locomotor surfaces, crusher, radius, centre flag, human rules, layers, ignored obstacle, tunnelling):\n");
	fprintf(out, "  distinct signatures over all searches %u | signatures that ever failed %u | failed more than once %u (%llu repeat failures) | failed every time they were asked %u | succeeded and failed alternately %u\n",
		sigsTotal, sigsFailed, sigsRepeatFail, repeatFails, sigsAllFail, sigsFlip);
	fprintf(out, "  failures that repeat an earlier signature with NO zone recalculation in between: %u of %zu\n", sigsFailRepeatTopologyStable, s_records.size());
	fprintf(out, "  repeat failures (same signature asked again): %u | with ZERO pathfinder cell-state mutations since the previous request of that signature (the only case a state-version token could prove unchanged): %u | mean mutations in between %.0f, minimum %llu\n",
		repeatsTotal, repeatsNoMutation, repeatsTotal ? static_cast<double>(mutSum) / repeatsTotal : 0.0, repeatsTotal ? mutMin : 0ULL);
	fprintf(out, "  expensive failures that repeat an earlier signature: %u of %zu (%.1f ms of %.1f ms)\n", expRepeat, exp.size(), expRepeatMs, [&]() { double t = 0; for (auto *r : exp) t += r->ms; return t; }());
	// unique start/goal/object stats among expensive failures
	std::unordered_set<unsigned long long> uSig; std::unordered_set<unsigned> uObj; std::map<std::pair<int, int>, unsigned> uGoal;
	for (const FailRec *r : exp) { uSig.insert(r->sig); uObj.insert(r->object); ++uGoal[{ r->gx / 4, r->gy / 4 }]; }
	fprintf(out, "  expensive failures: %zu | unique signatures %zu | unique requesting objects %zu | unique goal areas (4x4 cells) %zu\n", exp.size(), uSig.size(), uObj.size(), uGoal.size());
	fprintf(out, "\nTOP 25 EXPENSIVE FAILURES:\n");
	std::vector<const FailRec *> top(exp.begin(), exp.end());
	std::sort(top.begin(), top.end(), [](const FailRec *a, const FailRec *b) { return a->ms > b->ms; });
	for (size_t i = 0; i < top.size() && i < 25; ++i)
	{
		const FailRec &r = *top[i];
		fprintf(out, "  %7.2f ms frame %6u obj %6u %-22s p%d %-20s state %-22s cmd %-6s hier %d pops %6u closed %6u | start(%d,%d) goal(%d,%d) goalType %s flags %s | goal nbrs reached %d/%d valid %d | nearest explored %.1f cells | frontier: cliff %u water %u struct %u allyBusy %u enemyFixed %u offmap %u | blockers %u units | static BFS %d | repeat %u\n",
			r.ms, r.frame, r.object, r.tmpl.c_str(), r.player, r.team.c_str(), aiStateName(r.aiState), cmdName(r.cmd), r.hier, r.pops, r.closed, r.sx, r.sy, r.gx, r.gy,
			cellTypeName(r.goalType), flagName(r.goalFlags), r.goalReachedNeighbours, r.goalNeighbours, r.goalValidNeighbours, r.nearestClosed,
			r.frontier[R_TERRAIN_CLIFF], r.frontier[R_TERRAIN_WATER], r.frontier[R_TERRAIN_OBSTACLE], r.frontier[R_ALLY_NOT_IDLE], r.frontier[R_ENEMY_FIXED], r.frontier[R_OFF_MAP], r.blockerUnits, r.staticBfs, r.repeat);
	}
}

void writeCsv(FILE *out)
{
	if (!out || !enabled()) return;
	fprintf(out, "frame,object,template,player,team,human,aistate,cmd,attackPath,hier,surfaces,crusher,radius,center,layer,destLayer,fromX,fromY,rawToX,rawToY,startX,startY,goalX,goalY,zone1,zone2,pops,cellsAllocated,closed,ms,sig,repeat,prevFrame,zoneRecalcsSincePrev,goalType,goalFlags,goalPinched,goalObstacle,goalNeighbours,goalValidNeighbours,goalReachedNeighbours,nearestClosedCells,bboxW,bboxH,fCliff,fWater,fRubble,fStruct,fOther,fOffMap,fAllyNoAI,fAllyBusy,fEnemyFixed,fPasses,blockerUnits,staticBfs,bfsCells,goalToEdge\n");
	for (const FailRec &r : s_records)
	{
		fprintf(out, "%u,%u,%s,%d,%s,%d,%s,%s,%d,%d,%u,%d,%d,%d,%d,%d,%.1f,%.1f,%.1f,%.1f,%d,%d,%d,%d,%d,%d,%u,%u,%u,%.4f,%016llX,%u,%u,%u,%s,%s,%d,%s,%d,%d,%d,%.2f,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%.0f\n",
			r.frame, r.object, r.tmpl.c_str(), r.player, r.team.c_str(), r.human, aiStateName(r.aiState), cmdName(r.cmd), r.attackPath, r.hier, r.surfaces, r.crusher, r.radius, r.center, r.layer, r.destLayer,
			r.fx, r.fy, r.rx, r.ry, r.sx, r.sy, r.gx, r.gy, r.zone1, r.zone2, r.pops, r.cells, r.closed, r.ms, r.sig, r.repeat, r.sigPrevFrame, r.zoneRecalcsSincePrev,
			cellTypeName(r.goalType), flagName(r.goalFlags), r.goalPinched, r.goalObstacle.c_str(), r.goalNeighbours, r.goalValidNeighbours, r.goalReachedNeighbours, r.nearestClosed, r.bboxW, r.bboxH,
			r.frontier[R_TERRAIN_CLIFF], r.frontier[R_TERRAIN_WATER], r.frontier[R_TERRAIN_RUBBLE], r.frontier[R_TERRAIN_OBSTACLE], r.frontier[R_TERRAIN_OTHER], r.frontier[R_OFF_MAP],
			r.frontier[R_ALLY_NO_AI], r.frontier[R_ALLY_NOT_IDLE], r.frontier[R_ENEMY_FIXED], r.frontier[R_PASSES], r.blockerUnits, r.staticBfs, r.bfsCells, r.goalToEdgeMin);
	}
}
}
