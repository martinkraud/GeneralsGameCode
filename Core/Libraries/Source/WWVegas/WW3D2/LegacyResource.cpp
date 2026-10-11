// SPDX-License-Identifier: GPL-3.0-or-later
#include "LegacyResource.h"
#include "LegacyResourceEncoding.h"
#include "LegacyTextureCreation.h"
#include <vector>
#include <cmath>
#include <fstream>
#include <limits>
#include <new>

namespace LegacyD3DX {
namespace {
using ResourceEncoding::E;
unsigned Bytes(D3DFORMAT f){switch(f){
case D3DFMT_A8R8G8B8:case D3DFMT_X8R8G8B8:return 4;
case D3DFMT_R8G8B8:return 3;
case D3DFMT_R5G6B5:case D3DFMT_A1R5G5B5:case D3DFMT_A4R4G4B4:case D3DFMT_A8L8:case D3DFMT_V8U8:return 2;
case D3DFMT_L8:case D3DFMT_P8:return 1;
default:return 0;}}
bool DXT(D3DFORMAT f){return f==D3DFMT_DXT1 || f==D3DFMT_DXT3 || f==D3DFMT_DXT5;}
unsigned RowSize(D3DFORMAT f,unsigned w){return DXT(f)?((w+3)/4)*(f==D3DFMT_DXT1?8:16):Bytes(f)*w;}
unsigned Rows(D3DFORMAT f,unsigned h){return DXT(f)?(h+3)/4:h;}
unsigned Read16(const unsigned char *p){return p[0]|unsigned(p[1])<<8;}
unsigned Read32(const unsigned char *p){return Read16(p)|Read16(p+2)<<16;}
void Write16(unsigned char *p,unsigned v){p[0]=static_cast<unsigned char>(v);p[1]=static_cast<unsigned char>(v>>8);}
float Normalize(unsigned v,unsigned maximum){return (E(v)*E(float(1.0/maximum))).F();}
D3DXCOLOR Decode(const unsigned char *p,D3DFORMAT f){
    if(f==D3DFMT_A8R8G8B8 || f==D3DFMT_X8R8G8B8 || f==D3DFMT_R8G8B8)
        return {Normalize(p[2],255),Normalize(p[1],255),Normalize(p[0],255),f==D3DFMT_A8R8G8B8?Normalize(p[3],255):1.0f};
    unsigned v=Bytes(f)==1?p[0]:Read16(p);
    if(f==D3DFMT_R5G6B5)return {Normalize(v>>11,31),Normalize((v>>5)&63,63),Normalize(v&31,31),1};
    if(f==D3DFMT_A1R5G5B5)return {Normalize((v>>10)&31,31),Normalize((v>>5)&31,31),Normalize(v&31,31),float(v>>15)};
    if(f==D3DFMT_A4R4G4B4)return {Normalize((v>>8)&15,15),Normalize((v>>4)&15,15),Normalize(v&15,15),Normalize(v>>12,15)};
    if(f==D3DFMT_L8 || f==D3DFMT_A8L8){float l=Normalize(p[0],255);return {l,l,l,f==D3DFMT_A8L8?Normalize(p[1],255):1.0f};}
    if(f==D3DFMT_P8)return {1,1,1,1};
    if(f==D3DFMT_V8U8)return {float(static_cast<signed char>(p[0]))/256.0f+0.5f,float(static_cast<signed char>(p[1]))/256.0f+0.5f,0.5f,1};
    return {0,0,0,1};
}
unsigned Quantize(float v,unsigned maximum){return unsigned(E((E(ResourceEncoding::Clamp(v))*E(maximum)+E(0.5)).F()).I());}
void Encode(unsigned char *p,D3DFORMAT f,const D3DXCOLOR &v){
    ResourceEncoding::Truncate rounding;
    if(f==D3DFMT_A8R8G8B8 || f==D3DFMT_X8R8G8B8 || f==D3DFMT_R8G8B8){
        p[0]=static_cast<unsigned char>(Quantize(v.b,255));p[1]=static_cast<unsigned char>(Quantize(v.g,255));p[2]=static_cast<unsigned char>(Quantize(v.r,255));
        if(f!=D3DFMT_R8G8B8)p[3]=f==D3DFMT_A8R8G8B8?static_cast<unsigned char>(Quantize(v.a,255)):0;return;
    }
    if(f==D3DFMT_R5G6B5)Write16(p,Quantize(v.r,31)<<11|Quantize(v.g,63)<<5|Quantize(v.b,31));
    if(f==D3DFMT_A1R5G5B5)Write16(p,Quantize(v.a,1)<<15|Quantize(v.r,31)<<10|Quantize(v.g,31)<<5|Quantize(v.b,31));
    if(f==D3DFMT_A4R4G4B4)Write16(p,Quantize(v.a,15)<<12|Quantize(v.r,15)<<8|Quantize(v.g,15)<<4|Quantize(v.b,15));
    if(f==D3DFMT_L8 || f==D3DFMT_A8L8){p[0]=static_cast<unsigned char>(Quantize(v.r,255));if(f==D3DFMT_A8L8)p[1]=static_cast<unsigned char>(Quantize(v.a,255));}
}
D3DXCOLOR DecodeDXT(const unsigned char *block,D3DFORMAT f,unsigned index){
    const unsigned char *color=block+(f==D3DFMT_DXT1?0:8);
    unsigned first=Read16(color),second=Read16(color+2),selector=(Read32(color+4)>>(2*index))&3;
    auto a=ResourceEncoding::Decode565(first),b=ResourceEncoding::Decode565(second);
    D3DXCOLOR result(a[0],a[1],a[2],1);
    if(selector==1)result={b[0],b[1],b[2],1};
    else if(selector>=2){
        if(first<=second){
            if(selector==3)result={0,0,0,0};
            else for(unsigned c=0;c<3;++c)(&result.r)[c]=((E(a[c])+E(b[c]))*E(0.5)).F();
        } else for(unsigned c=0;c<3;++c){
            E delta=E(b[c])-E(a[c]);if(selector==3 && c)delta=delta.F();
            (&result.r)[c]=(delta*E(selector==2?1.0f/3:2.0f/3)+E(a[c])).F();
        }
    }
    if(f==D3DFMT_DXT3)result.a=Normalize((block[index/2]>>((index%2)*4))&15,15);
    if(f==D3DFMT_DXT5){
        std::uint64_t bits=0;for(unsigned i=0;i<6;++i)bits|=std::uint64_t(block[i+2])<<(8*i);
        unsigned n=unsigned(bits>>(3*index))&7;float x=Normalize(block[0],255),y=Normalize(block[1],255);
        if(n<2)result.a=n?y:x;
        else if(block[0]>block[1])result.a=((E(8-n)*E(x)+E(n-1)*E(y))*E(1.0f/7)).F();
        else if(n<6)result.a=((E(6-n)*E(x)+E(n-1)*E(y))*E(0.2f)).F();
        else result.a=n==6?0.0f:1.0f;
    }
    return result;
}
struct Weight {unsigned destination;float value;};
using Table=std::vector<std::vector<Weight>>;
Table Triangle(unsigned source,unsigned destination){
    Table table(source);E ratio=E(destination)/E(source);float r=ratio.F(),half=(E(0.5)/ratio).F();
    for(unsigned i=0;i<source;++i){
        unsigned previous=0;float sum=0;
        auto flush=[&](){if(sum>ResourceEncoding::Word(0x3727c5ac))table[i].push_back({previous,sum});};
        for(unsigned side=0;side<2;++side){
            float origin=(E(i+side)-E(0.5)).F();E start=E(origin)*E(r);float begin=start.F(),end=(start+E(r)).F();
            for(int bin=int(std::floor(start.D()));E(bin)<E(end);++bin){
                unsigned folded=unsigned(bin<0?bin+int(destination):(bin>=int(destination)?bin-int(destination):bin));
                if(folded!=previous){flush();sum=0;previous=folded;}
                E low=E(bin)<E(begin)?E(begin):E(bin);float high=std::min(float(bin+1),end);
                E value=(E(high)+low)*E(half)-E(origin);if(side)value=E(1)-value;
                sum=((E(high)-low)*value+E(sum)).F();
            }
        }
        flush();
    }
    return table;
}
struct Image {unsigned width,height;D3DFORMAT format;unsigned pitch;std::vector<unsigned char> bytes;};
std::vector<D3DXCOLOR> DecodeImage(const Image &src){
    std::vector<D3DXCOLOR> pixels(size_t(src.width)*src.height);
    for(unsigned y=0;y<src.height;++y)for(unsigned x=0;x<src.width;++x){
        const auto *p=src.bytes.data()+(DXT(src.format)?y/4:y)*src.pitch;
        pixels[y*src.width+x]=DXT(src.format)?DecodeDXT(p+(x/4)*(src.format==D3DFMT_DXT1?8:16),src.format,(y%4)*4+x%4):Decode(p+x*Bytes(src.format),src.format);
    }
    return pixels;
}
HRESULT Convert(const Image &src,Image &dst,DWORD filter,bool retainBlocks=true){
    unsigned w=dst.width,h=dst.height;
    if(src.format==dst.format && src.width==w && src.height==h && (!DXT(src.format) || retainBlocks)){
        for(unsigned y=0;y<Rows(dst.format,h);++y)std::memcpy(dst.bytes.data()+y*dst.pitch,src.bytes.data()+y*src.pitch,RowSize(dst.format,w));return S_OK;
    }
    if(!DXT(src.format) && !Bytes(src.format))return D3DERR_INVALIDCALL;
    if(!DXT(dst.format) && !Bytes(dst.format))return D3DERR_INVALIDCALL;
    bool half=filter==D3DX_FILTER_BOX && ((src.width/2==w)||(src.width==1 && w==1)) && ((src.height/2==h)||(src.height==1 && h==1));
    if(half && src.width>=2 && src.height>=2 && src.format==dst.format && !DXT(dst.format)){
        unsigned widths[4]={8,8,8,8},shifts[4]={16,8,0,24};
        if(dst.format==D3DFMT_R5G6B5){widths[0]=widths[2]=5;widths[1]=6;widths[3]=0;shifts[0]=11;shifts[1]=5;shifts[2]=0;}
        if(dst.format==D3DFMT_A1R5G5B5){widths[0]=widths[1]=widths[2]=5;widths[3]=1;shifts[0]=10;shifts[1]=5;shifts[2]=0;shifts[3]=15;}
        if(dst.format==D3DFMT_A4R4G4B4){for(unsigned c=0;c<4;++c)widths[c]=4;shifts[0]=8;shifts[1]=4;shifts[2]=0;shifts[3]=12;}
        if(dst.format==D3DFMT_R8G8B8)widths[3]=0;
        if(dst.format==D3DFMT_L8 || dst.format==D3DFMT_A8L8){widths[0]=8;widths[1]=widths[2]=0;widths[3]=dst.format==D3DFMT_A8L8?8:0;shifts[0]=0;shifts[3]=8;}
        if(dst.format==D3DFMT_V8U8 || dst.format==D3DFMT_P8)return D3DERR_INVALIDCALL;
        unsigned count=Bytes(dst.format);
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){unsigned result=0;
            for(unsigned c=0;c<4;++c)if(widths[c]){unsigned sum=0,mask=(1u<<widths[c])-1;
                for(unsigned yy=0;yy<2;++yy)for(unsigned xx=0;xx<2;++xx){unsigned value=0;std::memcpy(&value,src.bytes.data()+(y*2+yy)*src.pitch+(x*2+xx)*count,count);sum+=(value>>shifts[c])&mask;}
                result|=((sum+2)>>2)<<shifts[c];}
            std::memcpy(dst.bytes.data()+y*dst.pitch+x*count,&result,count);
        }return S_OK;
    }
    auto input=DecodeImage(src);std::vector<D3DXCOLOR> output(size_t(w)*h,D3DXCOLOR(0,0,0,0));
    if(filter==D3DX_FILTER_NONE || (src.width==w && src.height==h)){
        for(unsigned y=0;y<std::min(h,src.height);++y)for(unsigned x=0;x<std::min(w,src.width);++x)output[y*w+x]=input[y*src.width+x];
    } else if(half){
        unsigned nx=src.width==1?0:1,ny=src.height==1?0:1;
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)for(unsigned c=0;c<4;++c){
            E a=(&input[(y*2)*src.width+x*2].r)[c],b=(&input[(y*2)*src.width+x*2+nx].r)[c];
            E d=(&input[(y*2+ny)*src.width+x*2].r)[c],e=(&input[(y*2+ny)*src.width+x*2+nx].r)[c];
            E sum=a+b;if(c>=2)sum=sum.F();sum=sum+d;if(c==0 || c==3)sum=sum.F();sum=sum+e;
            if(c<=1)sum=sum.F();
            (&output[y*w+x].r)[c]=(sum*E(0.25)).F();
        }
    } else {
        auto tx=Triangle(src.width,w),ty=Triangle(src.height,h);
        for(unsigned sy=0;sy<src.height;++sy)for(unsigned sx=0;sx<src.width;++sx)
            for(auto wy:ty[sy])for(auto wx:tx[sx]){
                E coefficient=E(wx.value)*E(wy.value);auto &target=output[wy.destination*w+wx.destination];const auto &pixel=input[sy*src.width+sx];
                for(unsigned c=0;c<4;++c)(&target.r)[c]=(coefficient*E((&pixel.r)[c])+E((&target.r)[c])).F();
            }
    }
    unsigned writtenW=w,writtenH=h;
    if(DXT(dst.format)){
        for(unsigned y=0;y<writtenH;y+=4)for(unsigned x=0;x<writtenW;x+=4){D3DXCOLOR block[16];
            for(unsigned yy=0;yy<4;++yy)for(unsigned xx=0;xx<4;++xx)block[yy*4+xx]=output[((y+yy)%h)*w+(x+xx)%w];
            HRESULT hr=EncodeResourceBlock(dst.format,dst.bytes.data()+(y/4)*dst.pitch+(x/4)*(dst.format==D3DFMT_DXT1?8:16),block);if(FAILED(hr))return hr;
        }
    } else for(unsigned y=0;y<writtenH;++y)for(unsigned x=0;x<writtenW;++x)Encode(dst.bytes.data()+y*dst.pitch+x*Bytes(dst.format),dst.format,output[y*w+x]);
    return S_OK;
}
template<class T>struct Ref {T *p=nullptr;~Ref(){if(p)p->Release();}};
struct Access {
    IDirect3DSurface8 *original=nullptr;Ref<IDirect3DSurface8> stage;Ref<IDirect3DDevice8> device;
    bool locked=false,writing=false;D3DLOCKED_RECT data{};
    ~Access(){Close();}
    void Close(){if(locked){(stage.p?stage.p:original)->UnlockRect();locked=false;
        if(stage.p && writing)device.p->CopyRects(stage.p,nullptr,0,original,nullptr);}}
    HRESULT Open(IDirect3DSurface8 *surface,const RECT *requested,bool write,D3DSURFACE_DESC &desc,RECT &rect){
        original=surface;writing=write;surface->GetDesc(&desc);
        rect=requested?*requested:RECT{0,0,LONG(desc.Width),LONG(desc.Height)};
        if(rect.left<0 || rect.top<0 || rect.right<rect.left || rect.bottom<rect.top || rect.right>LONG(desc.Width) || rect.bottom>LONG(desc.Height))return D3DERR_INVALIDCALL;
        bool full=rect.left==0 && rect.top==0 && rect.right==LONG(desc.Width) && rect.bottom==LONG(desc.Height);
        const RECT *region=full?nullptr:&rect;
        if(desc.Pool!=D3DPOOL_DEFAULT){HRESULT hr=surface->LockRect(&data,region,write?2048:2064);locked=SUCCEEDED(hr);return hr;}
        Ref<IDirect3DBaseTexture8> owner;surface->GetContainer(IID_IDirect3DBaseTexture8,reinterpret_cast<void**>(&owner.p));
        if(!owner.p){HRESULT direct=surface->LockRect(&data,region,write?2048:2064);
            if(SUCCEEDED(direct)){locked=true;return S_OK;}}
        HRESULT hr=surface->GetDevice(&device.p);if(FAILED(hr))return hr;
        hr=device.p->CreateImageSurface(desc.Width,desc.Height,desc.Format,&stage.p);if(FAILED(hr))return hr;
        if(!write || !full){hr=device.p->CopyRects(surface,nullptr,0,stage.p,nullptr);if(FAILED(hr))return hr;}
        hr=stage.p->LockRect(&data,region,write?2048:2064);locked=SUCCEEDED(hr);return hr;
    }
    HRESULT Open(IDirect3DSurface8 *surface,const RECT &requested,bool write){D3DSURFACE_DESC desc{};RECT rect{};return Open(surface,&requested,write,desc,rect);}
};
RECT Region(const D3DSURFACE_DESC &d,const RECT *r){return r?*r:RECT{0,0,LONG(d.Width),LONG(d.Height)};}
Image Snapshot(const Access &access,const RECT &r,D3DFORMAT f){
    Image image{unsigned(r.right-r.left),unsigned(r.bottom-r.top),f,RowSize(f,unsigned(r.right-r.left)),{}};
    image.bytes.resize(size_t(image.pitch)*Rows(f,image.height));
    for(unsigned y=0;y<Rows(f,image.height);++y)std::memcpy(image.bytes.data()+y*image.pitch,static_cast<const unsigned char*>(access.data.pBits)+y*access.data.Pitch,image.pitch);
    return image;
}
}
HRESULT EncodeResourceBlock(D3DFORMAT format,unsigned char *out,const D3DXCOLOR *rgba) {
    if(!out || !rgba || (format!=D3DFMT_DXT1 && format!=D3DFMT_DXT3 && format!=D3DFMT_DXT5))return D3DERR_INVALIDCALL;
    auto color=ResourceEncoding::Color(rgba,format==D3DFMT_DXT1);
    if(!color.valid)return D3DERR_INVALIDCALL;
    unsigned offset=0;
    if(format!=D3DFMT_DXT1){
        auto alpha=format==D3DFMT_DXT3?ResourceEncoding::Alpha3(rgba):ResourceEncoding::Alpha5(rgba).bytes;
        std::memcpy(out,alpha.data(),8);offset=8;
    }
    std::memcpy(out+offset,color.bytes.data(),8);return S_OK;
}
HRESULT LoadSurfaceFromSurface(IDirect3DSurface8 *destination,const PALETTEENTRY *destinationPalette,
    const RECT *destinationRect,IDirect3DSurface8 *source,const PALETTEENTRY *sourcePalette,
    const RECT *sourceRect,DWORD filter,D3DCOLOR colorKey) try {
    if(!source || !destination || sourcePalette || destinationPalette || colorKey ||
        (filter!=D3DX_FILTER_NONE && filter!=D3DX_FILTER_BOX && filter!=D3DX_FILTER_TRIANGLE))return D3DERR_INVALIDCALL;
    D3DSURFACE_DESC dd{},sd{};RECT dr{},sr{};
    if(filter!=D3DX_FILTER_BOX){
        if(!destinationRect)destination->GetDesc(&dd);if(!sourceRect)source->GetDesc(&sd);
        dr=Region(dd,destinationRect);sr=Region(sd,sourceRect);
        if(dr.right-dr.left==sr.right-sr.left && dr.bottom-dr.top==sr.bottom-sr.top){
        Ref<IDirect3DDevice8> device;HRESULT hr=source->GetDevice(&device.p);
        if(SUCCEEDED(hr) && device.p){POINT point{dr.left,dr.top};
            if(SUCCEEDED(device.p->CopyRects(source,&sr,1,destination,&point)))return S_OK;}
        }
    }
    Access input;HRESULT hr=input.Open(source,sourceRect,false,sd,sr);if(FAILED(hr))return hr;
    auto sourceImage=Snapshot(input,sr,sd.Format);
    if(source==destination)input.Close();
    Access output;hr=output.Open(destination,destinationRect,true,dd,dr);if(FAILED(hr))return hr;
    if(dr.right==dr.left || dr.bottom==dr.top || sr.right==sr.left || sr.bottom==sr.top)return S_OK;
    auto destinationImage=Snapshot(output,dr,dd.Format);
    hr=Convert(sourceImage,destinationImage,filter);
    if(SUCCEEDED(hr))for(unsigned y=0;y<Rows(dd.Format,destinationImage.height);++y)
        std::memcpy(static_cast<unsigned char*>(output.data.pBits)+y*output.data.Pitch,destinationImage.bytes.data()+y*destinationImage.pitch,destinationImage.pitch);
    return hr;
} catch(const std::bad_alloc &) {return E_OUTOFMEMORY;}
HRESULT FilterTexture(IDirect3DBaseTexture8 *base,const PALETTEENTRY *palette,UINT sourceLevel,DWORD filter){
    if(!base || palette || sourceLevel!=0 || filter!=D3DX_FILTER_BOX)return D3DERR_INVALIDCALL;
    if(base->GetType()!=D3DRTYPE_TEXTURE)return D3DERR_INVALIDCALL;
    auto *texture=static_cast<IDirect3DTexture8*>(base);unsigned levels=texture->GetLevelCount();
    if(!levels)return D3DERR_INVALIDCALL;
    IDirect3DSurface8 *previous=nullptr;HRESULT hr=texture->GetSurfaceLevel(0,&previous);if(FAILED(hr))return hr;
    for(unsigned level=1;level<levels;++level){IDirect3DSurface8 *next=nullptr;hr=texture->GetSurfaceLevel(level,&next);
        if(SUCCEEDED(hr))hr=LoadSurfaceFromSurface(next,nullptr,nullptr,previous,nullptr,nullptr,filter,0);
        previous->Release();previous=next;if(FAILED(hr))break;}
    if(previous)previous->Release();return hr;
}
namespace {
bool DecodeFile(const std::vector<unsigned char> &bytes,std::vector<Image> &images){
    if(bytes.size()>=128 && !std::memcmp(bytes.data(),"DDS ",4)){
        if(Read32(bytes.data()+4)!=124 || Read32(bytes.data()+76)!=32 || Read32(bytes.data()+112) || Read32(bytes.data()+24)>1)return false;
        unsigned w=Read32(bytes.data()+16),h=Read32(bytes.data()+12),levels=std::max(1u,Read32(bytes.data()+28));
        D3DFORMAT format=D3DFMT_UNKNOWN;
        if(Read32(bytes.data()+80)&4)format=D3DFORMAT(Read32(bytes.data()+84));
        else if(Read32(bytes.data()+88)==32 && Read32(bytes.data()+92)==0xff0000 && Read32(bytes.data()+96)==0xff00 && Read32(bytes.data()+100)==0xff)
            format=Read32(bytes.data()+104)==0xff000000?D3DFMT_A8R8G8B8:D3DFMT_X8R8G8B8;
        if(!w || !h || w>16384 || h>16384 || levels>15 || (!DXT(format) && !Bytes(format)))return false;
        size_t at=128;
        for(unsigned level=0;level<levels;++level){Image image{w,h,format,RowSize(format,w),{}};size_t size=size_t(image.pitch)*Rows(format,h);
            if(at>bytes.size() || size>bytes.size()-at)return false;
            image.bytes.assign(bytes.begin()+at,bytes.begin()+at+size);images.push_back(std::move(image));at+=size;w=std::max(1u,w/2);h=std::max(1u,h/2);}
        return true;
    }
    if(bytes.size()<18)return false;
    unsigned type=bytes[2],w=Read16(bytes.data()+12),h=Read16(bytes.data()+14),bits=bytes[16],sample=bits/8;
    if(!w || !h || w>16384 || h>16384 || (bytes[17]&0xc0) ||
        !((type==1 && bits==8 && bytes[1]==1)||(type==3 && bits==8)||(type==2 || type==10) && (bits==24 || bits==32)))return false;
    size_t at=18+bytes[0];unsigned first=Read16(bytes.data()+3),entries=Read16(bytes.data()+5),paletteBytes=bytes[7]/8;
    std::vector<unsigned char> palette;
    if(type==1){size_t size=size_t(entries)*paletteBytes;if(paletteBytes<3 || paletteBytes>4 || at>bytes.size() || size>bytes.size()-at)return false;
        palette.assign(bytes.begin()+at,bytes.begin()+at+size);at+=size;}
    size_t count=size_t(w)*h;std::vector<unsigned char> decoded(count*sample);size_t written=0;
    while(written<count){unsigned run=1;bool repeat=false;
        if(type==10){if(at>=bytes.size())return false;unsigned header=bytes[at++];run=(header&127)+1;repeat=(header&128)!=0;
            // Quarantined packet-spanning legacy failure: reject safely, never
            // emulate heap corruption. Accepted row-bounded RLE is supported.
            if(run>w-written%w)return false;}
        size_t needed=size_t(repeat?1:run)*sample;if(run>count-written || at>bytes.size() || needed>bytes.size()-at)return false;
        for(unsigned i=0;i<run;++i){std::memcpy(decoded.data()+written++*sample,bytes.data()+at,sample);if(!repeat)at+=sample;}
        if(repeat)at+=sample;
    }
    D3DFORMAT format=type==3?D3DFMT_L8:(bits==24?D3DFMT_X8R8G8B8:D3DFMT_A8R8G8B8);
    Image image{w,h,format,w*Bytes(format),{}};image.bytes.resize(size_t(image.pitch)*h);
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){unsigned sx=bytes[17]&16?w-1-x:x,sy=bytes[17]&32?y:h-1-y;
        const unsigned char *p=decoded.data()+(size_t(sy)*w+sx)*sample;auto *dst=image.bytes.data()+y*image.pitch+x*Bytes(format);
        if(type==1){unsigned index=p[0];if(index<first || index-first>=entries)return false;p=palette.data()+(index-first)*paletteBytes;
            std::memcpy(dst,p,3);dst[3]=paletteBytes==4?p[3]:255;}
        else {std::memcpy(dst,p,sample);if(format==D3DFMT_X8R8G8B8)dst[3]=0;}
    }
    images.push_back(std::move(image));return true;
}
}
HRESULT CreateTextureFromFileExA(IDirect3DDevice8 *device,const char *name,UINT width,UINT height,UINT levels,
    DWORD usage,D3DFORMAT format,D3DPOOL pool,DWORD filter,DWORD mipFilter,D3DCOLOR key,
    D3DXIMAGE_INFO *info,PALETTEENTRY *palette,IDirect3DTexture8 **out) try {
    if(!device || !name || !out || info || palette || key || filter!=D3DX_FILTER_BOX || mipFilter!=D3DX_FILTER_BOX)return D3DERR_INVALIDCALL;
    constexpr HRESULT invalidData=HRESULT(0x88760b59u);
    std::ifstream file(name,std::ios::binary|std::ios::ate);if(!file)return invalidData;
    auto length=file.tellg();if(length<0 || length>256*1024*1024)return invalidData;
    std::vector<unsigned char> bytes(static_cast<size_t>(length));file.seekg(0);
    if(!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))return invalidData;
    std::vector<Image> authored;if(!DecodeFile(bytes,authored))return invalidData;
    if(width==D3DX_DEFAULT)width=authored[0].width;if(height==D3DX_DEFAULT)height=authored[0].height;
    if(format==D3DFMT_UNKNOWN)format=authored[0].format;
    Ref<IDirect3DTexture8> texture;HRESULT hr=CreateTexture(device,width,height,levels,usage,format,pool,&texture.p);if(FAILED(hr))return hr;
    Image previous{};
    for(unsigned level=0;level<texture.p->GetLevelCount();++level){
        Ref<IDirect3DSurface8> surface;hr=texture.p->GetSurfaceLevel(level,&surface.p);if(FAILED(hr))return hr;
        D3DSURFACE_DESC desc{};surface.p->GetDesc(&desc);RECT rect{0,0,LONG(desc.Width),LONG(desc.Height)};
        Image image{desc.Width,desc.Height,desc.Format,RowSize(desc.Format,desc.Width),{}};image.bytes.resize(size_t(image.pitch)*Rows(image.format,image.height));
        const Image &source=level<authored.size()?authored[level]:previous;
        // Sub-block authored DDS mips are decoded and re-encoded by the pinned
        // codec rather than copied as arbitrary unused block bytes.
        hr=Convert(source,image,filter,!(DXT(source.format) && (source.width<4 || source.height<4)));
        if(FAILED(hr))return hr;
        Access access;hr=access.Open(surface.p,rect,true);if(FAILED(hr))return hr;
        for(unsigned y=0;y<Rows(image.format,image.height);++y)std::memcpy(static_cast<unsigned char*>(access.data.pBits)+y*access.data.Pitch,image.bytes.data()+y*image.pitch,image.pitch);
        previous=std::move(image);
    }
    *out=texture.p;texture.p=nullptr;return S_OK;
} catch(const std::bad_alloc &) {return E_OUTOFMEMORY;}
}
