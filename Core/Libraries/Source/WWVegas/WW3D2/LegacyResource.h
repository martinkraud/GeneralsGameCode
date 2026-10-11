// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <d3dx8.h>

namespace LegacyD3DX {
// Bounded X64.9 resource contract; no palettes, color key or arbitrary flags.
HRESULT EncodeResourceBlock(D3DFORMAT format,unsigned char *out,const D3DXCOLOR *rgba);
HRESULT LoadSurfaceFromSurface(IDirect3DSurface8 *destination,const PALETTEENTRY *destinationPalette,
    const RECT *destinationRect,IDirect3DSurface8 *source,const PALETTEENTRY *sourcePalette,
    const RECT *sourceRect,DWORD filter,D3DCOLOR colorKey);
HRESULT FilterTexture(IDirect3DBaseTexture8 *texture,const PALETTEENTRY *palette,UINT sourceLevel,DWORD filter);
HRESULT CreateTextureFromFileExA(IDirect3DDevice8 *device,const char *name,UINT width,UINT height,UINT levels,
    DWORD usage,D3DFORMAT format,D3DPOOL pool,DWORD filter,DWORD mipFilter,D3DCOLOR key,
    D3DXIMAGE_INFO *info,PALETTEENTRY *palette,IDirect3DTexture8 **out);
}
