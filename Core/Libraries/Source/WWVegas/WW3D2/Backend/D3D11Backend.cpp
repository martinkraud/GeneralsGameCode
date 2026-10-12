// SPDX-License-Identifier: GPL-3.0-or-later
#include "D3D11Backend.h"
#include "WW3D2/Render2DSubmission.h"
#include "WW3D2/Texture2DCPUContent.h"
#include "WW3D2/shader.h"
#include "WWMath/vector3.h"
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <map>
#include <cstdio>
#include <atomic>

using Microsoft::WRL::ComPtr;
namespace {
void Check(HRESULT result, const char *operation)
{
    if (FAILED(result)) {
        char hex[16]; std::snprintf(hex, sizeof(hex), "0x%08X", static_cast<unsigned>(result));
        throw std::runtime_error(std::string(operation) + " HRESULT=" + hex);
    }
}
[[noreturn]] void Unsupported() { throw std::runtime_error("Unsupported D3D11.1 renderer state/operation"); }
Texture2DSampler SentenceSampler()
{
    using F = TextureFilterClass;
    return {F::FILTER_TYPE_NONE, F::FILTER_TYPE_NONE, F::FILTER_TYPE_NONE,
            F::TEXTURE_ADDRESS_CLAMP, F::TEXTURE_ADDRESS_CLAMP};
}
ShaderClass SentenceShader()
{
    ShaderClass shader;
    shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
    shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
    shader.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA);
    shader.Set_Src_Blend_Func(ShaderClass::SRCBLEND_SRC_ALPHA);
    shader.Set_Fog_Func(ShaderClass::FOG_DISABLE);
    shader.Set_Primary_Gradient(ShaderClass::GRADIENT_MODULATE);
    shader.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
    return shader;
}
ShaderClass DiffuseAlphaShader()
{
    ShaderClass shader = SentenceShader();
    shader.Set_Texturing(ShaderClass::TEXTURING_DISABLE);
    return shader;
}
// Render2D state set the UI actually produces: normal alpha, additive and
// opaque ("solid") blends, each textured or flat, plus the disabled-button
// grayscale combiner. Anything else stays an explicit unsupported failure.
enum class Pipe { Textured, Flat, Gray };
enum class BlendMode { Alpha, Additive, Opaque };
struct Render2DMode { Pipe pipe; BlendMode blend; };
unsigned BlendVariantBits(ShaderClass base, ShaderClass::SrcBlendFuncType src, ShaderClass::DstBlendFuncType dst)
{
    base.Set_Src_Blend_Func(src);
    base.Set_Dst_Blend_Func(dst);
    return base.Get_Bits();
}
bool ClassifyRender2D(unsigned bits, bool grayscale, Render2DMode &mode)
{
    const ShaderClass textured = SentenceShader(), flat = DiffuseAlphaShader();
    if (grayscale) {
        // Legacy: Set_Opaque_Shader + DOT3 luminance combiner over the bound texture.
        if (bits != textured.Get_Bits()) return false;
        mode = { Pipe::Gray, BlendMode::Opaque };
        return true;
    }
    struct Candidate { const ShaderClass &base; Pipe pipe; };
    for (const Candidate &c : { Candidate{textured, Pipe::Textured}, Candidate{flat, Pipe::Flat} }) {
        if (bits == c.base.Get_Bits()) { mode = { c.pipe, BlendMode::Alpha }; return true; }
        if (bits == BlendVariantBits(c.base, ShaderClass::SRCBLEND_ONE, ShaderClass::DSTBLEND_ONE)) {
            mode = { c.pipe, BlendMode::Additive }; return true;
        }
        if (bits == BlendVariantBits(c.base, ShaderClass::SRCBLEND_ONE, ShaderClass::DSTBLEND_ZERO)) {
            mode = { c.pipe, BlendMode::Opaque }; return true;
        }
    }
    return false;
}
bool ValidateSampler(const Texture2DSampler &s)
{
    const auto expected = SentenceSampler();
    if (s.min == expected.min && s.mag == expected.mag && s.mip == expected.mip &&
        s.u == expected.u && s.v == expected.v) return false;
    using F = TextureFilterClass;
    if (s.min == F::FILTER_TYPE_DEFAULT && s.mag == F::FILTER_TYPE_DEFAULT && s.mip == F::FILTER_TYPE_NONE &&
        s.u == F::TEXTURE_ADDRESS_REPEAT && s.v == F::TEXTURE_ADDRESS_REPEAT) return true;
    Unsupported();
}
std::atomic<unsigned> liveResidencies{0};
class Residency final : public Texture2DResidency {
public:
    Residency() { ++liveResidencies; }
    ~Residency() override { --liveResidencies; }
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> view;
    Texture2DDescription description;
    bool Describe(Texture2DDescription &out) override { out = description; return true; }
    // Immutable sentence resources: mutable mapping is a later boundary.
    Texture2DContent *Map(unsigned, const Texture2DRegion *) override { return nullptr; }
};
struct Vertex { float xyz[3]; unsigned color; float uv[2]; };
static_assert(sizeof(Vertex) == 24);
const char shaderSource[] = R"(
cbuffer Placement : register(b0) { float2 correction; float2 unused; };
struct V { float3 position : POSITION; float4 color : COLOR; float2 uv : TEXCOORD; };
struct P { float4 position : SV_POSITION; float4 color : COLOR; float2 uv : TEXCOORD; };
P VS(V v) {
    P p; p.position = float4(v.position.xy + correction, v.position.z, 1);
    p.color = v.color.bgra; p.uv = v.uv; return p;
}
Texture2D image : register(t0); SamplerState imageSampler : register(s0);
float4 PS(P p) : SV_TARGET { return image.Sample(imageSampler, p.uv) * p.color; }
float4 PSColor(P p) : SV_TARGET { return p.color; }
// Legacy disabled-button combiner: 4*dot(0.5*tex, tfactor-0.5) with tfactor
// 0x80A5CA8E gives luminance weights (0.294118, 0.584314, 0.113725); vertex RGB
// does not participate and the opaque shader overwrites the destination.
float4 PSGray(P p) : SV_TARGET {
    float4 t = image.Sample(imageSampler, p.uv);
    float g = saturate(dot(t.rgb, float3(0.294118, 0.584314, 0.113725)));
    return float4(g, g, g, t.a);
}
)";
const char worldShaderSource[] = R"(
cbuffer World : register(b0) { row_major float4x4 wvp; float4 flags; };
Texture2D image : register(t0); SamplerState imageSampler : register(s0);
struct VW { float3 pos : POSITION; float4 col : COLOR; float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; };
struct PW { float4 pos : SV_POSITION; float4 col : COLOR; float2 uv : TEXCOORD0; };
PW VSWorld(VW v) {
    PW o;
    o.pos = mul(float4(v.pos, 1), wvp);
    o.col = v.col.bgra;               // packed ARGB (D3DCOLOR) bytes are B,G,R,A
    o.uv = flags.x > 0.5 ? v.uv1 : v.uv0;
    return o;
}
// Terrain combiner: rgb = tex * diffuse; alpha only participates in the blend pass.
float4 PSTerrain(PW p) : SV_TARGET {
    float4 t = image.Sample(imageSampler, p.uv);
    float a = flags.y > 0.5 ? t.a * p.col.a : 1.0;
    return float4(t.rgb * p.col.rgb, a);
}
)";
ComPtr<ID3DBlob> CompileWorld(const char *entry, const char *profile)
{
    ComPtr<ID3DBlob> code, errors;
    HRESULT result = D3DCompile(worldShaderSource, sizeof(worldShaderSource) - 1, "D3D11World",
        nullptr, nullptr, entry, profile, D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &errors);
    if (FAILED(result) && errors)
        throw std::runtime_error(std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()));
    Check(result, "D3DCompile world");
    return code;
}
// Row-vector matrix product (Direct3D convention): out = a * b.
void Multiply(const float *a, const float *b, float *out)
{
    float r[16];
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
        float v = 0;
        for (int k = 0; k < 4; ++k) v += a[i * 4 + k] * b[k * 4 + j];
        r[i * 4 + j] = v;
    }
    std::memcpy(out, r, sizeof(r));
}
class GeoResidency final : public GeometryResidency {
public:
    GeoResidency() { ++liveResidencies; }
    ~GeoResidency() override { --liveResidencies; }
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11Buffer> vertices, indices;
    unsigned vertexCount = 0, indexCount = 0;
    unsigned Vertex_Count() const override { return vertexCount; }
    unsigned Index_Count() const override { return indexCount; }
};
ComPtr<ID3DBlob> Compile(const char *entry, const char *profile)
{
    ComPtr<ID3DBlob> code, errors;
    HRESULT result = D3DCompile(shaderSource, sizeof(shaderSource) - 1, "D3D11Sentence",
        nullptr, nullptr, entry, profile, D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &errors);
    if (FAILED(result) && errors)
        throw std::runtime_error(std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()));
    Check(result, "D3DCompile");
    return code;
}
}

