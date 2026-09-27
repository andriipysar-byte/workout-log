set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Everything but main(), so the tests drive the same model and widgets the app runs.
add_library(wlapp STATIC
    src/app_model.cpp
    src/block_cards.cpp
    src/cycle_view.cpp
    src/exercise_picker.cpp
    src/main_window.cpp
    src/muscle_map_widget.cpp
    src/new_session_dialog.cpp
    src/plan_dialogs.cpp
    src/plan_view.cpp
    src/run_view.cpp
    src/session_editor.cpp
    src/session_folder.cpp
    src/theme.cpp
    src/widgets.cpp
    resources/resources.qrc
)
target_include_directories(wlapp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(wlapp PUBLIC wlcore wlfs Qt6::Widgets Qt6::Svg PRIVATE wl_warnings)

qt_add_executable(WorkoutLog MACOSX_BUNDLE src/main.cpp)
target_link_libraries(WorkoutLog PRIVATE wlapp wl_warnings)
set_target_properties(WorkoutLog PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME WorkoutLog
    MACOSX_BUNDLE_GUI_IDENTIFIER com.workoutlog.app)

qt_add_executable(wlapp_tests tests/app_model_test.cpp tests/run_test.cpp tests/widgets_test.cpp tests/main.cpp)
target_link_libraries(wlapp_tests PRIVATE wlapp Qt6::Test doctest::doctest wl_warnings)
target_compile_definitions(wlapp_tests PRIVATE WL_REPO_ROOT="${PROJECT_SOURCE_DIR}")
add_test(NAME wlapp COMMAND wlapp_tests)
set_tests_properties(wlapp PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
