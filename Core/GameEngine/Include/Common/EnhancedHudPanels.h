// SPDX-License-Identifier: GPL-3.0-or-later
// Enhanced gameplay HUD: the pure models behind the General rank milestones, the General Powers popup and the Battle Overview.
// No engine types: everything here is testable arithmetic / data classification. Presentation only - no gameplay state is
// owned, written or derived beyond reading numbers the retail UI already shows.
#pragma once

#include "Common/EnhancedHudLayout.h"

#include <cmath>
#include <cstdlib>
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
enum class PowerState { Available, Purchased, Locked, NeedsPoints };
inline PowerState powerState(bool enabled, bool alwaysColor)
{
	if (enabled) return PowerState::Available;
	return alwaysColor ? PowerState::Purchased : PowerState::Locked;
}
// The same, with the one extra fact the retail enable flag folds together: a disabled, not owned power whose rank AND chain
// prerequisites are met is not locked - it is waiting for a General Point (player->hasScience / ScienceStore prerequisites, both
// read-only). Locked therefore always means "rank or an upgrade step is missing".
inline PowerState powerStateDetailed(bool enabled, bool owned, bool prerequisitesMet)
{
	if (owned) return PowerState::Purchased;
	if (enabled) return PowerState::Available;
	return prerequisitesMet ? PowerState::NeedsPoints : PowerState::Locked;
}

