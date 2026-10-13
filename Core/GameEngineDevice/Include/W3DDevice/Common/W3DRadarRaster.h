// SPDX-License-Identifier: GPL-3.0-or-later
// The deterministic raster rules of the original W3DRadar, kept free of engine singletons so they can be
// exercised offline: height shading of the terrain texture, the 2x2 object marker, the stealth pulse, the
// shroud level -> alpha mapping and the logic-cell -> radar-cell rectangle. W3DRadar uses these (the legacy
// code paths are moved here unchanged); NativeRadarTest drives them against the dynamic UI texture path.
#pragma once

#include "Lib/BaseType.h"
#include "Common/GameCommon.h"
#include "Common/Radar.h"

namespace RadarRaster
{

// "Is the point legal, that is, inside the resolution of the radar cells" (legalRadarPoint).
inline bool legalPoint(int px, int py)
{
	return !(px < 0 || py < 0 || px >= RADAR_CELL_WIDTH || py >= RADAR_CELL_HEIGHT);
}

// Shade `color` for `height`: heights above midZ lighten toward white, below darken toward black.
// (W3DRadar::interpolateColorForHeight)
inline void shadeForHeight(RGBColor *color, Real height, Real hiZ, Real midZ, Real loZ)
{
	const Real howBright = 0.95f;  // bigger is brighter (0.0 to 1.0)
	const Real howDark   = 0.60f;  // bigger is darker (0.0 to 1.0)

	if (hiZ == midZ)
		hiZ = midZ + 0.1f;
	if (midZ == loZ)
		loZ = midZ - 0.1f;
	if (hiZ == loZ)
		hiZ = loZ + 0.2f;

	Real t;
	RGBColor colorTarget;

	if (height >= midZ)
	{
		t = (height - midZ) / (hiZ - midZ);

		colorTarget.red = color->red + (1.0f - color->red) * howBright;
		colorTarget.green = color->green + (1.0f - color->green) * howBright;
		colorTarget.blue = color->blue + (1.0f - color->blue) * howBright;
	}
	else  // interpolate darker
	{
		t = (midZ - height) / (midZ - loZ);

		colorTarget.red = color->red + (0.0f - color->red) * howDark;
		colorTarget.green = color->green + (0.0f - color->green) * howDark;
		colorTarget.blue = color->blue + (0.0f - color->blue) * howDark;
	}

	color->red = color->red + (colorTarget.red - color->red) * t;
	color->green = color->green + (colorTarget.green - color->green) * t;
	color->blue = color->blue + (colorTarget.blue - color->blue) * t;

	if (color->red < 0.0f) color->red = 0.0f;
	if (color->red > 1.0f) color->red = 1.0f;
	if (color->green < 0.0f) color->green = 0.0f;
	if (color->green > 1.0f) color->green = 1.0f;
	if (color->blue < 0.0f) color->blue = 0.0f;
	if (color->blue > 1.0f) color->blue = 1.0f;
}

// Alpha of a stealthed marker: a one second ramp between `minAlpha` and opaque (renderObjectList).
inline UnsignedByte stealthAlpha(UnsignedInt frame, UnsignedInt framesForTransition, UnsignedByte minAlpha = 32)
{
	const Real alphaScale = INT_TO_REAL(frame % framesForTransition) / (framesForTransition / 2.0f);
	if (alphaScale > 0.0f)
		return REAL_TO_UNSIGNEDBYTE(((alphaScale - 1.0f) * (255.0f - minAlpha)) + minAlpha);
	return REAL_TO_UNSIGNEDBYTE((alphaScale * (255.0f - minAlpha)) + minAlpha);
}

// Radar shroud alpha for a logic cell (W3DRadar::setShroudLevel): shrouded opaque, fogged half, clear none.
inline Int shroudAlpha(CellShroudStatus setting)
{
	if (setting == CELLSHROUD_SHROUDED)
		return 255;
	if (setting == CELLSHROUD_FOGGED)
		return 127;
	return 0;
}

// Plot the original 2x2 marker whose lower-left cell is (x, y) through `put(x, y)`, clipping each of the
// four cells separately like the legacy code (order: (x,y), (x,y+1), (x+1,y+1), (x+1,y)).
template <class Put>
inline void plotMarker(Int x, Int y, Put put)
{
	if (legalPoint(x, y)) put(x, y);
	++y;
	if (legalPoint(x, y)) put(x, y);
	++x;
	if (legalPoint(x, y)) put(x, y);
	--y;
	if (legalPoint(x, y)) put(x, y);
}

// World position -> radar cell, the same truncation Radar::worldToRadar applies (before its clamp).
inline void worldToCell(Real worldX, Real worldY, Real xSample, Real ySample, Int *cellX, Int *cellY)
{
	*cellX = static_cast<Int>(worldX / xSample);
	*cellY = static_cast<Int>(worldY / ySample);
}

// The radar rectangle (inclusive) a logic shroud cell covers, with Radar::worldToRadar's clamping.
struct CellRect { Int minX, minY, maxX, maxY; };
inline CellRect shroudCellRect(Int shroudX, Int shroudY, Int cellWidth, Int cellHeight, Real xSample, Real ySample)
{
	auto clampCell = [](Int v, Int limit) { return v < 0 ? 0 : (v >= limit ? limit - 1 : v); };
	Int ax, ay, bx, by;
	worldToCell(static_cast<Real>(shroudX * cellWidth), static_cast<Real>(shroudY * cellHeight), xSample, ySample, &ax, &ay);
	worldToCell(static_cast<Real>((shroudX + 1) * cellWidth), static_cast<Real>((shroudY + 1) * cellHeight), xSample, ySample, &bx, &by);
	return { clampCell(ax, RADAR_CELL_WIDTH), clampCell(ay, RADAR_CELL_HEIGHT),
	         clampCell(bx, RADAR_CELL_WIDTH), clampCell(by, RADAR_CELL_HEIGHT) };
}

}
