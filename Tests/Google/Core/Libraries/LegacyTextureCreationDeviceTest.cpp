// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "WW3D2/LegacyTextureCreation.h"
#include <d3dx8tex.h>
#include <vector>
#include <array>
#include <stdexcept>

namespace {
template<class T> struct COM {
    T *p=nullptr;
    ~COM() { if(p)p->Release(); }
};
template<class T> std::vector<UINT> Properties(T *resource) {
    std::vector<UINT> result{UINT(resource->GetType()),resource->GetLevelCount()};
    for(UINT n=0;n<resource->GetLevelCount();++n) {
        D3DSURFACE_DESC desc{};
        if(FAILED(resource->GetLevelDesc(n,&desc)))throw std::runtime_error("GetLevelDesc failed");
        result.insert(result.end(),{UINT(desc.Format),UINT(desc.Type),desc.Usage,UINT(desc.Pool),
            desc.Size,UINT(desc.MultiSampleType),desc.Width,desc.Height});
    }
    return result;
}
template<> std::vector<UINT> Properties(IDirect3DVolumeTexture8 *resource) {
    std::vector<UINT> result{UINT(resource->GetType()),resource->GetLevelCount()};
    for(UINT n=0;n<resource->GetLevelCount();++n) {
        D3DVOLUME_DESC desc{};
        if(FAILED(resource->GetLevelDesc(n,&desc)))throw std::runtime_error("GetVolumeLevelDesc failed");
        result.insert(result.end(),{UINT(desc.Format),UINT(desc.Type),desc.Usage,UINT(desc.Pool),
            desc.Size,desc.Width,desc.Height,desc.Depth});
    }
    return result;
}

TEST(LegacyTextureCreationDevice, SystemWin32DX8PropertiesMatchPinnedLibrary) {
    struct Module { HMODULE h=LoadLibraryExW(L"d3d8.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        ~Module(){if(h)FreeLibrary(h);} } module;
    if(!module.h)GTEST_SKIP() << "System Win32 d3d8.dll unavailable, error=" << GetLastError();
    auto create=reinterpret_cast<IDirect3D8 *(WINAPI *)(UINT)>(GetProcAddress(module.h,"Direct3DCreate8"));
    if(!create)GTEST_SKIP() << "Direct3DCreate8 unavailable";
    COM<IDirect3D8> d3d; d3d.p=create(D3D_SDK_VERSION);
    if(!d3d.p)GTEST_SKIP() << "Direct3DCreate8 returned null";
    struct Window { HWND h=CreateWindowExW(0,L"STATIC",L"DX8 resource validation",WS_OVERLAPPEDWINDOW,
        0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        ~Window(){if(h)DestroyWindow(h);} } window;
    ASSERT_NE(window.h,nullptr);
    D3DDISPLAYMODE mode{};
    ASSERT_EQ(d3d.p->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,&mode),S_OK);
    D3DPRESENT_PARAMETERS parameters{};
    parameters.BackBufferWidth=64; parameters.BackBufferHeight=64; parameters.BackBufferFormat=mode.Format;
    parameters.BackBufferCount=1; parameters.SwapEffect=D3DSWAPEFFECT_DISCARD;
    parameters.hDeviceWindow=window.h; parameters.Windowed=TRUE;
    COM<IDirect3DDevice8> device;
    const HRESULT available=d3d.p->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window.h,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,&parameters,&device.p);
    if(FAILED(available))GTEST_SKIP() << "Real Win32 DX8 HAL device unavailable, HRESULT=" << std::hex << available;
    unsigned successes[3]{};
    for(auto size : {std::array<UINT,3>{63,31,7},{16,16,4},{1,1,1}})
        for(UINT levels : {0u,1u,3u})
            for(auto format : {D3DFMT_A8R8G8B8,D3DFMT_R5G6B5,D3DFMT_DXT1})
                for(auto pool : {D3DPOOL_DEFAULT,D3DPOOL_MANAGED,D3DPOOL_SYSTEMMEM})
                    for(DWORD usage : {DWORD(0),DWORD(D3DUSAGE_RENDERTARGET)}) {
                        auto compare=[&](auto &old,auto &current,HRESULT before,HRESULT after,unsigned kind) {
                            EXPECT_EQ(before,after);
                            if(SUCCEEDED(before) && SUCCEEDED(after)) {
                                ASSERT_NE(old.p,nullptr); ASSERT_NE(current.p,nullptr);
                                EXPECT_EQ(Properties(old.p),Properties(current.p)); ++successes[kind];
                            } else { EXPECT_EQ(old.p,nullptr); EXPECT_EQ(current.p,nullptr); }
                        };
                        COM<IDirect3DTexture8> oldTexture,newTexture;
                        auto a=D3DXCreateTexture(device.p,size[0],size[1],levels,usage,format,pool,&oldTexture.p);
                        auto b=LegacyD3DX::CreateTexture(device.p,size[0],size[1],levels,usage,format,pool,&newTexture.p);
                        compare(oldTexture,newTexture,a,b,0);
                        COM<IDirect3DCubeTexture8> oldCube,newCube;
                        a=D3DXCreateCubeTexture(device.p,size[0],levels,usage,format,pool,&oldCube.p);
                        b=LegacyD3DX::CreateCubeTexture(device.p,size[0],levels,usage,format,pool,&newCube.p);
                        compare(oldCube,newCube,a,b,1);
                        COM<IDirect3DVolumeTexture8> oldVolume,newVolume;
                        a=D3DXCreateVolumeTexture(device.p,size[0],size[1],size[2],levels,usage,format,pool,&oldVolume.p);
                        b=LegacyD3DX::CreateVolumeTexture(device.p,size[0],size[1],size[2],levels,usage,format,pool,&newVolume.p);
                        compare(oldVolume,newVolume,a,b,2);
                    }
    std::printf("Real DX8 successful matched creations: 2D=%u cube=%u volume=%u\n",successes[0],successes[1],successes[2]);
    EXPECT_GT(successes[0],0u);
    RecordProperty("matched_2d",successes[0]); RecordProperty("matched_cube",successes[1]); RecordProperty("matched_volume",successes[2]);
}
}
