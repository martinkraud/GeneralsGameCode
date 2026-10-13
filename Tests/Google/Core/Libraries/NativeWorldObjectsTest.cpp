// SPDX-License-Identifier: GPL-3.0-or-later
// Native world-object renderer: real W3D models loaded by the legacy asset manager (no DX8 object)
// drawn through the D3D11 WARP backend. Opt-in: needs an extracted asset directory
// (GENERALS_OFFLINE_ASSETS) and optionally an output directory for evidence images
// (GENERALS_OFFLINE_OUT); skipped otherwise so the suite stays hermetic.
#include <gtest/gtest.h>
#if defined(_WIN64)
#include "W3DDevice/GameClient/NativeWorldObjects.h"
#include "WW3D2/Backend/D3D11Backend.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/camera.h"
#include "WW3D2/coltest.h"
#include "WWMath/lineseg.h"
#include "WW3D2/hlod.h"
#include "WW3D2/rendobj.h"
#include "WWLib/ffactory.h"
#include "WWMath/matrix3d.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {
bool WriteBmp(const std::string &path, const std::vector<unsigned char> &rgba, unsigned w, unsigned h)
{
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    unsigned char header[54] = {'B', 'M'};
    const unsigned dataSize = w * h * 4, fileSize = 54 + dataSize, offset = 54, dib = 40, planes = 1, bits = 32;
    const int topDown = -static_cast<int>(h);
    std::memcpy(header + 2, &fileSize, 4); std::memcpy(header + 10, &offset, 4); std::memcpy(header + 14, &dib, 4);
    std::memcpy(header + 18, &w, 4); std::memcpy(header + 22, &topDown, 4);
    std::memcpy(header + 26, &planes, 2); std::memcpy(header + 28, &bits, 2); std::memcpy(header + 34, &dataSize, 4);
    std::fwrite(header, 1, 54, f);
    std::vector<unsigned char> bgra(rgba.size());
    for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
        bgra[i] = rgba[i + 2]; bgra[i + 1] = rgba[i + 1]; bgra[i + 2] = rgba[i]; bgra[i + 3] = 255;
    }
    std::fwrite(bgra.data(), 1, bgra.size(), f);
    std::fclose(f);
    return true;
}

unsigned CountNonBackground(const std::vector<unsigned char> &px, const unsigned char (&bg)[3])
{
    unsigned n = 0;
    for (size_t i = 0; i + 3 < px.size(); i += 4)
        if (px[i] != bg[0] || px[i + 1] != bg[1] || px[i + 2] != bg[2]) ++n;
    return n;
}
}

