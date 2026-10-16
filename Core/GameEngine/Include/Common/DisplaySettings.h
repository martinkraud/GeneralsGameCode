// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33: persisted display settings (Options.ini keys), their migration from pre-X64.33 preferences and the pure
// resolution of "what the user asked for" into "what the window/back buffer will actually be" for a given set of
// attached monitors. Engine-free: usable from tests, WinMain (before any engine singleton exists) and the Options UI.
//
// Keys written (all optional on read; unknown keys in the file are never touched):
//   DisplayVersion  1         marks preferences written by a build that understands this block
//   DisplayMonitor  0..n      monitor index in DisplayModes::enumerateMonitors() order (0 = primary)
//   DisplayMonitorName text   stable Windows device name of the monitor (e.g. \\.\DISPLAY2); preferred over the index
//   DisplayWidth/Height       client/back-buffer size; absent or 0 = automatic
//   RefreshRate     Hz or 0   Borderless/Exclusive only; 0 = desktop rate
//   DisplayMode     Windowed | Borderless | Exclusive
//   VSync           yes | no
//   UIScale         Auto | 50..400
//   SmoothUnitMotion yes | no ground interpolation (Modern only; see GroundTranslation)
//   RenderScale     100|90|80|75|67|50   internal 3D resolution in percent of the presentation size (WorldRenderScale; default 100)
//   InterfaceStyle  Original | Enhanced   gameplay HUD presentation (Enhanced = USA prototype; see EnhancedHud.h)
// The legacy `Resolution`, `FrameRateLimit` and `FPSLimit` keys keep their meaning (frame pacing is owned by the
// existing FramePacer; nothing here limits frames).
#pragma once

#include "Common/DisplayGeometry.h"
#include "Common/DisplayModes.h"
#include "WW3D2/Backend/WorldRenderScale.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace DisplayConfig
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
	std::string monitorName;                 // stable device name; empty = use the index
	Size resolution;                         // {0,0} = automatic
	int refreshHz = 0;
	WindowMode mode = WindowMode::Windowed;
	bool vsync = false;
	int uiScalePercent = DisplayGeometry::kAutoUiScale;
	bool smoothMotion = false;
	bool enhancedHud = false;                // Options > Gameplay > Interface: Original (default) | Enhanced
	unsigned renderScalePercent = 100;       // X64.37 v3: internal 3D resolution (WorldRenderScale::kChoices); 100 = native

	bool operator==(const Settings &o) const
	{
		return explicitSettings == o.explicitSettings && monitor == o.monitor && monitorName == o.monitorName && resolution == o.resolution &&
		       refreshHz == o.refreshHz && mode == o.mode && vsync == o.vsync &&
		       uiScalePercent == o.uiScalePercent && smoothMotion == o.smoothMotion && enhancedHud == o.enhancedHud &&
		       renderScalePercent == o.renderScalePercent;
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
	if (const std::string *t = detail::find(kv, "DisplayMonitorName"); t && t->size() < 64)
		s.monitorName = *t;
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
	if (const std::string *t = detail::find(kv, "InterfaceStyle"))
		s.enhancedHud = detail::iequals(*t, "Enhanced");
	if (const std::string *t = detail::find(kv, "RenderScale"))
		s.renderScalePercent = detail::parseInt(*t, n) ? WorldRenderScale::sanitize(n) : WorldRenderScale::kDefaultPercent;   // invalid = 100
	return s;
}

