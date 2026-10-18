// SPDX-License-Identifier: GPL-3.0-or-later
// Ordinary-host profile display lifetime (X64.22). Kept separate from the
// attached diagnostic in W3DDisplayPresentation.cpp.
#if defined(_WIN64)
#include "W3DDevice/GameClient/W3DDisplay.h"
#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
#include <tuple>
#include <vector>
#include "Common/GlobalData.h"
#include "Common/Errors.h"
#include "Common/StartupDiagnostics.h"
#include "Common/NativeOrdinaryHost.h"
#include "Common/DisplayRuntime.h"
#include "Common/EnhancedHud.h"
#include "Common/DisplaySettings.h"
#include "Common/DisplayModes.h"
#include "Common/ModernExperience.h"
#include "Common/GroundTranslation.h"
#include "Common/PerformanceProfile.h"
#include "GameClient/ParticleSys.h"
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
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/WinInstanceData.h"
#include "GameClient/GUICallbacks.h"
#include "Common/KindOf.h"
#include "Common/XferCRC.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/AI.h"
#include "Common/PlayerTemplate.h"
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
    if (!borderless)
        wanted.resolution = { static_cast<int>(width), static_cast<int>(height) };   // borderless keeps the remembered windowed size
    const DisplayConfig::Environment env = enumerateEnvironment();
    wanted.monitor = monitorIndex >= 0 ? monitorIndex : DisplayRuntime::state().resolved.monitorIndex;
    wanted.monitorName = wanted.monitor >= 0 && wanted.monitor < static_cast<int>(env.monitors.size())
        ? DisplayConfig::monitorId(env.monitors[static_cast<size_t>(wanted.monitor)]) : std::string();
    const DisplayConfig::Resolved r = DisplayConfig::resolve(wanted, env);
    if (!r.client.valid()) return false;
    try {
        placeWindow(ApplicationHWnd, r);
        DisplayRuntime::State &state = DisplayRuntime::state();
        const DisplayGeometry::Size previous = state.resolved.client;
        state.settings = wanted;
        state.settings.monitor = r.monitorIndex;
        state.settings.monitorName = r.monitorName;
        if (r.mode == DisplayConfig::WindowMode::Windowed)
            state.settings.resolution = r.client;      // the size actually in use (it may have been reduced to fit the desktop)
        state.resolved = r;
        DisplayRuntime::setClient(r.client);
        if (previous != r.client) {
            WW3D::Resize_Native_Presentation(r.client.width, r.client.height);
            Render2DClass::Set_Screen_Resolution(RectClass(0, 0, r.client.width, r.client.height));
            // X64.37: the logical display follows the swap chain whichever setting caused the new size (resolution, a switch to
            // borderless on a desktop of another size, another monitor): width/height, the 2D coordinate range, the mouse limits
            // and the tactical view proportions. Display::setDisplayMode is idempotent for an unchanged size.
            if (TheTacticalView)
                TheDisplay->Display::setDisplayMode(r.client.width, r.client.height, 32, TRUE);
            else {
                TheDisplay->setWidth(r.client.width);
                TheDisplay->setHeight(r.client.height);
            }
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

// X64.34: per-frame deltas of the native backend's cumulative counters plus two world gauges, attributed to the
// current outer frame of an active profiler capture (the sample point is the start of the display draw, so a frame's
// numbers describe the preceding draw pass). Free when no capture is running.
static bool g_countersPrimed = false;
// X64.35 baseline2: texture-creation attribution for the summary. The table is cumulative in the backend; a capture
// reports the difference to the snapshot taken when it started.
static std::map<std::tuple<std::string, unsigned, unsigned, unsigned, unsigned>, std::pair<unsigned long long, unsigned long long>> g_textureSnapshot;
static D3D11Backend::RenderStats g_captureStartStats;
static void NativeCaptureStarted()
{
    g_textureSnapshot.clear();
    if (auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend())) {
        for (const auto &r : backend->Texture_Creation_Table())
            g_textureSnapshot[{r.label, r.entry, r.width, r.height, r.format}] = {r.created, r.bytes};
        g_captureStartStats = backend->Render_Stats();
    }
}
static std::string NativeCaptureReport()
{
    auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend());
    if (!backend) return std::string();
    struct Row { std::string label; unsigned entry, w, h, fmt; unsigned long long created, bytes; };
    std::vector<Row> rows;
    unsigned long long total[3] = {0, 0, 0}, bytes[3] = {0, 0, 0};
    for (const auto &r : backend->Texture_Creation_Table()) {
        unsigned long long c0 = 0, b0 = 0;
        auto it = g_textureSnapshot.find({r.label, r.entry, r.width, r.height, r.format});
        if (it != g_textureSnapshot.end()) { c0 = it->second.first; b0 = it->second.second; }
        if (r.created <= c0) continue;
        rows.push_back({r.label, r.entry, r.width, r.height, r.format, r.created - c0, r.bytes - b0});
        if (r.entry < 3) { total[r.entry] += r.created - c0; bytes[r.entry] += r.bytes - b0; }
    }
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.created > b.created; });
    std::string out = "\n=== Texture creation attribution (this capture) ===\n";
    char line[640];
    snprintf(line, sizeof(line), "by_entry: content_file created %llu (%llu KB), dynamic_ui created %llu (%llu KB), mipped created %llu (%llu KB)\n",
             total[0], bytes[0] >> 10, total[1], bytes[1] >> 10, total[2], bytes[2] >> 10);
    out += line;
    out += "rows (top 30 by creations): entry(0 content,1 dynamic,2 mipped) WxH dxgi_format created kb label\n";
    for (size_t i = 0; i < rows.size() && i < 30; ++i) {
        snprintf(line, sizeof(line), "%u %ux%u %u %llu %llu %s\n", rows[i].entry, rows[i].w, rows[i].h, rows[i].fmt, rows[i].created,
                 rows[i].bytes >> 10, rows[i].label.empty() ? "(untagged)" : rows[i].label.c_str());
        out += line;
    }
    const D3D11Backend::RenderStats now = backend->Render_Stats();
    snprintf(line, sizeof(line),
             "textures_destroyed_this_capture: %llu  live_textures_now: %u  dynamic_updates: %llu (%llu KB)\n"
             "gpu_timestamp_frames_discarded_disjoint: %llu  ring_full_skipped: %llu\n=== end texture attribution ===\n",
             now.textures_destroyed - g_captureStartStats.textures_destroyed, backend->Live_Textures(),
             now.texture_updates - g_captureStartStats.texture_updates, (now.texture_update_bytes - g_captureStartStats.texture_update_bytes) >> 10,
             now.gpu_disjoint - g_captureStartStats.gpu_disjoint, now.gpu_skipped - g_captureStartStats.gpu_skipped);
    out += line;
    return out;
}
void NativeRegisterPerformanceExtensions()
{
    PerformanceProfile::setCaptureExtension(&NativeCaptureStarted, &NativeCaptureReport);
}
void NativeSampleRenderCounters()
{
    if (!PerformanceProfile::activeRecorder) {
        if (g_countersPrimed) {   // capture ended: stop issuing GPU timestamp queries
            if (auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend())) backend->Set_Gpu_Timing(false);
        }
        g_countersPrimed = false;   // the first sample of the next capture only establishes the baseline (no whole-session spike)
        return;
    }
    PerformanceProfile::samplePageFaults();
    static unsigned lastDraws = 0, lastTextures = 0, lastBuffers = 0;
    static D3D11Backend::RenderStats last;
    static bool contextSet = false;
    bool &primed = g_countersPrimed;
    if (!primed) {
        if (auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend())) {
            lastDraws = backend->Draw_Count(); lastTextures = backend->Texture_Creations();
            lastBuffers = backend->Buffer_Creations(); last = backend->Render_Stats();
            backend->Set_Gpu_Timing(true);
        }
        primed = true;
    }
    if (auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend())) {
        const unsigned draws = backend->Draw_Count(), textures = backend->Texture_Creations(), buffers = backend->Buffer_Creations();
        PerformanceProfile::count(PerformanceProfile::Counter::DrawCalls, draws - lastDraws);
        PerformanceProfile::count(PerformanceProfile::Counter::TexturesCreated, textures - lastTextures);
        PerformanceProfile::count(PerformanceProfile::Counter::BuffersCreated, buffers - lastBuffers);
        lastDraws = draws; lastTextures = textures; lastBuffers = buffers;
        const D3D11Backend::RenderStats now = backend->Render_Stats();
        using PC = PerformanceProfile::Counter;
        PerformanceProfile::count(PC::Draws2D, now.draws_2d - last.draws_2d);
        PerformanceProfile::count(PC::DrawsWorld, now.draws_world - last.draws_world);
        PerformanceProfile::count(PC::DrawsWater, now.draws_water - last.draws_water);
        PerformanceProfile::count(PC::Triangles, now.triangles - last.triangles);
        PerformanceProfile::count(PC::StateRequested, now.state_requested - last.state_requested);
        PerformanceProfile::count(PC::StateChanged, now.state_changed - last.state_changed);
        PerformanceProfile::count(PC::TextureBinds, now.texture_binds - last.texture_binds);
        PerformanceProfile::count(PC::ShaderChanges, now.shader_changes - last.shader_changes);
        PerformanceProfile::count(PC::TargetChanges, now.target_changes - last.target_changes);
        PerformanceProfile::count(PC::BufferChanges, now.buffer_changes - last.buffer_changes);
        PerformanceProfile::count(PC::PresentTicks, now.present_ticks - last.present_ticks);
        PerformanceProfile::count(PC::SubmitTicks, now.submit_ticks - last.submit_ticks);
        PerformanceProfile::count(PC::UploadTicks, now.upload_ticks - last.upload_ticks);
        PerformanceProfile::count(PC::GpuMicros, (now.gpu_ns - last.gpu_ns) / 1000);
        PerformanceProfile::count(PC::GpuFrames, now.gpu_frames - last.gpu_frames);
        PerformanceProfile::count(PC::TexturesDestroyed, now.textures_destroyed - last.textures_destroyed);
        PerformanceProfile::count(PC::TextureUpdates, now.texture_updates - last.texture_updates);
        PerformanceProfile::count(PC::TextureUpdateKb, (now.texture_update_bytes - last.texture_update_bytes) >> 10);
        PerformanceProfile::count(PC::TexturesLive, backend->Live_Textures());
        last = now;
        if (!contextSet) {
            contextSet = true;
            const DisplayRuntime::State &state = DisplayRuntime::state();
            char text[512];
            sprintf_s(text, "adapter=[%s]; client=%dx%d; vsync=%d; mode=%s", backend->Adapter_Description(),
                static_cast<int>(TheDisplay ? TheDisplay->getWidth() : 0), static_cast<int>(TheDisplay ? TheDisplay->getHeight() : 0),
                state.resolved.vsync ? 1 : 0, "native-d3d11");
            PerformanceProfile::setRenderContext(text);
        }
    }
    if (TheGameLogic)
        PerformanceProfile::count(PerformanceProfile::Counter::ObjectsLive, TheGameLogic->getObjectCount());
    if (TheParticleSystemManager)
        PerformanceProfile::count(PerformanceProfile::Counter::ParticlesLive, TheParticleSystemManager->getParticleCount());
}

