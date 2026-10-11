// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/Texture2D.h"
#include <memory>
#include <vector>

class Texture2DCPUContent;

// Backend-local API. SDK declarations and COM ownership stay in Impl.
class D3D11Backend final : public IRenderBackend
{
public:
    struct Configuration {
        void *window = nullptr; // null selects a bounded offscreen target
        unsigned width = 128, height = 96;
        bool software = false; // explicit WARP selection for automated validation
    };
    // The engine bridge accesses real TextureClass ownership. Keeping it in a
    // separate TU lets the real-device probe use neutral CPU content without
    // pulling unrelated legacy loaders/world renderer into its executable.
    struct TextureBridge {
        Texture2DResidency *(*create)(D3D11Backend &, TextureClass &) = nullptr;
        void (*apply)(D3D11Backend &, TextureClass &, unsigned) = nullptr;
    };
    static std::unique_ptr<D3D11Backend> Create(const Configuration &, TextureBridge);
    static std::unique_ptr<D3D11Backend> Create(const Configuration &config)
    { return Create(config, TextureBridge{}); }
    static std::unique_ptr<D3D11Backend> Create_For_W3D(const Configuration &);
    ~D3D11Backend() override;
    D3D11Backend(const D3D11Backend &) = delete;
    D3D11Backend &operator=(const D3D11Backend &) = delete;

    Texture2DResidency *Create_Content_Residency(const Texture2DCPUContent &);
    void Bind_Content(Texture2DResidency *, const Texture2DSampler &);
    void Submit_Render2D_Content(const Render2DSubmission &, Texture2DResidency *, const Texture2DSampler &);
    static std::vector<unsigned char> Convert_Sentence_RGBA8(const Texture2DCPUContent &);
    // Bounded validation readback, not an engine screenshot/readback migration.
    std::vector<unsigned char> Readback_RGBA8();
    void Resize(unsigned width, unsigned height);
    unsigned Texture_Creations() const;
    unsigned Buffer_Creations() const;
    unsigned Draw_Count() const;
    unsigned Feature_Level() const;

    void Set_Gamma(float, float, float, bool, bool) override;
    void Begin_Scene() override;
    void End_Scene(bool flip_frame = true) override;
    void Flip_To_Primary() override;
    void Clear(bool, bool, const Vector3 &, float = 0, float = 1, unsigned = 0) override;
    void Set_Viewport(const RenderBackendViewport &) override;
    void Invalidate_Cached_Render_States() override;
    void Set_Ambient(const Vector3 &) override;
    void Set_Light_Environment(LightEnvironmentClass *) override;
    void Submit_Render2D(const Render2DSubmission &) override;
    Texture2DResidency *Create_Render2D_Residency(TextureClass &) override;
    void Apply_Render2D_Texture(TextureClass &, unsigned) override;
    void Copy_Render2D_Surface(SurfaceClass &, SurfaceClass &) override;
private:
    struct Impl;
    explicit D3D11Backend(const Configuration &, TextureBridge);
    std::unique_ptr<Impl> impl;
    void Draw(const Render2DSubmission &);
};
