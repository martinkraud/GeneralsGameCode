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
