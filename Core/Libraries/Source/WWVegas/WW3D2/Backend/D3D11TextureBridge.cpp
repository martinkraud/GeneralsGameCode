// SPDX-License-Identifier: GPL-3.0-or-later
#include "D3D11Backend.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/shader.h"
#include "WW3D2/UITextureContent.h"
#include "WWLib/ffactory.h"
#include "WWLib/WWFILE.h"
#include <stdexcept>

namespace
{
// Same pixels as the legacy MissingTexture (translucent magenta), at the minimum size.
Texture2DCPUContent *MissingTextureContent()
{
    auto *content = new Texture2DCPUContent(4, 4, WW3D_FORMAT_A8R8G8B8, 0);
    int pitch = 0;
    unsigned char *rows = static_cast<unsigned char *>(content->Lock(&pitch));
    for (unsigned y = 0; y < 4; ++y) {
        unsigned *row = reinterpret_cast<unsigned *>(rows + static_cast<size_t>(y) * pitch);
        for (unsigned x = 0; x < 4; ++x) row[x] = 0x7FFF00FF;
    }
    content->Unlock();
    return content;
}
}

std::unique_ptr<D3D11Backend> D3D11Backend::Create_For_W3D(const Configuration &config)
{
    TextureBridge bridge;
    bridge.prepare = [](D3D11Backend &backend, TextureClass &texture) {
        // Any requested mip count is accepted: the native residency is single-level (top mip), the same content
        // the legacy chain was generated from. Gameplay code asks for full chains (laser/particle/decal textures).
        if (WW3D::Get_Render_Backend() != &backend ||
            texture.Get_Asset_Type() != TextureBaseClass::TEX_REGULAR || texture.Is_Procedural() ||
            texture.Peek_D3D_Base_Texture())
            throw std::runtime_error("D3D11.2 requires an ordinary file texture without native storage");
        if (texture.Get_Texture_Format() != WW3D_FORMAT_UNKNOWN && !texture.Get_Render2D_CPU_Content())
            throw std::runtime_error("D3D11.2 explicit legacy format conversion is unsupported");
        UITexturePolicy policy;
        policy.allow_compression = texture.Is_Compression_Allowed();
        policy.texture_bits = WW3D::Get_Texture_Bitdepth();
        const auto &hsv = texture.Get_HSV_Shift();
        policy.hsv_shift = hsv.X != 0 || hsv.Y != 0 || hsv.Z != 0;
        auto result = Load_UI_Texture_Content(texture.Get_Full_Path().str(), policy, [](const std::string &name) {
            UITextureBytes result;
            if (!_TheFileFactory) { result.status = UITextureStatus::Invalid; return result; }
            file_auto_ptr file(_TheFileFactory, name.c_str());
            if (!file || !file->Is_Available()) return result;
            result.status = UITextureStatus::Invalid;
            if (!file->Open()) return result;
            const int size = file->Size();
            if (size <= 0 || size > 256 * 1024 * 1024) return result;
            result.bytes.resize(size);
            if (file->Read(result.bytes.data(), size) == size) result.status = UITextureStatus::Ready;
            return result;
        });
        if (result.status != UITextureStatus::Ready) {
            // Legacy behavior (MissingTexture): an absent/unsupported file never aborts the frame; it draws the
            // 0x7FFF00FF placeholder. The first occurrences are reported through the backend diagnostic sink.
            backend.Note_Missing_Texture(texture.Get_Full_Path().str(), static_cast<unsigned>(result.status));
            result.content.reset(MissingTextureContent());
        }
        texture.Install_Render2D_File_Content(*result.content);
        return true;
    };
    bridge.create = [](D3D11Backend &backend, TextureClass &texture) -> Texture2DResidency * {
        const auto *content = texture.Get_Render2D_CPU_Content();
        if (!content) throw std::runtime_error("D3D11 requires supported CPU content");
        // X64.35 baseline2: measurement label only (which engine texture caused this creation).
        {
            const StringClass &diagnostic = texture.Get_Diagnostic_Label();
            D3D11Backend::Set_Creation_Label(!diagnostic.Is_Empty() ? (std::string("text:") + diagnostic.str()).c_str()
                                                                    : texture.Get_Full_Path().str());
        }
        struct ClearLabel { ~ClearLabel() { D3D11Backend::Set_Creation_Label(nullptr); } } clearLabel;
        if (texture.Is_Render2D_Dynamic()) {
            texture.Render2D_Uploaded_Revision() = content->Revision();
            return backend.Create_Dynamic_Content_Residency(*content);
        }
        return backend.Create_Content_Residency(*content);
    };
    bridge.apply = [](D3D11Backend &backend, TextureClass &texture, unsigned stage) {
        if (stage) throw std::runtime_error("D3D11.1 supports only texture stage zero");
        if (WW3D::Get_Render_Backend() != &backend ||
            !WW3D::Is_Texturing_Enabled() || ShaderClass::Is_Backface_Culling_Inverted())
            throw std::runtime_error("D3D11 requires active backend and normal texturing/culling");
        // Requires this backend to be the WW3D active backend. The default game
        // factory remains DX8; this bridge is not a mixed-backend routing mode.
        texture.Prepare_Render2D_Texture();
        if (!texture.Get_Render2D_CPU_Content()) throw std::runtime_error("D3D11 requires CPU content");
        backend.Set_UI_Filter_Policy(static_cast<TextureFilterClass::TextureFilterMode>(WW3D::Get_Texture_Filter()),
                                     WW3D::Get_Anisotropy_Level());
        Texture2DResidency &residency = texture.Get_Render2D_Residency();
        if (texture.Is_Render2D_Dynamic()) {
            // Dynamic UI content: re-upload only when a surface lock was released since the last upload.
            const auto *content = texture.Get_Render2D_CPU_Content();
            if (content->Revision() != texture.Render2D_Uploaded_Revision()) {
                backend.Update_Content_Residency(residency, *content);
                texture.Render2D_Uploaded_Revision() = content->Revision();
            }
        }
        backend.Bind_Content(&residency, Describe_Texture2D_Sampler(texture.Get_Filter()));
    };
    Configuration effective = config;
    effective.depth_buffer = config.depth_buffer || Depth_Buffer_For_W3D_Enabled();
    return Create(effective, bridge);
}
