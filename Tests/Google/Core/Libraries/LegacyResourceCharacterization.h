// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "LegacyTextureCreationStubs.h"
#include <d3dx8.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <array>
#include <cstdint>

namespace ResourceCharacterization {
using TextureCreationCases::Unexpected;
inline unsigned PixelBytes(D3DFORMAT f) {
    switch(f) {
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: return 4;
    case D3DFMT_R8G8B8: return 3;
    case D3DFMT_R5G6B5: case D3DFMT_A1R5G5B5: case D3DFMT_A4R4G4B4:
    case D3DFMT_V8U8: case D3DFMT_A8L8: return 2;
    case D3DFMT_L8: case D3DFMT_P8: return 1;
    default: Unexpected("unbounded fixture format");
    }
}
inline std::string Hex(const void *data,std::size_t count) {
    const auto *p=static_cast<const unsigned char*>(data);
    std::ostringstream s; s<<std::hex<<std::setfill('0');
    for(std::size_t i=0;i<count;++i)s<<std::setw(2)<<unsigned(p[i]);
    return s.str();
}
inline std::string Events(const std::vector<std::string> &v) {
    std::string s;
    for(const auto &x:v){if(!s.empty())s+=';';s+=x;}
    return s;
}
inline bool Compressed(D3DFORMAT f) {return f==D3DFMT_DXT1 || f==D3DFMT_DXT3 || f==D3DFMT_DXT5;}
inline unsigned RowBytes(D3DFORMAT f,unsigned w) {
    return Compressed(f)?std::max(1u,(w+3)/4)*(f==D3DFMT_DXT1?8:16):w*PixelBytes(f);
}
inline unsigned StorageRows(D3DFORMAT f,unsigned h) {return Compressed(f)?std::max(1u,(h+3)/4):h;}
struct Device : TextureCreationCases::IDirect3DDevice8Stub {
    std::vector<std::string> &events;
    ULONG refs=1;
    explicit Device(std::vector<std::string> &e):events(e) {}
    ULONG STDMETHODCALLTYPE AddRef() override {events.push_back("device:add");return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override {events.push_back("device:release");return --refs;}
    HRESULT STDMETHODCALLTYPE CopyRects(IDirect3DSurface8*,const RECT*,UINT,IDirect3DSurface8*,const POINT*) override {
        events.push_back("device:copy-rects-unavailable");return D3DERR_INVALIDCALL;
    }
    HRESULT STDMETHODCALLTYPE CreateImageSurface(UINT w,UINT h,D3DFORMAT f,IDirect3DSurface8 **out) override {
        events.push_back("device:image-fail,"+std::to_string(w)+","+std::to_string(h)+","+std::to_string(f));
        *out=nullptr;return E_FAIL;
    }
};
inline Device *observerDevice=nullptr;
struct Surface : IDirect3DSurface8 {
    std::string name;
    D3DSURFACE_DESC desc{};
    unsigned pitch;
    std::vector<unsigned char> bytes;
    std::vector<std::string> &events;
    ULONG refs=1;
    HRESULT descResult=S_OK,lockResult=S_OK,unlockResult=S_OK;
    bool locked=false;
    IDirect3DBaseTexture8 *owner=nullptr;
    Surface(std::string n,unsigned w,unsigned h,D3DFORMAT f,std::vector<std::string> &e)
        :name(n),pitch(((RowBytes(f,w)+3)&~3u)+8),bytes(pitch*StorageRows(f,h)+16,0xa5),events(e) {
        desc.Format=f;desc.Type=D3DRTYPE_SURFACE;desc.Pool=D3DPOOL_SYSTEMMEM;
        desc.Width=w;desc.Height=h;desc.Size=RowBytes(f,w)*StorageRows(f,h);
    }
    void Log(const std::string &s){events.push_back(name+":"+s);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {Unexpected("surface QI");}
    ULONG STDMETHODCALLTYPE AddRef() override {Log("add");return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override {Log("release");return --refs;}
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8 **out) override {
        Log("get-device");if(!observerDevice)Unexpected("missing observer device");
        *out=observerDevice;observerDevice->AddRef();return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID,const void*,DWORD,DWORD) override {Unexpected("surface private");}
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID,void*,DWORD*) override {Unexpected("surface private");}
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID) override {Unexpected("surface private");}
    HRESULT STDMETHODCALLTYPE GetContainer(REFIID iid,void **out) override {
        Log("container,"+std::to_string(iid.Data1));
        if(!owner){*out=nullptr;return E_NOINTERFACE;}
        if(iid!=IID_IDirect3DBaseTexture8 && iid!=IID_IDirect3DTexture8 && iid!=IID_IUnknown)
            Unexpected("unknown surface container IID");
        *out=owner;owner->AddRef();return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC *out) override {
        // Keep the injected failure payload defined: the pinned helper can read
        // it even when GetDesc reports failure. No uninitialized oracle input.
        Log("desc");*out=desc;return descResult;
    }
    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT *out,const RECT *rect,DWORD flags) override {
        std::ostringstream s;s<<"lock,"<<flags;
        if(rect)s<<","<<rect->left<<","<<rect->top<<","<<rect->right<<","<<rect->bottom;
        else s<<",full";
        Log(s.str());
        if(FAILED(lockResult))return lockResult;
        if(locked)Unexpected("nested surface lock");
        if(rect && (rect->left<0 || rect->top<0 || rect->right>LONG(desc.Width) || rect->bottom>LONG(desc.Height)))
            return D3DERR_INVALIDCALL;
        locked=true;out->Pitch=pitch;
        if(Compressed(desc.Format))out->pBits=bytes.data()+(rect?(rect->top/4)*pitch+(rect->left/4)*(desc.Format==D3DFMT_DXT1?8:16):0);
        else out->pBits=bytes.data()+(rect?rect->top*pitch+rect->left*PixelBytes(desc.Format):0);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UnlockRect() override {
        Log("unlock");if(!locked)Unexpected("unlock without lock");locked=false;return unlockResult;
    }
};
inline void Fill(Surface &s,unsigned pattern) {
    for(unsigned y=0;y<s.desc.Height;++y)for(unsigned x=0;x<s.desc.Width;++x) {
        auto *p=s.bytes.data()+y*s.pitch+x*PixelBytes(s.desc.Format);
        if(pattern==0)for(unsigned c=0;c<PixelBytes(s.desc.Format);++c)p[c]=static_cast<unsigned char>((x*37+y*73+c*61)&255);
        if(pattern==1)for(unsigned c=0;c<PixelBytes(s.desc.Format);++c)p[c]=((x+y)&1)?255:0;
        if(pattern==2)for(unsigned c=0;c<PixelBytes(s.desc.Format);++c)p[c]=(x==s.desc.Width/2 && y==s.desc.Height/2)?255:0;
        if(pattern==3)for(unsigned c=0;c<PixelBytes(s.desc.Format);++c)p[c]=(c==3)?static_cast<unsigned char>(x*255/std::max(1u,s.desc.Width-1)):255;
    }
}
struct Texture : TextureCreationCases::IDirect3DTexture8Stub {
    std::vector<std::string> &events;
    std::vector<Surface> levels;
    ULONG refs=1;
    int failLevel=-1;
    Texture(unsigned w,unsigned h,D3DFORMAT f,std::vector<std::string> &e):events(e) {
        for(unsigned n=0;;++n) {
            levels.emplace_back("m"+std::to_string(n),w,h,f,events);
            levels.back().owner=this;
            if(w==1 && h==1)break;w=std::max(1u,w/2);h=std::max(1u,h/2);
        }
    }
    ULONG STDMETHODCALLTYPE AddRef() override {events.push_back("texture:add");return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override {events.push_back("texture:release");return --refs;}
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override {events.push_back("texture:type");return D3DRTYPE_TEXTURE;}
    DWORD STDMETHODCALLTYPE GetLevelCount() override {events.push_back("texture:levels");return DWORD(levels.size());}
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT n,D3DSURFACE_DESC *out) override {events.push_back("texture:desc,"+std::to_string(n));*out=levels.at(n).desc;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT n,IDirect3DSurface8 **out) override {
        events.push_back("texture:surface,"+std::to_string(n));
        if(int(n)==failLevel){*out=nullptr;return E_FAIL;}
        *out=&levels.at(n);levels[n].AddRef();return S_OK;
    }
};
struct Row {
    std::string id,kind,events,pixels,source;
    HRESULT hr=S_OK;
    unsigned sf=0,df=0,sw=0,sh=0,dw=0,dh=0,filter=0,pitch=0,level=0;
    std::string JSON() const {
        std::ostringstream s;
        s<<"{\"id\":"<<std::quoted(id)<<",\"kind\":"<<std::quoted(kind)<<",\"hr\":"<<std::uint32_t(hr)
         <<",\"sf\":"<<sf<<",\"df\":"<<df<<",\"sw\":"<<sw<<",\"sh\":"<<sh<<",\"dw\":"<<dw<<",\"dh\":"<<dh
         <<",\"filter\":"<<filter<<",\"pitch\":"<<pitch<<",\"level\":"<<level
         <<",\"pixels\":"<<std::quoted(pixels)<<",\"source\":"<<std::quoted(source)<<",\"events\":"<<std::quoted(events)<<"}";
        return s.str();
    }
};
inline Row Copy(const std::string &id,Surface &src,Surface &dst,DWORD filter,const RECT *sr=nullptr,const RECT *dr=nullptr) {
    src.events.clear();
    Row r;r.id=id;r.kind="surface";r.sf=src.desc.Format;r.df=dst.desc.Format;
    r.sw=src.desc.Width;r.sh=src.desc.Height;r.dw=dst.desc.Width;r.dh=dst.desc.Height;r.filter=filter;r.pitch=dst.pitch;
    r.hr=D3DXLoadSurfaceFromSurface(&dst,nullptr,dr,&src,nullptr,sr,filter,0);
    r.pixels=Hex(dst.bytes.data(),dst.bytes.size());r.source=Hex(src.bytes.data(),src.bytes.size());r.events=Events(src.events);
    return r;
}
}
