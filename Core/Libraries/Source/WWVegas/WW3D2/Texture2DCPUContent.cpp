// SPDX-License-Identifier: GPL-3.0-or-later
#include "Texture2DCPUContent.h"
#include <cstring>
#include <stdexcept>
#include <limits>

Texture2DCPUContent::Texture2DCPUContent(unsigned w, unsigned h, int row_pitch)
    : Texture2DCPUContent(w, h, WW3D_FORMAT_A4R4G4B4, row_pitch)
{}

Texture2DCPUContent::Texture2DCPUContent(unsigned w, unsigned h, WW3DFormat f, int row_pitch)
    : width(w), height(h), format(f), rows(h), row_bytes(0), pitch(0), pixels(nullptr)
{
    if (!w || !h || w > 16384 || h > 16384) throw std::invalid_argument("CPU texture dimensions");
    switch (f) {
    case WW3D_FORMAT_A4R4G4B4:
    case WW3D_FORMAT_R5G6B5: row_bytes = w * 2; break;
    case WW3D_FORMAT_A8R8G8B8:
    case WW3D_FORMAT_X8R8G8B8: row_bytes = w * 4; break;
    case WW3D_FORMAT_DXT1: rows = (h + 3) / 4; row_bytes = ((w + 3) / 4) * 8; break;
    case WW3D_FORMAT_DXT3:
    case WW3D_FORMAT_DXT5: rows = (h + 3) / 4; row_bytes = ((w + 3) / 4) * 16; break;
    default: throw std::invalid_argument("Unsupported CPU texture format");
    }
    pitch = row_pitch ? row_pitch : static_cast<int>(row_bytes);
    if (pitch < static_cast<int>(row_bytes)) throw std::invalid_argument("CPU texture pitch");
    if (static_cast<size_t>(pitch) > (std::numeric_limits<size_t>::max)() / rows)
        throw std::length_error("CPU texture storage size");
    // Match the original producer: allocation is not a request to clear pixels.
    pixels = W3DNEWARRAY unsigned char[static_cast<size_t>(pitch) * rows];
}

Texture2DCPUContent::~Texture2DCPUContent() { WWASSERT(!locked); delete[] pixels; }

void Texture2DCPUContent::Get_Description(SurfaceDescription &out) const
{
    out = {format, width, height};
}

Texture2DDescription Texture2DCPUContent::Describe() const
{
    return {width, height, 1, format, false};
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
    auto *copy = W3DNEW Texture2DCPUContent(width, height, format, pitch);
    std::memcpy(copy->pixels, pixels, static_cast<size_t>(pitch) * rows);
    return copy;
}

void Texture2DCPUContent::Copy_Rows(void *destination, int destination_pitch) const
{
    WWASSERT(!locked && destination && destination_pitch >= static_cast<int>(row_bytes));
    auto *row = static_cast<unsigned char *>(destination);
    for (unsigned y = 0; y < rows; ++y)
        std::memcpy(row + static_cast<size_t>(y) * destination_pitch,
                    pixels + static_cast<size_t>(y) * pitch, row_bytes);
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
        const auto d = content.Describe();
        const unsigned bytes = d.format == WW3D_FORMAT_A4R4G4B4 || d.format == WW3D_FORMAT_R5G6B5 ? 2 : 4;
        mapping.data = base + static_cast<size_t>(y) * mapping.row_pitch + x * bytes;
    }
    ~CPUContentLease() override { content.Unlock(); content.Release_Ref(); }
    Texture2DMapping Mapping() const override { return mapping; }
};
}

Texture2DContent *Texture2DCPUContent::Map(const Texture2DRegion *region)
{
    if (locked) return nullptr;
    if (format == WW3D_FORMAT_DXT1 || format == WW3D_FORMAT_DXT3 || format == WW3D_FORMAT_DXT5) return nullptr;
    if (region && (region->left >= region->right || region->top >= region->bottom ||
                   region->right > width || region->bottom > height)) return nullptr;
    return new CPUContentLease(*this, region ? region->left : 0, region ? region->top : 0);
}
