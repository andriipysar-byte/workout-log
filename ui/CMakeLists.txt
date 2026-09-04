add_executable(wl_ui
    main.cpp
    platform.cpp
    theme.cpp
    fonts.cpp
    widgets.cpp
    app_model.cpp
    svg_texture.cpp
    calendar_view.cpp
    cycle_view.cpp
    session_editor.cpp
)
target_link_libraries(wl_ui PRIVATE workoutlog imgui lunasvg::lunasvg wl_warnings)
