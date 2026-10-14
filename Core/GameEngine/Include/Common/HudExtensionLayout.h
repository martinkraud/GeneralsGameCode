// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 FIX1: geometry of the widescreen control-bar extension (pure, testable).
//
// The control bar art is an 800-reference-unit-wide band hugging the bottom edge. On a wider surface the band is
// centred and the world used to show black on both sides of it. The extension continues the band to the surface
// edges with a two-texel-wide slice of the bar's own left console panel, stretched horizontally: stretching a
// single column only ever repeats flat metal/bezel rows (it cannot distort radar, buttons or decoration, which live
// in the original, proportionally scaled core). Nothing here depends on a monitor size.
//
//   [ left extension ][ original control bar core ][ right extension ]
//        blocks input        owns all controls          blocks input
//
// Future HUD regions (notification area, optional panels) attach to these same rectangles.
#pragma once

#include <algorithm>

namespace HudExtensionLayout
{

struct Rect
{
	int x = 0, y = 0, w = 0, h = 0;
	int right() const { return x + w; }
	int bottom() const { return y + h; }
	bool empty() const { return w <= 0 || h <= 0; }
};

struct Layout
{
	Rect core;                // the original band (physical pixels)
	Rect left;                // extension to the left surface edge (empty when the band reaches it)
	Rect right;               // extension to the right surface edge
	bool qualifies = false;   // a full-width band touching the bottom of the reference canvas
};

// `bandX/bandY/bandW/bandH` is the drawn band in physical pixels, `surfaceW` the back-buffer width.
// `refBandW/refBandBottom` describe the band on the creation canvas (to decide whether it is the full-width bar).
inline Layout compute(int bandX, int bandY, int bandW, int bandH, int surfaceW,
                      int refBandW, int refBandBottom, int refCanvasW, int refCanvasH)
{
	Layout l;
	l.core = {bandX, bandY, bandW, bandH};
	l.qualifies = refCanvasW > 0 && refBandW * 10 >= refCanvasW * 9 && refBandBottom >= refCanvasH - 8 && bandW > 0 && bandH > 0;
	if (!l.qualifies)
		return l;
	if (bandX > 0)
		l.left = {0, bandY, bandX, bandH};
	if (bandX + bandW < surfaceW)
		l.right = {bandX + bandW, bandY, surfaceW - (bandX + bandW), bandH};
	return l;
}

// Source column (fraction of the band width): inside the left console panel, clear of the panel bezel corner and of
// the end-cap decoration of the top plate in all three factions' art.
constexpr double kSliceFraction = 0.11;
constexpr int kSliceTexels = 2;

}  // namespace HudExtensionLayout
