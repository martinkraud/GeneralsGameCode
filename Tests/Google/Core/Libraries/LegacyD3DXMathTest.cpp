// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "WWMath/LegacyD3DXMath.h"
#include "LegacyD3DXMathCases.h"
#include <limits>
#ifdef LEGACY_MATH_LIVE_REFERENCE
extern "C" int WINAPI D3DXCpuOptimizations(BOOL);
#endif

namespace {
struct Native {
    static auto Multiply(D3DXMATRIX *o,const D3DXMATRIX *a,const D3DXMATRIX *b) { return LegacyD3DX::MatrixMultiply(o,a,b); }
    static auto Transpose(D3DXMATRIX *o,const D3DXMATRIX *a) { return LegacyD3DX::MatrixTranspose(o,a); }
    static auto Scaling(D3DXMATRIX *o,float x,float y,float z) { return LegacyD3DX::MatrixScaling(o,x,y,z); }
    static auto Translation(D3DXMATRIX *o,float x,float y,float z) { return LegacyD3DX::MatrixTranslation(o,x,y,z); }
    static auto Rotation(D3DXMATRIX *o,float a) { return LegacyD3DX::MatrixRotationZ(o,a); }
    static auto Vec4(D3DXVECTOR4 *o,const D3DXVECTOR4 *v,const D3DXMATRIX *m) { return LegacyD3DX::Vec4Transform(o,v,m); }
    static auto Vec3(D3DXVECTOR4 *o,const D3DXVECTOR3 *v,const D3DXMATRIX *m) { return LegacyD3DX::Vec3Transform(o,v,m); }
    static auto Inverse(D3DXMATRIX *o,float *d,const D3DXMATRIX *m) { return LegacyD3DX::MatrixInverse(o,d,m); }
};
#include "LegacyD3DXMathReference.inc"
template<class T> void SameBits(const T &a, const T &b) { EXPECT_EQ(std::memcmp(&a, &b, sizeof(T)), 0); }
template<class API> void Aliases()
{
    for (unsigned i = 0; i < 32; ++i) {
        const auto c = MathCases::Input(i); D3DXMATRIX expected, a = c.a, b = c.b;
        EXPECT_EQ(API::Multiply(&expected, &a, &b), &expected);
        EXPECT_EQ(API::Multiply(&a, &a, &b), &a); SameBits(a, expected);
        a = c.a; EXPECT_EQ(API::Multiply(&b, &a, &b), &b); SameBits(b, expected);
        a = c.a; API::Multiply(&expected, &a, &a); API::Multiply(&a, &a, &a); SameBits(a, expected);
        a = c.a; API::Transpose(&expected, &a); API::Transpose(&a, &a); SameBits(a, expected);
        D3DXVECTOR4 v = c.v, transformed; API::Vec4(&transformed, &v, &c.a);
        API::Vec4(&v, &v, &c.a); SameBits(v, transformed);
        // Vec3 input shares the first twelve bytes of a sixteen-byte output.
        D3DXVECTOR4 storage = c.v; D3DXVECTOR3 v3(c.v.x,c.v.y,c.v.z);
        API::Vec3(&transformed, &v3, &c.a);
        API::Vec3(&storage, reinterpret_cast<const D3DXVECTOR3 *>(&storage), &c.a); SameBits(storage, transformed);
        a = c.a; expected = c.a; float d1 = 123, d2 = 123;
        const auto *r1 = API::Inverse(&expected, &d1, &c.a);
        const auto *r2 = API::Inverse(&a, &d2, &a);
        EXPECT_EQ(r1 != nullptr, r2 != nullptr); SameBits(a, expected); SameBits(d1,d2);
        b = c.a; const auto *r3 = API::Inverse(&b, nullptr, &c.a);
        EXPECT_EQ(r1 != nullptr, r3 != nullptr); SameBits(b,expected);
    }
}
}
TEST(LegacyD3DXMath, RecordedWin32BitsForAllEightOperations)
{
    size_t at = 0;
    for (unsigned i = 0; i < 32; ++i) {
        SCOPED_TRACE(i); const auto words = MathCases::Output<Native>(i);
        for (auto word : words) { ASSERT_LT(at, std::size(reference)); EXPECT_EQ(word, reference[at]) << "word " << at; ++at; }
    }
    EXPECT_EQ(at, std::size(reference));
}
TEST(LegacyD3DXMath, EveryActualPointOrientationMatchesWin32Bits)
{
    for (unsigned i = 0; i < 256; ++i) {
        D3DXMATRIX m; Native::Rotation(&m,MathCases::Input(i).angle);
        EXPECT_EQ(MathCases::Bits(m._11),rotations[i*2]) << i;
        EXPECT_EQ(MathCases::Bits(m._12),rotations[i*2+1]) << i;
    }
}
TEST(LegacyD3DXMath, AliasingAndReturnPointers) { Aliases<Native>(); }
TEST(LegacyD3DXMath, InverseFailureDoesNotMutateEitherOutput)
{
    for (float value : {0.0f, -0.0f, std::numeric_limits<float>::quiet_NaN()}) {
        auto c = MathCases::Input(0); c.a._11 = value; D3DXMATRIX out; std::memset(&out,0x55,sizeof(out));
        const auto original = out; float det = 123;
        EXPECT_EQ(Native::Inverse(&out,&det,&c.a),nullptr); SameBits(out,original); EXPECT_EQ(det,123);
    }
    auto c = MathCases::Input(5); D3DXMATRIX out; float det = 0;
    EXPECT_EQ(Native::Inverse(&out,&det,&c.a),&out); EXPECT_NE(det,0); // no epsilon singular cutoff
}
TEST(LegacyD3DXMath, RowVectorTranslationAndComposition)
{
    D3DXMATRIX scale,move; Native::Scaling(&scale,2,3,4); Native::Translation(&move,5,6,7);
    const auto composed=LegacyD3DX::Multiply(scale,move);
    D3DXVECTOR4 v(1,2,3,1),out; Native::Vec4(&out,&v,&composed);
    EXPECT_EQ(out.x,7); EXPECT_EQ(out.y,12); EXPECT_EQ(out.z,19); EXPECT_EQ(out.w,1);
}
#ifdef LEGACY_MATH_LIVE_REFERENCE
TEST(LegacyD3DXMath, LivePinnedWin32Reference4096Cases)
{
    ASSERT_EQ(D3DXCpuOptimizations(TRUE),2) << "Live oracle requires the audited default SSE2 dispatch";
    for(unsigned i=0;i<4096;++i) { SCOPED_TRACE(i); EXPECT_EQ(MathCases::Output<Native>(i),MathCases::Output<MathCases::Reference>(i)); }
}
TEST(LegacyD3DXMath, LiveReferenceAliases) { Aliases<MathCases::Reference>(); }
TEST(LegacyD3DXMath, LiveBezierBasis4096Vectors)
{
    const auto basis=MathCases::Input(6).a;
    for(unsigned i=0;i<4096;++i) {
        const auto c=MathCases::Input(i); D3DXVECTOR4 oldResult,newResult;
        Native::Vec4(&newResult,&c.v,&basis); MathCases::Reference::Vec4(&oldResult,&c.v,&basis); SameBits(oldResult,newResult);
    }
}
#endif
