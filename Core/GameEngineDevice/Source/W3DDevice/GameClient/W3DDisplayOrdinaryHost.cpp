// SPDX-License-Identifier: GPL-3.0-or-later
// Ordinary-host profile display lifetime (X64.22). Kept separate from the
// attached diagnostic in W3DDisplayPresentation.cpp.
#if defined(_WIN64)
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "Common/GlobalData.h"
#include "Common/Errors.h"
#include "Common/StartupDiagnostics.h"
#include "Common/NativeOrdinaryHost.h"
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
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include "WW3D2/camera.h"
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

void W3DDisplay::initOrdinaryHostPresentation()
{
    StartupDiagnostics::Phase phase("display_ordinary_init");
    if (!TheGlobalData->m_headless || m_2DRender)
        throw std::runtime_error("Ordinary host display requires the headless world selection and no prior 2D helper");
    if (!TheGlobalData->m_windowed || TheGlobalData->m_xResolution != 800 || TheGlobalData->m_yResolution != 600) {
        char detail[160];
        sprintf_s(detail, "windowed=%d xres=%d yres=%d; only windowed 800x600 is supported",
            TheGlobalData->m_windowed ? 1 : 0, (int)TheGlobalData->m_xResolution, (int)TheGlobalData->m_yResolution);
        StartupDiagnostics::checkpoint("display_config", "fail", detail, 1);
        throw std::runtime_error(detail);
    }
    if (!NativeOrdinaryHost::clientIsExact(ApplicationHWnd)) {
        StartupDiagnostics::checkpoint("display_config", "fail", "ApplicationHWnd client is not 800x600", 1);
        throw std::runtime_error("ApplicationHWnd client is not 800x600");
    }
    StartupDiagnostics::checkpoint("display_config", "complete", "windowed=1 800x600x32 ApplicationHWnd");
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
    WW3D::Init_Native_Presentation(configuration);

    m_2DRender = NEW Render2DClass;
    setWidth(800);
    setHeight(600);
    setBitDepth(32);
    setWindowed(TRUE);
    Render2DClass::Set_Screen_Resolution(RectClass(0, 0, 800, 600));
    // Same pixel-center policy the legacy non-headless init selects.
    WW3D::Set_Screen_UV_Bias(true);

    int w = 0, h = 0, b = 0; bool windowed = false;
    WW3D::Get_Device_Resolution(w, h, b, windowed);
    if (w != 800 || h != 600 || b != 32 || !windowed)
        throw std::runtime_error("Ordinary host device dimensions disagree");
    WW3D::Get_Render_Target_Resolution(w, h, b, windowed);
    if (w != 800 || h != 600 || b != 32)
        throw std::runtime_error("Ordinary host target dimensions disagree");
    const NativeOrdinaryHost::ModuleAudit audit = NativeOrdinaryHost::auditLoadedModules();
    char detail[160];
    sprintf_s(detail, "d3d8=%d d3dx8=%d d3d11=%d dxgi=%d", audit.d3d8, audit.d3dx8, audit.d3d11, audit.dxgi);
    if (!NativeOrdinaryHost::auditAcceptable(audit)) {
        StartupDiagnostics::checkpoint("display_module_audit", "fail", detail, 1);
        throw std::runtime_error("Legacy D3D8 module loaded under the ordinary host profile");
    }
    StartupDiagnostics::checkpoint("display_module_audit", "complete", detail);
    StartupDiagnostics::checkpoint("display_size", "complete", "device=800x600 target=800x600 bits=32 windowed=1");
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
}

