// SPDX-License-Identifier: GPL-3.0-or-later
// Renderer-neutral decode of the legacy world textures (roads, bridges, terrain macro maps):
// dds-first (the legacy loader's rule), DXT1/3/5 or 32-bit DDS, else 24/32-bit TGA, expanded to
// RGBA8 with a 2x2 box-filtered mip chain (the legacy MIP_LEVELS_N / D3DX_FILTER_BOX behavior).
// SDK-free; file access stays with the caller through UITextureReader.
#pragma once

#include "UITextureContent.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct WorldTextureImage
{
    unsigned width = 0, height = 0;
    // mips[0] is width*height RGBA8 (R,G,B,A byte order); each next level halves (min 1).
    std::vector<std::vector<unsigned char>> mips;
};

struct WorldTextureResult
{
    UITextureStatus status = UITextureStatus::Absent;
    WorldTextureImage image;
};

namespace WorldTexture
{
inline unsigned U16(const unsigned char *p) { return p[0] | (unsigned(p[1]) << 8); }
inline unsigned U32(const unsigned char *p) { return U16(p) | (U16(p + 2) << 16); }

inline void Rgb565(unsigned c, unsigned char *out)
{
    const unsigned r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    out[0] = static_cast<unsigned char>((r << 3) | (r >> 2));
    out[1] = static_cast<unsigned char>((g << 2) | (g >> 4));
    out[2] = static_cast<unsigned char>((b << 3) | (b >> 2));
}

// One 8-byte BC1 colour block -> 16 RGBA texels. `fourColor` forces opaque 4-colour mode (BC2/BC3).
inline void DecodeBC1Colour(const unsigned char *block, bool fourColor, unsigned char (*texels)[4])
{
    const unsigned c0 = U16(block), c1 = U16(block + 2);
    unsigned char palette[4][4];
    Rgb565(c0, palette[0]); Rgb565(c1, palette[1]);
    palette[0][3] = palette[1][3] = 255;
    if (c0 > c1 || fourColor) {
        for (int i = 0; i < 3; ++i) {
            palette[2][i] = static_cast<unsigned char>((2 * palette[0][i] + palette[1][i]) / 3);
            palette[3][i] = static_cast<unsigned char>((palette[0][i] + 2 * palette[1][i]) / 3);
        }
        palette[2][3] = palette[3][3] = 255;
    } else {
        for (int i = 0; i < 3; ++i) {
            palette[2][i] = static_cast<unsigned char>((palette[0][i] + palette[1][i]) / 2);
            palette[3][i] = 0;
        }
        palette[2][3] = 255; palette[3][3] = 0;   // transparent black
    }
    const unsigned bits = U32(block + 4);
    for (int i = 0; i < 16; ++i) std::memcpy(texels[i], palette[(bits >> (2 * i)) & 3], 4);
}

inline void DecodeBC3Alpha(const unsigned char *block, unsigned char *alpha /*16*/)
{
    unsigned a[8];
    a[0] = block[0]; a[1] = block[1];
    if (a[0] > a[1]) {
        for (int i = 1; i < 7; ++i) a[1 + i] = ((7 - i) * a[0] + i * a[1]) / 7;
    } else {
        for (int i = 1; i < 5; ++i) a[1 + i] = ((5 - i) * a[0] + i * a[1]) / 5;
        a[6] = 0; a[7] = 255;
    }
    std::uint64_t bits = 0;
    for (int i = 0; i < 6; ++i) bits |= std::uint64_t(block[2 + i]) << (8 * i);
    for (int i = 0; i < 16; ++i) alpha[i] = static_cast<unsigned char>(a[(bits >> (3 * i)) & 7]);
}

// kind: 1 = DXT1, 3 = DXT3, 5 = DXT5. `data` must hold ceil(w/4)*ceil(h/4) blocks.
inline bool DecodeDXT(int kind, const unsigned char *data, size_t size, unsigned w, unsigned h, std::vector<unsigned char> &rgba)
{
    const unsigned bw = (w + 3) / 4, bh = (h + 3) / 4, stride = kind == 1 ? 8 : 16;
    if (size < static_cast<size_t>(bw) * bh * stride) return false;
    rgba.assign(static_cast<size_t>(w) * h * 4, 0);
    for (unsigned by = 0; by < bh; ++by)
        for (unsigned bx = 0; bx < bw; ++bx) {
            const unsigned char *block = data + (static_cast<size_t>(by) * bw + bx) * stride;
            unsigned char texels[16][4];
            unsigned char alpha[16];
            if (kind == 1) {
                DecodeBC1Colour(block, false, texels);
                for (int i = 0; i < 16; ++i) alpha[i] = texels[i][3];
            } else if (kind == 3) {
                DecodeBC1Colour(block + 8, true, texels);
                for (int i = 0; i < 16; ++i) {
                    const unsigned nibble = (block[i / 2] >> ((i & 1) * 4)) & 15;
                    alpha[i] = static_cast<unsigned char>(nibble * 17);
                }
            } else {
                DecodeBC1Colour(block + 8, true, texels);
                DecodeBC3Alpha(block, alpha);
            }
            for (unsigned i = 0; i < 16; ++i) {
                const unsigned x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                if (x >= w || y >= h) continue;
                unsigned char *out = &rgba[(static_cast<size_t>(y) * w + x) * 4];
                out[0] = texels[i][0]; out[1] = texels[i][1]; out[2] = texels[i][2]; out[3] = alpha[i];
            }
        }
    return true;
}

inline bool ValidDimensions(unsigned w, unsigned h) { return w >= 1 && h >= 1 && w <= 4096 && h <= 4096; }

inline WorldTextureResult DecodeDDS(const std::vector<unsigned char> &b)
{
    WorldTextureResult r;
    if (b.size() < 128 || std::memcmp(b.data(), "DDS ", 4) || U32(&b[4]) != 124 || U32(&b[76]) != 32) {
        r.status = UITextureStatus::Invalid; return r;
    }
    const unsigned w = U32(&b[16]), h = U32(&b[12]), flags = U32(&b[80]);
    if (!ValidDimensions(w, h) || U32(&b[112]) || U32(&b[24]) > 1) { r.status = UITextureStatus::Unsupported; return r; }
    std::vector<unsigned char> rgba;
    if (flags & 4) {                                   // FourCC
        int kind = 0;
        if (!std::memcmp(&b[84], "DXT1", 4)) kind = 1;
        else if (!std::memcmp(&b[84], "DXT3", 4)) kind = 3;
        else if (!std::memcmp(&b[84], "DXT5", 4)) kind = 5;
        if (!kind) { r.status = UITextureStatus::Unsupported; return r; }
        if (!DecodeDXT(kind, &b[128], b.size() - 128, w, h, rgba)) { r.status = UITextureStatus::Invalid; return r; }
    } else if ((flags & 0x40) && U32(&b[88]) == 32 && U32(&b[92]) == 0x00ff0000 && U32(&b[96]) == 0x0000ff00 &&
               U32(&b[100]) == 0x000000ff) {          // A8R8G8B8 / X8R8G8B8
        if (b.size() - 128 < static_cast<size_t>(w) * h * 4) { r.status = UITextureStatus::Invalid; return r; }
        const bool hasAlpha = (flags & 1) != 0;
        rgba.resize(static_cast<size_t>(w) * h * 4);
        for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
            const unsigned char *s = &b[128 + i * 4];
            rgba[i * 4] = s[2]; rgba[i * 4 + 1] = s[1]; rgba[i * 4 + 2] = s[0]; rgba[i * 4 + 3] = hasAlpha ? s[3] : 255;
        }
    } else { r.status = UITextureStatus::Unsupported; return r; }
    r.status = UITextureStatus::Ready;
    r.image.width = w; r.image.height = h;
    r.image.mips.push_back(std::move(rgba));
    return r;
}

