// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// TEST ONLY. Numerical decisions derived from the pinned encoder, with no
// D3DX calls, fixture lookup, or production entry point. Win32/MSVC only.
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <float.h>
#if !defined(_MSC_VER) || !defined(_M_IX86)
#error This numerical oracle checker is restricted to the Win32 characterization target.
#endif

namespace ResourceEncoderModel {
// MSVC long double is binary64. Preserve x87 register intermediates explicitly
// in 80-bit storage instead; F() alone denotes an original binary32 store.
// Arithmetic obeys the caller's x87 precision and the current rounding scope.
struct E {
    unsigned char bits[10];
    E(double v=0) {
        unsigned char *p=bits;
        __asm { mov eax,p
            fld qword ptr v
            fstp tbyte ptr [eax] }
    }
    float F() const {
        const unsigned char *p=bits;float v;
        __asm { mov eax,p
            fld tbyte ptr [eax]
            fstp dword ptr v }
        return v;
    }
    int I() const {
        const unsigned char *p=bits;int v;
        __asm { mov eax,p
            fld tbyte ptr [eax]
            fistp dword ptr v }
        return v;
    }
};
inline E operator+(const E &a,const E &b) {
    const void *pa=a.bits,*pb=b.bits;E r;void *pr=r.bits;
    __asm { mov eax,pa
        mov edx,pb
        mov ecx,pr
        fld tbyte ptr [eax]
        fld tbyte ptr [edx]
        faddp st(1),st(0)
        fstp tbyte ptr [ecx] }
    return r;
}
inline E operator-(const E &a,const E &b) {
    const void *pa=a.bits,*pb=b.bits;E r;void *pr=r.bits;
    __asm { mov eax,pa
        mov edx,pb
        mov ecx,pr
        fld tbyte ptr [eax]
        fld tbyte ptr [edx]
        fsubp st(1),st(0)
        fstp tbyte ptr [ecx] }
    return r;
}
inline E operator*(const E &a,const E &b) {
    const void *pa=a.bits,*pb=b.bits;E r;void *pr=r.bits;
    __asm { mov eax,pa
        mov edx,pb
        mov ecx,pr
        fld tbyte ptr [eax]
        fld tbyte ptr [edx]
        fmulp st(1),st(0)
        fstp tbyte ptr [ecx] }
    return r;
}
inline E operator/(const E &a,const E &b) {
    const void *pa=a.bits,*pb=b.bits;E r;void *pr=r.bits;
    __asm { mov eax,pa
        mov edx,pb
        mov ecx,pr
        fld tbyte ptr [eax]
        fld tbyte ptr [edx]
        fdivp st(1),st(0)
        fstp tbyte ptr [ecx] }
    return r;
}
inline unsigned Compare(const E &a,const E &b) {
    const void *pa=a.bits,*pb=b.bits;unsigned short status;
    __asm { mov eax,pa
        mov edx,pb
        fld tbyte ptr [edx]
        fld tbyte ptr [eax]
        fcompp
        fnstsw ax
        mov status,ax }
    return status&0x4500;
}
inline bool operator<(const E &a,const E &b){return Compare(a,b)==0x100;}
inline bool operator>(const E &a,const E &b){return Compare(a,b)==0;}
inline bool operator==(const E &a,const E &b){return Compare(a,b)==0x4000;}
inline bool operator<=(const E &a,const E &b){return a<b || a==b;}
inline bool operator>=(const E &a,const E &b){return a>b || a==b;}
struct Truncate {
    unsigned previous=0;
    Truncate(){unsigned ignored;_controlfp_s(&previous,0,0);_controlfp_s(&ignored,_RC_CHOP,_MCW_RC);}
    ~Truncate(){unsigned ignored;_controlfp_s(&ignored,previous,_MCW_RC);}
};
inline float Word(std::uint32_t word){float v;std::memcpy(&v,&word,4);return v;}
inline float Clamp(float v){return std::max(0.0f,std::min(1.0f,v));}
inline void Diffuse(float *errors,unsigned i,const E &residual) {
    auto add=[&](unsigned n,double w){errors[n]=(E(w)*residual+E(errors[n])).F();};
    if(i%4!=3)add(i+1,7.0/16);
    if(i<12){if(i%4)add(i+3,3.0/16);add(i+4,5.0/16);if(i%4!=3)add(i+5,1.0/16);}
}
struct AlphaFit {float lo,hi;unsigned iterations=0;};
// conventionalCoefficient is a deliberate negative-control mutation only.
inline AlphaFit OptimizeAlpha(const float *values,unsigned steps,bool conventionalCoefficient=false) {
    const float c8[]={1,Word(0x3f5b6db7),Word(0x3f36db6e),Word(0x3f124925),Word(0x3edb6db7),Word(0x3e924925),Word(0x3e124925),0};
    const float d8[]={0,Word(0x3e124925),Word(0x3e924925),Word(0x3edb6db7),Word(0x3f124925),Word(0x3f36db6e),Word(0x3f5b6db7),conventionalCoefficient?1.0f:Word(0x3f924925)};
    const float c6[]={1,0.8f,0.6f,0.4f,0.2f,0},d6[]={0,0.2f,0.4f,0.6f,0.8f,1};
    const float *c=steps==6?c6:c8,*d=steps==6?d6:d8;
    AlphaFit result{1,0};
    for(unsigned i=0;i<16;++i){float v=values[i];
        if(v<result.lo && (steps==8 || v>0))result.lo=v;
        if(v>result.hi && (steps==8 || v<1))result.hi=v;
    }
    {
        Truncate rounding;
        for(unsigned iteration=0;iteration<8;++iteration) {
            if(E(result.hi)-E(result.lo)<E(1.0/256))break;
            ++result.iterations;
            float scale=(E(steps-1)/(E(result.hi)-E(result.lo))).F();
            float palette[8]={};for(unsigned n=0;n<steps;++n)
                palette[n]=(E(result.hi)*E(d[n])+E(result.lo)*E(c[n])).F();
            float dx=0,dy=0,ddx=0,ddy=0;
            for(unsigned i=0;i<16;++i) {
                E projection=(E(values[i])-E(result.lo))*E(scale);unsigned n;
                if(projection<=E(0)) {
                    if(steps==6 && E(result.lo)*E(0.5)>=E(values[i]))continue;
                    n=0;
                } else if(projection>=E(steps-1)) {
                    if(steps==6 && (E(result.hi)+E(1))*E(0.5)<=E(values[i]))continue;
                    n=steps-1;
                } else n=unsigned(E((projection+E(0.5)).F()).I());
                E delta=E(values[i])-E(palette[n]);
                dx=(delta*E(c[n])+E(dx)).F();ddx=(E(c[n])*E(c[n])+E(ddx)).F();
                dy=(delta*E(d[n])+E(dy)).F();ddy=(E(d[n])*E(d[n])+E(ddy)).F();
            }
            if(ddx>0)result.lo=(E(result.lo)-E(dx)/E(ddx)).F();
            if(ddy>0)result.hi=(E(result.hi)-E(dy)/E(ddy)).F();
            if(result.lo>result.hi)std::swap(result.lo,result.hi);
            if(E(dx)*E(dx)<=E(1.0/64) && E(dy)*E(dy)<=E(1.0/64))break;
        }
    }
    result.lo=Clamp(result.lo);result.hi=Clamp(result.hi);return result;
}
struct AlphaResult {std::array<unsigned char,8> bytes{};unsigned steps=0,iterations=0;};
inline AlphaResult Alpha5(const D3DXCOLOR *input,bool conventionalCoefficient=false) {
    AlphaResult result;float values[16],errors[16]={};float lo=input[0].a,hi=lo;
    {
        Truncate rounding;
        for(unsigned i=0;i<16;++i) {
            E adjusted=E(errors[i])+E(input[i].a);float saved=adjusted.F();
            int q=E((adjusted*E(255)+E(0.5)).F()).I();E value=E(q)*E(Word(0x3b808081));
            values[i]=value.F();if(value<E(lo))lo=value.F();else if(value>E(hi))hi=value.F();
            Diffuse(errors,i,E(saved)-value);
        }
    }
    if(lo==1){result.bytes[0]=result.bytes[1]=255;return result;}
    unsigned steps=(lo!=0 && hi!=1)?8:6;result.steps=steps;
    auto fit=OptimizeAlpha(values,steps,conventionalCoefficient);result.iterations=fit.iterations;
    int a,b;
    {Truncate rounding;a=E((E(fit.lo)*E(255)+E(0.5)).F()).I();b=E((E(fit.hi)*E(255)+E(0.5)).F()).I();}
    E x=E(a&255)*E(Word(0x3b808081)),y=E(b&255)*E(Word(0x3b808081));
    if(steps==8 && (a&255)==(b&255)){result.bytes[0]=result.bytes[1]=static_cast<unsigned char>(a);return result;}
    float palette[8]={},low,high;
    const unsigned map8[]={0,2,3,4,5,6,7,1},map6[]={0,2,3,4,5,1};
    if(steps==6) {
        result.bytes[0]=static_cast<unsigned char>(a);result.bytes[1]=static_cast<unsigned char>(b);
        low=x.F();high=y.F();palette[0]=low;palette[1]=high;
        for(unsigned i=1;i<5;++i)palette[i+1]=((E(5-i)*E(low)+E(i)*E(high))*E(0.2f)).F();
        palette[6]=0;palette[7]=1;
    } else {
        result.bytes[0]=static_cast<unsigned char>(b);result.bytes[1]=static_cast<unsigned char>(a);
        // These are projection start/end, not numerical min/max: eight-step
        // selectors run from the stored high endpoint towards the low one.
        low=y.F();high=x.F();palette[0]=low;palette[1]=high;
        for(unsigned i=1;i<7;++i)palette[i+1]=((E(7-i)*E(low)+E(i)*E(high))*E(Word(0x3e124925))).F();
    }
    float scale=low==high?0:(E(steps-1)/(E(high)-E(low))).F();
    std::fill(std::begin(errors),std::end(errors),0.0f);std::uint64_t selectors=0;
    {
        Truncate rounding;
        for(unsigned i=0;i<16;++i) {
            E adjusted=E(errors[i])+E(input[i].a);float saved=adjusted.F();
            E projection=(adjusted-E(low))*E(scale);unsigned n;
            if(projection<=E(0))n=steps==6 && E(low)*E(0.5)>=E(saved)?6:0;
            else if(projection>=E(steps-1))n=steps==6 && (E(high)+E(1))*E(0.5)<=E(saved)?7:1;
            else {unsigned index=unsigned(E((projection+E(0.5)).F()).I());n=(steps==6?map6:map8)[index];}
            selectors|=std::uint64_t(n)<<(3*i);Diffuse(errors,i,E(saved)-E(palette[n]));
        }
    }
    for(unsigned i=0;i<6;++i)result.bytes[i+2]=static_cast<unsigned char>(selectors>>(8*i));return result;
}

using RGB=std::array<float,3>;
inline RGB Weights(){return {Word(0x3e981530),1,Word(0x3dce6734)};}
struct RGBFit {RGB lo,hi;unsigned orientation=0,iterations=0;bool valid=true;};
inline RGBFit OptimizeRGB(const std::array<RGB,16> &input,unsigned steps) {
    RGBFit result{Weights(),{0,0,0}};
    for(const auto &p:input)for(unsigned c=0;c<3;++c){result.lo[c]=std::min(result.lo[c],p[c]);result.hi[c]=std::max(result.hi[c],p[c]);}
    E dr=(E(result.hi[0])-E(result.lo[0])).F(),dg=(E(result.hi[1])-E(result.lo[1])).F(),db=E(result.hi[2])-E(result.lo[2]);
    E length=db*db+dg*dg+dr*dr;
    if(length<E(Word(0x00800000)))return result;
    E inverse=E(1)/length;
    RGB axis{(dr*inverse).F(),(dg*inverse).F(),(db*inverse).F()},center;
    for(unsigned c=0;c<3;++c)center[c]=((E(result.hi[c])+E(result.lo[c]))*E(0.5)).F();
    E first=0;float scores[4]={};
    for(const auto &p:input) {
        E r=((E(p[0])-E(center[0]))*E(axis[0])).F();
        E g=((E(p[1])-E(center[1]))*E(axis[1])).F();
        E b=(E(p[2])-E(center[2]))*E(axis[2]);
        E q=g+b+r;first=q*q+first;
        q=g+r-b;scores[1]=(q*q+E(scores[1])).F();
        q=r-g+b;scores[2]=(q*q+E(scores[2])).F();
        q=r-g-b;scores[3]=(q*q+E(scores[3])).F();
    }
    E best=first;for(unsigned n=1;n<4;++n)if(best<E(scores[n])){best=scores[n];result.orientation=n;}
    if(result.orientation&2)std::swap(result.lo[1],result.hi[1]);
    if(result.orientation&1)std::swap(result.lo[2],result.hi[2]);
    if(length<E(Word(0x39800000)))return result;
    const float c4[]={1,Word(0x3f2aaaab),Word(0x3eaaaaab),0},d4[]={0,Word(0x3eaaaaab),Word(0x3f2aaaab),1};
    const float c3[]={1,0.5f,0},d3[]={0,0.5f,1};
    const float *c=steps==3?c3:c4,*d=steps==3?d3:d4;
    Truncate rounding;
    for(unsigned iteration=0;iteration<8;++iteration) {
        std::array<RGB,4> palette{};
        for(unsigned n=0;n<steps;++n)for(unsigned channel=0;channel<3;++channel)
            palette[n][channel]=(E(result.lo[channel])*E(c[n])+E(result.hi[channel])*E(d[n])).F();
        dr=E(result.hi[0])-E(result.lo[0]);dg=(E(result.hi[1])-E(result.lo[1])).F();db=E(result.hi[2])-E(result.lo[2]);
        length=db*db+dg*dg+dr*dr;
        if(length<E(Word(0x39800000)))break;
        ++result.iterations;inverse=E(steps-1)/length;
        axis={(dr*inverse).F(),(dg*inverse).F(),(db*inverse).F()};
        RGB dx{},dy{};float ddx=0,ddy=0;
        for(const auto &p:input) {
            E projection=((E(p[1])-E(result.lo[1]))*E(axis[1])+(E(p[0])-E(result.lo[0]))*E(axis[0]))+
                (E(p[2])-E(result.lo[2]))*E(axis[2]);
            float saved=projection.F();int n=projection>=E(steps-1)?int(steps-1):E((E(saved)+E(0.5)).F()).I();
            // The pinned optimizer has no generic lower clamp. Do not invent
            // one if a required input ever reaches an out-of-table index.
            if(n<0 || n>=int(steps)){result.valid=false;return result;}
            E residual[3]={E(palette[n][0])-E(p[0]),(E(palette[n][1])-E(p[1])).F(),E(palette[n][2])-E(p[2])};
            E cw=E(c[n])*E(0.125),dw=E(d[n])*E(0.125);
            ddx=(cw*E(c[n])+E(ddx)).F();ddy=(dw*E(d[n])+E(ddy)).F();
            for(unsigned channel=0;channel<3;++channel){dx[channel]=(cw*residual[channel]+E(dx[channel])).F();dy[channel]=(dw*residual[channel]+E(dy[channel])).F();}
        }
        if(ddx>0){E factor=E(-1)/E(ddx);for(unsigned channel=0;channel<3;++channel)result.lo[channel]=(E(dx[channel])*factor+E(result.lo[channel])).F();}
        if(ddy>0){E factor=E(-1)/E(ddy);for(unsigned channel=0;channel<3;++channel)result.hi[channel]=(E(dy[channel])*factor+E(result.hi[channel])).F();}
        bool converged=true;
        for(unsigned channel=0;channel<3;++channel)if(E(dx[channel])*E(dx[channel])>E(Word(0x37800000)) || E(dy[channel])*E(dy[channel])>E(Word(0x37800000)))converged=false;
        if(converged)break;
    }
    return result;
}
inline unsigned Encode565(const RGB &rgb) {
    Truncate rounding;unsigned fields[3];
    for(unsigned c=0;c<3;++c)fields[c]=unsigned(E((E(Clamp(rgb[c]))*E(c==1?63:31)+E(0.5)).F()).I());
    return fields[0]<<11|fields[1]<<5|fields[2];
}
inline RGB Decode565(unsigned value) {
    return {(E(value>>11)*E(Word(0x3d042108))).F(),(E((value>>5)&63)*E(Word(0x3c820821))).F(),(E(value&31)*E(Word(0x3d042108))).F()};
}
inline void ColorDiffuse(float errors[3][16],unsigned i,const E *residual) {
    for(unsigned c=0;c<3;++c) {
        auto add=[&](unsigned n,double w,const E &r){errors[c][n]=(E(w)*r+E(errors[c][n])).F();};
        // Green/blue diagonal contributions reload an explicitly stored
        // binary32 residual; red keeps its x87 intermediate.
        float saved=residual[c].F();
        if(i%4!=3)add(i+1,7.0/16,residual[c]);
        if(i<12){if(i%4)add(i+3,3.0/16,residual[c]);add(i+4,5.0/16,residual[c]);
            if(i%4!=3)add(i+5,1.0/16,c?E(saved):residual[c]);}
    }
}
struct ColorResult {std::array<unsigned char,8> bytes{};unsigned steps=0,orientation=0,iterations=0;bool valid=true;};
inline ColorResult Color(const D3DXCOLOR *input,bool alphaTest) {
    ColorResult result;unsigned transparent=0;for(unsigned i=0;i<16;++i)if(input[i].a<0.5f)++transparent;
    if(alphaTest && transparent==16){result.bytes={0,0,255,255,255,255,255,255};return result;}
    unsigned steps=alphaTest && transparent?3:4;result.steps=steps;
    RGB weights=Weights();std::array<RGB,16> quantized;float errors[3][16]={};
    {
        Truncate rounding;
        for(unsigned i=0;i<16;++i) {
            float adjusted[3];E normalized[3],residual[3];
            for(unsigned c=0;c<3;++c) {
                adjusted[c]=(E(errors[c][i])+E((&input[i].r)[c])).F();
                int q=E((E(adjusted[c])*E(c==1?63:31)+E(0.5)).F()).I();
                normalized[c]=E(q)*E(Word(c==1?0x3c820821:0x3d042108));
                if(c!=2)normalized[c]=normalized[c].F();
                residual[c]=E(adjusted[c])-normalized[c];
                quantized[i][c]=(normalized[c]*E(weights[c])).F();
            }
            ColorDiffuse(errors,i,residual);
        }
    }
    auto fit=OptimizeRGB(quantized,steps);result.valid=fit.valid;result.orientation=fit.orientation;result.iterations=fit.iterations;
    if(!fit.valid)return result;
    RGB inverse{Word(0x4057762e),1,Word(0x411ec1dd)},a,b;
    for(unsigned c=0;c<3;++c){a[c]=(E(fit.lo[c])*E(inverse[c])).F();b[c]=(E(fit.hi[c])*E(inverse[c])).F();}
    unsigned first=Encode565(a),second=Encode565(b);
    auto putEndpoints=[&](){result.bytes[0]=static_cast<unsigned char>(first);result.bytes[1]=static_cast<unsigned char>(first>>8);
        result.bytes[2]=static_cast<unsigned char>(second);result.bytes[3]=static_cast<unsigned char>(second>>8);};
    if(steps==4 && first==second){putEndpoints();return result;}
    a=Decode565(first);b=Decode565(second);
    for(unsigned c=0;c<3;++c){a[c]=(E(a[c])*E(weights[c])).F();b[c]=(E(b[c])*E(weights[c])).F();}
    if((steps==3)!=(second>=first)){std::swap(first,second);std::swap(a,b);}
    putEndpoints();std::array<RGB,4> palette{};palette[0]=a;palette[1]=b;
    E delta[3]={E(b[0])-E(a[0]),E(b[1])-E(a[1]),E(b[2])-E(a[2])};
    for(unsigned c=0;c<3;++c) {
        palette[2][c]=(delta[c]*E(steps==3?0.5f:Word(0x3eaaaaab))+E(a[c])).F();
        if(steps==4)palette[3][c]=(delta[c]*E(Word(0x3f2aaaab))+E(a[c])).F();
    }
    // The pinned denominator stores the red delta, retains green extended,
    // and reloads the stored blue delta.
    E red=delta[0].F(),green=delta[1],blue=delta[2].F();
    E scale=first==second?E(0):E(steps-1)/(green*green+blue*blue+red*red);
    RGB axis{(red*scale).F(),(green*scale).F(),(blue*scale).F()};
    for(auto &channel:errors)std::fill(std::begin(channel),std::end(channel),0.0f);
    const unsigned map3[]={0,2,1},map4[]={0,2,3,1};std::uint32_t selectors=0;
    Truncate rounding;
    for(unsigned i=0;i<16;++i) {
        if(steps==3 && input[i].a<0.5f){selectors|=3u<<(2*i);continue;}
        RGB adjusted;for(unsigned c=0;c<3;++c)adjusted[c]=(E(weights[c])*E((&input[i].r)[c])+E(errors[c][i])).F();
        E projection=((E(adjusted[0])-E(a[0]))*E(axis[0])+(E(adjusted[2])-E(a[2]))*E(axis[2]))+
            (E(adjusted[1])-E(a[1]))*E(axis[1]);
        unsigned n;
        if(projection<=E(0))n=0;else if(projection>=E(steps-1))n=1;
        else n=(steps==3?map3:map4)[E((projection+E(0.5)).F()).I()];
        selectors|=n<<(2*i);
        // The cached preconditioned alpha is exactly one for every pixel.
        E residual[3];for(unsigned c=0;c<3;++c)residual[c]=E(adjusted[c])-E(palette[n][c]);
        // Unlike the prepass, selector blue is reloaded from binary32 for all
        // neighbors. Green retains extended except on the diagonal.
        residual[2]=residual[2].F();ColorDiffuse(errors,i,residual);
    }
    for(unsigned i=0;i<4;++i)result.bytes[i+4]=static_cast<unsigned char>(selectors>>(8*i));return result;
}
}
