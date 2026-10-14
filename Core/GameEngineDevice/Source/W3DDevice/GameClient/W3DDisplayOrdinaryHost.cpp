// SPDX-License-Identifier: GPL-3.0-or-later
// Ordinary-host profile display lifetime (X64.22). Kept separate from the
// attached diagnostic in W3DDisplayPresentation.cpp.
#if defined(_WIN64)
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "Common/GlobalData.h"
#include "Common/Errors.h"
#include "Common/StartupDiagnostics.h"
#include "Common/NativeOrdinaryHost.h"
#include "Common/DisplayRuntime.h"
#include "Common/DisplaySettings.h"
#include "Common/DisplayModes.h"
#include "Common/ModernExperience.h"
#include "Common/GroundTranslation.h"
#include "Common/OptionPreferences.h"
#include "Common/GameEngine.h"
#include "Common/MessageStream.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/Backend/NativeGraphicsSmoke.h"
#include "WW3D2/Backend/D3D11Backend.h"
#include "WW3D2/render2d.h"
#include "GameClient/Image.h"
#include "GameClient/GameText.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/Anim2D.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/WindowXlat.h"
#include "GameClient/GameClient.h"
#include "Common/GameDefines.h"
#include "GameLogic/GameLogic.h"
#include "Common/PlayerList.h"
#include "Common/FramePacer.h"
#include "GameClient/View.h"
#include "W3DDevice/GameClient/W3DView.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/NativeWorldObjects.h"
#include "W3DDevice/GameClient/NativeWorldEffects.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include "WW3D2/camera.h"
#include "GameClient/ControlBar.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Object.h"
#include "Common/Player.h"
#include "Common/Money.h"
#include "GameClient/Water.h"
#include "GameClient/GameWindow.h"
#include "Common/Radar.h"
#include "Common/NameKeyGenerator.h"
#include "GameLogic/TerrainLogic.h"
#include <vector>
#include <memory>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Ordinary-host profile (StartupDiagnostics::ordinaryHost).
//
// Unlike runNativePresentationSmoke this is not an attached diagnostic: the
// ordinary W3DDisplay::init/draw/~W3DDisplay own the presentation. The engine
// stays on the accepted headless world/audio/radar selection (m_headless is not
// changed); only the display lifetime and the application window are real.
// ---------------------------------------------------------------------------
namespace {
void HostLog(void *, const char *stage, const char *status, const char *detail, unsigned code) {
    StartupDiagnostics::checkpoint(stage, status, detail, code);
}
}
extern HWND ApplicationHWnd;

// ---------------------------------------------------------------------------------------------------------------
// X64.33 Modern Experience display. The accepted classic path (fixed 800x600 logical display, review scale, stretched
// swap chain) is left as it was; this path makes the window client, the swap chain and the logical display the same
// configured size, so 4:3, 16:9, 21:9 and 32:9 are ordinary sizes.
// ---------------------------------------------------------------------------------------------------------------
namespace {
bool modernRequested()
{
    return StartupDiagnostics::normalLaunch() && ModernExperience::enabled(ModernExperience::FEATURE_MODERN_DISPLAY);
}

DisplayConfig::Environment enumerateEnvironment()
{
    DisplayConfig::Environment env;
    env.monitors = DisplayModes::enumerateMonitors();
    RECT frame = { 0, 0, 100, 100 };
    if (AdjustWindowRect(&frame, NativeOrdinaryHost::WindowedStyle, FALSE)) {
        env.frameWidth = (frame.right - frame.left) - 100;
        env.frameHeight = (frame.bottom - frame.top) - 100;
    }
    return env;
}

DisplayConfig::KeyValues readOptionKeyValues()
{
    DisplayConfig::KeyValues kv;
    OptionPreferences prefs;
    for (OptionPreferences::const_iterator it = prefs.begin(); it != prefs.end(); ++it)
        kv[it->first.str()] = it->second.str();
    return kv;
}

// Position/style/size of the window for a resolved configuration. WinMain creates the window hidden in the modern
// path so the player never sees the provisional size.
void placeWindow(HWND window, const DisplayConfig::Resolved &r)
{
    const bool borderless = r.mode != DisplayConfig::WindowMode::Windowed;
    const DWORD style = borderless ? (WS_POPUP | WS_VISIBLE) : NativeOrdinaryHost::WindowedStyle;
    RECT outer = { 0, 0, r.client.width, r.client.height };
    if (!borderless && !AdjustWindowRect(&outer, style, FALSE))
        throw std::runtime_error("AdjustWindowRect failed for the modern display");
    const int width = outer.right - outer.left, height = outer.bottom - outer.top;
    int x = r.monitorBounds.x, y = r.monitorBounds.y;
    if (!borderless) {
        x = r.monitorWork.x + (r.monitorWork.width - width) / 2;
        y = r.monitorWork.y + (r.monitorWork.height - height) / 2;
        if (x < r.monitorWork.x) x = r.monitorWork.x;
        if (y < r.monitorWork.y) y = r.monitorWork.y;
    }
    SetWindowLongPtr(window, GWL_STYLE, static_cast<LONG_PTR>(style));
    if (!SetWindowPos(window, HWND_TOP, x, y, width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW))
        throw std::runtime_error("SetWindowPos failed for the modern display");
    SetForegroundWindow(window);
    SetFocus(window);
}
}

// Runtime change of the display (Options): same window, same swap chain, new size/mode. Returns false (state
// unchanged) when it cannot be applied; the persisted preference is written by the caller either way.
bool NativeApplyDisplayMode(unsigned width, unsigned height, bool borderless, int monitorIndex)
{
    if (!DisplayRuntime::modern() || !ApplicationHWnd || !TheDisplay) return false;
    DisplayConfig::Settings wanted = DisplayRuntime::state().settings;
    wanted.explicitSettings = true;
    wanted.mode = borderless ? DisplayConfig::WindowMode::Borderless : DisplayConfig::WindowMode::Windowed;
    wanted.resolution = { static_cast<int>(width), static_cast<int>(height) };
    wanted.monitor = monitorIndex >= 0 ? monitorIndex : DisplayRuntime::state().resolved.monitorIndex;
    const DisplayConfig::Environment env = enumerateEnvironment();
    const DisplayConfig::Resolved r = DisplayConfig::resolve(wanted, env);
    if (!r.client.valid()) return false;
    try {
        placeWindow(ApplicationHWnd, r);
        DisplayRuntime::State &state = DisplayRuntime::state();
        const DisplayGeometry::Size previous = state.resolved.client;
        state.settings = wanted;
        state.resolved = r;
        DisplayRuntime::setClient(r.client);
        if (previous != r.client) {
            WW3D::Resize_Native_Presentation(r.client.width, r.client.height);
            Render2DClass::Set_Screen_Resolution(RectClass(0, 0, r.client.width, r.client.height));
        }
        char detail[160];
        sprintf_s(detail, "client=%dx%d mode=%s monitor=%d ui_scale=%d%%", r.client.width, r.client.height,
            DisplayConfig::modeName(r.mode), r.monitorIndex, DisplayRuntime::layout().uiScalePercent);
        StartupDiagnostics::checkpoint("display_apply", "complete", detail);
        return true;
    } catch (const std::exception &error) {
        StartupDiagnostics::checkpoint("display_apply", "fail", error.what(), 1);
        return false;
    }
}

// Resolution list for the Options screen (Modern Experience only): real, fitting sizes for the active monitor/mode.
void NativeModernSizeChoices(std::vector<DisplayGeometry::Size> &out)
{
    out.clear();
    if (!DisplayRuntime::modern()) return;
    const DisplayConfig::Environment env = enumerateEnvironment();
    const DisplayRuntime::State &state = DisplayRuntime::state();
    DisplayModes::Monitor monitor;
    if (env.monitors.empty()) {
        monitor.bounds = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        monitor.work = monitor.bounds;
    } else {
        size_t index = static_cast<size_t>(state.resolved.monitorIndex);
        if (index >= env.monitors.size()) index = 0;
        monitor = env.monitors[index];
    }
    out = DisplayConfig::sizeChoices(monitor, state.resolved.mode, env.frameWidth, env.frameHeight,
        state.resolved.client);
}

