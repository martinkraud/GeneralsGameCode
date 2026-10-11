// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "LegacyTextureCreationStubs.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace TextureCreationCases {
using Event = std::array<std::uint32_t,8>;
enum Op { GetD3D = 1, AddD3D, ReleaseD3D, GetCaps, GetMode, CheckFormat, Create };
struct Input {
    D3DRESOURCETYPE type = D3DRTYPE_TEXTURE;
    UINT width = 63, height = 31, depth = 7, levels = 0;
    DWORD usage = 0;
    D3DFORMAT format = D3DFMT_A8R8G8B8;
    D3DPOOL pool = D3DPOOL_MANAGED;
    D3DCAPS8 caps = [] { D3DCAPS8 c{}; c.AdapterOrdinal = 2; c.DeviceType = D3DDEVTYPE_HAL;
        c.TextureCaps = D3DPTEXTURECAPS_MIPMAP | D3DPTEXTURECAPS_MIPCUBEMAP |
            D3DPTEXTURECAPS_MIPVOLUMEMAP | D3DPTEXTURECAPS_CUBEMAP | D3DPTEXTURECAPS_VOLUMEMAP;
        c.MaxTextureWidth = 1024; c.MaxTextureHeight = 512; c.MaxVolumeExtent = 256; return c; }();
    std::vector<D3DFORMAT> supported; // Empty means reject all, never permissive by default.
    HRESULT createResult = S_OK;
    bool clearOnFailure = false, nullDevice = false, nullOutput = false;
};

template<class Base> struct Resource : Base {
    ULONG refs = 0, adds = 0, releases = 0;
    ULONG STDMETHODCALLTYPE AddRef() override { ++adds; return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { ++releases; return --refs; }
};
struct RecordingD3D : IDirect3D8Stub {
    Input &input;
    std::vector<Event> &events;
    ULONG refs = 1;
    RecordingD3D(Input &i, std::vector<Event> &e) : input(i), events(e) {}
    ULONG STDMETHODCALLTYPE AddRef() override { events.push_back({AddD3D}); return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { events.push_back({ReleaseD3D}); return --refs; }
    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT adapter, D3DDEVTYPE device, D3DFORMAT display,
        DWORD usage, D3DRESOURCETYPE type, D3DFORMAT format) override {
        events.push_back({CheckFormat,adapter,UINT(device),UINT(display),usage,UINT(type),UINT(format)});
        return std::find(input.supported.begin(),input.supported.end(),format) != input.supported.end()
            ? S_OK : D3DERR_NOTAVAILABLE;
    }
};
struct RecordingDevice : IDirect3DDevice8Stub {
    Input input;
    std::vector<Event> events;
    RecordingD3D d3d;
    Resource<IDirect3DTexture8Stub> texture, initialTexture;
    Resource<IDirect3DCubeTexture8Stub> cube, initialCube;
    Resource<IDirect3DVolumeTexture8Stub> volume, initialVolume;
    explicit RecordingDevice(const Input &i) : input(i), d3d(input,events) {}
    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D8 **out) override {
        events.push_back({GetD3D}); *out = &d3d; d3d.AddRef(); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS8 *out) override {
        events.push_back({GetCaps}); *out = input.caps; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE *out) override {
        events.push_back({GetMode}); *out = {800,600,60,D3DFMT_X8R8G8B8}; return S_OK;
    }
    template<class Interface, class Base> HRESULT Finish(Interface **out, Resource<Base> &resource) {
        if (SUCCEEDED(input.createResult)) { resource.refs = 1; *out = &resource; }
        else if (input.clearOnFailure) *out = nullptr;
        return input.createResult;
    }
    HRESULT STDMETHODCALLTYPE CreateTexture(UINT w, UINT h, UINT levels, DWORD usage,
        D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8 **out) override {
        events.push_back({Create,w,h,1,levels,usage,UINT(format),UINT(pool)});
        return Finish(out,texture);
    }
    HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT edge, UINT levels, DWORD usage,
        D3DFORMAT format, D3DPOOL pool, IDirect3DCubeTexture8 **out) override {
        events.push_back({Create,edge,edge,1,levels,usage,UINT(format),UINT(pool)});
        return Finish(out,cube);
    }
    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT w, UINT h, UINT d, UINT levels, DWORD usage,
        D3DFORMAT format, D3DPOOL pool, IDirect3DVolumeTexture8 **out) override {
        events.push_back({Create,w,h,d,levels,usage,UINT(format),UINT(pool)});
        return Finish(out,volume);
    }
};
struct Result {
    HRESULT hr;
    UINT output; // 0=null, 1=unchanged caller object, 2=new resource.
    std::vector<Event> events;
    std::array<ULONG,7> ownership; // D3D refs; new refs/adds/releases; caller object's refs/adds/releases.
    bool operator==(const Result &r) const {
        return hr==r.hr && output==r.output && events==r.events && ownership==r.ownership;
    }
};
template<class API> Result Run(const Input &i) {
    RecordingDevice d(i);
    auto *device = i.nullDevice ? nullptr : &d;
    HRESULT hr; UINT output; std::array<ULONG,7> ownership;
    // Typed outputs let unexpected resource calls fail through real test vtables.
    if (i.type == D3DRTYPE_TEXTURE) {
        IDirect3DTexture8 *out = &d.initialTexture;
        hr = API::Texture(device,i.width,i.height,i.levels,i.usage,i.format,i.pool,i.nullOutput ? nullptr : &out);
        output = out == nullptr ? 0 : out == &d.initialTexture ? 1 : out == &d.texture ? 2 : 99;
        ownership = {d.d3d.refs,d.texture.refs,d.texture.adds,d.texture.releases,
            d.initialTexture.refs,d.initialTexture.adds,d.initialTexture.releases};
        if (output == 2 && out->Release()!=0)Unexpected("caller texture release imbalance");
    } else if (i.type == D3DRTYPE_CUBETEXTURE) {
        IDirect3DCubeTexture8 *out = &d.initialCube;
        hr = API::Cube(device,i.width,i.levels,i.usage,i.format,i.pool,i.nullOutput ? nullptr : &out);
        output = out == nullptr ? 0 : out == &d.initialCube ? 1 : out == &d.cube ? 2 : 99;
        ownership = {d.d3d.refs,d.cube.refs,d.cube.adds,d.cube.releases,
            d.initialCube.refs,d.initialCube.adds,d.initialCube.releases};
        if (output == 2 && out->Release()!=0)Unexpected("caller cube release imbalance");
    } else {
        IDirect3DVolumeTexture8 *out = &d.initialVolume;
        hr = API::Volume(device,i.width,i.height,i.depth,i.levels,i.usage,i.format,i.pool,i.nullOutput ? nullptr : &out);
        output = out == nullptr ? 0 : out == &d.initialVolume ? 1 : out == &d.volume ? 2 : 99;
        ownership = {d.d3d.refs,d.volume.refs,d.volume.adds,d.volume.releases,
            d.initialVolume.refs,d.initialVolume.adds,d.initialVolume.releases};
        if (output == 2 && out->Release()!=0)Unexpected("caller volume release imbalance");
    }
    return {hr,output,d.events,ownership};
}
}
