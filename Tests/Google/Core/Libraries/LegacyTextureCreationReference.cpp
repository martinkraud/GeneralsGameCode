// SPDX-License-Identifier: GPL-3.0-or-later
#include "LegacyTextureCreationCases.h"
#include "LegacyTextureCreationReferenceAPI.h"
#include <cstring>
using namespace TextureCreationCases;

int main(int argc,char **argv) {
    if (argc==2 && std::strcmp(argv[1],"--examples")==0) {
        std::puts("// Actual pinned Win32 D3DX8 results, no replacement.\nstatic const Result textureExamples[] = {");
        Examples([](const Input &i) {
            auto r=Run<Legacy>(i);
            std::printf("{HRESULT(0x%08lxu),%u,{",r.hr,r.output);
            for(auto e:r.events) {
                unsigned size=8; while(size>1 && e[size-1]==0)--size;
                std::printf("{"); for(unsigned n=0;n<size;++n)std::printf("%uu,",e[n]); std::printf("},");
            }
            std::printf("},{"); for(auto w:r.ownership)std::printf("%luu,",w); std::puts("}},");
        });
        std::puts("};"); return 0;
    }
    if (argc==2 && std::strcmp(argv[1],"--policy")==0) {
        std::puts("// Pinned Win32 D3DX8 7bddff8c, measured creation format policy.\n"
                  "// Candidate query order; rank 0=ineligible, lower nonzero rank=preferred.\n"
                  "inline constexpr unsigned textureFormats[] = {");
        for (auto f:formats) std::printf("%uu,",f);
        std::puts("\n};\ninline constexpr unsigned char textureFormatRanks[][43] = {");
        for (unsigned request=0;request<=std::size(formats);++request) {
            Input i=Supported(); i.format=D3DFORMAT(request ? formats[request-1] : 0);
            std::array<unsigned,43> rank{};
            for(unsigned n=1;n<=std::size(formats);++n) {
                auto r=Run<Legacy>(i); if(FAILED(r.hr))break;
                if(r.events.empty() || r.events.back()[0]!=Create)return 2;
                const auto selected=D3DFORMAT(r.events.back()[6]);
                auto pos=std::find(std::begin(formats),std::end(formats),UINT(selected));
                if(pos==std::end(formats))return 3;
                rank[pos-std::begin(formats)]=n;
                i.supported.erase(std::remove(i.supported.begin(),i.supported.end(),selected),i.supported.end());
            }
            std::printf("{"); for(auto r:rank)std::printf("%u,",r); std::puts("},");
        }
        std::puts("};"); return 0;
    }
    std::puts("// Pinned Win32 D3DX8 7bddff8c, actual CPU recording transcript fingerprints.\n"
              "// No replacement participates. Each group covers up to 1024 complete results.\n"
              "static constexpr std::uint64_t textureReferenceGroups[] = {");
    unsigned count=0; std::uint64_t hash=14695981039346656037ull;
    Sweep([&](const Input &i) {
        Hash(hash,Run<Legacy>(i)); ++count;
        if(count%1024==0) { std::printf("0x%016llxull,\n",hash); hash=14695981039346656037ull; }
    });
    if(count%1024)std::printf("0x%016llxull,\n",hash);
    std::printf("};\nstatic constexpr unsigned textureReferenceCount=%u;\n",count);
}
