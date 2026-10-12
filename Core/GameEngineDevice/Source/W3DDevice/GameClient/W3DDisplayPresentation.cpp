// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(_WIN64)
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "Common/GlobalData.h"
#include "Common/StartupDiagnostics.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/Backend/NativeGraphicsSmoke.h"
#include <stdexcept>
#endif
#if defined(_WIN64)
// The retained display borrows the checked platform host HWND. WW3D owns the
// attached device. Headless subsystem selection is unchanged; no ordinary draw,
// engine update, shell or world rendering is reachable through this gate.
int W3DDisplay::runNativePresentationSmoke()
{
    if (!m_initialized || !TheGlobalData->m_headless || m_2DRender) {
        StartupDiagnostics::checkpoint("display_attach", "fail",
            "Native display presentation requires initialized headless W3DDisplay", 1);
        return 1; // Keep normal GameMain engine cleanup on a rejected attachment.
    }
    struct Driver final : NativeGraphicsSmoke::DisplayDriver {
        W3DDisplay &display;
        Int width, height, bits;
        Bool windowed;
        bool attached = false, firstBegin = true, firstEnd = true;
        explicit Driver(W3DDisplay &value) : display(value), width(value.getWidth()),
            height(value.getHeight()), bits(value.getBitDepth()), windowed(value.getWindowed()) {}
        static void Log(void *, const char *stage, const char *status, const char *detail, unsigned code) {
            StartupDiagnostics::checkpoint(stage, status, detail, code);
        }
        void Initialize(void *window) override {
            Log(nullptr,"display_attach","begin","",0);
            PresentationConfiguration config; config.window = window; config.diagnostic = Log;
            WW3D::Init_Native_Presentation(config); attached = true;
            // The graphical setter dereferences the headless-null Render2D helper.
            display.Display::setWidth(800); display.Display::setHeight(600);
            display.setBitDepth(32); display.setWindowed(true);
            int w,h,b; bool windowed;
            WW3D::Get_Device_Resolution(w,h,b,windowed);
            if(w!=800 || h!=600 || b!=32 || !windowed)
                throw std::runtime_error("Native display/device dimensions disagree");
            WW3D::Get_Render_Target_Resolution(w,h,b,windowed);
            if(w!=800 || h!=600 || b!=32)
                throw std::runtime_error("Native display/target dimensions disagree");
            Log(nullptr,"display_size","complete","device=800x600 target=800x600 bits=32 windowed=1",0);
            Log(nullptr,"display_attach","complete","W3DDisplay -> WW3D active backend; presentation-only",0);
        }
        void BeginClear() override {
            if(WW3D::Begin_Render(true,false,Vector3(0.08f,0.32f,0.55f),1)!=WW3D_ERROR_OK)
                throw std::runtime_error("Native display Begin_Render failed");
            if(firstBegin) { firstBegin=false; Log(nullptr,"display_frame_begin","complete","WW3D::Begin_Render color-only",0); }
        }
        void EndPresent() override {
            if(WW3D::End_Render(true)!=WW3D_ERROR_OK)
                throw std::runtime_error("Native display End_Render failed");
            if(firstEnd) { firstEnd=false; Log(nullptr,"display_frame_end","complete","WW3D::End_Render Present",0); }
        }
        PresentationStatus Status() const noexcept override {
            return attached ? WW3D::Native_Presentation_Status() : PresentationStatus{};
        }
        void Shutdown() noexcept override {
            Log(nullptr,"display_detach","begin","",0);
            if(attached) { WW3D::Shutdown_Native_Presentation(); attached=false; }
            display.Display::setWidth(width); display.Display::setHeight(height);
            display.setBitDepth(bits); display.setWindowed(windowed);
            Log(nullptr,"display_detach","complete","device released; display dimensions restored",0);
        }
    } driver(*this);
    NativeGraphicsSmoke::Configuration config; config.display=&driver; config.sink=Driver::Log;
    return NativeGraphicsSmoke::Run(config);
}
#endif
