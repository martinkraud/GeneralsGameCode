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

namespace {
// Minimal read-only BIG archive index (later archives override earlier entries by base name).
class BigFiles
{
public:
    bool Add(const std::string &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        unsigned char header[16];
        file.read(reinterpret_cast<char *>(header), 16);
        if (!file || std::memcmp(header, "BIGF", 4)) return false;
        const unsigned count = (header[8] << 24) | (header[9] << 16) | (header[10] << 8) | header[11];
        const size_t archive = paths_.size();
        paths_.push_back(path);
        for (unsigned i = 0; i < count; ++i) {
            unsigned char e[8];
            file.read(reinterpret_cast<char *>(e), 8);
            std::string name;
            for (char c; file.get(c) && c;) name.push_back(c);
            if (!file) return false;
            const unsigned offset = (e[0] << 24) | (e[1] << 16) | (e[2] << 8) | e[3];
            const unsigned size = (e[4] << 24) | (e[5] << 16) | (e[6] << 8) | e[7];
            files_[Base(name)] = Entry{archive, offset, size};
        }
        return true;
    }
    bool Read(const std::string &name, std::vector<unsigned char> &out) const
    {
        const auto found = files_.find(Base(name));
        if (found == files_.end()) return false;
        std::ifstream file(paths_[found->second.archive], std::ios::binary);
        file.seekg(found->second.offset);
        out.resize(found->second.size);
        file.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(out.size()));
        return static_cast<bool>(file);
    }
    std::vector<std::string> Names(const char *suffix) const
    {
        std::vector<std::string> names;
        const size_t length = std::strlen(suffix);
        for (const auto &entry : files_)
            if (entry.first.size() >= length && !entry.first.compare(entry.first.size() - length, length, suffix))
                names.push_back(entry.first);
        std::sort(names.begin(), names.end());
        return names;
    }
private:
    struct Entry { size_t archive; unsigned offset, size; };
    static std::string Base(const std::string &name)
    {
        const size_t slash = name.find_last_of("\\/");
        std::string base = slash == std::string::npos ? name : name.substr(slash + 1);
        for (char &c : base) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return base;
    }
    std::vector<std::string> paths_;
    std::unordered_map<std::string, Entry> files_;
};

class BigFileFactory : public FileFactoryClass
{
public:
    explicit BigFileFactory(const BigFiles &files) : files_(files) {}
    FileClass *Get_File(char const *filename) override
    {
        std::vector<unsigned char> bytes;
        if (!filename || !files_.Read(filename, bytes) || bytes.empty()) return nullptr;
        auto owned = std::make_unique<std::vector<unsigned char>>(std::move(bytes));
        FileClass *file = new RAMFileClass(owned->data(), static_cast<int>(owned->size()));
        storage_[file] = std::move(owned);
        return file;
    }
    void Return_File(FileClass *file) override
    {
        storage_.erase(file);
        delete file;
    }
private:
    const BigFiles &files_;
    std::unordered_map<FileClass *, std::unique_ptr<std::vector<unsigned char>>> storage_;
};

bool EndsWith(const std::string &s, const char *suffix)
{
    const size_t n = std::strlen(suffix);
    return s.size() >= n && !s.compare(s.size() - n, n, suffix);
}
}

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
        NativeWorldObjects::releaseAll();
        manager.Free_Assets();
    }
    NativeWorldObjects::setFileReaderForTests({});
    NativeWorldObjects::setRenderTargetSizeForTests(0, 0);
    _TheFileFactory = previous;
}
#endif
