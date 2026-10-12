// SPDX-License-Identifier: GPL-3.0-or-later
// Renderer-neutral CPU build of the legacy terrain tile atlas (TerrainTextureClass::update).
// The legacy path locks a D3D8 A1R5G5B5 surface and calls D3DX FilterTexture; the
// native path produces the same content on the CPU as RGBA8 with a box-filtered mip
// chain (the legacy D3DX_FILTER_BOX), ready for residency. SDK-free and testable.
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

struct TerrainAtlasTile
{
    int x, y;                         // top-left texel in the atlas (legacy: x <= 0 means unplaced)
    const unsigned char *bgra;        // extent*extent*4 bytes, BGRX rows bottom-up (legacy tile data)
};

struct TerrainAtlasClass
{
    int originX, originY;             // texture class position (legacy: originX <= 0 means unplaced)
    int widthInTiles;
};

struct TerrainAtlasContent
{
    unsigned width = 0, height = 0;
    // mips[0] is width*height RGBA8 (R,G,B,A byte order); each next level halves (min 1).
    std::vector<std::vector<unsigned char>> mips;
};

// Legacy A1R5G5B5 quantization of an 8-bit-per-channel BGR texel, expanded back to
// 8 bits the way the D3D8 sampler would (5-bit replicate). Alpha is always 1.
inline void TerrainAtlas_Quantize(const unsigned char *bgr, unsigned char *rgba)
{
    const unsigned r5 = bgr[2] >> 3, g5 = bgr[1] >> 3, b5 = bgr[0] >> 3;
    rgba[0] = static_cast<unsigned char>((r5 << 3) | (r5 >> 2));
    rgba[1] = static_cast<unsigned char>((g5 << 3) | (g5 >> 2));
    rgba[2] = static_cast<unsigned char>((b5 << 3) | (b5 >> 2));
    rgba[3] = 255;
}

// Builds the atlas exactly like TerrainTextureClass::update:
//  - tiles copied row-inverted (legacy "invert to match") with A1R5G5B5 quantization,
//  - a 4 texel border per texture class duplicated from the opposite edge
//    (columns, then rows including the corners),
//  - box-filter mip chain down to 1x1.
// `extent` is TILE_PIXEL_EXTENT (64), `border` the legacy 4.
inline bool BuildTerrainAtlas(unsigned width, unsigned height, int extent, int border,
                              const std::vector<TerrainAtlasTile> &tiles,
                              const std::vector<TerrainAtlasClass> &classes,
                              TerrainAtlasContent &out)
{
    if (!width || !height || extent <= 0 || border < 0) return false;
    std::vector<unsigned char> base(static_cast<size_t>(width) * height * 4, 0);
    auto texel = [&](int x, int y) -> unsigned char * {
        return &base[(static_cast<size_t>(y) * width + x) * 4];
    };
    auto inside = [&](int x, int y) { return x >= 0 && y >= 0 && x < int(width) && y < int(height); };
    for (const TerrainAtlasTile &tile : tiles) {
        if (tile.x <= 0 || !tile.bgra) continue;
        for (int j = 0; j < extent; ++j) {
            const unsigned char *row = tile.bgra + static_cast<size_t>(extent - 1 - j) * 4 * extent;
            for (int i = 0; i < extent; ++i) {
                if (!inside(tile.x + i, tile.y + j)) return false;
                TerrainAtlas_Quantize(row + i * 4, texel(tile.x + i, tile.y + j));
            }
        }
    }
    for (const TerrainAtlasClass &c : classes) {
        if (c.originX <= 0) continue;
        const int w = c.widthInTiles * extent;
        if (!inside(c.originX - border, c.originY - border) || !inside(c.originX + w + border - 1, c.originY + w + border - 1))
            return false;
        for (int j = 0; j < w; ++j) {            // columns before/after
            for (int k = 0; k < border; ++k) {
                std::memcpy(texel(c.originX - border + k, c.originY + j), texel(c.originX + w - border + k, c.originY + j), 4);
                std::memcpy(texel(c.originX + w + k, c.originY + j), texel(c.originX + k, c.originY + j), 4);
            }
        }
        for (int j = 0; j < border; ++j) {       // rows before/after (full width + borders)
            for (int i = -border; i < w + border; ++i) {
                std::memcpy(texel(c.originX + i, c.originY - j - 1), texel(c.originX + i, c.originY + w - j - 1), 4);
                std::memcpy(texel(c.originX + i, c.originY + w + j), texel(c.originX + i, c.originY + j), 4);
            }
        }
    }
    out.width = width; out.height = height;
    out.mips.clear();
    out.mips.push_back(std::move(base));
    unsigned w = width, h = height;
    while (w > 1 || h > 1) {
        const unsigned nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1;
        std::vector<unsigned char> next(static_cast<size_t>(nw) * nh * 4);
        const std::vector<unsigned char> &src = out.mips.back();
        for (unsigned y = 0; y < nh; ++y) for (unsigned x = 0; x < nw; ++x) {
            for (unsigned c = 0; c < 4; ++c) {
                unsigned sum = 0, count = 0;
                for (unsigned dy = 0; dy < (h > 1 ? 2u : 1u); ++dy) for (unsigned dx = 0; dx < (w > 1 ? 2u : 1u); ++dx) {
                    sum += src[((static_cast<size_t>(y) * (h > 1 ? 2 : 1) + dy) * w + x * (w > 1 ? 2 : 1) + dx) * 4 + c];
                    ++count;
                }
                next[(static_cast<size_t>(y) * nw + x) * 4 + c] = static_cast<unsigned char>((sum + count / 2) / count);
            }
        }
        out.mips.push_back(std::move(next));
        w = nw; h = nh;
    }
    return true;
}
