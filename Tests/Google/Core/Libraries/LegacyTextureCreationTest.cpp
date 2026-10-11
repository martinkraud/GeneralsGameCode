// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "LegacyTextureCreationCases.h"
#include "WW3D2/LegacyTextureCreation.h"
#include <stdexcept>
#ifdef LEGACY_TEXTURE_LIVE_REFERENCE
#include "LegacyTextureCreationReferenceAPI.h"
#endif
namespace {
using namespace TextureCreationCases;
struct Native {
    inline static constexpr auto Texture=&LegacyD3DX::CreateTexture;
    inline static constexpr auto Cube=&LegacyD3DX::CreateCubeTexture;
    inline static constexpr auto Volume=&LegacyD3DX::CreateVolumeTexture;
};
#include "LegacyTextureCreationExamples.inc"
#include "LegacyTextureCreationReference.inc"

void Compare(const Result &actual,const Result &expected,unsigned index) {
    SCOPED_TRACE(index);
    EXPECT_EQ(actual.hr,expected.hr);
    EXPECT_EQ(actual.output,expected.output);
    EXPECT_EQ(actual.events,expected.events);
    EXPECT_EQ(actual.ownership,expected.ownership);
    if (!(actual==expected))throw std::runtime_error("Creation transcript mismatch; stop at first failing case");
}
TEST(LegacyTextureCreation, CompleteLiteralLegacyTranscripts) {
    unsigned index=0;
    Examples([&](const Input &i) { Compare(TextureCreationCases::Run<Native>(i),textureExamples[index],index); ++index; });
    EXPECT_EQ(index,std::size(textureExamples));
}
TEST(LegacyTextureCreation, RecordedPolicySweep) {
    unsigned count=0,group=0; std::uint64_t hash=14695981039346656037ull;
    Sweep([&](const Input &i) {
        Hash(hash,TextureCreationCases::Run<Native>(i)); ++count;
        if (count%1024==0) {
            EXPECT_EQ(hash,textureReferenceGroups[group]) << "group " << group;
            ++group; hash=14695981039346656037ull;
        }
    });
    if(count%1024) { EXPECT_EQ(hash,textureReferenceGroups[group]); ++group; }
    EXPECT_EQ(count,textureReferenceCount);
    EXPECT_EQ(group,std::size(textureReferenceGroups));
}
#ifdef LEGACY_TEXTURE_LIVE_REFERENCE
TEST(LegacyTextureCreation, LivePinnedFullTranscripts) {
    unsigned index=0;
    Sweep([&](const Input &i) { Compare(TextureCreationCases::Run<Native>(i),TextureCreationCases::Run<Legacy>(i),index); ++index; });
    EXPECT_EQ(index,textureReferenceCount);
}
#endif
}
