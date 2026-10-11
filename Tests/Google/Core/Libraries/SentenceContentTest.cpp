// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include <cstring>
#include <memory>
#include <vector>
#include "WW3D2/FontChars.h"
#include "WW3D2/Texture2DCPUContent.h"

namespace {
struct Release { void operator()(const RefCountClass *p) const { p->Release_Ref(); } };
using Content = std::unique_ptr<Texture2DCPUContent, Release>;
using Font = std::unique_ptr<FontCharsClass, Release>;

void Fill(Texture2DCPUContent &content, unsigned char value)
{
    int pitch;
    auto *pixels = content.Lock(&pitch);
    std::memset(pixels, value, static_cast<size_t>(pitch) * content.Describe().height);
    content.Unlock();
}
}

TEST(SentenceContentTest, RealGDIProducerWritesExactPitchedGlyphAndAlpha)
{
    Font font(new FontCharsClass);
    ASSERT_TRUE(font->Initialize_GDI_Font("Arial", 12, false, nullptr));
    const unsigned w = font->Get_Char_Width(L'W');
    const unsigned h = font->Get_Char_Height();
    ASSERT_GT(w, 0u); ASSERT_GT(h, 0u);
    Content tight(new Texture2DCPUContent(w, h));
    Content padded(new Texture2DCPUContent(w + 6, h + 4, (w + 6) * 2 + 8));
    Fill(*tight, 0); Fill(*padded, 0x12);
    int a, b;
    auto *first = static_cast<uint16 *>(tight->Lock(&a));
    font->Blit_Char(L'W', first, a, 0, 0);
    tight->Unlock();
    auto *second = static_cast<uint16 *>(padded->Lock(&b));
    font->Blit_Char(L'W', second, b, 3, 2);
    padded->Unlock();
    unsigned visible = 0;
    for (unsigned y = 0; y < h; ++y) {
        auto *row = reinterpret_cast<const uint16 *>(tight->Pixels() + y * a);
        for (unsigned x = 0; x < w; ++x) {
            // Legacy overlap is OR, including the original destination value.
            uint16 expected = row[x];
            if (x < static_cast<unsigned>(font->Get_Extra_Overlap())) expected |= 0x1212;
            EXPECT_EQ(second[(y + 2) * (b / 2) + x + 3], expected);
            EXPECT_TRUE((row[x] & 0xfff) == 0 || (row[x] & 0xfff) == 0xfff);
            if (row[x] >> 12) ++visible;
        }
        EXPECT_EQ(padded->Pixels()[(y + 2) * b], 0x12);
        EXPECT_EQ(padded->Pixels()[(y + 2) * b + b - 1], 0x12);
    }
    EXPECT_GT(visible, 0u);
    const auto desc = tight->Describe();
    EXPECT_EQ(desc.width, w); EXPECT_EQ(desc.height, h);
    EXPECT_EQ(desc.format, WW3D_FORMAT_A4R4G4B4);
    EXPECT_EQ(desc.mip_count, 1u); EXPECT_FALSE(desc.missing);
}

TEST(SentenceContentTest, RealProducerCacheAndRepeatedBlitAreByteIdentical)
{
    Font font(new FontCharsClass);
    ASSERT_TRUE(font->Initialize_GDI_Font("Arial", 12, true, nullptr));
    Content content(new Texture2DCPUContent(128, 128));
    Fill(*content, 0);
    int pitch;
    auto *pixels = static_cast<uint16 *>(content->Lock(&pitch));
    font->Blit_Char(L'A', pixels, pitch, 4, 3);
    content->Unlock();
    Content saved(content->Snapshot());
    Fill(*content, 0);
    pixels = static_cast<uint16 *>(content->Lock(&pitch));
    font->Blit_Char(L'A', pixels, pitch, 4, 3);
    content->Unlock();
    EXPECT_EQ(std::memcmp(saved->Pixels(), content->Pixels(), pitch * 128), 0);
}

TEST(SentenceContentTest, RegionLeaseRetainsBackingAndUsesOriginalPitch)
{
    auto *content = new Texture2DCPUContent(5, 4, 16);
    Fill(*content, 0);
    Texture2DRegion region{1, 2, 4, 4};
    std::unique_ptr<Texture2DContent> lease(content->Map(&region));
    ASSERT_NE(lease, nullptr);
    EXPECT_EQ(lease->Mapping().row_pitch, 16);
    EXPECT_EQ(content->Map(), nullptr);
    auto *pixels = static_cast<unsigned char *>(lease->Mapping().data);
    pixels[0] = 0x34; pixels[1] = 0xf2; pixels[16] = 0xab;
    lease.reset();
    EXPECT_EQ(content->Pixels()[34], 0x34);
    EXPECT_EQ(content->Pixels()[35], 0xf2);
    EXPECT_EQ(content->Pixels()[50], 0xab);
    lease.reset(content->Map());
    content->Release_Ref(); // lease is now the only owner; release is balanced.
    EXPECT_EQ(static_cast<unsigned char *>(lease->Mapping().data)[34], 0x34);
}

TEST(SentenceContentTest, InvalidRegionsAreRejectedWithoutLocking)
{
    Content content(new Texture2DCPUContent(4, 3));
    for (auto region : {Texture2DRegion{0, 0, 5, 1}, Texture2DRegion{2, 0, 1, 1},
                        Texture2DRegion{0, 1, 1, 1}, Texture2DRegion{0, 0, 1, 4}})
        EXPECT_EQ(content->Map(&region), nullptr);
    std::unique_ptr<Texture2DContent> lease(content->Map());
    EXPECT_NE(lease, nullptr);
}

TEST(SentenceContentTest, SnapshotFreezesContentAndUploadsOnlyActiveRows)
{
    Content content(new Texture2DCPUContent(3, 2, 10));
    Fill(*content, 0x35);
    Content snapshot(content->Snapshot());
    Fill(*content, 0xa9);
    std::vector<unsigned char> uploaded(24, 0xee);
    snapshot->Copy_Rows(uploaded.data(), 12);
    for (unsigned y = 0; y < 2; ++y)
        for (unsigned x = 0; x < 12; ++x)
            EXPECT_EQ(uploaded[y * 12 + x], x < 6 ? 0x35 : 0xee);
    snapshot->Copy_Rows(uploaded.data(), 12); // repeated upload is stable.
    EXPECT_EQ(uploaded[0], 0x35);
    Content replacement(content->Snapshot());
    replacement->Copy_Rows(uploaded.data(), 12);
    EXPECT_EQ(uploaded[0], 0xa9); // replacement never reuses stale source bytes.
    EXPECT_EQ(snapshot->Pixels()[0], 0x35);
}