void W3DDisplay::initOrdinaryHostPresentation()
{
    StartupDiagnostics::Phase phase("display_ordinary_init");
    if (!TheGlobalData->m_headless || m_2DRender)
        throw std::runtime_error("Ordinary host display requires the headless world selection and no prior 2D helper");
    const bool modern = modernRequested();
    DisplayGeometry::Size displaySize = { 800, 600 };
    bool vsync = false;
    if (modern) {
        // X64.33: resolve the persisted display settings against the attached monitors and shape the (hidden)
        // window to match before any swap chain exists.
        const DisplayConfig::Settings settings = DisplayConfig::load(readOptionKeyValues(), false);
        const DisplayConfig::Environment env = enumerateEnvironment();
        const DisplayConfig::Resolved resolved = DisplayConfig::resolve(settings, env,
            { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) });
        DisplayRuntime::State &state = DisplayRuntime::state();
        state.modern = true;
        DisplayRuntime::applyModeHook() = &NativeApplyDisplayMode;
        DisplayRuntime::vsyncHook() = [](bool enabled) { WW3D::Set_Native_Vsync(enabled); };
        if (settings.smoothMotion) GroundTranslation::setEnabled(true);   // Options > Unit Motion; off by default
        state.settings = settings;
        state.resolved = resolved;
        DisplayRuntime::setClient(resolved.client);
        placeWindow(ApplicationHWnd, resolved);
        displaySize = resolved.client;
        vsync = resolved.vsync;
        TheWritableGlobalData->m_windowed = TRUE;
        TheWritableGlobalData->m_xResolution = displaySize.width;
        TheWritableGlobalData->m_yResolution = displaySize.height;
        char detail[320];
        sprintf_s(detail, "client=%dx%d mode=%s monitor=%d/%d desktop_refresh=%dHz vsync=%d ui_scale=%d%% (%s) aspect=%s settings=%s",
            displaySize.width, displaySize.height, DisplayConfig::modeName(resolved.mode),
            resolved.monitorIndex, static_cast<int>(env.monitors.size()), resolved.refreshHz, vsync ? 1 : 0,
            resolved.layout.uiScalePercent, settings.uiScalePercent > 0 ? "fixed" : "auto",
            DisplayModes::aspectFamily(displaySize), settings.explicitSettings ? "stored" : "migrated-defaults");
        StartupDiagnostics::checkpoint("display_modern", "complete", detail);
        for (const std::string &note : resolved.notes)
            StartupDiagnostics::checkpoint("display_modern_note", "observed", note.c_str());
    } else if (StartupDiagnostics::normalLaunch()) {
        // Classic normal launch: the native runtime presents the original 800x600 logical display in a window.
        TheWritableGlobalData->m_windowed = TRUE;
        TheWritableGlobalData->m_xResolution = 800;
        TheWritableGlobalData->m_yResolution = 600;
    }
    if (!modern && (!TheGlobalData->m_windowed || TheGlobalData->m_xResolution != 800 || TheGlobalData->m_yResolution != 600)) {
        char detail[160];
        sprintf_s(detail, "windowed=%d xres=%d yres=%d; only windowed 800x600 is supported",
            TheGlobalData->m_windowed ? 1 : 0, (int)TheGlobalData->m_xResolution, (int)TheGlobalData->m_yResolution);
        StartupDiagnostics::checkpoint("display_config", "fail", detail, 1);
        throw std::runtime_error(detail);
    }
    if (!NativeOrdinaryHost::clientIsExact(ApplicationHWnd)) {
        StartupDiagnostics::checkpoint("display_config", "fail", "ApplicationHWnd client is not the expected size", 1);
        throw std::runtime_error("ApplicationHWnd client is not the expected size");
    }
    {
        char detail[96];
        sprintf_s(detail, "windowed=1 %dx%dx32 ApplicationHWnd", displaySize.width, displaySize.height);
        StartupDiagnostics::checkpoint("display_config", "complete", detail);
    }
    // Receipted decisions: each legacy DX8-only initializer this profile does not run.
    static const char *const bypass[] = {
        "legacy_ww3d_init: WW3D::Init/DX8Backend/DX8Wrapper::Init (D3D8.DLL) not entered",
        "legacy_render_device: Set_Render_Device, enumeration, MSAA, filter, anisotropy not entered",
        "legacy_scenes: 2D/3D/interface scenes and lights not created (no world)",
        "legacy_shader_manager: W3DShaderManager not initialized (legacy shaders)",
        "legacy_debug_display_browser: debug display and embedded browser not created",
        "legacy_gamma_lod: gamma and texture LOD reduction not applied",
    };
    for (const char *line : bypass) StartupDiagnostics::checkpoint("display_bypass", "complete", line);

    if (StartupDiagnostics::ordinaryTerrain()) {
        // Terrain world profile: CPU-backed legacy buffers and the depth buffer are explicit,
        // process-wide opt-ins set before the backend and any terrain buffer exist. X64.22-24
        // profiles never reach this branch and keep the exact accepted behavior.
        D3D11Backend::Enable_CPU_Backed_Geometry(true);
        D3D11Backend::Enable_Depth_Buffer_For_W3D(true);
        StartupDiagnostics::checkpoint("terrain_optin", "complete",
            "cpu_backed_geometry=1 depth_buffer=1 (terrain-only world pass; meshes/water/shadows/particles absent)");
    }
    PresentationConfiguration configuration;
    configuration.window = ApplicationHWnd;
    configuration.diagnostic = HostLog;
    configuration.width = static_cast<unsigned>(displaySize.width);
    configuration.height = static_cast<unsigned>(displaySize.height);
    configuration.vsync = vsync;
    WW3D::Init_Native_Presentation(configuration);

    m_2DRender = NEW Render2DClass;
    setWidth(displaySize.width);
    setHeight(displaySize.height);
    setBitDepth(32);
    setWindowed(TRUE);
    Render2DClass::Set_Screen_Resolution(RectClass(0, 0, displaySize.width, displaySize.height));
    // Same pixel-center policy the legacy non-headless init selects.
    WW3D::Set_Screen_UV_Bias(true);

    int w = 0, h = 0, b = 0; bool windowed = false;
    WW3D::Get_Device_Resolution(w, h, b, windowed);
    if (w != displaySize.width || h != displaySize.height || b != 32 || !windowed)
        throw std::runtime_error("Ordinary host device dimensions disagree");
    WW3D::Get_Render_Target_Resolution(w, h, b, windowed);
    if (w != displaySize.width || h != displaySize.height || b != 32)
        throw std::runtime_error("Ordinary host target dimensions disagree");
    const NativeOrdinaryHost::ModuleAudit audit = NativeOrdinaryHost::auditLoadedModules();
    char detail[160];
    sprintf_s(detail, "d3d8=%d d3dx8=%d d3d11=%d dxgi=%d", audit.d3d8, audit.d3dx8, audit.d3d11, audit.dxgi);
    if (!NativeOrdinaryHost::auditAcceptable(audit)) {
        StartupDiagnostics::checkpoint("display_module_audit", "fail", detail, 1);
        throw std::runtime_error("Legacy D3D8 module loaded under the ordinary host profile");
    }
    StartupDiagnostics::checkpoint("display_module_audit", "complete", detail);
    {
        char sizeDetail[96];
        sprintf_s(sizeDetail, "device=%dx%d target=%dx%d bits=32 windowed=1", displaySize.width, displaySize.height,
            displaySize.width, displaySize.height);
        StartupDiagnostics::checkpoint("display_size", "complete", sizeDetail);
    }
    if (StartupDiagnostics::normalLaunch()) {
        // Normal launch has no host loop to do the play profile's one-time setup: the decal / planar shadow
        // manager is installed with the display and removed by releaseAll() at shutdown.
        NativeWorldEffects::install();
    }
}

struct W3DDisplay::OrdinaryHostContent {
    const Image *logo = nullptr;
    DisplayString *label = nullptr;
    DisplayString *caption = nullptr;
    bool firstDraw = true;
    void release() {
        if (TheDisplayStringManager) {
            if (label) TheDisplayStringManager->freeDisplayString(label);
            if (caption) TheDisplayStringManager->freeDisplayString(caption);
        }
        label = caption = nullptr;
    }
    ~OrdinaryHostContent() { release(); }
};

void W3DDisplay::drawOrdinaryHostFrame()
{
    static bool firstFrame = true;
    // Color-only frame: PresentationLifecycle rejects depth, WW3D::Render throws.
    if (WW3D::Begin_Render(true, false, Vector3(0.08f, 0.32f, 0.55f), 1.0f) != WW3D_ERROR_OK)
        throw std::runtime_error("Ordinary host Begin_Render failed");
    if (firstFrame) StartupDiagnostics::checkpoint("display_frame_begin", "complete", "ordinary W3DDisplay::draw color-only; no views/world");
    try {
        if (OrdinaryHostContent *c = m_hostContent) {
            const Color drop = 0x00000000;
            // Original panel primitives, original global-cache logo, ordinary strings.
            drawFillRect(533, 109, 222, 250, 0x7e000000);
            drawOpenRect(532, 108, 224, 252, 1.0f, 0xffffbb00);
            drawImage(c->logo, 504, 16, 791, 110, 0xffffffff, DRAW_IMAGE_ALPHA);
            c->label->draw(540, 116, 0xffffbe01, drop);
            c->caption->draw(16, 16, 0xffffffff, drop);
            if (c->firstDraw) {
                c->firstDraw = false;
                StartupDiagnostics::checkpoint("ui_ordinary_draw", "complete",
                    "W3DDisplay fill/open rect, global-cache GeneralsLogo, original label, DisplayString caption -> Render2D -> D3D11");
            }
        }
    } catch (...) {
        try { WW3D::End_Render(false); } catch (...) {}
        throw;
    }
    if (WW3D::End_Render(true) != WW3D_ERROR_OK)
        throw std::runtime_error("Ordinary host End_Render failed");
    if (firstFrame) { firstFrame = false; StartupDiagnostics::checkpoint("display_frame_end", "complete", "WW3D::End_Render Present"); }
}

