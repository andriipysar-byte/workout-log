include(FetchContent)

# Prefer a system install when present; fetch and build from source otherwise. Pinned
# to explicit tags, never branches, so a build is reproducible.
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS NAMES nlohmann_json
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

if(WL_BUILD_UI)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.16
        GIT_SHALLOW TRUE
        FIND_PACKAGE_ARGS NAMES SDL3
    )
    set(SDL_SHARED OFF CACHE INTERNAL "")
    set(SDL_STATIC ON CACHE INTERNAL "")
    set(SDL_TEST_LIBRARY OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(SDL3)

    # v3.0.1's own CMakeLists fetched plutovg from GIT_TAG "main" -- an unpinned branch
    # that defeats "pinned to explicit tags" above. From v3.1.0 on, lunasvg vendors
    # plutovg as plain tracked files inside its own repo tree (not even a submodule),
    # so pinning lunasvg alone is enough for a reproducible build again.
    set(LUNASVG_BUILD_EXAMPLES OFF CACHE INTERNAL "")
    set(PLUTOVG_BUILD_EXAMPLES OFF CACHE INTERNAL "")
    FetchContent_Declare(
        lunasvg
        GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
        GIT_TAG v3.5.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(lunasvg)

    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.6
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)

    # ImGui ships no CMakeLists of its own; the target is declared here, not in
    # ui/CMakeLists.txt, so it lands before wl_sanitize_after_deps() (called by the
    # root CMakeLists.txt right after this file). That macro adds sanitizer flags at
    # *directory* scope for everything added afterwards, so a target created inside
    # ui/ would get instrumented; declaring it here keeps ImGui (and its bundled
    # imgui_stdlib.cpp) out of the ASan/TSan builds, matching the "project" scope
    # documented in misc/cmake/Sanitize.cmake. (This is not about clang-tidy: that
    # target's file glob only ever looks under core/tools/capi/ui, so ImGui sources
    # under _deps/ are excluded regardless of which CMakeLists declares the target.)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imgui_SOURCE_DIR}/misc/cpp
    )
    target_link_libraries(imgui PUBLIC SDL3::SDL3)
endif()
