// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Precompiled/CppMacros.h"
#include "WW3D2/dx8fvf.h"
#include <cstdint>
#include <initializer_list>

namespace FVFStrideCases {
// Actual named declarations, including the dormant tangent/displacement forms.
inline constexpr std::uint32_t production[] = {
    DX8_FVF_XYZ, DX8_FVF_XYZN, DX8_FVF_XYZNUV1, DX8_FVF_XYZNUV2,
    DX8_FVF_XYZNDUV1, DX8_FVF_XYZNDUV2, DX8_FVF_XYZDUV1, DX8_FVF_XYZDUV2,
    DX8_FVF_XYZUV1, DX8_FVF_XYZUV2, DX8_FVF_XYZNDUV1TG3,
    DX8_FVF_XYZNUV2DMAP, DX8_FVF_XYZNDCUBEMAP
};

// Identical input order for the legacy-only recorder and x86/AMD64 tests.
template<class Visit> void Sweep(Visit visit)
{
    for (auto fvf : production) visit(fvf);
    // Both titles' shadow buffer table: XYZ / XYZRHW, diffuse, 0..2 UV sets.
    for (auto position : {D3DFVF_XYZ, D3DFVF_XYZRHW})
        for (unsigned diffuse = 0; diffuse < 2; ++diffuse)
            for (unsigned count = 0; count <= 2; ++count)
                visit(position | diffuse * D3DFVF_DIFFUSE | count << 8);
    // Mesh Define_FVF: XYZ, optional normals/two colors, 0..8 UV sets.
    for (unsigned attributes = 0; attributes < 8; ++attributes)
        for (unsigned count = 0; count <= 8; ++count)
            visit(D3DFVF_XYZ | ((attributes & 1) ? D3DFVF_NORMAL : 0) |
                  ((attributes & 2) ? D3DFVF_DIFFUSE : 0) |
                  ((attributes & 4) ? D3DFVF_SPECULAR : 0) | count << 8);
    // All position/attribute/count/reserved/last-beta combinations, twice.
    for (std::uint32_t low = 0; low < 0x10000; ++low) {
        visit(low); visit(low | 0xe4e40000u);
    }
    // Every encoding of all eight texture dimensions, for all 16 count values.
    for (std::uint32_t dimensions = 0; dimensions < 0x10000; ++dimensions)
        for (unsigned count = 0; count < 16; ++count)
            visit(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE |
                  count << 8 | dimensions << 16);
    std::uint32_t state = 0x12345abc;
    for (unsigned i = 0; i < 65536; ++i) {
        state = state * 1664525u + 1013904223u; visit(state);
    }
    visit(0xffffffffu);
}

inline void HashWord(std::uint64_t &hash, std::uint32_t word)
{
    for (unsigned byte = 0; byte < 4; ++byte) {
        hash ^= (word >> (byte * 8)) & 255;
        hash *= 1099511628211ull;
    }
}
}
