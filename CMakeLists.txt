cmake_minimum_required(VERSION 3.24)
project(WorkoutLog VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# The core, tools and MCP server must build on a box without Qt (CI, a headless
# server), so the app is the only thing behind this switch.
option(WL_BUILD_APP "Build the Qt Widgets app" ON)
set(WL_SANITIZE "" CACHE STRING "Sanitizers for first-party targets: \"address\" (ASan + UBSan) or \"thread\" (TSan)")

include(cmake/Warnings.cmake)
include(cmake/Dependencies.cmake)

enable_testing()

# Added after the fetched dependencies so only first-party code is instrumented.
if(WL_SANITIZE STREQUAL "address")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
elseif(WL_SANITIZE STREQUAL "thread")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
elseif(NOT WL_SANITIZE STREQUAL "")
    message(FATAL_ERROR "WL_SANITIZE must be empty, \"address\" or \"thread\", not \"${WL_SANITIZE}\"")
endif()

add_subdirectory(core)
add_subdirectory(storage)
add_subdirectory(tools)

if(WL_BUILD_APP)
    add_subdirectory(app)
endif()
