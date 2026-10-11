// SPDX-License-Identifier: GPL-3.0-or-later
#include "DX8Backend.h"
#include "WW3D2/Texture2D.h"
#include "WW3D2/texture.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/formconv.h"
#include "WW3D2/statistics.h"
#include "WWMath/vector2i.h"

// Retain the existing global policy, including capability fallback, DEFAULT,
// single-mip NONE and the different anisotropy policy on stages above zero.
extern unsigned _MinTextureFilters[MAX_TEXTURE_STAGES][TextureFilterClass::FILTER_TYPE_COUNT];
extern unsigned _MagTextureFilters[MAX_TEXTURE_STAGES][TextureFilterClass::FILTER_TYPE_COUNT];
extern unsigned _MipMapFilters[MAX_TEXTURE_STAGES][TextureFilterClass::FILTER_TYPE_COUNT];

namespace
{
class DX8Texture2DContent : public Texture2DContent
{
    SurfaceClass *surface;
    Texture2DMapping mapping;
public:
    DX8Texture2DContent(SurfaceClass *owned_surface, const Texture2DRegion *region)
        : surface(owned_surface)
    {
        if (region)
            mapping.data = surface->Lock(&mapping.row_pitch,
                Vector2i(region->left, region->top), Vector2i(region->right, region->bottom));
        else mapping.data = surface->Lock(&mapping.row_pitch);
    }
    ~DX8Texture2DContent() override
    {
        if (mapping.data) surface->Unlock();
        surface->Release_Ref();
    }
    Texture2DMapping Mapping() const override { return mapping; }
};

class DX8Texture2DResidency : public Texture2DResidency
{
    // Adopt the reference previously owned by TextureBaseClass. Its pointer
    // remains a legacy alias; Release_D3D_Storage destroys this owner first.
    TextureClass &texture;
    IDirect3DTexture8 *storage;
public:
    explicit DX8Texture2DResidency(TextureClass &logical)
        : texture(logical), storage(logical.Peek_D3D_Texture()) {}
    ~DX8Texture2DResidency() override { if (storage) storage->Release(); }
    IDirect3DBaseTexture8 *Current() { return texture.Peek_D3D_Base_Texture(); }
    bool Describe(Texture2DDescription &description) override
    {
        IDirect3DTexture8 *current = texture.Peek_D3D_Texture();
        if (!current) return false;
        D3DSURFACE_DESC desc = {};
        if (FAILED(current->GetLevelDesc(0, &desc))) return false;
        description = {desc.Width, desc.Height, current->GetLevelCount(),
                       D3DFormat_To_WW3DFormat(desc.Format), texture.Is_Missing_Texture()};
        return true;
    }
    Texture2DContent *Map(unsigned mip, const Texture2DRegion *region) override
    {
        IDirect3DTexture8 *current = texture.Peek_D3D_Texture();
        if (!current || mip >= current->GetLevelCount()) return nullptr;
        SurfaceClass *surface = texture.Get_Surface_Level(mip);
        return surface ? new DX8Texture2DContent(surface, region) : nullptr;
    }
};

struct DX8SamplerCommands
{
    void Min(unsigned stage, TextureFilterClass::FilterType value)
    { DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, _MinTextureFilters[stage][value]); }
    void Mag(unsigned stage, TextureFilterClass::FilterType value)
    { DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, _MagTextureFilters[stage][value]); }
    void Mip(unsigned stage, TextureFilterClass::FilterType value)
    { DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MIPFILTER, _MipMapFilters[stage][value]); }
    void U(unsigned stage, TextureFilterClass::TxtAddrMode value)
    {
        switch (value) {
        case TextureFilterClass::TEXTURE_ADDRESS_REPEAT:
            DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP); break;
        case TextureFilterClass::TEXTURE_ADDRESS_CLAMP:
            DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP); break;
        }
    }
    void V(unsigned stage, TextureFilterClass::TxtAddrMode value)
    {
        switch (value) {
        case TextureFilterClass::TEXTURE_ADDRESS_REPEAT:
            DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP); break;
        case TextureFilterClass::TEXTURE_ADDRESS_CLAMP:
            DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP); break;
        }
    }
};

struct DX8Texture2DCommands
{
    bool Texturing_Enabled() { return WW3D::Is_Texturing_Enabled(); }
    void Record(TextureClass &texture) { DX8_RECORD_TEXTURE(&texture); }
    Texture2DResidency &Ensure_Residency(TextureClass &texture) { return texture.Get_Render2D_Residency(); }
    void Bind(unsigned stage, Texture2DResidency &residency)
    { DX8Wrapper::Set_DX8_Texture(stage, static_cast<DX8Texture2DResidency &>(residency).Current()); }
    void Bind_Null(unsigned stage) { DX8Wrapper::Set_DX8_Texture(stage, nullptr); }
    void Apply_Sampler(unsigned stage, const Texture2DSampler &sampler)
    {
        DX8SamplerCommands commands;
        Execute_Texture2D_Sampler(commands, stage, sampler);
    }
};
}

Texture2DResidency *DX8Backend::Create_Render2D_Residency(TextureClass &texture)
{
    return new DX8Texture2DResidency(texture);
}

void DX8Backend::Apply_Render2D_Texture(TextureClass &texture, unsigned stage)
{
    DX8Texture2DCommands commands;
    Execute_Texture2D(commands, &texture, stage);
}

void DX8Backend::Copy_Render2D_Surface(SurfaceClass &source, SurfaceClass &destination)
{
    // Keep CopyRects rather than replacing format/pitch/copy behavior with a
    // CPU memcpy. Caller retains the original surface acquisition/release order.
    DX8Wrapper::_Copy_DX8_Rects(source.Peek_D3D_Surface(), nullptr, 0, destination.Peek_D3D_Surface(), nullptr);
}
