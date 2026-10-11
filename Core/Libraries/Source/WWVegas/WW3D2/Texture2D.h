// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ww3dformat.h"
#include "texturefilter.h"

// W3D policy, not resolved device sampler constants. DEFAULT/BEST still depend
// on the reference backend's existing global filter policy and capabilities.
struct Texture2DSampler
{
    TextureFilterClass::FilterType min, mag, mip;
    TextureFilterClass::TxtAddrMode u, v;
};

template<class Filter>
inline Texture2DSampler Describe_Texture2D_Sampler(const Filter &filter)
{
    return {filter.Get_Min_Filter(), filter.Get_Mag_Filter(), filter.Get_Mip_Mapping(),
            filter.Get_U_Addr_Mode(), filter.Get_V_Addr_Mode()};
}

struct Texture2DDescription
{
    unsigned width, height, mip_count;
    WW3DFormat format;
    bool missing;
};

struct Texture2DRegion { unsigned left, top, right, bottom; }; // exclusive end
struct Texture2DMapping { void *data; int row_pitch; };

// A synchronous mapped subresource. The backend owns its surface reference and
// unlocks/releases it on destruction. No borrowed pointer survives this lease.
class Texture2DContent
{
public:
    virtual ~Texture2DContent() {}
    virtual Texture2DMapping Mapping() const = 0;
};

// Owned by the logical W3D texture; created/destroyed by its backend. The DX8
// adapter adopts the existing native reference without adding another owner.
// Replacement/invalidation destroys residency before changing the legacy alias.
class Texture2DResidency
{
public:
    virtual ~Texture2DResidency() {}
    virtual bool Describe(Texture2DDescription &description) = 0;
    virtual Texture2DContent *Map(unsigned mip, const Texture2DRegion *region = nullptr) = 0;
};

// Logical ownership of backend storage, without changing TextureClass's
// existing logical reference count or adding a second native reference.
class Texture2DResidencyOwner
{
    Texture2DResidency *residency = nullptr;
public:
    Texture2DResidencyOwner() = default;
    Texture2DResidencyOwner(const Texture2DResidencyOwner &) = delete;
    Texture2DResidencyOwner &operator=(const Texture2DResidencyOwner &) = delete;
    ~Texture2DResidencyOwner() { Reset(); }
    bool Has_Residency() const { return residency != nullptr; }
    void Reset() { delete residency; residency = nullptr; }
    template<class Factory> Texture2DResidency &Get_Or_Create(Factory factory)
    {
        if (!residency) residency = factory();
        return *residency;
    }
};

// Actual application sequence, shared with recording tests. Null skips both
// initialization and sampler changes, exactly like the reference null path.
// Initialization stays at deferred state application, not geometry submission.
template<class Commands, class Texture>
void Execute_Texture2D(Commands &commands, Texture *texture, unsigned stage)
{
    if (!texture) { commands.Bind_Null(stage); return; }
    texture->Prepare_Render2D_Texture();
    commands.Record(*texture);
    Texture2DResidency &residency = commands.Ensure_Residency(*texture);
    if (commands.Texturing_Enabled()) commands.Bind(stage, residency);
    else commands.Bind_Null(stage);
    commands.Apply_Sampler(stage, Describe_Texture2D_Sampler(texture->Get_Filter()));
}

// Preserve sampler application order independently of device enum translation.
template<class Commands>
void Execute_Texture2D_Sampler(Commands &commands, unsigned stage, const Texture2DSampler &sampler)
{
    commands.Min(stage, sampler.min);
    commands.Mag(stage, sampler.mag);
    commands.Mip(stage, sampler.mip);
    commands.U(stage, sampler.u);
    commands.V(stage, sampler.v);
}
