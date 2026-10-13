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
#include "WW3D2/hanim.h"
#include "WW3D2/hlod.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/rendobj.h"
#include "WWLib/ffactory.h"
#include "WWLib/ramfile.h"
#include "WWMath/matrix3d.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <unordered_map>
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
    NativeWorldObjects::resetStatsForTests();
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

TEST(NativeWorldObjectsTest, AnimatedSkinAdvancesWithLogicTimeWithoutTheLegacyRenderer)
{
    NativeWorldObjects::resetStatsForTests();
    const char *assets = std::getenv("GENERALS_OFFLINE_ASSETS");
    if (!assets || !*assets) GTEST_SKIP() << "GENERALS_OFFLINE_ASSETS not set (opt-in asset test)";
    std::string dir = assets;
    if (dir.back() != '\\' && dir.back() != '/') dir += '\\';
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
    constexpr unsigned kSize = 256;
    NativeWorldObjects::setRenderTargetSizeForTests(kSize, kSize);
    {
        WW3DAssetManager manager;
        manager.Set_WW3D_Load_On_Demand(true);
        RenderObjClass *ranger = manager.Create_Render_Obj("UIRGRD_SKN");
        ASSERT_NE(ranger, nullptr);
        manager.Load_3D_Assets("UIRGRD_IDA.W3D");
        std::string animName;
        {
            std::unique_ptr<AssetIterator> it(manager.Create_HAnim_Iterator());
            for (it->First(); !it->Is_Done(); it->Next()) {
                const std::string name = it->Current_Item_Name();
                if (name.find("UIRGRD") != std::string::npos && animName.empty()) animName = name;
            }
        }
        ASSERT_FALSE(animName.empty()) << "no ranger animation found";
        HAnimClass *anim = manager.Get_HAnim(animName.c_str());
        ASSERT_NE(anim, nullptr);
        ranger->Set_Animation(anim, 0.0f, RenderObjClass::ANIM_MODE_LOOP);
        anim->Release_Ref();

        D3D11Backend::Configuration c;
        c.software = true; c.width = c.height = kSize; c.depth_buffer = true;
        std::unique_ptr<D3D11Backend> backend = D3D11Backend::Create(c);
        ASSERT_TRUE(backend);
        CameraClass camera;
        camera.Set_Aspect_Ratio(1.0f);
        camera.Set_View_Plane(DEG_TO_RADF(40.0f), -1);
        camera.Set_Clip_Planes(1.0f, 500.0f);
        camera.Set_Viewport(Vector2(0, 0), Vector2(1, 1));
        Matrix3D cam(1);
        cam.Look_At(Vector3(35.0f, -45.0f, 20.0f), Vector3(0.0f, 0.0f, 8.0f), 0.0f);
        camera.Set_Transform(cam);
        NativeWorldObjects::SceneLighting lights = {};
        lights.ambient[0] = lights.ambient[1] = lights.ambient[2] = 0.4f;
        lights.count = 1;
        lights.direction[0][0] = 0.4f; lights.direction[0][1] = -0.5f; lights.direction[0][2] = 0.77f;
        lights.diffuse[0][0] = lights.diffuse[0][1] = lights.diffuse[0][2] = 0.8f;
        NativeWorldObjects::ObjectInput input = {};
        input.object = ranger;
        input.opacity = 1.0f;
        input.infantry = true;
        auto frame = [&]() {
            backend->Clear(true, true, Vector3(0.1f, 0.1f, 0.1f), 1.0f, 1.0f, 0);
            backend->Begin_Scene();
            NativeWorldObjects::renderObjects(*backend, camera, lights, lights, &input, 1);
            backend->End_Scene(false);
            return backend->Readback_RGBA8();
        };
        const std::vector<unsigned char> first = frame();
        WW3D::Update_Logic_Frame_Time(450.0f);
        const std::vector<unsigned char> second = frame();
        size_t differing = 0;
        for (size_t i = 0; i < first.size(); ++i) if (first[i] != second[i]) ++differing;
        std::printf("animation=%s differing_bytes=%zu skinned=%u\n", animName.c_str(), differing, NativeWorldObjects::stats().skinned);
        EXPECT_GT(differing, 200u);   // the skin was re-deformed from the advanced animation frame
        NativeWorldObjects::releaseAll();
        ranger->Release_Ref();
        manager.Free_Assets();
    }
    NativeWorldObjects::setFileReaderForTests({});
    NativeWorldObjects::setRenderTargetSizeForTests(0, 0);
    _TheFileFactory = previous;
}

