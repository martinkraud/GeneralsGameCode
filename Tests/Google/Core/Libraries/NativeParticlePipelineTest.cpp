// SPDX-License-Identifier: GPL-3.0-or-later
// X64.29 review follow-up: the whole particle chain, driven by the retail ParticleSystem.ini definitions:
//   INI template -> ParticleSystemManager::createParticleSystem -> update (particles alive) ->
//   NativeWorldEffects emission -> D3D11 submission on the particle layer -> pixels.
// The first native review showed particle systems and quads being counted while nothing was visible; the
// cause was the particle layer never being submitted. This test fails if any link of the chain breaks.
// Opt-in (GENERALS_GAME_DIR): needs the retail archives.
#include <gtest/gtest.h>
#if defined(_WIN64)
#include "Common/GlobalData.h"
#include "Common/GameType.h"
#include "Common/NameKeyGenerator.h"
#include "Common/FileSystem.h"
#include "Common/LocalFileSystem.h"
#include "Common/ArchiveFileSystem.h"
#include "Common/FramePacer.h"
#include "Common/INI.h"
#include "Common/GameLOD.h"
#include "GameLogic/TerrainLogic.h"
#include "GameClient/ParticleSys.h"
#include "W3DDevice/GameClient/W3DGameClient.h"
#include "W3DDevice/GameClient/NativeWorldEffects.h"
#include "W3DDevice/GameClient/NativeWorldObjects.h"
#include "Win32Device/Common/Win32BIGFileSystem.h"
#include "Win32Device/Common/Win32LocalFileSystem.h"
#include "WW3D2/Backend/D3D11Backend.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/camera.h"
#include "WWLib/ffactory.h"
#include "WWLib/ramfile.h"
#include "WWMath/matrix3d.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "BigArchiveTestSupport.h"

// The device library references the window-procedure owned mouse; the test executable has no WinMain.
class Win32Mouse;
Win32Mouse *TheWin32Mouse = nullptr;

namespace
{
// Concrete manager for the test (the render hooks are pure virtual in the base class).
class TestParticleSystemManager : public ParticleSystemManager
{
public:
    Int getOnScreenParticleCount() override { return 0; }
    void doParticles(RenderInfoClass &) override {}
    void queueParticleRender() override {}
};

std::vector<std::string> TemplateNames(const std::string &ini)
{
    // "ParticleSystem Name ... End" blocks whose emitter is a plain particle system that does not need terrain.
    std::vector<std::string> names;
    std::istringstream in(ini);
    std::string line, current;
    bool skip = false, inBlock = false;
    auto trim = [](std::string s) {
        const size_t a = s.find_first_not_of(" \t\r");
        const size_t b = s.find_last_not_of(" \t\r");
        return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
    };
    while (std::getline(in, line)) {
        const size_t semi = line.find(';');
        if (semi != std::string::npos) line.resize(semi);
        line = trim(line);
        if (line.compare(0, 15, "ParticleSystem ") == 0 && !inBlock) {
            current = trim(line.substr(15));
            inBlock = true; skip = false;
        } else if (inBlock && line == "End") {
            if (!skip && !current.empty()) names.push_back(current);
            inBlock = false;
        } else if (inBlock) {
            std::string compact;
            for (char c : line) if (c != ' ' && c != '\t') compact.push_back(c);
            if (compact == "EmitAboveGroundOnly=Yes" || compact == "Type=DRAWABLE" || compact == "Type=SMUDGE") skip = true;

        }
    }
    return names;
}
}

