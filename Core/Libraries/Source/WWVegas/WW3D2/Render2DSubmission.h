// SPDX-License-Identifier: GPL-3.0-or-later
// W3D's synchronous Render2D contract. No graphics SDK declarations belong here.
#pragma once

#include "IRenderBackend.h"

class TextureClass;
class ShaderClass;
class Vector2;

struct Render2DSubmission
{
    // Borrowed until Submit_Render2D returns. Texture identity is the W3D object,
    // including nullptr; sampler/texture lifetime remains owned by W3D.
    TextureClass *texture;
    const ShaderClass *shader;
    const Vector2 *positions;
    const Vector2 *uvs;
    const unsigned long *colors;
    const unsigned short *indices;
    int vertex_count;
    int index_count;
    float z;
    RenderBackendViewport viewport;
    bool grayscale;
};

// A write-only mapping of XYZ (already in clip coordinates, NOT XYZRHW),
// packed ARGB32 and UV0. Other elements/padding must remain untouched.
struct Render2DVertexMapping
{
    unsigned char *data;
    unsigned stride;
    unsigned position_offset;
    unsigned diffuse_offset;
    unsigned uv_offset;
};

void Write_Render2D_Vertices(const Render2DSubmission &, const Render2DVertexMapping &);

// Compile-time resource/command contract, shared by the reference adapter and
// recording tests. Backend-specific access objects stay stack scoped (no new
// heap allocations or virtual locks). VertexAccess/IndexAccess own temporary
// dynamic ranges; their WriteLock owns only the lock, not the access lifetime.
// Locks return a mapping/uint16 array at the allocated range's start. Access
// destruction releases IB then VB AFTER transform restoration/invalidation.
// W3D ShaderClass carries the existing neutral blend/depth/cull/combiner state.
template<class Commands>
void Execute_Render2D(Commands &commands, const Render2DSubmission &submission)
{
    if (!submission.index_count) return;

    typename Commands::SavedTransforms saved;
    commands.Save_Transforms(saved);
    commands.Set_Viewport(submission.viewport);
    commands.Set_Texture(submission.texture);
    commands.Set_Prelit_Diffuse_Material();
    commands.Set_Identity_Transforms();

    typename Commands::VertexAccess vertices(commands, static_cast<unsigned short>(submission.vertex_count));
    {
        typename Commands::VertexAccess::WriteLock lock(vertices);
        Write_Render2D_Vertices(submission, lock.Map());
    }
    typename Commands::IndexAccess indices(commands, static_cast<unsigned short>(submission.index_count));
    {
        typename Commands::IndexAccess::WriteLock lock(indices);
        unsigned short *destination = lock.Map();
        for (int i = 0; i < submission.index_count; ++i) destination[i] = submission.indices[i];
    }
    commands.Bind_Vertices(vertices);
    commands.Bind_Indices(indices, 0);

    if (submission.grayscale)
    {
        commands.Set_Opaque_Shader();
        commands.Apply_State_Changes();
        // Capability query and overrides must occur after the W3D state flush.
        commands.Set_Grayscale_Combiner(commands.Supports_Dot3());
    }
    else
    {
        commands.Set_Shader(*submission.shader);
    }
    commands.Draw_Triangles(0, static_cast<unsigned short>(submission.index_count / 3),
                            0, static_cast<unsigned short>(submission.vertex_count));
    commands.Restore_Transforms(saved);
    if (submission.grayscale) commands.Invalidate_Shader();
}