// Same orientation rules as the engine Targa loader / UITextureContent TGA().
inline WorldTextureResult DecodeTGA(const std::vector<unsigned char> &b)
{
    WorldTextureResult r;
    if (b.size() < 18) { r.status = UITextureStatus::Invalid; return r; }
    if (b[1] || (b[2] != 2 && b[2] != 10) || (b[16] != 24 && b[16] != 32) || (b[17] & 0xc0)) {
        r.status = UITextureStatus::Unsupported; return r;
    }
    const unsigned w = U16(&b[12]), h = U16(&b[14]), bytes = b[16] / 8;
    if (!ValidDimensions(w, h)) { r.status = UITextureStatus::Unsupported; return r; }
    size_t at = 18 + b[0];
    const size_t count = static_cast<size_t>(w) * h;
    if (at > b.size()) { r.status = UITextureStatus::Invalid; return r; }
    std::vector<unsigned char> decoded(count * bytes);
    size_t written = 0;
    while (written < count) {
        unsigned run = 1; bool repeat = false;
        if (b[2] == 10) {
            if (at >= b.size()) { r.status = UITextureStatus::Invalid; return r; }
            const unsigned header = b[at++]; run = (header & 127) + 1; repeat = (header & 128) != 0;
        }
        if (run > count - written || b.size() - at < static_cast<size_t>(repeat ? 1 : run) * bytes) {
            r.status = UITextureStatus::Invalid; return r;
        }
        for (unsigned i = 0; i < run; ++i) {
            std::memcpy(&decoded[(written++) * bytes], &b[at], bytes);
            if (!repeat) at += bytes;
        }
        if (repeat) at += bytes;
    }
    std::vector<unsigned char> rgba(count * 4);
    for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x) {
            const unsigned sx = (b[17] & 16) ? w - 1 - x : x;
            const unsigned sy = (b[17] & 32) ? y : h - 1 - y;
            const unsigned char *s = &decoded[(static_cast<size_t>(sy) * w + sx) * bytes];
            unsigned char *d = &rgba[(static_cast<size_t>(y) * w + x) * 4];
            d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = bytes == 4 ? s[3] : 255;
        }
    r.status = UITextureStatus::Ready;
    r.image.width = w; r.image.height = h;
    r.image.mips.push_back(std::move(rgba));
    return r;
}

