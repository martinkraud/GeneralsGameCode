// SPDX-License-Identifier: GPL-3.0-or-later
#include "Render2DSubmission.h"
#include "WWMath/vector2.h"
#include <cstring>

void Write_Render2D_Vertices(const Render2DSubmission &submission, const Render2DVertexMapping &mapping)
{
    unsigned char *destination = mapping.data;
    for (int i = 0; i < submission.vertex_count; ++i)
    {
        const float position[] = {submission.positions[i].X, submission.positions[i].Y, submission.z};
        const unsigned int diffuse = submission.colors[i];
        const float uv[] = {submission.uvs[i].X, submission.uvs[i].Y};
        std::memcpy(destination + mapping.position_offset, position, sizeof(position));
        std::memcpy(destination + mapping.diffuse_offset, &diffuse, sizeof(diffuse));
        std::memcpy(destination + mapping.uv_offset, uv, sizeof(uv));
        destination += mapping.stride;
    }
}
