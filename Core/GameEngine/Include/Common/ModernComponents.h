// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience component model (pure, engine-free): the states every component can be in, the style each (kind, state) resolves to, and the chamfered-shape geometry.
//
// All Modern screens draw their controls through ONE table (`style()`), so hover / focus / pressed / disabled / selected look the same on a menu item, a category tab, a combo box or a list row,
// and a new component only needs a Kind and (if it is a new look) a row in the table. Nothing here knows a window or a display.
#pragma once

#include <cmath>
#include <cstdint>
#include <type_traits>

#include "Common/ModernTheme.h"

namespace ModernComponents
{
using ModernTheme::Color;
using ModernTheme::Palette;

enum class State : std::uint8_t { Normal, Hover, Focus, Pressed, Disabled, Selected };

struct Flags
{
	bool enabled = true;
	bool hover = false;
	bool focus = false;
	bool pressed = false;
	bool selected = false;
};

// One resolution order for every component: disabled beats everything, a held press beats focus, focus beats hover, hover beats the resting selected look.
inline State resolve(const Flags &f)
{
	if (!f.enabled) return State::Disabled;
	if (f.pressed) return State::Pressed;
	if (f.focus) return State::Focus;
	if (f.hover) return State::Hover;
	if (f.selected) return State::Selected;
	return State::Normal;
}

enum class Kind : std::uint8_t
{
	Nav,          // navigation item: text-forward, no resting box (main menu)
	NavMinor,     // lesser navigation item (Back footer)
	NavDanger,    // exit navigation item (Exit Game): navigation look in the danger colour
	Primary,      // the one main action of a screen: filled accent
	Secondary,    // an ordinary action: outlined
	Danger,       // exit / destructive: outlined in the danger colour
	Tab,          // category rail / tab entry
	Field,        // value box of a combo / entry
	Row,          // list row
	Count
};

struct Style
{
	Color fill;        // surface (alpha 0 = none)
	Color edge;        // outline (alpha 0 = none)
	Color text;
	Color tick;        // the accent tick / bar at the leading edge (alpha 0 = none)
	std::uint8_t edgeWidth;   // 0, 1 or 2 (scaled by the caller)
	bool chamfer;      // the surface has cut corners
};

inline Color mix(const Color &a, const Color &b, int percentB)
{
	auto lerp = [&](int x, int y) { return static_cast<std::uint8_t>((x * (100 - percentB) + y * percentB + 50) / 100); };
	return Color{ lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), lerp(a.a, b.a) };
}

inline Style style(Kind kind, State state, const Palette &p)
{
	const Color none = { 0, 0, 0, 0 };
	const bool danger = kind == Kind::Danger || kind == Kind::NavDanger;
	const Color accent = danger ? p.danger : p.accent;
	switch (kind)
	{
	case Kind::Nav:
	case Kind::NavMinor:
	case Kind::NavDanger:
	{
		const Color rest = kind == Kind::Nav ? p.text : (kind == Kind::NavDanger ? mix(p.text, p.danger, 30) : p.textSecondary);
		switch (state)
		{
		case State::Hover:    return { p.accentSoft, none, p.text, accent, 0, true };
		case State::Focus:    return { p.accentSoft.withAlpha(72), none, p.text, accent, 0, true };
		case State::Pressed:  return { accent.withAlpha(120), none, p.text, accent, 0, true };
		case State::Disabled: return { none, none, p.textDisabled, none, 0, false };
		case State::Selected: return { none, none, p.text, accent, 0, false };
		default:              return { none, none, rest, none, 0, false };
		}
	}
	case Kind::Primary:
		switch (state)
		{
		case State::Hover:    return { mix(p.accent, Color{ 255, 255, 255, 255 }, 14), none, p.textOnAccent, none, 0, true };
		case State::Focus:    return { mix(p.accent, Color{ 255, 255, 255, 255 }, 22), p.text, p.textOnAccent, none, 1, true };
		case State::Pressed:  return { mix(p.accent, Color{ 0, 0, 0, 255 }, 22), none, p.textOnAccent, none, 0, true };
		case State::Disabled: return { p.panelRaised, p.hairline, p.textDisabled, none, 1, true };
		default:              return { p.accent, none, p.textOnAccent, none, 0, true };
		}
	case Kind::Secondary:
	case Kind::Danger:
		switch (state)
		{
		case State::Hover:    return { accent.withAlpha(34), accent, p.text, none, 1, true };
		case State::Focus:    return { accent.withAlpha(52), p.focus, p.text, none, 2, true };
		case State::Pressed:  return { accent.withAlpha(110), accent, p.text, none, 1, true };
		case State::Disabled: return { none, p.hairline, p.textDisabled, none, 1, true };
		case State::Selected: return { accent.withAlpha(34), accent, p.text, none, 1, true };
		default:              return { p.panelRaised.withAlpha(150), p.edge, p.text, none, 1, true };
		}
	case Kind::Tab:
		switch (state)
		{
		case State::Hover:    return { p.accentSoft, none, p.text, p.edge, 0, true };
		case State::Focus:    return { p.accentSoft.withAlpha(72), none, p.text, accent, 0, true };
		case State::Pressed:  return { accent.withAlpha(110), none, p.text, accent, 0, true };
		case State::Disabled: return { none, none, p.textDisabled, none, 0, false };
		case State::Selected: return { p.panelRaised, none, p.text, accent, 0, true };
		default:              return { none, none, p.textSecondary, none, 0, false };
		}
	case Kind::Field:
		switch (state)
		{
		case State::Hover:    return { p.field, p.edge, p.text, none, 1, true };
		case State::Focus:    return { p.field, p.focus, p.text, none, 2, true };
		case State::Pressed:  return { p.field, p.accent, p.text, none, 2, true };
		case State::Disabled: return { p.field.withAlpha(120), p.hairline, p.textDisabled, none, 1, true };
		default:              return { p.field, p.hairline, p.text, none, 1, true };
		}
	case Kind::Row:
	default:
		switch (state)
		{
		case State::Hover:    return { p.accentSoft.withAlpha(30), none, p.text, none, 0, false };
		case State::Focus:    return { p.accentSoft, none, p.text, accent, 0, false };
		case State::Pressed:  return { accent.withAlpha(110), none, p.text, accent, 0, false };
		case State::Disabled: return { none, none, p.textDisabled, none, 0, false };
		case State::Selected: return { p.accentSoft.withAlpha(64), none, p.text, accent, 0, false };
		default:              return { none, none, p.textSecondary, none, 0, false };
		}
	}
}

