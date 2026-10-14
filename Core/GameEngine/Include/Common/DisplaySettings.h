// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33: persisted display settings (Options.ini keys), their migration from pre-X64.33 preferences and the pure
// resolution of "what the user asked for" into "what the window/back buffer will actually be" for a given set of
// attached monitors. Engine-free: usable from tests, WinMain (before any engine singleton exists) and the Options UI.
//
// Keys written (all optional on read; unknown keys in the file are never touched):
//   DisplayVersion  1         marks preferences written by a build that understands this block
//   DisplayMonitor  0..n      monitor index in DisplayModes::enumerateMonitors() order (0 = primary)
//   DisplayWidth/Height       client/back-buffer size; absent or 0 = automatic
//   RefreshRate     Hz or 0   Borderless/Exclusive only; 0 = desktop rate
//   DisplayMode     Windowed | Borderless | Exclusive
//   VSync           yes | no
//   UIScale         Auto | 50..400
//   SmoothUnitMotion yes | no ground interpolation (Modern only; see GroundTranslation)
// The legacy `Resolution`, `FrameRateLimit` and `FPSLimit` keys keep their meaning (frame pacing is owned by the
// existing FramePacer; nothing here limits frames).
#pragma once

#include "Common/DisplayGeometry.h"
#include "Common/DisplayModes.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace DisplaySettings
{
using DisplayGeometry::Size;
using DisplayGeometry::Rect;
using KeyValues = std::map<std::string, std::string>;

constexpr int kVersion = 1;
constexpr int kMaxDimension = 16384;

enum class WindowMode { Windowed, Borderless, Exclusive };

struct Settings
{
	bool explicitSettings = false;           // DisplayVersion present
	int monitor = 0;
	Size resolution;                         // {0,0} = automatic
	int refreshHz = 0;
	WindowMode mode = WindowMode::Windowed;
	bool vsync = false;
	int uiScalePercent = DisplayGeometry::kAutoUiScale;
	bool smoothMotion = false;

	bool operator==(const Settings &o) const
	{
		return explicitSettings == o.explicitSettings && monitor == o.monitor && resolution == o.resolution &&
		       refreshHz == o.refreshHz && mode == o.mode && vsync == o.vsync &&
		       uiScalePercent == o.uiScalePercent && smoothMotion == o.smoothMotion;
	}
};

// Classic = the retail presentation: the 800x600 logical display in a window, no modern options.
inline Settings classicDefaults()
{
	Settings s;
	s.resolution = DisplayGeometry::kReferenceSize;
	s.uiScalePercent = 100;
	return s;
}

inline const char *modeName(WindowMode m)
{
	switch (m)
	{
	case WindowMode::Borderless: return "Borderless";
	case WindowMode::Exclusive: return "Exclusive";
	default: return "Windowed";
	}
}

namespace detail
{
inline bool iequals(const std::string &a, const char *b)
{
	size_t i = 0;
	for (; i < a.size() && b[i]; ++i)
		if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
			return false;
	return i == a.size() && b[i] == '\0';
}

inline bool parseInt(const std::string &text, int &out)
{
	if (text.empty()) return false;
	char *end = nullptr;
	const long v = std::strtol(text.c_str(), &end, 10);
	if (end == text.c_str() || *end != '\0' || v < -1000000 || v > 1000000) return false;
	out = static_cast<int>(v);
	return true;
}

inline const std::string *find(const KeyValues &kv, const char *key)
{
	for (const auto &e : kv)
		if (iequals(e.first, key)) return &e.second;
	return nullptr;
}

inline void put(KeyValues &kv, const char *key, const std::string &value)
{
	for (auto &e : kv)
		if (iequals(e.first, key)) { e.second = value; return; }
	kv[key] = value;
}
}  // namespace detail

// Reads the settings block. Classic ignores stored values entirely (it is the retail presentation). Preferences
// written before X64.33 (no DisplayVersion) migrate to the modern automatic defaults: their `Resolution` key was
// always the retail 800x600 and never an expression of intent on this runtime.
inline Settings load(const KeyValues &kv, bool classic)
{
	if (classic)
		return classicDefaults();
	Settings s;                                       // modern automatic defaults
	const std::string *version = detail::find(kv, "DisplayVersion");
	int v = 0;
	if (!version || !detail::parseInt(*version, v) || v < 1)
		return s;
	s.explicitSettings = true;

	int n = 0;
	if (const std::string *t = detail::find(kv, "DisplayMonitor"); t && detail::parseInt(*t, n) && n >= 0 && n < 64)
		s.monitor = n;
	int w = 0, h = 0;
	const std::string *tw = detail::find(kv, "DisplayWidth");
	const std::string *th = detail::find(kv, "DisplayHeight");
	if (tw && th && detail::parseInt(*tw, w) && detail::parseInt(*th, h) &&
	    w >= DisplayGeometry::kReferenceSize.width && h >= DisplayGeometry::kReferenceSize.height &&
	    w <= kMaxDimension && h <= kMaxDimension)
		s.resolution = {w, h};
	if (const std::string *t = detail::find(kv, "RefreshRate"); t && detail::parseInt(*t, n) && n >= 0 && n <= 1000)
		s.refreshHz = n;
	if (const std::string *t = detail::find(kv, "DisplayMode"))
	{
		if (detail::iequals(*t, "Borderless")) s.mode = WindowMode::Borderless;
		else if (detail::iequals(*t, "Exclusive")) s.mode = WindowMode::Exclusive;
		else s.mode = WindowMode::Windowed;
	}
	if (const std::string *t = detail::find(kv, "VSync"))
		s.vsync = detail::iequals(*t, "yes") || detail::iequals(*t, "true") || *t == "1";
	if (const std::string *t = detail::find(kv, "UIScale"))
	{
		if (detail::parseInt(*t, n) && n >= DisplayGeometry::kMinUiScalePercent && n <= DisplayGeometry::kMaxUiScalePercent)
			s.uiScalePercent = n;
		else
			s.uiScalePercent = DisplayGeometry::kAutoUiScale;   // "Auto" and anything invalid
	}
	if (const std::string *t = detail::find(kv, "SmoothUnitMotion"))
		s.smoothMotion = detail::iequals(*t, "yes") || detail::iequals(*t, "true") || *t == "1";
	return s;
}

// Writes the block into an existing key/value set; every other key is left exactly as it was.
inline void store(const Settings &s, KeyValues &kv)
{
	detail::put(kv, "DisplayVersion", std::to_string(kVersion));
	detail::put(kv, "DisplayMonitor", std::to_string(s.monitor));
	detail::put(kv, "DisplayWidth", std::to_string(s.resolution.valid() ? s.resolution.width : 0));
	detail::put(kv, "DisplayHeight", std::to_string(s.resolution.valid() ? s.resolution.height : 0));
	detail::put(kv, "RefreshRate", std::to_string(s.refreshHz));
	detail::put(kv, "DisplayMode", modeName(s.mode));
	detail::put(kv, "VSync", s.vsync ? "yes" : "no");
	detail::put(kv, "UIScale", s.uiScalePercent > 0 ? std::to_string(s.uiScalePercent) : std::string("Auto"));
	detail::put(kv, "SmoothUnitMotion", s.smoothMotion ? "yes" : "no");
}

// ---------------------------------------------------------------------------------------------------------------
struct Environment
{
	std::vector<DisplayModes::Monitor> monitors;
	int frameWidth = 16;        // window frame (non-client) extra size of the windowed style
	int frameHeight = 39;
};

struct Resolved
{
	int monitorIndex = 0;
	WindowMode mode = WindowMode::Windowed;   // effective (Exclusive is reported as Borderless: deferred)
	Size client;                              // window client = back buffer = logical display
	Rect monitorBounds;
	Rect monitorWork;
	int refreshHz = 0;
	bool vsync = false;
	DisplayGeometry::Layout layout;
	std::vector<std::string> notes;           // every fallback taken, for the log
};

// Largest comfortable windowed client size for a monitor, never larger than the work area minus the frame:
// the biggest standard resolution of the monitor's own aspect family that fits, else a computed size.
inline Size autoWindowedSize(const DisplayModes::Monitor &monitor, int frameW, int frameH, Size limit = {0, 0})
{
	static const Size standard[] = {
		{7680, 4320}, {5120, 2880}, {5120, 1440}, {3840, 2160}, {3840, 1600}, {3840, 1080}, {3440, 1440}, {2560, 1600},
		{2560, 1440}, {2560, 1080}, {1920, 1200}, {1920, 1080}, {1680, 1050}, {1600, 1200}, {1600, 900}, {1440, 900},
		{1366, 768}, {1280, 1024}, {1280, 960}, {1280, 800}, {1280, 720}, {1024, 768}, {800, 600},
	};
	const double aspect = monitor.bounds.height > 0
		? static_cast<double>(monitor.bounds.width) / monitor.bounds.height : 16.0 / 9.0;
	const int maxW = monitor.work.width - frameW;
	const int maxH = monitor.work.height - frameH;
	auto fits = [&](Size s) {
		return s.width <= maxW && s.height <= maxH && (!limit.valid() || (s.width <= limit.width && s.height <= limit.height));
	};
	Size best;
	long long bestArea = 0;
	for (const Size &s : standard)
	{
		const double ratio = static_cast<double>(s.width) / s.height;
		if (std::fabs(ratio - aspect) / aspect > 0.02 || !fits(s)) continue;
		const long long area = static_cast<long long>(s.width) * s.height;
		if (area > bestArea) { best = s; bestArea = area; }
	}
	if (best.valid())
		return best;
	// Unusual aspect: derive a size from the largest fitting height.
	for (int h = maxH; h >= 600; h -= 8)
	{
		int w = static_cast<int>(h * aspect + 0.5) / 8 * 8;
		if (w >= 800 && fits({w, h})) return {w, h};
	}
	return DisplayGeometry::kReferenceSize;
}

inline Resolved resolve(const Settings &s, const Environment &env, Size fallbackDesktop = {1920, 1080})
{
	Resolved r;
	DisplayModes::Monitor monitor;
	if (env.monitors.empty())
	{
		monitor.bounds = {0, 0, fallbackDesktop.width, fallbackDesktop.height};
		monitor.work = monitor.bounds;
		monitor.primary = true;
		monitor.desktop.width = fallbackDesktop.width;
		monitor.desktop.height = fallbackDesktop.height;
		r.notes.push_back("no monitors enumerated; using desktop metrics");
	}
	else
	{
		int index = s.monitor;
		if (index < 0 || index >= static_cast<int>(env.monitors.size()))
		{
			r.notes.push_back("monitor " + std::to_string(index) + " not attached; using primary");
			index = 0;
			for (size_t i = 0; i < env.monitors.size(); ++i)
				if (env.monitors[i].primary) { index = static_cast<int>(i); break; }
		}
		r.monitorIndex = index;
		monitor = env.monitors[static_cast<size_t>(index)];
	}
	r.monitorBounds = monitor.bounds;
	r.monitorWork = monitor.work;
	r.vsync = s.vsync;

	r.mode = s.mode;
	if (r.mode == WindowMode::Exclusive)
	{
		r.mode = WindowMode::Borderless;
		r.notes.push_back("exclusive fullscreen is deferred; using borderless");
	}

	if (r.mode == WindowMode::Borderless)
	{
		r.client = {monitor.bounds.width, monitor.bounds.height};
		if (s.resolution.valid() && s.resolution != r.client)
			r.notes.push_back("borderless uses the desktop resolution; stored size ignored");
		r.refreshHz = monitor.desktop.refreshHz;
	}
	else
	{
		const int maxW = monitor.work.width - env.frameWidth;
		const int maxH = monitor.work.height - env.frameHeight;
		if (s.resolution.valid() && s.resolution.width <= maxW && s.resolution.height <= maxH)
			r.client = s.resolution;
		else
		{
			if (s.resolution.valid())
				r.notes.push_back("windowed size " + DisplayModes::resolutionLabel(s.resolution) +
				                  " does not fit the work area; reduced");
			r.client = autoWindowedSize(monitor, env.frameWidth, env.frameHeight,
			                            s.resolution.valid() ? s.resolution : Size{0, 0});
			if (!s.resolution.valid() && r.client.width == 0) r.client = DisplayGeometry::kReferenceSize;
		}
		r.refreshHz = monitor.desktop.refreshHz;
	}
	r.layout = DisplayGeometry::computeLayout(r.client, s.uiScalePercent);
	return r;
}

}  // namespace DisplaySettings
