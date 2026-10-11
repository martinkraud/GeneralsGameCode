// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "LegacyTextureCreationRecording.h"
namespace TextureCreationCases {
// Candidate order measured from the pinned D3DX8 reject-all trace.
inline constexpr UINT formats[] = {20,21,22,23,24,40,41,25,26,27,28,29,30,31,34,50,51,52,60,61,62,63,64,65,67,1498831189,844715353,827611204,844388420,861165636,877942852,894720068,70,71,73,75,80,77,79,909200416,909200449,909201952,909201985};
inline Input Supported() { Input i; for (auto f : formats) i.supported.push_back(D3DFORMAT(f)); return i; }
inline void Caps(Input &i, unsigned variant) {
    switch(variant) {
    case 1: i.caps.TextureCaps = 0; break;
    case 2: i.caps.TextureCaps |= D3DPTEXTURECAPS_POW2; break;
    case 3: i.caps.TextureCaps |= D3DPTEXTURECAPS_CUBEMAP_POW2 | D3DPTEXTURECAPS_VOLUMEMAP_POW2; break;
    case 4: i.caps.TextureCaps |= D3DPTEXTURECAPS_SQUAREONLY; break;
    case 5: i.caps.MaxTextureAspectRatio = 2; break;
    case 6: i.caps.MaxTextureWidth = 100; i.caps.MaxTextureHeight = 47; i.caps.MaxVolumeExtent = 19; break;
    case 7: i.caps.TextureCaps |= D3DPTEXTURECAPS_NONPOW2CONDITIONAL; break;
    case 8: i.caps.TextureCaps |= D3DPTEXTURECAPS_POW2 | D3DPTEXTURECAPS_NONPOW2CONDITIONAL; break;
    case 9: i.caps.TextureCaps &= ~(D3DPTEXTURECAPS_CUBEMAP | D3DPTEXTURECAPS_VOLUMEMAP); break;
    }
}
template<class Visit> void Examples(Visit visit) {
    for (auto type : {D3DRTYPE_TEXTURE,D3DRTYPE_CUBETEXTURE,D3DRTYPE_VOLUMETEXTURE})
        for (unsigned n=0;n<30;++n) {
            auto i=Supported(); i.type=type;
            switch(n) {
            case 1: Caps(i,2); Caps(i,3); break;
            case 2: i.width=i.height=i.depth=0; break;
            case 3: i.width=i.height=i.depth=0xffffffffu; break;
            case 4: i.width=0xffffffffu; i.height=19; break;
            case 5: i.levels=1; break;
            case 6: i.levels=99; break;
            case 7: Caps(i,1); break;
            case 8: Caps(i,4); break;
            case 9: Caps(i,5); i.width=257; i.height=7; break;
            case 10: Caps(i,6); Caps(i,2); Caps(i,3); i.width=i.height=i.depth=4096; break;
            case 11: Caps(i,7); break;
            case 12: i.format=D3DFMT_DXT1; i.width=3; i.height=5; break;
            case 13: i.supported.clear(); break;
            case 14: i.supported={D3DFMT_R5G6B5}; break;
            case 15: i.format=D3DFMT_L8; i.supported={D3DFMT_A8L8}; break;
            case 16: i.createResult=D3DERR_OUTOFVIDEOMEMORY; break;
            case 17: i.createResult=D3DERR_NOTAVAILABLE; i.clearOnFailure=true; break;
            case 18: i.createResult=E_FAIL; break;
            case 19: i.createResult=S_FALSE; break;
            case 20: i.nullDevice=true; break;
            case 21: i.nullOutput=true; break;
            case 22: i.pool=D3DPOOL(99); break;
            case 23: i.usage=4; break;
            case 24: i.pool=D3DPOOL_SCRATCH; i.format=D3DFMT_UNKNOWN; Caps(i,1); break;
            case 25: i.pool=D3DPOOL_SCRATCH; i.format=D3DFMT_DXT1; break;
            case 26: Caps(i,9); break;
            case 27: i.depth=50; break;
            case 28: i.format=D3DFORMAT(0xdeadbeefu); break;
            case 29: i.pool=D3DPOOL_SYSTEMMEM; Caps(i,6); i.width=i.height=i.depth=4096; break;
            }
            visit(i);
        }
}
template<class Visit> void Sweep(Visit visit) {
    for (auto type : {D3DRTYPE_TEXTURE,D3DRTYPE_CUBETEXTURE,D3DRTYPE_VOLUMETEXTURE}) {
        // Engine formats, all pinned candidates, unknown/invalid, pools and usage.
        for (UINT f : {0u,0xdeadbeefu}) {
            auto i=Supported(); i.type=type; i.format=D3DFORMAT(f); visit(i);
        }
        for (auto f : formats) for (auto pool : {D3DPOOL_DEFAULT,D3DPOOL_MANAGED,D3DPOOL_SYSTEMMEM,D3DPOOL_SCRATCH})
            for (DWORD usage : {0u,1u,2u,8u,0x200u,0xffffffffu,4u}) {
                auto i=Supported(); i.type=type; i.format=D3DFORMAT(f); i.pool=pool; i.usage=usage; visit(i);
            }
        for (unsigned caps=0; caps<10; ++caps)
            for (auto whd : {std::array<UINT,3>{0,0,0},{1,2,3},{63,31,7},{31,63,19},{257,129,65},
                    {4096,2048,1024},{0xffffffffu,0xffffffffu,0xffffffffu},{0xffffffffu,19,0xffffffffu},
                    {19,0xffffffffu,0},{1025,513,257},{4,4,4},{1,511,31}})
                for (UINT levels : {0u,1u,2u,5u,32u,0xffffffffu})
                    for (auto pool : {D3DPOOL_MANAGED,D3DPOOL_SCRATCH})
                        for (auto format : {D3DFMT_A8R8G8B8,D3DFMT_DXT1}) {
                            auto i=Supported(); i.type=type; Caps(i,caps); i.width=whd[0]; i.height=whd[1];
                            i.depth=whd[2]; i.levels=levels; i.pool=pool; i.format=format; visit(i);
                        }
        // Every singleton/pair support set for every recognized requested format.
        for (unsigned request=0; request<=std::size(formats); ++request) {
            Input i; i.type=type; i.format=D3DFORMAT(request ? formats[request-1] : 0);
            visit(i); // reject all
            for (unsigned a=0; a<std::size(formats); ++a) {
                i.supported={D3DFORMAT(formats[a])}; visit(i);
                for (unsigned b=a+1; b<std::size(formats); ++b) {
                    i.supported={D3DFORMAT(formats[a]),D3DFORMAT(formats[b])}; visit(i);
                }
            }
            std::uint32_t state=0xabcd1234;
            for (unsigned n=0;n<64;++n) {
                i.supported.clear();
                for(auto f:formats) { state=state*1664525u+1013904223u; if(state&0x80000000u)i.supported.push_back(D3DFORMAT(f)); }
                visit(i);
            }
        }
        for (auto hr : {S_OK,S_FALSE,E_FAIL,D3DERR_INVALIDCALL,D3DERR_OUTOFVIDEOMEMORY,D3DERR_NOTAVAILABLE})
            for(bool clear : {false,true}) {
                auto i=Supported(); i.type=type; i.createResult=hr; i.clearOnFailure=clear; visit(i);
            }
        for (unsigned invalid=0;invalid<3;++invalid) {
            auto i=Supported(); i.type=type; i.nullDevice=invalid==0; i.nullOutput=invalid==1;
            if(invalid==2)i.pool=D3DPOOL(99); visit(i);
        }
    }
}
inline void Hash(std::uint64_t &hash, UINT word) {
    for(unsigned b=0;b<4;++b) { hash^=(word>>(8*b))&255u; hash*=1099511628211ull; }
}
inline void Hash(std::uint64_t &hash,const Result &r) {
    Hash(hash,UINT(r.hr)); Hash(hash,r.output); Hash(hash,UINT(r.events.size()));
    for(auto e:r.events) for(auto w:e)Hash(hash,w);
    for(auto w:r.ownership)Hash(hash,w);
}
}
