// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "LegacyResourceCharacterization.h"
#include <filesystem>
#include <cstring>
#include <map>
#include <cmath>
#include <float.h>
#if defined(_M_IX86)
#include "LegacyResourceEncoderModel.h"
#endif
#include "LegacyResource.h"

// Internal pinned encoder entry points reached by CD3DXCodecDXT. Test-only
// declarations allow exact float boundary probes; no replacement is supplied.
HRESULT __cdecl D3DXEncodeDXT1(unsigned char *,const D3DXCOLOR *);
HRESULT __cdecl D3DXEncodeDXT3(unsigned char *,const D3DXCOLOR *);
HRESULT __cdecl D3DXEncodeDXT5(unsigned char *,const D3DXCOLOR *);

namespace {
using namespace ResourceCharacterization;
const auto evidence=std::filesystem::path(RESOURCE_EVIDENCE_DIR);
std::map<std::string,std::string> LoadReference(const char *name="LegacyResourceReference.jsonl") {
    std::ifstream input(std::filesystem::path(RESOURCE_SOURCE_DIR)/
        "Tests/Google/Core/Libraries"/name);
    if(!input)Unexpected("missing compact resource reference");
    std::map<std::string,std::string> rows;
    std::string line;
    while(std::getline(input,line)) {
        const auto end=line.find('"',7);
        if(line.substr(0,7)!="{\"id\":\"" || end==std::string::npos || line.size()>16384)
            Unexpected("invalid compact reference");
        if(!rows.emplace(line.substr(7,end-7),line).second || rows.size()>128)
            Unexpected("unbounded or duplicate reference");
    }
    return rows;
}
const auto reference=LoadReference();
const auto samplingReference=LoadReference("LegacyResourceSamplingReference.jsonl");
const auto encoderReference=LoadReference("LegacyResourceEncoderReference.jsonl");
std::size_t encoderHits=0;
std::size_t referenceHits=0;
std::size_t samplingHits=0;
std::vector<Row> samplingCases;
std::vector<Row> compressedCases;
void Emit(std::ofstream &out,const Row &row) {
    if(row.kind=="surface" && row.id.rfind("b-",0)==0)samplingCases.push_back(row);
    if(row.kind=="compressed-cpu-mip")compressedCases.push_back(row);
    const auto json=row.JSON();out<<json<<'\n';out.flush();
    const auto found=reference.find(row.id);
    if(found!=reference.end()){EXPECT_EQ(json,found->second)<<row.id;++referenceHits;}
    const auto sample=samplingReference.find(row.id);
    if(sample!=samplingReference.end()){EXPECT_EQ(json,sample->second)<<row.id;++samplingHits;}
    const auto encoder=encoderReference.find(row.id);
    if(encoder!=encoderReference.end()){EXPECT_EQ(json,encoder->second)<<row.id;++encoderHits;}
}
const D3DFORMAT formats[]={D3DFMT_A8R8G8B8,D3DFMT_X8R8G8B8,D3DFMT_R8G8B8,
    D3DFMT_R5G6B5,D3DFMT_A1R5G5B5,D3DFMT_A4R4G4B4};

TEST(LegacyResourceCharacterization, RecordingSurfaceAndMipEvidence) {
    referenceHits=0;
    std::filesystem::create_directories(evidence);
    std::ofstream out(evidence/"recording.jsonl");ASSERT_TRUE(out.good());
    std::vector<std::string> events;
    Device device(events);observerDevice=&device;
    const std::array<unsigned,4> shapes[]={{4,4,4,4},{5,3,2,1},{3,2,5,3},{1,7,1,3},{7,1,3,1},{5,3,3,2}};
    for(auto sf:formats)for(auto df:formats)for(DWORD filter:{D3DX_FILTER_NONE,D3DX_FILTER_BOX,D3DX_FILTER_TRIANGLE})
        for(unsigned p=0;p<4;++p)for(unsigned shape=0;shape<std::size(shapes);++shape) {
            auto d=shapes[shape];Surface src("s",d[0],d[1],sf,events),dst("d",d[2],d[3],df,events);Fill(src,p);
            auto before=src.bytes;
            Emit(out,Copy("copy-"+std::to_string(sf)+"-"+std::to_string(df)+"-"+std::to_string(filter)+"-"+std::to_string(p)+"-"+std::to_string(shape),src,dst,filter));
            ASSERT_EQ(src.bytes,before);ASSERT_FALSE(src.locked);ASSERT_FALSE(dst.locked);
            ASSERT_EQ(src.refs,1u);ASSERT_EQ(dst.refs,1u);
        }
    for(DWORD filter:{D3DX_FILTER_NONE,D3DX_FILTER_BOX,D3DX_FILTER_TRIANGLE}) {
        Surface src("s",5,5,D3DFMT_A8R8G8B8,events),dst("d",7,6,D3DFMT_A8R8G8B8,events);Fill(src,0);
        RECT sr{1,1,4,4},dr{2,1,5,4};Emit(out,Copy("rect-"+std::to_string(filter),src,dst,filter,&sr,&dr));
        sr={-1,0,3,3};Emit(out,Copy("invalid-rect-"+std::to_string(filter),src,dst,filter,&sr,&dr));
    }
    for(auto sf:{D3DFMT_L8,D3DFMT_A8L8,D3DFMT_V8U8,D3DFMT_P8}) {
        Surface src("s",4,4,sf,events),dst("d",2,2,D3DFMT_A8R8G8B8,events);Fill(src,0);
        Emit(out,Copy("secondary-"+std::to_string(sf),src,dst,D3DX_FILTER_BOX));
    }
    for(unsigned failure=0;failure<6;++failure) {
        Surface src("s",4,4,D3DFMT_A8R8G8B8,events),dst("d",2,2,D3DFMT_A8R8G8B8,events);Fill(src,0);
        if(failure==0)src.descResult=E_FAIL;if(failure==1)dst.descResult=E_FAIL;
        if(failure==2)src.lockResult=E_FAIL;if(failure==3)dst.lockResult=E_FAIL;
        if(failure==4)src.unlockResult=E_FAIL;if(failure==5)dst.unlockResult=E_FAIL;
        Emit(out,Copy("failure-"+std::to_string(failure),src,dst,D3DX_FILTER_BOX));
    }
    // Exhaustive packed-word decoding is evidence, not a production converter.
    for(auto sf:{D3DFMT_R5G6B5,D3DFMT_A1R5G5B5,D3DFMT_A4R4G4B4}) {
        Surface src("s",256,256,sf,events),dst("d",256,256,D3DFMT_A8R8G8B8,events);
        auto expected=dst.bytes;
        for(unsigned v=0;v<65536;++v)std::memcpy(src.bytes.data()+(v/256)*src.pitch+(v%256)*2,&v,2);
        Emit(out,Copy("packed-all-"+std::to_string(sf),src,dst,D3DX_FILTER_NONE));
        // Measured on all 65,536 words: nearest scaled expansion, not replication.
        for(unsigned v=0;v<65536;++v) {
            const unsigned widths[4]={sf==D3DFMT_A4R4G4B4?4u:5u,
                sf==D3DFMT_R5G6B5?6u:(sf==D3DFMT_A4R4G4B4?4u:5u),
                sf==D3DFMT_A4R4G4B4?4u:5u,sf==D3DFMT_A4R4G4B4?4u:1u};
            unsigned shift=0;
            for(unsigned c=0;c<4;++c) {
                const unsigned mask=(1u<<widths[c])-1;
                const unsigned value=c==3 && sf==D3DFMT_R5G6B5?1:((v>>shift)&mask);
                expected[(v/256)*dst.pitch+(v%256)*4+c]=static_cast<unsigned char>((value*255+mask/2)/mask);
                shift+=widths[c];
            }
        }
        EXPECT_EQ(dst.bytes,expected)<<sf;
    }
    for(auto df:{D3DFMT_R5G6B5,D3DFMT_A1R5G5B5,D3DFMT_A4R4G4B4}) {
        Surface src("s",256,1,D3DFMT_A8R8G8B8,events),dst("d",256,1,df,events);
        for(unsigned x=0;x<256;++x)for(unsigned c=0;c<4;++c)src.bytes[x*4+c]=static_cast<unsigned char>(x);
        Emit(out,Copy("quantize-"+std::to_string(df),src,dst,D3DX_FILTER_NONE));
    }
    for(auto shape:{std::array<unsigned,2>{3,5},{5,3},{5,2},{7,3},{3,2},{2,5}})
        for(unsigned impulse=0;impulse<shape[0];++impulse)
            for(DWORD filter:{D3DX_FILTER_NONE,D3DX_FILTER_BOX,D3DX_FILTER_TRIANGLE}) {
                Surface src("s",shape[0],1,D3DFMT_A8R8G8B8,events),dst("d",shape[1],1,D3DFMT_A8R8G8B8,events);
                for(unsigned x=0;x<shape[0];++x)for(unsigned c=0;c<4;++c)src.bytes[x*4+c]=x==impulse?255:0;
                Emit(out,Copy("impulse-"+std::to_string(shape[0])+"-"+std::to_string(shape[1])+"-"+std::to_string(impulse)+"-"+std::to_string(filter),src,dst,filter));
            }
    {
        Surface src("s",2,1,D3DFMT_A8R8G8B8,events),dst("d",1,1,D3DFMT_A8R8G8B8,events);
        const unsigned char data[]={0,0,255,0,255,0,0,255};std::memcpy(src.bytes.data(),data,8);
        Emit(out,Copy("alpha-independent",src,dst,D3DX_FILTER_BOX));
    }
    for(auto f:formats)for(auto shape:{std::array<unsigned,2>{8,4},{5,3},{1,7},{7,1}})for(unsigned p=0;p<4;++p) {
        Texture texture(shape[0],shape[1],f,events);Fill(texture.levels[0],p);
        auto before=texture.levels[0].bytes;events.clear();
        HRESULT hr=D3DXFilterTexture(&texture,nullptr,0,D3DX_FILTER_BOX);
        ASSERT_EQ(texture.levels[0].bytes,before);ASSERT_EQ(texture.refs,1u);
        for(unsigned level=0;level<texture.levels.size();++level) {
            auto &s=texture.levels[level];Row row;
            row.id="mip-"+std::to_string(f)+"-"+std::to_string(shape[0])+"-"+std::to_string(shape[1])+"-"+std::to_string(p)+"-"+std::to_string(level);
            row.kind="mip";row.hr=hr;row.df=f;row.dw=s.desc.Width;row.dh=s.desc.Height;row.level=level;row.pitch=s.pitch;
            row.pixels=Hex(s.bytes.data(),s.bytes.size());row.events=Events(events);Emit(out,row);
            ASSERT_EQ(s.refs,1u);ASSERT_FALSE(s.locked);
        }
    }
    for(int failure:{1,2}) {
        Texture t(8,4,D3DFMT_A8R8G8B8,events);Fill(t.levels[0],0);t.failLevel=failure;events.clear();
        HRESULT hr=D3DXFilterTexture(&t,nullptr,0,D3DX_FILTER_BOX);
        for(unsigned level=0;level<t.levels.size();++level) {
            auto &s=t.levels[level];Row r;r.id="mip-failure-"+std::to_string(failure)+"-"+std::to_string(level);
            r.kind="mip-failure";r.hr=hr;r.level=level;r.dw=s.desc.Width;r.dh=s.desc.Height;r.pitch=s.pitch;
            r.pixels=Hex(s.bytes.data(),s.bytes.size());r.events=Events(events);Emit(out,r);ASSERT_EQ(s.refs,1u);
        }
    }
    EXPECT_EQ(device.refs,1u);observerDevice=nullptr;
    EXPECT_EQ(referenceHits,reference.size());
}

template<class T> struct COM {T *p=nullptr;~COM(){if(p)p->Release();}};
std::pair<unsigned,unsigned> Rows(D3DFORMAT f,unsigned w,unsigned h) {
    if(f>=D3DFMT_DXT1 && f<=D3DFMT_DXT5)
        return {std::max(1u,(w+3)/4)*(f==D3DFMT_DXT1?8:16),std::max(1u,(h+3)/4)};
    return {w*PixelBytes(f),h};
}
Row Read(const std::string &id,IDirect3DSurface8 *surface,HRESULT operation,const std::string &kind) {
    Row r;r.id=id;r.kind=kind;r.hr=operation;D3DSURFACE_DESC d{};
    if(FAILED(surface->GetDesc(&d)))Unexpected("real GetDesc");
    r.df=d.Format;r.dw=d.Width;r.dh=d.Height;
    r.events="pool="+std::to_string(d.Pool)+",usage="+std::to_string(d.Usage);
    D3DLOCKED_RECT lock{};HRESULT hr=surface->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    r.events+=",read-lock="+std::to_string(std::uint32_t(hr));
    if(SUCCEEDED(hr)) {
        auto geometry=Rows(d.Format,d.Width,d.Height);r.pitch=lock.Pitch;
        std::vector<unsigned char> bytes;
        for(unsigned y=0;y<geometry.second;++y) {
            auto *p=static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch;
            bytes.insert(bytes.end(),p,p+geometry.first);
        }
        r.pixels=Hex(bytes.data(),bytes.size());
        r.events+=",read-unlock="+std::to_string(std::uint32_t(surface->UnlockRect()));
    }
    return r;
}
void Write(const std::filesystem::path &path,const std::vector<unsigned char> &bytes) {
    std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    if(!f.good())Unexpected("fixture write failed");
}
std::vector<unsigned char> TGA(unsigned w,unsigned h,unsigned bits,unsigned type,bool top) {
    std::vector<unsigned char> bytes(18,0);bytes[2]=static_cast<unsigned char>(type);
    bytes[12]=static_cast<unsigned char>(w);bytes[14]=static_cast<unsigned char>(h);
    bytes[16]=static_cast<unsigned char>(bits);bytes[17]=(top?0x20:0)|(bits==32?8:0);
    if(type==1) {
        bytes[1]=1;bytes[5]=4;bytes[7]=32;
        const unsigned char palette[]={0,0,0,0,0,0,255,0,0,255,0,128,255,0,0,255};
        bytes.insert(bytes.end(),palette,palette+16);
    }
    if(type==10)bytes.push_back(static_cast<unsigned char>(w*h-1)); // one raw RLE packet
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)for(unsigned c=0;c<bits/8;++c) {
        unsigned v=(type==1)?((x+y)%4):((x*37+y*73+c*61)&255);
        if(bits==32 && c==3)v=x*255/std::max(1u,w-1);
        bytes.push_back(static_cast<unsigned char>(v));
    }
    return bytes;
}
std::vector<unsigned char> DDS(bool compressed,unsigned levels) {
    std::vector<DWORD> h(32,0);h[0]=0x20534444;h[1]=124;h[2]=0x21007|(compressed?0x80000:8);
    h[3]=h[4]=8;h[5]=32;h[7]=levels;h[19]=32;h[20]=compressed?4:0x41;
    if(compressed)h[21]=MAKEFOURCC('D','X','T','1');
    else {h[22]=32;h[23]=0xff0000;h[24]=0xff00;h[25]=0xff;h[26]=0xff000000;}
    h[27]=levels>1?0x401008:0x1000;
    std::vector<unsigned char> bytes(reinterpret_cast<unsigned char*>(h.data()),reinterpret_cast<unsigned char*>(h.data())+128);
    unsigned w=8;
    for(unsigned n=0;n<levels;++n) {
        if(compressed) {
            for(unsigned i=0;i<std::max(1u,(w+3)/4)*std::max(1u,(w+3)/4);++i) {
                const unsigned char block[]={0,0xf8,0xe0,7,0xe4,0xe4,0xe4,0xe4};
                bytes.insert(bytes.end(),block,block+8);
            }
        } else for(unsigned y=0;y<w;++y)for(unsigned x=0;x<w;++x)
            for(unsigned c=0;c<4;++c)bytes.push_back(static_cast<unsigned char>((n*61+x*37+y*73+c*41)&255));
        w=std::max(1u,w/2);
    }
    return bytes;
}
TEST(LegacyResourceCharacterization, RealWin32DeviceFilesCopiesAndMips) {
    std::filesystem::create_directories(evidence/"fixtures");
    std::ofstream out(evidence/"device.jsonl");ASSERT_TRUE(out.good());
    struct Module {HMODULE h=LoadLibraryExW(L"d3d8.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        ~Module(){if(h)FreeLibrary(h);}} module;
    if(!module.h)GTEST_SKIP()<<"System Win32 DX8 unavailable, error="<<GetLastError();
    auto create=reinterpret_cast<IDirect3D8 *(WINAPI *)(UINT)>(GetProcAddress(module.h,"Direct3DCreate8"));
    if(!create)GTEST_SKIP()<<"Direct3DCreate8 unavailable";
    COM<IDirect3D8> d3d;d3d.p=create(D3D_SDK_VERSION);if(!d3d.p)GTEST_SKIP()<<"No DX8 interface";
    struct Window {HWND h=CreateWindowExW(0,L"STATIC",L"DX8 characterization",WS_OVERLAPPEDWINDOW,
        0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);~Window(){if(h)DestroyWindow(h);}} window;
    ASSERT_NE(window.h,nullptr);D3DDISPLAYMODE mode{};
    ASSERT_EQ(d3d.p->GetAdapterDisplayMode(0,&mode),S_OK);
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.BackBufferFormat=mode.Format;
    pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window.h;pp.Windowed=TRUE;
    COM<IDirect3DDevice8> device;
    HRESULT available=d3d.p->CreateDevice(0,D3DDEVTYPE_HAL,window.h,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,&device.p);
    if(FAILED(available))GTEST_SKIP()<<"Real HAL device unavailable: "<<std::hex<<available;
    D3DADAPTER_IDENTIFIER8 adapter{};
    ASSERT_EQ(d3d.p->GetAdapterIdentifier(0,0,&adapter),S_OK);
    Row adapterRow;adapterRow.id="adapter";adapterRow.kind="device-environment";
    adapterRow.events=std::string(adapter.Driver)+","+adapter.Description+",vendor="+std::to_string(adapter.VendorId)+
        ",device="+std::to_string(adapter.DeviceId)+",version="+std::to_string(adapter.DriverVersion.QuadPart);
    Emit(out,adapterRow);
    std::vector<std::filesystem::path> files;
    auto fixture=[&](const char *name,const std::vector<unsigned char> &b){auto p=evidence/"fixtures"/name;Write(p,b);files.push_back(p);};
    fixture("rgba-odd.tga",TGA(5,3,32,2,false));fixture("rgba-top.tga",TGA(5,3,32,2,true));
    fixture("rgb.tga",TGA(4,4,24,2,false));fixture("rgba-rle.tga",TGA(5,3,32,10,false));
    auto rawTGA=TGA(5,3,32,2,false);
    std::vector<unsigned char> rowRLE(rawTGA.begin(),rawTGA.begin()+18);rowRLE[2]=10;
    for(unsigned y=0;y<3;++y){rowRLE.push_back(4);rowRLE.insert(rowRLE.end(),rawTGA.begin()+18+y*20,rawTGA.begin()+38+y*20);}
    fixture("row-rle.tga",rowRLE);
    fixture("gray.tga",TGA(5,3,8,3,false));fixture("palette.tga",TGA(5,3,8,1,false));
    fixture("one-by-seven.tga",TGA(1,7,32,2,false));fixture("seven-by-one.tga",TGA(7,1,32,2,false));
    fixture("rgba-mips.dds",DDS(false,4));fixture("dxt1-mips.dds",DDS(true,4));
    fixture("malformed.tga",{0,1,2});fixture("unsupported.bin",{'n','o','t',' ','a','n',' ','i','m','a','g','e'});
    files.push_back(evidence/"fixtures"/"missing.tga");
    const char *repo[]={"Core/Libraries/Source/WWVegas/WW3D2/RequiredAssets/AddProjectorGradient.tga",
        "Core/Libraries/Source/WWVegas/WW3D2/RequiredAssets/MultProjectorGradient.tga",
        "Core/Libraries/Source/WWVegas/WW3D2/RequiredAssets/w3d_missing_texture.tga",
        "Core/Tools/W3DView/Res/cursor.tga","Core/Tools/W3DView/Res/grab.tga",
        "Core/Tools/W3DView/Res/orbit.tga","Core/Tools/W3DView/Res/zoom.tga"};
    for(auto name:repo)files.push_back(std::filesystem::path(RESOURCE_SOURCE_DIR)/name);
    COM<IDirect3DTexture8> initial;ASSERT_EQ(device.p->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&initial.p),S_OK);
    unsigned successes=0;
    for(const auto &file:files)for(UINT levels:{0u,1u,3u}) {
        const char *selected=std::getenv("X64_9_FILE_CASE");
        if(selected && file.filename().string()+"-"+std::to_string(levels)!=selected)continue;
        // The pinned decoder rejects this packet-spanning RLE input and corrupts
        // the heap. Row-bounded RLE succeeds; keep the unsafe variant isolated.
        if(!selected && file.filename()=="rgba-rle.tga")continue;
        std::fprintf(stderr,"File case: %s-%u\n",file.filename().string().c_str(),levels);
        IDirect3DTexture8 *texture=initial.p;
        HRESULT hr=D3DXCreateTextureFromFileExA(device.p,file.string().c_str(),D3DX_DEFAULT,D3DX_DEFAULT,
            levels,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,D3DX_FILTER_BOX,D3DX_FILTER_BOX,0,nullptr,nullptr,&texture);
        std::string id=file.filename().string()+"-"+std::to_string(levels);
        Row r;r.id=id;r.kind="file";r.hr=hr;r.level=levels;
        r.events="output="+std::to_string(texture==nullptr?0:(texture==initial.p?1:2));
        r.events+=",initial-add="+std::to_string(initial.p->AddRef());r.events+=",initial-release="+std::to_string(initial.p->Release());
        if(SUCCEEDED(hr) && texture) {
            ++successes;
            const ULONG added=texture->AddRef();
            const ULONG released=texture->Release();
            r.events+=",add="+std::to_string(added)+",release="+std::to_string(released);
            Emit(out,r);
            for(UINT n=0;n<texture->GetLevelCount();++n) {
                COM<IDirect3DSurface8> surface;ASSERT_EQ(texture->GetSurfaceLevel(n,&surface.p),S_OK);
                Row level=Read(id+"-"+std::to_string(n),surface.p,hr,"file-level");level.level=n;Emit(out,level);
            }
        } else Emit(out,r);
        if(texture && texture!=initial.p)texture->Release();
    }
    if(std::getenv("X64_9_FILE_CASE"))return;
    EXPECT_GT(successes,0u);
    std::vector<std::string> events;
    for(auto f:formats)for(auto pool:{D3DPOOL_MANAGED,D3DPOOL_SYSTEMMEM,D3DPOOL_DEFAULT}) {
        COM<IDirect3DTexture8> texture;HRESULT hr=device.p->CreateTexture(8,4,0,0,f,pool,&texture.p);
        std::string id="real-mip-"+std::to_string(f)+"-"+std::to_string(pool);
        if(FAILED(hr)){Row r;r.id=id;r.kind="mip-create-failure";r.hr=hr;Emit(out,r);continue;}
        bool initialized=true;
        for(UINT n=0;n<texture.p->GetLevelCount();++n) {
            D3DLOCKED_RECT lock{};HRESULT locked=texture.p->LockRect(n,&lock,nullptr,0);
            if(FAILED(locked)) {
                Row r;r.id=id;r.kind="mip-initialize-lock-failure";r.hr=locked;r.level=n;
                Emit(out,r);initialized=false;break;
            }
            D3DSURFACE_DESC desc{};ASSERT_EQ(texture.p->GetLevelDesc(n,&desc),S_OK);
            Surface source("fixture",desc.Width,desc.Height,f,events);Fill(source,0);
            for(unsigned y=0;y<desc.Height;++y) {
                auto *p=static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch;
                if(n)std::memset(p,0xa5,desc.Width*PixelBytes(f));
                else std::memcpy(p,source.bytes.data()+y*source.pitch,desc.Width*PixelBytes(f));
            }
            ASSERT_EQ(texture.p->UnlockRect(n),S_OK);
        }
        if(!initialized)continue;
        hr=D3DXFilterTexture(texture.p,nullptr,0,D3DX_FILTER_BOX);
        for(UINT n=0;n<texture.p->GetLevelCount();++n) {
            COM<IDirect3DSurface8> s;ASSERT_EQ(texture.p->GetSurfaceLevel(n,&s.p),S_OK);
            Row r=Read(id+"-"+std::to_string(n),s.p,hr,"real-mip");r.level=n;Emit(out,r);
        }
    }
    for(auto sf:formats)for(auto df:formats)for(DWORD filter:{D3DX_FILTER_NONE,D3DX_FILTER_BOX,D3DX_FILTER_TRIANGLE}) {
        COM<IDirect3DSurface8> src,dst;HRESULT a=device.p->CreateImageSurface(5,3,sf,&src.p),b=device.p->CreateImageSurface(3,2,df,&dst.p);
        std::string id="real-copy-"+std::to_string(sf)+"-"+std::to_string(df)+"-"+std::to_string(filter);
        if(FAILED(a)||FAILED(b)){Row r;r.id=id;r.kind="surface-create-failure";r.hr=FAILED(a)?a:b;Emit(out,r);continue;}
        for(auto pair:{std::pair<IDirect3DSurface8*,bool>{src.p,true},{dst.p,false}}) {
            D3DSURFACE_DESC d{};ASSERT_EQ(pair.first->GetDesc(&d),S_OK);D3DLOCKED_RECT lock{};
            ASSERT_EQ(pair.first->LockRect(&lock,nullptr,0),S_OK);Surface input("fixture",d.Width,d.Height,d.Format,events);Fill(input,0);
            for(unsigned y=0;y<d.Height;++y) {
                auto *p=static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch;
                if(pair.second)std::memcpy(p,input.bytes.data()+y*input.pitch,d.Width*PixelBytes(d.Format));
                else std::memset(p,0xa5,d.Width*PixelBytes(d.Format));
            }
            ASSERT_EQ(pair.first->UnlockRect(),S_OK);
        }
        HRESULT hr=D3DXLoadSurfaceFromSurface(dst.p,nullptr,nullptr,src.p,nullptr,nullptr,filter,0);
        Row r=Read(id,dst.p,hr,"real-copy");r.sf=sf;r.sw=5;r.sh=3;r.filter=filter;Emit(out,r);
    }
    // Production SurfaceClass can wrap back buffers and non-lockable targets.
    COM<IDirect3DSurface8> backBuffer,readback,target,savedTarget,savedDepth;
    ASSERT_EQ(device.p->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&backBuffer.p),S_OK);
    ASSERT_EQ(device.p->CreateImageSurface(64,64,mode.Format,&readback.p),S_OK);
    ASSERT_EQ(device.p->Clear(0,nullptr,D3DCLEAR_TARGET,0xff123456,1,0),S_OK);
    HRESULT copied=D3DXLoadSurfaceFromSurface(readback.p,nullptr,nullptr,backBuffer.p,nullptr,nullptr,D3DX_FILTER_NONE,0);
    Emit(out,Read("backbuffer-copy",readback.p,copied,"device-access"));
    HRESULT created=device.p->CreateRenderTarget(64,64,mode.Format,D3DMULTISAMPLE_NONE,FALSE,&target.p);
    if(SUCCEEDED(created)) {
        ASSERT_EQ(device.p->GetRenderTarget(&savedTarget.p),S_OK);
        device.p->GetDepthStencilSurface(&savedDepth.p);
        ASSERT_EQ(device.p->SetRenderTarget(target.p,nullptr),S_OK);
        ASSERT_EQ(device.p->Clear(0,nullptr,D3DCLEAR_TARGET,0xffabcdef,1,0),S_OK);
        ASSERT_EQ(device.p->SetRenderTarget(savedTarget.p,savedDepth.p),S_OK);
        copied=D3DXLoadSurfaceFromSurface(readback.p,nullptr,nullptr,target.p,nullptr,nullptr,D3DX_FILTER_NONE,0);
        Emit(out,Read("nonlockable-target-copy",readback.p,copied,"device-access"));
        copied=D3DXLoadSurfaceFromSurface(target.p,nullptr,nullptr,readback.p,nullptr,nullptr,D3DX_FILTER_NONE,0);
        Row r;r.id="nonlockable-target-write";r.kind="device-access";r.hr=copied;Emit(out,r);
    } else {Row r;r.id="nonlockable-target-create";r.kind="device-access";r.hr=created;Emit(out,r);}
    COM<IDirect3DTexture8> paletteOutput;
    HRESULT paletteResult=D3DXCreateTextureFromFileExA(device.p,(evidence/"fixtures"/"palette.tga").string().c_str(),
        D3DX_DEFAULT,D3DX_DEFAULT,1,0,D3DFMT_P8,D3DPOOL_MANAGED,D3DX_FILTER_BOX,D3DX_FILTER_BOX,0,nullptr,nullptr,&paletteOutput.p);
    Row paletteRow;paletteRow.id="explicit-p8";paletteRow.kind="file-policy";paletteRow.hr=paletteResult;
    paletteRow.events=paletteOutput.p?"nonnull":"null";
    if(paletteOutput.p){D3DSURFACE_DESC d{};ASSERT_EQ(paletteOutput.p->GetLevelDesc(0,&d),S_OK);
        paletteRow.df=d.Format;paletteRow.dw=d.Width;paletteRow.dh=d.Height;}
    Emit(out,paletteRow);
    COM<IDirect3DTexture8> compressed;
    HRESULT compressedResult=D3DXCreateTextureFromFileExA(device.p,(evidence/"fixtures"/"dxt1-mips.dds").string().c_str(),
        D3DX_DEFAULT,D3DX_DEFAULT,0,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,D3DX_FILTER_BOX,D3DX_FILTER_BOX,0,nullptr,nullptr,&compressed.p);
    if(SUCCEEDED(compressedResult)) {
        compressedResult=D3DXFilterTexture(compressed.p,nullptr,0,D3DX_FILTER_BOX);
        for(UINT n=0;n<compressed.p->GetLevelCount();++n) {
            COM<IDirect3DSurface8> s;ASSERT_EQ(compressed.p->GetSurfaceLevel(n,&s.p),S_OK);
            Row r=Read("compressed-filter-"+std::to_string(n),s.p,compressedResult,"compressed-filter");r.level=n;Emit(out,r);
        }
    } else {Row r;r.id="compressed-create";r.kind="compressed-filter";r.hr=compressedResult;Emit(out,r);}
    std::printf("Real resource characterization: %u successful file loads; evidence=%s\n",successes,evidence.string().c_str());
}