void W3DDisplay::releaseOrdinaryHostPresentation()
{
    // Called after the asset manager released the global UI textures.
    StartupDiagnostics::checkpoint("display_detach", "begin", "ordinary destructor");
    if (StartupDiagnostics::ordinaryTerrain()) {
        NativeWorldObjects::releaseAll();   // residencies and render-object references first (play profile)
        if (TheTerrainRenderObject) {
            if (auto *terrain = dynamic_cast<HeightMapRenderObjClass *>(TheTerrainRenderObject))
                terrain->releaseNativeResources();
        }
        D3D11Backend::Enable_CPU_Backed_Geometry(false);
        D3D11Backend::Enable_Depth_Buffer_For_W3D(false);
    }
    const unsigned live = D3D11Backend::Live_Residencies();
    char detail[96]; sprintf_s(detail, "live_residencies=%u before backend release", live);
    if (!NativeOrdinaryHost::residencyReleasedBeforeBackend(live)) {
        StartupDiagnostics::checkpoint("ui_residency_release", "fail", detail, live);
        StartupDiagnostics::markFailed();
    } else {
        StartupDiagnostics::checkpoint("ui_residency_release", "complete", detail);
    }
    WW3D::Shutdown_Native_Presentation();
    StartupDiagnostics::checkpoint("display_detach", "complete", "backend/device released; HWND still owned by WinMain");
}

int W3DDisplay::runOrdinaryHostGate()
{
    using NativeGraphicsSmoke::Decision;
    if (!StartupDiagnostics::ordinaryHost() || !m_initialized || !m_2DRender || !TheGameEngine ||
        !TheMessageStream || !ApplicationHWnd || m_hostContent) {
        StartupDiagnostics::checkpoint("ordinary_host_loop", "fail", "preconditions", 1);
        return 1;
    }
    unsigned frames = 0, visibleFrames = 0, occludedFrames = 0, elapsed = 0;
    int result = 1;
    std::unique_ptr<OrdinaryHostContent> content(new OrdinaryHostContent);
    const char *phase = "ui_ordinary_content";
    try {
        StartupDiagnostics::checkpoint(phase, "begin", "GeneralsLogo + GUI:SelectDifficulty + caption");
        content->logo = TheMappedImageCollection ? TheMappedImageCollection->findImageByName("GeneralsLogo") : nullptr;
        if (!content->logo || content->logo->getStatus() != IMAGE_STATUS_NONE || content->logo->getFilename().isEmpty())
            throw std::runtime_error("Original GeneralsLogo mapped image unavailable");
        if (!TheGameText || !TheDisplayStringManager || !TheFontLibrary)
            throw std::runtime_error("Ordinary text/font subsystems unavailable");
        Bool exists = FALSE;
        const UnicodeString text = TheGameText->fetch("GUI:SelectDifficulty", &exists);
        if (!exists || text.isEmpty()) throw std::runtime_error("Original menu label unavailable");
        GameFont *font = TheFontLibrary->getFont("Arial", 14, FALSE);
        if (!font) throw std::runtime_error("Ordinary Arial font unavailable");
        content->label = TheDisplayStringManager->newDisplayString();
        content->label->setFont(font);
        content->label->setText(text);
        content->caption = TheDisplayStringManager->newDisplayString();
        content->caption->setFont(font);
        content->caption->setText(UnicodeString(L"AMD64 ordinary host - presentation gate (no gameplay)"));
        char detail[256];
        sprintf_s(detail, "file=%s label=GUI:SelectDifficulty font=Arial size=14", content->logo->getFilename().str());
        StartupDiagnostics::checkpoint(phase, "complete", detail);
        m_hostContent = content.get();

        phase = "ordinary_host_loop";
        StartupDiagnostics::checkpoint(phase, "begin", "serviceWindowsOS + TheDisplay->DRAW; no update/execute");
        const ULONGLONG start = GetTickCount64();
        bool lastVisible = false;
        for (;;) {
            TheGameEngine->serviceWindowsOS(); // ordinary application WndProc
            if (!IsWindow(ApplicationHWnd)) throw std::runtime_error("Application window destroyed");
            const bool aborted = TheGameEngine->getQuitting() ||
                TheMessageStream->containsMessageOfType(GameMessage::MSG_META_DEMO_INSTANT_QUIT);
            const bool iconic = IsIconic(ApplicationHWnd) != FALSE;
            if (!iconic && !NativeOrdinaryHost::clientIsExact(ApplicationHWnd))
                throw std::runtime_error("Unsupported resize: client is not 800x600");
            elapsed = static_cast<unsigned>(GetTickCount64() - start);
            const bool visible = IsWindowVisible(ApplicationHWnd) && !iconic;
            const Decision decision = NativeGraphicsSmoke::Evaluate(elapsed, frames, visibleFrames,
                visible && lastVisible, aborted, 5000, 300);
            if (decision != Decision::Continue) {
                if (decision == Decision::Failure)
                    throw std::runtime_error(aborted ? "Close/quit abort" : "No visible Present or frame/time bound reached");
                break;
            }
            TheDisplay->DRAW(); // ordinary entry point (W3DDisplay::draw)
            if (!iconic) {
                const PresentationStatus present = WW3D::Native_Presentation_Status();
                ++frames; if (present.occluded) ++occludedFrames;
                lastVisible = visible && !present.occluded && present.result == 0;
                if (lastVisible) ++visibleFrames;
                if (frames == 1 || (lastVisible && visibleFrames == 1)) {
                    sprintf_s(detail, "HRESULT=0x%08X visible=%u occluded=%u removed=0x%08X frame=%u",
                        present.result, visible ? 1u : 0u, present.occluded ? 1u : 0u, present.removed_reason, frames);
                    StartupDiagnostics::checkpoint("graphics_present", lastVisible ? "complete" : "observed", detail, present.result);
                }
            }
            Sleep(20);
        }
        result = 0;
    } catch (const std::exception &error) {
        StartupDiagnostics::checkpoint(phase, "fail", error.what(), 1);
        result = 1;
    } catch (const ErrorCode error) {
        StartupDiagnostics::checkpoint(phase, "fail", "engine ErrorCode", static_cast<unsigned>(error));
        result = 1;
    } catch (...) {
        StartupDiagnostics::checkpoint(phase, "fail", "unknown exception", 1);
        result = 1;
    }
    // Strings and font references go before display/device teardown.
    m_hostContent = nullptr;
    content.reset();
    char summary[160];
    sprintf_s(summary, "frames=%u visible_frames=%u occluded_frames=%u elapsed_ms=%u driver=hardware",
        frames, visibleFrames, occludedFrames, elapsed);
    StartupDiagnostics::checkpoint("ordinary_host_summary", result ? "fail" : "complete", summary, result);
    if (result) StartupDiagnostics::markFailed();
    return result;
}
// ---------------------------------------------------------------------------
// Ordinary-menu profile (StartupDiagnostics::ordinaryMenu): the original
// interactive main menu on the ordinary application/display lifetime, driven by
// a bounded menu-only loop. World/audio/radar/view selection stays headless;
// GameLogic::update, GameClient::update, GameEngine::update/execute and message
// propagation through gameplay translators are never run. Any request that would
// leave the UI (new game, network, any other Shell screen) is an explicit
// boundary stop, never an emulated success.
// ---------------------------------------------------------------------------
#if RTS_GENERALS
namespace {
struct MenuBoundary {
    bool hit = false;
    char reason[64] = {};
    char detail[160] = {};
} g_menuBoundary;

void RecordBoundary(const char *reason, const char *detail) {
    if (g_menuBoundary.hit) return;
    g_menuBoundary.hit = true;
    strcpy_s(g_menuBoundary.reason, reason);
    strcpy_s(g_menuBoundary.detail, detail);
}

// Only the original main menu may be pushed; every other screen is outside the
// audited UI-only boundary (several touch DX8 enumeration, shell map or world).
Bool MenuPushGuard(const AsciiString &filename) {
    if (filename.compareNoCase("Menus/MainMenu.wnd") == 0) return TRUE;
    RecordBoundary("shell_push", filename.str());
    return FALSE;
}

struct MenuCounters {
    unsigned frames = 0, visibleFrames = 0, occludedFrames = 0;
    unsigned mouseMoves = 0, leftDown = 0, leftUp = 0, keyDown = 0;
    ICoord2D lastMouse = { -1, -1 };
    bool closeRequested = false;
};

// Routes raw input to the window system only; consumes and deletes every stream
// message. Gameplay translators are not attached to this path.
void DrainMenuStream(WindowTranslator &translator, MenuCounters &c, bool route) {
    GameMessage *msg;
    while ((msg = TheMessageStream->getFirstMessage()) != nullptr) {
        const GameMessage::Type type = msg->getType();
        switch (type) {
        case GameMessage::MSG_RAW_MOUSE_POSITION: {
            const ICoord2D p = msg->getArgument(0)->pixel;
            if (p.x != c.lastMouse.x || p.y != c.lastMouse.y) { ++c.mouseMoves; c.lastMouse = p; }
            if (route) translator.translateGameMessage(msg);
            break;
        }
        case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN: ++c.leftDown; if (route) translator.translateGameMessage(msg); break;
        case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP: ++c.leftUp; if (route) translator.translateGameMessage(msg); break;
        case GameMessage::MSG_RAW_KEY_DOWN: ++c.keyDown; if (route) translator.translateGameMessage(msg); break;
        case GameMessage::MSG_META_DEMO_INSTANT_QUIT: c.closeRequested = true; break; // WM_CLOSE / quit
        case GameMessage::MSG_NEW_GAME: RecordBoundary("new_game", "MSG_NEW_GAME requested by the menu"); break;
        case GameMessage::MSG_CLEAR_GAME_DATA: RecordBoundary("clear_game_data", "MSG_CLEAR_GAME_DATA requested"); break;
        default:
            if (type >= GameMessage::MSG_BEGIN_NETWORK_MESSAGES && type <= GameMessage::MSG_END_NETWORK_MESSAGES)
                RecordBoundary("network_message", "network message requested by the menu");
            else if (route)
                translator.translateGameMessage(msg); // remaining raw input (mouse buttons/wheel, key up)
            break;
        }
        deleteInstance(msg);
    }
}
} // namespace
#endif

