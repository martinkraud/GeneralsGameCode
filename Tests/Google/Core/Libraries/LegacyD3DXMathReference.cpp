// SPDX-License-Identifier: GPL-3.0-or-later
#include "LegacyD3DXMathCases.h"
#include <cstdio>
extern "C" int WINAPI D3DXCpuOptimizations(BOOL);

// Win32-only reference recorder; never links the replacement implementation.
int main()
{
    if (D3DXCpuOptimizations(TRUE) != 2) {
        std::fputs("Reference fixture requires the audited default SSE2 dispatch (CPU=2).\n", stderr);
        return 1;
    }
    std::puts("// Pinned min-dx8-sdk 7bddff8c, Win32 D3DX8 CPU=2, 2026-10-11.");
    std::puts("// Test oracle from actual library execution; not replacement-generated.\nstatic const std::uint32_t reference[] = {");
    for (unsigned i = 0; i < 32; ++i) {
        const auto words = MathCases::Output<MathCases::Reference>(i);
        std::printf("// Case %u\n", i);
        for (unsigned k = 0; k < words.size(); ++k) std::printf("0x%08xu,%s", words[k], k % 8 == 7 ? "\n" : " ");
        std::puts("");
    }
    std::puts("};\nstatic const std::uint32_t rotations[] = {");
    for (unsigned i = 0; i < 256; ++i) {
        D3DXMATRIX m; D3DXMatrixRotationZ(&m, MathCases::Input(i).angle);
        std::printf("0x%08xu, 0x%08xu,%s", MathCases::Bits(m._11), MathCases::Bits(m._12), i % 4 == 3 ? "\n" : " ");
    }
    std::puts("};");
}