// Threshold inputs only; all filtering/conversion is performed by pinned D3DX.
void Threshold(Surface &s,unsigned variant) {
    const unsigned char values[]={0,1,2,3,7,8,15,16,31,32,63,64,126,127,128,129,254,255};
    for(unsigned y=0;y<s.desc.Height;++y)for(unsigned x=0;x<s.desc.Width;++x)
        for(unsigned c=0;c<4;++c) {
            unsigned value=values[(x*3+y*7+c*5+variant)%std::size(values)];
            if(variant==1)value=(x*13+y*29+c*47+127)&255;
            if(variant==2)value=(x==0 && y==s.desc.Height-1)?(c==3?0:255):
                ((x==s.desc.Width-1 && y==0)?(c==3?255:0):((x+y+c)&1));
            s.bytes[y*s.pitch+x*4+c]=static_cast<unsigned char>(value);
        }
}
std::string Active(const Surface &s) {
    std::string value;
    for(unsigned y=0;y<s.desc.Height;++y)value+=Hex(s.bytes.data()+y*s.pitch,s.desc.Width*PixelBytes(s.desc.Format));
    return value;
}

std::vector<unsigned char> CompressedBlocks(D3DFORMAT f,unsigned w,unsigned h,unsigned order) {
    std::vector<unsigned char> bytes;
    for(unsigned block=0;block<((w+3)/4)*((h+3)/4);++block) {
        if(f==D3DFMT_DXT3) {
            for(unsigned i=0;i<8;++i)bytes.push_back(static_cast<unsigned char>(((i*2+block)&15)|(((15-i*2+block)&15)<<4)));
        } else {
            bytes.push_back(order?16:240);bytes.push_back(order?240:16);
            std::uint64_t indices=0;for(unsigned i=0;i<16;++i)indices|=std::uint64_t((i+block)%8)<<(3*i);
            for(unsigned i=0;i<6;++i)bytes.push_back(static_cast<unsigned char>(indices>>(8*i)));
        }
        const unsigned char color[]={0,0xf8,0xe0,7,0xe4,0x1b,0xe4,0x1b};
        bytes.insert(bytes.end(),color,color+8);
    }
    return bytes;
}
void GenerateSamplingEvidence() {
    samplingCases.clear();compressedCases.clear();
    std::ofstream out(evidence/"sampling.jsonl");ASSERT_TRUE(out.good());
    samplingHits=0;std::vector<std::string> events;Device device(events);observerDevice=&device;
    // Each ratio distinguishes an explicit dispatch/parity/axis question.
    const std::array<unsigned,4> shapes[]={{4,1,2,1},{5,1,2,1},{4,1,3,1},{5,1,3,1},{3,1,5,1},{6,1,4,1},
        {1,4,1,2},{1,5,1,3},{1,3,1,5},{4,4,2,2},{5,3,3,2},{4,6,3,5},{3,5,6,4}};
    for(unsigned shape=0;shape<std::size(shapes);++shape)for(unsigned variant=0;variant<3;++variant)
        for(DWORD filter:{D3DX_FILTER_BOX,D3DX_FILTER_TRIANGLE}) {
            auto d=shapes[shape];Surface src("s",d[0],d[1],D3DFMT_A8R8G8B8,events),dst("d",d[2],d[3],D3DFMT_A8R8G8B8,events);
            Threshold(src,variant);auto before=src.bytes;
            Emit(out,Copy("b-threshold-"+std::to_string(shape)+"-"+std::to_string(variant)+"-"+std::to_string(filter),src,dst,filter));
            EXPECT_EQ(src.bytes,before);EXPECT_EQ(src.refs,1u);EXPECT_EQ(dst.refs,1u);
            EXPECT_FALSE(src.locked);EXPECT_FALSE(dst.locked);
            // Deliberate byte-quantized horizontal then vertical experiment.
            // A mismatch distinguishes this from the oracle's true 2D route.
            if(d[1]>1 && d[0]>1) {
                Surface horizontal("h",d[2],d[1],D3DFMT_A8R8G8B8,events),vertical("v",d[2],d[3],D3DFMT_A8R8G8B8,events);
                Row first=Copy("b-pass-h-"+std::to_string(shape)+"-"+std::to_string(variant)+"-"+std::to_string(filter),src,horizontal,filter);
                Emit(out,first);
                Emit(out,Copy("b-pass-v-"+std::to_string(shape)+"-"+std::to_string(variant)+"-"+std::to_string(filter),horizontal,vertical,filter));
            }
        }
    for(auto f:{D3DFMT_R5G6B5,D3DFMT_A1R5G5B5,D3DFMT_A4R4G4B4,D3DFMT_A8R8G8B8,D3DFMT_X8R8G8B8})
        for(unsigned shape:{9u,10u})for(unsigned variant=0;variant<3;++variant)
            for(DWORD filter:{D3DX_FILTER_BOX,D3DX_FILTER_TRIANGLE}) {
                auto d=shapes[shape];Surface input("input",d[0],d[1],D3DFMT_A8R8G8B8,events);Threshold(input,variant);
                Surface packed("s",d[0],d[1],f,events),dst("d",d[2],d[3],f,events);
                ASSERT_EQ(Copy("setup",input,packed,D3DX_FILTER_NONE).hr,S_OK);
                std::string id=std::to_string(f)+"-"+std::to_string(shape)+"-"+std::to_string(variant)+"-"+std::to_string(filter);
                Emit(out,Copy("b-packed-"+id,packed,dst,filter));
                Surface expanded("e",d[0],d[1],D3DFMT_A8R8G8B8,events),filtered("f",d[2],d[3],D3DFMT_A8R8G8B8,events),repacked("r",d[2],d[3],f,events);
                ASSERT_EQ(Copy("expand",packed,expanded,D3DX_FILTER_NONE).hr,S_OK);
                ASSERT_EQ(Copy("filter",expanded,filtered,filter).hr,S_OK);
                Emit(out,Copy("b-byte-intermediate-"+id,filtered,repacked,D3DX_FILTER_NONE));
            }

    for(auto f:{D3DFMT_DXT3,D3DFMT_DXT5})for(unsigned order=0;order<2;++order)
        for(auto dims:{std::array<unsigned,4>{8,8,8,8},{8,8,3,2},{5,3,3,2}})
            for(auto df:{D3DFMT_A8R8G8B8,D3DFMT_A4R4G4B4})
                for(DWORD filter:{D3DX_FILTER_NONE,D3DX_FILTER_BOX,D3DX_FILTER_TRIANGLE}) {
                    Surface src("s",dims[0],dims[1],f,events),dst("d",dims[2],dims[3],df,events);
                    auto bytes=CompressedBlocks(f,dims[0],dims[1],order);
                    for(unsigned y=0;y<StorageRows(f,dims[1]);++y)
                        std::memcpy(src.bytes.data()+y*src.pitch,bytes.data()+y*RowBytes(f,dims[0]),RowBytes(f,dims[0]));
                    auto before=src.bytes;
                    Emit(out,Copy("b-dxt-"+std::to_string(f)+"-"+std::to_string(order)+"-"+std::to_string(dims[0])+"-"+
                        std::to_string(dims[2])+"-"+std::to_string(df)+"-"+std::to_string(filter),src,dst,filter));
                    EXPECT_EQ(src.bytes,before);EXPECT_FALSE(src.locked);EXPECT_FALSE(dst.locked);
                    EXPECT_EQ(src.refs,1u);EXPECT_EQ(dst.refs,1u);
                }
    for(auto f:{D3DFMT_DXT3,D3DFMT_DXT5})for(unsigned order=0;order<2;++order) {
        Texture texture(8,8,f,events);std::vector<std::string> before;
        for(auto &level:texture.levels) {
            auto blocks=CompressedBlocks(f,level.desc.Width,level.desc.Height,order);
            for(unsigned y=0;y<StorageRows(f,level.desc.Height);++y)
                std::memcpy(level.bytes.data()+y*level.pitch,blocks.data()+y*RowBytes(f,level.desc.Width),RowBytes(f,level.desc.Width));
            before.push_back(Hex(level.bytes.data(),level.bytes.size()));
        }
        events.clear();HRESULT hr=D3DXFilterTexture(&texture,nullptr,0,D3DX_FILTER_BOX);
        for(unsigned n=0;n<texture.levels.size();++n) {
            auto &level=texture.levels[n];Row r;r.id="b-cpu-compressed-mip-"+std::to_string(f)+"-"+std::to_string(order)+"-"+std::to_string(n);
            r.kind="compressed-cpu-mip";r.hr=hr;r.sf=f;r.df=f;r.dw=level.desc.Width;r.dh=level.desc.Height;
            r.filter=D3DX_FILTER_BOX;r.pitch=level.pitch;r.level=n;r.source=before[n];
            r.pixels=Hex(level.bytes.data(),level.bytes.size());r.events=Events(events);Emit(out,r);
            EXPECT_EQ(level.refs,1u);EXPECT_FALSE(level.locked);
        }
        EXPECT_EQ(texture.refs,1u);
    }
    EXPECT_EQ(device.refs,1u);observerDevice=nullptr;
    EXPECT_EQ(samplingHits,samplingReference.size());
}
TEST(LegacyResourceCharacterization, SamplingThresholdAndPackedEvidence) {
    ASSERT_EQ(samplingReference.size(),94u);
    GenerateSamplingEvidence();
}