struct D3D11Backend::Impl {
    Configuration config;
    TextureBridge bridge;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapchain;
    ComPtr<ID3D11Texture2D> target;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps, colorPS, grayPS;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11BlendState> blend, blendAdditive, blendOpaque;
    ComPtr<ID3D11DepthStencilState> depth;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11SamplerState> sampler;
    std::map<unsigned, ComPtr<ID3D11SamplerState>> uiSamplers;
    ComPtr<ID3D11SamplerState> boundSampler;
    ComPtr<ID3D11Buffer> vertices, indices, placement;
    // World pipeline (created lazily on the first world submission).
    ComPtr<ID3D11Texture2D> depthTexture;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11VertexShader> worldVS;
    ComPtr<ID3D11PixelShader> terrainPS;
    ComPtr<ID3D11InputLayout> worldLayout;
    ComPtr<ID3D11Buffer> worldCB;
    ComPtr<ID3D11RasterizerState> worldRaster;
    ComPtr<ID3D11DepthStencilState> depthWrite, depthTestOnly;
    std::map<unsigned, ComPtr<ID3D11SamplerState>> worldSamplers;
    bool worldReady = false;
    ComPtr<ID3D11ShaderResourceView> white, bound;
    unsigned vertexCapacity = 0, indexCapacity = 0;
    unsigned textures = 0, buffers = 0, draws = 0;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_10_0;
    bool scene = false;
    Presentation lastPresent;
    ~Impl() { if (context) { context->ClearState(); context->Flush(); } }
    void Target()
    {
        if (swapchain) Check(swapchain->GetBuffer(0, IID_PPV_ARGS(&target)), "GetBuffer");
        else {
            D3D11_TEXTURE2D_DESC d{};
            d.Width = config.width; d.Height = config.height; d.MipLevels = d.ArraySize = 1;
            d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1;
            d.BindFlags = D3D11_BIND_RENDER_TARGET;
            Check(device->CreateTexture2D(&d, nullptr, &target), "Create offscreen target");
        }
        Check(device->CreateRenderTargetView(target.Get(), nullptr, &rtv), "Create RTV");
        depthTexture.Reset(); dsv.Reset();
        if (config.depth_buffer) {
            D3D11_TEXTURE2D_DESC z{};
            z.Width = config.width; z.Height = config.height; z.MipLevels = z.ArraySize = 1;
            z.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; z.SampleDesc.Count = 1;
            z.BindFlags = D3D11_BIND_DEPTH_STENCIL;
            Check(device->CreateTexture2D(&z, nullptr, &depthTexture), "Create depth buffer");
            Check(device->CreateDepthStencilView(depthTexture.Get(), nullptr, &dsv), "Create DSV");
        }
    }
    void EnsureWorld()
    {
        if (worldReady) return;
        auto vsCode = CompileWorld("VSWorld", "vs_4_0");
        Check(device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &worldVS), "Create world VS");
        auto psCode = CompileWorld("PSTerrain", "ps_4_0");
        Check(device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &terrainPS), "Create terrain PS");
        const D3D11_INPUT_ELEMENT_DESC e[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}};
        Check(device->CreateInputLayout(e, 4, vsCode->GetBufferPointer(), vsCode->GetBufferSize(), &worldLayout), "Create world layout");
        D3D11_BUFFER_DESC cb{}; cb.ByteWidth = 80; cb.Usage = D3D11_USAGE_DEFAULT; cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Check(device->CreateBuffer(&cb, nullptr, &worldCB), "Create world constants");
        // Legacy default D3DCULL_CCW: clockwise triangles are front faces.
        D3D11_RASTERIZER_DESC r{}; r.FillMode = D3D11_FILL_SOLID; r.CullMode = D3D11_CULL_BACK;
        r.FrontCounterClockwise = FALSE; r.DepthClipEnable = TRUE;
        Check(device->CreateRasterizerState(&r, &worldRaster), "Create world raster state");
        D3D11_DEPTH_STENCIL_DESC d{}; d.DepthEnable = TRUE; d.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        Check(device->CreateDepthStencilState(&d, &depthWrite), "Create depth-write state");
        d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        Check(device->CreateDepthStencilState(&d, &depthTestOnly), "Create depth-test state");
        worldReady = true;
    }
    void Buffer(ComPtr<ID3D11Buffer> &buffer, unsigned &capacity, unsigned needed, unsigned bind)
    {
        if (capacity >= needed) return;
        D3D11_BUFFER_DESC d{};
        d.ByteWidth = (std::max)(needed, capacity * 2);
        d.Usage = D3D11_USAGE_DYNAMIC; d.BindFlags = bind; d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ComPtr<ID3D11Buffer> replacement;
        Check(device->CreateBuffer(&d, nullptr, &replacement), "Create dynamic buffer");
        buffer = replacement; capacity = d.ByteWidth; ++buffers;
    }
};