// Upgrade connectors. `isPrerequisite(a, b)` is the REAL relation (science a is a prerequisite of science b, from
// ScienceStore::getPrerequisiteSciences). A connector joins a to b only when that relation holds AND b is the plate directly
// below a in the same column (nothing between them). Unrelated powers are never joined, however they are arranged.
struct Connector { int from, to; };
template <class IsPrerequisite>
int chainConnectors(const Rect *rects, int count, IsPrerequisite &&isPrerequisite, Connector *out, int capacity)
{
	int n = 0;
	for (int a = 0; a < count; ++a)
		for (int b = 0; b < count; ++b)
		{
			if (a == b || !isPrerequisite(a, b)) continue;
			if (std::abs(rects[a].x - rects[b].x) > 2 || rects[b].y < rects[a].bottom()) continue;
			bool between = false;
			for (int m = 0; m < count && !between; ++m)
				if (m != a && m != b && std::abs(rects[m].x - rects[a].x) <= 2 && rects[m].y >= rects[a].bottom() && rects[m].bottom() <= rects[b].y)
					between = true;
			if (between || n >= capacity) continue;
			out[n++] = {a, b};
		}
	return n;
}
// Depth of a power in its chain (1 = first step): 1 + the deepest prerequisite among the shown powers.
template <class IsPrerequisite>
int chainDepth(int index, int count, IsPrerequisite &&isPrerequisite, int guard = 8)
{
	if (guard <= 0) return 1;
	int depth = 1;
	for (int a = 0; a < count; ++a)
		if (a != index && isPrerequisite(a, index))
			depth = std::max(depth, 1 + chainDepth(a, count, isPrerequisite, guard - 1));
	return depth;
}
// True when the power belongs to a chain (it has a prerequisite or a successor among the shown powers).
template <class IsPrerequisite>
bool inChain(int index, int count, IsPrerequisite &&isPrerequisite)
{
	for (int a = 0; a < count; ++a)
		if (a != index && (isPrerequisite(a, index) || isPrerequisite(index, a))) return true;
	return false;
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

// ---- visual state vocabulary (resting / hover / pressed are interaction; available / purchased / locked / needs points are data) ----
// A blue rim is never decoration: it appears only for hover/pressed (Active) or for an available action. Resting controls and
// status modules carry a neutral or muted edge, so nothing looks permanently keyboard-focused or selected.
enum class Interaction { Resting, Hover, Pressed };
inline Interaction interactionOf(bool hover, bool down) { return down ? Interaction::Pressed : (hover ? Interaction::Hover : Interaction::Resting); }
enum class Outline { None, Neutral, Muted, Active, Gold };
// General Powers entry button: no outline at rest (its own art carries the identity); blue only while the pointer is on it.
inline Outline generalButtonOutline(Interaction i) { return i == Interaction::Resting ? Outline::None : Outline::Active; }
// Unspent General Points are announced by a status lamp (and the pips), never by an outline around the button.
inline bool generalNotificationLamp(int points) { return points > 0; }
// Outline of a power plate in the General Powers console.
inline Outline powerOutline(PowerState state, Interaction i)
{
	switch (state)
	{
		case PowerState::Purchased: return Outline::Gold;
		case PowerState::Available: return i == Interaction::Resting ? Outline::Muted : Outline::Active;
		case PowerState::NeedsPoints: return i == Interaction::Resting ? Outline::Neutral : Outline::Muted;
		default: return Outline::None;
	}
}
// The General Points readout is an instrument: it never has a full outline; emphasis is a rail (points > 0) or none.
inline bool pointsReadoutRail(int points) { return points > 0; }

// Enhanced re-layout of the SAME windows (reference units, parent 440 x 500): a promotion / upgrade console, stacked in rank
// order. Geometry follows the real content (audit of Science.ini / CommandSet.ini):
//   1 star : 3 powers, none chained           3 star: Pathfinder + three chains (Paradrop, A-10, Repair, three steps each)
//   5 star : 1 power (Daisy Cutter)           every power costs one General Point; rank 1 / 3 / 5 opens a tier
// The 3 star grid keeps its four columns (a column is a chain); the steps of a chain are spaced so the connector between them
// is readable. The retail tier header texts, "Level" and the points label/count are replaced by measured Enhanced readouts drawn
// from the same real values (the retail windows are hidden and restored for Original).
constexpr int kPopupNewW = 440, kPopupNewH = 508;
constexpr Rect kPopupNewHeader = {8, 8, 424, 88};
constexpr Rect kPopupNewTier1 = {8, 102, 424, 86};
constexpr Rect kPopupNewTier3 = {8, 194, 424, 212};
constexpr Rect kPopupNewTier5 = {8, 412, 424, 88};
// rank level at which each tier opens: Rank.ini grants SCIENCE_Rank1 / 3 / 5 at ranks 1 / 3 / 5 and the powers of the tier require
// that rank science (the command sets are legacy-named Rank1 / Rank3 / Rank8). Capped by the rank table at run time.
constexpr int kPopupTierRank[3] = {1, 3, 5};
constexpr int kPopupTierStars[3] = {1, 3, 5};
// 3 star grid: 4 columns x 3 rows; column pitch and row pitch in reference units
constexpr int kPopupGridX0 = 33, kPopupGridPitchX = 106, kPopupGridY0 = 226, kPopupGridPitchY = 62;
inline int popupRequiredRank(int tier, int rankLevels) { return tier < 0 || tier > 2 ? 1 : (kPopupTierRank[tier] < rankLevels ? kPopupTierRank[tier] : rankLevels); }

struct PopupTarget { bool valid; Rect rect; bool hide; };

inline bool popupNear(int a, int b) { return a - b <= 1 && b - a <= 1; }
inline bool popupSame(const Rect &r, int x, int y, int w, int h)
{
	return popupNear(r.x, x) && popupNear(r.y, y) && popupNear(r.w, w) && popupNear(r.h, h);
}
inline int popupClamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Enhanced target (reference units, relative to the new parent) of a retail window given its retail rectangle relative to the
// retail parent. Windows that are not part of the retail GeneralsExpPoints.wnd layout are reported invalid and left alone.
inline PopupTarget popupTarget(const Rect &r)
{
	struct Entry { int rx, ry, rw, rh; Rect to; bool hide; };
	static const Entry table[] = {
		{0, 0, 296, 34, kPopupNewHeader, false},                   // header decoration window
		{8, 6, 160, 26, {18, 14, 190, 26}, false},                 // StaticTextTitle (rank name)
		{176, 6, 113, 26, {216, 14, 100, 26}, true},               // StaticTextLevel (secondary: drawn as "RANK n / N")
		{8, 38, 160, 16, {18, 52, 66, 16}, false},                 // "Experience"
		{8, 54, 167, 18, {88, 50, 170, 18}, false},                // experience bar housing
		{11, 59, 161, 8, {91, 55, 164, 8}, false},                 // ProgressBarExperience
		{174, 36, 84, 36, {330, 14, 90, 34}, true},                // "Remaining skill points" (drawn as GENERAL POINTS)
		{262, 40, 23, 31, {370, 48, 23, 31}, true},                // StaticTextRankPointsAvailable (drawn large)
		{0, 76, 296, 24, {14, 106, 412, 24}, true},                // 1 star header text (replaced by the tier label)
		{0, 160, 296, 24, {14, 198, 412, 24}, true},               // 3 star header text
		{0, 348, 296, 24, {14, 416, 412, 24}, true},               // 5 star header text
		{198, 392, 88, 32, {332, 452, 88, 32}, false},             // ButtonExit
	};
	for (const Entry &e : table)
		if (popupSame(r, e.rx, e.ry, e.rw, e.rh))
			return {true, e.to, e.hide};
	if (popupNear(r.w, 56) && popupNear(r.h, 48))            // a power button
	{
		if (r.y < 160)                                       // 1 star: three in a row, centred on the 4 column grid
			return {true, {86 + 106 * popupClamp((r.x - 16 + 34) / 68, 0, 2), 132, r.w, r.h}, false};
		if (r.y < 348)                                       // 3 star: column c, row k of the retail grid
		{
			const int col = popupClamp((r.x - 16 + 34) / 68, 0, 3), row = popupClamp((r.y - 188 + 26) / 52, 0, 2);
			return {true, {kPopupGridX0 + kPopupGridPitchX * col, kPopupGridY0 + kPopupGridPitchY * row, r.w, r.h}, false};
		}
		return {true, {kPopupGridX0, 444, r.w, r.h}, false};                     // 5 star
	}
	return {false, {}, false};
}

// Where the popup goes and how large it is drawn: the reference size times the UI scale, reduced so it always fits the
// surface; centred in the battlefield above the console (or in the surface when there is no room above it).
struct PopupPlacement { Rect rect; double scale; };
inline PopupPlacement popupPlacement(int surfaceW, int surfaceH, int consoleTop, double uiScale)
{
	double f = uiScale > 0.0 ? uiScale : 1.0;
	f = std::min(f, 0.90 * surfaceW / kPopupNewW);
	f = std::min(f, 0.80 * surfaceH / kPopupNewH);
	if (f < 0.25) f = 0.25;
	const int w = static_cast<int>(std::lround(kPopupNewW * f)), h = static_cast<int>(std::lround(kPopupNewH * f));
	const int x = (surfaceW - w) / 2;
	int y = (consoleTop - h) / 2;
	if (y < 0) y = std::max(0, (surfaceH - h) / 2);
	if (y + h > surfaceH) y = std::max(0, surfaceH - h);
	return {{x, y, w, h}, f};
}

// Where the retail hover tooltip (ControlBarPopupDescription) is placed. The tooltip window is an ENABLED window on top of the
// others: whatever it covers loses the mouse. Over a General Powers button that is a loop - the tooltip appears, takes the mouse
// from the button, the button's tooltip callback stops running, the layout hides itself, the button gets the mouse back, the
// delay elapses, the tooltip appears again - which is the visible flicker. The tooltip therefore must never cover the popup.
// Default (no popup): above the island, centred on the command frame. With the popup open: beside it (right, then left), else
// between the popup and the island, else above the popup, else inside the island strip; the first spot that clears the popup.
inline int rectOverlapArea(const Rect &a, const Rect &b)
{
	const int w = std::min(a.right(), b.right()) - std::max(a.x, b.x), h = std::min(a.bottom(), b.bottom()) - std::max(a.y, b.y);
	return w > 0 && h > 0 ? w * h : 0;
}
inline Rect tooltipPlacement(int surfaceW, int surfaceH, const Rect &island, const Rect &commandFrame, int w, int h, const Rect &popup,
                             int cursorX = -1, int cursorY = -1)
{
	const int gap = 4;
	auto clampX = [&](int x) { return std::max(0, std::min(x, surfaceW - w)); };
	auto clampY = [&](int y) { return std::max(0, std::min(y, surfaceH - h)); };
	const Rect standard = {clampX(commandFrame.x + (commandFrame.w - w) / 2), clampY(island.y - h - gap), w, h};
	if (popup.w <= 0 || popup.h <= 0 || rectOverlapArea(standard, popup) == 0)
		return standard;
	const Rect candidates[5] = {
		{popup.right() + gap, clampY(std::min(standard.y, popup.bottom() - h)), w, h},                    // right of the popup
		{popup.x - gap - w, clampY(std::min(standard.y, popup.bottom() - h)), w, h},                      // left of the popup
		{standard.x, popup.bottom() + gap, w, h},                                                          // between popup and island
		{clampX(popup.x + (popup.w - w) / 2), popup.y - gap - h, w, h},                                    // above the popup
		{standard.x, clampY(std::max(island.y, surfaceH - h)), w, h},                                      // inside the island strip
	};
	for (const Rect &c : candidates)
		if (c.x >= 0 && c.y >= 0 && c.right() <= surfaceW && c.bottom() <= surfaceH && rectOverlapArea(c, popup) == 0)
			return c;
	// No clear spot (compact surfaces, tall tooltips): the tooltip may overlap the console but must never cover the pointer, or
	// the pointer would leave the button it is on and the flicker would return. Beside the pointer, first spot inside the surface.
	if (cursorX >= 0 && cursorY >= 0)
	{
		const int m = 28;                                                                                   // pointer + hot spot margin
		const Rect byCursor[4] = {
			{cursorX + m, clampY(cursorY - h / 2), w, h},                                                  // right
			{cursorX - m - w, clampY(cursorY - h / 2), w, h},                                              // left
			{clampX(cursorX - w / 2), cursorY + m, w, h},                                                  // below
			{clampX(cursorX - w / 2), cursorY - m - h, w, h},                                              // above
		};
		for (const Rect &c : byCursor)
			if (c.x >= 0 && c.y >= 0 && c.right() <= surfaceW && c.bottom() <= surfaceH && !(cursorX >= c.x - 2 && cursorX <= c.right() + 2 && cursorY >= c.y - 2 && cursorY <= c.bottom() + 2))
				return c;
	}
	return standard;
}

// Largest point size <= base (never below minSize) at which text of width `widthAtBase` (measured at `base`) fits `maxWidth`.
// Used by the label drawing so a label can never cross its module.
inline int fitPointSize(int base, int minSize, int widthAtBase, int maxWidth)
{
	if (widthAtBase <= 0 || widthAtBase <= maxWidth || maxWidth <= 0) return base;
	const int size = static_cast<int>(static_cast<long long>(base) * maxWidth / widthAtBase);
	return size < minSize ? minSize : size;
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
