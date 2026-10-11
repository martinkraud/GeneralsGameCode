// SPDX-License-Identifier: GPL-3.0-or-later
#include "Texture2D.h"
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
