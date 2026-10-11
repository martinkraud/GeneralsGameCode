# g_googletest and z_googletest compile these sources together with the Core tests.
set(DEPSI_GOOGLETEST_SRC
    Utility/stringex_test.cpp
)

add_library(depsi_googletest INTERFACE)

target_sources(depsi_googletest INTERFACE ${DEPSI_GOOGLETEST_SRC})

target_link_libraries(depsi_googletest INTERFACE
    deps_utility
)

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
