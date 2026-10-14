// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Pure, engine-independent helpers for the AMD64 ordinary-host profile
// (-nativeOrdinaryHost). Deliberately usable from tests without engine state.
#include <windows.h>
#include <cstdlib>
#include "Common/DisplayRuntime.h"

namespace NativeOrdinaryHost {

constexpr int ClientWidth = 800;
constexpr int ClientHeight = 600;

// Developer-review presentation scale (play profile only, percent). The logical display, the UI layout and the
// input coordinates stay exactly 800x600; only the window (and the stretched swap chain) is larger so a visual
// review is practical on a 1080p display. Isolated from the later resolution/UI-scaling architecture.
inline int &activeScalePercent() { static int percent = 100; return percent; }

// GENERALS_NATIVE_REVIEW_SCALE: percent ("150") or factor ("1.5"); default 150 (1200x900 client, the largest
// 4:3 size that fits a 1080p desktop with its title bar and taskbar). Clamped to the work area, never below 100.
inline int chooseReviewScalePercent(int workWidth, int workHeight, int frameWidth, int frameHeight) {
    int percent = 150;
    if (const char *text = std::getenv("GENERALS_NATIVE_REVIEW_SCALE")) {
        const double value = std::atof(text);
        if (value > 0.0) percent = static_cast<int>(value < 10.0 ? value * 100.0 + 0.5 : value + 0.5);
    }
    if (percent < 100) percent = 100;
    if (percent > 300) percent = 300;
    // Fit: outer window = client * percent + frame.
    const int fitW = (workWidth - frameWidth) * 100 / 800;
    const int fitH = (workHeight - frameHeight) * 100 / 600;
    const int fit = fitW < fitH ? fitW : fitH;
    if (fit < percent) percent = fit;
    return percent < 100 ? 100 : percent;
}

constexpr int scaledSize(int logical, int percent) { return (logical * percent + 50) / 100; }

// Same style bits the legacy windowed initializeAppWindows path selects.
constexpr DWORD WindowedStyle = WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX | WS_SYSMENU | WS_DLGFRAME | WS_CAPTION;

struct WindowGeometry {
    DWORD style;
    int x, y, width, height; // outer rectangle
};

// Legacy centering (client-width based) with the outer size from AdjustWindowRect. `percent` != 100 is the
// developer-review scale of the client area.
inline bool computeWindowGeometry(int screenWidth, int screenHeight, WindowGeometry &out, int percent = 100) {
    const int clientW = scaledSize(ClientWidth, percent), clientH = scaledSize(ClientHeight, percent);
    RECT rect = { 0, 0, clientW, clientH };
    if (!AdjustWindowRect(&rect, WindowedStyle, FALSE)) return false;
    out.style = WindowedStyle;
    out.x = screenWidth / 2 - clientW / 2;
    out.y = screenHeight / 2 - clientH / 2;
    if (out.y < 0) out.y = 0;
    out.width = rect.right - rect.left;
    out.height = rect.bottom - rect.top;
    return true;
}

// The modern presentation is a fixed-size swap chain; the client must be exact (800x600, or its review
// scale, which DXGI stretches).
inline bool clientIsExact(HWND window) {
    RECT client = {};
    // Modern Experience (X64.33): the client is the configured display size, the swap chain is not stretched.
    const DisplayGeometry::Size modern = DisplayRuntime::expectedClient();
    if (modern.valid())
        return window && GetClientRect(window, &client) &&
            client.left == 0 && client.top == 0 && client.right == modern.width && client.bottom == modern.height;
    const int percent = activeScalePercent();
    return window && GetClientRect(window, &client) &&
        client.left == 0 && client.top == 0 &&
        client.right == scaledSize(ClientWidth, percent) && client.bottom == scaledSize(ClientHeight, percent);
}

// Window coordinates -> logical 800x600 coordinates (identity at 100 %).
inline void clientToLogical(long &x, long &y) {
    const int percent = activeScalePercent();
    if (percent == 100) return;
    x = x * 100 / percent;
    y = y * 100 / percent;
}

struct ModuleAudit { bool d3d8, d3dx8, d3d11, dxgi; };

// Loaded-module audit of this process. A loaded D3D8 module means the legacy
// device path was reached and the profile contract is violated.
inline ModuleAudit auditLoadedModules() {
    ModuleAudit audit = {};
    audit.d3d8 = GetModuleHandleW(L"d3d8.dll") != nullptr || GetModuleHandleW(L"d3d8thk.dll") != nullptr;
    audit.d3dx8 = GetModuleHandleW(L"d3dx8.dll") != nullptr;
    audit.d3d11 = GetModuleHandleW(L"d3d11.dll") != nullptr;
    audit.dxgi = GetModuleHandleW(L"dxgi.dll") != nullptr;
    return audit;
}
inline bool auditAcceptable(const ModuleAudit &audit) { return !audit.d3d8 && !audit.d3dx8; }

// Release-order contract: logical UI texture residency must be gone before the
// modern backend (device) is destroyed.
inline bool residencyReleasedBeforeBackend(unsigned liveResidencies) { return liveResidencies == 0; }

} // namespace NativeOrdinaryHost
