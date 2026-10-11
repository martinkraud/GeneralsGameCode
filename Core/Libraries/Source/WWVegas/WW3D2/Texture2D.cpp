// SPDX-License-Identifier: GPL-3.0-or-later
#include "Texture2D.h"
#include "Texture2DCPUContent.h"
#include "texture.h"
#include "ww3d.h"
#include "IRenderBackend.h"

void TextureClass::Prepare_Render2D_Texture()
{
    if (!Initialized) Init();
    LastAccessed = WW3D::Get_Sync_Time();
}

Texture2DResidency &TextureClass::Get_Render2D_Residency()
{
    return Render2DResidency.Get_Or_Create([this]() {
        return WW3D::Get_Render_Backend()->Create_Render2D_Residency(*this);
    });
}

void TextureClass::Apply_Render2D_Texture(unsigned stage)
{
    WW3D::Get_Render_Backend()->Apply_Render2D_Texture(*this, stage);
}

TextureClass::TextureClass(const Texture2DCPUContent &content)
    : TextureBaseClass(content.Describe().width, content.Describe().height,
                       MIP_LEVELS_1, POOL_MANAGED, false, false),
      TextureFormat(content.Describe().format), Filter(MIP_LEVELS_1)
{
    Initialized = true;
    IsProcedural = true;
    IsReducible = false;
    Render2DUsed = true;
    Render2DCPUContent = content.Snapshot();
    LastAccessed = WW3D::Get_Sync_Time();
}

TextureClass::~TextureClass() { Discard_Render2D_CPU_Content(); }

void TextureClass::Discard_Render2D_CPU_Content()
{
    if (Render2DCPUContent) Render2DCPUContent->Release_Ref();
    Render2DCPUContent = nullptr;
}

void TextureClass::Install_Render2D_File_Content(const Texture2DCPUContent &content)
{
    auto *snapshot = content.Snapshot();
    Release_D3D_Storage();
    Discard_Render2D_CPU_Content();
    Render2DCPUContent = snapshot;
    const auto d = content.Describe();
    Width = d.width; Height = d.height; TextureFormat = d.format;
    Initialized = true; Render2DUsed = true;
    LastAccessed = WW3D::Get_Sync_Time();
}
