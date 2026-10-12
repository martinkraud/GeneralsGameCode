// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace NativeGraphicsSmoke {
using Sink = void (*)(void *, const char *, const char *, const char *, unsigned);
struct Configuration {
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
// Shared diagnostic host only. Never enters WW3D, engine update or simulation.
int Run(const Configuration &);
}
