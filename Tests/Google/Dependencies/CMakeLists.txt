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