namespace {
unsigned g_terrainFrames = 0, g_terrainDraws = 0;
HeightMapRenderObjClass::NativeTerrainStats g_terrainStats = {};
HeightMapRenderObjClass::NativeMapFeatures g_mapFeatures = {};
// Camera exercise (X64.26): the game loop requests a probe after each scripted camera move; the
// next terrain frame reads the target back before the UI is drawn and records the result.
unsigned g_motionStepRequest = 0;
unsigned g_uiProbesDone = 0;

struct PixelStats { unsigned nonblack_pct = 0; unsigned char center[3] = {}; };

PixelStats MeasurePixels(const std::vector<unsigned char> &pixels)
{
    PixelStats stats;
    size_t lit = 0;
    for (size_t i = 0; i + 3 < pixels.size(); i += 4)
        if (pixels[i] | pixels[i + 1] | pixels[i + 2]) ++lit;
    const size_t total = pixels.size() / 4;
    stats.nonblack_pct = total ? static_cast<unsigned>(lit * 100 / total) : 0u;
    const size_t c = (total / 2 + 400) * 4 < pixels.size() ? (total / 2 + 400) * 4 : 0;
    if (pixels.size() >= 4) { stats.center[0] = pixels[c]; stats.center[1] = pixels[c + 1]; stats.center[2] = pixels[c + 2]; }
    return stats;
}

// Evidence image next to the startup log (the isolated run directory). 32-bit BGRA top-down BMP
// from the backend's RGBA readback; failures are reported, never fatal.
bool WriteBmpFile(const char *path, const std::vector<unsigned char> &rgba, unsigned width, unsigned height)
{
    if (rgba.size() != static_cast<size_t>(width) * height * 4) return false;
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    unsigned char header[54] = {'B', 'M'};
    const unsigned dataSize = width * height * 4, fileSize = 54 + dataSize;
    std::memcpy(header + 2, &fileSize, 4);
    const unsigned offset = 54, dib = 40, planes = 1, bits = 32;
    std::memcpy(header + 10, &offset, 4); std::memcpy(header + 14, &dib, 4);
    std::memcpy(header + 18, &width, 4);
    const int topDown = -static_cast<int>(height);
    std::memcpy(header + 22, &topDown, 4);
    std::memcpy(header + 26, &planes, 2); std::memcpy(header + 28, &bits, 2);
    std::memcpy(header + 34, &dataSize, 4);
    std::vector<unsigned char> bgra(rgba.size());
    for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
        bgra[i] = rgba[i + 2]; bgra[i + 1] = rgba[i + 1]; bgra[i + 2] = rgba[i]; bgra[i + 3] = 255;
    }
    DWORD written = 0;
    const bool ok = WriteFile(file, header, 54, &written, nullptr) && written == 54 &&
        WriteFile(file, bgra.data(), static_cast<DWORD>(bgra.size()), &written, nullptr) && written == bgra.size();
    CloseHandle(file);
    return ok;
}

bool WriteBmp(const char *name, const std::vector<unsigned char> &rgba, unsigned width, unsigned height)
{
    char path[MAX_PATH];
    sprintf_s(path, "%s\\%s", StartupDiagnostics::state().root, name);
    return WriteBmpFile(path, rgba, width, height);
}

// Bounded Control Bar / HUD review facts (UI investigation, not a world feature).
void LogControlBarFacts()
{
    char detail[320];
    static const char *const names[] = {"ControlBar.wnd:ControlBarParent", "ControlBar.wnd:BackgroundMarker",
                                        "ControlBar.wnd:LeftHUD", "ControlBar.wnd:RightHUD"};
    for (const char *name : names) {
        GameWindow *window = TheWindowManager ?
            TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(name)) : nullptr;
        if (!window) { sprintf_s(detail, "%s absent", name); }
        else {
            ICoord2D pos = {0, 0}, size = {0, 0};
            window->winGetScreenPosition(&pos.x, &pos.y);
            window->winGetSize(&size.x, &size.y);
            sprintf_s(detail, "%s hidden=%d pos=%d,%d size=%dx%d", name, window->winIsHidden() ? 1 : 0, pos.x, pos.y, size.x, size.y);
        }
        StartupDiagnostics::checkpoint("ui_probe_window", "observed", detail);
    }
    sprintf_s(detail, "radar=%s control_bar=%d viewport_scale=%.2f view_height=%d display_height=%d",
        dynamic_cast<RadarDummy *>(TheRadar) ? "dummy" : (TheRadar ? "real" : "none"), TheControlBar ? 1 : 0,
        TheGlobalData->m_viewportHeightScale, TheTacticalView ? TheTacticalView->getHeight() : -1,
        TheDisplay ? static_cast<int>(TheDisplay->getHeight()) : -1);
    StartupDiagnostics::checkpoint("ui_probe_facts", "observed", detail);
}
}

// The world-objects receipt is produced inside the pass but must follow terrain_first_frame in the
// log (the classifier's phase order); the menu-frame code publishes it right after that receipt.
static char g_worldObjectsFirstDetail[400];
static bool g_worldObjectsFirstPending = false;

// Terrain-only native world pass (profile ordinaryTerrain, in game, map loaded). Returns the
// number of draws; zero when there is nothing to draw yet (shell, loading, no camera).
static bool shellGameActive()
{
    return TheGameLogic && TheGameLogic->isInShellGame();
}

static int drawNativeTerrainPass()
{
    if (!StartupDiagnostics::ordinaryTerrain() || !TheGameLogic || !TheGameLogic->isInGame() ||
        TheGameLogic->isLoadingMap() || !TheTerrainRenderObject || !TheTacticalView)
        return 0;
    // The shell map (the animated 3D background of the original main menu) is a normal-launch feature;
    // the evidence profiles keep their accepted shell-off behavior.
    if (TheGameLogic->isInShellGame() &&
        !(StartupDiagnostics::normalLaunch() && StartupDiagnostics::nativeFeature("shellmap")))
        return 0;
    auto *terrain = dynamic_cast<HeightMapRenderObjClass *>(TheTerrainRenderObject);
    auto *view = dynamic_cast<W3DView *>(TheTacticalView);
    if (!terrain || !view || !terrain->getMap()) return 0;
    CameraClass *camera = view->get3DCamera();
    if (!camera) return 0;
    auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend());
    if (!backend) throw std::runtime_error("Terrain pass requires the D3D11 backend");
    backend->Clear(false, true, Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, 0); // depth only
    const int draws = terrain->renderNative(*camera);
    if (draws && StartupDiagnostics::ordinaryPlay()) {
        // World objects (buildings, units) share the terrain camera, viewport and depth buffer.
        NativeWorldObjects::render(*backend, *camera);
        static bool reported = false;
        const NativeWorldObjects::Stats os = NativeWorldObjects::stats();
        if (!reported && os.lastObjectsDrawn) {
            reported = true;
            sprintf_s(g_worldObjectsFirstDetail, "registered=%u objects_drawn=%u batches=%u triangles=%u meshes_drawn=%u skinned=%u textures=%u texture_failures=%u team_colored=%u unsupported_states=%u",
                NativeWorldObjects::registered(), os.lastObjectsDrawn, os.lastBatches, os.lastTriangles,
                os.meshesDrawn, os.skinned, os.textureLoads, os.textureFailures, os.teamColored, os.unsupportedState);
            g_worldObjectsFirstPending = true;
        }
    }
    return draws;
}

