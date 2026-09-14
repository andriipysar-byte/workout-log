#pragma once

#include <imgui.h>

#include <map>
#include <optional>
#include <string>

#include "app_model.hpp"
#include "widgets.hpp"
#include "workoutlog/cycle_plan.hpp"

// The cycle-plan editor: a screen over cycles.json (workoutlog::cycle_plan), not
// over data/*.json -- distinct from cycle_view, which is a read-only coverage
// matrix derived from *logged* sessions. There is no SwiftUI counterpart yet
// (issue #19); this is the first implementation, tracked as an intentional
// ImGui-only row in docs/ui-parity/MATRIX.md until #19 lands.
//
// The core CRUD (#16) only covers sessions and blocks within an existing cycle --
// nothing adds or removes a *cycle* -- so neither does this screen; picking which
// cycle to edit is the only place `file.cycles` itself isn't mutated here.
namespace workoutlog::ui::cycle_editor {

// Per-block scratch state that has no home in the domain model, mirroring
// session_editor::BlockUiState. sets_reps/scheme are edited as a single
// comma-separated text field ("6,6,6,6,6") rather than a per-int add/remove list --
// they're planning shorthand, not data anyone steps through one entry at a time --
// rebuilt from the model each frame like widgets::optional_text_field, so they need
// no state of their own here.
struct BlockUiState {
    widgets::NumberFieldState duration_field;
    // Keyed by index within Block::exercises, mirroring session_editor::State's
    // per-block map one level deeper -- each metcon-exercise row's weight_kg needs
    // its own persistent tint state (widgets.hpp's one-frame-lag design), not a
    // fresh one every frame.
    std::map<std::size_t, widgets::NumberFieldState> exercise_weight_fields;
};

struct SessionUiState {
    std::map<std::size_t, BlockUiState> blocks;
};

struct State {
    bool attempted_load = false;
    std::optional<std::string> load_error;
    cycle_plan::File file;
    std::string status;

    std::optional<std::string> selected_cycle_id;
    std::optional<std::size_t> selected_session_index;
    std::optional<std::size_t> selected_block_index;

    std::map<std::size_t, SessionUiState> sessions;

    void forget_session_ui() { sessions.clear(); }
};

// Loads cycles.json on first call (or after a failed load, on the next Reload
// click), then draws the cycle/session/block pickers and field editors. All
// mutation goes through workoutlog::cycle_plan's CRUD + CyclePlanStore::save(), so
// an edit that would leave the file schema-invalid is rejected before anything is
// written (surfaced in state.status, never silently dropped).
void draw(AppModel& model, State& state);

} // namespace workoutlog::ui::cycle_editor
