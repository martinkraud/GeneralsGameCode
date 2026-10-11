// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace LegacyD3DX {
// Pinned D3DX8 integer sizing semantics, including permissive reserved inputs.
// This is not FVF validation. No device, SDK header or graphics library needed.
constexpr std::uint32_t FVFVertexSize(std::uint32_t fvf)
{
    std::uint32_t size = 0;
    switch (fvf & 0x0eu) {
        case 0x02: size = 12; break;
        case 0x04: case 0x06: size = 16; break;
        case 0x08: size = 20; break;
        case 0x0a: size = 24; break;
        case 0x0c: size = 28; break;
        case 0x0e: size = 32; break;
    }
    if (fvf & 0x10u) size += 12; // normal
    if (fvf & 0x20u) size += 4;  // point size
    if (fvf & 0x40u) size += 4;  // diffuse
    if (fvf & 0x80u) size += 4;  // specular
    std::uint32_t dimensions = fvf >> 16;
    constexpr std::uint32_t bytes[] = {8,12,16,4};
    for (unsigned count = (fvf >> 8) & 15u; count != 0; --count) {
        size += bytes[dimensions & 3u];
        dimensions >>= 2;
    }
    return size;
}
}
