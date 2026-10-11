// SPDX-License-Identifier: GPL-3.0-or-later
#include "Texture2DCPUContent.h"
#include <cstring>

Texture2DCPUContent::Texture2DCPUContent(unsigned w, unsigned h, int row_pitch)
    : width(w), height(h), pitch(row_pitch ? row_pitch : static_cast<int>(w * 2)), pixels(nullptr)
{
    WWASSERT(width && height && pitch > 0 && static_cast<unsigned>(pitch) >= width * 2);
    // Match the original producer: allocation is not a request to clear pixels.
    pixels = W3DNEWARRAY unsigned char[static_cast<size_t>(pitch) * height];
}

Texture2DCPUContent::~Texture2DCPUContent() { WWASSERT(!locked); delete[] pixels; }

void Texture2DCPUContent::Get_Description(SurfaceDescription &out) const
{
    out = {WW3D_FORMAT_A4R4G4B4, width, height};
}

Texture2DDescription Texture2DCPUContent::Describe() const
{
    return {width, height, 1, WW3D_FORMAT_A4R4G4B4, false};
}

void *Texture2DCPUContent::Lock(int *row_pitch)
{
    WWASSERT(!locked);
    locked = true;
    *row_pitch = pitch;
    return pixels;
}

void Texture2DCPUContent::Unlock() { WWASSERT(locked); locked = false; }

const unsigned char *Texture2DCPUContent::Pixels() const { WWASSERT(!locked); return pixels; }

Texture2DCPUContent *Texture2DCPUContent::Snapshot() const
{
    WWASSERT(!locked);
    auto *copy = W3DNEW Texture2DCPUContent(width, height, pitch);
    std::memcpy(copy->pixels, pixels, static_cast<size_t>(pitch) * height);
    return copy;
}

void Texture2DCPUContent::Copy_Rows(void *destination, int destination_pitch) const
{
    WWASSERT(!locked && destination && destination_pitch >= static_cast<int>(width * 2));
    auto *row = static_cast<unsigned char *>(destination);
    for (unsigned y = 0; y < height; ++y)
        std::memcpy(row + static_cast<size_t>(y) * destination_pitch,
                    pixels + static_cast<size_t>(y) * pitch, width * 2);
}

namespace
{
class CPUContentLease : public Texture2DContent
{
    Texture2DCPUContent &content;
    Texture2DMapping mapping;
public:
    CPUContentLease(Texture2DCPUContent &source, unsigned x, unsigned y) : content(source)
    {
        content.Add_Ref();
        auto *base = static_cast<unsigned char *>(content.Lock(&mapping.row_pitch));
        mapping.data = base + static_cast<size_t>(y) * mapping.row_pitch + x * 2;
    }
    ~CPUContentLease() override { content.Unlock(); content.Release_Ref(); }
    Texture2DMapping Mapping() const override { return mapping; }
};
}

Texture2DContent *Texture2DCPUContent::Map(const Texture2DRegion *region)
{
    if (locked) return nullptr;
    if (region && (region->left >= region->right || region->top >= region->bottom ||
                   region->right > width || region->bottom > height)) return nullptr;
    return new CPUContentLease(*this, region ? region->left : 0, region ? region->top : 0);
}
