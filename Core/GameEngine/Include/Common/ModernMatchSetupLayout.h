// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Frontend screen kit, part 2 (pure, engine-free): the Match Setup composition  MAP | PLAYERS | MATCH RULES  plus the Back / Start action row.
//
//   * MAP       the map preview (with the start positions drawn over it by the retail code), the map name and the Select Map action;
//   * PLAYERS   eight rows of Player | Color | Faction | Team under column heads;
//   * RULES     one card per rule (caption above its control), the cards come from a SPEC LIST: the layout takes only the number of rules, so a new rule (a synthetic one in the tests, a
//               future game option) needs no bespoke geometry - it is one more card; below the cards the career record of the local player.
//
// The composition sits inside the centred 16:9 box (ultrawide keeps the shell map visible on both sides, nothing is stretched); the UI scale is reduced - never clipped - until the three zones
// fit the box and the surface height. Units: reference units * (effective scale / 100). Rectangles are absolute surface pixels.
#pragma once

#include "Common/ModernScreenLayout.h"

namespace ModernMatchSetup
{
using ModernScreen::Rect;

constexpr int kSlots = 8;
constexpr int kColumns = 4;                // Player | Color | Faction | Team
constexpr int kMaxRules = 14;
constexpr int kHonorRows = 4;              // wins, losses, current streak, best streak

struct Input
{
	int surfaceW = 0, surfaceH = 0, uiPercent = 100;
	int ruleCount = 0;
	bool honors = true;                    // the career record block
};

struct Layout
{
	Rect composition, panel;
	Rect title, titleRule;
	Rect mapZone, playersZone, rulesZone;                       // the zone surfaces (decor panels)
	Rect mapHeading, playersHeading, rulesHeading;              // section heading text
	Rect mapPreview, mapName, selectMap;
	Rect columnHead[kColumns];
	Rect cell[kSlots][kColumns];
	Rect ruleLabel[kMaxRules], ruleControl[kMaxRules];
	int ruleCount = 0;                                          // rules that got a card (<= kMaxRules)
	Rect honorLabel[kHonorRows], honorValue[kHonorRows];
	bool hasHonors = false;
	Rect status, back, start;
	int effectiveScale = 100;
	bool compact = false;
};

namespace detail
{
inline int sc(int ref, int percent) { return (ref * percent + 50) / 100; }
}

inline Layout compute(const Input &in)
{
	using namespace detail;
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	Layout L;
	if (in.surfaceW <= 0 || in.surfaceH <= 0) return L;
	int scale = in.uiPercent > 0 ? in.uiPercent : 100;
	int compW = static_cast<int>(in.surfaceH * ModernTheme::kCompositionMaxAspect + 0.5);
	if (compW > in.surfaceW) compW = in.surfaceW;
	L.composition = Rect{ (in.surfaceW - compW) / 2, 0, compW, in.surfaceH };
	const int rules = in.ruleCount < 0 ? 0 : (in.ruleCount > kMaxRules ? kMaxRules : in.ruleCount);

	for (int attempt = 0; attempt < 80; ++attempt)
	{
		const int pad = sc(24, scale), gap = sc(16, scale), edge = sc(m.edgeMargin, scale), inner = sc(14, scale);
		const int titleH = sc(34, scale), ruleH = sc(m.ruleThickness, scale) > 0 ? sc(m.ruleThickness, scale) : 1;
		const int headH = sc(26, scale), rowH = sc(34, scale), rowGap = sc(6, scale);
		const int actionH = sc(m.actionHeight, scale), statusH = sc(22, scale);
		const int cardLabelH = sc(20, scale), cardControlH = sc(32, scale), cardGap = sc(14, scale);
		const int honorH = sc(22, scale);

		const int mapW = sc(300, scale), playersW = sc(580, scale), rulesW = sc(320, scale);
		const int panelW = 2 * pad + mapW + playersW + rulesW + 2 * gap;

		// zone heights
		const int previewH = mapW * 5 / 6 - 2 * inner * 0;
		const int mapNeed = inner + headH + gap / 2 + previewH + gap / 2 + sc(28, scale) + gap / 2 + sc(38, scale) + inner;
		const int playersNeed = inner + headH + gap / 2 + sc(24, scale) + rowGap + kSlots * (rowH + rowGap) + inner;
		const int honorsNeed = in.honors ? gap + kHonorRows * honorH + sc(8, scale) : 0;
		const int rulesNeed = inner + headH + gap / 2 + (rules ? rules * (cardLabelH + cardControlH + cardGap) : 0) + honorsNeed + inner;
		int zoneH = mapNeed;
		if (playersNeed > zoneH) zoneH = playersNeed;
		if (rulesNeed > zoneH) zoneH = rulesNeed;
		const int panelH = pad + titleH + gap / 2 + ruleH + gap + zoneH + gap + statusH + sc(4, scale) + actionH + pad;

		const bool fits = panelW <= compW - 2 * edge && panelH <= in.surfaceH - 2 * edge;
		if (!fits && scale > 30) { scale = scale * 94 / 100; continue; }

		L.panel = Rect{ L.composition.x + (compW - panelW) / 2, (in.surfaceH - panelH) / 2, panelW, panelH };
		const Rect &P = L.panel;
		L.title = Rect{ P.x + pad, P.y + pad, P.w - 2 * pad, titleH };
		L.titleRule = Rect{ L.title.x, L.title.bottom() + gap / 2, L.title.w, ruleH };
		const int zoneTop = L.titleRule.bottom() + gap;
		L.mapZone = Rect{ P.x + pad, zoneTop, mapW, zoneH };
		L.playersZone = Rect{ L.mapZone.right() + gap, zoneTop, playersW, zoneH };
		L.rulesZone = Rect{ L.playersZone.right() + gap, zoneTop, rulesW, zoneH };

		// map
		L.mapHeading = Rect{ L.mapZone.x + inner, L.mapZone.y + inner, mapW - 2 * inner, headH };
		const int prevW = mapW - 2 * inner;
		const int prevH = prevW * 5 / 6;
		L.mapPreview = Rect{ L.mapZone.x + inner, L.mapHeading.bottom() + gap / 2, prevW, prevH };
		L.mapName = Rect{ L.mapZone.x + inner, L.mapPreview.bottom() + gap / 2, prevW, sc(28, scale) };
		L.selectMap = Rect{ L.mapZone.x + inner, L.mapName.bottom() + gap / 2, prevW, sc(38, scale) };

		// players
		L.playersHeading = Rect{ L.playersZone.x + inner, L.playersZone.y + inner, playersW - 2 * inner, headH };
		const int colGap = sc(10, scale);
		const int tableW = playersW - 2 * inner;
		int colW[kColumns] = { sc(150, scale), sc(92, scale), 0, sc(72, scale) };
		colW[2] = tableW - colW[0] - colW[1] - colW[3] - 3 * colGap;
		const int headY = L.playersHeading.bottom() + gap / 2;
		int x = L.playersZone.x + inner;
		int colX[kColumns];
		for (int c = 0; c < kColumns; ++c) { colX[c] = x; x += colW[c] + colGap; }
		for (int c = 0; c < kColumns; ++c) L.columnHead[c] = Rect{ colX[c], headY, colW[c], sc(24, scale) };
		const int rowsTop = headY + sc(24, scale) + rowGap;
		for (int r = 0; r < kSlots; ++r)
			for (int c = 0; c < kColumns; ++c)
				L.cell[r][c] = Rect{ colX[c], rowsTop + r * (rowH + rowGap), colW[c], rowH };

		// rules
		L.rulesHeading = Rect{ L.rulesZone.x + inner, L.rulesZone.y + inner, rulesW - 2 * inner, headH };
		const int ruleW = rulesW - 2 * inner;
		int y = L.rulesHeading.bottom() + gap / 2;
		L.ruleCount = rules;
		for (int i = 0; i < rules; ++i)
		{
			L.ruleLabel[i] = Rect{ L.rulesZone.x + inner, y, ruleW, cardLabelH };
			L.ruleControl[i] = Rect{ L.rulesZone.x + inner, y + cardLabelH, ruleW, cardControlH };
			y += cardLabelH + cardControlH + cardGap;
		}
		L.hasHonors = in.honors;
		if (in.honors)
		{
			y += gap / 2;
			for (int i = 0; i < kHonorRows; ++i)
			{
				const int vw = sc(64, scale);
				L.honorLabel[i] = Rect{ L.rulesZone.x + inner, y + i * honorH, ruleW - vw, honorH };
				L.honorValue[i] = Rect{ L.rulesZone.right() - inner - vw, y + i * honorH, vw, honorH };
			}
		}

		// bottom
		const int actionsTop = P.bottom() - pad - actionH;
		L.status = Rect{ P.x + pad, L.mapZone.bottom() + gap / 2, P.w - 2 * pad - 2 * sc(180, scale), statusH };
		const int bw = sc(180, scale);
		L.start = Rect{ P.right() - pad - bw, actionsTop, bw, actionH };
		L.back = Rect{ L.start.x - gap - bw, actionsTop, bw, actionH };
		L.effectiveScale = scale;
		L.compact = scale < (in.uiPercent > 0 ? in.uiPercent : 100);
		return L;
	}
	return L;
}
} // namespace ModernMatchSetup
