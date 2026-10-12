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
#endif
