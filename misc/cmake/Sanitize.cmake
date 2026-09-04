# GCC/Clang sanitizer builds. Adapted from ../remoteguard/CERAMIC/misc/cmake/Sanitize.cmake.
#
# Three opt-in build configurations that instrument the build with a sanitizer.
# ASan, UBSan, and TSan (and LSan, which ASan bundles) build under either GCC or
# Clang. All are otherwise inert. Every sanitizer build is compiled -O1 -g
# -fno-omit-frame-pointer (see below).
#
#   -DWL_SANITIZE_UBSAN=ON   -fsanitize=undefined -fsanitize-trap=undefined
#   -DWL_SANITIZE_ASAN=ON    -fsanitize=address
#   -DWL_SANITIZE_TSAN=ON    -fsanitize=thread
#
# UBSan here traps on undefined behaviour (SIGILL) instead of pulling in the
# runtime diagnostic library, so no extra link dependency is needed, and it is
# applied to the whole tree (fetched deps included). -fsanitize-trap=undefined
# traps only the UBSan checks, so it composes with ASan's runtime in the
# asan-ubsan profile (WL_SANITIZE_ASAN + WL_SANITIZE_UBSAN together) without
# trying to trap -- and break -- the address runtime.
#
# ASan and TSan each need their runtime, which the same flag passed at link time
# provides. They are mutually exclusive -- at most one can instrument a build --
# so enabling both fails configuration; use separate build trees (see
# CMakePresets.json: gcc-asan-ubsan / gcc-tsan / clang-asan-ubsan / clang-tsan).
#
# ASan finds heap/stack/use-after-free; it also bundles LeakSanitizer, which runs
# at process exit (detect_leaks=1, the default on Linux/x86-64) and reports memory
# still reachable nowhere at teardown.
#
# TSan finds data races and other threading errors -- relevant once the SDL3/ImGui
# UI's event loop and any background loading exist.
#
# ASan and TSan each have two scopes, selected by WL_SANITIZE_ASAN_SCOPE /
# WL_SANITIZE_TSAN_SCOPE:
#
#   project   (default) instrument first-party code only. The flags are applied
#             *after* cmake/Dependencies.cmake's FetchContent_MakeAvailable calls
#             via wl_sanitize_after_deps(), so SDL3/ImGui/lunasvg/nlohmann_json get
#             neither the compile nor the link flag. This is what you want in
#             practice: it keeps the report focused on our own bugs, builds
#             faster, and avoids linking the sanitizer runtime into a dependency
#             shared library that rejects undefined symbols at link time.
#   all       instrument the whole build, deps included. Maximum coverage, but
#             may fail to link a dependency that rejects undefined symbols in a
#             shared library -- use it only when every dependency tolerates the
#             sanitizer runtime.
#
# Because the project scope must inject flags between the deps and the
# first-party targets, the root CMakeLists.txt calls wl_sanitize_after_deps()
# right after include(cmake/Dependencies.cmake) and before add_subdirectory(core).
# UBSan and ASan/TSan scope=all are applied here (globally), before that point,
# so they reach every target including fetched deps.

option(WL_SANITIZE_UBSAN "Build with UBSan (-fsanitize=undefined -fsanitize-trap=undefined; GCC or Clang)" OFF)
option(WL_SANITIZE_ASAN  "Build with ASan (-fsanitize=address; GCC or Clang)" OFF)
option(WL_SANITIZE_TSAN  "Build with TSan (-fsanitize=thread; GCC or Clang)" OFF)
set(WL_SANITIZE_ASAN_SCOPE "project" CACHE STRING
    "ASan instrumentation scope: 'project' (first-party only, deps excluded) or 'all' (whole build)")
set_property(CACHE WL_SANITIZE_ASAN_SCOPE PROPERTY STRINGS project all)
set(WL_SANITIZE_TSAN_SCOPE "project" CACHE STRING
    "TSan instrumentation scope: 'project' (first-party only, deps excluded) or 'all' (whole build)")
set_property(CACHE WL_SANITIZE_TSAN_SCOPE PROPERTY STRINGS project all)

