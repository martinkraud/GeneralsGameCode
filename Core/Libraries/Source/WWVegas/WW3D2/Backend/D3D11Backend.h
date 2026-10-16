// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "WW3D2/PresentationLifecycle.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/Texture2D.h"
#include "WW3D2/WorldSubmission.h"
#include <memory>
#include <string>
#include <vector>

class Texture2DCPUContent;

// Backend-local API. SDK declarations and COM ownership stay in Impl.
class D3D11Backend final : public IRenderBackend
{
public:
    using DiagnosticSink = void (*)(void *, const char *, const char *, const char *, unsigned);
    using Presentation = PresentationStatus;
    struct Configuration {
        void *window = nullptr; // null selects a bounded offscreen target
        unsigned width = 128, height = 96;
        bool software = false; // explicit WARP selection for automated validation
        bool diagnostic_windowed = false; // opt-in: suppress DXGI Alt-Enter
        bool depth_buffer = false; // opt-in D24S8 depth/stencil for world submissions (R0)
        bool vsync = false; // X64.33: Present(1) instead of Present(0)
        DiagnosticSink diagnostic = nullptr;
        void *diagnostic_context = nullptr;
        TextureFilterClass::TextureFilterMode ui_filter = TextureFilterClass::TEXTURE_FILTER_BILINEAR;
        unsigned ui_anisotropy = 2;
    };
    // The engine bridge accesses real TextureClass ownership. Keeping it in a
    // separate TU lets the real-device probe use neutral CPU content without
    // pulling unrelated legacy loaders/world renderer into its executable.
    struct TextureBridge {
        Texture2DResidency *(*create)(D3D11Backend &, TextureClass &) = nullptr;
        void (*apply)(D3D11Backend &, TextureClass &, unsigned) = nullptr;
        bool (*prepare)(D3D11Backend &, TextureClass &) = nullptr;
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
    // X64.37 v3 Render Scale: the 3D world (Submit_World) renders into an intermediate target of `percent` of the presentation size
    // and is composited to the back buffer before the first 2D submission, the screenshot readback or the present; 2D, text and the
    // back buffer itself stay at full resolution. 100 = off (the exact previous path). Takes effect on the next submission.
    void Set_World_Render_Scale(unsigned percent);
    unsigned World_Render_Scale() const;
    // The explicit end of the world phase of a picture (W3DDisplay::draw calls it right after drawViews()): whatever the world drew at the
    // reduced size is composited into the back buffer NOW, before any interface drawing. The first 2D submission, the end of the scene and
    // a readback composite as well (defence in depth); a composite with nothing pending does nothing.
    void End_World_Phase();
    void World_Scale_Counters(unsigned &scaled_world_submits, unsigned &composites) const;   // native triage
    void Set_Vsync(bool enabled); // X64.33; takes effect on the next Present
    unsigned Texture_Creations() const;
    unsigned Buffer_Creations() const;
    unsigned Draw_Count() const;
    // X64.35: cumulative measurement counters. state_requested = state calls the per-draw re-issue makes;
    // state_changed = how many of those differ from the previously issued value (the real change rate).
    struct RenderStats
    {
        unsigned long long draws_2d = 0, draws_world = 0, draws_water = 0, triangles = 0;
        unsigned long long state_requested = 0, state_changed = 0, texture_binds = 0, shader_changes = 0,
                           target_changes = 0, buffer_changes = 0;
        // X64.35 baseline2: CPU wall time in QueryPerformanceCounter ticks measured *inside* the backend. submit =
        // Render2D + world submit functions (state sets, constant-buffer updates, DrawIndexed); present =
        // IDXGISwapChain::Present; upload = texture creation + dynamic texture updates. GPU = elapsed GPU time of
        // frames whose timestamp queries were retrieved without blocking (nanoseconds, summed), gpu_frames of them.
        unsigned long long submit_ticks = 0, present_ticks = 0, upload_ticks = 0, present_calls = 0;
        unsigned long long gpu_ns = 0, gpu_frames = 0, gpu_disjoint = 0, gpu_skipped = 0;
        unsigned long long textures_destroyed = 0, texture_updates = 0, texture_update_bytes = 0, texture_bytes_created = 0;
    };
    RenderStats Render_Stats() const;
    unsigned Live_Textures() const;
    // Opt-in GPU timestamp measurement (disjoint + begin/end per frame, ring of 4, polled without flushing or
    // blocking). Off by default; the display host enables it only while a performance capture runs.
    void Set_Gpu_Timing(bool enabled);
    // Attribution of texture creations. entry: 0 file/content (immutable), 1 dynamic UI, 2 mipped atlas/LUT.
    struct TextureCreationRow
    {
        std::string label; unsigned entry = 0, width = 0, height = 0, format = 0;
        unsigned long long created = 0, bytes = 0;
    };
    std::vector<TextureCreationRow> Texture_Creation_Table() const;
    // Thread-local label recorded with subsequent texture creations (empty = untagged). Measurement only.
    static void Set_Creation_Label(const char *label);
    const char *Adapter_Description() const; // adapter, ids, feature level, VRAM (X64.35 receipt context)
    // Opt-in (default off) for the world renderer: legacy DX8 vertex/index buffers become
    // CPU-backed while this backend owns WW3D. Profiles that do not render the world
    // (X64.22-24) keep the exact previous runtime behavior.
    static void Enable_CPU_Backed_Geometry(bool enable);
    static bool CPU_Backed_Geometry_Enabled();
    // Opt-in (default off): the WW3D-owned backend is created with the D24S8 depth
    // buffer (the lifecycle Begin stays color-only; world frames clear depth through
    // the backend after Begin). Must be set before WW3D::Init_Native_Presentation.
    static void Enable_Depth_Buffer_For_W3D(bool enable);
    static bool Depth_Buffer_For_W3D_Enabled();
    // World (R1/R2): neutral geometry residency and purpose-built material draws.
    GeometryResidency *Create_Geometry_Residency(const GeometryCPUContent &);
    // Immutable RGBA8 texture with a full caller-supplied mip chain (terrain atlas).
    // mips[0] is width*height*4 bytes; each level halves down to 1x1.
    Texture2DResidency *Create_Mipped_Rgba8_Residency(unsigned width, unsigned height,
                                                      const std::vector<std::vector<unsigned char>> &mips);
    // Replace vertices [first, first+count) in place (dynamic terrain).
    void Update_Geometry_Vertices(GeometryResidency &, unsigned first, const WorldVertexXYZDUV2 *vertices, unsigned count);
    // Fixed-capacity geometry whose vertices and indices are rewritten as content changes (roads,
    // bridges, extra-blend tile lists). Capacities are in vertices/indices; contents start empty.
    GeometryResidency *Create_Dynamic_Geometry(unsigned max_vertices, unsigned max_indices);
    void Update_Geometry_Indices(GeometryResidency &, unsigned first, const unsigned short *indices, unsigned count);
    void Submit_World(const WorldSubmission &);
    bool Has_Depth_Buffer() const;
    // Process-wide count of live logical GPU residencies (textures and geometry, all backends).
    static unsigned Live_Residencies();
    unsigned Feature_Level() const;
    Presentation Last_Presentation() const;
    void Set_UI_Filter_Policy(TextureFilterClass::TextureFilterMode, unsigned anisotropy);
    // Reports a file texture that was replaced by the missing-texture placeholder (first 16, then silent).
    void Note_Missing_Texture(const std::string &name, unsigned status);
    unsigned Missing_Texture_Count() const;

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
    bool Prepare_File_Texture(TextureClass &) override;
    bool Supports_Dynamic_Render2D_Content() const override { return true; }
    // Mutable counterpart of Create_Content_Residency: DEFAULT usage so a CPU revision change can be
    // re-uploaded through Update_Content_Residency (same conversion, same texel layout).
    Texture2DResidency *Create_Dynamic_Content_Residency(const Texture2DCPUContent &);
    void Update_Content_Residency(Texture2DResidency &, const Texture2DCPUContent &);
    unsigned Dynamic_Uploads() const;
    void Copy_Render2D_Surface(SurfaceClass &, SurfaceClass &) override;
private:
    struct Impl;
    explicit D3D11Backend(const Configuration &, TextureBridge);
    std::unique_ptr<Impl> impl;
    void Draw(const Render2DSubmission &);
};