std::vector<unsigned char> Unhex(const std::string &s) {
    std::vector<unsigned char> bytes;
    for(std::size_t i=0;i<s.size();i+=2)bytes.push_back(static_cast<unsigned char>(std::stoul(s.substr(i,2),nullptr,16)));
    return bytes;
}
void Upload(IDirect3DSurface8 *surface,const unsigned char *bytes,unsigned pitch) {
    D3DSURFACE_DESC d{};ASSERT_EQ(surface->GetDesc(&d),S_OK);
    D3DLOCKED_RECT lock{};ASSERT_EQ(surface->LockRect(&lock,nullptr,0),S_OK);
    for(unsigned y=0;y<StorageRows(d.Format,d.Height);++y)
        std::memcpy(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch,bytes+y*pitch,RowBytes(d.Format,d.Width));
    ASSERT_EQ(surface->UnlockRect(),S_OK);
}
Row ReadStorage(const std::string &id,IDirect3DSurface8 *surface,HRESULT hr,const char *kind) {
    Row row;row.id=id;row.kind=kind;row.hr=hr;
    D3DSURFACE_DESC d{};if(FAILED(surface->GetDesc(&d)))Unexpected("real storage description");
    row.df=d.Format;row.dw=d.Width;row.dh=d.Height;
    D3DLOCKED_RECT lock{};HRESULT access=surface->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    row.events="read-lock="+std::to_string(std::uint32_t(access));
    if(SUCCEEDED(access)) {
        row.pitch=lock.Pitch;
        for(unsigned y=0;y<StorageRows(d.Format,d.Height);++y)
            row.pixels+=Hex(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch,RowBytes(d.Format,d.Width));
        EXPECT_EQ(surface->UnlockRect(),S_OK);
    }
    return row;
}
// Forward all successful access to the real device. Only an owned DEFAULT-pool
// texture surface is represented by an observer interface: GetContainer succeeds,
// so the legacy helper never takes its standalone-surface private-layout branch.
struct StagingDevice : Device {
    IDirect3DDevice8 *real;
    IDirect3DSurface8 *observed=nullptr,*underlying=nullptr;
    unsigned copies=0,failCopy=0;
    bool failCreate=false;
    StagingDevice(std::vector<std::string> &events,IDirect3DDevice8 *d):Device(events),real(d) {}
    HRESULT STDMETHODCALLTYPE CreateImageSurface(UINT w,UINT h,D3DFORMAT f,IDirect3DSurface8 **out) override {
        events.push_back("stage-create,"+std::to_string(w)+","+std::to_string(h)+","+std::to_string(f));
        if(failCreate){*out=nullptr;return E_OUTOFMEMORY;}
        return real->CreateImageSurface(w,h,f,out);
    }
    HRESULT STDMETHODCALLTYPE CopyRects(IDirect3DSurface8 *src,const RECT *rect,UINT count,
        IDirect3DSurface8 *dst,const POINT *point) override {
        ++copies;
        HRESULT hr=copies==failCopy?E_FAIL:real->CopyRects(src==observed?underlying:src,rect,count,
            dst==observed?underlying:dst,point);
        events.push_back("stage-copy,"+std::to_string(copies)+","+std::to_string(src==observed)+","+
            std::to_string(dst==observed)+","+std::to_string(std::uint32_t(hr)));
        return hr;
    }
};
TEST(LegacyResourceCharacterization, RealSamplingScaledAccessAndAliasing) {
    if(samplingCases.empty())GenerateSamplingEvidence();
    std::ofstream out(evidence/"sampling-device.jsonl");ASSERT_TRUE(out.good());
    struct Module {HMODULE h=LoadLibraryExW(L"d3d8.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        ~Module(){if(h)FreeLibrary(h);}} module;
    if(!module.h)GTEST_SKIP()<<"System Win32 DX8 unavailable";
    auto create=reinterpret_cast<IDirect3D8 *(WINAPI *)(UINT)>(GetProcAddress(module.h,"Direct3DCreate8"));
    ASSERT_NE(create,nullptr);COM<IDirect3D8> d3d;d3d.p=create(D3D_SDK_VERSION);ASSERT_NE(d3d.p,nullptr);
    struct Window {HWND h=CreateWindowExW(0,L"STATIC",L"DX8 sampling characterization",WS_OVERLAPPEDWINDOW,
        0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);~Window(){if(h)DestroyWindow(h);}} window;
    ASSERT_NE(window.h,nullptr);D3DDISPLAYMODE mode{};ASSERT_EQ(d3d.p->GetAdapterDisplayMode(0,&mode),S_OK);
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.BackBufferFormat=mode.Format;
    pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window.h;pp.Windowed=TRUE;
    COM<IDirect3DDevice8> device;
    HRESULT available=d3d.p->CreateDevice(0,D3DDEVTYPE_HAL,window.h,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,&device.p);
    if(FAILED(available))GTEST_SKIP()<<"Real HAL unavailable: "<<std::hex<<available;
    unsigned agreements=0,limitations=0;
    for(const auto &cpu:samplingCases) {
        COM<IDirect3DSurface8> src,dst;
        HRESULT a=device.p->CreateImageSurface(cpu.sw,cpu.sh,D3DFORMAT(cpu.sf),&src.p);
        HRESULT b=device.p->CreateImageSurface(cpu.dw,cpu.dh,D3DFORMAT(cpu.df),&dst.p);
        if(FAILED(a)||FAILED(b)) {
            Row r;r.id="real-"+cpu.id;r.kind="sampling-create-limitation";r.hr=FAILED(a)?a:b;Emit(out,r);++limitations;continue;
        }
        auto source=Unhex(cpu.source),expected=Unhex(cpu.pixels);
        const unsigned sourcePitch=unsigned((source.size()-16)/StorageRows(D3DFORMAT(cpu.sf),cpu.sh));
        Upload(src.p,source.data(),sourcePitch);
        std::vector<unsigned char> sentinel(expected.size(),0xa5);
        Upload(dst.p,sentinel.data(),cpu.pitch);
        HRESULT hr=D3DXLoadSurfaceFromSurface(dst.p,nullptr,nullptr,src.p,nullptr,nullptr,cpu.filter,0);
        Row real=ReadStorage("real-"+cpu.id,dst.p,hr,"sampling-real");
        real.sf=cpu.sf;real.sw=cpu.sw;real.sh=cpu.sh;real.filter=cpu.filter;
        std::string active;
        for(unsigned y=0;y<cpu.dh;++y)active+=Hex(expected.data()+y*cpu.pitch,RowBytes(D3DFORMAT(cpu.df),cpu.dw));
        EXPECT_EQ(hr,cpu.hr)<<cpu.id;EXPECT_EQ(real.pixels,active)<<cpu.id;
        Row unchanged=ReadStorage("source",src.p,S_OK,"source");std::string sourceActive;
        for(unsigned y=0;y<StorageRows(D3DFORMAT(cpu.sf),cpu.sh);++y)
            sourceActive+=Hex(source.data()+y*sourcePitch,RowBytes(D3DFORMAT(cpu.sf),cpu.sw));
        EXPECT_EQ(unchanged.pixels,sourceActive)<<cpu.id;
        Emit(out,real);++agreements;
    }
    std::vector<std::string> events;
    for(auto f:{D3DFMT_DXT3,D3DFMT_DXT5})for(unsigned order=0;order<2;++order) {
        COM<IDirect3DTexture8> texture;
        HRESULT hr=device.p->CreateTexture(8,8,0,0,f,D3DPOOL_MANAGED,&texture.p);
        if(FAILED(hr)){Row r;r.id="compressed-mip-create-"+std::to_string(f);r.hr=hr;r.kind="adapter-limitation";Emit(out,r);continue;}
        for(UINT n=0;n<texture.p->GetLevelCount();++n) {
            COM<IDirect3DSurface8> s;ASSERT_EQ(texture.p->GetSurfaceLevel(n,&s.p),S_OK);
            D3DSURFACE_DESC d{};ASSERT_EQ(s.p->GetDesc(&d),S_OK);
            auto blocks=CompressedBlocks(f,d.Width,d.Height,order);Upload(s.p,blocks.data(),RowBytes(f,d.Width));
        }
        hr=D3DXFilterTexture(texture.p,nullptr,0,D3DX_FILTER_BOX);
        for(UINT n=0;n<texture.p->GetLevelCount();++n) {
            COM<IDirect3DSurface8> s;ASSERT_EQ(texture.p->GetSurfaceLevel(n,&s.p),S_OK);
            auto r=ReadStorage("b-compressed-mip-"+std::to_string(f)+"-"+std::to_string(order)+"-"+std::to_string(n),s.p,hr,"compressed-mip");
            const std::string cpuID="b-cpu-compressed-mip-"+std::to_string(f)+"-"+std::to_string(order)+"-"+std::to_string(n);
            const auto found=std::find_if(compressedCases.begin(),compressedCases.end(),[&](const Row &x){return x.id==cpuID;});
            ASSERT_NE(found,compressedCases.end());auto bytes=Unhex(found->pixels);std::string active;
            for(unsigned y=0;y<StorageRows(f,found->dh);++y)active+=Hex(bytes.data()+y*found->pitch,RowBytes(f,found->dw));
            EXPECT_EQ(hr,found->hr);EXPECT_EQ(r.pixels,active);
            r.level=n;Emit(out,r);
        }
    }
    // Public SurfaceClass APIs do not exclude self/underlying-resource aliases.
    // Use actual interfaces: do not invent permissive nested-lock behavior.
    for(unsigned shape=0;shape<3;++shape)for(DWORD filter:{D3DX_FILTER_NONE,D3DX_FILTER_BOX,D3DX_FILTER_TRIANGLE}) {
        COM<IDirect3DSurface8> s;ASSERT_EQ(device.p->CreateImageSurface(8,8,D3DFMT_A8R8G8B8,&s.p),S_OK);
        Surface input("input",8,8,D3DFMT_A8R8G8B8,events);Threshold(input,1);Upload(s.p,input.bytes.data(),input.pitch);
        RECT sr{0,0,8,8},dr{0,0,8,8};if(shape==1){sr={0,0,6,6};dr={2,2,8,8};}if(shape==2)dr={1,1,5,5};
        HRESULT hr=D3DXLoadSurfaceFromSurface(s.p,nullptr,&dr,s.p,nullptr,&sr,filter,0);
        auto r=ReadStorage("b-alias-"+std::to_string(shape)+"-"+std::to_string(filter),s.p,hr,"alias-real");
        r.source=Active(input);r.filter=filter;
        auto actual=Unhex(r.pixels);
        ASSERT_EQ(actual.size(),8u*8u*4u);
        for(LONG y=0;y<8;++y)for(LONG x=0;x<8;++x)
            if(x<dr.left || x>=dr.right || y<dr.top || y>=dr.bottom)
                EXPECT_EQ(Hex(actual.data()+(y*8+x)*4,4),Hex(input.bytes.data()+y*input.pitch+x*4,4));
        COM<IDirect3DSurface8> snapshot,separate;
        ASSERT_EQ(device.p->CreateImageSurface(8,8,D3DFMT_A8R8G8B8,&snapshot.p),S_OK);
        ASSERT_EQ(device.p->CreateImageSurface(8,8,D3DFMT_A8R8G8B8,&separate.p),S_OK);
        Upload(snapshot.p,input.bytes.data(),input.pitch);Upload(separate.p,input.bytes.data(),input.pitch);
        HRESULT separateResult=D3DXLoadSurfaceFromSurface(separate.p,nullptr,&dr,snapshot.p,nullptr,&sr,filter,0);
        auto comparison=ReadStorage(r.id+"-snapshot",separate.p,separateResult,"alias-snapshot");
        r.events+=",snapshot-equal="+std::to_string(r.pixels==comparison.pixels);
        Emit(out,r);Emit(out,comparison);
    }
    COM<IDirect3DTexture8> targetTexture;
    HRESULT targetResult=device.p->CreateTexture(8,8,1,D3DUSAGE_RENDERTARGET,mode.Format,D3DPOOL_DEFAULT,&targetTexture.p);
    if(SUCCEEDED(targetResult)) {
        COM<IDirect3DSurface8> target,savedTarget,savedDepth,system,readback;
        ASSERT_EQ(targetTexture.p->GetSurfaceLevel(0,&target.p),S_OK);
        ASSERT_EQ(device.p->GetRenderTarget(&savedTarget.p),S_OK);device.p->GetDepthStencilSurface(&savedDepth.p);
        ASSERT_EQ(device.p->CreateImageSurface(8,8,mode.Format,&system.p),S_OK);
        ASSERT_EQ(device.p->CreateImageSurface(8,8,mode.Format,&readback.p),S_OK);
        Surface input("input",8,8,mode.Format,events);Fill(input,0);
        for(unsigned route=0;route<3;++route)for(DWORD filter:{D3DX_FILTER_BOX,D3DX_FILTER_TRIANGLE}) {
            Upload(system.p,input.bytes.data(),input.pitch);
            ASSERT_EQ(device.p->SetRenderTarget(target.p,nullptr),S_OK);
            for(LONG y=0;y<8;++y)for(LONG x=0;x<8;++x) {
                D3DRECT pixel{x,y,x+1,y+1};
                ASSERT_EQ(device.p->Clear(1,&pixel,D3DCLEAR_TARGET,0xff000000u|unsigned(x*29)<<16|unsigned(y*31)<<8|unsigned(x+y),1,0),S_OK);
            }
            ASSERT_EQ(device.p->SetRenderTarget(savedTarget.p,savedDepth.p),S_OK);
            RECT sr{1,2,7,7},dr{2,1,5,3};
            IDirect3DSurface8 *src=route==1?system.p:target.p,*dst=route==0?system.p:target.p;
            ASSERT_EQ(D3DXLoadSurfaceFromSurface(readback.p,nullptr,nullptr,src,nullptr,nullptr,D3DX_FILTER_NONE,0),S_OK);
            auto beforeSource=ReadStorage("before-source",readback.p,S_OK,"staging-before");
            ASSERT_EQ(D3DXLoadSurfaceFromSurface(readback.p,nullptr,nullptr,dst,nullptr,nullptr,D3DX_FILTER_NONE,0),S_OK);
            auto beforeDest=ReadStorage("b-staging-before-"+std::to_string(route)+"-"+std::to_string(filter),readback.p,S_OK,"staging-before");
            Emit(out,beforeDest);
            HRESULT hr=D3DXLoadSurfaceFromSurface(dst,nullptr,&dr,src,nullptr,&sr,filter,0);
            HRESULT read=D3DXLoadSurfaceFromSurface(readback.p,nullptr,nullptr,dst,nullptr,nullptr,D3DX_FILTER_NONE,0);
            auto r=ReadStorage("b-staging-"+std::to_string(route)+"-"+std::to_string(filter),readback.p,hr,"staging-real");
            r.filter=filter;r.source=beforeSource.pixels;r.events+=",readback="+std::to_string(std::uint32_t(read));
            auto actual=Unhex(r.pixels),before=Unhex(beforeDest.pixels);
            const auto pixelBytes=PixelBytes(mode.Format);
            ASSERT_EQ(actual.size(),8u*8u*pixelBytes);
            ASSERT_EQ(before.size(),actual.size());
            for(LONG y=0;y<8;++y)for(LONG x=0;x<8;++x)
                if(x<dr.left || x>=dr.right || y<dr.top || y>=dr.bottom)
                    EXPECT_EQ(Hex(actual.data()+(y*8+x)*pixelBytes,pixelBytes),Hex(before.data()+(y*8+x)*pixelBytes,pixelBytes));
            // Compare staged real access with separate, directly lockable oracle
            // inputs containing the exact pre-operation resource bytes.
            Surface cpuSrc("s",8,8,mode.Format,events),cpuDst("d",8,8,mode.Format,events);
            auto sourceBytes=Unhex(beforeSource.pixels);
            ASSERT_EQ(sourceBytes.size(),actual.size());
            for(unsigned y=0;y<8;++y) {
                std::memcpy(cpuSrc.bytes.data()+y*cpuSrc.pitch,sourceBytes.data()+y*8*pixelBytes,8*pixelBytes);
                std::memcpy(cpuDst.bytes.data()+y*cpuDst.pitch,before.data()+y*8*pixelBytes,8*pixelBytes);
            }
            Device observer(events);observerDevice=&observer;
            auto cpu=Copy("staging-comparison",cpuSrc,cpuDst,filter,&sr,&dr);
            observerDevice=nullptr;
            EXPECT_EQ(hr,cpu.hr);EXPECT_EQ(r.pixels,Active(cpuDst));
            EXPECT_EQ(observer.refs,1u);EXPECT_EQ(cpuSrc.refs,1u);EXPECT_EQ(cpuDst.refs,1u);
            Emit(out,r);
        }
        for(unsigned route=0;route<2;++route)for(unsigned failure=0;failure<4;++failure) {
            Upload(system.p,input.bytes.data(),input.pitch);
            ASSERT_EQ(device.p->SetRenderTarget(target.p,nullptr),S_OK);
            ASSERT_EQ(device.p->Clear(0,nullptr,D3DCLEAR_TARGET,0xff123456,1,0),S_OK);
            ASSERT_EQ(device.p->SetRenderTarget(savedTarget.p,savedDepth.p),S_OK);
            Surface observed("owned-target",8,8,mode.Format,events);
            ASSERT_EQ(target.p->GetDesc(&observed.desc),S_OK);
            observed.owner=targetTexture.p;observed.lockResult=E_FAIL;
            StagingDevice observer(events,device.p);observer.observed=&observed;observer.underlying=target.p;
            observer.failCreate=failure==1;observer.failCopy=failure>=2?failure-1:0;
            IDirect3DSurface8 *actualDest=route?target.p:system.p;
            ASSERT_EQ(D3DXLoadSurfaceFromSurface(readback.p,nullptr,nullptr,actualDest,nullptr,nullptr,D3DX_FILTER_NONE,0),S_OK);
            auto before=ReadStorage("before",readback.p,S_OK,"before");
            const ULONG ownerRefs=targetTexture.p->AddRef();targetTexture.p->Release();
            const ULONG realDeviceRefs=device.p->AddRef();device.p->Release();
            RECT sr{1,2,7,7},dr{2,1,5,3};events.clear();observerDevice=&observer;
            HRESULT hr=D3DXLoadSurfaceFromSurface(route?&observed:system.p,nullptr,&dr,
                route?system.p:&observed,nullptr,&sr,D3DX_FILTER_TRIANGLE,0);
            observerDevice=nullptr;auto transcript=Events(events);
            ASSERT_EQ(D3DXLoadSurfaceFromSurface(readback.p,nullptr,nullptr,actualDest,nullptr,nullptr,D3DX_FILTER_NONE,0),S_OK);
            auto r=ReadStorage("b-stage-failure-"+std::to_string(route)+"-"+std::to_string(failure),readback.p,hr,"staging-policy");
            r.source=before.pixels;r.events=transcript;r.filter=D3DX_FILTER_TRIANGLE;Emit(out,r);
            EXPECT_EQ(observer.refs,1u);EXPECT_EQ(observed.refs,1u);EXPECT_FALSE(observed.locked);
            EXPECT_EQ(targetTexture.p->AddRef(),ownerRefs);targetTexture.p->Release();
            EXPECT_EQ(device.p->AddRef(),realDeviceRefs);device.p->Release();
            if(failure==1)EXPECT_EQ(hr,E_OUTOFMEMORY);
            else if(failure==2)EXPECT_EQ(hr,E_FAIL);
            else EXPECT_EQ(hr,S_OK);
            if(failure==1 || failure==2 || (route==1 && failure==3))EXPECT_EQ(r.pixels,before.pixels);
            EXPECT_EQ(observer.copies,failure==1?0u:(failure==2?1u:(route?2u:1u)));
        }
    } else {Row r;r.id="b-target-create";r.kind="adapter-limitation";r.hr=targetResult;Emit(out,r);}
    COM<IDirect3DSurface8> back,standalone,saved,savedDepth;
    ASSERT_EQ(device.p->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back.p),S_OK);
    HRESULT standaloneResult=device.p->CreateRenderTarget(64,64,mode.Format,D3DMULTISAMPLE_NONE,FALSE,&standalone.p);
    ASSERT_EQ(device.p->GetRenderTarget(&saved.p),S_OK);device.p->GetDepthStencilSurface(&savedDepth.p);
    for(unsigned resource=0;resource<2;++resource) {
        if(resource && FAILED(standaloneResult)) {
            Row r;r.id="b-standalone-create";r.hr=standaloneResult;r.kind="adapter-limitation";Emit(out,r);continue;
        }
        auto *target=resource?standalone.p:back.p;
        for(unsigned route=0;route<2;++route)for(DWORD filter:{D3DX_FILTER_BOX,D3DX_FILTER_TRIANGLE}) {
            COM<IDirect3DSurface8> system,sourceRead,destRead;
            ASSERT_EQ(device.p->CreateImageSurface(8,8,mode.Format,&system.p),S_OK);
            Surface input("input",8,8,mode.Format,events);Fill(input,0);Upload(system.p,input.bytes.data(),input.pitch);
            ASSERT_EQ(device.p->SetRenderTarget(target,nullptr),S_OK);
            ASSERT_EQ(device.p->Clear(0,nullptr,D3DCLEAR_TARGET,0xff40a0c0,1,0),S_OK);
            ASSERT_EQ(device.p->SetRenderTarget(saved.p,savedDepth.p),S_OK);
            auto *src=route?system.p:target,*dst=route?target:system.p;
            D3DSURFACE_DESC sd{},dd{};ASSERT_EQ(src->GetDesc(&sd),S_OK);ASSERT_EQ(dst->GetDesc(&dd),S_OK);
            ASSERT_EQ(device.p->CreateImageSurface(sd.Width,sd.Height,mode.Format,&sourceRead.p),S_OK);
            ASSERT_EQ(device.p->CreateImageSurface(dd.Width,dd.Height,mode.Format,&destRead.p),S_OK);
            ASSERT_EQ(D3DXLoadSurfaceFromSurface(sourceRead.p,nullptr,nullptr,src,nullptr,nullptr,D3DX_FILTER_NONE,0),S_OK);
            ASSERT_EQ(D3DXLoadSurfaceFromSurface(destRead.p,nullptr,nullptr,dst,nullptr,nullptr,D3DX_FILTER_NONE,0),S_OK);
            auto beforeSource=ReadStorage("source",sourceRead.p,S_OK,"before");
            auto beforeDest=ReadStorage("before",destRead.p,S_OK,"before");
            RECT sr{1,2,7,7},dr{2,1,5,3};
            HRESULT hr=D3DXLoadSurfaceFromSurface(dst,nullptr,&dr,src,nullptr,&sr,filter,0);
            ASSERT_EQ(D3DXLoadSurfaceFromSurface(destRead.p,nullptr,nullptr,dst,nullptr,nullptr,D3DX_FILTER_NONE,0),S_OK);
            auto r=ReadStorage("b-raw-staging-"+std::to_string(resource)+"-"+std::to_string(route)+"-"+std::to_string(filter),destRead.p,hr,"raw-staging");
            r.sf=sd.Format;r.sw=sd.Width;r.sh=sd.Height;r.source=beforeSource.pixels;r.filter=filter;
            Surface cpuSrc("s",sd.Width,sd.Height,mode.Format,events),cpuDst("d",dd.Width,dd.Height,mode.Format,events);
            auto sbytes=Unhex(beforeSource.pixels),dbytes=Unhex(beforeDest.pixels);
            const auto pixelBytes=PixelBytes(mode.Format);
            ASSERT_EQ(sbytes.size(),sd.Width*sd.Height*pixelBytes);ASSERT_EQ(dbytes.size(),dd.Width*dd.Height*pixelBytes);
            for(unsigned y=0;y<sd.Height;++y)std::memcpy(cpuSrc.bytes.data()+y*cpuSrc.pitch,sbytes.data()+y*sd.Width*pixelBytes,sd.Width*pixelBytes);
            for(unsigned y=0;y<dd.Height;++y)std::memcpy(cpuDst.bytes.data()+y*cpuDst.pitch,dbytes.data()+y*dd.Width*pixelBytes,dd.Width*pixelBytes);
            Device observer(events);observerDevice=&observer;
            auto cpu=Copy("raw-staging-comparison",cpuSrc,cpuDst,filter,&sr,&dr);observerDevice=nullptr;
            EXPECT_EQ(hr,cpu.hr);EXPECT_EQ(r.pixels,Active(cpuDst));EXPECT_EQ(observer.refs,1u);
            Emit(out,r);
        }
    }
    std::printf("Sampling real-device cross-check: %u agreements, %u creation limitations\n",agreements,limitations);
}

using EncoderBlock=std::array<D3DXCOLOR,16>;
struct EncoderFixture {std::string name;EncoderBlock colors;bool bytes=true;};
D3DXCOLOR ByteColor(unsigned r,unsigned g,unsigned b,unsigned a=255) {
    const float unit=1.0f/255.0f;return D3DXCOLOR(r*unit,g*unit,b*unit,a*unit);
}
HRESULT EncodeBlock(D3DFORMAT f,unsigned char *out,const EncoderBlock &block) {
#if !defined(_M_IX86)
    return LegacyD3DX::EncodeResourceBlock(f,out,block.data());
#else
    if(f==D3DFMT_DXT1)return D3DXEncodeDXT1(out,block.data());
    if(f==D3DFMT_DXT3)return D3DXEncodeDXT3(out,block.data());
    return D3DXEncodeDXT5(out,block.data());
#endif
}
std::string StorageActive(const Surface &s) {
    std::string value;for(unsigned y=0;y<StorageRows(s.desc.Format,s.desc.Height);++y)
        value+=Hex(s.bytes.data()+y*s.pitch,RowBytes(s.desc.Format,s.desc.Width));return value;
}
std::vector<EncoderFixture> EncoderFixtures() {
    std::vector<EncoderFixture> v;
    auto add=[&](std::string name,auto pixel,bool bytes=true) {
        EncoderFixture f;f.name=name;f.bytes=bytes;for(unsigned i=0;i<16;++i)f.colors[i]=pixel(i);v.push_back(f);
    };
    for(unsigned n:{0u,1u,7u,8u,127u,128u,254u,255u})add("constant-"+std::to_string(n),[=](unsigned){return ByteColor(n,n,n);});
    for(unsigned count:{1u,3u,8u,15u})for(unsigned reverse:{0u,1u})
        add("frequency-"+std::to_string(count)+"-"+std::to_string(reverse),[=](unsigned i){return (reverse?15-i:i)<count?ByteColor(255,0,0):ByteColor(0,255,0);});
    add("three-colors",[](unsigned i){return i%3==0?ByteColor(255,0,0):(i%3==1?ByteColor(0,255,0):ByteColor(0,0,255));});
    add("four-colors",[](unsigned i){return ByteColor((i&1)?255:0,(i&2)?255:0,(i&1)?0:255);});
    add("near-equal",[](unsigned i){return ByteColor(123+i%4,127+(i*3)%4,129-i%3);});
    add("isolated-outlier",[](unsigned i){return i==7?ByteColor(250,1,19):ByteColor(64,128,192);});
    add("weighted-axis",[](unsigned i){return ByteColor((i&1)?200:20,(i&2)?140:80,(i&4)?190:40);});
    for(unsigned channel=0;channel<4;++channel)add("gradient-"+std::to_string(channel),[=](unsigned i){return ByteColor(channel==0?i*17:64,channel==1?i*17:128,channel==2?i*17:192,channel==3?i*17:255);});
    add("checker",[](unsigned i){return ((i%4+i/4)&1)?ByteColor(3,252,129):ByteColor(252,3,126);});
    for(unsigned shift:{0u,1u,2u})add("selector-threshold-"+std::to_string(shift),[=](unsigned i){const unsigned q[]={0,255,42,43,127,128,212,213};unsigned n=q[(i+shift)%8];return ByteColor(n,n,n);});
    for(unsigned a:{0u,127u,128u,255u})add("alpha-constant-"+std::to_string(a),[=](unsigned i){return ByteColor((i&1)?255:0,(i&2)?255:0,90,a);});
    for(unsigned rgb:{0u,255u})add("transparent-rgb-"+std::to_string(rgb),[=](unsigned i){return i==5?ByteColor(rgb,rgb,rgb,127):ByteColor(200,40,90);});
    add("alpha-six-step",[](unsigned i){const unsigned a[]={0,51,102,153,204,255};return ByteColor(64,128,192,a[i%6]);});
    add("alpha-eight-step",[](unsigned i){return ByteColor(64,128,192,16+(i%8)*32);});
    add("alpha-near-ties",[](unsigned i){const unsigned a[]={0,255,16,240,31,32,33,47,48,49,127,128,129,223,224,225};return ByteColor(64,128,192,a[i]);});
    add("alpha-reverse",[](unsigned i){const unsigned a[]={0,255,16,240,31,32,33,47,48,49,127,128,129,223,224,225};return ByteColor(64,128,192,a[15-i]);});
    for(unsigned a:{0u,16u,64u})for(unsigned count:{1u,8u,15u})add("alpha-two-"+std::to_string(a)+"-"+std::to_string(count),[=](unsigned i){return ByteColor(64,128,192,i<count?a:255-a);});
    // Exact binary32 alpha threshold, including the mathematical midpoint.
    for(unsigned side=0;side<3;++side)add("float-alpha-half-"+std::to_string(side),[=](unsigned i){auto p=ByteColor(i*17,255-i*17,64);p.a=side==0?std::nextafter(0.5f,0.0f):(side==1?0.5f:std::nextafter(0.5f,1.0f));return p;},false);
    for(unsigned k=0;k<15;++k)for(unsigned side=0;side<3;++side)
        add("float-alpha-nibble-"+std::to_string(k)+"-"+std::to_string(side),[=](unsigned){
            auto p=ByteColor(64,128,192);float q=(k+0.5f)/15.0f;
            p.a=side==1?q:std::nextafter(q,side==0?0.0f:1.0f);return p;},false);
    // Purposeful exhaustive 5/6-bit endpoint boundaries, not an image sweep.
    for(unsigned c=0;c<3;++c)for(unsigned k=0;k<(c==1?63u:31u);++k)for(unsigned side=0;side<3;++side)
        add("float-565-"+std::to_string(c)+"-"+std::to_string(k)+"-"+std::to_string(side),[=](unsigned){
            D3DXCOLOR p(0,0,0,1);float q=(k+0.5f)/(c==1?63.0f:31.0f);
            if(side!=1)q=std::nextafter(q,side==0?0.0f:1.0f);
            (&p.r)[c]=q;return p;},false);
    return v;
}

// Independent *partial* decision checker: DXT3 alpha raster diffusion only.
// Does not fit RGB endpoints, call any oracle routine, or replace a helper.
std::array<unsigned char,8> CheckDXT3Alpha(const EncoderBlock &block) {
    unsigned previous=0;_controlfp_s(&previous,0,0);unsigned ignored=0;
    _controlfp_s(&ignored,_RC_CHOP,_MCW_RC);
    float error[16]={};std::uint64_t packed=0;
    for(unsigned i=0;i<16;++i) {
        const double sum=double(block[i].a)+double(error[i]);
        volatile float adjusted=static_cast<float>(sum);
        volatile float scaled=static_cast<float>(sum*15.0+0.5);
        const unsigned q=static_cast<unsigned>(scaled);packed|=std::uint64_t(q)<<(4*i);
        const double residual=double(adjusted)-double(q)*double(1.0f/15.0f);
        auto diffuse=[&](unsigned at,double weight){error[at]=static_cast<float>(double(error[at])+residual*weight);};
        if(i%4!=3)diffuse(i+1,7.0/16);
        if(i<12) {if(i%4)diffuse(i+3,3.0/16);diffuse(i+4,5.0/16);if(i%4!=3)diffuse(i+5,1.0/16);}
    }
    _controlfp_s(&ignored,previous,_MCW_RC);
    std::array<unsigned char,8> result{};for(unsigned i=0;i<8;++i)result[i]=static_cast<unsigned char>(packed>>(i*8));return result;
}
std::string PredictEncoder(const EncoderBlock &block,D3DFORMAT format) {
#if !defined(_M_IX86)
    unsigned char output[16]={};EXPECT_EQ(LegacyD3DX::EncodeResourceBlock(format,output,block.data()),S_OK);
    return Hex(output,RowBytes(format,4));
#else
    auto color=ResourceEncoderModel::Color(block.data(),format==D3DFMT_DXT1);
    EXPECT_TRUE(color.valid);
    std::string bytes;
    if(format==D3DFMT_DXT3){auto alpha=CheckDXT3Alpha(block);bytes=Hex(alpha.data(),8);}
    if(format==D3DFMT_DXT5){auto alpha=ResourceEncoderModel::Alpha5(block.data());bytes=Hex(alpha.bytes.data(),8);}
    return bytes+Hex(color.bytes.data(),8);
#endif
}
std::vector<Row> encoderResourceCases;
void GenerateEncoderEvidence() {
    encoderHits=0;encoderResourceCases.clear();std::ofstream out(evidence/"encoder.jsonl");ASSERT_TRUE(out.good());
    ASSERT_EQ(encoderReference.size(),128u);
    std::vector<std::string> events;Device device(events);observerDevice=&device;
    auto direct=[&](const EncoderFixture &fixture,D3DFORMAT f) {
        unsigned char bytes[16]={};Row r;r.id="c-direct-"+fixture.name+"-"+std::to_string(f);r.kind="encoder-direct";
        r.sw=r.sh=r.dw=r.dh=4;r.df=f;r.pitch=RowBytes(f,4);r.hr=EncodeBlock(f,bytes,fixture.colors);
        r.source=Hex(fixture.colors.data(),sizeof(fixture.colors));r.pixels=Hex(bytes,r.pitch);r.events="input=RGBA-binary32;decision="+fixture.name;
        EXPECT_EQ(r.hr,S_OK);Emit(out,r);
        EXPECT_EQ(PredictEncoder(fixture.colors,f),r.pixels)<<fixture.name;
        if(f==D3DFMT_DXT3) {auto alpha=CheckDXT3Alpha(fixture.colors);EXPECT_EQ(Hex(alpha.data(),alpha.size()),Hex(bytes,8))<<fixture.name;}
        return r;
    };
    for(const auto &fixture:EncoderFixtures())for(auto f:{D3DFMT_DXT1,D3DFMT_DXT3,D3DFMT_DXT5}) {
        auto encoded=direct(fixture,f);
        if(!fixture.bytes)continue;
        Surface src("s",4,4,D3DFMT_A8R8G8B8,events),dst("d",4,4,f,events);
        for(unsigned i=0;i<16;++i) {
            const auto &p=fixture.colors[i];auto *b=src.bytes.data()+(i/4)*src.pitch+(i%4)*4;
            b[0]=static_cast<unsigned char>(p.b*255+0.5f);b[1]=static_cast<unsigned char>(p.g*255+0.5f);
            b[2]=static_cast<unsigned char>(p.r*255+0.5f);b[3]=static_cast<unsigned char>(p.a*255+0.5f);
        }
        auto r=Copy("c-resource-"+fixture.name+"-"+std::to_string(f),src,dst,D3DX_FILTER_BOX);
        EXPECT_EQ(r.hr,S_OK);EXPECT_EQ(StorageActive(dst),encoded.pixels)<<fixture.name;
        Emit(out,r);encoderResourceCases.push_back(r);
    }
    for(unsigned a=0;a<256;++a) {
        EncoderFixture fixture;fixture.name="alpha-scalar-"+std::to_string(a);fixture.colors.fill(ByteColor(64,128,192,a));
        direct(fixture,D3DFMT_DXT3);
    }
    // Validation-only blocks are not used to derive the diffusion checker.
    unsigned state=0x9264c31u;
    for(unsigned n=0;n<32;++n) {
        EncoderFixture fixture;fixture.name="alpha-validation-"+std::to_string(n);
        for(auto &p:fixture.colors){state=state*1664525u+1013904223u;p=ByteColor(64,128,192,state>>24);}
        direct(fixture,D3DFMT_DXT3);
    }
    for(auto dims:{std::array<unsigned,2>{1,1},{2,2},{1,4},{4,1},{2,4},{4,2}})
        for(auto f:{D3DFMT_DXT1,D3DFMT_DXT3,D3DFMT_DXT5}) {
            Surface src("s",dims[0],dims[1],D3DFMT_A8R8G8B8,events),dst("d",dims[0],dims[1],f,events);Threshold(src,1);
            // Opaque RGB is necessary to distinguish DXT1 extension policies;
            // an entirely transparent source would collapse to the sentinel.
            if(f==D3DFMT_DXT1)for(unsigned y=0;y<dims[1];++y)for(unsigned x=0;x<dims[0];++x)
                src.bytes[y*src.pitch+x*4+3]=255;
            auto r=Copy("c-small-"+std::to_string(dims[0])+"-"+std::to_string(dims[1])+"-"+std::to_string(f),src,dst,D3DX_FILTER_BOX);
            for(unsigned mode=0;mode<3;++mode) {
                EncoderBlock block{};
                for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x) {
                    if(mode==2 && (x>=dims[0] || y>=dims[1])){block[y*4+x]=D3DXCOLOR(0,0,0,0);continue;}
                    unsigned xx=mode==0?x%dims[0]:std::min(x,dims[0]-1),yy=mode==0?y%dims[1]:std::min(y,dims[1]-1);
                    auto *p=src.bytes.data()+yy*src.pitch+xx*4;block[y*4+x]=ByteColor(p[2],p[1],p[0],p[3]);
                }
                unsigned char bytes[16]={};ASSERT_EQ(EncodeBlock(f,bytes,block),S_OK);
                if(mode==0)EXPECT_EQ(PredictEncoder(block,f),Hex(bytes,RowBytes(f,4)));
                r.events+=";extension-"+std::to_string(mode)+"="+std::to_string(StorageActive(dst)==Hex(bytes,RowBytes(f,4)));
                if(mode==0)EXPECT_EQ(StorageActive(dst),Hex(bytes,RowBytes(f,4)));
            }
            EXPECT_EQ(r.hr,S_OK);Emit(out,r);encoderResourceCases.push_back(r);
        }
    // Half-size BOX retains fractional channel values before the float codec.
    // Contrast with the explicitly byte-materialized two-call route; do not
    // use that route as an assumed model of the direct compressed operation.
    for(unsigned pattern=0;pattern<3;++pattern)for(auto f:{D3DFMT_DXT1,D3DFMT_DXT3,D3DFMT_DXT5}) {
        Surface src("s",8,8,D3DFMT_A8R8G8B8,events),dst("d",4,4,f,events),
            intermediate("m",4,4,D3DFMT_A8R8G8B8,events),via("v",4,4,f,events);
        EncoderFixture fixture;fixture.name="box-float-"+std::to_string(pattern);fixture.bytes=false;
        for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x) {
            auto *p=src.bytes.data()+y*src.pitch+x*4;
            for(unsigned c=0;c<4;++c)p[c]=static_cast<unsigned char>(
                (pattern==0?((x/2+y/2*4)*17+c*37+(x%2)*1+(y%2)*2):
                 pattern==1?(x*31+y*17+c*43):((x/2)*7+(y/2)*19+c*61+(x%2+y%2)*3))&255);
        }
        for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x) {
            float channels[4]={};
            for(unsigned c=0;c<4;++c)for(unsigned yy=0;yy<2;++yy)for(unsigned xx=0;xx<2;++xx)
                channels[c]+=src.bytes[(y*2+yy)*src.pitch+(x*2+xx)*4+c]*(1.0f/255.0f)*0.25f;
            fixture.colors[y*4+x]=D3DXCOLOR(channels[2],channels[1],channels[0],channels[3]);
        }
        const std::string id="c-boundary-"+std::to_string(pattern)+"-"+std::to_string(f);
        auto r=Copy(id,src,dst,D3DX_FILTER_BOX);
        auto first=Copy(id+"-byte-stage",src,intermediate,D3DX_FILTER_BOX);EXPECT_EQ(first.hr,S_OK);
        auto second=Copy(id+"-byte-encode",intermediate,via,D3DX_FILTER_BOX);EXPECT_EQ(second.hr,S_OK);
        auto encoded=direct(fixture,f);
        r.events+=";float-match="+std::to_string(StorageActive(dst)==encoded.pixels)+
            ";byte-intermediate-match="+std::to_string(StorageActive(dst)==StorageActive(via));
        EXPECT_EQ(StorageActive(dst),encoded.pixels)<<id;
        EXPECT_EQ(r.hr,S_OK);Emit(out,r);Emit(out,first);Emit(out,second);encoderResourceCases.push_back(r);
    }
    EXPECT_EQ(device.refs,1u);observerDevice=nullptr;EXPECT_EQ(encoderHits,encoderReference.size());
}
TEST(LegacyResourceCharacterization, CompressedEncoderDecisions) {GenerateEncoderEvidence();}

