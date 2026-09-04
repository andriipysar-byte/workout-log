#pragma once

#include <imgui.h>

#include <optional>
#include <string>

#include "app_model.hpp"
#include "cycle_view.hpp"
#include "platform.hpp"
#include "session_editor.hpp"

// The application shell, ported from RootView in Views.swift: the sidebar
// (List/Cycle switch, file list, folder + reload), the detail pane dispatch, and
// the status bar.
namespace workoutlog::ui::root_view {

enum class Tab { list, cycle };

struct State {
    Tab tab = Tab::list;
    cycle_view::State cycle;
    session_editor::State editor;

    // In-app fallback for the folder picker (Platform::request_folder_dialog can
    // fail with no XDG portal or zenity available) and the dialog's own status.
    std::string folder_path_input;
    bool folder_dialog_pending = false;
    std::optional<std::string> folder_dialog_error;
};

// Draws the whole application for one frame: the full-viewport host window,
// sidebar, detail pane and status bar. mono_font is threaded through to
// session_editor (the notation input and set-summary line).
void draw(Platform& platform, AppModel& model, State& state, ImFont* mono_font);

} // namespace workoutlog::ui::root_view
