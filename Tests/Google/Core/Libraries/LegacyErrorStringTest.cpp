// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "WW3D2/LegacyErrorString.h"
#include <array>
#include <cstring>
#ifdef LEGACY_ERROR_LIVE_REFERENCE
#include <d3dx8.h>
#endif

namespace {
struct Record {
    std::uint32_t value, size;
    bool nullOutput;
    std::uint32_t hr;
    const char *bytes;
    std::size_t touched;
};
const Record records[] = {
#include "LegacyErrorStringReference.inc"
};
struct Result {
    std::uint32_t hr;
    std::array<char,520> bytes;
};
template<class Function> Result Capture(Function function, std::uint32_t value,
    std::uint32_t size, bool nullOutput) {
    Result r{};
    r.bytes.fill(static_cast<char>(0xa5));
    r.hr=static_cast<std::uint32_t>(function(value,nullOutput?nullptr:r.bytes.data(),size));
    return r;
}

TEST(LegacyErrorString, ExactPinnedBytesReturnAndCanaries) {
    for (const auto &record : records) {
        SCOPED_TRACE(::testing::Message() << std::hex << record.value << " size=" << std::dec
            << record.size << " null=" << record.nullOutput);
        auto actual=Capture(LegacyD3DX::GetErrorStringA,record.value,record.size,record.nullOutput);
        std::array<char,520> expected;
        expected.fill(static_cast<char>(0xa5));
        ASSERT_LE(record.touched,expected.size());
        std::memcpy(expected.data(),record.bytes,record.touched);
        EXPECT_EQ(actual.hr,record.hr);
        EXPECT_EQ(actual.bytes,expected);
    }
}

#ifdef LEGACY_ERROR_LIVE_REFERENCE
TEST(LegacyErrorString, IndependentPinnedLibraryMatchesAllRecordedCases) {
    for (const auto &record : records) {
        auto actual=Capture(LegacyD3DX::GetErrorStringA,record.value,record.size,record.nullOutput);
        auto legacy=Capture(D3DXGetErrorStringA,record.value,record.size,record.nullOutput);
        ASSERT_EQ(actual.hr,legacy.hr);
        ASSERT_EQ(actual.bytes,legacy.bytes) << std::hex << record.value << " size=" << record.size;
    }
}

TEST(LegacyErrorString, FacilitySweepAndSeededWordsMatchPinnedLibrary) {
    auto compare=[](std::uint32_t value, std::uint32_t size) {
        auto actual=Capture(LegacyD3DX::GetErrorStringA,value,size,false);
        auto legacy=Capture(D3DXGetErrorStringA,value,size,false);
        EXPECT_EQ(actual.hr,legacy.hr);
        EXPECT_EQ(actual.bytes,legacy.bytes) << std::hex << value << " size=" << size;
    };
    // Every low-word value in the production D3D HRESULT facility, at the
    // actual caller buffer size; then independent fallback/truncation coverage.
    for (std::uint32_t low=0;low<65536;++low) compare(0x88760000u|low,256);
    std::uint32_t word=0x64e88001u;
    for (unsigned i=0;i<4096;++i) {
        word=word*1664525u+1013904223u;
        for (std::uint32_t size=0;size<=34;++size) compare(word,size);
        compare(word,512);
    }
}
#endif
}