TEST(NativeWorldObjectsTest, FoliageUsesTheTreeTextureAlphaTestAndSkipsTranslucentSorting)
{
    NativeWorldObjects::resetStatsForTests();
    const char *assets = std::getenv("GENERALS_OFFLINE_ASSETS");
    if (!assets || !*assets) GTEST_SKIP() << "GENERALS_OFFLINE_ASSETS not set (opt-in asset test)";
    const char *outDir = std::getenv("GENERALS_OFFLINE_OUT");
    std::string dir = assets;
    if (dir.back() != '\\' && dir.back() != '/') dir += '\\';
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
    constexpr unsigned kSize = 256;
    NativeWorldObjects::setRenderTargetSizeForTests(kSize, kSize);
    {
        WW3DAssetManager manager;
        manager.Set_WW3D_Load_On_Demand(true);
        const char *models[] = {"PTPINE01", "PTOAK01"};
        const char *textures[] = {"ptpine01.tga", "ptoak01.tga"};
        std::vector<RenderObjClass *> objects;
        std::vector<NativeWorldObjects::ObjectInput> inputs;
        for (int i = 0; i < 2; ++i) {
            RenderObjClass *tree = manager.Create_Render_Obj(models[i]);
            ASSERT_NE(tree, nullptr) << models[i];
            Matrix3D tm(1);
            tm.Set_Translation(Vector3(-25.0f + 50.0f * i, 0.0f, 0.0f));
            tree->Set_Transform(tm);
            objects.push_back(tree);
            NativeWorldObjects::ObjectInput input = {};
            input.object = tree;
            input.opacity = 1.0f;
            input.textureOverride = textures[i];
            input.foliage = true;
            inputs.push_back(input);
        }
        D3D11Backend::Configuration c;
        c.software = true; c.width = c.height = kSize; c.depth_buffer = true;
        std::unique_ptr<D3D11Backend> backend = D3D11Backend::Create(c);
        ASSERT_TRUE(backend);
        CameraClass camera;
        camera.Set_Aspect_Ratio(1.0f);
        camera.Set_View_Plane(DEG_TO_RADF(40.0f), -1);
        camera.Set_Clip_Planes(1.0f, 800.0f);
        camera.Set_Viewport(Vector2(0, 0), Vector2(1, 1));
        Matrix3D cam(1);
        cam.Look_At(Vector3(0.0f, -160.0f, 70.0f), Vector3(0.0f, 0.0f, 25.0f), 0.0f);
        camera.Set_Transform(cam);
        NativeWorldObjects::SceneLighting lights = {};
        lights.ambient[0] = lights.ambient[1] = lights.ambient[2] = 0.45f;
        lights.count = 1;
        lights.direction[0][0] = 0.4f; lights.direction[0][1] = -0.5f; lights.direction[0][2] = 0.77f;
        lights.diffuse[0][0] = lights.diffuse[0][1] = lights.diffuse[0][2] = 0.7f;
        backend->Clear(true, true, Vector3(0.3f, 0.45f, 0.2f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        const int draws = NativeWorldObjects::renderObjects(*backend, camera, lights, lights, inputs.data(),
                                                            static_cast<unsigned>(inputs.size()));
        backend->End_Scene(false);
        const std::vector<unsigned char> pixels = backend->Readback_RGBA8();
        const NativeWorldObjects::Stats stats = NativeWorldObjects::stats();
        const unsigned char background[3] = {pixels[0], pixels[1], pixels[2]};
        const unsigned drawn = CountNonBackground(pixels, background);
        std::printf("foliage draws=%d triangles=%u texture_failures=%u translucent=%u non_background=%u\n", draws,
                    stats.triangles, stats.textureFailures, stats.translucentBatches, drawn);
        EXPECT_GT(draws, 0);
        EXPECT_EQ(stats.textureFailures, 0u);
        EXPECT_EQ(stats.translucentBatches, 0u);   // foliage is alpha tested and depth writing, never sorted
        EXPECT_GT(drawn, 400u);
        EXPECT_LT(drawn, kSize * kSize / 2);       // cut-out leaves, not full quads
        if (outDir && *outDir) WriteBmp(std::string(outDir) + "\\native_foliage.bmp", pixels, kSize, kSize);
        NativeWorldObjects::releaseAll();
        for (RenderObjClass *object : objects) object->Release_Ref();
        manager.Free_Assets();
    }
    NativeWorldObjects::setFileReaderForTests({});
    NativeWorldObjects::setRenderTargetSizeForTests(0, 0);
    _TheFileFactory = previous;
}

#include "BigArchiveTestSupport.h"

// Bulk robustness: every renderable prototype of the retail W3D archive goes through the native mesh
// path (base pose) without a fault; unsupported conditions are tallied. Opt-in (GENERALS_GAME_DIR).
TEST(NativeWorldObjectsTest, EveryRetailW3DPrototypeRendersWithoutFaults)
{
    NativeWorldObjects::resetStatsForTests();
    const char *game = std::getenv("GENERALS_GAME_DIR");
    if (!game || !*game) GTEST_SKIP() << "GENERALS_GAME_DIR not set (opt-in archive test)";
    std::string dir = game;
    if (dir.back() != '\\' && dir.back() != '/') dir += '\\';
    BigFiles big;
    for (const char *archive : {"Textures.big", "Terrain.big", "W3D.big", "Patch.big"})
        big.Add(dir + archive);
    const char *limitText = std::getenv("GENERALS_MODEL_LIMIT");
    const size_t limit = limitText ? static_cast<size_t>(std::strtoul(limitText, nullptr, 10)) : 0;

    BigFileFactory factory(big);
    FileFactoryClass *previous = _TheFileFactory;
    _TheFileFactory = &factory;
    auto reader = [&big](const std::string &path, std::vector<unsigned char> &bytes) { return big.Read(path, bytes); };
    NativeWorldObjects::setFileReaderForTests(reader);
    constexpr unsigned kSize = 160;
    NativeWorldObjects::setRenderTargetSizeForTests(kSize, kSize);
    {
        WW3DAssetManager manager;
        manager.Set_WW3D_Load_On_Demand(false);
        D3D11Backend::Configuration c;
        c.software = true; c.width = c.height = kSize; c.depth_buffer = true;
        std::unique_ptr<D3D11Backend> backend = D3D11Backend::Create(c);
        ASSERT_TRUE(backend);

        const std::vector<std::string> names = big.Names(".w3d");
        std::vector<std::string> order;
        for (const std::string &n : names) if (EndsWith(n, "_skl.w3d")) order.push_back(n);
        const size_t skeletons = order.size();
        for (const std::string &n : names) if (!EndsWith(n, "_skl.w3d")) order.push_back(n);

        NativeWorldObjects::SceneLighting lights = {};
        lights.ambient[0] = lights.ambient[1] = lights.ambient[2] = 0.4f;
        lights.count = 1;
        lights.direction[0][0] = 0.4f; lights.direction[0][1] = -0.5f; lights.direction[0][2] = 0.77f;
        lights.diffuse[0][0] = lights.diffuse[0][1] = lights.diffuse[0][2] = 0.8f;

        std::set<std::string> seenPrototypes;
        size_t files = 0, prototypes = 0, rendered = 0, empty = 0, classCounts[64] = {};
        std::vector<std::string> emptyNames, failureNames;
        size_t failures = 0, degenerate = 0;
        unsigned maxTriangles = 0;
        for (const std::string &file : order) {
            if (limit && files >= limit + skeletons) break;
            ++files;
            if (!manager.Load_3D_Assets(file.c_str())) continue;
            std::vector<std::string> fresh;
            {
                std::unique_ptr<RenderObjIterator> it(manager.Create_Render_Obj_Iterator());
                for (it->First(); !it->Is_Done(); it->Next()) {
                    const std::string name = it->Current_Item_Name();
                    if (seenPrototypes.insert(name).second) fresh.push_back(name);
                }
            }
            for (const std::string &name : fresh) {
                ++prototypes;
                RenderObjClass *object = manager.Create_Render_Obj(name.c_str());
                if (!object) continue;
                const int id = object->Class_ID();
                if (id >= 0 && id < 64) ++classCounts[id];
                if (id == RenderObjClass::CLASSID_HLOD || id == RenderObjClass::CLASSID_MESH ||
                    id == RenderObjClass::CLASSID_COLLECTION || id == RenderObjClass::CLASSID_DISTLOD) {
                    const SphereClass sphere = object->Get_Bounding_Sphere();
                    if (!std::isfinite(sphere.Radius) || !std::isfinite(sphere.Center.X) ||
                        !std::isfinite(sphere.Center.Y) || !std::isfinite(sphere.Center.Z)) {
                        // Animation-only prototypes (no geometry) have no finite bounds; the game camera is unaffected.
                        ++degenerate;
                        object->Release_Ref();
                        continue;
                    }
                    const float radius = sphere.Radius > 0.01f ? sphere.Radius : 1.0f;
                    CameraClass camera;
                    camera.Set_Aspect_Ratio(1.0f);
                    camera.Set_View_Plane(DEG_TO_RADF(40.0f), -1);
                    camera.Set_Clip_Planes(radius * 0.05f, radius * 20.0f);
                    camera.Set_Viewport(Vector2(0, 0), Vector2(1, 1));
                    Matrix3D cam(1);
                    const Vector3 target = sphere.Center;
                    cam.Look_At(target + Vector3(radius * 2.2f, -radius * 2.2f, radius * 1.6f), target, 0.0f);
                    camera.Set_Transform(cam);
                    NativeWorldObjects::ObjectInput input = {};
                    input.object = object;
                    input.hexColor = 0xff3070ff;
                    input.opacity = 1.0f;
                    backend->Clear(true, true, Vector3(0, 0, 0), 1.0f, 1.0f, 0);
                    backend->Begin_Scene();
                    const unsigned before = NativeWorldObjects::stats().triangles;
                    int draws = 0;
                    try {
                        draws = NativeWorldObjects::renderObjects(*backend, camera, lights, lights, &input, 1);
                    } catch (const std::exception &error) {
                        ++failures;
                        char extra[160];
                        std::snprintf(extra, sizeof(extra), " class=%d center=(%g,%g,%g) radius=%g subobjects=%d", id,
                                      sphere.Center.X, sphere.Center.Y, sphere.Center.Z, sphere.Radius, object->Get_Num_Sub_Objects());
                        if (failureNames.size() < 40) failureNames.push_back(name + ": " + error.what() + extra);
                    }
                    backend->End_Scene(false);
                    const unsigned triangles = NativeWorldObjects::stats().triangles - before;
                    if (draws > 0) ++rendered;
                    else { ++empty; if (emptyNames.size() < 40) emptyNames.push_back(name); }
                    if (triangles > maxTriangles) maxTriangles = triangles;
                }
                object->Release_Ref();
            }
            // Re-installing the reader also drops the decoded-texture cache (bounded memory).
            if (files % 25 == 0) NativeWorldObjects::setFileReaderForTests(reader);
        }
        const NativeWorldObjects::Stats stats = NativeWorldObjects::stats();
        std::printf("files=%zu prototypes=%zu rendered=%zu empty=%zu max_triangles=%u meshes=%u skinned=%u texture_loads=%u "
                    "texture_failures=%u unsupported_class=%u unsupported_state=%u chunk_overflow=%u team_colored=%u\n",
                    files, prototypes, rendered, empty, maxTriangles, stats.meshes, stats.skinned, stats.textureLoads,
                    stats.textureFailures, stats.unsupportedClass, stats.unsupportedState, stats.chunkOverflow, stats.teamColored);
        std::printf("state families: multipass=%u mapper=%u blend=%u modulate2x=%u secondary=%u detail=%u\n",
                    stats.stateMultiPass, stats.stateMapper, stats.stateBlend, stats.stateModulate2x,
                    stats.stateSecondary, stats.stateDetail);
        for (int i = 0; i < 24; ++i)
            if (stats.mapperKinds[i]) std::printf("mapper kind %d: %u\n", i, stats.mapperKinds[i]);
        for (int i = 0; i < 64; ++i)
            if (classCounts[i]) std::printf("class %d: %zu\n", i, classCounts[i]);
        for (const std::string &n : emptyNames) std::printf("empty: %s\n", n.c_str());
        std::printf("exceptions=%zu degenerate_bounds=%zu\n", failures, degenerate);
        for (const std::string &n : failureNames) std::printf("exception: %s\n", n.c_str());
        for (unsigned i = 0; i < NativeWorldObjects::failedTextureCount() && i < 12; ++i)
            std::printf("texture failure: %s\n", NativeWorldObjects::failedTexture(i));
        EXPECT_GT(rendered, 100u);
        EXPECT_EQ(stats.chunkOverflow, 0u);
        EXPECT_EQ(failures, 0u);
        // X64.29 coverage lock: every retail prototype's material state has a native form (multi-pass drawn,
        // texture mappers evaluated); a regression that re-introduces unsupported families fails here.
        EXPECT_EQ(stats.unsupportedState, 0u);
        EXPECT_EQ(stats.unsupportedClass, 0u);
        EXPECT_GT(stats.mapperApplied, 1000u);
        EXPECT_GT(stats.stateMultiPass, 1000u);
        NativeWorldObjects::releaseAll();
        manager.Free_Assets();
    }
    NativeWorldObjects::setFileReaderForTests({});
    NativeWorldObjects::setRenderTargetSizeForTests(0, 0);
    _TheFileFactory = previous;
}

// Visual evidence gallery for the material families the sweep measures (environment-mapped and
// multi-pass models): the first prototypes whose draw applied a mapper or more than one pass are rendered
// into one tiled image. Opt-in (GENERALS_GAME_DIR + GENERALS_OFFLINE_OUT).
TEST(NativeWorldObjectsTest, MaterialFamilyGalleryRendersMappedAndMultiPassModels)
{
    NativeWorldObjects::resetStatsForTests();
    const char *game = std::getenv("GENERALS_GAME_DIR");
    const char *outDir = std::getenv("GENERALS_OFFLINE_OUT");
    if (!game || !*game || !outDir || !*outDir) GTEST_SKIP() << "GENERALS_GAME_DIR/GENERALS_OFFLINE_OUT not set (opt-in)";
    std::string dir = game;
    if (dir.back() != '\\' && dir.back() != '/') dir += '\\';
    BigFiles big;
    for (const char *archive : {"Textures.big", "Terrain.big", "W3D.big", "Patch.big"})
        big.Add(dir + archive);
    BigFileFactory factory(big);
    FileFactoryClass *previous = _TheFileFactory;
    _TheFileFactory = &factory;
    NativeWorldObjects::setFileReaderForTests(
        [&big](const std::string &path, std::vector<unsigned char> &bytes) { return big.Read(path, bytes); });
    constexpr unsigned kTile = 192, kCols = 4, kRows = 3;
    NativeWorldObjects::setRenderTargetSizeForTests(kTile, kTile);
    {
        WW3DAssetManager manager;
        manager.Set_WW3D_Load_On_Demand(false);
        D3D11Backend::Configuration c;
        c.software = true; c.width = c.height = kTile; c.depth_buffer = true;
        std::unique_ptr<D3D11Backend> backend = D3D11Backend::Create(c);
        ASSERT_TRUE(backend);
        NativeWorldObjects::SceneLighting lights = {};
        lights.ambient[0] = lights.ambient[1] = lights.ambient[2] = 0.4f;
        lights.count = 1;
        lights.direction[0][0] = 0.4f; lights.direction[0][1] = -0.5f; lights.direction[0][2] = 0.77f;
        lights.diffuse[0][0] = lights.diffuse[0][1] = lights.diffuse[0][2] = 0.8f;

        std::vector<unsigned char> sheet(static_cast<size_t>(kTile) * kCols * kTile * kRows * 4, 0);
        unsigned placed = 0, withMapper = 0, withPasses = 0;
        const std::vector<std::string> names = big.Names(".w3d");
        std::set<std::string> seen;
        // GENERALS_GALLERY_MODELS="AVCRUSADER,AVCOMANCHE": render exactly these prototypes instead of scanning.
        std::vector<std::string> explicitModels;
        if (const char *list = std::getenv("GENERALS_GALLERY_MODELS")) {
            std::string token;
            for (const char *p = list;; ++p) {
                if (*p == ',' || *p == 0) { if (!token.empty()) explicitModels.push_back(token); token.clear(); if (!*p) break; }
                else token += *p;
            }
        }
        if (!explicitModels.empty()) {
            for (const std::string &file : names)
                if (!EndsWith(file, "_skl.w3d")) manager.Load_3D_Assets(file.c_str());
        }
        for (const std::string &file : names) {
            if (placed >= kCols * kRows) break;
            if (!explicitModels.empty() && &file != &names[0]) break;
            if (EndsWith(file, "_skl.w3d")) continue;
            std::vector<std::string> fresh;
            if (!explicitModels.empty()) {
                fresh = explicitModels;
            } else if (manager.Load_3D_Assets(file.c_str())) {
                std::unique_ptr<RenderObjIterator> it(manager.Create_Render_Obj_Iterator());
                for (it->First(); !it->Is_Done(); it->Next()) {
                    const std::string name = it->Current_Item_Name();
                    if (seen.insert(name).second) fresh.push_back(name);
                }
            }
            for (const std::string &name : fresh) {
                if (placed >= kCols * kRows) break;
                RenderObjClass *object = manager.Create_Render_Obj(name.c_str());
                if (!object) continue;
                if (object->Class_ID() != RenderObjClass::CLASSID_HLOD) { object->Release_Ref(); continue; }
                const SphereClass sphere = object->Get_Bounding_Sphere();
                if (!std::isfinite(sphere.Radius) || sphere.Radius < 0.5f) { object->Release_Ref(); continue; }
                CameraClass camera;
                camera.Set_Aspect_Ratio(1.0f);
                camera.Set_View_Plane(DEG_TO_RADF(40.0f), -1);
                camera.Set_Clip_Planes(sphere.Radius * 0.05f, sphere.Radius * 20.0f);
                camera.Set_Viewport(Vector2(0, 0), Vector2(1, 1));
                Matrix3D cam(1);
                cam.Look_At(sphere.Center + Vector3(sphere.Radius * 2.2f, -sphere.Radius * 2.2f, sphere.Radius * 1.4f),
                            sphere.Center, 0.0f);
                camera.Set_Transform(cam);
                NativeWorldObjects::ObjectInput input = {};
                input.object = object;
                input.hexColor = 0xff3070ff;
                input.opacity = 1.0f;
                const NativeWorldObjects::Stats before = NativeWorldObjects::stats();
                backend->Clear(true, true, Vector3(0.2f, 0.25f, 0.3f), 1.0f, 1.0f, 0);
                backend->Begin_Scene();
                NativeWorldObjects::renderObjects(*backend, camera, lights, lights, &input, 1);
                backend->End_Scene(false);
                const NativeWorldObjects::Stats after = NativeWorldObjects::stats();
                const bool mapped = after.mapperApplied > before.mapperApplied;
                const bool multi = after.stateMultiPass > before.stateMultiPass;
                // Take mapped models first (environment maps are what the sweep found most often).
                const bool want = !explicitModels.empty() || (mapped && withMapper < 8) || (multi && !mapped && withPasses < 4);
                if (want) {
                    if (mapped) ++withMapper; else ++withPasses;
                    const std::vector<unsigned char> pixels = backend->Readback_RGBA8();
                    const unsigned col = placed % kCols, row = placed / kCols;
                    for (unsigned y = 0; y < kTile; ++y)
                        std::memcpy(&sheet[((static_cast<size_t>(row) * kTile + y) * kTile * kCols + col * kTile) * 4],
                                    &pixels[static_cast<size_t>(y) * kTile * 4], kTile * 4);
                    std::printf("gallery[%u]: %s mapper=%d passes=%d\n", placed, name.c_str(), mapped ? 1 : 0, multi ? 1 : 0);
                    ++placed;
                }
                object->Release_Ref();
            }
        }
        EXPECT_GT(placed, 0u);
        WriteBmp(std::string(outDir) + "\\native_gallery.bmp", sheet, kTile * kCols, kTile * kRows);
        std::printf("mapper_applied=%u multipass=%u unsupported_state=%u\n", NativeWorldObjects::stats().mapperApplied,
                    NativeWorldObjects::stats().stateMultiPass, NativeWorldObjects::stats().unsupportedState);
        NativeWorldObjects::releaseAll();
        manager.Free_Assets();
    }
    NativeWorldObjects::setFileReaderForTests({});
    NativeWorldObjects::setRenderTargetSizeForTests(0, 0);
    _TheFileFactory = previous;
}
#endif
