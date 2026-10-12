// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "WW3D2/PresentationLifecycle.h"
namespace NativeGraphicsSmoke {
using Sink = void (*)(void *, const char *, const char *, const char *, unsigned);
// Borrowed display driver. Host owns HWND/pump; driver owns its device and
// must release it during Shutdown, before the host destroys the HWND.
class DisplayDriver {
public:
    virtual ~DisplayDriver()=default;
    virtual void Initialize(void *window)=0;
    virtual void BeginClear()=0;
    virtual void EndPresent()=0;
    virtual PresentationStatus Status() const noexcept=0;
    virtual void Shutdown() noexcept=0;
};
struct Configuration {
    DisplayDriver *display = nullptr; // null preserves the accepted direct-backend smoke route
    Sink sink = nullptr;
    void *context = nullptr;
    bool software = false; // test-only caller choice; game always uses hardware
    bool visible = true;   // isolated tests use a hidden window
    unsigned hold_ms = 5000, max_frames = 300; // no command-line overrides
};
enum class Decision { Continue, Success, Failure };
inline Decision Evaluate(unsigned elapsed, unsigned frames, unsigned visible_frames,
                         bool visible_now, bool aborted, unsigned hold, unsigned limit) {
    if (aborted || elapsed > hold + 1000) return Decision::Failure;
    if (elapsed >= hold) return visible_frames && visible_now ? Decision::Success : Decision::Failure;
    if (frames >= limit) return Decision::Failure;
    return Decision::Continue;
}
// Shared bounded window/pump. No engine update or simulation.
int Run(const Configuration &);
}
