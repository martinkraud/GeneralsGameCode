// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include <d3dx8.h>
#include "WW3D2/LegacyFixedPixelShader.h"
namespace {
template<class T> struct COM { T *p=nullptr; ~COM(){if(p)p->Release();} };
TEST(LegacyFixedPixelShaderDevice, RealDX8CreationMatchesPinnedAssembler) {
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

    D3DCAPS8 caps{};
    ASSERT_EQ(device.p->GetDeviceCaps(&caps),S_OK);
    if(caps.PixelShaderVersion<D3DPS_VERSION(1,4))
        GTEST_SKIP() << "Real adapter pixel shader version=" << std::hex << caps.PixelShaderVersion << "; ps.1.4 required";
    using namespace LegacyD3DX::FixedPixelShader;
    const char *sources[]={RiverWaterSource,WaterSource,TrapezoidWaterSource,ProfilerSwizzleSource};
    const DWORD *tokens[]={RiverWaterCode,WaterCode,TrapezoidWaterCode,ProfilerSwizzleCode};
    for(unsigned i=0;i<4;++i) {
        COM<ID3DXBuffer> assembled,errors;
        ASSERT_EQ(D3DXAssembleShader(sources[i],UINT(strlen(sources[i])),0,nullptr,&assembled.p,&errors.p),S_OK);
        ASSERT_NE(assembled.p,nullptr);
        DWORD oldHandle=0,newHandle=0;
        const HRESULT a=device.p->CreatePixelShader(static_cast<const DWORD*>(assembled.p->GetBufferPointer()),&oldHandle);
        const HRESULT b=device.p->CreatePixelShader(tokens[i],&newHandle);
        EXPECT_EQ(a,b) << "program=" << i;
        if(SUCCEEDED(a)) { EXPECT_NE(oldHandle,0u); EXPECT_EQ(device.p->DeletePixelShader(oldHandle),S_OK); }
        if(SUCCEEDED(b)) { EXPECT_NE(newHandle,0u); EXPECT_EQ(device.p->DeletePixelShader(newHandle),S_OK); }
        EXPECT_EQ(a,S_OK) << "Supported fixed shader failed real device creation: program=" << i;
        std::printf("Real DX8 fixed shader %u: legacy=%08lx pinned=%08lx\n",i,a,b);
    }
}
}