#if defined(_M_IX86)
TEST(LegacyResourceCharacterization, NumericalEncoderDecisions) {
    unsigned orientations=0,colorLimit=0,alphaLimit=0,initial=0,final=0;
    _controlfp_s(&initial,0,0);
    for(const auto &fixture:EncoderFixtures()) {
        unsigned char oracle[16]={};ASSERT_EQ(EncodeBlock(D3DFMT_DXT5,oracle,fixture.colors),S_OK);
        auto predicted=ResourceEncoderModel::Alpha5(fixture.colors.data());
        alphaLimit=std::max(alphaLimit,predicted.iterations);
        EXPECT_EQ(Hex(predicted.bytes.data(),8),Hex(oracle,8))<<fixture.name
            <<" steps="<<predicted.steps<<" iterations="<<predicted.iterations;
        for(auto format:{D3DFMT_DXT1,D3DFMT_DXT3,D3DFMT_DXT5}) {
            ASSERT_EQ(EncodeBlock(format,oracle,fixture.colors),S_OK);
            auto color=ResourceEncoderModel::Color(fixture.colors.data(),format==D3DFMT_DXT1);
            orientations|=1u<<color.orientation;colorLimit=std::max(colorLimit,color.iterations);
            ASSERT_TRUE(color.valid)<<fixture.name;
            EXPECT_EQ(Hex(color.bytes.data(),8),Hex(oracle+(format==D3DFMT_DXT1?0:8),8))<<fixture.name
                <<" format="<<format<<" orientation="<<color.orientation<<" iterations="<<color.iterations;
        }
    }
    _controlfp_s(&final,0,0);EXPECT_EQ(initial&(_MCW_RC|_MCW_PC),final&(_MCW_RC|_MCW_PC));
    std::printf("Existing numerical fixtures: 471 blocks; orientations=%u max_iterations=%u/%u FPU=%x\n",orientations,colorLimit,alphaLimit,initial);
}