// Terrain-only native world pass (profile ordinaryTerrain, in game, map loaded). Returns the
// number of draws; zero when there is nothing to draw yet (shell, loading, no camera).
static int drawNativeTerrainPass()
{
    if (!StartupDiagnostics::ordinaryTerrain() || !TheGameLogic || !TheGameLogic->isInGame() ||
        TheGameLogic->isInShellGame() || TheGameLogic->isLoadingMap() || !TheTerrainRenderObject ||
        !TheTacticalView)
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
                char detail[200];
                const HeightMapRenderObjClass::NativeTerrainStats stats =
                    static_cast<HeightMapRenderObjClass *>(TheTerrainRenderObject)->nativeStats();
                sprintf_s(detail, "draws=%d atlas_uploads=%u tile_uploads=%u (terrain base+blend; UI follows)",
                    terrainDraws, stats.atlasUploads, stats.tileUploads);
                StartupDiagnostics::checkpoint("terrain_first_frame", "complete", detail);
            }
            if (g_terrainFrames == 29) {
                // One-time evidence: what the terrain pass actually left in the target before the
                // UI is drawn (separates "nothing submitted/visible" from "covered by the UI").
                char detail[256];
                if (auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend())) {
                    const std::vector<unsigned char> pixels = backend->Readback_RGBA8();
                    size_t lit = 0;
                    for (size_t i = 0; i + 3 < pixels.size(); i += 4)
                        if (pixels[i] | pixels[i + 1] | pixels[i + 2]) ++lit;
                    const size_t total = pixels.size() / 4;
                    const size_t c = (total / 2 + 400) * 4 < pixels.size() ? (total / 2 + 400) * 4 : 0;
                    Vector3 cam(0, 0, 0), fwd(0, 0, 0);
                    if (auto *view = dynamic_cast<W3DView *>(TheTacticalView)) {
                        cam = view->get3DCamera()->Get_Position();
                        fwd = view->get3DCamera()->Get_Forward_Dir();
                    }
                    sprintf_s(detail, "nonblack_pct=%u center_rgb=%02X%02X%02X cam=(%.0f,%.0f,%.0f) fwd=(%.2f,%.2f,%.2f)",
                        total ? static_cast<unsigned>(lit * 100 / total) : 0u, pixels[c], pixels[c + 1], pixels[c + 2],
                        cam.X, cam.Y, cam.Z, fwd.X, fwd.Y, fwd.Z);
                    StartupDiagnostics::checkpoint("terrain_probe", "observed", detail);
                }
            }
            ++g_terrainFrames;
            g_terrainDraws += static_cast<unsigned>(terrainDraws);
            g_terrainStats = static_cast<HeightMapRenderObjClass *>(TheTerrainRenderObject)->nativeStats();
        }
        if (TheWindowManager) TheWindowManager->winRepaint();
        if (TheMouse) TheMouse->DRAW();
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
    const unsigned kTotalMs = 900000;        // 15 minutes wall clock
    const unsigned kLogicFrameBound = 1800;  // 60 s of logic at 30 fps after the game starts
    const unsigned kMilestones[] = { 1, 30, 150, 300, 900, 1800 };
    unsigned nextMilestone = 0, updates = 0, elapsed = 0, startFrame = 0, lastFrame = 0;
    int result = 1;
    const char *exitReason = "failure";
    const char *phase = "game_loop";
    bool inGame = false, exiting = false;
    unsigned exitingUpdates = 0;
    char detail[256];
    try {
        TheShell->setPushGuard(GamePushGuard);
        StartupDiagnostics::checkpoint(phase, "begin",
            "Win32GameEngine::update + FramePacer (ordinary loop); shell map off; push allow-list MainMenu/SkirmishGameOptionsMenu");
        const ULONGLONG start = GetTickCount64();
        for (;;) {
            TheGameEngine->update();          // GameClient (+draw), message stream, GameLogic, OS pump
            TheFramePacer->update();
            ++updates;
            if (!IsWindow(ApplicationHWnd)) throw std::runtime_error("Application window destroyed");
            if (!IsIconic(ApplicationHWnd) && !NativeOrdinaryHost::clientIsExact(ApplicationHWnd))
                throw std::runtime_error("Unsupported resize: client is not 800x600");
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
            }
            if (inGame && !exiting) {
                const unsigned frame = TheGameLogic->getFrame();
                lastFrame = frame;
                while (nextMilestone < sizeof(kMilestones) / sizeof(kMilestones[0]) &&
                       frame - startFrame >= kMilestones[nextMilestone]) {
                    sprintf_s(detail, "logic_frame=%u crc=%08X presents=%u", frame, TheGameLogic->getCRC(),
                        m_hostPresents);
                    StartupDiagnostics::checkpoint("game_progress", "observed", detail);
                    ++nextMilestone;
                }
                if (frame - startFrame >= kLogicFrameBound) {
                    // Ordinary exit message; processed on the next updates.
                    TheGameLogic->exitGame();
                    exiting = true;
                    exitReason = "frame_bound";
                }
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
    if (g_menuBoundary.hit) {
        sprintf_s(detail, "reason=%s %s", g_menuBoundary.reason, g_menuBoundary.detail);
        StartupDiagnostics::checkpoint("menu_boundary", "observed", detail);
    }
    sprintf_s(detail, "calls=%u (first receipts bounded; verified every call)", nativeFloatingPointModeCalls());
    StartupDiagnostics::checkpoint("FP_summary", "observed", detail);
    if (StartupDiagnostics::ordinaryTerrain()) {
        sprintf_s(detail, "frames=%u draws=%u atlas_uploads=%u tile_uploads=%u tile_updates=%u",
            g_terrainFrames, g_terrainDraws, g_terrainStats.atlasUploads, g_terrainStats.tileUploads,
            g_terrainStats.tileUpdates);
        StartupDiagnostics::checkpoint("terrain_summary", g_terrainFrames ? "complete" : "fail", detail, g_terrainFrames ? 0 : 1);
        if (!g_terrainFrames) result = 1;
    }
    sprintf_s(detail, "presents=%u updates=%u elapsed_ms=%u game=%u logic_frames=%u exit=%s driver=hardware",
        m_hostPresents, updates, elapsed, inGame ? 1u : 0u,
        inGame ? lastFrame - startFrame : 0u, exitReason);
    StartupDiagnostics::checkpoint("game_summary", result ? "fail" : "complete", detail, result);
    if (result) StartupDiagnostics::markFailed();
    return result;
#endif
}
#endif