// Writes the block into an existing key/value set; every other key is left exactly as it was.
inline void store(const Settings &s, KeyValues &kv)
{
	detail::put(kv, "DisplayVersion", std::to_string(kVersion));
	detail::put(kv, "DisplayMonitor", std::to_string(s.monitor));
	detail::put(kv, "DisplayMonitorName", s.monitorName);
	detail::put(kv, "DisplayWidth", std::to_string(s.resolution.valid() ? s.resolution.width : 0));
	detail::put(kv, "DisplayHeight", std::to_string(s.resolution.valid() ? s.resolution.height : 0));
	detail::put(kv, "RefreshRate", std::to_string(s.refreshHz));
	detail::put(kv, "DisplayMode", modeName(s.mode));
	detail::put(kv, "VSync", s.vsync ? "yes" : "no");
	detail::put(kv, "UIScale", s.uiScalePercent > 0 ? std::to_string(s.uiScalePercent) : std::string("Auto"));
	detail::put(kv, "SmoothUnitMotion", s.smoothMotion ? "yes" : "no");
	detail::put(kv, "InterfaceStyle", s.enhancedHud ? "Enhanced" : "Original");
	detail::put(kv, "RenderScale", std::to_string(WorldRenderScale::sanitize(static_cast<int>(s.renderScalePercent))));
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
	std::string monitorName;                  // device name of the monitor that was actually chosen
	WindowMode mode = WindowMode::Windowed;   // effective (Exclusive is reported as Borderless: deferred)
	Size client;                              // window client = back buffer = logical display
	Rect monitorBounds;
	Rect monitorWork;
	int refreshHz = 0;
	bool vsync = false;
	DisplayGeometry::Layout layout;
	std::vector<std::string> notes;           // every fallback taken, for the log
};

// Stable identity of a monitor: its Windows device name (\\.\DISPLAYn is assigned per output and survives a reordering of
// the enumeration, unlike the index). ASCII by construction; anything else is replaced.
inline std::string monitorId(const DisplayModes::Monitor &m)
{
	std::string id;
	for (wchar_t c : m.deviceName)
		id.push_back(c > 0 && c < 128 ? static_cast<char>(c) : '?');
	return id;
}

// Chooses the monitor: by stable name when one is stored and attached; otherwise by index when no name is stored; otherwise (the
// stored monitor was disconnected, or the index is out of range) the primary monitor. Every fallback is reported in `notes`.
inline int chooseMonitor(const std::vector<DisplayModes::Monitor> &monitors, const std::string &name, int index,
                         std::vector<std::string> *notes = nullptr)
{
	if (monitors.empty()) return 0;
	if (!name.empty())
	{
		for (size_t i = 0; i < monitors.size(); ++i)
			if (monitorId(monitors[i]) == name) return static_cast<int>(i);
		if (notes) notes->push_back("monitor " + name + " is not attached; using primary");
	}
	else if (index >= 0 && index < static_cast<int>(monitors.size()))
		return index;
	else if (notes)
		notes->push_back("monitor " + std::to_string(index) + " not attached; using primary");
	for (size_t i = 0; i < monitors.size(); ++i)
		if (monitors[i].primary) return static_cast<int>(i);
	return 0;
}

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

// Sizes offered in the Resolution list. Windowed: standard sizes (and sizes the driver reports) that fit the
// work area, the current size always included. Borderless/Exclusive: the desktop size only (no fake modes).
inline std::vector<Size> sizeChoices(const DisplayModes::Monitor &monitor, WindowMode mode, int frameW, int frameH,
                                     Size current = {0, 0})
{
	std::vector<Size> out;
	auto add = [&](Size s) {
		if (!s.valid()) return;
		for (const Size &e : out) if (e == s) return;
		out.push_back(s);
	};
	if (mode != WindowMode::Windowed)
	{
		add({monitor.bounds.width, monitor.bounds.height});
		return out;
	}
	// 4:3, 5:4, 16:10, 16:9, 21:9 and 32:9 families (and the sizes between that desktops commonly use).
	static const Size standard[] = {
		{800, 600}, {1024, 768}, {1152, 864}, {1280, 720}, {1280, 800}, {1280, 960}, {1280, 1024}, {1360, 768}, {1366, 768},
		{1440, 900}, {1600, 900}, {1600, 1200}, {1680, 1050}, {1920, 1080}, {1920, 1200}, {2048, 1152}, {2560, 1080},
		{2560, 1440}, {2560, 1600}, {2880, 1620}, {3200, 1800}, {3440, 1440}, {3840, 1080}, {3840, 1600}, {3840, 2160},
		{5120, 1440}, {5120, 2160}, {5120, 2880}, {7680, 2160}, {7680, 4320},
	};
	const int maxW = monitor.work.width - frameW, maxH = monitor.work.height - frameH;
	for (const Size &s : standard)
		if (s.width <= maxW && s.height <= maxH) add(s);
	for (const Size &s : DisplayModes::resolutionsOf(monitor.modes))
		if (s.width <= maxW && s.height <= maxH) add(s);
	add(current);
	std::sort(out.begin(), out.end(), [](const Size &a, const Size &b) {
		return a.width != b.width ? a.width < b.width : a.height < b.height;
	});
	return out;
}