// Fixed holdout set authored after the model matched the complete C fixture
// set. No expected output table, search or oracle-derived decision is used.
std::vector<EncoderBlock> EncoderValidationBlocks() {
    std::vector<EncoderBlock> blocks(32);unsigned state=0x649d2c11u;
    for(unsigned n=0;n<blocks.size();++n)for(unsigned i=0;i<16;++i) {
        auto byte=[&](){state=state*1664525u+1013904223u;return state>>24;};
        unsigned r=byte(),g=byte(),b=byte(),a=byte();
        auto p=ByteColor(r,g,b,a);
        if(n<8) {
            if(n%4==0)p.a=i%3==0?0.0f:1.0f;
            if(n%4==1)p.a=ByteColor(0,0,0,16+a%224).a;
            if(n%4==2)p.a=1;
            if(n%4==3)p.a=0;
        } else if(n<16) {
            p=ByteColor(i<(n-7)?17:229,i<(n-7)?203:31,(i&1)?61:197,32+a%192);
            if(n&1)std::swap(p.g,p.b);
        } else if(n<24) {
            float midpoint=(float((n-16)*3)+0.5f)/31.0f;
            p.r=(i&1)?std::nextafter(midpoint,1.0f):std::nextafter(midpoint,0.0f);
            p.g=p.r;p.b=p.r;
            p.a=(i%3==0)?std::nextafter(0.5f,0.0f):((i%3==1)?0.5f:std::nextafter(0.5f,1.0f));
        } else {
            // Fractional normalized filter-like values; explicit extrema select
            // six-step fitting, while order/asymmetry exercises diffusion.
            p.r=(r%255+0.25f)*(1.0f/255);p.g=(g%255+0.5f)*(1.0f/255);p.b=(b%255+0.75f)*(1.0f/255);
            p.a=i==0?0.0f:(i==15?1.0f:(a%255+0.25f)*(1.0f/255));
        }
        blocks[n][i]=p;
    }
    return blocks;
}
TEST(LegacyResourceCharacterization, NumericalEncoderHoldout) {
    std::ofstream out(evidence/"numerical-validation.jsonl");ASSERT_TRUE(out.good());
    unsigned differences=0,orientationMask=0,alphaModes=0,maxColorIterations=0,maxAlphaIterations=0;
    auto blocks=EncoderValidationBlocks();
    for(unsigned n=0;n<blocks.size();++n) {
        for(const auto &p:blocks[n])for(unsigned c=0;c<4;++c){ASSERT_GE((&p.r)[c],0);ASSERT_LE((&p.r)[c],1);}
        auto color=ResourceEncoderModel::Color(blocks[n].data(),false);
        auto alpha=ResourceEncoderModel::Alpha5(blocks[n].data());
        auto conventional=ResourceEncoderModel::Alpha5(blocks[n].data(),true);
        differences+=alpha.bytes!=conventional.bytes;orientationMask|=1u<<color.orientation;
        alphaModes|=1u<<alpha.steps;maxColorIterations=std::max(maxColorIterations,color.iterations);
        maxAlphaIterations=std::max(maxAlphaIterations,alpha.iterations);
        for(auto f:{D3DFMT_DXT1,D3DFMT_DXT3,D3DFMT_DXT5}) {
            unsigned char oracle[16]={};ASSERT_EQ(EncodeBlock(f,oracle,blocks[n]),S_OK);
            EXPECT_EQ(PredictEncoder(blocks[n],f),Hex(oracle,RowBytes(f,4)))<<"holdout="<<n<<" format="<<f;
            Row r;r.id="d-validation-"+std::to_string(n)+"-"+std::to_string(f);r.kind="encoder-numerical-validation";
            r.sw=r.sh=r.dw=r.dh=4;r.df=f;r.pitch=RowBytes(f,4);r.source=Hex(blocks[n].data(),sizeof(EncoderBlock));
            r.pixels=Hex(oracle,r.pitch);r.events="prediction="+PredictEncoder(blocks[n],f);Emit(out,r);
        }
    }
    EXPECT_GT(differences,0u);EXPECT_EQ(orientationMask,15u);
    EXPECT_TRUE(alphaModes&(1u<<6));EXPECT_TRUE(alphaModes&(1u<<8));
    std::printf("Numerical holdout: 32 blocks / 96 full encodings; orientations=%u alpha_modes=%u max_iterations=%u/%u coefficient_mutations=%u\n",
        orientationMask,alphaModes,maxColorIterations,maxAlphaIterations,differences);
}