void W3DDisplay::drawOrdinaryMenuFrame()
{
    // Black clear like the legacy no-shell-map menu; windows then the cursor layer.
    if (WW3D::Begin_Render(true, false, Vector3(0.0f, 0.0f, 0.0f), 1.0f) != WW3D_ERROR_OK)
        throw std::runtime_error("Ordinary menu Begin_Render failed");
    try {
        const int terrainDraws = drawNativeTerrainPass();
        if (terrainDraws) {
            if (g_terrainFrames == 0) {
                char detail[320];
                auto *terrainObject = static_cast<HeightMapRenderObjClass *>(TheTerrainRenderObject);
                const HeightMapRenderObjClass::NativeTerrainStats stats = terrainObject->nativeStats();
                g_mapFeatures = terrainObject->nativeMapFeatures();
                const int softEdge = (TheGlobalData->m_showSoftWaterEdge &&
                                      TheWaterTransparency->m_transparentWaterDepth != 0) ? 1 : 0;
                sprintf_s(detail, "tiles=%u draws=%d atlas_uploads=%u use_cloud=%d use_noise=%d soft_edge=%d map_extra=%u map_shore=%u map_roads=%u map_bridges=%u (terrain combined single pass + overlays; UI follows)",
                    stats.tileUploads, terrainDraws, stats.atlasUploads,
                    HeightMapRenderObjClass::nativeUseCloud() ? 1 : 0, TheGlobalData->m_useLightMap ? 1 : 0, softEdge,
                    g_mapFeatures.extraBlendTiles, g_mapFeatures.shorelineTiles, g_mapFeatures.roadSegments, g_mapFeatures.bridges);
                StartupDiagnostics::checkpoint("terrain_first_frame", "complete", detail);
            }
            if (g_worldObjectsFirstPending) {
                g_worldObjectsFirstPending = false;
                StartupDiagnostics::checkpoint("world_objects_first_frame", "complete", g_worldObjectsFirstDetail);
            }
            const bool firstProbe = (g_terrainFrames == 29) && StartupDiagnostics::enabled();
            if (firstProbe || g_motionStepRequest) {
                // One-time / per-camera-step evidence: what the terrain pass left in the target
                // before the UI is drawn (separates "not visible" from "covered by the UI").
                char detail[320];
                if (auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend())) {
                    const std::vector<unsigned char> pixels = backend->Readback_RGBA8();
                    const PixelStats stats = MeasurePixels(pixels);
                    const HeightMapRenderObjClass::NativeTerrainStats ts =
                        static_cast<HeightMapRenderObjClass *>(TheTerrainRenderObject)->nativeStats();
                    Vector3 cam(0, 0, 0), fwd(0, 0, 0);
                    if (auto *view = dynamic_cast<W3DView *>(TheTacticalView)) {
                        cam = view->get3DCamera()->Get_Position();
                        fwd = view->get3DCamera()->Get_Forward_Dir();
                    }
                    if (firstProbe) {
                        sprintf_s(detail, "nonblack_pct=%u center_rgb=%02X%02X%02X cam=(%.0f,%.0f,%.0f) fwd=(%.2f,%.2f,%.2f)",
                            stats.nonblack_pct, stats.center[0], stats.center[1], stats.center[2],
                            cam.X, cam.Y, cam.Z, fwd.X, fwd.Y, fwd.Z);
                        StartupDiagnostics::checkpoint("terrain_probe", "observed", detail);
                        if (!WriteBmp("terrain_world_frame.bmp", pixels, 800, 600))
                            StartupDiagnostics::checkpoint("terrain_image", "observed", "world frame image not written");
                    }
                    if (g_motionStepRequest) {
                        const Coord3D pivot = TheTacticalView ? TheTacticalView->getPosition() : Coord3D{0, 0, 0};
                        sprintf_s(detail, "step=%u pivot=(%.0f,%.0f) nonblack_pct=%u tile_updates=%u tile_uploads=%u cloud_frames=%u noise_frames=%u cam=(%.0f,%.0f,%.0f)",
                            g_motionStepRequest, pivot.x, pivot.y, stats.nonblack_pct, ts.tileUpdates, ts.tileUploads,
                            ts.cloudFrames, ts.noiseFrames, cam.X, cam.Y, cam.Z);
                        StartupDiagnostics::checkpoint("terrain_motion", "observed", detail);
                        g_motionStepRequest = 0;
                    }
                }
            }
            ++g_terrainFrames;
            g_terrainDraws += static_cast<unsigned>(terrainDraws);
            g_terrainStats = static_cast<HeightMapRenderObjClass *>(TheTerrainRenderObject)->nativeStats();
        }
        if (StartupDiagnostics::ordinaryPlay() && terrainDraws && TheInGameUI && !shellGameActive()) {
            // Native play profile: the legacy in-game composition after the 3D scene - per-drawable
            // icon UI (health bars, veterancy, names), then InGameUI::DRAW which draws the selection
            // region, move/placement hints, messages and repaints every window (Control Bar included).
            if (auto *view = dynamic_cast<W3DView *>(TheTacticalView)) view->drawNativeWorldOverlays();
            TheInGameUI->DRAW();
        } else if (TheWindowManager) {
            TheWindowManager->winRepaint();
        }
        if (TheMouse) TheMouse->DRAW();
        if (StartupDiagnostics::enabled() && StartupDiagnostics::ordinaryTerrain() && terrainDraws && g_uiProbesDone == 0 && g_terrainFrames >= 240) {
            // Bounded Control Bar review: composed frame after the UI (bottom 20% = outside the
            // 0.80 world viewport) with per-third non-black statistics and an evidence image.
            g_uiProbesDone = 1;
            if (auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend())) {
                const std::vector<unsigned char> pixels = backend->Readback_RGBA8();
                unsigned lit[3] = {0, 0, 0}, total[3] = {0, 0, 0}, all = 0;
                for (unsigned y = 480; y < 600; ++y)
                    for (unsigned x = 0; x < 800; ++x) {
                        const size_t i = (static_cast<size_t>(y) * 800 + x) * 4;
                        const unsigned part = x < 267 ? 0 : (x < 534 ? 1 : 2);
                        ++total[part];
                        if (pixels[i] | pixels[i + 1] | pixels[i + 2]) { ++lit[part]; ++all; }
                    }
                const PixelStats whole = MeasurePixels(pixels);
                char detail[256];
                sprintf_s(detail, "whole_nonblack_pct=%u strip_nonblack_pct=%u strip_left=%u strip_mid=%u strip_right=%u",
                    whole.nonblack_pct, all * 100 / (total[0] + total[1] + total[2]),
                    lit[0] * 100 / total[0], lit[1] * 100 / total[1], lit[2] * 100 / total[2]);
                StartupDiagnostics::checkpoint("ui_probe", "observed", detail);
                if (!WriteBmp("terrain_ui_frame.bmp", pixels, 800, 600))
                    StartupDiagnostics::checkpoint("terrain_image", "observed", "ui frame image not written");
                LogControlBarFacts();
            }
        }
    } catch (...) {
        try { WW3D::End_Render(false); } catch (...) {}
        throw;
    }
    if (WW3D::End_Render(true) != WW3D_ERROR_OK)
        throw std::runtime_error("Ordinary menu End_Render failed");
    const PresentationStatus present = WW3D::Native_Presentation_Status();
    ++m_hostPresents;
    m_hostPresentResult = present.result;
    m_hostPresentOccluded = present.occluded;
}