# Apply ASan/TSan to the first-party targets only. Called from the root
# CMakeLists.txt after cmake/Dependencies.cmake, so the directory-scoped flags
# are inherited by the first-party subdirectories that follow but not by the
# already-fetched deps. Defined unconditionally (and a no-op outside
# project-scope ASan/TSan) so the caller can invoke it without guarding on the
# options.
macro(wl_sanitize_after_deps)
    if(WL_SANITIZE_ASAN AND WL_SANITIZE_ASAN_SCOPE STREQUAL "project")
        message(STATUS "WorkoutLog2: ASan enabled (-fsanitize=address), scope=project -- deps excluded")
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()
    if(WL_SANITIZE_TSAN AND WL_SANITIZE_TSAN_SCOPE STREQUAL "project")
        message(STATUS "WorkoutLog2: TSan enabled (-fsanitize=thread), scope=project -- deps excluded")
        add_compile_options(-fsanitize=thread)
        add_link_options(-fsanitize=thread)
    endif()
endmacro()

if(NOT (WL_SANITIZE_UBSAN OR WL_SANITIZE_ASAN OR WL_SANITIZE_TSAN))
    return()
endif()

if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    message(FATAL_ERROR
        "WL_SANITIZE_UBSAN/ASAN/TSAN require GCC or Clang; "
        "current CXX compiler is '${CMAKE_CXX_COMPILER_ID}'.")
endif()

# Every sanitizer build: light optimization so instrumented runs stay usable while
# still exposing bugs -O0 can mask, plus frame pointers and debug info for
# readable, symbolized reports. Emitted as directory compile options, which land
# after CMAKE_CXX_FLAGS_<CONFIG> on the command line, so this -O1 wins over the
# -O0 a Debug build would otherwise set (last flag wins).
add_compile_options(-O1 -g -fno-omit-frame-pointer)

if(WL_SANITIZE_ASAN AND WL_SANITIZE_TSAN)
    message(FATAL_ERROR
        "WL_SANITIZE_ASAN and WL_SANITIZE_TSAN are mutually exclusive (each ships its own "
        "incompatible runtime). Enable one at a time in separate build trees.")
endif()

if(WL_SANITIZE_ASAN AND NOT WL_SANITIZE_ASAN_SCOPE MATCHES "^(project|all)$")
    message(FATAL_ERROR "WL_SANITIZE_ASAN_SCOPE must be 'project' or 'all', got '${WL_SANITIZE_ASAN_SCOPE}'.")
endif()

if(WL_SANITIZE_TSAN AND NOT WL_SANITIZE_TSAN_SCOPE MATCHES "^(project|all)$")
    message(FATAL_ERROR "WL_SANITIZE_TSAN_SCOPE must be 'project' or 'all', got '${WL_SANITIZE_TSAN_SCOPE}'.")
endif()

if(WL_SANITIZE_UBSAN)
    message(STATUS "WorkoutLog2: UBSan enabled (-fsanitize=undefined -fsanitize-trap=undefined)")
    add_compile_options(-fsanitize=undefined -fsanitize-trap=undefined)
    add_link_options(-fsanitize=undefined -fsanitize-trap=undefined)
endif()

# scope=project is deferred to wl_sanitize_after_deps(); scope=all is global.
if(WL_SANITIZE_ASAN AND WL_SANITIZE_ASAN_SCOPE STREQUAL "all")
    message(STATUS "WorkoutLog2: ASan enabled (-fsanitize=address), scope=all -- deps included")
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

if(WL_SANITIZE_TSAN AND WL_SANITIZE_TSAN_SCOPE STREQUAL "all")
    message(STATUS "WorkoutLog2: TSan enabled (-fsanitize=thread), scope=all -- deps included")
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

# TSan maps its shadow memory at fixed addresses and aborts with "unexpected memory
# mapping" when a high vm.mmap_rnd_bits lands the binary there; `setarch -R`
# disables ASLR for the child without privileges. Probed, not assumed: a container
# whose seccomp profile denies personality(2) cannot run it at all.
# tools/CMakeLists.txt applies the result to the `wl_verify` ctest test only.
if(WL_SANITIZE_TSAN)
    find_program(WL_SETARCH_EXECUTABLE setarch)
    if(WL_SETARCH_EXECUTABLE)
        execute_process(
            COMMAND "${WL_SETARCH_EXECUTABLE}" -R "${CMAKE_COMMAND}" -E true
            RESULT_VARIABLE _wl_setarch_result
            OUTPUT_QUIET ERROR_QUIET)
        if(_wl_setarch_result EQUAL 0)
            set(WL_TSAN_TEST_LAUNCHER "${WL_SETARCH_EXECUTABLE}" -R)
            message(STATUS "WorkoutLog2: TSan tests run under `setarch -R` (ASLR disabled)")
        endif()
    endif()
    if(NOT WL_TSAN_TEST_LAUNCHER)
        message(STATUS "WorkoutLog2: `setarch -R` unusable here -- TSan tests run with ASLR unchanged")
    endif()
endif()