// Box-filtered levels below mips[0] until `levels` exist or 1x1 is reached.
inline void BuildMips(WorldTextureImage &image, unsigned levels)
{
    unsigned w = image.width, h = image.height;
    while (image.mips.size() < levels && (w > 1 || h > 1)) {
        const std::vector<unsigned char> &src = image.mips.back();
        const unsigned nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1;
        const unsigned sx = w > 1 ? 2 : 1, sy = h > 1 ? 2 : 1;
        std::vector<unsigned char> next(static_cast<size_t>(nw) * nh * 4);
        for (unsigned y = 0; y < nh; ++y)
            for (unsigned x = 0; x < nw; ++x)
                for (unsigned c = 0; c < 4; ++c) {
                    unsigned sum = 0;
                    for (unsigned dy = 0; dy < sy; ++dy)
                        for (unsigned dx = 0; dx < sx; ++dx)
                            sum += src[(static_cast<size_t>(y * sy + dy) * w + x * sx + dx) * 4 + c];
                    next[(static_cast<size_t>(y) * nw + x) * 4 + c] = static_cast<unsigned char>((sum + sx * sy / 2) / (sx * sy));
                }
        image.mips.push_back(std::move(next));
        w = nw; h = nh;
    }
}
}

// Decodes only the top level (no mip chain); callers that recolor the texels build mips afterwards.
inline WorldTextureResult Load_World_Texture_Base(const std::string &name, const UITextureReader &read)
{
    WorldTextureResult result;
    if (name.size() < 4 || name[name.size() - 4] != '.') {
        result.status = UITextureStatus::Unsupported; return result;
    }
    const UITextureBytes dds = read(name.substr(0, name.size() - 3) + "dds");
    if (dds.status == UITextureStatus::Ready) {
        result = WorldTexture::DecodeDDS(dds.bytes);
    } else if (dds.status == UITextureStatus::Absent) {
        const UITextureBytes tga = read(name);
        if (tga.status != UITextureStatus::Ready) { result.status = tga.status; return result; }
        result = WorldTexture::DecodeTGA(tga.bytes);
    } else {
        result.status = dds.status;
    }
    return result;
}

// `name` is the legacy texture name with a 3-letter extension (usually .tga); the .dds sibling is tried first.
inline WorldTextureResult Load_World_Texture(const std::string &name, const UITextureReader &read, unsigned levels)
{
    WorldTextureResult result;
    if (levels < 1 || levels > 12) { result.status = UITextureStatus::Unsupported; return result; }
    result = Load_World_Texture_Base(name, read);
    if (result.status == UITextureStatus::Ready) WorldTexture::BuildMips(result.image, levels);
    return result;
}
