// SPDX-License-Identifier: GPL-3.0-or-later
#include "LegacyD3DXMath.h"
#include <cmath>
#include <xmmintrin.h> // Scalar reciprocal only: retain the legacy RCPSs seed.

static_assert(sizeof(D3DXMATRIX) == 64 && sizeof(D3DXVECTOR4) == 16 && sizeof(D3DXVECTOR3) == 12,
              "Legacy CPU math layouts");
namespace LegacyD3DX {
D3DXMATRIX *MatrixMultiply(D3DXMATRIX *out, const D3DXMATRIX *a, const D3DXMATRIX *b)
{
    D3DXMATRIX result;
    for (unsigned r = 0; r < 4; ++r) for (unsigned c = 0; c < 4; ++c)
        result(r,c) = ((a->m[r][0] * b->m[0][c] + a->m[r][1] * b->m[1][c]) +
                       a->m[r][2] * b->m[2][c]) + a->m[r][3] * b->m[3][c];
    *out = result; return out;
}
D3DXMATRIX Multiply(const D3DXMATRIX &a, const D3DXMATRIX &b)
{ D3DXMATRIX result; MatrixMultiply(&result, &a, &b); return result; }
D3DXMATRIX *MatrixTranspose(D3DXMATRIX *out, const D3DXMATRIX *in)
{
    D3DXMATRIX result;
    for (unsigned r = 0; r < 4; ++r) for (unsigned c = 0; c < 4; ++c) result(r,c) = in->m[c][r];
    *out = result; return out;
}
D3DXMATRIX *MatrixScaling(D3DXMATRIX *out, float x, float y, float z)
{
    const D3DXMATRIX result(x,0,0,0, 0,y,0,0, 0,0,z,0, 0,0,0,1);
    *out = result; return out;
}
D3DXMATRIX *MatrixTranslation(D3DXMATRIX *out, float x, float y, float z)
{
    const D3DXMATRIX result(1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1);
    *out = result; return out;
}
D3DXMATRIX *MatrixRotationZ(D3DXMATRIX *out, float angle)
{
    // The reference uses x87 FSINCOS. Double CRT evaluation rounds identically
    // for the audited 256 point-orientation angles; do not substitute sinf/cosf.
    const float s = static_cast<float>(std::sin(static_cast<double>(angle)));
    const float c = static_cast<float>(std::cos(static_cast<double>(angle)));
    const D3DXMATRIX result(c,s,0,0, -s,c,0,0, 0,0,1,0, 0,0,0,1);
    *out = result; return out;
}
D3DXVECTOR4 *Vec4Transform(D3DXVECTOR4 *out, const D3DXVECTOR4 *v, const D3DXMATRIX *m)
{
    D3DXVECTOR4 result;
    for (unsigned c = 0; c < 4; ++c)
        result[c] = (v->x * m->m[0][c] + v->y * m->m[1][c]) +
                    (v->z * m->m[2][c] + v->w * m->m[3][c]);
    *out = result; return out;
}
D3DXVECTOR4 *Vec3Transform(D3DXVECTOR4 *out, const D3DXVECTOR3 *v, const D3DXMATRIX *m)
{
    D3DXVECTOR4 result;
    for (unsigned c = 0; c < 4; ++c)
        result[c] = (v->x * m->m[0][c] + v->y * m->m[1][c]) +
                    (v->z * m->m[2][c] + m->m[3][c]);
    *out = result; return out;
}
D3DXMATRIX *MatrixInverse(D3DXMATRIX *out, float *determinant, const D3DXMATRIX *input)
{
    // Scalar expansion of the pinned SSE2 cofactor sequence, preserving each
    // rounded product/sum and determinant reduction. Not WWMath::Inverse or
    // Gaussian elimination. All input is consumed before either output writes.
    const auto &m = *input;
    const float c0 = m.m[2][3] * m.m[0][2];
    const float c1 = m.m[3][3] * m.m[1][2];
    const float c2 = m.m[0][3] * m.m[2][2];
    const float c3 = m.m[1][3] * m.m[3][2];
    const float c4 = m.m[2][1] * c3;
    const float c5 = m.m[3][1] * c2;
    const float c6 = m.m[0][1] * c1;
    const float c7 = m.m[1][1] * c0;
    const float c8 = c3 * m.m[0][0];
    const float c9 = c2 * m.m[1][0];
    const float c10 = c1 * m.m[2][0];
    const float c11 = c0 * m.m[3][0];
    const float c12 = m.m[2][1] * c1;
    const float c13 = m.m[3][1] * c0;
    const float c14 = m.m[0][1] * c3;
    const float c15 = m.m[1][1] * c2;
    const float c16 = c1 * m.m[0][0];
    const float c17 = c0 * m.m[1][0];
    const float c18 = c3 * m.m[2][0];
    const float c19 = c2 * m.m[3][0];
    const float c20 = c4 - c12;
    const float c21 = c5 - c13;
    const float c22 = c6 - c14;
    const float c23 = c7 - c15;
    const float c24 = m.m[2][1] * m.m[0][2];
    const float c25 = m.m[3][1] * m.m[1][2];
    const float c26 = m.m[0][1] * m.m[2][2];
    const float c27 = m.m[1][1] * m.m[3][2];
    const float c28 = m.m[2][3] * c25;
    const float c29 = m.m[3][3] * c24;
    const float c30 = m.m[0][3] * c27;
    const float c31 = m.m[1][3] * c26;
    const float c32 = c28 + c20;
    const float c33 = c29 + c21;
    const float c34 = c30 + c22;
    const float c35 = c31 + c23;
    const float c36 = c25 * m.m[0][0];
    const float c37 = c24 * m.m[1][0];
    const float c38 = c27 * m.m[2][0];
    const float c39 = c26 * m.m[3][0];
    const float c40 = m.m[2][3] * c27;
    const float c41 = m.m[3][3] * c26;
    const float c42 = m.m[0][3] * c25;
    const float c43 = m.m[1][3] * c24;
    const float c44 = c27 * m.m[0][0];
    const float c45 = c26 * m.m[1][0];
    const float c46 = c25 * m.m[2][0];
    const float c47 = c24 * m.m[3][0];
    const float c48 = c32 - c40;
    const float c49 = c33 - c41;
    const float c50 = c34 - c42;
    const float c51 = c35 - c43;
    const float c52 = m.m[0][1] * m.m[2][3];
    const float c53 = m.m[1][1] * m.m[3][3];
    const float c54 = m.m[2][1] * m.m[0][3];
    const float c55 = m.m[3][1] * m.m[1][3];
    const float c56 = m.m[2][2] * c53;
    const float c57 = m.m[3][2] * c52;
    const float c58 = m.m[0][2] * c55;
    const float c59 = m.m[1][2] * c54;
    const float c60 = c56 + c48;
    const float c61 = c57 + c49;
    const float c62 = c58 + c50;
    const float c63 = c59 + c51;
    const float c64 = c53 * m.m[0][0];
    const float c65 = c52 * m.m[1][0];
    const float c66 = c55 * m.m[2][0];
    const float c67 = c54 * m.m[3][0];
    const float c68 = m.m[2][2] * c55;
    const float c69 = m.m[3][2] * c54;
    const float c70 = m.m[0][2] * c53;
    const float c71 = m.m[1][2] * c52;
    const float c72 = c55 * m.m[0][0];
    const float c73 = c54 * m.m[1][0];
    const float c74 = c53 * m.m[2][0];
    const float c75 = c52 * m.m[3][0];
    const float c76 = c60 - c68;
    const float c77 = c61 - c69;
    const float c78 = c62 - c70;
    const float c79 = c63 - c71;
    const float c80 = c72 - c64;
    const float c81 = c73 - c65;
    const float c82 = c74 - c66;
    const float c83 = c75 - c67;
    const float c84 = m.m[0][0] * c76;
    const float c85 = m.m[1][0] * c77;
    const float c86 = m.m[2][0] * c78;
    const float c87 = m.m[3][0] * c79;
    const float c88 = c86 + c84;
    const float c89 = c87 + c85;
    const float c90 = c84 + c86;
    const float c91 = c85 + c87;
    const float c92 = c89 + c88;
    const float c93 = m.m[0][0] * m.m[2][1];
    const float c94 = m.m[1][0] * m.m[3][1];
    const float c95 = m.m[2][0] * m.m[0][1];
    const float c96 = m.m[3][0] * m.m[1][1];
    const float c97 = m.m[2][3] * c94;
    const float c98 = m.m[3][3] * c93;
    const float c99 = m.m[0][3] * c96;
    const float c100 = m.m[1][3] * c95;
    const float c101 = c97 + c82;
    const float c102 = c98 + c83;
    const float c103 = c99 + c80;
    const float c104 = c100 + c81;
    const float c105 = m.m[2][3] * c96;
    const float c106 = m.m[3][3] * c95;
    const float c107 = m.m[0][3] * c94;
    const float c108 = m.m[1][3] * c93;
    const float c109 = c105 - c101;
    const float c110 = c106 - c102;
    const float c111 = c107 - c103;
    const float c112 = c108 - c104;
    const float c113 = m.m[2][3] * m.m[0][0];
    const float c114 = m.m[3][3] * m.m[1][0];
    const float c115 = m.m[0][3] * m.m[2][0];
    const float c116 = m.m[1][3] * m.m[3][0];
    const float c117 = m.m[2][1] * c114;
    const float c118 = m.m[3][1] * c113;
    const float c119 = m.m[0][1] * c116;
    const float c120 = m.m[1][1] * c115;
    const float c121 = c117 + c109;
    const float c122 = c118 + c110;
    const float c123 = c119 + c111;
    const float c124 = c120 + c112;
    const float c125 = m.m[2][1] * c116;
    const float c126 = m.m[3][1] * c115;
    const float c127 = m.m[0][1] * c114;
    const float c128 = m.m[1][1] * c113;
    const float c129 = c121 - c125;
    const float c130 = c122 - c126;
    const float c131 = c123 - c127;
    const float c132 = c124 - c128;
    const float c133 = c8 - c16;
    const float c134 = c9 - c17;
    const float c135 = c10 - c18;
    const float c136 = c11 - c19;
    const float c137 = c114 * m.m[2][2];
    const float c138 = c113 * m.m[3][2];
    const float c139 = c116 * m.m[0][2];
    const float c140 = c115 * m.m[1][2];
    const float c141 = c116 * m.m[2][2];
    const float c142 = c115 * m.m[3][2];
    const float c143 = c114 * m.m[0][2];
    const float c144 = c113 * m.m[1][2];
    const float c145 = c135 - c137;
    const float c146 = c136 - c138;
    const float c147 = c133 - c139;
    const float c148 = c134 - c140;
    const float c149 = m.m[0][0] * m.m[2][2];
    const float c150 = m.m[1][0] * m.m[3][2];
    const float c151 = m.m[2][0] * m.m[0][2];
    const float c152 = m.m[3][0] * m.m[1][2];
    const float c153 = c145 + c141;
    const float c154 = c146 + c142;
    const float c155 = c147 + c143;
    const float c156 = c148 + c144;
    const float c157 = m.m[2][3] * c150;
    const float c158 = m.m[3][3] * c149;
    const float c159 = m.m[0][3] * c152;
    const float c160 = m.m[1][3] * c151;
    const float c161 = m.m[2][3] * c152;
    const float c162 = m.m[3][3] * c151;
    const float c163 = m.m[0][3] * c150;
    const float c164 = m.m[1][3] * c149;
    const float c165 = c153 + c157;
    const float c166 = c154 + c158;
    const float c167 = c155 + c159;
    const float c168 = c156 + c160;
    const float c169 = c165 - c161;
    const float c170 = c166 - c162;
    const float c171 = c167 - c163;
    const float c172 = c168 - c164;
    const float c173 = c44 - c36;
    const float c174 = c45 - c37;
    const float c175 = c46 - c38;
    const float c176 = c47 - c39;
    const float c177 = c94 * m.m[2][2];
    const float c178 = c93 * m.m[3][2];
    const float c179 = c96 * m.m[0][2];
    const float c180 = c95 * m.m[1][2];
    const float c181 = c177 - c175;
    const float c182 = c178 - c176;
    const float c183 = c179 - c173;
    const float c184 = c180 - c174;
    const float c185 = m.m[2][2] * c96;
    const float c186 = m.m[3][2] * c95;
    const float c187 = m.m[0][2] * c94;
    const float c188 = m.m[1][2] * c93;
    const float c189 = c181 - c185;
    const float c190 = c182 - c186;
    const float c191 = c183 - c187;
    const float c192 = c184 - c188;
    const float c193 = c150 * m.m[2][1];
    const float c194 = c149 * m.m[3][1];
    const float c195 = c152 * m.m[0][1];
    const float c196 = c151 * m.m[1][1];
    const float c197 = m.m[2][1] * c152;
    const float c198 = m.m[3][1] * c151;
    const float c199 = m.m[0][1] * c150;
    const float c200 = m.m[1][1] * c149;
    const float c201 = c189 - c193;
    const float c202 = c190 - c194;
    const float c203 = c191 - c195;
    const float c204 = c192 - c196;
    const float c205 = c197 + c201;
    const float c206 = c198 + c202;
    const float c207 = c199 + c203;
    const float c208 = c200 + c204;
    const float det = c92;
    // COMISS/JE in the reference also rejects unordered determinants. Failure
    // leaves BOTH output and optional determinant untouched (including zero).
    if (det == 0.0f || std::isnan(det)) return nullptr;
    const float seed = _mm_cvtss_f32(_mm_rcp_ss(_mm_set_ss(det)));
    const float square = seed * seed;
    const float product = det * square;
    const float twice = seed + seed;
    const float reciprocal = twice - product;
    D3DXMATRIX result;
    result(0,0) = c76 * reciprocal;
    result(0,1) = c77 * reciprocal;
    result(0,2) = c78 * reciprocal;
    result(0,3) = c79 * reciprocal;
    result(1,0) = c169 * reciprocal;
    result(1,1) = c170 * reciprocal;
    result(1,2) = c171 * reciprocal;
    result(1,3) = c172 * reciprocal;
    result(2,0) = c129 * reciprocal;
    result(2,1) = c130 * reciprocal;
    result(2,2) = c131 * reciprocal;
    result(2,3) = c132 * reciprocal;
    result(3,0) = c205 * reciprocal;
    result(3,1) = c206 * reciprocal;
    result(3,2) = c207 * reciprocal;
    result(3,3) = c208 * reciprocal;
    if (determinant) *determinant = det;
    *out = result; return out;
}
}
