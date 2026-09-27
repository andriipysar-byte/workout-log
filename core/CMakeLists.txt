# The domain core (ADR-004): no Qt, no filesystem, no streams. Its only
# dependency is nlohmann/json; the `purity` test below turns that into a failure.
add_library(wlcore STATIC
    src/analytics.cpp
    src/catalogue.cpp
    src/cycle.cpp
    src/cycle_planning.cpp
    src/dart_parse.cpp
    src/date.cpp
    src/json.cpp
    src/models.cpp
    src/muscles.cpp
    src/notation.cpp
    src/prescription.cpp
    src/session_store.cpp
    src/utf8.cpp
    src/validation.cpp
    src/wall_clock.cpp
)
target_include_directories(wlcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(wlcore PUBLIC nlohmann_json::nlohmann_json PRIVATE wl_warnings)
target_compile_features(wlcore PUBLIC cxx_std_20)

add_test(NAME purity
    COMMAND ${CMAKE_COMMAND} -DDIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/purity.cmake)

file(GLOB WL_CORE_TESTS CONFIGURE_DEPENDS tests/*_test.cpp)
add_executable(wlcore_tests tests/main.cpp ${WL_CORE_TESTS})
target_link_libraries(wlcore_tests PRIVATE wlcore doctest::doctest wl_warnings)
target_compile_definitions(wlcore_tests PRIVATE WL_REPO_ROOT="${PROJECT_SOURCE_DIR}")
add_test(NAME wlcore COMMAND wlcore_tests)