std::unique_ptr<D3D11Backend> D3D11Backend::Create(const Configuration &config, TextureBridge bridge)
{ return std::unique_ptr<D3D11Backend>(new D3D11Backend(config, bridge)); }

D3D11Backend::D3D11Backend(const Configuration &config, TextureBridge bridge) : impl(new Impl)
{
    if (!config.width || !config.height || config.width > 16384 || config.height > 16384) Unsupported();
    impl->config = config; impl->bridge = bridge;
    Set_UI_Filter_Policy(config.ui_filter, config.ui_anisotropy);
    const char *phase = "graphics_device";
    auto report = [&](const char *status, const char *detail = "", unsigned code = 0) {
        if (config.diagnostic) config.diagnostic(config.diagnostic_context, phase, status, detail, code);
    };
    try {
    report("begin", config.software ? "driver=WARP" : "driver=hardware");
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    const auto driver = config.software ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
    DXGI_SWAP_CHAIN_DESC swap{};
    swap.BufferDesc.Width = config.width; swap.BufferDesc.Height = config.height;
    swap.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap.SampleDesc.Count = 1; swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap.BufferCount = 1; swap.OutputWindow = static_cast<HWND>(config.window);
    swap.Windowed = TRUE; swap.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    Check(D3D11CreateDeviceAndSwapChain(nullptr, driver, nullptr, 0, levels, 3, D3D11_SDK_VERSION,
        config.window ? &swap : nullptr, config.window ? impl->swapchain.GetAddressOf() : nullptr,
        &impl->device, &impl->level, &impl->context), "D3D11CreateDeviceAndSwapChain");
    if (config.diagnostic || config.diagnostic_windowed) {
        ComPtr<IDXGIDevice> dxgiDevice; ComPtr<IDXGIAdapter> adapter;
        Check(impl->device.As(&dxgiDevice), "Query DXGI device");
        Check(dxgiDevice->GetAdapter(&adapter), "Get adapter");
        DXGI_ADAPTER_DESC desc{}; Check(adapter->GetDesc(&desc), "Get adapter description");
        char name[256]{};
        if (!WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), nullptr, nullptr))
            throw std::runtime_error("Adapter name conversion failed");
        char detail[512]; std::snprintf(detail, sizeof(detail),
            "driver=%s feature=0x%X vendor=0x%X device=0x%X adapter=%s size=%ux%u",
            config.software ? "WARP" : "hardware", static_cast<unsigned>(impl->level),
            desc.VendorId, desc.DeviceId, name, config.width, config.height);
        report("complete", detail);
        if (config.diagnostic_windowed) {
            phase = "graphics_window_association"; report("begin");
            ComPtr<IDXGIFactory> factory;
            Check(adapter->GetParent(IID_PPV_ARGS(&factory)), "Get DXGI factory");
            Check(factory->MakeWindowAssociation(static_cast<HWND>(config.window),
                DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES), "MakeWindowAssociation");
            report("complete");
        }
    } else report("complete");
    phase = "graphics_target"; report("begin");
    impl->Target(); report("complete", "RGBA8 single-sample backbuffer/RTV");
    phase = "graphics_setup"; report("begin");
    auto vs = Compile("VS", "vs_4_0"), ps = Compile("PS", "ps_4_0");
    Check(impl->device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &impl->vs), "Create VS");
    Check(impl->device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &impl->ps), "Create PS");
    auto colorPS = Compile("PSColor", "ps_4_0");
    Check(impl->device->CreatePixelShader(colorPS->GetBufferPointer(), colorPS->GetBufferSize(), nullptr, &impl->colorPS), "Create diffuse UI PS");
    const D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    Check(impl->device->CreateInputLayout(elements, 3, vs->GetBufferPointer(), vs->GetBufferSize(), &impl->layout), "Create layout");
    D3D11_BLEND_DESC b{}; auto &rt = b.RenderTarget[0];
    rt.BlendEnable = TRUE; rt.SrcBlend = rt.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    rt.DestBlend = rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = rt.BlendOpAlpha = D3D11_BLEND_OP_ADD; rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    Check(impl->device->CreateBlendState(&b, &impl->blend), "Create blend state");
    {
        auto grayPS = Compile("PSGray", "ps_4_0");
        Check(impl->device->CreatePixelShader(grayPS->GetBufferPointer(), grayPS->GetBufferSize(), nullptr, &impl->grayPS), "Create grayscale UI PS");
        D3D11_BLEND_DESC add{}; auto &a = add.RenderTarget[0];
        a.BlendEnable = TRUE; a.SrcBlend = a.SrcBlendAlpha = D3D11_BLEND_ONE;
        a.DestBlend = a.DestBlendAlpha = D3D11_BLEND_ONE;
        a.BlendOp = a.BlendOpAlpha = D3D11_BLEND_OP_ADD; a.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        Check(impl->device->CreateBlendState(&add, &impl->blendAdditive), "Create additive blend state");
        D3D11_BLEND_DESC opaque{}; auto &o = opaque.RenderTarget[0];
        o.BlendEnable = FALSE; o.SrcBlend = o.SrcBlendAlpha = D3D11_BLEND_ONE;
        o.DestBlend = o.DestBlendAlpha = D3D11_BLEND_ZERO;
        o.BlendOp = o.BlendOpAlpha = D3D11_BLEND_OP_ADD; o.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        Check(impl->device->CreateBlendState(&opaque, &impl->blendOpaque), "Create opaque blend state");
    }
    D3D11_DEPTH_STENCIL_DESC d{}; d.DepthEnable = FALSE; d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    d.DepthFunc = D3D11_COMPARISON_ALWAYS;
    Check(impl->device->CreateDepthStencilState(&d, &impl->depth), "Create depth state");
    D3D11_RASTERIZER_DESC r{}; r.FillMode = D3D11_FILL_SOLID; r.CullMode = D3D11_CULL_BACK;
    r.FrontCounterClockwise = TRUE; r.DepthClipEnable = TRUE;
    Check(impl->device->CreateRasterizerState(&r, &impl->raster), "Create raster state");
    D3D11_SAMPLER_DESC s{}; s.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    s.ComparisonFunc = D3D11_COMPARISON_NEVER; s.MaxAnisotropy = 1; s.MinLOD = s.MaxLOD = 0;
    Check(impl->device->CreateSamplerState(&s, &impl->sampler), "Create sentence sampler");
    D3D11_BUFFER_DESC cb{}; cb.ByteWidth = 16; cb.Usage = D3D11_USAGE_DEFAULT; cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Check(impl->device->CreateBuffer(&cb, nullptr, &impl->placement), "Create placement buffer");
    D3D11_TEXTURE2D_DESC white{}; white.Width = white.Height = white.ArraySize = white.MipLevels = 1;
    white.Format = DXGI_FORMAT_R8G8B8A8_UNORM; white.SampleDesc.Count = 1;
    white.Usage = D3D11_USAGE_IMMUTABLE; white.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const unsigned pixel = 0xffffffff; D3D11_SUBRESOURCE_DATA data{&pixel, 4, 0};
    ComPtr<ID3D11Texture2D> texture;
    Check(impl->device->CreateTexture2D(&white, &data, &texture), "Create null diffuse texture");
    Check(impl->device->CreateShaderResourceView(texture.Get(), nullptr, &impl->white), "Create null SRV");
    report("complete", "VS/PS/layout/states/resources");
    phase = "graphics_viewport"; report("begin");
    Set_Viewport({0, 0, config.width, config.height, 0, 1}); report("complete");
    } catch (const std::exception &error) { report("fail", error.what(), 1); throw; }
}
D3D11Backend::~D3D11Backend() = default;

