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

    FetchContent_Declare(
        lunasvg
        GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
        GIT_TAG v3.0.1
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
endif()
