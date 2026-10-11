// SPDX-License-Identifier: GPL-3.0-or-later
#include "UITextureContent.h"
#include <cstring>

namespace {
unsigned U16(const unsigned char *p) { return p[0] | (unsigned(p[1]) << 8); }
unsigned U32(const unsigned char *p) { return U16(p) | (U16(p + 2) << 16); }
bool Dimensions(unsigned w, unsigned h, const UITexturePolicy &p)
{
    // This slice preserves dimensions exactly. Legacy POT scaling, dimension
    // clamping/aspect adjustments and compressed reduction are not guessed.
    return w >= 4 && h >= 4 && w <= p.max_width && h <= p.max_height &&
           w <= 8192 && h <= 8192 && !(w & (w - 1)) && !(h & (h - 1));
}
UITextureResult Result(UITextureStatus status) { return {status, {}}; }
UITextureResult DDS(const std::vector<unsigned char> &b, const UITexturePolicy &p)
{
    if (b.size() < 128 || std::memcmp(b.data(), "DDS ", 4) || U32(&b[4]) != 124 || U32(&b[76]) != 32)
        return Result(UITextureStatus::Invalid);
    if (!(U32(&b[80]) & 4) || U32(&b[112]) || U32(&b[24]) > 1)
        return Result(UITextureStatus::Unsupported); // non-FourCC, cube/volume
    unsigned w = U32(&b[16]), h = U32(&b[12]);
    if (!Dimensions(w, h, p)) return Result(UITextureStatus::Unsupported);
    WW3DFormat format;
    if (!std::memcmp(&b[84], "DXT1", 4)) format = WW3D_FORMAT_DXT1;
    else if (!std::memcmp(&b[84], "DXT3", 4)) format = WW3D_FORMAT_DXT3;
    else if (!std::memcmp(&b[84], "DXT5", 4)) format = WW3D_FORMAT_DXT5;
    else return Result(UITextureStatus::Unsupported); // DXT2/4 premultiplication, DX10 etc.
    UITextureContent content(W3DNEW Texture2DCPUContent(w, h, format, 0));
    const size_t size = size_t(content->Row_Pitch()) * content->Storage_Rows();
    // DDSFileClass loads its retained source chain even for a single-mip
    // destination: zero becomes one; the two lowest levels are dropped. Keep
    // that availability check, but copy only level zero into CPU residency.
    unsigned levels = U32(&b[28]);
    if (levels > 14) return Result(UITextureStatus::Invalid);
    levels = levels > 2 ? levels - 2 : 1;
    size_t required = 0, level_size = size;
    for (unsigned i = 0; i < levels; ++i) {
        required += level_size;
        if (level_size > 16) level_size /= 4;
    }
    if (b.size() - 128 < required) return Result(UITextureStatus::Invalid);
    int pitch;
    std::memcpy(content->Lock(&pitch), &b[128], size);
    content->Unlock();
    // The single-mip caller retains the top level, just as reduction zero in
    // DDSFileClass::Copy_Level_To_Surface; lower mips never enter residency.
    return {UITextureStatus::Ready, std::move(content)};
}
UITextureResult TGA(const std::vector<unsigned char> &b, const UITexturePolicy &p)
{
    if (b.size() < 18) return Result(UITextureStatus::Invalid);
    if (b[1] || (b[2] != 2 && b[2] != 10) || (b[16] != 24 && b[16] != 32) || (b[17] & 0xc0))
        return Result(UITextureStatus::Unsupported);
    unsigned w = U16(&b[12]), h = U16(&b[14]), bytes = b[16] / 8;
    if (!Dimensions(w, h, p)) return Result(UITextureStatus::Unsupported);
    size_t at = 18 + b[0], count = size_t(w) * h;
    if (at > b.size()) return Result(UITextureStatus::Invalid);
    std::vector<unsigned char> decoded(count * bytes);
    size_t written = 0;
    while (written < count) {
        unsigned run = 1; bool repeat = false;
        if (b[2] == 10) {
            if (at >= b.size()) return Result(UITextureStatus::Invalid);
            unsigned header = b[at++]; run = (header & 127) + 1; repeat = (header & 128) != 0;
        }
        if (run > count - written || b.size() - at < size_t(repeat ? 1 : run) * bytes)
            return Result(UITextureStatus::Invalid);
        for (unsigned i = 0; i < run; ++i) {
            std::memcpy(&decoded[(written++) * bytes], &b[at], bytes);
            if (!repeat) at += bytes;
        }
        if (repeat) at += bytes;
    }
    // Get_WW3D_Format + Get_Valid_Texture_Format: 24-bit BGR -> X8R8G8B8;
    // 32-bit BGRA -> A8R8G8B8 on the supported 32-bit reference policy.
    const auto f = p.texture_bits == 16 ? (bytes == 4 ? WW3D_FORMAT_A4R4G4B4 : WW3D_FORMAT_R5G6B5) :
                                        (bytes == 4 ? WW3D_FORMAT_A8R8G8B8 : WW3D_FORMAT_X8R8G8B8);
    UITextureContent content(W3DNEW Texture2DCPUContent(w, h, f, 0));
    int pitch; auto *out = static_cast<unsigned char *>(content->Lock(&pitch));
    for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x) {
        // Same orientation as Load_Uncompressed_Mipmap's YORIGIN xor followed
        // by Targa::Load's XFlip/YFlip. Source bytes stay BGR(A), no inversion.
        unsigned sx = (b[17] & 16) ? w - 1 - x : x;
        unsigned sy = (b[17] & 32) ? y : h - 1 - y;
        const auto *src = &decoded[(size_t(sy) * w + sx) * bytes];
        auto *dst = out + size_t(y) * pitch + x * (p.texture_bits == 16 ? 2 : 4);
        if (p.texture_bits == 16) {
            unsigned short value = bytes == 4 ? ((src[3] & 0xf0) << 8) | ((src[2] & 0xf0) << 4) | (src[1] & 0xf0) | (src[0] >> 4) :
                                               ((src[2] >> 3) << 11) | ((src[1] >> 2) << 5) | (src[0] >> 3);
            std::memcpy(dst, &value, 2);
        } else {
            std::memcpy(dst, src, 3); dst[3] = bytes == 4 ? src[3] : 255;
        }
    }
    content->Unlock();
    return {UITextureStatus::Ready, std::move(content)};
}
}

UITextureResult Load_UI_Texture_Content(const std::string &name, const UITexturePolicy &p, const UITextureReader &read)
{
    if (name.size() < 4 || name[name.size() - 4] != '.' || (p.texture_bits != 16 && p.texture_bits != 32) || p.hsv_shift)
        return Result(UITextureStatus::Unsupported);
    if (p.allow_compression) {
        const auto dds = read(name.substr(0, name.size() - 3) + "dds");
        if (dds.status == UITextureStatus::Ready) return DDS(dds.bytes, p);
        if (dds.status != UITextureStatus::Absent) return Result(dds.status);
    }
    const auto tga = read(name);
    if (tga.status != UITextureStatus::Ready) return Result(tga.status);
    return TGA(tga.bytes, p);
}
