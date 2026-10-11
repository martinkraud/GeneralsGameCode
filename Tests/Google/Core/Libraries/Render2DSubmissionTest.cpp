// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <string>
#include <vector>
#include "WW3D2/Render2DSubmission.h"
#include "WW3D2/shader.h"
#include "WW3D2/vertexbuffer.h"
#include "WW3D2/indexbuffer.h"
#include "WWMath/vector2.h"

namespace
{
// This recorder implements the same compile-time command/access contract as
// DX8Render2DCommands, without a device, FVF declarations or graphics SDK.
struct Recorder
{
    std::vector<std::string> events;
    std::array<unsigned char, 3 * 64> vertices;
    std::array<unsigned short, 6> indices{};
    RenderBackendViewport viewport{};
    TextureClass *texture = nullptr;
    unsigned shader_bits = 0;
    std::array<unsigned short, 4> draw{};
    unsigned vertex_count = 0, index_count = 0, index_base = 99;
    bool dot3 = false;
    bool vertex_locked = false, index_locked = false;
    int view = 37, projection = 51, world = 17;

    Recorder() { vertices.fill(0xcd); }
    void Event(const char *event) { events.emplace_back(event); }
    struct SavedTransforms { int view, projection; };
    struct VertexAccess
    {
        Recorder &r;
        VertexAccess(Recorder &recorder, unsigned short count) : r(recorder)
        { r.Event("allocate vertices"); r.vertex_count = count; }
        ~VertexAccess() { r.Event("release vertices"); }
        struct WriteLock
        {
            Recorder &r;
            explicit WriteLock(VertexAccess &v) : r(v.r)
            { r.Event("lock vertices"); r.vertex_locked = true; }
            ~WriteLock() { r.Event("unlock vertices"); r.vertex_locked = false; }
            // Deliberately not the legacy FVF layout: exercises semantic offsets.
            Render2DVertexMapping Map() { return {r.vertices.data(), 64, 4, 32, 44}; }
        };
    };
    struct IndexAccess
    {
        Recorder &r;
        IndexAccess(Recorder &recorder, unsigned short count) : r(recorder)
        { r.Event("allocate indices"); r.index_count = count; EXPECT_FALSE(r.vertex_locked); }
        ~IndexAccess() { r.Event("release indices"); }
        struct WriteLock
        {
            Recorder &r;
            explicit WriteLock(IndexAccess &i) : r(i.r)
            { r.Event("lock indices"); r.index_locked = true; }
            ~WriteLock() { r.Event("unlock indices"); r.index_locked = false; }
            unsigned short *Map() { return r.indices.data(); }
        };
    };
    void Save_Transforms(SavedTransforms &s)
    { Event("save transforms"); s = {view, projection}; }
    void Set_Viewport(const RenderBackendViewport &v) { Event("viewport"); viewport = v; }
    void Set_Texture(TextureClass *t) { Event("texture 0"); texture = t; }
    void Set_Prelit_Diffuse_Material() { Event("prelit diffuse"); }
    void Set_Identity_Transforms() { Event("identity transforms"); view = projection = world = 0; }
    void Bind_Vertices(const VertexAccess &v)
    { Event("bind vertices"); EXPECT_EQ(&v.r, this); EXPECT_FALSE(vertex_locked); EXPECT_FALSE(index_locked); }
    void Bind_Indices(const IndexAccess &i, unsigned short base)
    { Event("bind indices"); EXPECT_EQ(&i.r, this); index_base = base; }
    void Set_Shader(const ShaderClass &shader) { Event("shader"); shader_bits = shader.Get_Bits(); }
    void Set_Opaque_Shader() { Event("opaque shader"); }
    void Apply_State_Changes() { Event("flush W3D states"); }
    bool Supports_Dot3() { Event("query dot3"); return dot3; }
    void Set_Grayscale_Combiner(bool supported) { Event(supported ? "dot3 grayscale" : "modulate grayscale"); }
    void Draw_Triangles(unsigned short start, unsigned short count, unsigned short minimum, unsigned short vertices)
    { Event("triangle list"); draw = {start, count, minimum, vertices}; EXPECT_FALSE(vertex_locked); EXPECT_FALSE(index_locked); }
    void Restore_Transforms(const SavedTransforms &s)
    { Event("restore transforms"); view = s.view; projection = s.projection; }
    void Invalidate_Shader() { Event("invalidate shader"); }
};

struct Render2DSubmissionTest : testing::Test
{
    Vector2 positions[3] = {Vector2(-1, 1), Vector2(1, 1), Vector2(0, -1)};
    Vector2 uvs[3] = {Vector2(0, 0), Vector2(1, 0), Vector2(0.5f, 1)};
    unsigned long colors[3] = {0x12345678, 0x80abcdef, 0xff010203};
    unsigned short indices[6] = {2, 0, 1, 1, 0, 2};
    ShaderClass shader{0};
    // Identity only, never dereferenced by the recorder.
    char texture_identity;
    Render2DSubmission submission{reinterpret_cast<TextureClass *>(&texture_identity), &shader,
        positions, uvs, colors, indices, 3, 6, 0.25f, {11, 17, 640, 480, 0, 1}, false};
    Recorder recorder;

