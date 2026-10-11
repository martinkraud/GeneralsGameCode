// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
namespace TextureCreationCases {
[[noreturn]] inline void Unexpected(const char *method) { std::fprintf(stderr,"Unexpected COM call: %s\n",method); std::abort(); }
struct IDirect3D8Stub : IDirect3D8 {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override { Unexpected("IDirect3D8::QueryInterface"); }
    ULONG STDMETHODCALLTYPE AddRef() override { Unexpected("IDirect3D8::AddRef"); }
    ULONG STDMETHODCALLTYPE Release() override { Unexpected("IDirect3D8::Release"); }
    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void* pInitializeFunction) override { Unexpected("IDirect3D8::RegisterSoftwareDevice"); }
    UINT STDMETHODCALLTYPE GetAdapterCount() override { Unexpected("IDirect3D8::GetAdapterCount"); }
    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT Adapter,DWORD Flags,D3DADAPTER_IDENTIFIER8* pIdentifier) override { Unexpected("IDirect3D8::GetAdapterIdentifier"); }
    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT Adapter) override { Unexpected("IDirect3D8::GetAdapterModeCount"); }
    HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT Adapter,UINT Mode,D3DDISPLAYMODE* pMode) override { Unexpected("IDirect3D8::EnumAdapterModes"); }
    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT Adapter,D3DDISPLAYMODE* pMode) override { Unexpected("IDirect3D8::GetAdapterDisplayMode"); }
    HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT Adapter,D3DDEVTYPE CheckType,D3DFORMAT DisplayFormat,D3DFORMAT BackBufferFormat,BOOL Windowed) override { Unexpected("IDirect3D8::CheckDeviceType"); }
    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,DWORD Usage,D3DRESOURCETYPE RType,D3DFORMAT CheckFormat) override { Unexpected("IDirect3D8::CheckDeviceFormat"); }
    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT SurfaceFormat,BOOL Windowed,D3DMULTISAMPLE_TYPE MultiSampleType) override { Unexpected("IDirect3D8::CheckDeviceMultiSampleType"); }
    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,D3DFORMAT RenderTargetFormat,D3DFORMAT DepthStencilFormat) override { Unexpected("IDirect3D8::CheckDepthStencilMatch"); }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT Adapter,D3DDEVTYPE DeviceType,D3DCAPS8* pCaps) override { Unexpected("IDirect3D8::GetDeviceCaps"); }
    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT Adapter) override { Unexpected("IDirect3D8::GetAdapterMonitor"); }
    HRESULT STDMETHODCALLTYPE CreateDevice(UINT Adapter,D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DDevice8** ppReturnedDeviceInterface) override { Unexpected("IDirect3D8::CreateDevice"); }
};
struct IDirect3DDevice8Stub : IDirect3DDevice8 {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override { Unexpected("IDirect3DDevice8::QueryInterface"); }
    ULONG STDMETHODCALLTYPE AddRef() override { Unexpected("IDirect3DDevice8::AddRef"); }
    ULONG STDMETHODCALLTYPE Release() override { Unexpected("IDirect3DDevice8::Release"); }
    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override { Unexpected("IDirect3DDevice8::TestCooperativeLevel"); }
    UINT STDMETHODCALLTYPE GetAvailableTextureMem() override { Unexpected("IDirect3DDevice8::GetAvailableTextureMem"); }
    HRESULT STDMETHODCALLTYPE ResourceManagerDiscardBytes(DWORD Bytes) override { Unexpected("IDirect3DDevice8::ResourceManagerDiscardBytes"); }
    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D8** ppD3D8) override { Unexpected("IDirect3DDevice8::GetDirect3D"); }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS8* pCaps) override { Unexpected("IDirect3DDevice8::GetDeviceCaps"); }
    HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE* pMode) override { Unexpected("IDirect3DDevice8::GetDisplayMode"); }
    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) override { Unexpected("IDirect3DDevice8::GetCreationParameters"); }
    HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT XHotSpot,UINT YHotSpot,IDirect3DSurface8* pCursorBitmap) override { Unexpected("IDirect3DDevice8::SetCursorProperties"); }
    void STDMETHODCALLTYPE SetCursorPosition(UINT XScreenSpace,UINT YScreenSpace,DWORD Flags) override { Unexpected("IDirect3DDevice8::SetCursorPosition"); }
    BOOL STDMETHODCALLTYPE ShowCursor(BOOL bShow) override { Unexpected("IDirect3DDevice8::ShowCursor"); }
    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DSwapChain8** pSwapChain) override { Unexpected("IDirect3DDevice8::CreateAdditionalSwapChain"); }
    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) override { Unexpected("IDirect3DDevice8::Reset"); }
    HRESULT STDMETHODCALLTYPE Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion) override { Unexpected("IDirect3DDevice8::Present"); }
    HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT BackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface8** ppBackBuffer) override { Unexpected("IDirect3DDevice8::GetBackBuffer"); }
    HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS* pRasterStatus) override { Unexpected("IDirect3DDevice8::GetRasterStatus"); }
    void STDMETHODCALLTYPE SetGammaRamp(DWORD Flags,CONST D3DGAMMARAMP* pRamp) override { Unexpected("IDirect3DDevice8::SetGammaRamp"); }
    void STDMETHODCALLTYPE GetGammaRamp(D3DGAMMARAMP* pRamp) override { Unexpected("IDirect3DDevice8::GetGammaRamp"); }
    HRESULT STDMETHODCALLTYPE CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture8** ppTexture) override { Unexpected("IDirect3DDevice8::CreateTexture"); }
    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture8** ppVolumeTexture) override { Unexpected("IDirect3DDevice8::CreateVolumeTexture"); }
    HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture8** ppCubeTexture) override { Unexpected("IDirect3DDevice8::CreateCubeTexture"); }
    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer8** ppVertexBuffer) override { Unexpected("IDirect3DDevice8::CreateVertexBuffer"); }
    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer8** ppIndexBuffer) override { Unexpected("IDirect3DDevice8::CreateIndexBuffer"); }
    HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,BOOL Lockable,IDirect3DSurface8** ppSurface) override { Unexpected("IDirect3DDevice8::CreateRenderTarget"); }
    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,IDirect3DSurface8** ppSurface) override { Unexpected("IDirect3DDevice8::CreateDepthStencilSurface"); }
    HRESULT STDMETHODCALLTYPE CreateImageSurface(UINT Width,UINT Height,D3DFORMAT Format,IDirect3DSurface8** ppSurface) override { Unexpected("IDirect3DDevice8::CreateImageSurface"); }
    HRESULT STDMETHODCALLTYPE CopyRects(IDirect3DSurface8* pSourceSurface,CONST RECT* pSourceRectsArray,UINT cRects,IDirect3DSurface8* pDestinationSurface,CONST POINT* pDestPointsArray) override { Unexpected("IDirect3DDevice8::CopyRects"); }
    HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture8* pSourceTexture,IDirect3DBaseTexture8* pDestinationTexture) override { Unexpected("IDirect3DDevice8::UpdateTexture"); }
    HRESULT STDMETHODCALLTYPE GetFrontBuffer(IDirect3DSurface8* pDestSurface) override { Unexpected("IDirect3DDevice8::GetFrontBuffer"); }
    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirect3DSurface8* pRenderTarget,IDirect3DSurface8* pNewZStencil) override { Unexpected("IDirect3DDevice8::SetRenderTarget"); }
    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirect3DSurface8** ppRenderTarget) override { Unexpected("IDirect3DDevice8::GetRenderTarget"); }
    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface8** ppZStencilSurface) override { Unexpected("IDirect3DDevice8::GetDepthStencilSurface"); }
    HRESULT STDMETHODCALLTYPE BeginScene() override { Unexpected("IDirect3DDevice8::BeginScene"); }
    HRESULT STDMETHODCALLTYPE EndScene() override { Unexpected("IDirect3DDevice8::EndScene"); }
    HRESULT STDMETHODCALLTYPE Clear(DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil) override { Unexpected("IDirect3DDevice8::Clear"); }
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix) override { Unexpected("IDirect3DDevice8::SetTransform"); }
    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE State,D3DMATRIX* pMatrix) override { Unexpected("IDirect3DDevice8::GetTransform"); }
    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE,CONST D3DMATRIX*) override { Unexpected("IDirect3DDevice8::MultiplyTransform"); }
    HRESULT STDMETHODCALLTYPE SetViewport(CONST D3DVIEWPORT8* pViewport) override { Unexpected("IDirect3DDevice8::SetViewport"); }
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT8* pViewport) override { Unexpected("IDirect3DDevice8::GetViewport"); }
    HRESULT STDMETHODCALLTYPE SetMaterial(CONST D3DMATERIAL8* pMaterial) override { Unexpected("IDirect3DDevice8::SetMaterial"); }
    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL8* pMaterial) override { Unexpected("IDirect3DDevice8::GetMaterial"); }
    HRESULT STDMETHODCALLTYPE SetLight(DWORD Index,CONST D3DLIGHT8*) override { Unexpected("IDirect3DDevice8::SetLight"); }
    HRESULT STDMETHODCALLTYPE GetLight(DWORD Index,D3DLIGHT8*) override { Unexpected("IDirect3DDevice8::GetLight"); }
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index,BOOL Enable) override { Unexpected("IDirect3DDevice8::LightEnable"); }
    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD Index,BOOL* pEnable) override { Unexpected("IDirect3DDevice8::GetLightEnable"); }
    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD Index,CONST float* pPlane) override { Unexpected("IDirect3DDevice8::SetClipPlane"); }
    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD Index,float* pPlane) override { Unexpected("IDirect3DDevice8::GetClipPlane"); }
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State,DWORD Value) override { Unexpected("IDirect3DDevice8::SetRenderState"); }
    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State,DWORD* pValue) override { Unexpected("IDirect3DDevice8::GetRenderState"); }
    HRESULT STDMETHODCALLTYPE BeginStateBlock() override { Unexpected("IDirect3DDevice8::BeginStateBlock"); }
    HRESULT STDMETHODCALLTYPE EndStateBlock(DWORD* pToken) override { Unexpected("IDirect3DDevice8::EndStateBlock"); }
    HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD Token) override { Unexpected("IDirect3DDevice8::ApplyStateBlock"); }
    HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD Token) override { Unexpected("IDirect3DDevice8::CaptureStateBlock"); }
    HRESULT STDMETHODCALLTYPE DeleteStateBlock(DWORD Token) override { Unexpected("IDirect3DDevice8::DeleteStateBlock"); }
    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE Type,DWORD* pToken) override { Unexpected("IDirect3DDevice8::CreateStateBlock"); }
    HRESULT STDMETHODCALLTYPE SetClipStatus(CONST D3DCLIPSTATUS8* pClipStatus) override { Unexpected("IDirect3DDevice8::SetClipStatus"); }
    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS8* pClipStatus) override { Unexpected("IDirect3DDevice8::GetClipStatus"); }
    HRESULT STDMETHODCALLTYPE GetTexture(DWORD Stage,IDirect3DBaseTexture8** ppTexture) override { Unexpected("IDirect3DDevice8::GetTexture"); }
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD Stage,IDirect3DBaseTexture8* pTexture) override { Unexpected("IDirect3DDevice8::SetTexture"); }
    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue) override { Unexpected("IDirect3DDevice8::GetTextureStageState"); }
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value) override { Unexpected("IDirect3DDevice8::SetTextureStageState"); }
    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* pNumPasses) override { Unexpected("IDirect3DDevice8::ValidateDevice"); }
    HRESULT STDMETHODCALLTYPE GetInfo(DWORD DevInfoID,void* pDevInfoStruct,DWORD DevInfoStructSize) override { Unexpected("IDirect3DDevice8::GetInfo"); }
    HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT PaletteNumber,CONST PALETTEENTRY* pEntries) override { Unexpected("IDirect3DDevice8::SetPaletteEntries"); }
    HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT PaletteNumber,PALETTEENTRY* pEntries) override { Unexpected("IDirect3DDevice8::GetPaletteEntries"); }
    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT PaletteNumber) override { Unexpected("IDirect3DDevice8::SetCurrentTexturePalette"); }
    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT *PaletteNumber) override { Unexpected("IDirect3DDevice8::GetCurrentTexturePalette"); }
    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount) override { Unexpected("IDirect3DDevice8::DrawPrimitive"); }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE,UINT minIndex,UINT NumVertices,UINT startIndex,UINT primCount) override { Unexpected("IDirect3DDevice8::DrawIndexedPrimitive"); }
    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride) override { Unexpected("IDirect3DDevice8::DrawPrimitiveUP"); }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertexIndices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride) override { Unexpected("IDirect3DDevice8::DrawIndexedPrimitiveUP"); }
    HRESULT STDMETHODCALLTYPE ProcessVertices(UINT SrcStartIndex,UINT DestIndex,UINT VertexCount,IDirect3DVertexBuffer8* pDestBuffer,DWORD Flags) override { Unexpected("IDirect3DDevice8::ProcessVertices"); }
    HRESULT STDMETHODCALLTYPE CreateVertexShader(CONST DWORD* pDeclaration,CONST DWORD* pFunction,DWORD* pHandle,DWORD Usage) override { Unexpected("IDirect3DDevice8::CreateVertexShader"); }
    HRESULT STDMETHODCALLTYPE SetVertexShader(DWORD Handle) override { Unexpected("IDirect3DDevice8::SetVertexShader"); }
    HRESULT STDMETHODCALLTYPE GetVertexShader(DWORD* pHandle) override { Unexpected("IDirect3DDevice8::GetVertexShader"); }
    HRESULT STDMETHODCALLTYPE DeleteVertexShader(DWORD Handle) override { Unexpected("IDirect3DDevice8::DeleteVertexShader"); }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstant(DWORD Register,CONST void* pConstantData,DWORD ConstantCount) override { Unexpected("IDirect3DDevice8::SetVertexShaderConstant"); }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstant(DWORD Register,void* pConstantData,DWORD ConstantCount) override { Unexpected("IDirect3DDevice8::GetVertexShaderConstant"); }
    HRESULT STDMETHODCALLTYPE GetVertexShaderDeclaration(DWORD Handle,void* pData,DWORD* pSizeOfData) override { Unexpected("IDirect3DDevice8::GetVertexShaderDeclaration"); }
    HRESULT STDMETHODCALLTYPE GetVertexShaderFunction(DWORD Handle,void* pData,DWORD* pSizeOfData) override { Unexpected("IDirect3DDevice8::GetVertexShaderFunction"); }
    HRESULT STDMETHODCALLTYPE SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer8* pStreamData,UINT Stride) override { Unexpected("IDirect3DDevice8::SetStreamSource"); }
    HRESULT STDMETHODCALLTYPE GetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer8** ppStreamData,UINT* pStride) override { Unexpected("IDirect3DDevice8::GetStreamSource"); }
    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer8* pIndexData,UINT BaseVertexIndex) override { Unexpected("IDirect3DDevice8::SetIndices"); }
    HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer8** ppIndexData,UINT* pBaseVertexIndex) override { Unexpected("IDirect3DDevice8::GetIndices"); }
    HRESULT STDMETHODCALLTYPE CreatePixelShader(CONST DWORD* pFunction,DWORD* pHandle) override { Unexpected("IDirect3DDevice8::CreatePixelShader"); }
    HRESULT STDMETHODCALLTYPE SetPixelShader(DWORD Handle) override { Unexpected("IDirect3DDevice8::SetPixelShader"); }
    HRESULT STDMETHODCALLTYPE GetPixelShader(DWORD* pHandle) override { Unexpected("IDirect3DDevice8::GetPixelShader"); }
    HRESULT STDMETHODCALLTYPE DeletePixelShader(DWORD Handle) override { Unexpected("IDirect3DDevice8::DeletePixelShader"); }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstant(DWORD Register,CONST void* pConstantData,DWORD ConstantCount) override { Unexpected("IDirect3DDevice8::SetPixelShaderConstant"); }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstant(DWORD Register,void* pConstantData,DWORD ConstantCount) override { Unexpected("IDirect3DDevice8::GetPixelShaderConstant"); }
    HRESULT STDMETHODCALLTYPE GetPixelShaderFunction(DWORD Handle,void* pData,DWORD* pSizeOfData) override { Unexpected("IDirect3DDevice8::GetPixelShaderFunction"); }
    HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT Handle,CONST float* pNumSegs,CONST D3DRECTPATCH_INFO* pRectPatchInfo) override { Unexpected("IDirect3DDevice8::DrawRectPatch"); }
    HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT Handle,CONST float* pNumSegs,CONST D3DTRIPATCH_INFO* pTriPatchInfo) override { Unexpected("IDirect3DDevice8::DrawTriPatch"); }
    HRESULT STDMETHODCALLTYPE DeletePatch(UINT Handle) override { Unexpected("IDirect3DDevice8::DeletePatch"); }
};
struct IDirect3DTexture8Stub : IDirect3DTexture8 {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override { Unexpected("IDirect3DTexture8::QueryInterface"); }
    ULONG STDMETHODCALLTYPE AddRef() override { Unexpected("IDirect3DTexture8::AddRef"); }
    ULONG STDMETHODCALLTYPE Release() override { Unexpected("IDirect3DTexture8::Release"); }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** ppDevice) override { Unexpected("IDirect3DTexture8::GetDevice"); }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags) override { Unexpected("IDirect3DTexture8::SetPrivateData"); }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid,void* pData,DWORD* pSizeOfData) override { Unexpected("IDirect3DTexture8::GetPrivateData"); }
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override { Unexpected("IDirect3DTexture8::FreePrivateData"); }
    DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override { Unexpected("IDirect3DTexture8::SetPriority"); }
    DWORD STDMETHODCALLTYPE GetPriority() override { Unexpected("IDirect3DTexture8::GetPriority"); }
    void STDMETHODCALLTYPE PreLoad() override { Unexpected("IDirect3DTexture8::PreLoad"); }
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { Unexpected("IDirect3DTexture8::GetType"); }
    DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) override { Unexpected("IDirect3DTexture8::SetLOD"); }
    DWORD STDMETHODCALLTYPE GetLOD() override { Unexpected("IDirect3DTexture8::GetLOD"); }
    DWORD STDMETHODCALLTYPE GetLevelCount() override { Unexpected("IDirect3DTexture8::GetLevelCount"); }
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc) override { Unexpected("IDirect3DTexture8::GetLevelDesc"); }
    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level,IDirect3DSurface8** ppSurfaceLevel) override { Unexpected("IDirect3DTexture8::GetSurfaceLevel"); }
    HRESULT STDMETHODCALLTYPE LockRect(UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags) override { Unexpected("IDirect3DTexture8::LockRect"); }
    HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level) override { Unexpected("IDirect3DTexture8::UnlockRect"); }
    HRESULT STDMETHODCALLTYPE AddDirtyRect(CONST RECT* pDirtyRect) override { Unexpected("IDirect3DTexture8::AddDirtyRect"); }
};
struct IDirect3DCubeTexture8Stub : IDirect3DCubeTexture8 {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override { Unexpected("IDirect3DCubeTexture8::QueryInterface"); }
    ULONG STDMETHODCALLTYPE AddRef() override { Unexpected("IDirect3DCubeTexture8::AddRef"); }
    ULONG STDMETHODCALLTYPE Release() override { Unexpected("IDirect3DCubeTexture8::Release"); }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** ppDevice) override { Unexpected("IDirect3DCubeTexture8::GetDevice"); }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags) override { Unexpected("IDirect3DCubeTexture8::SetPrivateData"); }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid,void* pData,DWORD* pSizeOfData) override { Unexpected("IDirect3DCubeTexture8::GetPrivateData"); }
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override { Unexpected("IDirect3DCubeTexture8::FreePrivateData"); }
    DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override { Unexpected("IDirect3DCubeTexture8::SetPriority"); }
    DWORD STDMETHODCALLTYPE GetPriority() override { Unexpected("IDirect3DCubeTexture8::GetPriority"); }
    void STDMETHODCALLTYPE PreLoad() override { Unexpected("IDirect3DCubeTexture8::PreLoad"); }
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { Unexpected("IDirect3DCubeTexture8::GetType"); }
    DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) override { Unexpected("IDirect3DCubeTexture8::SetLOD"); }
    DWORD STDMETHODCALLTYPE GetLOD() override { Unexpected("IDirect3DCubeTexture8::GetLOD"); }
    DWORD STDMETHODCALLTYPE GetLevelCount() override { Unexpected("IDirect3DCubeTexture8::GetLevelCount"); }
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc) override { Unexpected("IDirect3DCubeTexture8::GetLevelDesc"); }
    HRESULT STDMETHODCALLTYPE GetCubeMapSurface(D3DCUBEMAP_FACES FaceType,UINT Level,IDirect3DSurface8** ppCubeMapSurface) override { Unexpected("IDirect3DCubeTexture8::GetCubeMapSurface"); }
    HRESULT STDMETHODCALLTYPE LockRect(D3DCUBEMAP_FACES FaceType,UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags) override { Unexpected("IDirect3DCubeTexture8::LockRect"); }
    HRESULT STDMETHODCALLTYPE UnlockRect(D3DCUBEMAP_FACES FaceType,UINT Level) override { Unexpected("IDirect3DCubeTexture8::UnlockRect"); }
    HRESULT STDMETHODCALLTYPE AddDirtyRect(D3DCUBEMAP_FACES FaceType,CONST RECT* pDirtyRect) override { Unexpected("IDirect3DCubeTexture8::AddDirtyRect"); }
};
struct IDirect3DVolumeTexture8Stub : IDirect3DVolumeTexture8 {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override { Unexpected("IDirect3DVolumeTexture8::QueryInterface"); }
    ULONG STDMETHODCALLTYPE AddRef() override { Unexpected("IDirect3DVolumeTexture8::AddRef"); }
    ULONG STDMETHODCALLTYPE Release() override { Unexpected("IDirect3DVolumeTexture8::Release"); }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** ppDevice) override { Unexpected("IDirect3DVolumeTexture8::GetDevice"); }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags) override { Unexpected("IDirect3DVolumeTexture8::SetPrivateData"); }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid,void* pData,DWORD* pSizeOfData) override { Unexpected("IDirect3DVolumeTexture8::GetPrivateData"); }
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override { Unexpected("IDirect3DVolumeTexture8::FreePrivateData"); }
    DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override { Unexpected("IDirect3DVolumeTexture8::SetPriority"); }
    DWORD STDMETHODCALLTYPE GetPriority() override { Unexpected("IDirect3DVolumeTexture8::GetPriority"); }
    void STDMETHODCALLTYPE PreLoad() override { Unexpected("IDirect3DVolumeTexture8::PreLoad"); }
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { Unexpected("IDirect3DVolumeTexture8::GetType"); }
    DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) override { Unexpected("IDirect3DVolumeTexture8::SetLOD"); }
    DWORD STDMETHODCALLTYPE GetLOD() override { Unexpected("IDirect3DVolumeTexture8::GetLOD"); }
    DWORD STDMETHODCALLTYPE GetLevelCount() override { Unexpected("IDirect3DVolumeTexture8::GetLevelCount"); }
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level,D3DVOLUME_DESC *pDesc) override { Unexpected("IDirect3DVolumeTexture8::GetLevelDesc"); }
    HRESULT STDMETHODCALLTYPE GetVolumeLevel(UINT Level,IDirect3DVolume8** ppVolumeLevel) override { Unexpected("IDirect3DVolumeTexture8::GetVolumeLevel"); }
    HRESULT STDMETHODCALLTYPE LockBox(UINT Level,D3DLOCKED_BOX* pLockedVolume,CONST D3DBOX* pBox,DWORD Flags) override { Unexpected("IDirect3DVolumeTexture8::LockBox"); }
    HRESULT STDMETHODCALLTYPE UnlockBox(UINT Level) override { Unexpected("IDirect3DVolumeTexture8::UnlockBox"); }
    HRESULT STDMETHODCALLTYPE AddDirtyBox(CONST D3DBOX* pDirtyBox) override { Unexpected("IDirect3DVolumeTexture8::AddDirtyBox"); }
};
}
