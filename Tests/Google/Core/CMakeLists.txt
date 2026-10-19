# Core GameEngine code needs a game to compile, so g_googletest and z_googletest compile these sources.
set(COREI_GOOGLETEST_SRC
    GoogleTestMain.cpp
    GameEngine/Common/AsciiStringTest.cpp
    GameEngine/Common/FramePacerTest.cpp
    GameEngine/Common/PerformanceProfileTest.cpp
    GameEngine/Common/PathProfileTest.cpp
    GameEngine/Common/RetailOpenListTest.cpp
    GameEngine/Common/PathPhaseTest.cpp
    GameEngine/Common/DeveloperHarnessTest.cpp
    GameEngine/Common/PerformanceProfileBenchmark.cpp
    GameEngine/Common/PresentationClockTest.cpp
    GameEngine/Common/PresentationScrollTest.cpp
    GameEngine/Common/DisplayGeometryTest.cpp
    GameEngine/Common/PresentationSmoothnessTest.cpp
    GameEngine/Common/DisplaySettingsTest.cpp
    GameEngine/Common/DisplayTransactionTest.cpp
    GameEngine/Common/RenderScaleTest.cpp
    GameEngine/Common/SimulationTickStatsTest.cpp
    GameEngine/Common/DeterminismFingerprintTest.cpp
    GameEngine/Common/SimulationWorkerPlanTest.cpp
    GameEngine/Common/ZoneEquivalenceTest.cpp
    GameEngine/Common/OpenListIndexTest.cpp
    GameEngine/Common/ScriptProfileTest.cpp
    GameEngine/Common/EnhancedHudLayoutTest.cpp
    GameEngine/Common/EnhancedHudInteractiveTest.cpp
    GameEngine/Common/GroundTranslationTest.cpp
    GameEngine/Common/GroundOrientationTest.cpp
    GameEngine/Common/GroundTranslationPolicyTest.cpp
    GameEngine/Common/GroundTranslationBenchmark.cpp
    GameEngine/Common/OptionPreferencesTest.cpp
    GameEngine/Common/PlayWorldLifecycleTest.cpp
    Libraries/Compression/CompressionManagerTest.cpp
    Libraries/Render2DSubmissionTest.cpp
    Libraries/Texture2DTest.cpp
    Libraries/UIProducerTest.cpp
    Libraries/NativeTerrainTest.cpp
    Libraries/NativeWorldObjectsTest.cpp
    Libraries/NativeWorldEffectsTest.cpp
    Libraries/NativeParticlePipelineTest.cpp
    Libraries/NativeRadarTest.cpp
    Libraries/NativeWorldOverlaysTest.cpp
    Libraries/NativeDecalGridCacheTest.cpp
)

add_library(corei_googletest INTERFACE)

# Actual production Bezier routines against the legacy library, Win32 only.
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND MSVC)
    list(APPEND COREI_GOOGLETEST_SRC GameEngine/Common/BezierCompatibilityTest.cpp)
endif()

target_sources(corei_googletest INTERFACE ${COREI_GOOGLETEST_SRC})

target_link_libraries(corei_googletest INTERFACE
    comctl32
    depsi_googletest
    GTest::gtest
    imm32
    vfw32
    winmm
)