int W3DDisplay::runOrdinaryMenuHost()
{
#if !RTS_GENERALS
    StartupDiagnostics::checkpoint("menu_host", "fail", "Generals only", 1);
    return 1;
#else
    using NativeGraphicsSmoke::Decision;
    if (!StartupDiagnostics::ordinaryMenu() || !m_initialized || !m_2DRender || !TheGameEngine ||
        !TheMessageStream || !ApplicationHWnd || !TheShell || !TheWindowManager || !TheMouse ||
        !TheKeyboard || !TheGameClient || !TheDisplayStringManager) {
        StartupDiagnostics::checkpoint("menu_host", "fail", "preconditions", 1);
        return 1;
    }
    // 10 minutes absolute, 2 minutes without any input: both are normal bounded ends.
    const unsigned kTotalMs = 600000, kIdleMs = 120000;
    MenuCounters c;
    int result = 1;
    const char *exitReason = "failure";
    unsigned elapsed = 0;
    const char *phase = "menu_shell";
    WindowTranslator translator;
    try {
        TheShell->setPushGuard(MenuPushGuard);
        StartupDiagnostics::checkpoint(phase, "begin", "intro finished (movies off); showShell -> push MainMenu.wnd; shell map off");
        TheGameClient->startMenuOnlyShell();
        WindowLayout *top = TheShell->top();
        if (!top || top->getFilename().compareNoCase("Menus/MainMenu.wnd") != 0)
            throw std::runtime_error("Shell did not present Menus/MainMenu.wnd");
        char detail[192];
        sprintf_s(detail, "top=%s screens=%d winCursors=%d windows loaded by the real manager; transitions real",
            top->getFilename().str(), static_cast<int>(TheShell->getScreenCount()),
            TheGlobalData->m_winCursors ? 1 : 0);
        StartupDiagnostics::checkpoint(phase, "complete", detail);

        phase = "menu_loop";
        StartupDiagnostics::checkpoint(phase, "begin", "serviceWindowsOS; keyboard/mouse; WindowTranslator; window manager; shell; draw");
        const ULONGLONG start = GetTickCount64();
        ULONGLONG lastInput = start;
        unsigned lastInputCount = 0;
        bool lastVisible = false;
        for (;;) {
            TheGameEngine->serviceWindowsOS(); // ordinary WndProc -> Win32Mouse events, WM_CLOSE
            if (!IsWindow(ApplicationHWnd)) throw std::runtime_error("Application window destroyed");
            const bool iconic = IsIconic(ApplicationHWnd) != FALSE;
            if (!iconic && !NativeOrdinaryHost::clientIsExact(ApplicationHWnd))
                throw std::runtime_error("Unsupported resize: client is not 800x600");

            TheKeyboard->UPDATE(); TheKeyboard->createStreamMessages();
            TheMouse->UPDATE(); TheMouse->createStreamMessages();
            DrainMenuStream(translator, c, true);

            if (TheAnim2DCollection) TheAnim2DCollection->UPDATE();
            TheWindowManager->UPDATE();            // processes destroy list + transitions
            TheDisplay->DRAW();                    // ordinary entry: winRepaint + cursor, UI only
            TheDisplayStringManager->update();
            TheShell->UPDATE();                    // layout updates (MainMenuUpdate, animations)
            DrainMenuStream(translator, c, false); // requests raised by callbacks

            const bool visible = IsWindowVisible(ApplicationHWnd) && !iconic;
            if (!iconic) {
                const PresentationStatus present = WW3D::Native_Presentation_Status();
                if (present.result != 0) {
                    sprintf_s(detail, "Present HRESULT=0x%08X removed=0x%08X frame=%u", present.result, present.removed_reason, c.frames + 1);
                    throw std::runtime_error(detail);
                }
                ++c.frames; if (present.occluded) ++c.occludedFrames;
                lastVisible = visible && !present.occluded;
                if (lastVisible) ++c.visibleFrames;
                if (c.frames == 1) {
                    sprintf_s(detail, "HRESULT=0x%08X visible=%u occluded=%u removed=0x%08X frame=%u",
                        present.result, visible ? 1u : 0u, present.occluded ? 1u : 0u, present.removed_reason, c.frames);
                    StartupDiagnostics::checkpoint("graphics_present", lastVisible ? "complete" : "observed", detail, present.result);
                }
            }
            const unsigned inputCount = c.mouseMoves + c.leftDown + c.leftUp + c.keyDown;
            if (inputCount != lastInputCount) {
                if (lastInputCount == 0) {
                    sprintf_s(detail, "first input: moves=%u down=%u up=%u keys=%u top=%s", c.mouseMoves, c.leftDown, c.leftUp, c.keyDown,
                        TheShell->top() ? TheShell->top()->getFilename().str() : "none");
                    StartupDiagnostics::checkpoint("menu_input", "observed", detail);
                }
                lastInputCount = inputCount; lastInput = GetTickCount64();
            }
            elapsed = static_cast<unsigned>(GetTickCount64() - start);
            if (g_menuBoundary.hit) { exitReason = "boundary"; break; }
            if (c.closeRequested) { exitReason = "window_close"; break; }
            if (TheGameEngine->getQuitting()) { exitReason = "menu_exit"; break; }
            if (elapsed >= kTotalMs) { exitReason = "time_bound"; break; }
            if (GetTickCount64() - lastInput >= kIdleMs) { exitReason = "idle_bound"; break; }
            Sleep(15);
        }
        result = c.frames ? 0 : 1;
        if (!c.visibleFrames) {
            StartupDiagnostics::checkpoint(phase, "fail", "no visible unoccluded frame", 1);
            result = 1;
        }
    } catch (const std::exception &error) {
        StartupDiagnostics::checkpoint(phase, "fail", error.what(), 1);
        result = 1;
    } catch (const ErrorCode error) {
        StartupDiagnostics::checkpoint(phase, "fail", "engine ErrorCode", static_cast<unsigned>(error));
        result = 1;
    } catch (...) {
        StartupDiagnostics::checkpoint(phase, "fail", "unknown exception", 1);
        result = 1;
    }
    TheShell->setPushGuard(nullptr);
    ClipCursor(nullptr); // never leave the cursor clipped to the game window
    if (g_menuBoundary.hit) {
        char detail[256];
        sprintf_s(detail, "reason=%s %s", g_menuBoundary.reason, g_menuBoundary.detail);
        StartupDiagnostics::checkpoint("menu_boundary", "observed", detail);
    }
    char summary[256];
    sprintf_s(summary, "frames=%u visible_frames=%u occluded_frames=%u elapsed_ms=%u moves=%u down=%u up=%u keys=%u exit=%s driver=hardware",
        c.frames, c.visibleFrames, c.occludedFrames, elapsed, c.mouseMoves, c.leftDown, c.leftUp, c.keyDown, exitReason);
    StartupDiagnostics::checkpoint("menu_summary", result ? "fail" : "complete", summary, result);
    if (result) StartupDiagnostics::markFailed();
    return result;
#endif
}
// ---------------------------------------------------------------------------
// Ordinary-game profile (StartupDiagnostics::ordinaryGame): the ordinary engine
// loop (Win32GameEngine::update + FramePacer, exactly what GameEngine::execute
// iterates) on the headless world/audio/radar selection with the real UI
// lifetime. The original MainMenu -> Skirmish setup -> Start path runs through
// the legacy Shell and GameLogic::startNewGame. Nothing is rendered for the
// world (the draw is the UI-only frame); this proves game logic, map loading and
// the in-game UI on AMD64 inside the real application. Boundaries: the Shell push
// allow-list below, a logic-frame bound and a wall-clock bound.
// ---------------------------------------------------------------------------
#if RTS_GENERALS
extern unsigned nativeFloatingPointModeCalls(); // GameLogic.cpp
namespace {
Bool GamePushGuard(const AsciiString &filename) {
    if (filename.compareNoCase("Menus/MainMenu.wnd") == 0 ||
        filename.compareNoCase("Menus/SkirmishGameOptionsMenu.wnd") == 0)
        return TRUE;
    RecordBoundary("shell_push", filename.str());
    return FALSE;
}

// Message tap (play profile): counts the user's commands as they pass the client translators, after
// the command translators have produced them and before the dispatcher consumes them. Observation
// only (always KEEP_MESSAGE); the counters feed the bounded play receipts.
struct PlayCounters {
    unsigned leftDown = 0, rightDown = 0, keys = 0, wheel = 0;
    unsigned selections = 0, moves = 0, attacks = 0, stops = 0, guards = 0;
    unsigned builds = 0, produced = 0, upgrades = 0, rallies = 0, sells = 0;
};
PlayCounters g_play;
class PlayTap : public GameMessageTranslator {
public:
    GameMessageDisposition translateGameMessage(const GameMessage *msg) override {
        switch (msg->getType()) {
        case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN: ++g_play.leftDown; break;
        case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN: ++g_play.rightDown; break;
        case GameMessage::MSG_RAW_KEY_DOWN: ++g_play.keys; break;
        case GameMessage::MSG_RAW_MOUSE_WHEEL: ++g_play.wheel; break;
        case GameMessage::MSG_CREATE_SELECTED_GROUP:
        case GameMessage::MSG_CREATE_SELECTED_GROUP_NO_SOUND: ++g_play.selections; break;
        case GameMessage::MSG_DO_MOVETO:
        case GameMessage::MSG_DO_ATTACKMOVETO:
        case GameMessage::MSG_DO_FORCEMOVETO: ++g_play.moves; break;
        case GameMessage::MSG_DO_ATTACK_OBJECT:
        case GameMessage::MSG_DO_FORCE_ATTACK_OBJECT:
        case GameMessage::MSG_DO_FORCE_ATTACK_GROUND: ++g_play.attacks; break;
        case GameMessage::MSG_DO_STOP: ++g_play.stops; break;
        case GameMessage::MSG_DO_GUARD_POSITION: ++g_play.guards; break;
        case GameMessage::MSG_DOZER_CONSTRUCT:
        case GameMessage::MSG_DOZER_CONSTRUCT_LINE: ++g_play.builds; break;
        case GameMessage::MSG_QUEUE_UNIT_CREATE: ++g_play.produced; break;
        case GameMessage::MSG_QUEUE_UPGRADE: ++g_play.upgrades; break;
        case GameMessage::MSG_SET_RALLY_POINT: ++g_play.rallies; break;
        case GameMessage::MSG_SELL: ++g_play.sells; break;
        default: break;
        }
        return KEEP_MESSAGE;
    }
};
// The message stream owns every attached translator (MessageStream::TranslatorData deletes it on
// removal), so the tap must be heap-allocated; a static instance made removeTranslator() free a
// non-heap pointer at Exit Game (X64.28 run 20261013T1151501824715Z).

// Bounded scripted camera exercise (X64.26): six deterministic look-at moves relative to the
// start pivot, each large enough (700 world units, > half the 129-cell draw window of 1290) to
// re-center the terrain draw window and rewrite vertex-buffer tiles through the ordinary
// updateCenter/updateBlock path; a probe is requested 90 logic frames after every move.
void DriveTerrainExercise(unsigned relativeFrame)
{
    static const unsigned kMoveAt[] = {120, 330, 540, 750, 960, 1170};
    static const float kDx[] = {700, 700, 0, -700, -700, 0};
    static const float kDy[] = {0, 700, 700, 700, 0, -700};
    static Coord3D origin = {0, 0, 0};
    static unsigned nextMove = 0, nextProbe = 0;
    for (; nextMove < 6 && relativeFrame >= kMoveAt[nextMove]; ++nextMove) {
        if (nextMove == 0) origin = TheTacticalView->getPosition();
        Region3D extent = {};
        TheTerrainLogic->getExtent(&extent);
        Coord3D target = origin;
        target.x += kDx[nextMove]; target.y += kDy[nextMove];
        const float margin = 300.0f;
        if (target.x < extent.lo.x + margin) target.x = extent.lo.x + margin;
        if (target.y < extent.lo.y + margin) target.y = extent.lo.y + margin;
        if (target.x > extent.hi.x - margin) target.x = extent.hi.x - margin;
        if (target.y > extent.hi.y - margin) target.y = extent.hi.y - margin;
        char detail[160];
        sprintf_s(detail, "step=%u logic_frame=%u target=(%.0f,%.0f) origin=(%.0f,%.0f)", nextMove + 1, relativeFrame,
            target.x, target.y, origin.x, origin.y);
        StartupDiagnostics::checkpoint("terrain_move", "observed", detail);
        TheTacticalView->lookAt(&target);
    }
    if (nextProbe < nextMove && relativeFrame >= kMoveAt[nextProbe] + 90 && g_motionStepRequest == 0) {
        g_motionStepRequest = nextProbe + 1;
        ++nextProbe;
    }
}
} // namespace
#endif

