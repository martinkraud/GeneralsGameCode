// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Pure, engine-independent helpers for the AMD64 ordinary-host profile
// (-nativeOrdinaryHost). Deliberately usable from tests without engine state.
#include <windows.h>

namespace NativeOrdinaryHost {

constexpr int ClientWidth = 800;
constexpr int ClientHeight = 600;

// Same style bits the legacy windowed initializeAppWindows path selects.
constexpr DWORD WindowedStyle = WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX | WS_SYSMENU | WS_DLGFRAME | WS_CAPTION;

struct WindowGeometry {
    DWORD style;
    int x, y, width, height; // outer rectangle
};

// Legacy centering (client-width based) with the outer size from AdjustWindowRect.
inline bool computeWindowGeometry(int screenWidth, int screenHeight, WindowGeometry &out) {
    RECT rect = { 0, 0, ClientWidth, ClientHeight };
    if (!AdjustWindowRect(&rect, WindowedStyle, FALSE)) return false;
    out.style = WindowedStyle;
    out.x = screenWidth / 2 - ClientWidth / 2;
    out.y = screenHeight / 2 - ClientHeight / 2;
    out.width = rect.right - rect.left;
    out.height = rect.bottom - rect.top;
    return true;
}

// The modern presentation is a fixed-size swap chain; the client must be exact.
inline bool clientIsExact(HWND window) {
    RECT client = {};
    return window && GetClientRect(window, &client) &&
        client.left == 0 && client.top == 0 &&
        client.right == ClientWidth && client.bottom == ClientHeight;
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
