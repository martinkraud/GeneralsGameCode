// SPDX-License-Identifier: GPL-3.0-or-later
// X64.37 v3: Render Scale - the pure arithmetic (no D3D, no engine singleton).
//
// Render Scale renders the 3D WORLD (the Submit_World submissions: terrain, water, objects, effects) into an intermediate target
// of `percent` of the presentation size and composites it to the back buffer with a bilinear filter. Everything else - the 2D
// interface, fonts, tooltips, radar, the control bar, mouse coordinates - stays in presentation space at full resolution. It is
// ordinary internal-resolution scaling: no temporal data, no vendor technology. It reduces GPU work only.
#pragma once

#include <string>

namespace WorldRenderScale
{
constexpr unsigned kChoices[] = {100, 90, 80, 75, 67, 50};
constexpr int kChoiceCount = static_cast<int>(sizeof(kChoices) / sizeof(kChoices[0]));
constexpr unsigned kDefaultPercent = 100;
constexpr unsigned kMinDimension = 64;     // the intermediate target is never smaller than this (or the presentation size)

struct Size { unsigned width = 0, height = 0; };

// Nearest offered value for anything stored or passed in; non-positive and absurd values mean the default (100 %).
inline unsigned sanitize(int percent)
{
	if (percent <= 0 || percent > 400) return kDefaultPercent;
	unsigned best = kDefaultPercent;
	int bestDelta = 1000000;
	for (unsigned c : kChoices)
	{
		const int delta = percent > static_cast<int>(c) ? percent - static_cast<int>(c) : static_cast<int>(c) - percent;
		if (delta < bestDelta) { best = c; bestDelta = delta; }
	}
	return best;
}

inline int indexOf(unsigned percent)
{
	for (int i = 0; i < kChoiceCount; ++i)
		if (kChoices[i] == percent) return i;
	return 0;
}

inline bool active(unsigned percent) { return percent < 100; }

// One dimension of the intermediate target: round(full * percent / 100), at least kMinDimension (never above `full`), never zero.
inline unsigned scaledDimension(unsigned full, unsigned percent)
{
	if (full == 0) return 0;
	if (percent >= 100) return full;
	const unsigned long long scaled = (static_cast<unsigned long long>(full) * percent + 50) / 100;
	unsigned v = static_cast<unsigned>(scaled);
	const unsigned floorValue = full < kMinDimension ? full : kMinDimension;
	if (v < floorValue) v = floorValue;
	if (v > full) v = full;
	return v;
}

inline Size internalSize(unsigned width, unsigned height, unsigned percent)
{
	return {scaledDimension(width, percent), scaledDimension(height, percent)};
}

// "75% (1920 x 1080)" - the effective internal resolution for a presentation size.
inline std::string label(unsigned percent, unsigned width, unsigned height)
{
	const Size s = internalSize(width, height, percent);
	return std::to_string(percent) + "% (" + std::to_string(s.width) + " x " + std::to_string(s.height) + ")";
}

// Maps a rectangle of the presentation space into the intermediate target (floor on the near edge, ceil on the far edge so the
// scaled rectangle always covers the original one; clamped to the target; never empty).
struct Rect { unsigned x = 0, y = 0, w = 0, h = 0; };
inline Rect mapRect(const Rect &r, unsigned fullW, unsigned fullH, unsigned lowW, unsigned lowH)
{
	if (fullW == 0 || fullH == 0 || lowW == 0 || lowH == 0) return {};
	unsigned x0 = static_cast<unsigned>(static_cast<unsigned long long>(r.x) * lowW / fullW);
	unsigned y0 = static_cast<unsigned>(static_cast<unsigned long long>(r.y) * lowH / fullH);
	unsigned x1 = static_cast<unsigned>((static_cast<unsigned long long>(r.x + r.w) * lowW + fullW - 1) / fullW);
	unsigned y1 = static_cast<unsigned>((static_cast<unsigned long long>(r.y + r.h) * lowH + fullH - 1) / fullH);
	if (x0 >= lowW) x0 = lowW - 1;
	if (y0 >= lowH) y0 = lowH - 1;
	if (x1 > lowW) x1 = lowW;
	if (y1 > lowH) y1 = lowH;
	if (x1 <= x0) x1 = x0 + 1;
	if (y1 <= y0) y1 = y0 + 1;
	return {x0, y0, x1 - x0, y1 - y0};
}
}  // namespace WorldRenderScale
