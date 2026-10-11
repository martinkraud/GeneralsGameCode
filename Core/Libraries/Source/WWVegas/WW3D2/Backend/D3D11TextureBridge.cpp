// SPDX-License-Identifier: GPL-3.0-or-later
#include "D3D11Backend.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/shader.h"
#include <stdexcept>

std::unique_ptr<D3D11Backend> D3D11Backend::Create_For_W3D(const Configuration &config)
{
    TextureBridge bridge;
    bridge.create = [](D3D11Backend &backend, TextureClass &texture) -> Texture2DResidency * {
        const auto *content = texture.Get_Render2D_CPU_Content();
        if (!content) throw std::runtime_error("D3D11.1 requires sentence CPU content; legacy asset loading is unsupported");
        return backend.Create_Content_Residency(*content);
    };
    bridge.apply = [](D3D11Backend &backend, TextureClass &texture, unsigned stage) {
        if (stage) throw std::runtime_error("D3D11.1 supports only texture stage zero");
        if (WW3D::Get_Render_Backend() != &backend || !texture.Get_Render2D_CPU_Content() ||
            !WW3D::Is_Texturing_Enabled() || ShaderClass::Is_Backface_Culling_Inverted())
            throw std::runtime_error("D3D11.1 requires active backend, sentence content and normal texturing/culling");
        // Requires this backend to be the WW3D active backend. The default game
        // factory remains DX8; this bridge is not a mixed-backend routing mode.
        texture.Prepare_Render2D_Texture();
        backend.Bind_Content(&texture.Get_Render2D_Residency(),
                             Describe_Texture2D_Sampler(texture.Get_Filter()));
    };
    return Create(config, bridge);
}