// X64.37 v4: the explicit end of the world phase of a picture (called by W3DDisplay::draw right after drawViews()). With Render Scale below
// 100 % the reduced-resolution world is composited into the back buffer here, before the interface draws; at 100 % it does nothing.
void NativeEndWorldPhase()
{
    if (!DisplayRuntime::modern()) return;
    if (auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend())) backend->End_World_Phase();
}

// Resolution list for a (possibly not yet applied) mode on a monitor (Modern Experience only): real, fitting sizes.
static void NativeModernSizeChoicesFor(bool borderless, int monitorIndex, std::vector<DisplayGeometry::Size> &out)
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
        size_t index = static_cast<size_t>(monitorIndex);
        if (index >= env.monitors.size()) index = 0;
        monitor = env.monitors[index];
    }
    const DisplayConfig::WindowMode mode = borderless ? DisplayConfig::WindowMode::Borderless : DisplayConfig::WindowMode::Windowed;
    out = DisplayConfig::sizeChoices(monitor, mode, env.frameWidth, env.frameHeight,
        (!borderless && state.resolved.mode == DisplayConfig::WindowMode::Windowed) ? state.resolved.client : DisplayGeometry::Size{});
}

// Resolution list for the Options screen (Modern Experience only): the active monitor and mode.
void NativeModernSizeChoices(std::vector<DisplayGeometry::Size> &out)
{
    const DisplayRuntime::State &state = DisplayRuntime::state();
    NativeModernSizeChoicesFor(state.resolved.mode != DisplayConfig::WindowMode::Windowed, state.resolved.monitorIndex, out);
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
        const DisplayConfig::Settings stored = DisplayConfig::load(readOptionKeyValues(), false);
        // command-line display flags of this launch are session overrides: they take precedence over the stored values
        // but are never written back unless the player changes that field in Options and keeps the change
        DisplayConfig::Settings settings = DisplayConfig::applyOverrides(stored, DisplayRuntime::state().overrides);
        const DisplayConfig::Environment env = enumerateEnvironment();
        DisplayConfig::Resolved resolved = DisplayConfig::resolve(settings, env,
            { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) });
        // X64.48A developer aid (normal launch only, off unless GENERALS_FORCE_CLIENT_SIZE=<width>x<height> is set): present at a client
        // size the attached monitors cannot show (e.g. 5120x1440 on a smaller screen) so the whole dimension chain can be captured and
        // tested. It only replaces the resolved client size; every other setting and all layout maths run unchanged.
        if (StartupDiagnostics::normalLaunch()) {
            int forcedW = 0, forcedH = 0;
            const char *forced = std::getenv("GENERALS_FORCE_CLIENT_SIZE");
            if (forced && std::sscanf(forced, "%dx%d", &forcedW, &forcedH) == 2 && forcedW >= 640 && forcedW <= 16384 && forcedH >= 480 && forcedH <= 16384) {
                resolved.client = { forcedW, forcedH };
                resolved.layout = DisplayGeometry::computeLayout(resolved.client, settings.uiScalePercent);
                resolved.notes.push_back("GENERALS_FORCE_CLIENT_SIZE developer override applied");
            }
        }
        DisplayRuntime::State &state = DisplayRuntime::state();
        state.modern = true;
        DisplayRuntime::applyModeHook() = &NativeApplyDisplayMode;
        DisplayRuntime::sizeChoicesHook() = &NativeModernSizeChoicesFor;
        DisplayRuntime::vsyncHook() = [](bool enabled) { WW3D::Set_Native_Vsync(enabled); };
        DisplayRuntime::renderScaleHook() = [](unsigned percent) {
            if (auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend())) backend->Set_World_Render_Scale(percent);
        };
        NativeRegisterPerformanceExtensions();
        if (settings.smoothMotion) GroundTranslation::setEnabled(true);   // Options > Unit Motion; off by default
        EnhancedHud::setRequested(settings.enhancedHud);                  // Options > Interface; Original by default
        state.stored = stored;
        state.settings = settings;
        state.settings.monitor = resolved.monitorIndex;            // what is running, not what was asked for
        state.settings.monitorName = resolved.monitorName;
        if (resolved.mode == DisplayConfig::WindowMode::Windowed)
            state.settings.resolution = resolved.client;
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
    if (modern && WorldRenderScale::active(DisplayRuntime::state().settings.renderScalePercent))
        if (DisplayRuntime::RenderScaleFn setScale = DisplayRuntime::renderScaleHook())
            setScale(DisplayRuntime::state().settings.renderScalePercent);        // the stored Render Scale, before the first frame

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
    {
        PERFORMANCE_PROFILE_SCOPE(NativeBeginRender);
        if (WW3D::Begin_Render(true, false, Vector3(0.0f, 0.0f, 0.0f), 1.0f) != WW3D_ERROR_OK)
            throw std::runtime_error("Ordinary menu Begin_Render failed");
    }
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
            if (auto *view = dynamic_cast<W3DView *>(TheTacticalView)) {
                PERFORMANCE_PROFILE_SCOPE(NativeOverlays);
                view->drawNativeWorldOverlays();
            }
            PERFORMANCE_PROFILE_SCOPE(NativeUi);
            TheInGameUI->DRAW();
        } else if (TheWindowManager) {
            PERFORMANCE_PROFILE_SCOPE(NativeUi);
            TheWindowManager->winRepaint();
        }
        if (TheMouse) {
            PERFORMANCE_PROFILE_SCOPE(NativeMouse);
            TheMouse->DRAW();
        }
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
    {
        PERFORMANCE_PROFILE_SCOPE(NativeEndRender);
        if (WW3D::End_Render(true) != WW3D_ERROR_OK)
            throw std::runtime_error("Ordinary menu End_Render failed");
    }
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
        sprintf_s(path, "%s\\Generals64_%04u.bmp", directory, n);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) continue;
        return WriteBmpFile(path, backend->Readback_RGBA8(), TheDisplay->getWidth(), TheDisplay->getHeight());
    }
    return false;
}

