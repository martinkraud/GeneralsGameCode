// SPDX-License-Identifier: GPL-3.0-or-later
#include "DX8Backend.h"
#include "WW3D2/Render2DSubmission.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8vertexbuffer.h"
#include "WW3D2/dx8indexbuffer.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/shader.h"
#include "WWMath/matrix4.h"

namespace
{
// The only SDK-specific implementation of the Render2D command/resource
// contract. Use the SAME dynamic accessors: ring offsets, discard/nooverwrite,
// engine refs, cached bindings and destruction order remain their responsibility.
class DX8Render2DCommands
{
public:
    struct SavedTransforms { Matrix4x4 view, projection; };

    class VertexAccess : public DynamicVBAccessClass
    {
    public:
        VertexAccess(DX8Render2DCommands &, unsigned short count)
            : DynamicVBAccessClass(BUFFER_TYPE_DYNAMIC_DX8, dynamic_fvf_type, count) {}
        VertexAccess(const VertexAccess &) = delete;
        VertexAccess &operator=(const VertexAccess &) = delete;
        class WriteLock
        {
            VertexAccess &access;
            DynamicVBAccessClass::WriteLockClass lock;
        public:
            explicit WriteLock(VertexAccess &vertices) : access(vertices), lock(&vertices) {}
            Render2DVertexMapping Map()
            {
                const FVFInfoClass &layout = access.FVF_Info();
                return {reinterpret_cast<unsigned char *>(lock.Get_Formatted_Vertex_Array()),
                        layout.Get_FVF_Size(), layout.Get_Location_Offset(),
                        layout.Get_Diffuse_Offset(), layout.Get_Tex_Offset(0)};
            }
        };
    };
    class IndexAccess : public DynamicIBAccessClass
    {
    public:
        IndexAccess(DX8Render2DCommands &, unsigned short count)
            : DynamicIBAccessClass(BUFFER_TYPE_DYNAMIC_DX8, count) {}
        IndexAccess(const IndexAccess &) = delete;
        IndexAccess &operator=(const IndexAccess &) = delete;
        class WriteLock
        {
            DynamicIBAccessClass::WriteLockClass lock;
        public:
            explicit WriteLock(IndexAccess &indices) : lock(&indices) {}
            unsigned short *Map() { return lock.Get_Index_Array(); }
        };
    };

    void Save_Transforms(SavedTransforms &saved)
    {
        DX8Wrapper::Get_Transform(D3DTS_VIEW, saved.view);
        DX8Wrapper::Get_Transform(D3DTS_PROJECTION, saved.projection);
    }
    void Set_Viewport(const RenderBackendViewport &viewport)
    {
        D3DVIEWPORT8 vp = {viewport.x, viewport.y, viewport.width, viewport.height,
                          viewport.min_z, viewport.max_z};
        DX8Wrapper::Set_Viewport(&vp);
    }
    void Set_Texture(TextureClass *texture) { DX8Wrapper::Set_Texture(0, texture); }
    void Set_Prelit_Diffuse_Material()
    {
        VertexMaterialClass *material = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
        DX8Wrapper::Set_Material(material);
        REF_PTR_RELEASE(material);
    }
    void Set_Identity_Transforms()
    {
        const Matrix4x4 identity(true);
        DX8Wrapper::Set_World_Identity();
        DX8Wrapper::Set_View_Identity();
        DX8Wrapper::Set_Transform(D3DTS_PROJECTION, identity);
    }
    void Bind_Vertices(const VertexAccess &vertices) { DX8Wrapper::Set_Vertex_Buffer(vertices); }
    void Bind_Indices(const IndexAccess &indices, unsigned short base) { DX8Wrapper::Set_Index_Buffer(indices, base); }
    void Set_Shader(const ShaderClass &shader) { DX8Wrapper::Set_Shader(shader); }
    void Set_Opaque_Shader() { DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader); }
    void Apply_State_Changes() { DX8Wrapper::Apply_Render_State_Changes(); }
    bool Supports_Dot3() { return DX8Wrapper::Get_Current_Caps()->Support_Dot3(); }
    void Set_Grayscale_Combiner(bool dot3)
    {
        if (dot3)
        {
            DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR, 0x80A5CA8E);
            DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLORARG0, D3DTA_TFACTOR | D3DTA_ALPHAREPLICATE);
            DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLORARG2, D3DTA_TFACTOR | D3DTA_ALPHAREPLICATE);
            DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
            DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
            DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
            DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_COLOROP, D3DTOP_DOTPRODUCT3);
        }
        else
        {
            DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR, 0x60606060);
            DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
            DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
            DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        }
    }
    void Draw_Triangles(unsigned short start, unsigned short triangles, unsigned short minimum, unsigned short count)
    {
        DX8Wrapper::Draw_Triangles(start, triangles, minimum, count);
    }
    void Restore_Transforms(const SavedTransforms &saved)
    {
        DX8Wrapper::Set_Transform(D3DTS_VIEW, saved.view);
        DX8Wrapper::Set_Transform(D3DTS_PROJECTION, saved.projection);
    }
    void Invalidate_Shader() { ShaderClass::Invalidate(); }
};
}

void DX8Backend::Submit_Render2D(const Render2DSubmission &submission)
{
    DX8Render2DCommands commands;
    Execute_Render2D(commands, submission);
}
