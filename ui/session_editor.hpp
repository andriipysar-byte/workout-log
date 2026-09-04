#pragma once

#include <imgui.h>

#include <filesystem>
#include <map>

#include "app_model.hpp"
#include "svg_texture.hpp"
#include "widgets.hpp"

struct SDL_Renderer;

// The session editor, ported from SessionEditorView/BlockCard/*Editor in
// Views.swift: the header grid, one card per block, the notation input, and the
// day muscle map.
namespace workoutlog::ui::session_editor {

// Per-block UI state that has no home in the domain model -- SwiftUI's @State
// lives on view identity; immediate mode has to own it explicitly. Holds fields
// for every block kind rather than a per-kind variant of states: simpler than the
// alternative, and the unused members for a block of a different kind cost
// nothing but a few bytes.
struct BlockUiState {
    std::string notation_text;       // StrengthBlock's notation entry
    SvgTexture exercise_map;         // StrengthBlock's per-exercise map (built only
                                      // while its CollapsingHeader is open -- ImGui
                                      // already tracks that open/closed state itself,
                                      // so there's no separate bool to keep in sync)
    widgets::NumberFieldState cardio_duration_field;
    widgets::NumberFieldState cardio_distance_field;
};

struct State {
    std::map<std::size_t, BlockUiState> blocks;
    widgets::NumberFieldState bodyweight_field;
    SvgTexture day_map;
    // Detects a session switch so `blocks` can be cleared -- otherwise text typed
    // into session A's block 2 would reappear in session B's block 2.
    std::filesystem::path last_selection;

    void forget_blocks() { blocks.clear(); }
};

// Draws a Save button (+ the Ctrl/Cmd+S shortcut), then the header grid, one card
// per block, and the day muscle map inside a scrolling region. Precondition:
// model.has_session() -- the caller (root_view) only shows this screen once a
// session is loaded, matching `model.session != nil` in Views.swift.
void draw(SDL_Renderer& renderer, AppModel& model, State& state, ImFont* mono_font);

} // namespace workoutlog::ui::session_editor
