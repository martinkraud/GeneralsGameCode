# g_googletest and z_googletest compile these sources together with the Core tests.
set(DEPSI_GOOGLETEST_SRC
    Utility/stringex_test.cpp
)

add_library(depsi_googletest INTERFACE)

target_sources(depsi_googletest INTERFACE ${DEPSI_GOOGLETEST_SRC})

target_link_libraries(depsi_googletest INTERFACE
    deps_utility
)

# Characterization only: actual pinned Win32 helpers, never production migration.
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND MSVC)
    add_executable(legacy_resource_characterization ../Core/Libraries/LegacyResourceCharacterizationTest.cpp)
    target_compile_features(legacy_resource_characterization PRIVATE cxx_std_17)
    target_compile_definitions(legacy_resource_characterization PRIVATE NOMINMAX
        RESOURCE_EVIDENCE_DIR="${PROJECT_SOURCE_DIR}/build/x64-9-resource-evidence"
        RESOURCE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_link_libraries(legacy_resource_characterization PRIVATE legacy_resource legacy_dx8_dependencies GTest::gtest_main advapi32 gdi32 user32)
    add_test(NAME legacy_resource_characterization COMMAND legacy_resource_characterization)
endif()

# Run the unchanged characterization inputs/references through the candidate.
if(MSVC)
    add_executable(legacy_resource_comparison ../Core/Libraries/LegacyResourceCharacterizationTest.cpp)
    target_compile_features(legacy_resource_comparison PRIVATE cxx_std_17)
    target_compile_definitions(legacy_resource_comparison PRIVATE NOMINMAX LEGACY_RESOURCE_REPLACEMENT
        RESOURCE_EVIDENCE_DIR="${PROJECT_SOURCE_DIR}/build/x64-10-resource-${CMAKE_SIZEOF_VOID_P}-evidence"
        RESOURCE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_link_libraries(legacy_resource_comparison PRIVATE legacy_resource legacy_dx8_dependencies GTest::gtest_main advapi32 gdi32 user32)
    add_test(NAME legacy_resource_comparison COMMAND legacy_resource_comparison)
endif()

# Fixed CPU diagnostics need neither a graphics SDK nor device implementation.
if(MSVC)
    add_executable(native_startup_fp_test Utility/NativeStartupFPTest.cpp)
    target_compile_features(native_startup_fp_test PRIVATE cxx_std_17)
    target_compile_options(native_startup_fp_test PRIVATE /fp:strict)
    target_include_directories(native_startup_fp_test PRIVATE ${PROJECT_SOURCE_DIR}/Core/GameEngine/Include)
    add_executable(native_startup_gate_test Utility/NativeStartupGateTest.cpp)
    target_compile_features(native_startup_gate_test PRIVATE cxx_std_17)
    target_include_directories(native_startup_gate_test PRIVATE ${PROJECT_SOURCE_DIR}/Core/GameEngine/Include)
    target_link_libraries(native_startup_gate_test PRIVATE GTest::gtest_main)
    add_test(NAME native_startup_gate_test COMMAND native_startup_gate_test)
    target_link_libraries(native_startup_fp_test PRIVATE GTest::gtest_main)
    add_test(NAME native_startup_fp_test COMMAND native_startup_fp_test)
endif()

add_executable(legacy_error_string_test ../Core/Libraries/LegacyErrorStringTest.cpp)
target_include_directories(legacy_error_string_test PRIVATE ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas)
target_link_libraries(legacy_error_string_test PRIVATE GTest::gtest_main)
add_test(NAME legacy_error_string_test COMMAND legacy_error_string_test)
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND MSVC)
    add_executable(legacy_error_string_comparison ../Core/Libraries/LegacyErrorStringTest.cpp)
    target_compile_definitions(legacy_error_string_comparison PRIVATE LEGACY_ERROR_LIVE_REFERENCE)
    target_include_directories(legacy_error_string_comparison PRIVATE ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas)
    target_link_libraries(legacy_error_string_comparison PRIVATE legacy_dx8_dependencies GTest::gtest_main advapi32 gdi32)
    add_test(NAME legacy_error_string_comparison COMMAND legacy_error_string_comparison)
endif()

# Fixed legacy shader CPU data needs no graphics SDK or implementation library.
add_executable(legacy_fixed_shader_test ../Core/Libraries/LegacyFixedPixelShaderTest.cpp)
target_compile_features(legacy_fixed_shader_test PRIVATE cxx_std_14)
target_compile_definitions(legacy_fixed_shader_test PRIVATE PROFILER_ENABLED)
target_include_directories(legacy_fixed_shader_test PRIVATE ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas)
target_link_libraries(legacy_fixed_shader_test PRIVATE GTest::gtest_main)
add_test(NAME legacy_fixed_shader_test COMMAND legacy_fixed_shader_test)
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND MSVC)
    add_executable(legacy_fixed_shader_comparison ../Core/Libraries/LegacyFixedPixelShaderTest.cpp)
    target_compile_features(legacy_fixed_shader_comparison PRIVATE cxx_std_14)
    target_compile_definitions(legacy_fixed_shader_comparison PRIVATE PROFILER_ENABLED LEGACY_SHADER_LIVE_REFERENCE)
    target_include_directories(legacy_fixed_shader_comparison PRIVATE ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas)
    target_link_libraries(legacy_fixed_shader_comparison PRIVATE legacy_dx8_dependencies GTest::gtest_main advapi32 gdi32)
    add_test(NAME legacy_fixed_shader_comparison COMMAND legacy_fixed_shader_comparison)
    add_executable(legacy_fixed_shader_device_test ../Core/Libraries/LegacyFixedPixelShaderDeviceTest.cpp)
    target_compile_features(legacy_fixed_shader_device_test PRIVATE cxx_std_14)
    target_compile_definitions(legacy_fixed_shader_device_test PRIVATE PROFILER_ENABLED)
    target_include_directories(legacy_fixed_shader_device_test PRIVATE ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas)
    target_link_libraries(legacy_fixed_shader_device_test PRIVATE legacy_dx8_dependencies GTest::gtest_main advapi32 gdi32 user32)
    add_test(NAME legacy_fixed_shader_device_test COMMAND legacy_fixed_shader_device_test)
