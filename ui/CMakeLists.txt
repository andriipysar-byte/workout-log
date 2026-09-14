# Every screen/model source except main.cpp, as a static library so wl_ui and
# ui/tests/wl_ui_snapshot (the offscreen snapshot harness, issue #21) share one
# compiled copy instead of each executable's add_executable() re-listing the same
# files -- the shape core/ + tools/ already use.
add_library(wl_ui_core STATIC
    platform.cpp
    theme.cpp
    fonts.cpp
    widgets.cpp
    app_model.cpp
    svg_texture.cpp
    calendar_view.cpp
    cycle_view.cpp
    session_editor.cpp
    root_view.cpp
)
target_link_libraries(wl_ui_core PUBLIC workoutlog imgui lunasvg::lunasvg PRIVATE wl_warnings)
target_include_directories(wl_ui_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(wl_ui main.cpp)
target_link_libraries(wl_ui PRIVATE wl_ui_core wl_warnings)

add_subdirectory(tests)
