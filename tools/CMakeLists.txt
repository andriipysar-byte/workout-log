# CROSSCOMPILING_EMULATOR is only initialized from CMAKE_CROSSCOMPILING_EMULATOR at
# the moment a target is CREATED (see `cmake --help-property CROSSCOMPILING_EMULATOR`);
# setting it afterwards via set_tests_properties() is silently inert. So this has to
# happen before add_executable(wl_verify ...), not after.
if(WL_SANITIZE_TSAN AND WL_TSAN_TEST_LAUNCHER)
    list(PREPEND CMAKE_CROSSCOMPILING_EMULATOR ${WL_TSAN_TEST_LAUNCHER})
endif()

add_executable(wl_verify wl_verify/main.cpp)
target_link_libraries(wl_verify PRIVATE workoutlog wl_warnings)

add_executable(wl_map wl_map/main.cpp)
target_link_libraries(wl_map PRIVATE workoutlog wl_warnings)

add_executable(wl_fmt wl_fmt/main.cpp)
target_link_libraries(wl_fmt PRIVATE workoutlog wl_warnings)

# The acceptance gate (see docs/05-architecture.md) as a ctest test, so the
# CMakePresets.json sanitizer profiles (`ctest --preset gcc-asan-ubsan`, etc.) have
# something to run it against. `COMMAND wl_verify` (a target name, not a path) is
# what makes CMake apply wl_verify's CROSSCOMPILING_EMULATOR automatically.
add_test(NAME wl_verify COMMAND wl_verify WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