TEST(NativeWorldObjectsTest, RealW3DModelsDrawThroughTheNativeMeshPath)
{
    const char *assets = std::getenv("GENERALS_OFFLINE_ASSETS");
    if (!assets || !*assets) GTEST_SKIP() << "GENERALS_OFFLINE_ASSETS not set (opt-in asset test)";
    const char *outDir = std::getenv("GENERALS_OFFLINE_OUT");
    std::string dir = assets;
    if (dir.back() != '\\' && dir.back() != '/') dir += '\\';

    // The directory is the file factory the legacy loader and the native texture reader both use.
    SimpleFileFactoryClass factory;
    factory.Set_Sub_Directory(dir.c_str());
    FileFactoryClass *previous = _TheFileFactory;
    _TheFileFactory = &factory;
    NativeWorldObjects::setFileReaderForTests([dir](const std::string &path, std::vector<unsigned char> &bytes) {
        std::string leaf = path;
        const size_t slash = leaf.find_last_of("\\/");
        if (slash != std::string::npos) leaf = leaf.substr(slash + 1);
        std::ifstream file(dir + leaf, std::ios::binary);
        if (!file) return false;
        bytes.assign(std::istreambuf_iterator<char>(file), {});
        return true;
    });
    constexpr unsigned kSize = 512;
    NativeWorldObjects::setRenderTargetSizeForTests(kSize, kSize);

    struct Model { const char *name; int color; float x; };
    const Model models[] = {
        {"AVPALADIN", 0xff2060ff, -60.0f},
        {"AVHUMMER", 0xffff2020, -20.0f},
        {"ABBARRACKS", 0xff20ff40, 40.0f},
        {"UIRGRD_SKN", 0xffffd020, 80.0f},
    };

    {
        WW3DAssetManager manager;
        manager.Set_WW3D_Load_On_Demand(true);
        std::vector<RenderObjClass *> objects;
        std::vector<NativeWorldObjects::ObjectInput> inputs;
        for (const Model &model : models) {
            RenderObjClass *object = manager.Create_Render_Obj(model.name);
            if (!object) { ADD_FAILURE() << "model not loadable: " << model.name; continue; }
            Matrix3D tm(1);
            tm.Set_Translation(Vector3(model.x, 0.0f, 0.0f));
            object->Set_Transform(tm);
            objects.push_back(object);
            NativeWorldObjects::ObjectInput input = {};
            input.object = object;
            input.hexColor = model.color;
            input.opacity = 1.0f;
            input.infantry = std::string(model.name).find("UIRGRD") == 0;
            inputs.push_back(input);
        }
        ASSERT_FALSE(inputs.empty());

        D3D11Backend::Configuration c;
        c.software = true; c.width = c.height = kSize; c.depth_buffer = true;
        std::unique_ptr<D3D11Backend> backend = D3D11Backend::Create(c);
        ASSERT_TRUE(backend);

        CameraClass camera;
        camera.Set_Aspect_Ratio(1.0f);
        camera.Set_View_Plane(DEG_TO_RADF(45.0f), -1);
        camera.Set_Clip_Planes(1.0f, 2000.0f);
        camera.Set_Viewport(Vector2(0, 0), Vector2(1, 1));
        Matrix3D cam(1);
        cam.Look_At(Vector3(10.0f, -170.0f, 95.0f), Vector3(10.0f, 0.0f, 5.0f), 0.0f);
        camera.Set_Transform(cam);

        NativeWorldObjects::SceneLighting lights = {};
        lights.ambient[0] = lights.ambient[1] = lights.ambient[2] = 0.35f;
        lights.count = 1;
        lights.direction[0][0] = 0.35f; lights.direction[0][1] = -0.45f; lights.direction[0][2] = 0.82f;
        lights.diffuse[0][0] = 0.85f; lights.diffuse[0][1] = 0.82f; lights.diffuse[0][2] = 0.75f;
        NativeWorldObjects::SceneLighting infantry = lights;

        backend->Clear(true, true, Vector3(0.2f, 0.25f, 0.3f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        const int draws = NativeWorldObjects::renderObjects(*backend, camera, lights, infantry, inputs.data(),
                                                            static_cast<unsigned>(inputs.size()));
        backend->End_Scene(false);
        const std::vector<unsigned char> pixels = backend->Readback_RGBA8();

        const NativeWorldObjects::Stats stats = NativeWorldObjects::stats();
        std::printf("draws=%d objects=%u drawn=%u meshes=%u meshes_drawn=%u skinned=%u batches=%u triangles=%u "
                    "textures=%u texture_failures=%u team_colored=%u unsupported_class=%u unsupported_state=%u\n",
                    draws, stats.objects, stats.objectsDrawn, stats.meshes, stats.meshesDrawn, stats.skinned,
                    stats.batches, stats.triangles, stats.textureLoads, stats.textureFailures, stats.teamColored,
                    stats.unsupportedClass, stats.unsupportedState);
        for (unsigned i = 0; i < NativeWorldObjects::failedTextureCount() && i < 12; ++i)
            std::printf("texture failure: %s\n", NativeWorldObjects::failedTexture(i));

        EXPECT_GT(draws, 0);
        EXPECT_EQ(stats.objectsDrawn, inputs.size());
        EXPECT_GT(stats.triangles, 500u);
        EXPECT_EQ(stats.textureFailures, 0u);
        EXPECT_EQ(stats.chunkOverflow, 0u);
        const unsigned char background[3] = {51, 64, 77};
        EXPECT_GT(CountNonBackground(pixels, background), kSize * kSize / 40);
        if (outDir && *outDir) WriteBmp(std::string(outDir) + "\\native_objects.bmp", pixels, kSize, kSize);

        // Picking: the registry ray cast resolves the object under the cursor ray (the click path).
        NativeWorldObjects::setActiveForTests(true);
        for (RenderObjClass *object : objects) {
            object->Set_Collision_Type(1);
            NativeWorldObjects::add(object);
        }
        EXPECT_EQ(NativeWorldObjects::registered(), objects.size());
        const Vector3 origin(10.0f, -170.0f, 95.0f);
        for (RenderObjClass *object : objects) {
            const Vector3 center = object->Get_Bounding_Sphere().Center;
            LineSegClass line(origin, origin + (center - origin) * 2.0f);
            CastResultStruct castResult;
            RayCollisionTestClass test(line, &castResult, COLL_TYPE_ALL, false, false);
            const bool hit = NativeWorldObjects::castRay(test, false, 1);
            EXPECT_TRUE(hit) << object->Get_Name();
            if (hit) {
                // The nearest registered object along the ray is the target or something in front of it.
                EXPECT_NE(test.CollidedRenderObj, nullptr);
                EXPECT_LT(test.Ray.Get_Length(), (center - origin).Length() * 2.0f);   // ray clipped on the hit
            }
        }
        {
            LineSegClass sky(origin, origin + Vector3(0.0f, 100.0f, 400.0f));
            CastResultStruct castResult;
            RayCollisionTestClass test(sky, &castResult, COLL_TYPE_ALL, false, false);
            EXPECT_FALSE(NativeWorldObjects::castRay(test, false, 1));
        }
        for (RenderObjClass *object : objects) NativeWorldObjects::remove(object);
        EXPECT_EQ(NativeWorldObjects::registered(), 0u);
        NativeWorldObjects::setActiveForTests(false);

        NativeWorldObjects::releaseAll();
        for (RenderObjClass *object : objects) object->Release_Ref();
        manager.Free_Assets();
    }
    NativeWorldObjects::setFileReaderForTests({});
    NativeWorldObjects::setRenderTargetSizeForTests(0, 0);
    _TheFileFactory = previous;
}
#endif