TEST(LegacyResourceCharacterization, ProductionEncoderCompatibility) {
    auto check=[](const EncoderBlock &block){
        for(auto format:{D3DFMT_DXT1,D3DFMT_DXT3,D3DFMT_DXT5}){
            unsigned char oracle[16]={},actual[16]={};
            ASSERT_EQ(EncodeBlock(format,oracle,block),S_OK);
            ASSERT_EQ(LegacyD3DX::EncodeResourceBlock(format,actual,block.data()),S_OK);
            EXPECT_EQ(Hex(actual,RowBytes(format,4)),Hex(oracle,RowBytes(format,4)));
        }
    };
    for(const auto &fixture:EncoderFixtures()){SCOPED_TRACE(fixture.name);check(fixture.colors);}
    for(const auto &block:EncoderValidationBlocks())check(block);
    // GameLogic explicitly selects PC_24. Keep that Win32 caller contract even
    // though the frozen X64.9 reference corpus was captured at PC_53.
    unsigned original=0,ignored=0,after=0;_controlfp_s(&original,0,0);
    _controlfp_s(&ignored,_PC_24,_MCW_PC);
    for(const auto &fixture:EncoderFixtures()){SCOPED_TRACE(fixture.name);check(fixture.colors);}
    for(const auto &block:EncoderValidationBlocks())check(block);
    _controlfp_s(&after,0,0);EXPECT_EQ(after&_MCW_PC,_PC_24);
    _controlfp_s(&ignored,original,_MCW_PC|_MCW_RC);
}

