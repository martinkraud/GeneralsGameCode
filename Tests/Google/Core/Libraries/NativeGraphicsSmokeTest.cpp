// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "WW3D2/Backend/NativeGraphicsSmoke.h"
#include <windows.h>
#include <string>
#include <vector>
using namespace NativeGraphicsSmoke;
TEST(NativeGraphicsSmoke, CompletionRequiresVisiblePresentAndBounds) {
    EXPECT_EQ(Evaluate(4999, 200, 200, true, false, 5000, 300),Decision::Continue);
    EXPECT_EQ(Evaluate(5000, 250, 250, true, false, 5000, 300),Decision::Success);
    EXPECT_EQ(Evaluate(5000, 250, 0, true, false, 5000, 300),Decision::Failure);
    EXPECT_EQ(Evaluate(5000, 250, 250, false, false, 5000, 300),Decision::Failure);
    EXPECT_EQ(Evaluate(0, 300, 1, true, false, 5000, 300),Decision::Failure);
    EXPECT_EQ(Evaluate(5000, 250, 250, true, true, 5000, 300),Decision::Failure);
    EXPECT_EQ(Evaluate(6001, 250, 250, true, false, 5000, 300),Decision::Failure);
}
namespace {
struct Probe {
    std::vector<std::string> events;
    const char *action = "none";
    static BOOL CALLBACK Window(HWND window, LPARAM value) {
        auto *self=reinterpret_cast<Probe *>(value);
        wchar_t name[100]{}; GetClassNameW(window,name,100);
        if (std::wstring(name).find(L"NativeGraphicsSmoke-")!=0) return TRUE;
        if (std::string(self->action)=="close") PostMessageW(window,WM_CLOSE,0,0);
        if (std::string(self->action)=="resize") SetWindowPos(window,nullptr,0,0,900,700,SWP_NOMOVE|SWP_NOZORDER);
        return TRUE;
    }
    static void Log(void *value,const char *stage,const char *status,const char *detail,unsigned) {
        auto *self=static_cast<Probe *>(value);
        self->events.push_back(std::string(stage)+":"+status+":"+detail);
        if (std::string(stage)=="graphics_backend" && std::string(status)=="complete")
            EnumThreadWindows(GetCurrentThreadId(),Window,reinterpret_cast<LPARAM>(self));
    }
    size_t Find(const std::string &prefix) const {
        for(size_t i=0;i<events.size();++i) if(events[i].find(prefix)==0) return i;
        return events.size();
    }
};
}
TEST(NativeGraphicsSmoke, HiddenRealWARPIsNotVisibleSuccessAndReleasesInOrder) {
    Probe probe; Configuration c; c.visible=false;c.software=true;c.hold_ms=200;c.max_frames=5;c.sink=Probe::Log;c.context=&probe;
    EXPECT_EQ(NativeGraphicsSmoke::Run(c),1);
    EXPECT_LT(probe.Find("graphics_device:complete:driver=WARP"),probe.events.size());
    EXPECT_LT(probe.Find("graphics_target:complete"),probe.events.size());
    EXPECT_LT(probe.Find("graphics_present:observed:"),probe.events.size());
    EXPECT_EQ(probe.Find("graphics_present:complete:"),probe.events.size());
    EXPECT_LT(probe.Find("graphics_backend_release:complete"),probe.Find("graphics_window_release:complete"));
    EXPECT_LT(probe.Find("graphics_window_release:complete"),probe.Find("graphics_class_release:complete"));
    EXPECT_LT(probe.Find("graphics_host:fail:"),probe.events.size());
}
TEST(NativeGraphicsSmoke, CloseAbortStillReleasesBackendBeforeWindow) {
    Probe probe;probe.action="close";Configuration c;c.visible=false;c.software=true;c.hold_ms=50;c.sink=Probe::Log;c.context=&probe;
    EXPECT_EQ(NativeGraphicsSmoke::Run(c),1);
    EXPECT_LT(probe.Find("graphics_messages:fail:Close/Escape/quit abort"),probe.events.size());
    EXPECT_LT(probe.Find("graphics_backend_release:complete"),probe.Find("graphics_window_release:complete"));
}
TEST(NativeGraphicsSmoke, UnexpectedResizeFailsClosedAndCleansUp) {
    Probe probe;probe.action="resize";Configuration c;c.visible=false;c.software=true;c.hold_ms=50;c.sink=Probe::Log;c.context=&probe;
    EXPECT_EQ(NativeGraphicsSmoke::Run(c),1);
    EXPECT_LT(probe.Find("graphics_messages:fail:Unsupported diagnostic resize"),probe.events.size());
    EXPECT_LT(probe.Find("graphics_class_release:complete"),probe.events.size());
}
TEST(NativeGraphicsSmoke, InvalidLimitsDoNotCreateWindowOrDevice) {
    Probe probe;Configuration c;c.hold_ms=5001;c.sink=Probe::Log;c.context=&probe;
    EXPECT_EQ(NativeGraphicsSmoke::Run(c),1); EXPECT_EQ(probe.events.size(),1u);
}
#include "WW3D2/Backend/D3D11Backend.h"
#include "WWMath/vector3.h"
#include <stdexcept>
namespace {
struct TestDisplay final : DisplayDriver {
    HWND window=nullptr;
    std::unique_ptr<PresentationLifecycle> lifecycle;
    int fail=0; bool releasedWithWindow=false; unsigned frames=0;
    void Initialize(void *hwnd) override {
        window=static_cast<HWND>(hwnd);
        if(fail==1)throw std::runtime_error("test attach failure");
        D3D11Backend::Configuration config;config.window=hwnd;config.width=800;config.height=600;
        config.software=true;config.diagnostic_windowed=true;
        lifecycle=std::make_unique<PresentationLifecycle>(D3D11Backend::Create(config),PresentationSize{800,600},
            [](IRenderBackend&b){return static_cast<D3D11Backend&>(b).Last_Presentation();});
    }
    void BeginClear() override {
        lifecycle->Begin(true,false,Vector3(0.08f,0.32f,0.55f),1);
        if(fail==2)throw std::runtime_error("test clear failure");
    }
    void EndPresent() override {
        if(fail==3)throw std::runtime_error("test present failure");
        lifecycle->End(true);++frames;
    }
    PresentationStatus Status() const noexcept override {return lifecycle?lifecycle->Status():PresentationStatus{};}
    void Shutdown() noexcept override {releasedWithWindow=IsWindow(window)!=FALSE;lifecycle.reset();}
};
}
TEST(NativeGraphicsSmoke, BorrowedDisplayRealWARPFramesAndCleanup) {
    TestDisplay display;Probe probe;Configuration c;c.display=&display;c.visible=false;c.software=true;
    c.hold_ms=200;c.max_frames=3;c.sink=Probe::Log;c.context=&probe;
    EXPECT_EQ(NativeGraphicsSmoke::Run(c),1); // Hidden window is never a visible milestone PASS.
    EXPECT_EQ(display.frames,3u); EXPECT_TRUE(display.releasedWithWindow);EXPECT_FALSE(IsWindow(display.window));
    EXPECT_FALSE(display.lifecycle);
    EXPECT_LT(probe.Find("graphics_backend_release:complete"),probe.Find("graphics_window_release:complete"));
}
TEST(NativeGraphicsSmoke, BorrowedDisplayPartialInitClearPresentFailuresReleaseBeforeHWND) {
    for(int fail=1;fail<=3;++fail){
        TestDisplay display;display.fail=fail;Probe probe;Configuration c;c.display=&display;c.visible=false;
        c.software=true;c.hold_ms=100;c.sink=Probe::Log;c.context=&probe;
        EXPECT_EQ(NativeGraphicsSmoke::Run(c),1);EXPECT_TRUE(display.releasedWithWindow);EXPECT_FALSE(display.lifecycle);EXPECT_FALSE(IsWindow(display.window));
        EXPECT_LT(probe.Find(fail==1?"graphics_backend:fail":fail==2?"graphics_clear:fail":"graphics_present:fail"),probe.events.size());
        EXPECT_LT(probe.Find("graphics_backend_release:complete"),probe.Find("graphics_window_release:complete"));
    }
}
