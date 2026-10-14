// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 Modern Experience display foundation: the single source of truth for physical <-> reference geometry.
//
// Vocabulary
//   physical   the window client area / back buffer in pixels (what the GPU renders and the mouse reports)
//   reference  the 800x600 canvas every original Generals layout (WND files, HUD constants) was authored for
//   canvas     the reference canvas placed on the physical screen: reference size x UI scale, centered
//   world      the 3D viewport; always the whole physical surface (never cropped to 4:3)
//   UI scale   uniform magnification of the reference canvas (percent); never anisotropic
//   anchor     which part of the physical screen a HUD element keeps its distance from
//
// Pure (no windows.h, no engine state): usable from tests and from every layer. Nothing here changes game
// logic; it only decides where pixels go.
#pragma once

#include <cmath>
#include <cstdint>

namespace DisplayGeometry
{

struct Size
{
	int width = 0;
	int height = 0;
	bool valid() const { return width > 0 && height > 0; }
	bool operator==(const Size &o) const { return width == o.width && height == o.height; }
	bool operator!=(const Size &o) const { return !(*this == o); }
};

struct Point
{
	int x = 0;
	int y = 0;
	bool operator==(const Point &o) const { return x == o.x && y == o.y; }
};

struct Rect
{
	int x = 0, y = 0, width = 0, height = 0;
	int right() const { return x + width; }
	int bottom() const { return y + height; }
	bool contains(Point p) const { return p.x >= x && p.y >= y && p.x < right() && p.y < bottom(); }
	bool operator==(const Rect &o) const { return x == o.x && y == o.y && width == o.width && height == o.height; }
};

constexpr Size kReferenceSize = {800, 600};
constexpr int kMinUiScalePercent = 50;
constexpr int kMaxUiScalePercent = 400;
constexpr int kAutoUiScale = 0;          // "Auto" in settings
constexpr int kAutoStepPercent = 25;     // Auto snaps down to quarter steps so UI art lands on whole pixels more often

enum class Anchor
{
	TopLeft, Top, TopRight,
	Left, Center, Right,
	BottomLeft, Bottom, BottomRight
};

// The largest scale (percent) at which the whole reference canvas still fits the physical surface.
inline int fitScalePercent(Size physical)
{
	if (!physical.valid())
		return 100;
	const long long byWidth = static_cast<long long>(physical.width) * 100 / kReferenceSize.width;
	const long long byHeight = static_cast<long long>(physical.height) * 100 / kReferenceSize.height;
	const long long fit = byWidth < byHeight ? byWidth : byHeight;
	return static_cast<int>(fit < 1 ? 1 : (fit > kMaxUiScalePercent ? kMaxUiScalePercent : fit));
}

// Auto: quarter steps (100, 125, 150 ...) that fit. Surfaces smaller than the canvas shrink it to fit exactly.
inline int autoUiScalePercent(Size physical)
{
	const int fit = fitScalePercent(physical);
	if (fit < 100)
		return fit < kMinUiScalePercent ? kMinUiScalePercent : fit;
	return (fit / kAutoStepPercent) * kAutoStepPercent;
}

// Requested scale (0 = Auto) -> effective scale: clamped to the supported range and to what fits.
inline int resolveUiScalePercent(Size physical, int requestedPercent)
{
	if (requestedPercent <= 0)
		return autoUiScalePercent(physical);
	int scale = requestedPercent;
	if (scale < kMinUiScalePercent)
		scale = kMinUiScalePercent;
	const int fit = fitScalePercent(physical);
	if (scale > fit)
		scale = fit < kMinUiScalePercent ? kMinUiScalePercent : fit;
	return scale;
}

struct Layout
{
	Size physical;           // back buffer / client size
	int uiScalePercent = 100;
	Rect canvas;             // reference canvas on the physical surface (centered)
	Rect world;              // 3D viewport: the whole surface
	double aspect = 4.0 / 3.0;

