// SPDX-License-Identifier: GPL-3.0-or-later
// Enhanced gameplay HUD: the pure models behind the General rank milestones, the General Powers popup and the Battle Overview.
// No engine types: everything here is testable arithmetic / data classification. Presentation only - no gameplay state is
// owned, written or derived beyond reading numbers the retail UI already shows.
#pragma once

#include "Common/EnhancedHudLayout.h"

#include <cmath>
#include <cstdio>
#include <cwchar>
#include <cstring>

namespace EnhancedHudPanels
{

using EnhancedHudLayout::Rect;

// ---- General rank milestones (the four cells of the General module) -------------------------------------------------------
//
// Audit result (what the retail data model really offers): there are no four "categories" of General Powers - the power
// buttons are grouped by the rank at which they open (GeneralsExpPoints.wnd sections for rank 1, 3 and 8) and every power is
// one purchase of the same kind. What the model does expose for the local player is: the rank level, the skill points, the
// per-level skill points needed (RankInfo), the unspent science purchase points. The four cells therefore show the General's
// PROMOTION MILESTONES: the rank ladder from rank 2 to the top is divided into four equal steps; each cell is one step.
//   Earned  - the General has reached that step (gold star)
//   Next    - the first step not yet reached: carries the real XP progress toward it (fill)
//   Locked  - later steps (dim star)
// Unspent General Points are shown as lit pips on the General Powers entry (see pointPips).
enum class Milestone { Earned, Next, Locked };

// Number of cells for a rank table with `levels` levels (level 1 has no promotion): at most four.
inline int milestoneCount(int levels) { return levels < 2 ? 0 : (levels - 1 < 4 ? levels - 1 : 4); }

// Rank that completes milestone `slot` (0-based) of `count` for a table with `levels` levels. Monotonic, ends at `levels`.
inline int milestoneRank(int levels, int slot, int count)
{
	if (count <= 0 || levels < 2) return 1;
	const int span = levels - 1;
	return 1 + ((slot + 1) * span + count - 1) / count;      // ceil((slot + 1) * span / count)
}

inline Milestone milestoneState(int rank, int levels, int slot, int count)
{
	if (rank >= milestoneRank(levels, slot, count)) return Milestone::Earned;
	if (slot == 0 || rank >= milestoneRank(levels, slot - 1, count)) return Milestone::Next;
	return Milestone::Locked;
}

// Real XP progress (permille, 0..1000) toward milestone `slot`: skill points between the previous milestone's requirement and
// this one's. `needed(level)` is the cumulative skill points the retail rank table requires for that level.
template <class Needed>
int milestoneProgressPermille(int skillPoints, int levels, int slot, int count, Needed &&needed)
{
	const int target = milestoneRank(levels, slot, count);
	const int from = slot == 0 ? 1 : milestoneRank(levels, slot - 1, count);
	const int lo = needed(from), hi = needed(target);
	if (hi <= lo) return 0;
	if (skillPoints <= lo) return 0;
	if (skillPoints >= hi) return 1000;
	return static_cast<int>(static_cast<long long>(skillPoints - lo) * 1000 / (hi - lo));
}

// Unspent General Points shown as lit pips (capped): `shown` of `slots`; more points than pips saturate.
inline int pointPips(int points, int slots) { return points <= 0 ? 0 : (points < slots ? points : slots); }

// ---- General Powers popup (GeneralsExpPoints.wnd) --------------------------------------------------------------------------
// State of a power button, from the retail window flags the ControlBar already sets (ControlBar::updateContextPurchaseScience):
//   enabled                          -> Available   (purchasable now: enough points, prerequisites met)
//   disabled + ALWAYS_COLOR          -> Purchased   (player->hasScience)
//   disabled                         -> Locked      (not enough points / prerequisites / script disabled)
enum class PowerState { Available, Purchased, Locked };
inline PowerState powerState(bool enabled, bool alwaysColor)
{
	if (enabled) return PowerState::Available;
	return alwaysColor ? PowerState::Purchased : PowerState::Locked;
}

// Retail layout of the popup relative to its parent window (GeneralsExpPoints.wnd at 800 x 600, parent 296 x 432). The
// retail windows are one tall column: header, 1-star row, 3-star grid, 5-star row, exit.
constexpr int kPopupW = 296, kPopupH = 432;
constexpr Rect kPopupHeader = {0, 0, 296, 76};          // title, level, experience bar, remaining points
constexpr Rect kPopupRank1 = {0, 76, 296, 84};          // "1 star" header text + three powers
constexpr Rect kPopupRank3 = {0, 160, 296, 188};        // "3 star" header text + twelve powers (4 columns x 3 rows)
constexpr Rect kPopupRank8 = {0, 348, 296, 84};         // "5 star" header text + one power + the exit button (same row)
inline Rect popupScaled(const Rect &r, double scale)
{
	return {static_cast<int>(std::lround(r.x * scale)), static_cast<int>(std::lround(r.y * scale)),
	        static_cast<int>(std::lround(r.w * scale)), static_cast<int>(std::lround(r.h * scale))};
}

// Enhanced re-layout of the SAME windows (reference units, parent 536 x 300): a compact console. The header carries rank,
// experience and General Points; the three rank tiers stay obvious - 1 star (top left), 5 star (bottom left, with Done) and the
// 3 star grid on the right (its 4 columns x 3 rows are science chains: the columns are kept exactly as in the retail layout, so
// no power moves relative to the others in its chain).
constexpr int kPopupNewW = 536, kPopupNewH = 300;
constexpr Rect kPopupNewHeader = {12, 10, 512, 76};
constexpr Rect kPopupNewTier1 = {12, 98, 216, 94};
constexpr Rect kPopupNewTier5 = {12, 204, 216, 84};
constexpr Rect kPopupNewTier3 = {240, 98, 284, 190};

struct PopupTarget { bool valid; Rect rect; };

inline bool popupNear(int a, int b) { return a - b <= 1 && b - a <= 1; }
inline bool popupSame(const Rect &r, int x, int y, int w, int h)
{
	return popupNear(r.x, x) && popupNear(r.y, y) && popupNear(r.w, w) && popupNear(r.h, h);
}

// Enhanced target (reference units, relative to the new parent) of a retail window given its retail rectangle relative to the
// retail parent. Windows that are not part of the retail GeneralsExpPoints.wnd layout are reported invalid and left alone.
inline PopupTarget popupTarget(const Rect &r)
{
	struct Entry { int rx, ry, rw, rh; Rect to; };
	static const Entry table[] = {
		{0, 0, 296, 34, kPopupNewHeader},                    // header decoration window
		{8, 6, 160, 26, {20, 14, 160, 26}},                  // StaticTextTitle (rank name)
		{176, 6, 113, 26, {186, 14, 113, 26}},               // StaticTextLevel
		{8, 38, 160, 16, {20, 46, 160, 16}},                 // "Experience"
		{8, 54, 167, 18, {20, 64, 300, 18}},                 // experience bar housing
		{11, 59, 161, 8, {23, 69, 294, 8}},                  // ProgressBarExperience
		{174, 36, 84, 36, {404, 14, 84, 36}},                // "Remaining skill points"
		{262, 40, 23, 31, {494, 18, 23, 31}},                // StaticTextRankPointsAvailable
		{0, 76, 296, 24, {16, 102, 208, 24}},                // 1 star header text
		{0, 160, 296, 24, {244, 102, 276, 24}},              // 3 star header text
		{0, 348, 296, 24, {16, 208, 208, 24}},               // 5 star header text
		{198, 392, 88, 32, {124, 240, 88, 32}},              // ButtonExit
	};
	for (const Entry &e : table)
		if (popupSame(r, e.rx, e.ry, e.rw, e.rh))
			return {true, e.to};
	if (popupNear(r.w, 56) && popupNear(r.h, 48))            // a power button
	{
		if (r.y < 160) return {true, {r.x + 8, r.y + 28, r.w, r.h}};            // 1 star row
		if (r.y < 348) return {true, {r.x + 236, r.y - 56, r.w, r.h}};          // 3 star grid
		return {true, {r.x + 8, r.y - 138, r.w, r.h}};                          // 5 star
	}
	return {false, {}};
}

// Where the popup goes and how large it is drawn: the reference size times the UI scale, reduced so it always fits the
// surface; centred in the battlefield above the console (or in the surface when there is no room above it).
struct PopupPlacement { Rect rect; double scale; };
inline PopupPlacement popupPlacement(int surfaceW, int surfaceH, int consoleTop, double uiScale)
{
	double f = uiScale > 0.0 ? uiScale : 1.0;
	f = std::min(f, 0.95 * surfaceW / kPopupNewW);
	f = std::min(f, 0.90 * surfaceH / kPopupNewH);
	if (f < 0.25) f = 0.25;
	const int w = static_cast<int>(std::lround(kPopupNewW * f)), h = static_cast<int>(std::lround(kPopupNewH * f));
	const int x = (surfaceW - w) / 2;
	int y = (consoleTop - h) / 2;
	if (y < 0) y = std::max(0, (surfaceH - h) / 2);
	if (y + h > surfaceH) y = std::max(0, surfaceH - h);
	return {{x, y, w, h}, f};
}

// ---- Power meter (ControlBar PowerWindow) ------------------------------------------------------------------------------------
// The meter is permanent console instrumentation: a recessed track of kPowerSegments segments is always drawn, whatever the
// power state; the retail gauge (real production / consumption, green / yellow / red from the game's own state) fills it.
constexpr int kPowerSegments = 20;
inline int powerSegmentX(int index, int width) { return width * index / kPowerSegments; }

// ---- Battle Overview (Diplomacy.wnd) ----------------------------------------------------------------------------------------
//
// Data audit. Live and safe (all of it is already shown by the retail player list or is shared by every player):
//   player name, side/faction, team, human/CPU (AI slot), alive / defeated / observer / disconnected status, mute state,
//   elapsed game time (logic frame / frames per second), map name.
// Not shown, with the reason:
//   army value, unit and building counts, income, money, resources gathered   - hidden information of other players (fog of
//                                                                               war); the local player's own values already
//                                                                               have a HUD home (cash panel)
//   kills / losses / buildings destroyed / efficiency                          - ScoreKeeper data is post-game; live exposure
//                                                                               would change what players know
//   ping / connection quality                                                  - NetworkMetrics are not per-slot available
//                                                                               to the UI in every game mode
// A tab is only built for data that is genuinely available; the others stay out of the UI until their data exists.
enum class OverviewTab { Players, Army, Economy, Combat };
struct OverviewTabInfo
{
	OverviewTab tab;
	const char *label;
	bool available;          // true only when the data exists and is safe to show live
	const char *reason;      // why it is not available
};
inline const OverviewTabInfo *overviewTabs(int &count)
{
	static const OverviewTabInfo table[] = {
		{OverviewTab::Players, "PLAYERS", true, ""},
		{OverviewTab::Army, "ARMY", false, "unit counts / army value of other players are hidden information"},
		{OverviewTab::Economy, "ECONOMY", false, "income and resources of other players are hidden information"},
		{OverviewTab::Combat, "COMBAT", false, "kills and losses are post-game statistics"},
	};
	count = static_cast<int>(sizeof(table) / sizeof(table[0]));
	return table;
}
inline int availableOverviewTabs()
{
	int n = 0, k = 0;
	for (const OverviewTabInfo *t = overviewTabs(n); n-- > 0; ++t)
		if (t->available) ++k;
	return k;
}

// hh:mm:ss from logic frames (no negative, hours grow past 99 without clipping).
inline int formatGameTime(long long frames, int framesPerSecond, wchar_t *out, int capacity)
{
	if (!out || capacity < 9 || framesPerSecond <= 0) return 0;
	if (frames < 0) frames = 0;
	const long long total = frames / framesPerSecond;
	const long long h = total / 3600, m = (total / 60) % 60, s = total % 60;
	return std::swprintf(out, static_cast<size_t>(capacity), L"%02lld:%02lld:%02lld", h, m, s);
}

// Display name of a map from its path ("Maps\\Tournament Desert\\Tournament Desert.map" -> "Tournament Desert").
inline int mapDisplayName(const char *path, wchar_t *out, int capacity)
{
	if (!out || capacity < 2) return 0;
	out[0] = 0;
	if (!path) return 0;
	const char *leaf = path;
	for (const char *p = path; *p; ++p)
		if (*p == '\\' || *p == '/') leaf = p + 1;
	int n = 0;
	for (const char *p = leaf; *p && n < capacity - 1; ++p)
	{
		if (*p == '.') break;
		out[n++] = static_cast<wchar_t>(static_cast<unsigned char>(*p));
	}
	out[n] = 0;
	return n;
}

// Retail layout of the Overview relative to its parent window (Diplomacy.wnd, parent 424 x 397).
constexpr int kOverviewW = 424, kOverviewH = 397;
constexpr Rect kOverviewHeader = {0, 0, 424, 34};        // title / map / time band (free in the retail layout)
constexpr Rect kOverviewTable = {4, 30, 416, 330};       // column titles + 8 player rows (InGameParent area)
constexpr int kOverviewRowY0 = 94 - 4, kOverviewRowPitch = 32, kOverviewRows = 8;   // rows relative to the parent origin
constexpr Rect kOverviewFooter = {0, 362, 424, 35};      // Done / hide button

}  // namespace EnhancedHudPanels
