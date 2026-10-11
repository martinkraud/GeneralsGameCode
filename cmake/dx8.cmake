# The upstream CMake target combines declarations with I386 binary libraries.
# Keep that target unchanged for Win32. On Win64, populate only the same pinned
# headers; the intentionally absent SOURCE_SUBDIR prevents executing its link setup.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(dx8_source_subdir SOURCE_SUBDIR headers_only_no_cmake)
else()
    set(dx8_source_subdir)
endif()

FetchContent_Declare(
    dx8
    GIT_REPOSITORY https://github.com/TheSuperHackers/min-dx8-sdk.git
    GIT_TAG        7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc
    ${dx8_source_subdir}
)

FetchContent_MakeAvailable(dx8)

# Declarations and inline math only: no library directories, link options or libs.
add_library(dx8_headers INTERFACE)
target_include_directories(dx8_headers INTERFACE ${dx8_SOURCE_DIR})

add_library(legacy_dx8_dependencies INTERFACE)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    # Preserve the upstream Win32 link/include/define contract exactly.
    target_link_libraries(legacy_dx8_dependencies INTERFACE d3d8lib)
else()
    # Input/GUID dependencies are unrelated to the renderer. Resolve their
    # native Windows SDK libraries, without the bundled I386 search directory.
    target_link_libraries(legacy_dx8_dependencies INTERFACE dx8_headers dinput8 dxguid)
    # Compile the real legacy backend, never silently omit it. This does not
    # provide a native D3D8/D3DX implementation or make a Win64 game linkable.
    target_compile_definitions(legacy_dx8_dependencies INTERFACE BUILD_WITH_D3D8)
endif()
unset(dx8_source_subdir)
