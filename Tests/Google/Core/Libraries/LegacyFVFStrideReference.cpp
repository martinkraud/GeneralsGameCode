// SPDX-License-Identifier: GPL-3.0-or-later
#include "LegacyFVFStrideCases.h"
#include <d3dx8core.h>
#include <cstdio>

// Win32 only. No replacement header/library participates in this recorder.
int main()
{
    std::puts("// Pinned min-dx8-sdk 7bddff8c, actual Win32 D3DX8, 2026-10-11.");
    std::puts("// Reference data only; not replacement-generated.\nstatic constexpr std::uint32_t lowByteSizes[] = {");
    for (unsigned i = 0; i < 256; ++i)
        std::printf("%u,%s", D3DXGetFVFVertexSize(i), i % 16 == 15 ? "\n" : " ");
    std::puts("};\nstatic constexpr std::uint32_t dimensionSizes[] = {");
    for (unsigned i = 0; i < 4; ++i)
        std::printf("%u,%s", D3DXGetFVFVertexSize(D3DFVF_TEX1 | i << 16), i == 3 ? "\n" : " ");
    std::puts("};\nstatic constexpr std::uint32_t productionSizes[] = {");
    for (auto fvf : FVFStrideCases::production) std::printf("%u,\n", D3DXGetFVFVertexSize(fvf));
    std::puts("};");
    std::uint64_t hash = 14695981039346656037ull;
    unsigned count = 0;
    FVFStrideCases::Sweep([&](std::uint32_t fvf) {
        FVFStrideCases::HashWord(hash,fvf);
        FVFStrideCases::HashWord(hash,D3DXGetFVFVertexSize(fvf)); ++count;
    });
    std::printf("static constexpr std::uint64_t sweepHash = 0x%016llxull;\n", hash);
    std::printf("static constexpr unsigned sweepCount = %u;\n", count);
}
