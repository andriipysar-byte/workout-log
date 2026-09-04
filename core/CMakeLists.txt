# The portable domain core (models, JSON codec, notation parser, catalogue,
# analytics). Must stay dependency-free apart from nlohmann::json -- SDL3, ImGui and
# lunasvg must never appear below this line (see the top-level CMakeLists.txt).
add_library(workoutlog STATIC
    src/models.cpp
    src/utf8.cpp
    src/json.cpp
    src/catalogue.cpp
    src/notation.cpp
    src/muscle_group.cpp
    src/muscle_activation.cpp
    src/muscle_map_svg.cpp
    src/paths.cpp
    src/store.cpp
    src/calendar.cpp
    src/cycle.cpp
)

target_include_directories(workoutlog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(workoutlog PUBLIC nlohmann_json::nlohmann_json PRIVATE wl_warnings)
target_compile_features(workoutlog PUBLIC cxx_std_20)
