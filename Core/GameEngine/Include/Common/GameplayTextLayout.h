// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience gameplay text: the pure rules that keep the retail in-match message lines readable on modern surfaces (Gameplay UI polish, post Zero Hour HUD).
// The retail message lines are single-line display strings at a fixed point size, drawn at (10, 10): on a 1024 x 768 surface a long message runs off the right edge, and on a
// 5120 x 1440 surface the same few points are tiny. Classic keeps the retail behaviour; Modern wraps the line inside the surface and scales the point size with the surface height.
// Engine-free, unit tested (GameplayTextLayout tests).
#pragma once

#include <algorithm>
#include <cmath>

namespace GameplayTextLayout
{
// Surfaces up to this height keep the retail point size; above it the size grows with the height, up to kMaxScalePercent.
constexpr int kReferenceHeight = 900;
constexpr int kMaxScalePercent = 160;
// The widest a wrapped message line may be, at 100 % (a wider line is hard to read and would run across an ultrawide surface).
constexpr int kMaxLineWidth = 900;

inline int scalePercent(int surfaceHeight)
{
	if (surfaceHeight <= 0)
		return 100;
	return std::max(100, std::min(kMaxScalePercent, static_cast<int>(std::lround(100.0 * surfaceHeight / kReferenceHeight))));
}

inline int pointSize(int basePoints, int surfaceHeight)
{
	return std::max(1, static_cast<int>(std::lround(basePoints * scalePercent(surfaceHeight) / 100.0)));
}

// The width at which a message wraps: the surface minus the origin margin on both sides, never wider than kMaxLineWidth scaled.
inline int wrapWidth(int surfaceWidth, int surfaceHeight, int originX)
{
	const int inside = std::max(1, surfaceWidth - 2 * std::max(0, originX));
	const int cap = std::max(1, kMaxLineWidth * scalePercent(surfaceHeight) / 100);
	return std::min(inside, cap);
}

// The superweapon countdown list. The INI anchor (0.7 of the bottom-anchored 800 x 600 block) is 180 reference units above the surface bottom, which in Modern is exactly the top of the
// Enhanced HUD's command region (over the power meter), and the retail bottom margin (82 % of the view height) lies inside the HUD. With the Enhanced HUD active the list is lifted so that
// four lines fit above the HUD, and the HUD's top is the margin.
inline int superweaponMargin(int retailMargin, int hudBandTop) { return std::min(retailMargin, hudBandTop - 6); }
inline int superweaponStartY(int anchorY, int hudBandTop, double referenceScale)
{
	return std::min(anchorY, hudBandTop - 6 - static_cast<int>(64.0 * referenceScale));
}
}  // namespace GameplayTextLayout