endif()

# Controlled DX8 COM recording is independent of graphics/device availability.
add_executable(legacy_texture_creation_test ../Core/Libraries/LegacyTextureCreationTest.cpp)
target_compile_features(legacy_texture_creation_test PRIVATE cxx_std_17)
target_include_directories(legacy_texture_creation_test PRIVATE ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas)
target_link_libraries(legacy_texture_creation_test PRIVATE dx8_headers GTest::gtest_main)
add_test(NAME legacy_texture_creation_test COMMAND legacy_texture_creation_test)
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND MSVC)
    add_executable(legacy_texture_creation_reference ../Core/Libraries/LegacyTextureCreationReference.cpp)
    add_executable(legacy_texture_creation_comparison ../Core/Libraries/LegacyTextureCreationTest.cpp)
    foreach(target legacy_texture_creation_reference legacy_texture_creation_comparison)
        target_compile_features(${target} PRIVATE cxx_std_17)
        target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas)
        target_link_libraries(${target} PRIVATE legacy_dx8_dependencies advapi32 gdi32)
    endforeach()
    target_compile_definitions(legacy_texture_creation_comparison PRIVATE LEGACY_TEXTURE_LIVE_REFERENCE)
    target_link_libraries(legacy_texture_creation_comparison PRIVATE GTest::gtest_main)
    add_test(NAME legacy_texture_creation_comparison COMMAND legacy_texture_creation_comparison)
    # Separate real resource proof; system DX8 may be unavailable on a test host.
    add_executable(legacy_texture_creation_device_test ../Core/Libraries/LegacyTextureCreationDeviceTest.cpp)
    target_compile_features(legacy_texture_creation_device_test PRIVATE cxx_std_17)
    target_include_directories(legacy_texture_creation_device_test PRIVATE ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas)
    target_link_libraries(legacy_texture_creation_device_test PRIVATE legacy_dx8_dependencies GTest::gtest_main advapi32 gdi32 user32)
    add_test(NAME legacy_texture_creation_device_test COMMAND legacy_texture_creation_device_test)
endif()

# Integer-only FVF sizing. Independent tests use actual SDK/engine declarations
# for layout assertions, but the production boundary needs only <cstdint>.
add_executable(legacy_fvf_stride_test ../Core/Libraries/LegacyFVFStrideTest.cpp)
target_include_directories(legacy_fvf_stride_test PRIVATE
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWLib
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Include
)
target_compile_definitions(legacy_fvf_stride_test PRIVATE DISABLE_GAMEMEMORY)
target_link_libraries(legacy_fvf_stride_test PRIVATE dx8_headers deps_utility deps_precompiled GTest::gtest_main)
add_test(NAME legacy_fvf_stride_test COMMAND legacy_fvf_stride_test)
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND MSVC)
    add_executable(legacy_fvf_stride_reference ../Core/Libraries/LegacyFVFStrideReference.cpp)
    add_executable(legacy_fvf_stride_comparison ../Core/Libraries/LegacyFVFStrideTest.cpp)
    foreach(target legacy_fvf_stride_reference legacy_fvf_stride_comparison)
        target_include_directories(${target} PRIVATE
            ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas
            ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWLib
            ${PROJECT_SOURCE_DIR}/Core/Libraries/Include
        )
        target_compile_definitions(${target} PRIVATE DISABLE_GAMEMEMORY)
        target_link_libraries(${target} PRIVATE legacy_dx8_dependencies deps_utility deps_precompiled advapi32)
    endforeach()
    target_compile_definitions(legacy_fvf_stride_comparison PRIVATE LEGACY_FVF_LIVE_REFERENCE)
    target_link_libraries(legacy_fvf_stride_comparison PRIVATE GTest::gtest_main)
    add_test(NAME legacy_fvf_stride_comparison COMMAND legacy_fvf_stride_comparison)
