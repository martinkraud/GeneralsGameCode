// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <d3dx8tex.h>
namespace TextureCreationCases {
struct Legacy {
    static HRESULT Texture(IDirect3DDevice8 *d, UINT w, UINT h, UINT l, DWORD u, D3DFORMAT f,
        D3DPOOL p, IDirect3DTexture8 **out) { return D3DXCreateTexture(d,w,h,l,u,f,p,out); }
    static HRESULT Cube(IDirect3DDevice8 *d, UINT w, UINT l, DWORD u, D3DFORMAT f,
        D3DPOOL p, IDirect3DCubeTexture8 **out) { return D3DXCreateCubeTexture(d,w,l,u,f,p,out); }
    static HRESULT Volume(IDirect3DDevice8 *d, UINT w, UINT h, UINT z, UINT l, DWORD u, D3DFORMAT f,
        D3DPOOL p, IDirect3DVolumeTexture8 **out) { return D3DXCreateVolumeTexture(d,w,h,z,l,u,f,p,out); }
};
}