TEST(LegacyResourceCharacterization, ProductionDecodeCompatibility) {
    std::vector<std::string> events;Device device(events);observerDevice=&device;
    for(const auto &fixture:EncoderFixtures())for(auto format:{D3DFMT_DXT1,D3DFMT_DXT3,D3DFMT_DXT5}){
        SCOPED_TRACE(fixture.name);Surface source("s",4,4,format,events),expected("a",4,4,D3DFMT_A8R8G8B8,events),actual("b",4,4,D3DFMT_A8R8G8B8,events);
        ASSERT_EQ(EncodeBlock(format,source.bytes.data(),fixture.colors),S_OK);
        ASSERT_EQ(PinnedResourceSurfaceCopy(&expected,nullptr,nullptr,&source,nullptr,nullptr,D3DX_FILTER_NONE,0),S_OK);
        ASSERT_EQ(LegacyD3DX::LoadSurfaceFromSurface(&actual,nullptr,nullptr,&source,nullptr,nullptr,D3DX_FILTER_NONE,0),S_OK);
        EXPECT_EQ(actual.bytes,expected.bytes);
    }
    observerDevice=nullptr;
}
#endif

TEST(LegacyResourceCharacterization, RealCompressedEncoderDecisions) {
    if(encoderResourceCases.empty())GenerateEncoderEvidence();
    std::ofstream out(evidence/"encoder-device.jsonl");ASSERT_TRUE(out.good());
    struct Module {HMODULE h=LoadLibraryExW(L"d3d8.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);~Module(){if(h)FreeLibrary(h);}} module;
    if(!module.h)GTEST_SKIP()<<"System Win32 DX8 unavailable";
    auto create=reinterpret_cast<IDirect3D8 *(WINAPI *)(UINT)>(GetProcAddress(module.h,"Direct3DCreate8"));ASSERT_NE(create,nullptr);
    COM<IDirect3D8> d3d;d3d.p=create(D3D_SDK_VERSION);ASSERT_NE(d3d.p,nullptr);
    struct Window {HWND h=CreateWindowExW(0,L"STATIC",L"DXT encoder characterization",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);~Window(){if(h)DestroyWindow(h);}} window;
    ASSERT_NE(window.h,nullptr);D3DDISPLAYMODE mode{};ASSERT_EQ(d3d.p->GetAdapterDisplayMode(0,&mode),S_OK);
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.BackBufferFormat=mode.Format;pp.BackBufferCount=1;
    pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window.h;pp.Windowed=TRUE;COM<IDirect3DDevice8> device;
    HRESULT available=d3d.p->CreateDevice(0,D3DDEVTYPE_HAL,window.h,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,&device.p);
    if(FAILED(available))GTEST_SKIP()<<"HAL unavailable: "<<std::hex<<available;
    unsigned agreements=0;
    for(const auto &cpu:encoderResourceCases) {
        COM<IDirect3DSurface8> src,dst;COM<IDirect3DTexture8> texture;
        ASSERT_EQ(device.p->CreateImageSurface(cpu.sw,cpu.sh,D3DFMT_A8R8G8B8,&src.p),S_OK);
        // A supported aligned parent supplies actual 1/2-wide mip surfaces.
        HRESULT hr=device.p->CreateTexture(cpu.dw*4,cpu.dh*4,0,0,D3DFORMAT(cpu.df),D3DPOOL_MANAGED,&texture.p);
        if(FAILED(hr)){Row r;r.id="real-"+cpu.id;r.kind="adapter-limitation";r.hr=hr;Emit(out,r);continue;}
        ASSERT_EQ(texture.p->GetSurfaceLevel(2,&dst.p),S_OK);
        auto source=Unhex(cpu.source);unsigned sourcePitch=unsigned((source.size()-16)/cpu.sh);Upload(src.p,source.data(),sourcePitch);
        hr=D3DXLoadSurfaceFromSurface(dst.p,nullptr,nullptr,src.p,nullptr,nullptr,D3DX_FILTER_BOX,0);
        auto r=ReadStorage("real-"+cpu.id,dst.p,hr,"encoder-real");auto expected=Unhex(cpu.pixels);
        std::string active;for(unsigned y=0;y<StorageRows(D3DFORMAT(cpu.df),cpu.dh);++y)active+=Hex(expected.data()+y*cpu.pitch,RowBytes(D3DFORMAT(cpu.df),cpu.dw));
        EXPECT_EQ(hr,cpu.hr);EXPECT_EQ(r.pixels,active)<<cpu.id;Emit(out,r);++agreements;
    }
    std::printf("Encoder real-device agreements: %u\n",agreements);
}
}