endif()

add_executable(legacy_d3dx_math_test ../Core/Libraries/LegacyD3DXMathTest.cpp)
target_link_libraries(legacy_d3dx_math_test PRIVATE legacy_d3dx_math GTest::gtest_main)
add_test(NAME legacy_d3dx_math_test COMMAND legacy_d3dx_math_test)

# Live legacy oracle is deliberately Win32-only and not part of game linkage.
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND MSVC)
    add_executable(legacy_d3dx_math_reference ../Core/Libraries/LegacyD3DXMathReference.cpp)
    target_link_libraries(legacy_d3dx_math_reference PRIVATE legacy_dx8_dependencies advapi32)
    add_executable(legacy_d3dx_math_comparison ../Core/Libraries/LegacyD3DXMathTest.cpp)
    target_compile_definitions(legacy_d3dx_math_comparison PRIVATE LEGACY_MATH_LIVE_REFERENCE)
    target_link_libraries(legacy_d3dx_math_comparison PRIVATE legacy_d3dx_math legacy_dx8_dependencies GTest::gtest_main advapi32)
    add_test(NAME legacy_d3dx_math_comparison COMMAND legacy_d3dx_math_comparison)
endif()

# Platform diagnostics can be validated natively before the renderer/game links.
# The normal game/tool graph is still enabled and built separately.
if(WIN32)
    add_executable(windows_diagnostics_test Utility/windows_diagnostics_test.cpp)
    target_link_libraries(windows_diagnostics_test PRIVATE
        deps_dbghelp
        deps_utility
        GTest::gtest_main
    )
    add_test(NAME windows_diagnostics_test COMMAND windows_diagnostics_test)
endif()

# Actual neutral upload/submission sources, with no legacy_dx8_dependencies,
# core_wwcommon, title PCH, or SDK include directory. Also works before native
# game links; this does not replace the full game/tool build.
add_executable(render2d_submission_test
    ../Core/Libraries/Render2DSubmissionTest.cpp
    ../Core/Libraries/Texture2DTest.cpp
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/Render2DSubmission.cpp
)
target_include_directories(render2d_submission_test PRIVATE
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWLib
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Include
    ${PROJECT_SOURCE_DIR}/GeneralsMD/Code/Libraries/Source/WWVegas
)
target_compile_definitions(render2d_submission_test PRIVATE DISABLE_GAMEMEMORY)
target_link_libraries(render2d_submission_test PRIVATE deps_utility GTest::gtest_main)
add_test(NAME render2d_submission_test COMMAND render2d_submission_test)

# Real GDI sentence glyph producer + CPU backing. No engine/DX8 archive, SDK
# directory or graphics fake is linked. GDI is rasterization, not a D3D device.
add_executable(sentence_content_test
    ../Core/Libraries/SentenceContentTest.cpp
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/FontChars.cpp
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/Texture2DCPUContent.cpp
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWLib/wwstring.cpp
)
target_include_directories(sentence_content_test PRIVATE
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWLib
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Include
)
target_compile_definitions(sentence_content_test PRIVATE DISABLE_GAMEMEMORY)
target_link_libraries(sentence_content_test PRIVATE deps_utility GTest::gtest_main user32 gdi32)
add_test(NAME sentence_content_test COMMAND sentence_content_test)

# Real D3D11/WARP rendering and controlled hidden-window DXGI validation.
add_executable(d3d11_sentence_test
    ../Core/Libraries/D3D11SentenceTest.cpp
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/FontChars.cpp
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/Texture2DCPUContent.cpp
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/Render2DSubmission.cpp
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWLib/wwstring.cpp
)
target_include_directories(d3d11_sentence_test PRIVATE
    ${PROJECT_SOURCE_DIR}/GeneralsMD/Code/Libraries/Source/WWVegas
)
target_compile_definitions(d3d11_sentence_test PRIVATE DISABLE_GAMEMEMORY)
target_link_libraries(d3d11_sentence_test PRIVATE w3d_d3d11 GTest::gtest_main user32 gdi32)
add_test(NAME d3d11_sentence_test COMMAND d3d11_sentence_test)

add_executable(d3d11_ui_test
    ../Core/Libraries/D3D11UITextureTest.cpp
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/UITextureContent.cpp
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/Texture2DCPUContent.cpp
    ${PROJECT_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/Render2DSubmission.cpp
)
target_include_directories(d3d11_ui_test PRIVATE ${PROJECT_SOURCE_DIR}/GeneralsMD/Code/Libraries/Source/WWVegas)
target_compile_definitions(d3d11_ui_test PRIVATE DISABLE_GAMEMEMORY
    D3D11_UI_ASSET="${PROJECT_SOURCE_DIR}/Core/Tools/W3DView/Res/grab.tga")
target_link_libraries(d3d11_ui_test PRIVATE w3d_d3d11 GTest::gtest_main)
add_test(NAME d3d11_ui_test COMMAND d3d11_ui_test)
