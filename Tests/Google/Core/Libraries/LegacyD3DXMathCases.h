// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <d3dx8math.h>
#include <cstdint>
#include <cstring>
#include <vector>

namespace MathCases {
struct Case { D3DXMATRIX a, b; D3DXVECTOR4 v; float angle; };
inline Case Input(unsigned index)
{
    Case c;
    std::uint32_t state = 0x921273u + index;
    auto value = [&]() { state = state * 1664525u + 1013904223u; return (int(state >> 16) - 32768) / 1024.0f; };
    for (unsigned r = 0; r < 4; ++r) for (unsigned k = 0; k < 4; ++k) { c.a(r,k) = value(); c.b(r,k) = value(); }
    const float x = value(), y = value(), z = value(), w = value();
    c.v = D3DXVECTOR4(x, y, z, w);
    c.angle = float(index % 256) / 255.0f * 2 * D3DX_PI;
    if (index < 7) {
        c.a = D3DXMATRIX(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
        if (index == 1) { c.a._41 = 17.125f; c.a._42 = -8.75f; c.a._43 = .03125f; }
        if (index == 2) { c.a._11 = 2; c.a._22 = -3; c.a._33 = .25f; }
        if (index == 3) { c.a._11 = 0; c.a._12 = 1; c.a._21 = -1; c.a._22 = 0; }
        if (index == 4) c.a._11 = 0;
        if (index == 5) c.a._11 = 1e-10f;
        if (index == 6) c.a = D3DXMATRIX(-1,3,-3,1, 3,-6,3,0, -3,3,0,0, 1,0,0,0);
    }
    return c;
}
inline std::uint32_t Bits(float f) { std::uint32_t u; std::memcpy(&u, &f, 4); return u; }
inline void Append(std::vector<std::uint32_t> &out, const void *p, unsigned count)
{
    const auto *bytes = static_cast<const unsigned char *>(p);
    for (unsigned i = 0; i < count; ++i) { std::uint32_t u; std::memcpy(&u, bytes + i * 4, 4); out.push_back(u); }
}
// Shared inputs/serialization only. Reference and replacement are evaluated
// independently; expected words are produced solely by the pinned x86 library.
template<class API> std::vector<std::uint32_t> Output(unsigned index)
{
    const auto c = Input(index); D3DXMATRIX out; D3DXVECTOR4 v;
    std::vector<std::uint32_t> words;
    API::Multiply(&out, &c.a, &c.b); Append(words, &out, 16);
    API::Transpose(&out, &c.a); Append(words, &out, 16);
    API::Scaling(&out, c.v.x, c.v.y, c.v.z); Append(words, &out, 16);
    API::Translation(&out, c.v.x, c.v.y, c.v.z); Append(words, &out, 16);
    API::Rotation(&out, c.angle); Append(words, &out, 16);
    API::Vec4(&v, &c.v, &c.a); Append(words, &v, 4);
    const D3DXVECTOR3 v3(c.v.x, c.v.y, c.v.z);
    API::Vec3(&v, &v3, &c.a); Append(words, &v, 4);
    std::memset(&out, 0x55, sizeof(out)); float determinant = 123;
    const auto *result = API::Inverse(&out, &determinant, &c.a);
    words.push_back(result == &out); Append(words, &out, 16); words.push_back(Bits(determinant));
    return words;
}
struct Reference {
    static auto Multiply(D3DXMATRIX *o,const D3DXMATRIX *a,const D3DXMATRIX *b) { return D3DXMatrixMultiply(o,a,b); }
    static auto Transpose(D3DXMATRIX *o,const D3DXMATRIX *a) { return D3DXMatrixTranspose(o,a); }
    static auto Scaling(D3DXMATRIX *o,float x,float y,float z) { return D3DXMatrixScaling(o,x,y,z); }
    static auto Translation(D3DXMATRIX *o,float x,float y,float z) { return D3DXMatrixTranslation(o,x,y,z); }
    static auto Rotation(D3DXMATRIX *o,float a) { return D3DXMatrixRotationZ(o,a); }
    static auto Vec4(D3DXVECTOR4 *o,const D3DXVECTOR4 *v,const D3DXMATRIX *m) { return D3DXVec4Transform(o,v,m); }
    static auto Vec3(D3DXVECTOR4 *o,const D3DXVECTOR3 *v,const D3DXMATRIX *m) { return D3DXVec3Transform(o,v,m); }
    static auto Inverse(D3DXMATRIX *o,float *d,const D3DXMATRIX *m) { return D3DXMatrixInverse(o,d,m); }
};
}