TEST(NativeParticlePipelineTest, RetailParticleDefinitionsRunFromIniToNativePixels)
{
    NativeWorldObjects::resetStatsForTests();
    NativeWorldEffects::resetStatsForTests();
    const char *game = std::getenv("GENERALS_GAME_DIR");
    if (!game || !*game) GTEST_SKIP() << "GENERALS_GAME_DIR not set (opt-in archive test)";
    std::string dir = game;
    if (dir.back() != '\\' && dir.back() != '/') dir += '\\';
    BigFiles big;
    for (const char *archive : {"INI.big", "Textures.big", "Terrain.big", "W3D.big", "Patch.big"}) big.Add(dir + archive);
    std::vector<unsigned char> iniBytes;
    ASSERT_TRUE(big.Read("ParticleSystem.ini", iniBytes));
    const std::string iniText(iniBytes.begin(), iniBytes.end());
    const char *outDir = std::getenv("GENERALS_OFFLINE_OUT");
    const std::string iniPath = std::string(outDir && *outDir ? outDir : ".") + "\\x29_particlesystem.ini";
    {
        std::ofstream out(iniPath, std::ios::binary);
        out.write(iniText.data(), static_cast<std::streamsize>(iniText.size()));
    }

    // --- engine globals the particle code reads (created only when the process does not have them) ---
    
    const bool madeGlobals = TheWritableGlobalData == nullptr;
    if (madeGlobals) TheWritableGlobalData = new GlobalData;
    const Bool oldUseFx = TheWritableGlobalData->m_useFX;
    TheWritableGlobalData->m_useFX = TRUE;
    // GameData.ini values the particle limits read (the retail file sets MaxParticleCount = 2500).
    const Int oldMaxParticles = TheWritableGlobalData->m_maxParticleCount;
    const Int oldMaxField = TheWritableGlobalData->m_maxFieldParticleCount;
    TheWritableGlobalData->m_maxParticleCount = 2500;
    TheWritableGlobalData->m_maxFieldParticleCount = 30;
    
    const bool madeKeys = TheNameKeyGenerator == nullptr;
    if (madeKeys) { TheNameKeyGenerator = new NameKeyGenerator; TheNameKeyGenerator->init(); }
    
    const bool madeFs = TheFileSystem == nullptr;
    if (madeFs) {
        TheFileSystem = new FileSystem;
        TheLocalFileSystem = new Win32LocalFileSystem;
        TheArchiveFileSystem = new Win32BIGFileSystem;
    }
    
    const bool madePacer = TheFramePacer == nullptr;
    if (madePacer) TheFramePacer = new FramePacer;
    
    GameClient *previousClient = TheGameClient;
    if (!TheGameClient) TheGameClient = new W3DGameClient;
    
    if (!TheGameLODManager) TheGameLODManager = new GameLODManager;   // particle LOD priorities (defaults: all kept)
    if (!TheTerrainLogic) TheTerrainLogic = new TerrainLogic;   // flat ground at height 0
    ParticleSystemManager *previousManager = TheParticleSystemManager;
    auto *manager = new TestParticleSystemManager;
    TheParticleSystemManager = manager;

    
    {
        INI ini;
        ini.loadFileDirectory(AsciiString(iniPath.c_str()), INI_LOAD_OVERWRITE, nullptr);
    }
    
    std::vector<std::string> names = TemplateNames(iniText);
    ASSERT_GT(names.size(), 100u) << "ParticleSystem.ini defines many systems";

    NativeWorldObjects::setFileReaderForTests(
        [&big](const std::string &path, std::vector<unsigned char> &bytes) { return big.Read(path, bytes); });
    constexpr unsigned kSize = 192;
    NativeWorldObjects::setRenderTargetSizeForTests(kSize, kSize);
    {
        D3D11Backend::Configuration c;
        c.software = true; c.width = c.height = kSize; c.depth_buffer = true;
        std::unique_ptr<D3D11Backend> backend = D3D11Backend::Create(c);
        ASSERT_TRUE(backend);
        CameraClass camera;
        camera.Set_Aspect_Ratio(1.0f);
        camera.Set_View_Plane(DEG_TO_RADF(45.0f), -1);
        camera.Set_Clip_Planes(1.0f, 4000.0f);
        camera.Set_Viewport(Vector2(0, 0), Vector2(1, 1));
        Matrix3D cam(1);
        cam.Look_At(Vector3(0.0f, -260.0f, 180.0f), Vector3(0.0f, 0.0f, 40.0f), 0.0f);
        camera.Set_Transform(cam);
        NativeWorldObjects::SceneLighting lights = {};
        lights.ambient[0] = lights.ambient[1] = lights.ambient[2] = 0.5f;

        auto render = [&]() {
            backend->Clear(true, true, Vector3(0.25f, 0.3f, 0.35f), 1.0f, 1.0f, 0);
            backend->Begin_Scene();
            NativeWorldObjects::renderObjects(*backend, camera, lights, lights, nullptr, 0);
            backend->End_Scene(false);
            return backend->Readback_RGBA8();
        };
        const std::vector<unsigned char> empty = render();

        // Create one system per template (a bounded sample), run them for a second of logic time, and require
        // that most of them reach the screen through the native path.
        const size_t sample = std::min<size_t>(names.size(), 60);
        const size_t step = std::max<size_t>(1, names.size() / sample);
        std::vector<ParticleSystem *> systems;
        unsigned requested = 0, created = 0;
        for (size_t i = 0; i < names.size() && systems.size() < sample; i += step) {
            const ParticleSystemTemplate *tmpl = manager->findTemplate(AsciiString(names[i].c_str()));
            ++requested;
            if (!tmpl) continue;
            ParticleSystem *sys = manager->createParticleSystem(tmpl, FALSE);
            if (!sys) continue;
            ++created;
            Coord3D pos = {static_cast<Real>((static_cast<int>(systems.size() % 8) - 4) * 18), static_cast<Real>(((static_cast<int>(systems.size()) / 8) % 4 - 2) * 14), 30.0f};
            sys->setPosition(&pos);
            systems.push_back(sys);
        }
        EXPECT_EQ(manager->getPipelineStats().systemsRequested, created) << "every template looked up resolved";
        EXPECT_GT(created, 20u);
        EXPECT_EQ(manager->getPipelineStats().systemsNullTemplate, 0u);

        unsigned peakAlive = 0;
        std::vector<unsigned char> pixels;
        for (int frame = 0; frame < 45; ++frame) {
            manager->update();
            peakAlive = std::max(peakAlive, manager->getParticleCount());
            pixels = render();
        }
        const NativeWorldEffects::Stats fx = NativeWorldEffects::stats();
        const NativeWorldObjects::Stats os = NativeWorldObjects::stats();
        std::printf("particle pipeline: templates=%zu created=%u alive_peak=%u added=%u systems_with_particles=%u quads=%u "
                    "batches=%u streaks=%u submitted_particle_layer=%u effect_batches=%u failures=%u\n",
                    names.size(), created, peakAlive, manager->getPipelineStats().particlesAdded, fx.particleSystems,
                    fx.particlesDrawn, fx.particleBatches, fx.streaks, os.layerDraws[4], os.effectBatches, os.textureFailures);
        for (unsigned i = 0; i < NativeWorldObjects::failedTextureCount() && i < 12; ++i)
            std::printf("particle texture failure: %s\n", NativeWorldObjects::failedTexture(i));
        EXPECT_GT(peakAlive, 20u) << "systems emitted particles";
        EXPECT_GT(manager->getPipelineStats().particlesAdded, 100u);
        EXPECT_GT(fx.particlesDrawn, 100u) << "particles reached native geometry";
        EXPECT_GT(fx.particleBatches, 10u);
        EXPECT_GT(os.layerDraws[4], 10u) << "particle-layer batches were submitted to D3D11";
        EXPECT_EQ(fx.dropped, 0u);
        unsigned changed = 0;
        for (size_t i = 0; i + 3 < pixels.size(); i += 4)
            if (pixels[i] != empty[i] || pixels[i + 1] != empty[i + 1] || pixels[i + 2] != empty[i + 2]) ++changed;
        EXPECT_GT(changed, 300u) << "particles are visible in the frame";

        if (outDir && *outDir) {
            const std::string path = std::string(outDir) + "\\native_particles.bmp";
            if (FILE *f = std::fopen(path.c_str(), "wb")) {
                unsigned char header[54] = {'B', 'M'};
                const unsigned w = kSize, h = kSize, dataSize = w * h * 4, fileSize = 54 + dataSize, offset = 54, dib = 40, planes = 1, bits = 32;
                const int topDown = -static_cast<int>(h);
                std::memcpy(header + 2, &fileSize, 4); std::memcpy(header + 10, &offset, 4); std::memcpy(header + 14, &dib, 4);
                std::memcpy(header + 18, &w, 4); std::memcpy(header + 22, &topDown, 4);
                std::memcpy(header + 26, &planes, 2); std::memcpy(header + 28, &bits, 2); std::memcpy(header + 34, &dataSize, 4);
                std::fwrite(header, 1, 54, f);
                std::vector<unsigned char> bgra(pixels.size());
                for (size_t i = 0; i + 3 < pixels.size(); i += 4) { bgra[i] = pixels[i + 2]; bgra[i + 1] = pixels[i + 1]; bgra[i + 2] = pixels[i]; bgra[i + 3] = 255; }
                std::fwrite(bgra.data(), 1, bgra.size(), f);
                std::fclose(f);
            }
        }
        for (ParticleSystem *s : systems) s->destroy();
        for (int i = 0; i < 5; ++i) manager->update();
        NativeWorldObjects::releaseAll();
    }
    NativeWorldObjects::setFileReaderForTests({});
    NativeWorldObjects::setRenderTargetSizeForTests(0, 0);

    delete manager;                       // its systems unlink through TheParticleSystemManager
    TheParticleSystemManager = previousManager;
    TheWritableGlobalData->m_useFX = oldUseFx;
    TheWritableGlobalData->m_maxParticleCount = oldMaxParticles;
    TheWritableGlobalData->m_maxFieldParticleCount = oldMaxField;
    (void)madeGlobals; (void)madeKeys; (void)madeFs; (void)madePacer; (void)previousClient;   // process-lifetime test doubles
}
#endif
