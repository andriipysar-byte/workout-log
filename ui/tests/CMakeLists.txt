# See tools/CMakeLists.txt for why this has to happen before add_executable(), not
# after: CROSSCOMPILING_EMULATOR is only picked up from
# CMAKE_CROSSCOMPILING_EMULATOR at the moment a target is created.
if(WL_SANITIZE_TSAN AND WL_TSAN_TEST_LAUNCHER)
    list(PREPEND CMAKE_CROSSCOMPILING_EMULATOR ${WL_TSAN_TEST_LAUNCHER})
endif()

add_executable(wl_ui_snapshot main.cpp)
target_link_libraries(wl_ui_snapshot PRIVATE wl_ui_core wl_warnings)

# Needs the real repo (cycles.json, data/*.json, exercises.json) to render against,
# same as wl_verify.
add_test(NAME wl_ui_snapshot COMMAND wl_ui_snapshot WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Platform's SDL_CreateRenderer(), on the "offscreen" video driver's OpenGL/EGL
# path, indirectly reaches dbus_bus_register() in the system libdbus -- which leaks
# under LeakSanitizer regardless of anything this project does (reproduced with a
# plain backtrace: SDL_EGL_InitializeOffscreen -> ... -> libdbus-1.so, no frame in
# this repo's code). ASAN_OPTIONS is inert outside an ASan build, so this is safe to
# set unconditionally rather than only under the gcc-asan-ubsan preset.
# ENVIRONMENT replaces, rather than merges with, the ASAN_OPTIONS the test-asan-ubsan
# preset sets project-wide, so the full baseline has to be restated here rather than
# just the leak-detection override.
set_tests_properties(wl_ui_snapshot PROPERTIES ENVIRONMENT
    "ASAN_OPTIONS=detect_container_overflow=0:detect_leaks=0:halt_on_error=1")
