// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Texture2DCPUContent.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Bounded equivalent of TextureLoadTask's ordinary single-mip file route.
// File access remains with the caller (the engine uses its actual FileFactory).
enum class UITextureStatus { Ready, Absent, Invalid, Unsupported };
struct UITextureBytes {
    UITextureStatus status = UITextureStatus::Absent;
    std::vector<unsigned char> bytes;
};
using UITextureReader = std::function<UITextureBytes(const std::string &)>;
struct UITexturePolicy {
    bool allow_compression = true;
    unsigned max_width = 8192, max_height = 8192;
    unsigned texture_bits = 32;
    // MIP_LEVELS_1 suppresses the global reduction; nonzero HSV is explicitly
    // outside this initial equivalent, never silently omitted.
    bool hsv_shift = false;
};
struct UIContentRelease { void operator()(Texture2DCPUContent *p) const { if (p) p->Release_Ref(); } };
using UITextureContent = std::unique_ptr<Texture2DCPUContent, UIContentRelease>;
struct UITextureResult {
    UITextureStatus status;
    UITextureContent content;
};
UITextureResult Load_UI_Texture_Content(const std::string &, const UITexturePolicy &, const UITextureReader &);
