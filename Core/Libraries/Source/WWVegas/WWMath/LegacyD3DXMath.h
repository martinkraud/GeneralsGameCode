// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <d3dx8math.h> // CPU layouts only; no D3D device/resource dependency.

// The pinned D3DX8 library's modern-x86 CPU arithmetic contract. This boundary
// retains the existing types/layouts; it is not a replacement math convention.
// Required pointers are non-null, as in the reference release implementation.
namespace LegacyD3DX {
D3DXMATRIX *MatrixMultiply(D3DXMATRIX *, const D3DXMATRIX *, const D3DXMATRIX *);
D3DXMATRIX Multiply(const D3DXMATRIX &, const D3DXMATRIX &);
D3DXMATRIX *MatrixTranspose(D3DXMATRIX *, const D3DXMATRIX *);
D3DXMATRIX *MatrixScaling(D3DXMATRIX *, float, float, float);
D3DXMATRIX *MatrixTranslation(D3DXMATRIX *, float, float, float);
D3DXMATRIX *MatrixRotationZ(D3DXMATRIX *, float);
D3DXMATRIX *MatrixInverse(D3DXMATRIX *, float *optionalDeterminant, const D3DXMATRIX *);
D3DXVECTOR4 *Vec3Transform(D3DXVECTOR4 *, const D3DXVECTOR3 *, const D3DXMATRIX *);
D3DXVECTOR4 *Vec4Transform(D3DXVECTOR4 *, const D3DXVECTOR4 *, const D3DXMATRIX *);
}
