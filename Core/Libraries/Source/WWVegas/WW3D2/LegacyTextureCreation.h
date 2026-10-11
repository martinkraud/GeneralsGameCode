// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <d3d8.h>

namespace LegacyD3DX {
namespace TextureCreationDetail {
#include "LegacyTextureFormatPolicy.inc"
static_assert(sizeof(UINT)==4 && sizeof(DWORD)==4);
static_assert(sizeof(textureFormats)/sizeof(textureFormats[0])==43);
static_assert(sizeof(textureFormatRanks)/sizeof(textureFormatRanks[0])==44);

// The pinned implementation compares static format scores, then byte sizes,
// then table order. Measured ranks encode that total order without reproducing
// its unrelated pixel conversion machinery. Query order remains the original.
inline D3DFORMAT Closest(IDirect3DDevice8 *device, DWORD usage, D3DRESOURCETYPE type, D3DFORMAT requested)
{
    IDirect3D8 *d3d = nullptr;
    D3DCAPS8 caps{};
    D3DDISPLAYMODE mode{};
    device->GetDirect3D(&d3d);
    device->GetDeviceCaps(&caps);
    device->GetDisplayMode(&mode);
    if (!d3d) return D3DFMT_UNKNOWN; // Failed interface acquisition has no usable device contract.
    unsigned row = 0;
    for (unsigned n=0;n<43;++n) if (textureFormats[n]==unsigned(requested)) { row=n+1; break; }
    unsigned best = 256;
    D3DFORMAT format = D3DFMT_UNKNOWN;
    for (unsigned n=0;n<43;++n) {
        const auto candidate = D3DFORMAT(textureFormats[n]);
        if (FAILED(d3d->CheckDeviceFormat(caps.AdapterOrdinal,caps.DeviceType,mode.Format,usage,type,candidate)))
            continue;
        if (candidate==requested) { format=candidate; break; }
        const auto rank = textureFormatRanks[row][n];
        if (rank && rank<best) { best=rank; format=candidate; }
    }
    d3d->Release();
    return format;
}
inline UINT Smaller(UINT a,UINT b) { return a<b ? a:b; }
inline UINT Larger(UINT a,UINT b) { return a>b ? a:b; }
inline UINT PowerOfTwo(UINT n) { UINT result=1; while(result<n)result<<=1; return result; }
inline UINT Levels(UINT n) { UINT result=0; while(n) { ++result; n>>=1; } return result; }
inline bool IsPowerOfTwo(UINT n) { return (n & (n-1))==0; }

inline HRESULT Requirements(IDirect3DDevice8 *device, D3DRESOURCETYPE type,
    UINT &width,UINT &height,UINT &depth,UINT &levels,DWORD usage,D3DFORMAT &format,D3DPOOL pool)
{
    if ((usage & 0x20bu)!=usage || unsigned(pool)>3) return D3DERR_INVALIDCALL;
    if (pool!=D3DPOOL_SCRATCH) {
        format=Closest(device,usage,type,format);
        if (format==D3DFMT_UNKNOWN)return D3DERR_NOTAVAILABLE;
    } else if (format==D3DFMT_UNKNOWN)format=D3DFMT_A8R8G8B8;

    constexpr UINT Default=0xffffffffu;
    if (width==Default)width=height==Default ? 256:height;
    if (height==Default)height=width;
    if (!width)width=1;
    if (!height)height=1;
    if (type==D3DRTYPE_CUBETEXTURE)width=height=Larger(width,height);
    if (!depth || depth==Default)depth=1;
    D3DCAPS8 caps{};
    if (pool!=D3DPOOL_SCRATCH) {
        device->GetDeviceCaps(&caps);
        if (type==D3DRTYPE_VOLUMETEXTURE) {
            width=Smaller(width,caps.MaxVolumeExtent);
            height=Smaller(height,caps.MaxVolumeExtent);
            depth=Smaller(depth,caps.MaxVolumeExtent);
        } else {
            width=Smaller(width,caps.MaxTextureWidth);
            height=Smaller(height,caps.MaxTextureHeight);
            if (type==D3DRTYPE_TEXTURE) {
                const UINT ratio=(caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) ? 1:caps.MaxTextureAspectRatio;
                if (ratio) { width=Smaller(width,ratio*height); height=Smaller(height,ratio*width); }
            }
        }
        const DWORD pow2=type==D3DRTYPE_TEXTURE ? D3DPTEXTURECAPS_POW2 :
            type==D3DRTYPE_CUBETEXTURE ? D3DPTEXTURECAPS_CUBEMAP_POW2:D3DPTEXTURECAPS_VOLUMEMAP_POW2;
        if (caps.TextureCaps & pow2) { width=PowerOfTwo(width); height=PowerOfTwo(height); depth=PowerOfTwo(depth); }
    }
    // Block compression rounds width/height even for scratch; depth is not rounded.
    if (format==D3DFMT_DXT1 || format==D3DFMT_DXT2 || format==D3DFMT_DXT3 ||
        format==D3DFMT_DXT4 || format==D3DFMT_DXT5) { width=(width+3)&~3u; height=(height+3)&~3u; }
    const DWORD mip=type==D3DRTYPE_TEXTURE ? D3DPTEXTURECAPS_MIPMAP :
        type==D3DRTYPE_CUBETEXTURE ? D3DPTEXTURECAPS_MIPCUBEMAP:D3DPTEXTURECAPS_MIPVOLUMEMAP;
    if (pool!=D3DPOOL_SCRATCH && (!(caps.TextureCaps & mip) ||
        ((caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) &&
        (!IsPowerOfTwo(width) || !IsPowerOfTwo(height) || !IsPowerOfTwo(depth))))) levels=1;
    else {
        const UINT maximum=Levels(type==D3DRTYPE_VOLUMETEXTURE ? Larger(Larger(width,height),depth):Larger(width,height));
        if (!levels || levels>maximum)levels=maximum;
    }
    return S_OK;
}
inline DWORD Usage(DWORD usage) { return usage==0xffffffffu ? 0:usage; }
inline HRESULT CreationResult(HRESULT hr) { return FAILED(hr) ? hr:S_OK; }
}

inline HRESULT CreateTexture(IDirect3DDevice8 *device,UINT width,UINT height,UINT levels,
    DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DTexture8 **out)
{
    if (!device || !out)return D3DERR_INVALIDCALL;
    usage=TextureCreationDetail::Usage(usage);
    UINT depth=1;
    const HRESULT hr=TextureCreationDetail::Requirements(device,D3DRTYPE_TEXTURE,width,height,depth,levels,usage,format,pool);
    if (FAILED(hr))return hr;
    return TextureCreationDetail::CreationResult(device->CreateTexture(width,height,levels,usage,format,pool,out));
}
inline HRESULT CreateCubeTexture(IDirect3DDevice8 *device,UINT edge,UINT levels,
    DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DCubeTexture8 **out)
{
    if (!device || !out)return D3DERR_INVALIDCALL;
    usage=TextureCreationDetail::Usage(usage);
    UINT height=edge,depth=1;
    const HRESULT hr=TextureCreationDetail::Requirements(device,D3DRTYPE_CUBETEXTURE,edge,height,depth,levels,usage,format,pool);
    if (FAILED(hr))return hr;
    // The pinned cube requirement helper aliases width and height; its final
    // height store wins when maximum width/height differ.
    return TextureCreationDetail::CreationResult(device->CreateCubeTexture(height,levels,usage,format,pool,out));
}
inline HRESULT CreateVolumeTexture(IDirect3DDevice8 *device,UINT width,UINT height,UINT depth,UINT levels,
    DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DVolumeTexture8 **out)
{
    if (!device || !out)return D3DERR_INVALIDCALL;
    usage=TextureCreationDetail::Usage(usage);
    const HRESULT hr=TextureCreationDetail::Requirements(device,D3DRTYPE_VOLUMETEXTURE,width,height,depth,levels,usage,format,pool);
    if (FAILED(hr))return hr;
    return TextureCreationDetail::CreationResult(device->CreateVolumeTexture(width,height,depth,levels,usage,format,pool,out));
}
}
