// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 FIX2: composition of the in-match HUD on a modern surface (pure, testable).
//
// FIX1 stretched a slice of the bar art across both sides of the surface. Native review rejected it: the radar,
// the command area and the info panel stopped looking like one console, and the band dominated the lower quarter
// of a wide screen. FIX2 composes instead of stretching:
//
//   world world world world world world world world world world world
//   world world world [ ONE coherent Generals control bar ] world world world
//
// * The CORE HUD is the original control bar, proportionally scaled by the UI scale and centred (its radar,
//   command area and info panel keep their original relationships). Its width is 800 reference units x scale: it
//   never grows with the surface.
// * The WORLD uses the whole surface, including beside the core; it is never letterboxed.
// * Under the core the world is hidden by an opaque backing exactly where the retail viewport ended (the
//   retail bar sat on black below its 80 % viewport), so the console panels read as they always did.
// * Nothing outside the core blocks input: the world is the world. Only the bar's own windows take clicks.
// * Named regions (timer block, notification block) are defined from the core, so later features attach to
//   stable anchors rather than to the surface edges.
#pragma once

#include <algorithm>

namespace HudComposition
{

struct Rect
{
	int x = 0, y = 0, w = 0, h = 0;
	int right() const { return x + w; }
	int bottom() const { return y + h; }
	bool empty() const { return w <= 0 || h <= 0; }
	bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

struct Composition
{
	Rect core;           // the control bar band (physical pixels): the only HUD input region
	Rect backing;        // opaque black under the core's lower part (retail: below the 80 % viewport)
	Rect worldLeft;      // visible world beside the core (informational; empty when the core reaches the edge)
	Rect worldRight;
	Rect timerBlock;     // stable anchor region for superweapon / named timers (above the core, inside its width)
	Rect noticeBlock;    // stable anchor region reserved for notifications (above the timers)
	bool qualifies = false;
};

// `band` is the drawn control bar band in physical pixels. `refBandW/refBandBottom/refCanvas*` describe it on the
// creation canvas (a full-width band touching the bottom). `backingTopFromBottom` is the retail
// "(1 - viewportHeightScale) x canvas height" scaled to physical pixels.
inline Composition compute(int bandX, int bandY, int bandW, int bandH, int surfaceW, int surfaceH,
                           int refBandW, int refBandBottom, int refCanvasW, int refCanvasH, int backingHeight)
{
	Composition c;
	c.core = {bandX, bandY, bandW, bandH};
	c.qualifies = refCanvasW > 0 && refBandW * 10 >= refCanvasW * 9 && refBandBottom >= refCanvasH - 8 && bandW > 0 && bandH > 0;
	if (!c.qualifies)
		return c;
	// The backing is part of the band: it is the lower `backingHeight` of the band wherever the band currently is.
	// FIX2 anchored it to the surface bottom, so while the bar slid in (offset > 0) the black backing sat at its final
	// place before the art arrived: the black flash seen on native review. Derived from the band it can never be
	// visible without the art it backs, and it is clipped to the surface.
	const int backH = std::min(std::max(backingHeight, 0), bandH);
	const int backTop = bandY + bandH - backH;
	const int clippedTop = std::max(backTop, 0);
	// The art band ends a reference pixel above the surface edge (y 599 of 600): when the band rests on the edge the
	// backing continues to it so no sliver of world shows under the bar; while it slides, it follows the band.
	const int edgeSlack = std::max(bandH / 64, 2);
	const int bandBottom = bandY + bandH;
	const int backBottom = (surfaceH - bandBottom >= 0 && surfaceH - bandBottom <= edgeSlack) ? surfaceH : bandBottom;
	const int clippedBottom = std::min(backBottom, surfaceH);
	const int topInSurface = std::min(clippedTop, surfaceH);
	c.backing = {bandX, topInSurface, bandW, std::max(clippedBottom - topInSurface, 0)};
	if (bandX > 0)
		c.worldLeft = {0, bandY, bandX, bandH};
	if (bandX + bandW < surfaceW)
		c.worldRight = {bandX + bandW, bandY, surfaceW - (bandX + bandW), bandH};
	// Timer/notice blocks: a column band just above the core, as wide as the core, so they never drift sideways
	// with the surface. Heights are fractions of the core height (UI-scale relative by construction).
	const int blockH = std::max(bandH / 4, 1);
	c.timerBlock = {bandX, bandY - blockH, bandW, blockH};
	c.noticeBlock = {bandX, bandY - 2 * blockH, bandW, blockH};
	return c;
}

// The lifecycle rule as a function: the backing is visible only when the control bar parent is visible AND the
// scheme's band art exists (the same two facts that make the HUD itself visible). Nothing else may show it.
inline bool backingVisible(bool controlBarParentVisible, bool bandArtAvailable)
{
	return controlBarParentVisible && bandArtAvailable;
}

}  // namespace HudComposition
