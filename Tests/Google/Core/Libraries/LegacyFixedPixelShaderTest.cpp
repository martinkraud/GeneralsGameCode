// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "WW3D2/LegacyFixedPixelShader.h"
#include <cstring>
#ifdef LEGACY_SHADER_LIVE_REFERENCE
#include <d3dx8.h>
#endif

namespace {
#include "LegacyFixedPixelShaderReference.inc"
using namespace LegacyD3DX::FixedPixelShader;
struct Program {
    const char *source, *referenceSource;
    const unsigned long *code, *referenceCode;
    std::size_t sourceBytes, referenceSourceBytes, codeBytes, referenceCodeBytes;
};
#define PROGRAM(n) {n##Source,n##ReferenceSource,n##Code,n##ReferenceCode, \
    sizeof(n##Source),sizeof(n##ReferenceSource),sizeof(n##Code),sizeof(n##ReferenceCode)}
const Program programs[] = {
    PROGRAM(RiverWater), PROGRAM(Water), PROGRAM(TrapezoidWater), PROGRAM(ProfilerSwizzle)
};
#undef PROGRAM

TEST(LegacyFixedPixelShader, ActualProductionSourcesMatchOriginalBytes) {
    for (const auto &p : programs) {
        ASSERT_EQ(p.sourceBytes,p.referenceSourceBytes);
        EXPECT_EQ(std::memcmp(p.source,p.referenceSource,p.sourceBytes),0);
        EXPECT_EQ(std::strlen(p.source)+1,p.sourceBytes);
    }
}

TEST(LegacyFixedPixelShader, EveryProductionTokenMatchesPinnedRecord) {
    const unsigned versions[] = {0xffff0101u,0xffff0101u,0xffff0101u,0xffff0104u};
    for (unsigned i=0;i<4;++i) {
        const auto &p=programs[i];
        ASSERT_EQ(p.codeBytes,p.referenceCodeBytes);
        ASSERT_EQ(p.codeBytes%4,0u);
        EXPECT_EQ(std::memcmp(p.code,p.referenceCode,p.codeBytes),0);
        EXPECT_EQ(p.code[0],versions[i]);
        EXPECT_EQ(p.code[p.codeBytes/4-1],0x0000fffful);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p.code)%alignof(unsigned long),0u);
    }
}

#ifdef LEGACY_SHADER_LIVE_REFERENCE
TEST(LegacyFixedPixelShader, IndependentPinnedAssemblerMatchesEveryByte) {
    for (const auto &p : programs) {
        // Assemble the independently recorded original input, not the replacement.
        ID3DXBuffer *code=nullptr,*errors=nullptr;
        const HRESULT hr=D3DXAssembleShader(p.referenceSource,UINT(p.referenceSourceBytes-1),
            0,nullptr,&code,&errors);
        EXPECT_EQ(hr,S_OK);
        EXPECT_EQ(errors,nullptr);
        if (code) {
            EXPECT_EQ(code->GetBufferSize(),p.codeBytes);
            EXPECT_EQ(code->GetBufferSize()/sizeof(DWORD),p.codeBytes/4);
            if (code->GetBufferSize()==p.codeBytes)
                EXPECT_EQ(std::memcmp(code->GetBufferPointer(),p.code,p.codeBytes),0);
            code->Release();
        } else ADD_FAILURE() << "Pinned assembler returned no bytecode";
        if (errors) errors->Release();
    }
}
#endif
}
