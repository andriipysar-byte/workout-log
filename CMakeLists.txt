cmake_minimum_required(VERSION 3.24)
project(WorkoutLog2 VERSION 0.1.0 LANGUAGES CXX)

# wl_core must also compile under Apple clang for the macOS SwiftUI app, so it stays
# inside a conservative C++20 subset: no <format>, no <expected>, no <chrono> calendar
# types, no std::regex, no unguarded floating-point std::from_chars. libc++ ships all of
# those late or not at all, and none of them are needed here.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Hard rule: workoutlog / wl-verify / wl-map / wl-fmt must never depend on SDL3, ImGui
# or lunasvg, so the verifier stays buildable on a headless box even when the UI
# dependencies can't be fetched or built. Gated separately behind WL_BUILD_UI.
option(WL_BUILD_UI "Build the SDL3 + Dear ImGui desktop app" OFF)
option(WL_BUILD_FFI "Build the C ABI shared library for Swift" OFF)

include(cmake/Warnings.cmake)
include("misc/cmake/Sanitize.cmake")
include("misc/cmake/Tidy.cmake")
include(cmake/Dependencies.cmake)

enable_testing()

# Sanitizer flags land here (project scope), after fetched deps but before any
# first-party target, so SDL3/ImGui/lunasvg/nlohmann_json stay uninstrumented
# while everything below is covered. See misc/cmake/Sanitize.cmake.
wl_sanitize_after_deps()

add_subdirectory(core)
add_subdirectory(tools)

if(WL_BUILD_FFI)
    add_subdirectory(capi)
endif()

if(WL_BUILD_UI)
    add_subdirectory(ui)
endif()
