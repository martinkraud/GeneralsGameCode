# g_googletest and z_googletest compile these sources together with the Core tests.
set(DEPSI_GOOGLETEST_SRC
    Utility/stringex_test.cpp
)

add_library(depsi_googletest INTERFACE)

target_sources(depsi_googletest INTERFACE ${DEPSI_GOOGLETEST_SRC})

target_link_libraries(depsi_googletest INTERFACE
    deps_utility
)

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