    const std::vector<std::string> prefix = {
        "save transforms", "viewport", "texture 0", "prelit diffuse", "identity transforms",
        "allocate vertices", "lock vertices", "unlock vertices",
        "allocate indices", "lock indices", "unlock indices", "bind vertices", "bind indices"};
    void Expect_Sequence(std::vector<std::string> middle, bool grayscale)
    {
        auto expected = prefix;
        expected.insert(expected.end(), middle.begin(), middle.end());
        expected.push_back("triangle list"); expected.push_back("restore transforms");
        if (grayscale) expected.push_back("invalidate shader");
        expected.push_back("release indices"); expected.push_back("release vertices");
        EXPECT_EQ(recorder.events, expected);
    }
};

TEST_F(Render2DSubmissionTest, PreservesBindingStateDrawAndLifetimeOrder)
{
    shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
    shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
    shader.Set_Src_Blend_Func(ShaderClass::SRCBLEND_SRC_ALPHA);
    shader.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA);
    Execute_Render2D(recorder, submission);
    Expect_Sequence({"shader"}, false);
    EXPECT_EQ(recorder.texture, submission.texture);
    EXPECT_EQ(recorder.shader_bits, shader.Get_Bits());
    EXPECT_EQ(recorder.viewport.x, 11u); EXPECT_EQ(recorder.viewport.y, 17u);
    EXPECT_EQ(recorder.viewport.width, 640u); EXPECT_EQ(recorder.viewport.height, 480u);
    EXPECT_EQ(recorder.viewport.min_z, 0.0f); EXPECT_EQ(recorder.viewport.max_z, 1.0f);
    EXPECT_EQ(recorder.vertex_count, 3u); EXPECT_EQ(recorder.index_count, 6u);
    EXPECT_EQ(recorder.index_base, 0u);
    EXPECT_EQ(recorder.draw, (std::array<unsigned short, 4>{0, 2, 0, 3}));
    EXPECT_EQ(recorder.view, 37); EXPECT_EQ(recorder.projection, 51);
    EXPECT_EQ(recorder.world, 0); // Legacy deliberately leaves world at identity.
}

TEST_F(Render2DSubmissionTest, WritesOnlyXYZARGBAndUV0AtBackendOffsets)
{
    Execute_Render2D(recorder, submission);
    auto expected = recorder.vertices;
    expected.fill(0xcd);
    for (int i = 0; i < 3; ++i)
    {
        const float xyz[3] = {positions[i].X, positions[i].Y, 0.25f};
        const unsigned int color = colors[i];
        const float uv[2] = {uvs[i].X, uvs[i].Y};
        std::memcpy(expected.data() + i * 64 + 4, xyz, 12);
        std::memcpy(expected.data() + i * 64 + 32, &color, 4);
        std::memcpy(expected.data() + i * 64 + 44, uv, 8);
    }
    EXPECT_EQ(recorder.vertices, expected); // Includes untouched padding/normal/UV1.
    EXPECT_EQ(recorder.indices, (std::array<unsigned short, 6>{2, 0, 1, 1, 0, 2}));
}

TEST_F(Render2DSubmissionTest, GrayscaleDot3FlushesBeforeOverrideAndInvalidatesAfterRestore)
{
    submission.grayscale = true; recorder.dot3 = true;
    Execute_Render2D(recorder, submission);
    Expect_Sequence({"opaque shader", "flush W3D states", "query dot3", "dot3 grayscale"}, true);
}

TEST_F(Render2DSubmissionTest, GrayscaleFallbackUsesModulation)
{
    submission.grayscale = true;
    Execute_Render2D(recorder, submission);
    Expect_Sequence({"opaque shader", "flush W3D states", "query dot3", "modulate grayscale"}, true);
}

TEST_F(Render2DSubmissionTest, NullTextureIsAnExplicitBinding)
{
    submission.texture = nullptr;
    Execute_Render2D(recorder, submission);
    EXPECT_EQ(recorder.texture, nullptr);
    Expect_Sequence({"shader"}, false);
}

TEST_F(Render2DSubmissionTest, EmptySubmissionDoesNotTouchBackendOrResources)
{
    submission.index_count = 0;
    Execute_Render2D(recorder, submission);
    EXPECT_TRUE(recorder.events.empty());
}

TEST_F(Render2DSubmissionTest, DrawCountRetainsLegacyTriangleDivision)
{
    submission.index_count = 5;
    Execute_Render2D(recorder, submission);
    EXPECT_EQ(recorder.index_count, 5u);
    EXPECT_EQ(recorder.draw[1], 1u);
}

TEST_F(Render2DSubmissionTest, ArbitraryW3DShaderBitsAreNotReplacedWithDefaultState)
{
    shader = ShaderClass(0x01234567);
    Execute_Render2D(recorder, submission);
    EXPECT_EQ(recorder.shader_bits, 0x01234567u);
}
}