// "1920 x 1080  (16:9)" - the aspect family makes a long list of modern sizes readable at a glance.
inline std::string resolutionChoiceLabel(Size s)
{
	return std::to_string(s.width) + " x " + std::to_string(s.height) + "  (" + DisplayModes::aspectFamily(s) + ")";
}

// ---- command-line overrides (session only) -------------------------------------------------------------------------------
// Precedence, highest first: (1) command line for THIS launch (-xres/-yres together, -win, -fullscreen), (2) the stored
// Options.ini values, (3) automatic defaults. An override is never written back by itself: the stored values stay as they were
// until the player changes that field in Options and keeps the change. `-fullscreen` means Borderless here: this renderer does
// not offer exclusive fullscreen (see SETTINGS_COMPLETION_AUDIT.md).
struct SessionOverrides
{
	Size resolution;                 // {0,0} = none
	bool windowed = false;           // -win
	bool fullscreen = false;         // -fullscreen
	bool any() const { return resolution.valid() || windowed || fullscreen; }
};

inline Settings applyOverrides(Settings s, const SessionOverrides &o)
{
	if (o.windowed) s.mode = WindowMode::Windowed;
	else if (o.fullscreen) s.mode = WindowMode::Borderless;
	if (o.resolution.valid() && o.resolution.width >= DisplayGeometry::kReferenceSize.width &&
	    o.resolution.height >= DisplayGeometry::kReferenceSize.height && o.resolution.width <= kMaxDimension &&
	    o.resolution.height <= kMaxDimension)
		s.resolution = o.resolution;
	return s;
}

// ---- what to persist after the player pressed Accept ------------------------------------------------------------------------
// `stored` = the values in Options.ini, `effective` = what is running (stored + session overrides), `requested` = what the
// controls now say. Only fields the player actually changed (requested != effective) are persisted; a field they did not touch
// keeps its stored value, so a command-line override never leaks into the file. Fields that can make the game unusable (size,
// mode, monitor, UI scale) are "risky": they are persisted only after the player keeps them (DisplayTransaction), everything else
// (VSync, unit motion, interface) is persisted at once.
struct PersistPlan
{
	Settings immediate;              // write now
	Settings afterKeep;              // write when the risky change is kept (== immediate when nothing risky changed)
	bool riskyChanged = false;       // a Keep/Revert confirmation is needed
};

inline bool sameMonitor(const Settings &a, const Settings &b)
{
	if (!a.monitorName.empty() && !b.monitorName.empty()) return a.monitorName == b.monitorName;
	return a.monitor == b.monitor;
}

inline PersistPlan planPersistence(const Settings &stored, const Settings &effective, const Settings &requested)
{
	PersistPlan plan;
	Settings now = stored;
	now.explicitSettings = true;
	if (requested.vsync != effective.vsync) now.vsync = requested.vsync;
	if (requested.smoothMotion != effective.smoothMotion) now.smoothMotion = requested.smoothMotion;
	if (requested.enhancedHud != effective.enhancedHud) now.enhancedHud = requested.enhancedHud;
	if (requested.renderScalePercent != effective.renderScalePercent) now.renderScalePercent = requested.renderScalePercent;
	Settings later = now;
	const bool sizeChanged = requested.resolution != effective.resolution;
	const bool modeChanged = requested.mode != effective.mode;
	const bool monitorChanged = !sameMonitor(requested, effective);
	const bool scaleChanged = requested.uiScalePercent != effective.uiScalePercent;
	if (sizeChanged) later.resolution = requested.resolution;
	if (modeChanged) later.mode = requested.mode;
	if (monitorChanged) { later.monitor = requested.monitor; later.monitorName = requested.monitorName; }
	if (scaleChanged) later.uiScalePercent = requested.uiScalePercent;
	plan.riskyChanged = sizeChanged || modeChanged || monitorChanged || scaleChanged;
	plan.immediate = now;
	plan.afterKeep = later;
	return plan;
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
		const int index = chooseMonitor(env.monitors, s.monitorName, s.monitor, &r.notes);
		r.monitorIndex = index;
		monitor = env.monitors[static_cast<size_t>(index)];
		r.monitorName = monitorId(monitor);
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

}  // namespace DisplayConfig
