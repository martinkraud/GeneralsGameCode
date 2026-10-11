// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "Common/BezierSegment.h"
#include "Common/BezFwdIterator.h"
#include <cstring>

namespace {
const D3DXMATRIX basis(-1,3,-3,1, 3,-6,3,0, -3,3,0,0, 1,0,0,0);
void Same(const Coord3D &a, const Coord3D &b)
{
    EXPECT_EQ(std::memcmp(&a.x,&b.x,sizeof(Real)),0);
    EXPECT_EQ(std::memcmp(&a.y,&b.y,sizeof(Real)),0);
    EXPECT_EQ(std::memcmp(&a.z,&b.z,sizeof(Real)),0);
}
}
TEST(BezierCompatibility, ProductionEvaluationMatchesPinnedLegacy)
{
    Coord3D points[4]={{-512.125f,0,17.25f},{127.75f,-230.25f,1000},{1024.5f,128.25f,250},{1500,500,-12.5f}};
    BezierSegment curve(points);
    for (unsigned i=0;i<=4096;++i) {
        Real t=i/4096.0f; D3DXVECTOR4 v(t*t*t,t*t,t,1),r;
        D3DXVec4Transform(&r,&v,&basis);
        D3DXVECTOR4 x(points[0].x,points[1].x,points[2].x,points[3].x);
        D3DXVECTOR4 y(points[0].y,points[1].y,points[2].y,points[3].y);
        D3DXVECTOR4 z(points[0].z,points[1].z,points[2].z,points[3].z);
        Coord3D expected={D3DXVec4Dot(&x,&r),D3DXVec4Dot(&y,&r),D3DXVec4Dot(&z,&r)},actual;
        curve.evaluateBezSegmentAtT(t,&actual); Same(actual,expected);
    }
}
TEST(BezierCompatibility, ProductionProjectilePointsMatchLegacyForwardDifferences)
{
    Coord3D points[4]={{-512.125f,0,17.25f},{127.75f,-230.25f,1000},{1024.5f,128.25f,250},{1500,500,-12.5f}};
    BezierSegment curve(points);
    for (Int steps : {2,3,17,64,257}) {
        VecCoord3D actual; curve.getSegmentPoints(steps,&actual); ASSERT_EQ(actual.size(),steps);
        const float d=1.0f/(steps-1),d2=d*d,d3=d*d2;
        D3DXVECTOR4 coords[3]={
            {points[0].x,points[1].x,points[2].x,points[3].x},
            {points[0].y,points[1].y,points[2].y,points[3].y},
            {points[0].z,points[1].z,points[2].z,points[3].z}};
        float dq[3],ddq[3],dddq[3];
        for (unsigned axis=0;axis<3;++axis) {
            D3DXVECTOR4 c; D3DXVec4Transform(&c,&coords[axis],&basis);
            dq[axis]=c.x*d3+c.y*d2+c.z*d;
            ddq[axis]=6*c.x*d3+2*c.y*d2;
            dddq[axis]=6*c.x*d3;
        }
        Coord3D current=points[0];
        for (Int i=0;i<steps;++i) {
            Same(actual[i],current);
            current.x+=dq[0];current.y+=dq[1];current.z+=dq[2];
            for (unsigned axis=0;axis<3;++axis) { dq[axis]+=ddq[axis];ddq[axis]+=dddq[axis]; }
        }
    }
}