	double scale() const { return uiScalePercent / 100.0; }
	bool isWide() const { return aspect > 4.0 / 3.0 + 0.005; }
};

inline int scaled(int value, int percent)
{
	// Round half away from zero; symmetric for negative reference offsets.
	const long long v = static_cast<long long>(value) * percent;
	return static_cast<int>(v >= 0 ? (v + 50) / 100 : -((-v + 50) / 100));
}

inline Layout computeLayout(Size physical, int requestedUiScalePercent = kAutoUiScale)
{
	Layout layout;
	if (!physical.valid())
		physical = kReferenceSize;
	layout.physical = physical;
	layout.uiScalePercent = resolveUiScalePercent(physical, requestedUiScalePercent);
	const int w = scaled(kReferenceSize.width, layout.uiScalePercent);
	const int h = scaled(kReferenceSize.height, layout.uiScalePercent);
	layout.canvas = {(physical.width - w) / 2, (physical.height - h) / 2, w, h};
	layout.world = {0, 0, physical.width, physical.height};
	layout.aspect = static_cast<double>(physical.width) / static_cast<double>(physical.height);
	return layout;
}

// The physical position of the reference origin for an anchor: reference (0,0) maps to this point.
inline Point anchorOrigin(const Layout &layout, Anchor anchor)
{
	const int blockW = scaled(kReferenceSize.width, layout.uiScalePercent);
	const int blockH = scaled(kReferenceSize.height, layout.uiScalePercent);
	Point origin = {layout.canvas.x, layout.canvas.y};
	switch (anchor)
	{
	case Anchor::TopLeft: case Anchor::Left: case Anchor::BottomLeft: origin.x = 0; break;
	case Anchor::TopRight: case Anchor::Right: case Anchor::BottomRight: origin.x = layout.physical.width - blockW; break;
	default: break;
	}
	switch (anchor)
	{
	case Anchor::TopLeft: case Anchor::Top: case Anchor::TopRight: origin.y = 0; break;
	case Anchor::BottomLeft: case Anchor::Bottom: case Anchor::BottomRight: origin.y = layout.physical.height - blockH; break;
	default: break;
	}
	return origin;
}

inline Point referenceToPhysical(const Layout &layout, Point reference, Anchor anchor = Anchor::Center)
{
	const Point origin = anchorOrigin(layout, anchor);
	return {origin.x + scaled(reference.x, layout.uiScalePercent), origin.y + scaled(reference.y, layout.uiScalePercent)};
}

inline Rect referenceToPhysical(const Layout &layout, Rect reference, Anchor anchor = Anchor::Center)
{
	const Point origin = anchorOrigin(layout, anchor);
	const int x1 = origin.x + scaled(reference.x, layout.uiScalePercent);
	const int y1 = origin.y + scaled(reference.y, layout.uiScalePercent);
	const int x2 = origin.x + scaled(reference.x + reference.width, layout.uiScalePercent);
	const int y2 = origin.y + scaled(reference.y + reference.height, layout.uiScalePercent);
	return {x1, y1, x2 - x1, y2 - y1};   // edge-rounded so adjacent reference rects never gap or overlap
}

inline Point physicalToReference(const Layout &layout, Point physical, Anchor anchor = Anchor::Center)
{
	const Point origin = anchorOrigin(layout, anchor);
	const int scale = layout.uiScalePercent;
	const long long dx = static_cast<long long>(physical.x - origin.x) * 100;
	const long long dy = static_cast<long long>(physical.y - origin.y) * 100;
	auto divRound = [](long long v, int d) { return static_cast<int>(v >= 0 ? (v + d / 2) / d : -((-v + d / 2) / d)); };
	return {divRound(dx, scale), divRound(dy, scale)};
}

// Anchor for a reference rectangle: a rectangle that touches the right edge keeps its distance from the right
// physical edge, one that touches the bottom keeps its distance from the bottom, otherwise it stays on the centered
// canvas. Used by layouts that carry no explicit policy.
inline Anchor inferAnchor(Rect reference, int touchTolerance = 4)
{
	const bool right = reference.right() >= kReferenceSize.width - touchTolerance && reference.x > kReferenceSize.width / 2;
	const bool bottom = reference.bottom() >= kReferenceSize.height - touchTolerance && reference.y > kReferenceSize.height / 2;
	if (right && bottom) return Anchor::BottomRight;
	if (right) return Anchor::Right;
	if (bottom) return Anchor::Bottom;
	return Anchor::Center;
}

// The two layout families. Frontend (menus, dialogs, popups) lives on the centered reference canvas. Hud is the
// in-match overlay set: the control bar block hugs the bottom edge, the special-power sidebar hugs the right edge,
// and everything keeps one consistent vertical origin (the bottom), so the bar, its tooltip popup, the chat line and
// the sidebar never drift apart on a surface taller than the scaled canvas.
enum class LayoutKind { Frontend, Hud };

inline LayoutKind layoutKindForFile(const char *path)
{
	if (!path) return LayoutKind::Frontend;
	const char *name = path;
	for (const char *p = path; *p; ++p)
		if (*p == '\\' || *p == '/') name = p + 1;
	static const char *const hudPrefixes[] = {"controlbar", "genpowersshortcutbar", "ingamechat", "replaycontrol"};
	for (const char *prefix : hudPrefixes)
	{
		const char *n = name;
		const char *q = prefix;
		while (*q && *n && (*n | 0x20) == *q) { ++n; ++q; }
		if (*q == '\0') return LayoutKind::Hud;
	}
	return LayoutKind::Frontend;
}

inline Anchor anchorFor(LayoutKind kind, Rect reference, int touchTolerance = 4)
{
	if (kind == LayoutKind::Frontend)
		return Anchor::Center;
	const bool right = reference.right() >= kReferenceSize.width - touchTolerance && reference.x > kReferenceSize.width / 2;
	return right ? Anchor::BottomRight : Anchor::Bottom;
}

// Horizontal field of view that keeps the 4:3 vertical field of view at any aspect ratio ("Hor+").
// W3D's CameraClass::Set_View_Plane takes the horizontal angle and derives the vertical one from the aspect, so
// leaving the 4:3 horizontal angle in place on a wider surface would crop the world vertically.
inline double horizontalFovForAspect(double horizontalFovAt4x3, double aspect)
{
	if (aspect <= 0.0)
		return horizontalFovAt4x3;
	const double halfTan43 = std::tan(horizontalFovAt4x3 * 0.5);
	return 2.0 * std::atan(halfTan43 * (aspect / (4.0 / 3.0)));
}

// Vertical field of view of a horizontal angle at an aspect (for diagnostics and tests).
inline double verticalFovForAspect(double horizontalFov, double aspect)
{
	return 2.0 * std::atan(std::tan(horizontalFov * 0.5) / aspect);
}

}  // namespace DisplayGeometry