std::vector<unsigned char> D3D11Backend::Convert_Sentence_RGBA8(const Texture2DCPUContent &content)
{
    const auto d = content.Describe();
    if (d.format != WW3D_FORMAT_A4R4G4B4 || d.mip_count != 1 || d.missing) Unsupported();
    std::vector<unsigned char> out(static_cast<size_t>(d.width) * d.height * 4);
    for (unsigned y = 0; y < d.height; ++y)
        for (unsigned x = 0; x < d.width; ++x) {
            unsigned short v;
            std::memcpy(&v, content.Pixels() + static_cast<size_t>(y) * content.Row_Pitch() + x * 2, 2);
            auto *p = out.data() + (static_cast<size_t>(y) * d.width + x) * 4;
            p[0] = ((v >> 8) & 15) * 17; p[1] = ((v >> 4) & 15) * 17;
            p[2] = (v & 15) * 17; p[3] = ((v >> 12) & 15) * 17;
        }
    return out;
}
Texture2DResidency *D3D11Backend::Create_Content_Residency(const Texture2DCPUContent &content)
{
    auto result = std::make_unique<Residency>();
    result->device = impl->device; result->description = content.Describe();
    const auto format = result->description.format;
    std::vector<unsigned char> pixels;
    DXGI_FORMAT native = DXGI_FORMAT_R8G8B8A8_UNORM;
    unsigned pitch = result->description.width * 4;
    const void *source = nullptr;
    if (format == WW3D_FORMAT_A4R4G4B4) {
        pixels = Convert_Sentence_RGBA8(content); source = pixels.data();
    } else if (format == WW3D_FORMAT_R5G6B5) {
        pixels.resize(size_t(result->description.width) * result->description.height * 4);
        for (unsigned y = 0; y < result->description.height; ++y) for (unsigned x = 0; x < result->description.width; ++x) {
            unsigned short value; std::memcpy(&value, content.Pixels() + size_t(y) * content.Row_Pitch() + x * 2, 2);
            auto *p = pixels.data() + size_t(y) * pitch + x * 4;
            p[0] = (((value >> 11) & 31) * 255 + 15) / 31;
            p[1] = (((value >> 5) & 63) * 255 + 31) / 63;
            p[2] = ((value & 31) * 255 + 15) / 31; p[3] = 255;
        }
        source = pixels.data();
    } else if (format == WW3D_FORMAT_A8R8G8B8 || format == WW3D_FORMAT_X8R8G8B8) {
        pixels.resize(size_t(result->description.width) * result->description.height * 4);
        for (unsigned y = 0; y < result->description.height; ++y) for (unsigned x = 0; x < result->description.width; ++x) {
            const auto *s = content.Pixels() + size_t(y) * content.Row_Pitch() + x * 4;
            auto *p = pixels.data() + size_t(y) * pitch + x * 4;
            p[0] = s[2]; p[1] = s[1]; p[2] = s[0]; p[3] = format == WW3D_FORMAT_A8R8G8B8 ? s[3] : 255;
        }
        source = pixels.data();
    } else {
        switch (format) {
        case WW3D_FORMAT_DXT1: native = DXGI_FORMAT_BC1_UNORM; break;
        case WW3D_FORMAT_DXT3: native = DXGI_FORMAT_BC2_UNORM; break;
        case WW3D_FORMAT_DXT5: native = DXGI_FORMAT_BC3_UNORM; break;
        default: Unsupported();
        }
        pitch = content.Row_Pitch(); source = content.Pixels();
    }
    D3D11_TEXTURE2D_DESC d{}; d.Width = result->description.width; d.Height = result->description.height;
    d.ArraySize = d.MipLevels = 1; d.Format = native;
    d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_IMMUTABLE; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{source, pitch, 0};
    Check(impl->device->CreateTexture2D(&d, &data, &result->texture), "Upload sentence texture");
    Check(impl->device->CreateShaderResourceView(result->texture.Get(), nullptr, &result->view), "Create sentence SRV");
    ++impl->textures;
    return result.release();
}
void D3D11Backend::Bind_Content(Texture2DResidency *content, const Texture2DSampler &sampler)
{
    const bool ui = ValidateSampler(sampler);
    impl->boundSampler = impl->sampler;
    if (ui) {
        const unsigned key = unsigned(impl->config.ui_filter) * 32 + impl->config.ui_anisotropy;
        auto &cached = impl->uiSamplers[key];
        if (!cached) {
            D3D11_SAMPLER_DESC s{};
            s.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            if (impl->config.ui_filter == TextureFilterClass::TEXTURE_FILTER_BILINEAR ||
                impl->config.ui_filter == TextureFilterClass::TEXTURE_FILTER_TRILINEAR)
                s.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            if (impl->config.ui_filter == TextureFilterClass::TEXTURE_FILTER_ANISOTROPIC)
                s.Filter = D3D11_FILTER_ANISOTROPIC;
            s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            s.ComparisonFunc = D3D11_COMPARISON_NEVER; s.MinLOD = s.MaxLOD = 0;
            s.MaxAnisotropy = impl->config.ui_anisotropy;
            Check(impl->device->CreateSamplerState(&s, &cached), "Create UI sampler");
        }
        impl->boundSampler = cached;
    }
    if (!content) { impl->bound.Reset(); return; }
    auto *resource = dynamic_cast<Residency *>(content);
    if (!resource || resource->device.Get() != impl->device.Get()) Unsupported();
    impl->bound = resource->view;
}
void D3D11Backend::Submit_Render2D_Content(const Render2DSubmission &s, Texture2DResidency *content, const Texture2DSampler &sampler)
{
    if (s.texture) Unsupported(); // explicit neutral-content route, never ignore logical identity
    Bind_Content(content, sampler); Draw(s);
}
void D3D11Backend::Submit_Render2D(const Render2DSubmission &s)
{
    if (!s.index_count) return;
    Render2DMode submitted = {};
    if (s.shader && !s.grayscale && ClassifyRender2D(s.shader->Get_Bits(), false, submitted) && submitted.pipe == Pipe::Flat) {
        // Legacy Render2D keeps Texture across Reset/Enable_Texturing(false).
        // A flat shader does not sample it. Normalize only the backend draw,
        // preserving the producer's identity/refcount for its next image draw.
        Render2DSubmission diffuse = s;
        diffuse.texture = nullptr;
        Bind_Content(nullptr, SentenceSampler());
        Draw(diffuse);
        return;
    }
    if (s.texture) Apply_Render2D_Texture(*s.texture, 0);
    else Bind_Content(nullptr, SentenceSampler());
    Draw(s);
}
void D3D11Backend::Draw(const Render2DSubmission &s)
{
    if (!s.index_count) return;
    Render2DMode mode = {};
    if (!impl->scene || !s.shader || !ClassifyRender2D(s.shader->Get_Bits(), s.grayscale, mode) ||
        (mode.pipe == Pipe::Flat && (s.texture || impl->bound)) ||
        (mode.pipe == Pipe::Gray && !impl->bound) ||
        s.vertex_count <= 0 || s.vertex_count > 65535 || s.index_count < 0 || s.index_count > 65535 ||
        s.index_count % 3 || !s.positions || !s.uvs || !s.colors || !s.indices ||
        !std::isfinite(s.z) || s.z < 0 || s.z > 1) Unsupported();
    for (int i = 0; i < s.index_count; ++i) if (s.indices[i] >= s.vertex_count) Unsupported();
    Set_Viewport(s.viewport);
    impl->Buffer(impl->vertices, impl->vertexCapacity, s.vertex_count * sizeof(Vertex), D3D11_BIND_VERTEX_BUFFER);
    impl->Buffer(impl->indices, impl->indexCapacity, s.index_count * sizeof(unsigned short), D3D11_BIND_INDEX_BUFFER);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Check(impl->context->Map(impl->vertices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), "Map vertices");
    Write_Render2D_Vertices(s, {static_cast<unsigned char *>(mapped.pData), sizeof(Vertex), 0, 12, 16});
    impl->context->Unmap(impl->vertices.Get(), 0);
    Check(impl->context->Map(impl->indices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), "Map indices");
    std::memcpy(mapped.pData, s.indices, s.index_count * sizeof(unsigned short));
    impl->context->Unmap(impl->indices.Get(), 0);
    // DX8 pixel centers are integers; D3D11 centers are half integers. Shift
    // the already-biased W3D clip input by +0.5,+0.5 viewport pixels.
    const float correction[] = {1.0f / s.viewport.width, -1.0f / s.viewport.height, 0, 0};
    impl->context->UpdateSubresource(impl->placement.Get(), 0, nullptr, correction, 0, 0);
    const unsigned stride = sizeof(Vertex), offset = 0;
    auto *vb = impl->vertices.Get(); auto *cb = impl->placement.Get(); auto *rtv = impl->rtv.Get();
    auto *srv = impl->bound ? impl->bound.Get() : impl->white.Get(); auto *sampler = impl->boundSampler.Get();
    impl->context->OMSetRenderTargets(1, &rtv, nullptr);
    auto *blendState = mode.blend == BlendMode::Additive ? impl->blendAdditive.Get()
        : mode.blend == BlendMode::Opaque ? impl->blendOpaque.Get() : impl->blend.Get();
    impl->context->OMSetBlendState(blendState, nullptr, 0xffffffff);
    impl->context->OMSetDepthStencilState(impl->depth.Get(), 0);
    impl->context->RSSetState(impl->raster.Get());
    impl->context->IASetInputLayout(impl->layout.Get());
    impl->context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    impl->context->IASetIndexBuffer(impl->indices.Get(), DXGI_FORMAT_R16_UINT, 0);
    impl->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    impl->context->VSSetShader(impl->vs.Get(), nullptr, 0);
    impl->context->VSSetConstantBuffers(0, 1, &cb);
    impl->context->GSSetShader(nullptr, nullptr, 0);
    impl->context->PSSetShader(mode.pipe == Pipe::Gray ? impl->grayPS.Get()
        : mode.pipe == Pipe::Flat ? impl->colorPS.Get() : impl->ps.Get(), nullptr, 0);
    impl->context->PSSetShaderResources(0, 1, &srv);
    impl->context->PSSetSamplers(0, 1, &sampler);
    impl->context->DrawIndexed(s.index_count, 0, 0); ++impl->draws;
}
void D3D11Backend::Begin_Scene() { if (impl->scene) Unsupported(); impl->scene = true; }
void D3D11Backend::End_Scene(bool flip) { if (!impl->scene) Unsupported(); impl->scene = false; if (flip) Flip_To_Primary(); }
void D3D11Backend::Flip_To_Primary() {
    if (!impl->swapchain) Unsupported();
    const HRESULT result = impl->swapchain->Present(0, 0);
    impl->lastPresent = {static_cast<unsigned>(result),
        FAILED(result) ? static_cast<unsigned>(impl->device->GetDeviceRemovedReason()) : 0u,
        result == DXGI_STATUS_OCCLUDED};
    Check(result, "Present");
}
D3D11Backend::Presentation D3D11Backend::Last_Presentation() const { return impl->lastPresent; }
void D3D11Backend::Clear(bool color, bool depth, const Vector3 &rgb, float alpha, float depth_value, unsigned stencil)
{
    if (depth && !impl->dsv) Unsupported(); // depth clear needs the opt-in depth buffer
    if (color) { const float rgba[] = {rgb.X, rgb.Y, rgb.Z, alpha}; impl->context->ClearRenderTargetView(impl->rtv.Get(), rgba); }
    if (depth) {
        const float z = std::isfinite(depth_value) && depth_value >= 0 && depth_value <= 1 ? depth_value : 1.0f;
        impl->context->ClearDepthStencilView(impl->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, z, static_cast<UINT8>(stencil));
    }
}
void D3D11Backend::Set_Viewport(const RenderBackendViewport &v)
{
    if (!v.width || !v.height || v.x > impl->config.width || v.y > impl->config.height ||
        v.width > impl->config.width - v.x || v.height > impl->config.height - v.y ||
        !std::isfinite(v.min_z) || !std::isfinite(v.max_z) || v.min_z < 0 || v.max_z > 1 || v.min_z > v.max_z) Unsupported();
    D3D11_VIEWPORT viewport{static_cast<float>(v.x), static_cast<float>(v.y),
        static_cast<float>(v.width), static_cast<float>(v.height), v.min_z, v.max_z};
    impl->context->RSSetViewports(1, &viewport);
}
void D3D11Backend::Resize(unsigned width, unsigned height)
{
    if (impl->scene || !width || !height || width > 16384 || height > 16384) Unsupported();
    impl->context->OMSetRenderTargets(0, nullptr, nullptr);
    impl->rtv.Reset(); impl->target.Reset(); impl->dsv.Reset(); impl->depthTexture.Reset();
    if (impl->swapchain) Check(impl->swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0), "ResizeBuffers");
    impl->config.width = width; impl->config.height = height; impl->Target();
    Set_Viewport({0, 0, width, height, 0, 1});
}
std::vector<unsigned char> D3D11Backend::Readback_RGBA8()
{
    D3D11_TEXTURE2D_DESC d{}; impl->target->GetDesc(&d);
    d.BindFlags = 0; d.Usage = D3D11_USAGE_STAGING; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    Check(impl->device->CreateTexture2D(&d, nullptr, &staging), "Create validation readback");
    impl->context->CopyResource(staging.Get(), impl->target.Get());
    D3D11_MAPPED_SUBRESOURCE m{};
    Check(impl->context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &m), "Read validation output");
    std::vector<unsigned char> pixels(static_cast<size_t>(d.Width) * d.Height * 4);
    for (unsigned y = 0; y < d.Height; ++y)
        std::memcpy(pixels.data() + static_cast<size_t>(y) * d.Width * 4,
                    static_cast<const unsigned char *>(m.pData) + static_cast<size_t>(y) * m.RowPitch, d.Width * 4);
    impl->context->Unmap(staging.Get(), 0);
    return pixels;
}
unsigned D3D11Backend::Texture_Creations() const { return impl->textures; }
unsigned D3D11Backend::Buffer_Creations() const { return impl->buffers; }
unsigned D3D11Backend::Draw_Count() const { return impl->draws; }
unsigned D3D11Backend::Live_Residencies() { return liveResidencies.load(); }
bool D3D11Backend::Has_Depth_Buffer() const { return impl->dsv != nullptr; }
GeometryResidency *D3D11Backend::Create_Geometry_Residency(const GeometryCPUContent &content)
{
    if (!content.Valid()) Unsupported();
    auto result = std::make_unique<GeoResidency>();
    result->device = impl->device;
    result->vertexCount = static_cast<unsigned>(content.vertices.size());
    result->indexCount = static_cast<unsigned>(content.indices.size());
    D3D11_BUFFER_DESC vb{}; vb.ByteWidth = result->vertexCount * sizeof(WorldVertexXYZDUV2);
    vb.Usage = D3D11_USAGE_DEFAULT; vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vd{content.vertices.data(), 0, 0};
    Check(impl->device->CreateBuffer(&vb, &vd, &result->vertices), "Create geometry vertices");
    D3D11_BUFFER_DESC ib{}; ib.ByteWidth = result->indexCount * sizeof(unsigned short);
    ib.Usage = D3D11_USAGE_IMMUTABLE; ib.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA id{content.indices.data(), 0, 0};
    Check(impl->device->CreateBuffer(&ib, &id, &result->indices), "Create geometry indices");
    impl->buffers += 2;
    return result.release();
}
void D3D11Backend::Update_Geometry_Vertices(GeometryResidency &geometry, unsigned first, const WorldVertexXYZDUV2 *vertices, unsigned count)
{
    auto *g = dynamic_cast<GeoResidency *>(&geometry);
    if (!g || g->device.Get() != impl->device.Get() || !vertices || !count || first > g->vertexCount ||
        count > g->vertexCount - first) Unsupported();
    D3D11_BOX box{first * static_cast<UINT>(sizeof(WorldVertexXYZDUV2)), 0, 0,
                  (first + count) * static_cast<UINT>(sizeof(WorldVertexXYZDUV2)), 1, 1};
    impl->context->UpdateSubresource(g->vertices.Get(), 0, &box, vertices, 0, 0);
}
void D3D11Backend::Submit_World(const WorldSubmission &s)
{
    if (!s.index_count) return;
    auto *g = dynamic_cast<GeoResidency *>(s.geometry);
    if (!impl->scene || !impl->dsv || !g || g->device.Get() != impl->device.Get() || s.index_count % 3 ||
        s.first_index > g->indexCount || s.index_count > g->indexCount - s.first_index) Unsupported();
    if (s.material != WorldMaterial::TerrainBase && s.material != WorldMaterial::TerrainBlend) Unsupported();
    if (!s.texture) Unsupported();
    auto *texture = dynamic_cast<Residency *>(s.texture);
    if (!texture || texture->device.Get() != impl->device.Get()) Unsupported();
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(s.world[i]) || !std::isfinite(s.view[i]) || !std::isfinite(s.projection[i])) Unsupported();
    impl->EnsureWorld();
    Set_Viewport(s.viewport);

    float wvp[16];
    Multiply(s.world, s.view, wvp);
    Multiply(wvp, s.projection, wvp);
    float constants[20];
    std::memcpy(constants, wvp, sizeof(wvp));
    constants[16] = s.material == WorldMaterial::TerrainBlend ? 1.0f : 0.0f; // UV set
    constants[17] = s.material == WorldMaterial::TerrainBlend ? 1.0f : 0.0f; // use texture/diffuse alpha
    constants[18] = constants[19] = 0.0f;
    impl->context->UpdateSubresource(impl->worldCB.Get(), 0, nullptr, constants, 0, 0);

    const unsigned samplerKey = (s.linear ? 1u : 0u) | (s.mip_linear ? 2u : 0u) | (s.clamp ? 4u : 0u);
    auto &sampler = impl->worldSamplers[samplerKey];
    if (!sampler) {
        D3D11_SAMPLER_DESC d{};
        d.Filter = s.linear ? (s.mip_linear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT)
                            : (s.mip_linear ? D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT);
        d.AddressU = d.AddressV = d.AddressW = s.clamp ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
        d.ComparisonFunc = D3D11_COMPARISON_NEVER; d.MaxAnisotropy = 1; d.MinLOD = 0; d.MaxLOD = D3D11_FLOAT32_MAX;
        Check(impl->device->CreateSamplerState(&d, &sampler), "Create world sampler");
    }
    const unsigned stride = sizeof(WorldVertexXYZDUV2), offset = 0;
    auto *vb = g->vertices.Get(); auto *cb = impl->worldCB.Get(); auto *rtv = impl->rtv.Get();
    auto *srv = texture->view.Get(); auto *smp = sampler.Get();
    impl->context->OMSetRenderTargets(1, &rtv, impl->dsv.Get());
    impl->context->OMSetBlendState(s.material == WorldMaterial::TerrainBlend ? impl->blend.Get() : impl->blendOpaque.Get(),
                                   nullptr, 0xffffffff);
    impl->context->OMSetDepthStencilState(s.depth_write ? impl->depthWrite.Get() : impl->depthTestOnly.Get(), 0);
    impl->context->RSSetState(impl->worldRaster.Get());
    impl->context->IASetInputLayout(impl->worldLayout.Get());
    impl->context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    impl->context->IASetIndexBuffer(g->indices.Get(), DXGI_FORMAT_R16_UINT, 0);
    impl->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    impl->context->VSSetShader(impl->worldVS.Get(), nullptr, 0);
    impl->context->VSSetConstantBuffers(0, 1, &cb);
    impl->context->PSSetShader(impl->terrainPS.Get(), nullptr, 0);
    impl->context->PSSetConstantBuffers(0, 1, &cb);
    impl->context->PSSetShaderResources(0, 1, &srv);
    impl->context->PSSetSamplers(0, 1, &smp);
    impl->context->DrawIndexed(s.index_count, s.first_index, 0);
    ++impl->draws;
    // The Render2D path binds its own pipeline per draw; leave no stale depth state.
    impl->bound.Reset();
}
unsigned D3D11Backend::Feature_Level() const { return impl->level; }
void D3D11Backend::Invalidate_Cached_Render_States() { impl->bound.Reset(); } // pipeline is explicitly bound each draw
void D3D11Backend::Set_Gamma(float, float, float, bool, bool) { Unsupported(); }
void D3D11Backend::Set_Ambient(const Vector3 &) { Unsupported(); }
void D3D11Backend::Set_Light_Environment(LightEnvironmentClass *) { Unsupported(); }
void D3D11Backend::Copy_Render2D_Surface(SurfaceClass &, SurfaceClass &) { Unsupported(); }
Texture2DResidency *D3D11Backend::Create_Render2D_Residency(TextureClass &texture)
{ if (!impl->bridge.create) Unsupported(); return impl->bridge.create(*this, texture); }
void D3D11Backend::Apply_Render2D_Texture(TextureClass &texture, unsigned stage)
{ if (!impl->bridge.apply) Unsupported(); impl->bridge.apply(*this, texture, stage); }
bool D3D11Backend::Prepare_File_Texture(TextureClass &texture)
{ if (!impl->bridge.prepare) Unsupported(); return impl->bridge.prepare(*this, texture); }
void D3D11Backend::Set_UI_Filter_Policy(TextureFilterClass::TextureFilterMode filter, unsigned anisotropy)
{
    if (filter < TextureFilterClass::TEXTURE_FILTER_NONE || filter >= TextureFilterClass::TEXTURE_FILTER_COUNT ||
        (anisotropy != 1 && anisotropy != 2 && anisotropy != 4 && anisotropy != 8 && anisotropy != 16)) Unsupported();
    impl->config.ui_filter = filter; impl->config.ui_anisotropy = anisotropy;
}
