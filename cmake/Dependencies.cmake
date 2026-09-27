include(FetchContent)

# Prefer a system install when present; fetch otherwise. Pinned to tags, never branches.
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS NAMES nlohmann_json
)
set(JSON_BuildTests OFF CACHE INTERNAL "")

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.12
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS NAMES doctest
)
set(DOCTEST_WITH_TESTS OFF CACHE INTERNAL "")
set(DOCTEST_NO_INSTALL ON CACHE INTERNAL "")

FetchContent_MakeAvailable(nlohmann_json doctest)

if(WL_BUILD_APP)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg Test)
endif()