// ---- contrast (used by the tests and by the audit: text must read on its surface over any shell-map backdrop) -------------------------------------------------------------------
inline double luminance(const Color &c)
{
	auto lin = [](int v) { const double s = v / 255.0; return s <= 0.03928 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4); };
	return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
}

// Contrast ratio of `text` over `surface` composited on `backdrop` (the surface alpha lets the shell map through, so the worst case is judged against a given backdrop colour).
inline double contrast(const Color &text, const Color &surface, const Color &backdrop)
{
	const Color under = mix(backdrop, Color{ surface.r, surface.g, surface.b, 255 }, surface.a * 100 / 255);
	const double a = luminance(text) + 0.05, b = luminance(under) + 0.05;
	return a > b ? a / b : b / a;
}

// ---- chamfered shapes ---------------------------------------------------------------------------------------------------------------------------------------------------------
struct Span { int x, y, w, h; };

constexpr int kMaxSpans = 96;

// The filled spans of a w x h rectangle whose corners are cut (top-left, top-right, bottom-right, bottom-left; 0 = square), relative to (0, 0).
// Returns the number of spans written (at most `max`); a cut is clamped to half the shorter side. The union of the spans is the shape; spans never overlap.
inline int chamferSpans(int w, int h, int cutTL, int cutTR, int cutBR, int cutBL, Span *out, int max)
{
	if (w <= 0 || h <= 0 || max <= 0) return 0;
	const int limit = (w < h ? w : h) / 2;
	auto clampCut = [&](int c) { return c < 0 ? 0 : (c > limit ? limit : c); };
	cutTL = clampCut(cutTL); cutTR = clampCut(cutTR); cutBR = clampCut(cutBR); cutBL = clampCut(cutBL);
	int n = 0;
	auto rowLeft = [&](int y) {
		int l = 0;
		if (y < cutTL) l = cutTL - y;
		const int fromBottom = h - 1 - y;
		if (fromBottom < cutBL) { const int v = cutBL - fromBottom; if (v > l) l = v; }
		return l;
	};
	auto rowRight = [&](int y) {
		int r = 0;
		if (y < cutTR) r = cutTR - y;
		const int fromBottom = h - 1 - y;
		if (fromBottom < cutBR) { const int v = cutBR - fromBottom; if (v > r) r = v; }
		return r;
	};
	int y = 0;
	while (y < h && n < max)
	{
		const int l = rowLeft(y), r = rowRight(y);
		int y2 = y + 1;
		while (y2 < h && rowLeft(y2) == l && rowRight(y2) == r) ++y2;   // merge identical rows
		out[n++] = Span{ l, y, w - l - r, y2 - y };
		y = y2;
	}
	return n;
}

// Is the point inside the chamfered shape? (hit-testing of non-rectangular components)
inline bool inChamfer(int px, int py, int w, int h, int cutTL, int cutTR, int cutBR, int cutBL)
{
	Span s[kMaxSpans];
	const int n = chamferSpans(w, h, cutTL, cutTR, cutBR, cutBL, s, kMaxSpans);
	for (int i = 0; i < n; ++i)
		if (px >= s[i].x && px < s[i].x + s[i].w && py >= s[i].y && py < s[i].y + s[i].h) return true;
	return false;
}

static_assert(std::is_trivially_copyable<Style>::value && std::is_trivially_copyable<Flags>::value, "plain data");
} // namespace ModernComponents