// Developer capture aid (normal launch only, off unless GENERALS_AUTOSHOT=<existing directory>): writes the composed frame of the
// shell once (after 360 presented frames) and of the first live match (after GENERALS_AUTOSHOT_FRAMES, default 300, frames), then
// asks the engine to quit. With -devMode -quickGame this lets a candidate be inspected without anybody at the keyboard.
void NativeAutoShot()
{
    static const char *directory = std::getenv("GENERALS_AUTOSHOT");
    if (!directory || !*directory || !StartupDiagnostics::normalLaunch() || !TheGameLogic || !TheGlobalData || !TheDisplay) return;
    auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend());
    if (!backend) return;
    static const unsigned gameFramesWanted = [] {
        const char *text = std::getenv("GENERALS_AUTOSHOT_FRAMES");
        const int value = text ? std::atoi(text) : 0;
        return value > 0 ? static_cast<unsigned>(value) : 300u;
    }();
    static unsigned shellFrames = 0, gameFrames = 0;
    static bool shellDone = false, gameDone = false;
    auto shoot = [&](const char *tag) {
        if (!strcmp(tag, "game") && TheTacticalView && TheTerrainLogic) {
            // where the view lands on the ground compared with the map: separates "the map is narrower than the view" from a render fault
            Region3D map; TheTerrainLogic->getExtent(&map);
            int ox = 0, oy = 0; TheTacticalView->getOrigin(&ox, &oy);
            const int w = TheTacticalView->getWidth(), h = TheTacticalView->getHeight();
            char footprint[700];
            const Coord3D &cam = TheTacticalView->getPosition();
            int used = sprintf_s(footprint, "map x=%.0f..%.0f y=%.0f..%.0f camera=%.0f,%.0f view=%dx%d at %d,%d ground", map.lo.x, map.hi.x, map.lo.y, map.hi.y, cam.x, cam.y, w, h, ox, oy);
            const int xs[] = { 0, w / 2, w - 1 }, ys[] = { 0, h / 2, h - 1 };
            for (int yi = 0; yi < 3; ++yi) for (int xi = 0; xi < 3; ++xi) {
                ICoord2D sp = { ox + xs[xi], oy + ys[yi] }; Coord3D g;
                if (TheTacticalView->screenToTerrain(&sp, &g)) {
                    float rgb[3] = { 1.0f, 1.0f, 1.0f };
                    NativeWorldEffects::shroudMultiplierAt(g.x, g.y, rgb);
                    used += sprintf_s(footprint + used, sizeof(footprint) - used, " [%d,%d]=%.0f,%.0f/shroud%.2f", xi, yi, g.x, g.y, rgb[0]);
                }
            }
            unsigned shroudCounts[4]; NativeWorldEffects::shroudSummary(shroudCounts);
            used += sprintf_s(footprint + used, sizeof(footprint) - used, " shroud_cells[unexplored=%u fogged=%u clear=%u total=%u]", shroudCounts[0], shroudCounts[1], shroudCounts[2], shroudCounts[3]);
            const NativeWorldEffects::Stats fxs = NativeWorldEffects::stats();
            used += sprintf_s(footprint + used, sizeof(footprint) - used, " shroud_cells_drawn=%u effects_dropped=%u shroud_on=%d", fxs.shroudCells, fxs.dropped, TheGlobalData->m_shroudOn ? 1 : 0);
            StartupDiagnostics::checkpoint("view_footprint", "observed", footprint);
        }
        if (!strcmp(tag, "game") && ThePlayerTemplateStore && TheControlBar) {
            // Feature parity probe (distribution rule: the overlay-installed runtime == the validated staged candidate): what the DATA of this runtime
            // says about the completed modern features - the special-power shortcut bar slot counts and the command buttons each side's shortcut set
            // really holds (the Generals "modern extensions" are loose INI overrides). The overlay install must print exactly the staged candidate's lines.
            for (int i = 0; i < ThePlayerTemplateStore->getPlayerTemplateCount(); ++i) {
                const PlayerTemplate *t = ThePlayerTemplateStore->getNthPlayerTemplate(i);
                if (!t || !t->isPlayableSide()) continue;
                const CommandSet *set = TheControlBar->findCommandSet(t->getSpecialPowerShortcutCommandSet());
                int buttons = 0;
                for (int b = 0; set && b < MAX_COMMANDS_PER_SET; ++b) if (set->getCommandButton(b)) ++buttons;
                char probe[300];
                sprintf_s(probe, "side=%s shortcut_slots=%d shortcut_set=%s shortcut_set_buttons=%d shortcut_window=%s", t->getName().str(),
                          t->getSpecialPowerShortcutButtonCount(), t->getSpecialPowerShortcutCommandSet().str(), buttons, t->getSpecialPowerShortcutWinName().str());
                StartupDiagnostics::checkpoint("feature_probe", "observed", probe);
            }
        }
        char path[MAX_PATH];
        sprintf_s(path, "%s\\autoshot-%s.bmp", directory, tag);
        const bool ok = WriteBmpFile(path, backend->Readback_RGBA8(), TheDisplay->getWidth(), TheDisplay->getHeight());
        StartupDiagnostics::checkpoint("autoshot", ok ? "complete" : "fail", path);
    };
    // GENERALS_AUTOFLOW=skirmish|campaign|challenge|saveload (developer aid): reproduce the player's frontend sequence through the real
    // screens: each step sends the same GBM_SELECTED a mouse click would to the real, visible and enabled button (its owning top window
    // receives it, as for a click), pushes the real screen, or opens the quit menu like Esc. Campaign: Single Player -> USA -> Easy.
    // Challenge: Single Player -> General's Challenge -> Easy -> general GENERALS_AUTOFLOW_GENERAL (default 0) -> Play. Save/Load:
    // Challenge start, play, Esc -> Save/Load -> Save, quit to the main menu, Single Player -> Load Game -> Load, play on, save again.
    // "state" records (frame, side, money, objects, the engine's own logic CRC) before the save and after the load show whether the
    // world came back. Nothing about the world is constructed or edited here.
    enum StepKind { SLOT_FILL, SLOT_PROBE, PRESS, PRESS_EITHER, PRESS_OPTIONAL, PUSH, QUIT_MENU, SELECT_ROW, STATE, WAIT_FRAMES, WAIT_INPUT, WAKE_MENU, END };
    struct Step { StepKind kind; const char *a; const char *b; unsigned wait; int where; };   // where: 0 any, 1 shell, 2 running game
    static const Step skirmishSteps[] = {
        { PUSH, "Menus/SkirmishGameOptionsMenu.wnd", nullptr, 600, 1 },
        { PRESS, "SkirmishGameOptionsMenu.wnd:ButtonStart", nullptr, 240, 1 }, { END, nullptr, nullptr, 0, 0 } };
    static const Step campaignSteps[] = {
        { WAKE_MENU, "MainMenu.wnd:MainMenuParent", nullptr, 600, 1 }, { PRESS, "MainMenu.wnd:ButtonSinglePlayer", nullptr, 60, 1 }, { PRESS, "MainMenu.wnd:ButtonUSA", nullptr, 300, 1 },
        { PRESS, "MainMenu.wnd:ButtonEasy", nullptr, 300, 1 }, { END, nullptr, nullptr, 0, 0 } };
    static const Step challengeSteps[] = {
        { WAKE_MENU, "MainMenu.wnd:MainMenuParent", nullptr, 600, 1 }, { PRESS, "MainMenu.wnd:ButtonSinglePlayer", nullptr, 60, 1 }, { PRESS, "MainMenu.wnd:ButtonChallenge", nullptr, 300, 1 },
        { PRESS, "MainMenu.wnd:ButtonEasy", nullptr, 300, 1 }, { PRESS, "ChallengeMenu.wnd:GeneralPosition", nullptr, 300, 1 },
        { PRESS, "ChallengeMenu.wnd:ButtonPlay", nullptr, 120, 1 }, { END, nullptr, nullptr, 0, 0 } };
    static const Step saveLoadSteps[] = {
        { WAKE_MENU, "MainMenu.wnd:MainMenuParent", nullptr, 600, 1 }, { PRESS, "MainMenu.wnd:ButtonSinglePlayer", nullptr, 60, 1 }, { PRESS, "MainMenu.wnd:ButtonChallenge", nullptr, 300, 1 },
        { PRESS, "MainMenu.wnd:ButtonEasy", nullptr, 300, 1 }, { PRESS, "ChallengeMenu.wnd:GeneralPosition", nullptr, 300, 1 },
        { PRESS, "ChallengeMenu.wnd:ButtonPlay", nullptr, 120, 1 },
        { WAIT_FRAMES, nullptr, nullptr, 300, 2 }, { WAIT_INPUT, nullptr, nullptr, 1, 2 }, { STATE, "before_save", nullptr, 1, 2 },
        { QUIT_MENU, nullptr, nullptr, 60, 2 }, { PRESS, "QuitMenu.wnd:ButtonSaveLoad", nullptr, 120, 2 },
        { SELECT_ROW, "PopupSaveLoad.wnd:ListboxGames", nullptr, 120, 2 }, { PRESS, "PopupSaveLoad.wnd:ButtonSave", nullptr, 30, 2 },
        { PRESS_EITHER, "PopupSaveLoad.wnd:ButtonSaveDescConfirm", "PopupSaveLoad.wnd:ButtonOverwriteConfirm", 60, 2 },
        { STATE, "saved", nullptr, 60, 2 },
        { PRESS, "QuitMenu.wnd:ButtonExit", nullptr, 60, 2 }, { PRESS_EITHER, "QuitMessageBox.wnd:ButtonYes", "MessageBox.wnd:ButtonYes", 120, 2 },
        { PRESS_OPTIONAL, "ScoreScreen.wnd:ButtonOk", "ScoreScreen.wnd:ButtonContinue", 300, 0 },
        { PRESS_OPTIONAL, "ChallengeMenu.wnd:ButtonBack", "ChallengeMenu.wnd:ButtonBack", 300, 0 },
        { WAKE_MENU, "MainMenu.wnd:MainMenuParent", nullptr, 600, 1 }, { PRESS, "MainMenu.wnd:ButtonLoadReplay", nullptr, 60, 1 }, { PRESS, "MainMenu.wnd:ButtonLoadGame", nullptr, 300, 1 },
        { SELECT_ROW, "SaveLoad.wnd:ListboxGames", nullptr, 300, 1 }, { PRESS, "SaveLoad.wnd:ButtonLoad", nullptr, 30, 1 },
        { STATE, "after_load", nullptr, 0, 2 }, { WAIT_FRAMES, nullptr, nullptr, 150, 2 }, { WAIT_INPUT, nullptr, nullptr, 1, 2 },
        { STATE, "after_load_150_frames", nullptr, 1, 2 },
        { QUIT_MENU, nullptr, nullptr, 60, 2 }, { PRESS, "QuitMenu.wnd:ButtonSaveLoad", nullptr, 120, 2 },
        { SELECT_ROW, "PopupSaveLoad.wnd:ListboxGames", nullptr, 120, 2 }, { PRESS, "PopupSaveLoad.wnd:ButtonSave", nullptr, 30, 2 },
        { PRESS_EITHER, "PopupSaveLoad.wnd:ButtonSaveDescConfirm", "PopupSaveLoad.wnd:ButtonOverwriteConfirm", 60, 2 },
        { STATE, "saved_again", nullptr, 60, 2 }, { END, nullptr, nullptr, 0, 0 } };
    // X64.48A: fill every Skirmish slot with an AI through the real combo boxes, then probe each slot's controls the way the mouse reaches them
    static const Step slotSteps[] = {
        { PUSH, "Menus/SkirmishGameOptionsMenu.wnd", nullptr, 600, 1 }, { SLOT_FILL, nullptr, nullptr, 300, 1 },
        { SLOT_PROBE, nullptr, nullptr, 120, 1 }, { END, nullptr, nullptr, 0, 0 } };
    static const int autoFlow = [] {
        const char *text = std::getenv("GENERALS_AUTOFLOW");
        if (!text) return 0;
        if (!_stricmp(text, "skirmish")) return 1;
        if (!_stricmp(text, "campaign")) return 2;
        if (!_stricmp(text, "challenge")) return 3;
        if (!_stricmp(text, "saveload")) return 4;
        if (!_stricmp(text, "slots")) return 5;
        return 0;
    }();
    static unsigned flowFrames = 0, stepAt = 0, stepIndex = 0, waitFromFrame = 0;
    static bool flowFailed = false;
    if (autoFlow && !flowFailed && TheGameEngine && TheWindowManager && TheShell) {
        const Step *steps = autoFlow == 1 ? skirmishSteps : autoFlow == 2 ? campaignSteps : autoFlow == 3 ? challengeSteps : autoFlow == 5 ? slotSteps : saveLoadSteps;
        ++flowFrames;
        const Step &step = steps[stepIndex];
        const bool shell = TheGameLogic->isInShellGame();
        const bool running = TheGameLogic->isInGame() && !shell && !TheGameLogic->isLoadingMap() && !TheGameLogic->isLoadingSave() &&
                             !TheGameLogic->isClearingGameData() && TheGameLogic->getFrame() > 0;
        const bool context = step.where == 0 || (step.where == 1 && shell && !TheGameLogic->isLoadingMap()) || (step.where == 2 && running);
        if (step.kind != END && context && flowFrames >= stepAt + step.wait) {
            char flowText[1400];
            auto topWindow = [](GameWindow *w) { while (w && w->winGetParent()) w = w->winGetParent(); return w; };
            auto usable = [](GameWindow *w) { for (GameWindow *c = w; c; c = c->winGetParent()) if (c->winIsHidden() || !BitIsSet(c->winGetStatus(), WIN_STATUS_ENABLED)) return false; return w != nullptr; };
            auto find = [](const char *name) { return TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(name)); };
            auto advance = [&](const char *what, const char *name) {
                sprintf_s(flowText, "step %u: %s %s", stepIndex, what, name ? name : "");
                StartupDiagnostics::checkpoint("autoflow", "observed", flowText);
                ++stepIndex; stepAt = flowFrames; waitFromFrame = TheGameLogic->getFrame();
            };
            bool done = false;
            if (step.kind == PUSH) {
                TheShell->push(step.a); advance("pushed", step.a); done = true;
            } else if (step.kind == PRESS || step.kind == PRESS_EITHER || step.kind == PRESS_OPTIONAL) {
                AsciiString name = step.a;
                if (!strcmp(step.a, "ChallengeMenu.wnd:GeneralPosition")) {
                    const char *general = std::getenv("GENERALS_AUTOFLOW_GENERAL");
                    name.format("ChallengeMenu.wnd:GeneralPosition%d", general ? std::atoi(general) : 0);
                }
                GameWindow *button = find(name.str());
                if ((step.kind == PRESS_EITHER || step.kind == PRESS_OPTIONAL) && !usable(button)) { name = step.b; button = find(name.str()); }
                if (!usable(button) && !strncmp(name.str(), "MainMenu.wnd:", 13) && flowFrames % 90 == 0) {
                    // the main menu can still be entering when the first key arrives: the player keeps moving the mouse
                    GameWindow *menu = find("MainMenu.wnd:MainMenuParent");
                    if (menu) TheWindowManager->winSendInputMsg(menu, GWM_CHAR, KEY_SPACE, KEY_STATE_DOWN);
                }
                if (usable(button)) {
                    GameWindow *top = button->winGetOwner() ? button->winGetOwner() : topWindow(button);   // the gadget sends its click to its owner
                    TheWindowManager->winSendSystemMsg(top, GBM_SELECTED, (WindowMsgData)button, button->winGetWindowId());
                    advance("pressed", name.str()); done = true;
                }
            } else if (step.kind == SELECT_ROW) {
                GameWindow *list = find(step.a);
                if (usable(list)) {
                    GadgetListBoxSetSelected(list, 0);
                    TheWindowManager->winSendSystemMsg(list->winGetOwner() ? list->winGetOwner() : topWindow(list), GLM_SELECTED, (WindowMsgData)list, 0);
                    advance("selected row 0 of", step.a); done = true;
                }
            } else if (step.kind == SLOT_FILL || step.kind == SLOT_PROBE) {
                GameWindow *parent = find("SkirmishGameOptionsMenu.wnd:SkirmishGameOptionsMenuParent");
                if (parent) {
                    for (int i = 0; i < 8; ++i) {
                        char name[96];
                        sprintf_s(name, "SkirmishGameOptionsMenu.wnd:ComboBoxPlayer%d", i);
                        GameWindow *combo = find(name);
                        if (!combo) { sprintf_s(flowText, "slot %d: combo missing", i); StartupDiagnostics::checkpoint("slotprobe", "fail", flowText); continue; }
                        if (step.kind == SLOT_FILL) {
                            if (i == 0) continue;                                   // the human slot
                            const int entries = GadgetComboBoxGetLength(combo);
                            const int pos = entries > 5 ? 5 : entries - 1;           // an AI entry
                            GadgetComboBoxSetSelectedPos(combo, pos);
                            TheWindowManager->winSendSystemMsg(parent, GCM_SELECTED, (WindowMsgData)combo, pos);
                            continue;
                        }
                        // the window the real mouse path (GameWindowManager::winProcessMouseEvent) would hand a click at this point to
                        GameWindow *rawHit = nullptr;   // what the mouse path found before it discards windows that take no input
                        auto underMouse = [&rawHit](int x, int y, bool ignoreEnabled = false) -> GameWindow * {
                            ICoord2D pos = { x, y };
                            GameWindow *tip = nullptr;
                            GameWindow *w = TheWindowManager->findWindowUnderMouse(tip, &pos, WIN_STATUS_ABOVE, WIN_STATUS_HIDDEN);
                            if (!w) w = TheWindowManager->findWindowUnderMouse(tip, &pos, WIN_STATUS_NONE, WIN_STATUS_ABOVE | WIN_STATUS_BELOW | WIN_STATUS_HIDDEN);
                            if (!w) w = TheWindowManager->findWindowUnderMouse(tip, &pos, WIN_STATUS_BELOW, WIN_STATUS_HIDDEN);
                            rawHit = w;
                            if (w && BitIsSet(w->winGetStatus(), WIN_STATUS_NO_INPUT))
                                w = (w->winGetParent() && BitIsSet(w->winGetParent()->winGetInstanceData()->getStyle(), GWS_COMBO_BOX)) ? w->winGetParent() : nullptr;
                            (void)ignoreEnabled;
                            return w;
                        };
                        // probe: where the combo is, and whether the mouse reaches it (and each row of its open list) instead of another window
                        auto describe = [](GameWindow *w) -> std::string {
                            if (!w) return "none";
                            std::string n = w->winGetInstanceData() ? w->winGetInstanceData()->m_decoratedNameString.str() : "?";
                            return n.empty() ? "(unnamed)" : n;
                        };
                        int cx, cy, cw, ch;
                        combo->winGetScreenPosition(&cx, &cy); combo->winGetSize(&cw, &ch);
                        GameWindow *hit = underMouse(cx + cw / 2, cy + ch / 2);
                        {
                            GameWindow *top = combo; while (top->winGetParent()) top = top->winGetParent();
                            sprintf_s(flowText, "slot %d context: top status=0x%x (above=%d below=%d hidden=%d enabled=%d) combo status=0x%x hit_ignoring_enabled=%s modal=%d captor=%d", i,
                                      (unsigned)top->winGetStatus(), BitIsSet(top->winGetStatus(), WIN_STATUS_ABOVE) ? 1 : 0, BitIsSet(top->winGetStatus(), WIN_STATUS_BELOW) ? 1 : 0,
                                      BitIsSet(top->winGetStatus(), WIN_STATUS_HIDDEN) ? 1 : 0, BitIsSet(top->winGetStatus(), WIN_STATUS_ENABLED) ? 1 : 0, (unsigned)combo->winGetStatus(),
                                      describe(underMouse(cx + cw / 2, cy + ch / 2, true)).c_str(),
                                      0, TheWindowManager->winGetCapture() ? 1 : 0);
                            if (i == 1 || i == 7) StartupDiagnostics::checkpoint("slotprobe", "context", flowText);
                        }
                        const bool comboHit = hit == combo || (hit && hit->winGetParent() == combo);
                        GameWindow *drop = GadgetComboBoxGetDropDownButton(combo);
                        int dx = 0, dy = 0, dw = 0, dh = 0;
                        if (drop) { drop->winGetScreenPosition(&dx, &dy); drop->winGetSize(&dw, &dh); }
                        GameWindow *hitDrop = drop ? underMouse(dx + dw / 2, dy + dh / 2) : nullptr;
                        if (drop) TheWindowManager->winSendSystemMsg(combo, GBM_SELECTED, (WindowMsgData)drop, drop->winGetWindowId());
                        GameWindow *list = GadgetComboBoxGetListBox(combo);
                        int lx = 0, ly = 0, lw = 0, lh = 0, entries = 0, rowsOk = 0, firstBad = -1;
                        std::string badWindow = "-", badIgnoringEnabled = "-", chain = "-";
                        int badX = 0, badY = 0;
                        const bool open = list && !list->winIsHidden();
                        if (open) {
                            list->winGetScreenPosition(&lx, &ly); list->winGetSize(&lw, &lh);
                            entries = GadgetListBoxGetNumEntries(list);
                            for (int row = 0; row < entries; ++row) {
                                const int rowH = entries ? lh / entries : lh;
                                GameWindow *w = underMouse(lx + lw / 2, ly + row * rowH + rowH / 2);
                                if (w == list || (w && (w->winGetParent() == list || w->winGetParent() == combo))) ++rowsOk;
                                else if (firstBad < 0) {
                                    firstBad = row; badWindow = describe(w); badX = lx + lw / 2; badY = ly + row * rowH + rowH / 2;
                                    badIgnoringEnabled = describe(underMouse(badX, badY, true));
                                    {   // every sibling of the combo whose rectangle covers the point, in the order the mouse path tries them
                                        std::string covering;
                                        GameWindow *container = combo->winGetParent();
                                        for (GameWindow *sib = container ? container->winGetChild() : nullptr; sib; sib = sib->winGetNext()) {
                                            int sx, sy, sw, sh; sib->winGetScreenPosition(&sx, &sy); sib->winGetSize(&sw, &sh);
                                            if (badX >= sx && badX <= sx + sw && badY >= sy && badY <= sy + sh && !sib->winIsHidden()) {
                                                char one[260]; sprintf_s(one, "%s key=%s #%d[%d,%d %dx%d st=0x%x] ", describe(sib).c_str(), TheNameKeyGenerator->keyToName((NameKeyType)sib->winGetWindowId()).str(), sib->winGetWindowId(), sx, sy, sw, sh, (unsigned)sib->winGetStatus());
                                                covering += one;
                                            }
                                        }
                                        char note[1000]; sprintf_s(note, "slot %d covering_siblings_at_%d,%d=%s", i, badX, badY, covering.c_str());
                                        StartupDiagnostics::checkpoint("slotprobe", "covering", note);
                                    }
                                    if (rawHit) {
                                        int rx, ry, rw, rh; rawHit->winGetScreenPosition(&rx, &ry); rawHit->winGetSize(&rw, &rh);
                                        char one[260]; sprintf_s(one, "RAW=%s[%d,%d %dx%d status=0x%x]", describe(rawHit).c_str(), rx, ry, rw, rh, (unsigned)rawHit->winGetStatus());
                                        badIgnoringEnabled = one;
                                    }
                                    // the windows between the list and the top: a point outside any of their rectangles can never reach the list
                                    chain.clear();
                                    for (GameWindow *a = list; a; a = a->winGetParent()) {
                                        int ax, ay, aw, ah; a->winGetScreenPosition(&ax, &ay); a->winGetSize(&aw, &ah);
                                        IRegion2D reg; a->winGetRegion(&reg);
                                        char one[200]; sprintf_s(one, "%s[screen %d,%d %dx%d region %d,%d..%d,%d%s]", describe(a).c_str(), ax, ay, aw, ah, reg.lo.x, reg.lo.y, reg.hi.x, reg.hi.y, (ay + ah > badY && ay <= badY && ax <= badX && ax + aw > badX) ? "" : " OUTSIDE");
                                        chain += one; chain += " < ";
                                    }
                                }
                            }
                        }
                        sprintf_s(flowText, "slot %d display=%dx%d combo=%d,%d %dx%d hit_combo=%d(%s) drop=%d,%d %dx%d hit_drop=%s list_open=%d list=%d,%d %dx%d rows=%d reachable=%d first_unreachable_row=%d at=%d,%d by=%s ignoring_enabled=%s chain=%s",
                                  i, TheDisplay->getWidth(), TheDisplay->getHeight(), cx, cy, cw, ch, comboHit ? 1 : 0, describe(hit).c_str(), dx, dy, dw, dh, describe(hitDrop).c_str(),
                                  open ? 1 : 0, lx, ly, lw, lh, entries, rowsOk, firstBad, badX, badY, badWindow.c_str(), badIgnoringEnabled.c_str(), chain.c_str());
                        StartupDiagnostics::checkpoint("slotprobe", (comboHit && (!open || rowsOk == entries)) ? "ok" : "defect", flowText);
                        GadgetComboBoxHideList(combo);
                    }
                    advance(step.kind == SLOT_FILL ? "filled the Skirmish slots" : "probed the Skirmish slots", nullptr); done = true;
                }
            } else if (step.kind == WAIT_INPUT) {
                if (TheInGameUI && TheInGameUI->getInputEnabled()) { advance("the player has input control", nullptr); done = true; }
            } else if (step.kind == WAKE_MENU) {
                GameWindow *menu = find(step.a);
                if (menu) {
                    TheWindowManager->winSendInputMsg(menu, GWM_CHAR, KEY_SPACE, KEY_STATE_DOWN);
                    advance("sent a key to", step.a); done = true;
                }
            } else if (step.kind == QUIT_MENU) {
                ToggleQuitMenu(); advance("opened the quit menu (Esc)", nullptr); done = true;
            } else if (step.kind == WAIT_FRAMES) {
                if (TheGameLogic->getFrame() >= waitFromFrame + step.wait) { advance("waited logic frames", nullptr); done = true; }
            } else if (step.kind == STATE) {
                Player *player = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
                unsigned structures = 0;
                for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject()) if (obj->isKindOf(KINDOF_STRUCTURE)) ++structures;
                sprintf_s(flowText, "state %s: frame=%u side=%s money=%u rank=%d skill=%d objects=%u structures=%u logic_crc=%08X", step.a,
                          TheGameLogic->getFrame(), player && player->getPlayerTemplate() ? player->getPlayerTemplate()->getName().str() : "?",
                          player ? player->getMoney()->countMoney() : 0u, player ? player->getRankLevel() : -1, player ? player->getSkillPoints() : -1,
                          TheGameLogic->getObjectCount(), structures, TheGameLogic->getCRC(CRC_RECALC));
                StartupDiagnostics::checkpoint("autoflow", "state", flowText);
                // the engine's logic CRC split like the X64.41 trace (objects / partition / players / ai) and one CRC per object, so a
                // difference between the saved world and the loaded one names what differs instead of just saying that it does
                auto crcOf = [](Snapshot *snapshot) {
                    XferCRC *x = NEW XferCRC; x->open("autoflowComponent"); x->xferSnapshot(snapshot); x->close();
                    const UnsignedInt c = x->getCRC(); delete x; return c; };
                static std::map<unsigned, unsigned> savedObjects;
                static std::map<unsigned, std::string> savedNames;
                const bool isSaved = !strcmp(step.a, "saved"), isLoaded = !strcmp(step.a, "after_load");
                std::map<unsigned, unsigned> now;
                std::map<unsigned, std::string> names;
                for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject()) {
                    now[obj->getID()] = crcOf(obj);
                    names[obj->getID()] = obj->getTemplate() ? obj->getTemplate()->getName().str() : "?";
                }
                sprintf_s(flowText, "components %s: partition=%08X players=%08X ai=%08X rng=%08X", step.a, crcOf(ThePartitionManager), crcOf(ThePlayerList),
                          crcOf(TheAI), static_cast<unsigned>(GetGameLogicRandomSeedCRC()));
                StartupDiagnostics::checkpoint("autoflow", "state", flowText);
                if (isSaved) { savedObjects = now; savedNames = names; }
                if (isLoaded) {
                    unsigned same = 0, differ = 0, missing = 0, extra = 0, shown = 0;
                    for (const auto &entry : savedObjects) {
                        auto it = now.find(entry.first);
                        if (it == now.end()) { ++missing; continue; }
                        if (it->second == entry.second) { ++same; continue; }
                        ++differ;
                        if (shown++ < 6) {
                            sprintf_s(flowText, "object differs: id=%u template=%s saved=%08X loaded=%08X", entry.first, savedNames[entry.first].c_str(), entry.second, it->second);
                            StartupDiagnostics::checkpoint("autoflow", "state", flowText);
                        }
                    }
                    for (const auto &entry : now) if (!savedObjects.count(entry.first)) ++extra;
                    sprintf_s(flowText, "objects compared saved vs loaded: identical=%u differ=%u missing_after_load=%u new_after_load=%u", same, differ, missing, extra);
                    StartupDiagnostics::checkpoint("autoflow", "state", flowText);
                }
                ++stepIndex; stepAt = flowFrames; waitFromFrame = TheGameLogic->getFrame(); done = true;
            }
            if (!done && step.kind == PRESS_OPTIONAL && flowFrames >= stepAt + step.wait + 1500) {
                advance("not shown, skipped", step.a);
                done = true;
            }
            if (!done && flowFrames >= stepAt + step.wait + 3600) {
                // never became available: report it once, stop driving and quit so the log is flushed
                char chain[200] = "missing";
                if (step.a) {
                    GameWindow *w = find(step.a);
                    int used = w ? 0 : 7;
                    if (w) chain[0] = 0;
                    for (GameWindow *c = w; c && used < 150; c = c->winGetParent())
                        used += sprintf_s(chain + used, sizeof(chain) - used, "[hidden=%d enabled=%d]", c->winIsHidden() ? 1 : 0, BitIsSet(c->winGetStatus(), WIN_STATUS_ENABLED) ? 1 : 0);
                }
                sprintf_s(flowText, "step %u: %s not available: %s", stepIndex, step.a ? step.a : "", chain);
                StartupDiagnostics::checkpoint("autoflow", "fail", flowText);
                shoot("flowfail");
                flowFailed = true;
                TheGameEngine->setQuitting(TRUE);
            }
        } else if (step.kind != END && !context && flowFrames % 900 == 0) {
            // waiting for a context that does not come: say what the engine thinks it is doing
            char flowText[200];
            sprintf_s(flowText, "step %u waits for context %d: shell=%d in_game=%d loading_map=%d loading_save=%d clearing=%d frame=%u input=%d", stepIndex,
                      step.where, shell ? 1 : 0, TheGameLogic->isInGame() ? 1 : 0, TheGameLogic->isLoadingMap() ? 1 : 0, TheGameLogic->isLoadingSave() ? 1 : 0,
                      TheGameLogic->isClearingGameData() ? 1 : 0, TheGameLogic->getFrame(), TheInGameUI && TheInGameUI->getInputEnabled() ? 1 : 0);
            StartupDiagnostics::checkpoint("autoflow", "waiting", flowText);
            if (flowFrames % 2700 == 0) shoot("stalled");
        } else if (step.kind == END && stepAt != 0xFFFFFFFFu) {
            StartupDiagnostics::checkpoint("autoflow", "observed", "flow complete");
            stepAt = 0xFFFFFFFFu;
            if (autoFlow >= 4) TheGameEngine->setQuitting(TRUE);
        }
    }
    // GENERALS_AUTOSHOT_EVERY=<frames>: a numbered sequence of frames (at most 80) instead of the two single captures; never quits.
    static const unsigned every = [] {
        const char *text = std::getenv("GENERALS_AUTOSHOT_EVERY");
        const int value = text ? std::atoi(text) : 0;
        return value > 0 ? static_cast<unsigned>(value) : 0u;
    }();
    if (every) {
        static unsigned drawn = 0, sequence = 0;
        if (++drawn % every == 0 && sequence < 80) {
            char tag[32];
            sprintf_s(tag, "seq%04u", ++sequence);
            shoot(tag);
        }
        return;
    }
    if (TheGameLogic->isInGame() && !TheGameLogic->isInShellGame() && !TheGameLogic->isLoadingMap() && !TheGameLogic->isClearingGameData() &&
        TheGameLogic->getFrame() > 0) {
        // GENERALS_AUTOSHOT_LOOKAT=center (developer aid): move the camera over the middle of the map shortly before the match capture, so the
        // frame shows the view on open ground and separates "the view reaches past the map edge" from a render fault.
        static const bool lookAtCenter = [] { const char *t = std::getenv("GENERALS_AUTOSHOT_LOOKAT"); return t && !_stricmp(t, "center"); }();
        if (lookAtCenter && !gameDone && gameFrames + 40 == gameFramesWanted && TheTacticalView && TheTerrainLogic) {
            Region3D map; TheTerrainLogic->getExtent(&map);
            Coord3D middle = { 0.5f * (map.lo.x + map.hi.x), 0.5f * (map.lo.y + map.hi.y), 0.0f };
            TheTacticalView->lookAt(&middle);
        }
        if (!gameDone && ++gameFrames >= gameFramesWanted) {
            shoot("game");
            gameDone = true;
            if (TheGameEngine && autoFlow < 4) TheGameEngine->setQuitting(TRUE);
        }
    } else if (!shellDone && ++shellFrames >= 360) {
        shoot("shell");
        shellDone = true;
    }
}

#endif
