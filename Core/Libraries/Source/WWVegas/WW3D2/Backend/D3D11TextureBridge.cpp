// SPDX-License-Identifier: GPL-3.0-or-later
#include "D3D11Backend.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/shader.h"
#include "WW3D2/UITextureContent.h"
#include "WWLib/ffactory.h"
#include "WWLib/WWFILE.h"
#include <stdexcept>

std::unique_ptr<D3D11Backend> D3D11Backend::Create_For_W3D(const Configuration &config)
{
    TextureBridge bridge;
    bridge.prepare = [](D3D11Backend &backend, TextureClass &texture) {
        if (WW3D::Get_Render_Backend() != &backend ||
            texture.Get_Asset_Type() != TextureBaseClass::TEX_REGULAR || texture.Is_Procedural() ||
            texture.MipLevelCount != MIP_LEVELS_1 || texture.Peek_D3D_Base_Texture())
            throw std::runtime_error("D3D11.2 requires an ordinary single-mip file texture without native storage");
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
        if (result.status != UITextureStatus::Ready)
            throw std::runtime_error("D3D11.2 file content status=" + std::to_string(static_cast<unsigned>(result.status)) +
                " (absent/invalid/unsupported; legacy MissingTexture GPU fallback is not migrated)");
        texture.Install_Render2D_File_Content(*result.content);
        return true;
    };
    bridge.create = [](D3D11Backend &backend, TextureClass &texture) -> Texture2DResidency * {
        const auto *content = texture.Get_Render2D_CPU_Content();
        if (!content) throw std::runtime_error("D3D11 requires supported CPU content");
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
        backend.Bind_Content(&texture.Get_Render2D_Residency(),
                             Describe_Texture2D_Sampler(texture.Get_Filter()));
    };
    Configuration effective = config;
    effective.depth_buffer = config.depth_buffer || Depth_Buffer_For_W3D_Enabled();
    return Create(effective, bridge);
}
