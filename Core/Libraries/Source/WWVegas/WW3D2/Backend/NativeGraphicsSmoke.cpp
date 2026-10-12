// SPDX-License-Identifier: GPL-3.0-or-later
#include "NativeGraphicsSmoke.h"
#include "D3D11Backend.h"
#include "WWMath/vector3.h"
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <stdexcept>
#include <string>

namespace NativeGraphicsSmoke {
namespace {
struct WindowState { bool abort = false, finishing = false; DWORD error = 0; };
LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM w, LPARAM l) {
    auto *state = reinterpret_cast<WindowState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = static_cast<WindowState *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
        SetLastError(0);
        if (!SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state)) && GetLastError()) return FALSE;
    }
    if (state) {
        if (message == WM_CLOSE || (message == WM_KEYDOWN && w == VK_ESCAPE)) { state->abort = true; return 0; }
        if (message == WM_SYSKEYDOWN && w == VK_RETURN) return 0; // fixed windowed gate
        if (message == WM_DESTROY && !state->finishing) state->abort = true;
        if (message == WM_PAINT) {
            PAINTSTRUCT paint{};
            if (!BeginPaint(window, &paint)) { state->error = GetLastError(); state->abort = true; }
            else if (!EndPaint(window, &paint)) { state->error = GetLastError(); state->abort = true; }
            return 0;
        }
    }
    return DefWindowProcW(window, message, w, l);
}
void Require(BOOL ok, const char *operation) {
    if (!ok) {
        const DWORD code = GetLastError();
        char text[128]; std::snprintf(text, sizeof(text), "%s GetLastError=0x%08X", operation, code);
        throw std::runtime_error(text);
    }
}
}
int Run(const Configuration &config) {
    auto report = [&](const char *stage, const char *status, const char *detail = "", unsigned code = 0) {
        if (config.sink) config.sink(config.context, stage, status, detail, code);
    };
    if (!config.hold_ms || config.hold_ms > 5000 || !config.max_frames || config.max_frames > 300) {
        report("graphics_host", "fail", "invalid bounded configuration", 1); return 1;
    }
    WindowState state;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    wchar_t name[96]; std::swprintf(name, 96, L"NativeGraphicsSmoke-%lu-%lu", GetCurrentProcessId(), GetCurrentThreadId());
    ATOM registered = 0; HWND window = nullptr;
    std::unique_ptr<D3D11Backend> backend;
    const char *phase = "graphics_host";
    unsigned frames = 0, visibleFrames = 0, occludedFrames = 0;
    unsigned elapsed = 0;
    int result = 1;
    report("graphics_host", "begin", "headless subsystems; execute/replay/simulation skipped");
    try {
        phase = "graphics_class"; report(phase, "begin");
        WNDCLASSW wc{}; wc.lpfnWndProc = Procedure; wc.hInstance = instance;
        wc.lpszClassName = name; wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        Require(wc.hCursor != nullptr, "LoadCursor");
        registered = RegisterClassW(&wc); Require(registered != 0, "RegisterClass"); report(phase, "complete");
        phase = "graphics_window"; report(phase, "begin");
        const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT size{0, 0, 800, 600}; Require(AdjustWindowRect(&size, style, FALSE), "AdjustWindowRect");
        window = CreateWindowExW(0, name, L"Native AMD64 D3D11 smoke - no gameplay", style,
            CW_USEDEFAULT, CW_USEDEFAULT, size.right-size.left, size.bottom-size.top,
            nullptr, nullptr, instance, &state);
        Require(window != nullptr, "CreateWindowEx");
        RECT client{}; Require(GetClientRect(window, &client), "GetClientRect");
        if (client.right != 800 || client.bottom != 600) throw std::runtime_error("Unexpected client dimensions");
        if (config.visible) ShowWindow(window, SW_SHOWNORMAL); // return is previous visibility, not an error status
        Require(UpdateWindow(window), "UpdateWindow");
        report(phase, "complete", config.visible ? "client=800x600 visible=1" : "client=800x600 visible=0 test-only");
        phase = "graphics_backend"; report(phase, "begin");
        D3D11Backend::Configuration settings;
        settings.window = window; settings.width = 800; settings.height = 600;
        settings.software = config.software; settings.diagnostic_windowed = true;
        settings.diagnostic = config.sink; settings.diagnostic_context = config.context;
        backend = D3D11Backend::Create(settings);
        report(phase, "complete");
        const ULONGLONG start = GetTickCount64();
        bool lastVisible = false;
        report("graphics_messages", "begin");
        for (;;) {
            phase = "graphics_messages";
            MSG message{}; unsigned messages = 0;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (++messages > 256) throw std::runtime_error("Message burst exceeds bound");
                if (message.message == WM_QUIT) state.abort = true;
                else { TranslateMessage(&message); DispatchMessageW(&message); }
            }
            if (state.error) { char text[96]; std::snprintf(text, sizeof(text), "WndProc error=0x%08X", state.error); throw std::runtime_error(text); }
            Require(GetClientRect(window, &client), "GetClientRect during pump");
            if (!IsIconic(window) && (client.right != 800 || client.bottom != 600))
                throw std::runtime_error("Unsupported diagnostic resize");
            elapsed = static_cast<unsigned>(GetTickCount64()-start);
            const bool visible = IsWindowVisible(window) && !IsIconic(window);
            const auto decision = Evaluate(elapsed, frames, visibleFrames, visible && lastVisible,
                state.abort, config.hold_ms, config.max_frames);
            if (decision != Decision::Continue) {
                result = decision == Decision::Success ? 0 : 1;
                if (result) throw std::runtime_error(state.abort ? "Close/Escape/quit abort" : "No visible Present or frame/time bound reached");
                break;
            }
            phase = "graphics_viewport";
            backend->Set_Viewport({0, 0, 800, 600, 0, 1});
            phase = "graphics_clear";
            if (!frames) report(phase, "begin", "RGBA=(0.08,0.32,0.55,1)");
            backend->Begin_Scene(); backend->Clear(true, false, Vector3(0.08f, 0.32f, 0.55f), 1);
            if (!frames) report(phase, "complete");
            phase = "graphics_present";
            if (!frames) report(phase, "begin");
            backend->End_Scene(true);
            const auto present = backend->Last_Presentation();
            ++frames; if (present.occluded) ++occludedFrames;
            lastVisible = visible && !present.occluded && present.result == 0;
            if (lastVisible) ++visibleFrames;
            if (frames == 1 || (lastVisible && visibleFrames == 1)) {
                char detail[160]; std::snprintf(detail, sizeof(detail),
                    "HRESULT=0x%08X visible=%u occluded=%u removed=0x%08X frame=%u",
                    present.result, visible ? 1u : 0u, present.occluded ? 1u : 0u, present.removed_reason, frames);
                report(phase, lastVisible ? "complete" : "observed", detail, present.result);
            }
            Sleep(20);
        }
        report("graphics_messages", "complete");
    } catch (const std::exception &error) {
        char detail[768]; const auto present = backend ? backend->Last_Presentation() : D3D11Backend::Presentation{};
        std::snprintf(detail, sizeof(detail), "%s Present=0x%08X removed=0x%08X", error.what(), present.result, present.removed_reason);
        report(phase, "fail", detail, 1); result = 1;
    } catch (...) { report(phase, "fail", "unknown exception", 1); result = 1; }
    // Explicit checked cleanup on every path; backend/context/resources precede HWND.
    report("graphics_backend_release", "begin"); backend.reset(); report("graphics_backend_release", "complete");
    phase = "graphics_window_release"; report(phase, "begin");
    state.finishing = true;
    if (window && !DestroyWindow(window)) { report(phase, "fail", "DestroyWindow", GetLastError()); result = 1; }
    else report(phase, "complete");
    phase = "graphics_class_release"; report(phase, "begin");
    if (registered && !UnregisterClassW(name, instance)) { report(phase, "fail", "UnregisterClass", GetLastError()); result = 1; }
    else report(phase, "complete");
    char detail[192]; std::snprintf(detail, sizeof(detail),
        "frames=%u visible_frames=%u occluded_frames=%u elapsed_ms=%u driver=%s",
        frames, visibleFrames, occludedFrames, elapsed, config.software ? "WARP" : "hardware");
    report("graphics_summary", result ? "fail" : "complete", detail, result);
    report("graphics_host", result ? "fail" : "complete", "execute/replay/simulation skipped", result);
    return result;
}
}
