// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "WW3D2/LegacyFVFStride.h"
#include "LegacyFVFStrideCases.h"
#ifdef LEGACY_FVF_LIVE_REFERENCE
#include <d3dx8core.h>
#endif

namespace {
#include "LegacyFVFStrideReference.inc"
using LegacyD3DX::FVFVertexSize;
static_assert(sizeof(unsigned) == 4 && sizeof(DWORD) == 4 && sizeof(float) == 4);
static_assert(D3DFVF_POSITION_MASK == 0x0e && D3DFVF_TEXCOUNT_MASK == 0xf00);
static_assert(D3DFVF_LASTBETA_UBYTE4 == 0x1000 && D3DFVF_RESERVED2 == 0xe000);
static_assert(FVFVertexSize(DX8_FVF_XYZNDUV2) == 44);
static_assert(sizeof(VertexFormatXYZ) == 12 && sizeof(VertexFormatXYZN) == 24);
static_assert(sizeof(VertexFormatXYZNUV1) == 32 && sizeof(VertexFormatXYZNUV2) == 40);
static_assert(sizeof(VertexFormatXYZNDUV1) == 36 && sizeof(VertexFormatXYZNDUV2) == 44);
static_assert(sizeof(VertexFormatXYZDUV1) == 24 && sizeof(VertexFormatXYZDUV2) == 32);
static_assert(sizeof(VertexFormatXYZUV1) == 20 && sizeof(VertexFormatXYZUV2) == 28);
static_assert(sizeof(VertexFormatXYZNDUV1TG3) == 72 && sizeof(VertexFormatXYZNDCUBEMAP) == 28);
// Freeze the existing mismatch; do not redesign the dormant format here.
static_assert(sizeof(VertexFormatXYZNUV2DMAP) == 48 && FVFVertexSize(DX8_FVF_XYZNUV2DMAP) == 52);
static_assert(offsetof(VertexFormatXYZNDUV2, diffuse) == 24 && offsetof(VertexFormatXYZNDUV2, u1) == 28);

// Oracle composition is justified by the pinned function's disassembly and
// independently checked over the entire sweep against the live x86 library.
// Both tables contain actual library results, never replacement-generated data.
std::uint32_t RecordedSize(std::uint32_t fvf)
{
    auto size = lowByteSizes[fvf & 255];
    auto dimensions = fvf >> 16;
    for (unsigned n = (fvf >> 8) & 15; n; --n, dimensions >>= 2)
        size += dimensionSizes[dimensions & 3];
    return size;
}
}

TEST(LegacyFVFStride, RecordedLegacySweep)
{
    unsigned count = 0;
    std::uint64_t hash = 14695981039346656037ull;
    FVFStrideCases::Sweep([&](std::uint32_t fvf) {
        const auto actual = FVFVertexSize(fvf);
        EXPECT_EQ(actual,RecordedSize(fvf)) << std::hex << fvf;
        FVFStrideCases::HashWord(hash,fvf); FVFStrideCases::HashWord(hash,actual); ++count;
    });
    EXPECT_EQ(count,sweepCount);
    EXPECT_EQ(hash,sweepHash); // Additional raw full-sweep reference fingerprint.
}
TEST(LegacyFVFStride, ActualProductionDeclarations)
{
    static_assert(std::size(productionSizes) == std::size(FVFStrideCases::production));
    for (unsigned i = 0; i < std::size(productionSizes); ++i)
        EXPECT_EQ(FVFVertexSize(FVFStrideCases::production[i]),productionSizes[i]);
    EXPECT_EQ(FVFVertexSize(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2),36);
}
TEST(LegacyFVFStride, ReservedAndMissingPositionRemainPermissive)
{
    EXPECT_EQ(FVFVertexSize(0),0);
    EXPECT_EQ(FVFVertexSize(D3DFVF_NORMAL | D3DFVF_TEX1),20);
    for (unsigned reserved = 0; reserved <= 0xf000; reserved += 0x1000)
        EXPECT_EQ(FVFVertexSize(D3DFVF_XYZB4 | reserved | D3DFVF_RESERVED0),28);
    EXPECT_EQ(FVFVertexSize(0xffffffffu),144);
}
TEST(LegacyFVFStride, InvalidTextureCountsAreNotClamped)
{
    EXPECT_EQ(FVFVertexSize(15u << 8),120);
    EXPECT_EQ(FVFVertexSize((15u << 8) | (2u << 30)),128);
}
#ifdef LEGACY_FVF_LIVE_REFERENCE
TEST(LegacyFVFStride, LivePinnedWin32Sweep)
{
    FVFStrideCases::Sweep([](std::uint32_t fvf) {
        const auto reference = D3DXGetFVFVertexSize(fvf);
        EXPECT_EQ(FVFVertexSize(fvf),reference) << std::hex << fvf;
        EXPECT_EQ(RecordedSize(fvf),reference) << std::hex << fvf;
    });
}
#endif