int W3DDisplay::runOrdinaryGameHost()
{
#if !RTS_GENERALS
    StartupDiagnostics::checkpoint("game_host", "fail", "Generals only", 1);
    return 1;
#else
    if (!StartupDiagnostics::ordinaryGame() || !m_initialized || !m_2DRender || !TheGameEngine ||
        !TheShell || !TheWindowManager || !TheGameLogic || !TheFramePacer || !ApplicationHWnd) {
        StartupDiagnostics::checkpoint("game_host", "fail", "preconditions", 1);
        return 1;
    }
    // The play profile is user driven: no scripted camera and no logic-frame bound; the wall-clock
    // bound is the only unattended end (60 minutes), the user normally leaves through the game.
    const bool play = StartupDiagnostics::ordinaryPlay();
    const unsigned kTotalMs = play ? 3600000 : 900000;
    const unsigned kLogicFrameBound = 1800;  // 60 s of logic at 30 fps after the game starts
    TranslatorID playTap = 0;
    unsigned nextPlayReceipt = 0, playReceipts = 0, playImages = 0;
    const unsigned kMilestones[] = { 1, 30, 150, 300, 900, 1800 };
    unsigned nextMilestone = 0, updates = 0, elapsed = 0, startFrame = 0, lastFrame = 0;
    int result = 1;
    const char *exitReason = "failure";
    const char *phase = "game_loop";
    bool inGame = false, exiting = false;
    unsigned exitingUpdates = 0;
    char detail[640];   // terrain_summary alone is ~270 characters (X64.27 run 20261013T0630289162651Z overflowed 256)
    try {
        TheShell->setPushGuard(GamePushGuard);
        if (play) NativeWorldEffects::install();   // decal / planar shadow manager of the play profile
        StartupDiagnostics::checkpoint(phase, "begin",
            "Win32GameEngine::update + FramePacer (ordinary loop); shell map off; push allow-list MainMenu/SkirmishGameOptionsMenu");
        const ULONGLONG start = GetTickCount64();
        for (;;) {
            TheGameEngine->update();          // GameClient (+draw), message stream, GameLogic, OS pump
            TheFramePacer->update();
            ++updates;
            if (!IsWindow(ApplicationHWnd)) throw std::runtime_error("Application window destroyed");
            if (!IsIconic(ApplicationHWnd) && !NativeOrdinaryHost::clientIsExact(ApplicationHWnd))
                throw std::runtime_error("Unsupported resize: client is not the configured size");
            if (m_hostPresentResult != 0) {
                sprintf_s(detail, "Present HRESULT=0x%08X frame=%u", m_hostPresentResult, m_hostPresents);
                throw std::runtime_error(detail);
            }
            if (m_hostPresents == 1 && !m_hostFirstLogged) {
                m_hostFirstLogged = true;
                sprintf_s(detail, "HRESULT=0x%08X visible=1 occluded=%u removed=0x00000000 frame=1",
                    m_hostPresentResult, m_hostPresentOccluded ? 1u : 0u);
                StartupDiagnostics::checkpoint("graphics_present", m_hostPresentOccluded ? "observed" : "complete", detail);
            }
            elapsed = static_cast<unsigned>(GetTickCount64() - start);

            // MSG_NEW_GAME sets the game mode one logic update BEFORE startNewGame runs
            // (m_startNewGame), so mode alone is not "started": require loading finished
            // and the player list built (neutral + at least two skirmish sides).
            const bool nowInGame = TheGameLogic->isInGame() && !TheGameLogic->isInShellGame() &&
                !TheGameLogic->isLoadingMap() && ThePlayerList && ThePlayerList->getPlayerCount() >= 2;
            if (nowInGame && !inGame) {
                inGame = true;
                startFrame = TheGameLogic->getFrame();
                sprintf_s(detail, "mode=%d map=%s players=%d frame=%u", static_cast<int>(TheGameLogic->getGameMode()),
                    TheGlobalData->m_mapName.str(), ThePlayerList ? ThePlayerList->getPlayerCount() : -1, startFrame);
                StartupDiagnostics::checkpoint("game_start", "complete", detail);
                if (play && TheMessageStream)
                    playTap = TheMessageStream->attachTranslator(MSGNEW("PlayTap") PlayTap, 998000000);
            }
            // Only while the logic is still in the game: the iteration that observes the player leaving the
            // match runs after clear-game-data reset the frame counter, which must not replace the last in-game
            // frame (X64.28 fix1 run 20261013T1216116100741Z reported logic_frames=0 after 6000+ frames).
            if (inGame && !exiting && TheGameLogic->isInGame()) {
                const unsigned frame = TheGameLogic->getFrame();
                lastFrame = frame;
                while (nextMilestone < sizeof(kMilestones) / sizeof(kMilestones[0]) &&
                       frame - startFrame >= kMilestones[nextMilestone]) {
                    sprintf_s(detail, "logic_frame=%u crc=%08X presents=%u", frame, TheGameLogic->getCRC(),
                        m_hostPresents);
                    StartupDiagnostics::checkpoint("game_progress", "observed", detail);
                    ++nextMilestone;
                }
                if (play && frame - startFrame >= nextPlayReceipt && playReceipts < 120) {
                    // Bounded progress evidence of what the player did and what the world pass drew.
                    const NativeWorldObjects::Stats os = NativeWorldObjects::stats();
                    Player *local = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
                    const Coord3D camera = TheTacticalView ? TheTacticalView->getPosition() : Coord3D{0, 0, 0};
                    sprintf_s(detail, "logic_frame=%u crc=%08X money=%u selected=%d cam=(%.0f,%.0f) objects_drawn=%u triangles=%u"
                        " left=%u right=%u keys=%u wheel=%u select=%u move=%u attack=%u stop=%u guard=%u build=%u produce=%u upgrade=%u rally=%u sell=%u",
                        frame, TheGameLogic->getCRC(), local && local->getMoney() ? local->getMoney()->countMoney() : 0u,
                        TheInGameUI ? TheInGameUI->getSelectCount() : -1, camera.x, camera.y, os.lastObjectsDrawn, os.lastTriangles,
                        g_play.leftDown, g_play.rightDown, g_play.keys, g_play.wheel, g_play.selections, g_play.moves,
                        g_play.attacks, g_play.stops, g_play.guards, g_play.builds, g_play.produced, g_play.upgrades,
                        g_play.rallies, g_play.sells);
                    StartupDiagnostics::checkpoint("play_progress", "observed", detail);
                    ++playReceipts;
                    nextPlayReceipt = (frame - startFrame) + (playReceipts < 4 ? 450 : 1800);   // 15 s x3, then 60 s
                    if (playImages < 3) {
                        // Composed frames (world + UI) next to the log: evidence of what was on screen.
                        if (auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend())) {
                            char name[48];
                            sprintf_s(name, "play_frame_%u.bmp", ++playImages);
                            if (!WriteBmp(name, backend->Readback_RGBA8(), TheDisplay->getWidth(), TheDisplay->getHeight()))
                                StartupDiagnostics::checkpoint("play_image", "observed", "image not written");
                        }
                    }
                }
                if (!play && frame - startFrame >= kLogicFrameBound) {
                    // Ordinary exit message; processed on the next updates.
                    TheGameLogic->exitGame();
                    exiting = true;
                    exitReason = "frame_bound";
                }
            }
            if (StartupDiagnostics::ordinaryTerrain() && !play && inGame && !exiting && TheTacticalView && TheTerrainLogic)
                DriveTerrainExercise(TheGameLogic->getFrame() - startFrame);
            if (play && inGame && !exiting && !TheGameLogic->isInGame()) {
                // The player left the match (quit menu, victory/defeat flow): ordinary clear-game-data ran.
                StartupDiagnostics::checkpoint("game_exit", "complete", "match ended by the player or the rules; logic not in game");
                exitReason = "game_ended";
                break;
            }
            if (exiting && ++exitingUpdates > 600)
                throw std::runtime_error("Game did not exit after exitGame()");
            if (exiting && !TheGameLogic->isInGame()) {
                StartupDiagnostics::checkpoint("game_exit", "complete", "MSG_CLEAR_GAME_DATA processed; logic not in game");
                break;
            }
            if (g_menuBoundary.hit) { exitReason = "boundary"; break; }
            if (TheGameEngine->getQuitting()) { if (!exiting) exitReason = inGame ? "game_quit" : "menu_exit"; break; }
            if (elapsed >= kTotalMs) { exitReason = "time_bound"; break; }
        }
        result = m_hostPresents ? 0 : 1;
    } catch (const std::exception &error) {
        StartupDiagnostics::checkpoint(phase, "fail", error.what(), 1);
        result = 1;
    } catch (const ErrorCode error) {
        StartupDiagnostics::checkpoint(phase, "fail", "engine ErrorCode", static_cast<unsigned>(error));
        result = 1;
    } catch (...) {
        StartupDiagnostics::checkpoint(phase, "fail", "unknown exception", 1);
        result = 1;
    }
    TheShell->setPushGuard(nullptr);
    ClipCursor(nullptr);
    if (playTap && TheMessageStream) TheMessageStream->removeTranslator(playTap);
    if (g_menuBoundary.hit) {
        sprintf_s(detail, "reason=%s %s", g_menuBoundary.reason, g_menuBoundary.detail);
        StartupDiagnostics::checkpoint("menu_boundary", "observed", detail);
    }
    if (play) {
        sprintf_s(detail, "receipts=%u images=%u left=%u right=%u keys=%u wheel=%u select=%u move=%u attack=%u stop=%u guard=%u"
            " build=%u produce=%u upgrade=%u rally=%u sell=%u", playReceipts, playImages, g_play.leftDown, g_play.rightDown,
            g_play.keys, g_play.wheel, g_play.selections, g_play.moves, g_play.attacks, g_play.stops, g_play.guards,
            g_play.builds, g_play.produced, g_play.upgrades, g_play.rallies, g_play.sells);
        StartupDiagnostics::checkpoint("play_summary", "complete", detail);
    }
    sprintf_s(detail, "calls=%u (first receipts bounded; verified every call)", nativeFloatingPointModeCalls());
    StartupDiagnostics::checkpoint("FP_summary", "observed", detail);
    if (StartupDiagnostics::ordinaryTerrain()) {
        sprintf_s(detail, "frames=%u draws=%u atlas_uploads=%u tile_uploads=%u tile_updates=%u cloud_frames=%u noise_frames=%u macro_loads=%u"
            " extra_frames=%u extra_tiles=%u road_frames=%u road_draws=%u bridge_frames=%u bridge_draws=%u shore_frames=%u shore_tiles=%u texture_loads=%u",
            g_terrainFrames, g_terrainDraws, g_terrainStats.atlasUploads, g_terrainStats.tileUploads,
            g_terrainStats.tileUpdates, g_terrainStats.cloudFrames, g_terrainStats.noiseFrames, g_terrainStats.macroLoads,
            g_terrainStats.extraFrames, g_terrainStats.extraTiles, g_terrainStats.roadFrames, g_terrainStats.roadDraws,
            g_terrainStats.bridgeFrames, g_terrainStats.bridgeDraws, g_terrainStats.shoreFrames, g_terrainStats.shoreTiles,
            g_terrainStats.worldTextureLoads);
        StartupDiagnostics::checkpoint("terrain_summary", g_terrainFrames ? "complete" : "fail", detail, g_terrainFrames ? 0 : 1);
        if (!g_terrainFrames) result = 1;
        if (play) {
            const NativeWorldObjects::Stats os = NativeWorldObjects::stats();
            sprintf_s(detail, "frames=%u registered=%u objects=%u objects_drawn=%u hidden=%u culled=%u meshes=%u meshes_drawn=%u skinned=%u"
                " batches=%u translucent_batches=%u vertices=%u triangles=%u texture_loads=%u texture_failures=%u team_colored=%u"
                " unsupported_class=%u unsupported_state=%u chunk_overflow=%u",
                os.frames, NativeWorldObjects::registered(), os.objects, os.objectsDrawn, os.hidden, os.culled, os.meshes,
                os.meshesDrawn, os.skinned, os.batches, os.translucentBatches, os.vertices, os.triangles, os.textureLoads,
                os.textureFailures, os.teamColored, os.unsupportedClass, os.unsupportedState, os.chunkOverflow);
            StartupDiagnostics::checkpoint("world_objects_summary", "complete", detail);
            // Family breakdown of unsupported_state (X64.29); a separate receipt keeps the accepted summary format.
            sprintf_s(detail, "multipass=%u mapper=%u blend=%u modulate2x=%u secondary=%u detail=%u mapper_kinds=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                os.stateMultiPass, os.stateMapper, os.stateBlend, os.stateModulate2x, os.stateSecondary, os.stateDetail,
                os.mapperKinds[1], os.mapperKinds[2], os.mapperKinds[3], os.mapperKinds[4], os.mapperKinds[5],
                os.mapperKinds[8], os.mapperKinds[9], os.mapperKinds[10], os.mapperKinds[11], os.mapperKinds[12], os.mapperKinds[13]);
            StartupDiagnostics::checkpoint("world_objects_families", "observed", detail);
            const NativeWorldEffects::Stats fx = NativeWorldEffects::stats();
            sprintf_s(detail, "lines=%u seglines=%u segquads=%u particle_systems=%u particles=%u particle_batches=%u streaks=%u"
                " volume_layers=%u culled=%u smudge_skipped=%u decals=%u shadow_decals=%u volume_shadows=%u shadows_seen=%u"
                " shadow_no_geometry=%u water=%u shroud_cells=%u scorches=%u dropped=%u effect_batches=%u",
                fx.lineBoxes, fx.segLines, fx.segQuads, fx.particleSystems, fx.particlesDrawn, fx.particleBatches, fx.streaks,
                fx.volumeLayers, fx.particlesCulled, fx.smudgeSystems, fx.decals, fx.shadowDecals, fx.volumeShadows,
                fx.shadowsSeen, fx.shadowNoGeometry, fx.waterPatches, fx.shroudCells, fx.scorches, fx.dropped,
                NativeWorldObjects::stats().effectBatches);
            StartupDiagnostics::checkpoint("world_effects_summary", "observed", detail);
            // X64.30 world overlays (separate receipt: the accepted summary format is unchanged).
            sprintf_s(detail, "tracks=%u bibs=%u ground_lines=%u water_sparkles=%u water_rivers=%u",
                fx.tracks, fx.bibs, fx.groundLines, fx.waterSparkles, fx.waterRivers);
            StartupDiagnostics::checkpoint("world_overlays", "observed", detail);
            // Particle pipeline: where effects are lost between the FX request and the D3D11 submission.
            sprintf_s(detail, "requested=%u null_template=%u created=%u removed=%u alive=%u peak_systems=%u particles_added=%u"
                " alive_particles=%u peak_particles=%u manager_updates=%u manager_draws=%u systems_with_particles=%u"
                " quads_emitted=%u batches=%u streaks=%u submitted_by_layer=%u,%u,%u,%u,%u",
                fx.pipelineRequested, fx.pipelineNullTemplate, fx.pipelineCreated, fx.pipelineRemoved, fx.systemsAlive,
                fx.pipelinePeakSystems, fx.pipelineParticlesAdded, fx.particlesAlive, fx.particlesAliveMax,
                fx.pipelineUpdates, fx.pipelineDraws, fx.particleSystems, fx.particlesDrawn, fx.particleBatches,
                fx.streaks, os.layerDraws[0], os.layerDraws[1], os.layerDraws[2], os.layerDraws[3], os.layerDraws[4]);
            StartupDiagnostics::checkpoint("particle_pipeline", "observed", detail);
            for (unsigned i = 0; i < NativeWorldObjects::failedTextureCount() && i < 12; ++i) {
                sprintf_s(detail, "texture=%.200s", NativeWorldObjects::failedTexture(i));
                StartupDiagnostics::checkpoint("world_objects_texture_failure", "observed", detail);
            }
        }
    }
    sprintf_s(detail, "presents=%u updates=%u elapsed_ms=%u game=%u logic_frames=%u exit=%s driver=hardware",
        m_hostPresents, updates, elapsed, inGame ? 1u : 0u,
        inGame ? lastFrame - startFrame : 0u, exitReason);
    StartupDiagnostics::checkpoint("game_summary", result ? "fail" : "complete", detail, result);
    if (result) StartupDiagnostics::markFailed();
    return result;
#endif
}
// Normal-launch screenshot (the legacy path reads DX8 surfaces): the composed target of the native backend
// written as BMP under <UserData>\Screenshots. Returns false when nothing could be written.
bool saveNativeScreenshot()
{
    auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend());
    if (!backend || !TheGlobalData) return false;
    char directory[MAX_PATH];
    sprintf_s(directory, "%sScreenshots", TheGlobalData->getPath_UserData().str());
    CreateDirectoryA(directory, nullptr);
    for (unsigned n = 0; n < 10000; ++n) {
        char path[MAX_PATH];
        sprintf_s(path, "%s\Generals64_%04u.bmp", directory, n);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) continue;
        return WriteBmpFile(path, backend->Readback_RGBA8(), TheDisplay->getWidth(), TheDisplay->getHeight());
    }
    return false;
}

#endif
