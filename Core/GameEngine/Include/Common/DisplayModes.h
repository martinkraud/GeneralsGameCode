// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33: display / monitor / mode enumeration (Win32 desktop APIs, GPU independent) and the pure filtering that
// turns the raw driver list into the list the player can pick from. Nothing is invented: every offered mode was
// reported by EnumDisplaySettingsEx for that monitor.
#pragma once

#include "Common/DisplayGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace DisplayModes
{
using DisplayGeometry::Rect;
using DisplayGeometry::Size;

struct Mode
{
	int width = 0;
	int height = 0;
	int refreshHz = 0;       // canonical (see canonicalRefreshHz); 0 = driver default / unknown
	int bitsPerPixel = 32;
	bool interlaced = false;

	Size size() const { return {width, height}; }
	bool operator==(const Mode &o) const { return width == o.width && height == o.height && refreshHz == o.refreshHz; }
};

struct Monitor
{
	std::wstring deviceName;     // \\.\DISPLAYn
	std::wstring description;    // adapter/monitor string reported by the driver
	Rect bounds;                 // virtual-desktop rectangle
	Rect work;                   // without the taskbar
	bool primary = false;
	Mode desktop;                // current desktop mode
	std::vector<Mode> modes;     // filtered, sorted, de-duplicated
};

// Windows commonly reports NTSC-family rates one below the nominal value (59 for 59.94, 119 for 119.88 ...) and
// sometimes the nominal value for the same mode. Both spellings are one mode for a player.
inline int canonicalRefreshHz(int reported)
{
	switch (reported)
	{
	case 23: return 24;
	case 29: return 30;
	case 59: return 60;
	case 74: return 75;
	case 89: return 90;
	case 119: return 120;
	case 143: return 144;
	case 164: return 165;
	case 239: return 240;
	default: return reported <= 1 ? 0 : reported;   // 0 and 1 mean "hardware default" in DEVMODE
	}
}

inline bool modeLess(const Mode &a, const Mode &b)
{
	if (a.width != b.width) return a.width < b.width;
	if (a.height != b.height) return a.height < b.height;
	return a.refreshHz < b.refreshHz;
}

// Keep 24/32 bpp, progressive, at least `minimum`; canonicalize refresh; drop duplicates; sort ascending.
inline std::vector<Mode> filterModes(const std::vector<Mode> &raw, Size minimum = DisplayGeometry::kReferenceSize)
{
	std::vector<Mode> out;
	for (Mode m : raw)
	{
		if (m.width < minimum.width || m.height < minimum.height) continue;
		if (m.bitsPerPixel < 24) continue;
		if (m.interlaced) continue;
		m.refreshHz = canonicalRefreshHz(m.refreshHz);
		m.bitsPerPixel = 32;
		out.push_back(m);
	}
	std::sort(out.begin(), out.end(), modeLess);
	out.erase(std::unique(out.begin(), out.end()), out.end());
	return out;
}

inline std::vector<Size> resolutionsOf(const std::vector<Mode> &modes)
{
	std::vector<Size> out;
	for (const Mode &m : modes)
		if (out.empty() || out.back() != m.size())
			out.push_back(m.size());
	return out;
}

inline std::vector<int> refreshRatesFor(const std::vector<Mode> &modes, Size size)
{
	std::vector<int> out;
	for (const Mode &m : modes)
		if (m.size() == size && std::find(out.begin(), out.end(), m.refreshHz) == out.end())
			out.push_back(m.refreshHz);
	std::sort(out.begin(), out.end());
	return out;
}

// Highest refresh at a size (0 when the size is not offered).
inline int bestRefreshFor(const std::vector<Mode> &modes, Size size)
{
	const std::vector<int> rates = refreshRatesFor(modes, size);
	return rates.empty() ? 0 : rates.back();
}

inline bool offers(const std::vector<Mode> &modes, Size size, int refreshHz = 0)
{
	for (const Mode &m : modes)
		if (m.size() == size && (refreshHz == 0 || m.refreshHz == refreshHz))
			return true;
	return false;
}

// Nearest offered resolution (by area, ties to the larger); invalid when the list is empty.
inline Size nearestResolution(const std::vector<Mode> &modes, Size wanted)
{
	Size best;
	long long bestDelta = -1;
	for (const Size &s : resolutionsOf(modes))
	{
		const long long delta = std::llabs(static_cast<long long>(s.width) * s.height -
		                                   static_cast<long long>(wanted.width) * wanted.height);
		if (bestDelta < 0 || delta < bestDelta || (delta == bestDelta && s.width > best.width))
		{
			best = s;
			bestDelta = delta;
		}
	}
	return best;
}

// "1920x1080" / "59.94 Hz" style labels used by the Options UI and the logs.
inline std::string resolutionLabel(Size size)
{
	return std::to_string(size.width) + "x" + std::to_string(size.height);
}

inline std::string refreshLabel(int hz)
{
	return hz > 0 ? std::to_string(hz) + " Hz" : std::string("Auto");
}

// Aspect-ratio family name for a size ("16:9", "21:9" ...), by nearest standard ratio within 2 %.
inline const char *aspectFamily(Size size)
{
	if (!size.valid()) return "?";
	struct Family { double ratio; const char *name; };
	static const Family families[] = {
		{5.0 / 4.0, "5:4"}, {4.0 / 3.0, "4:3"}, {3.0 / 2.0, "3:2"}, {16.0 / 10.0, "16:10"}, {16.0 / 9.0, "16:9"},
		{64.0 / 27.0, "21:9"}, {43.0 / 18.0, "21:9"}, {21.0 / 9.0, "21:9"}, {32.0 / 9.0, "32:9"}, {32.0 / 10.0, "32:10"},
	};
	const double ratio = static_cast<double>(size.width) / size.height;
	for (const Family &f : families)
		if (std::fabs(ratio - f.ratio) / f.ratio < 0.02)
			return f.name;
	return "other";
}

}  // namespace DisplayModes

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace DisplayModes
{

namespace detail
{
struct EnumContext
{
	std::vector<Monitor> *monitors;
};

inline BOOL CALLBACK enumMonitorProc(HMONITOR monitor, HDC, LPRECT, LPARAM param)
{
	auto *context = reinterpret_cast<EnumContext *>(param);
	MONITORINFOEXW info = {};
	info.cbSize = sizeof(info);
	if (!GetMonitorInfoW(monitor, &info))
		return TRUE;
	Monitor m;
	m.deviceName = info.szDevice;
	m.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
	m.bounds = {info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right - info.rcMonitor.left,
	            info.rcMonitor.bottom - info.rcMonitor.top};
	m.work = {info.rcWork.left, info.rcWork.top, info.rcWork.right - info.rcWork.left,
	          info.rcWork.bottom - info.rcWork.top};

	DISPLAY_DEVICEW device = {};
	device.cb = sizeof(device);
	if (EnumDisplayDevicesW(info.szDevice, 0, &device, 0))
		m.description = device.DeviceString;

	DEVMODEW current = {};
	current.dmSize = sizeof(current);
	if (EnumDisplaySettingsExW(info.szDevice, ENUM_CURRENT_SETTINGS, &current, 0))
	{
		m.desktop.width = static_cast<int>(current.dmPelsWidth);
		m.desktop.height = static_cast<int>(current.dmPelsHeight);
		m.desktop.refreshHz = canonicalRefreshHz(static_cast<int>(current.dmDisplayFrequency));
		m.desktop.bitsPerPixel = static_cast<int>(current.dmBitsPerPel);
	}
	else
	{
		m.desktop.width = m.bounds.width;
		m.desktop.height = m.bounds.height;
	}

	std::vector<Mode> raw;
	for (DWORD i = 0;; ++i)
	{
		DEVMODEW dm = {};
		dm.dmSize = sizeof(dm);
		if (!EnumDisplaySettingsExW(info.szDevice, i, &dm, 0))
			break;
		Mode mode;
		mode.width = static_cast<int>(dm.dmPelsWidth);
		mode.height = static_cast<int>(dm.dmPelsHeight);
		mode.refreshHz = static_cast<int>(dm.dmDisplayFrequency);
		mode.bitsPerPixel = static_cast<int>(dm.dmBitsPerPel);
		mode.interlaced = (dm.dmDisplayFlags & DM_INTERLACED) != 0;
		raw.push_back(mode);
	}
	m.modes = filterModes(raw);
	context->monitors->push_back(m);
	return TRUE;
}
}  // namespace detail

// All attached monitors, primary first, then by virtual-desktop position.
inline std::vector<Monitor> enumerateMonitors()
{
	std::vector<Monitor> monitors;
	detail::EnumContext context = {&monitors};
	EnumDisplayMonitors(nullptr, nullptr, detail::enumMonitorProc, reinterpret_cast<LPARAM>(&context));
	std::stable_sort(monitors.begin(), monitors.end(), [](const Monitor &a, const Monitor &b) {
		if (a.primary != b.primary) return a.primary;
		if (a.bounds.x != b.bounds.x) return a.bounds.x < b.bounds.x;
		return a.bounds.y < b.bounds.y;
	});
	return monitors;
}

}  // namespace DisplayModes
#endif
