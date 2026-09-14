#include "cycle_editor.hpp"

#include <imgui_stdlib.h>

#include <array>
#include <charconv>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>

#include "theme.hpp"
#include "workoutlog/notation.hpp"
#include "workoutlog/paths.hpp"

// Deliberately qualifies every workoutlog::cycle_plan:: type in full throughout
// this file rather than `using`-aliasing Session/Block/Cycle: app_model.hpp pulls
// in workoutlog::Session/Block too (the *logged*-session models), and this screen
// never touches those, so an unqualified "Session" here would be ambiguous to a
// reader even where it isn't to the compiler.
namespace workoutlog::ui::cycle_editor {

namespace {

void load(AppModel& model, State& state) {
    state.attempted_load = true;
    try {
        cycle_plan::CyclePlanStore store(paths::cycles_path(model.repo_root()));
        cycle_plan::File file = store.load();
        // Reject the same violations save() would (duplicate cycle id, bad
        // cycle_day, ...) up front: find_cycle() resolves by id, so editing a
        // file with a duplicate id would otherwise silently target the wrong
        // cycle instead of surfacing the problem.
        auto violations = cycle_plan::validate(file);
        if (!violations.empty()) {
            std::string msg = store.path().string() + " is invalid:\n";
            for (const auto& v : violations) msg += "  - " + v + "\n";
            throw std::runtime_error(msg);
        }
        state.file = std::move(file);
        state.load_error.reset();
    } catch (const std::exception& e) {
        state.load_error = e.what();
    }
}

void reset_selection(State& state) {
    state.selected_cycle_id.reset();
    state.selected_session_index.reset();
    state.selected_block_index.reset();
    state.forget_session_ui();
}

void save(AppModel& model, State& state) {
    try {
        cycle_plan::CyclePlanStore store(paths::cycles_path(model.repo_root()));
        store.save(state.file);
        state.status = "saved " + store.path().string();
    } catch (const std::exception& e) {
        state.status = std::string("error: ") + e.what();
    }
}

// "6,6,6,6,6" <-> vector<int>, the planning-shorthand text form cycle_plan::Block
// uses for sets_reps/scheme. Same commit-on-deactivate discipline as
// widgets::optional_number_field: the model is untouched while typing, and a token
// that doesn't parse is dropped rather than aborting the whole field or coercing it
// (AGENTS.md 1.2.2) -- there's no partial-list "invalid" state to reject into.
std::optional<std::vector<int>> parse_int_list(const std::string& text) {
    std::vector<int> out;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t comma = text.find(',', pos);
        const std::size_t len = comma == std::string::npos ? std::string::npos : comma - pos;
        std::string token = text.substr(pos, len);
        const std::size_t start = token.find_first_not_of(" \t");
        if (start != std::string::npos) {
            const std::size_t end = token.find_last_not_of(" \t");
            if (auto v = notation::parse_int_strict(token.substr(start, end - start + 1))) out.push_back(*v);
        }
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return out.empty() ? std::nullopt : std::optional<std::vector<int>>(std::move(out));
}

std::string int_list_to_text(const std::vector<int>& v) {
    std::string out;
    for (std::size_t i = 0; i < v.size(); i++) {
        if (i > 0) out += ",";
        out += std::to_string(v[i]);
    }
    return out;
}

void optional_int_list_field(const char* label, std::optional<std::vector<int>>& value) {
    std::string text = value.has_value() ? int_list_to_text(*value) : std::string();
    ImGui::InputText(label, &text);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (text.find_first_not_of(" \t") == std::string::npos) {
            value = std::nullopt;
        } else if (auto parsed = parse_int_list(text)) {
            value = std::move(parsed);
        }
        // else: unparseable on deactivation, discarded back to the last committed
        // value, never coerced (AGENTS.md 1.2.2) -- same discipline as
        // optional_int_field below.
    }
}

void optional_int_field(const char* label, std::optional<int>& value) {
    std::string text = value.has_value() ? std::to_string(*value) : std::string();
    ImGui::InputText(label, &text);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (text.empty()) {
            value = std::nullopt;
        } else if (auto v = notation::parse_int_strict(text)) {
            value = v;
        }
        // else: unparseable on deactivation, discarded back to the last committed
        // value, never coerced (AGENTS.md 1.2.2) -- next frame's text rebuilds from
        // `value` and simply reverts what's on screen.
    }
}

// Every enum this screen edits has a fixed, listed set of values and a
// to_string(E); one generic combo covers all of them rather than one hand-rolled
// BeginCombo per enum (BlockType, BlockRole, SessionType, and -- via the optional_
// variant below -- Weekday, MetconFormat).
template <typename E>
bool enum_combo(const char* label, E& value, std::span<const E> options, std::string (*to_str)(E)) {
    bool changed = false;
    const std::string current = to_str(value);
    if (ImGui::BeginCombo(label, current.c_str())) {
        for (E option : options) {
            const bool selected = option == value;
            if (ImGui::Selectable(to_str(option).c_str(), selected)) {
                value = option;
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

template <typename E>
bool optional_enum_combo(const char* label, std::optional<E>& value, std::span<const E> options,
                          std::string (*to_str)(E)) {
    bool changed = false;
    const std::string current = value.has_value() ? to_str(*value) : std::string("\xe2\x80\x94"); // em dash
    if (ImGui::BeginCombo(label, current.c_str())) {
        const bool none_selected = !value.has_value();
        if (ImGui::Selectable("\xe2\x80\x94", none_selected)) {
            value.reset();
            changed = true;
        }
        if (none_selected) ImGui::SetItemDefaultFocus();
        for (E option : options) {
            const bool selected = value.has_value() && *value == option;
            if (ImGui::Selectable(to_str(option).c_str(), selected)) {
                value = option;
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

constexpr std::array<cycle_plan::BlockType, 4> kBlockTypes = {
    cycle_plan::BlockType::cardio, cycle_plan::BlockType::strength, cycle_plan::BlockType::metcon,
    cycle_plan::BlockType::cooldown};
constexpr std::array<cycle_plan::BlockRole, 8> kBlockRoles = {
    cycle_plan::BlockRole::warmup,    cycle_plan::BlockRole::explosive, cycle_plan::BlockRole::main,
    cycle_plan::BlockRole::accessory, cycle_plan::BlockRole::grip,      cycle_plan::BlockRole::core,
    cycle_plan::BlockRole::metcon,    cycle_plan::BlockRole::cooldown};
constexpr std::array<cycle_plan::SessionType, 2> kSessionTypes = {cycle_plan::SessionType::metcon,
                                                                    cycle_plan::SessionType::heavy};
constexpr std::array<cycle_plan::Weekday, 7> kWeekdays = {
    cycle_plan::Weekday::mon, cycle_plan::Weekday::tue, cycle_plan::Weekday::wed, cycle_plan::Weekday::thu,
    cycle_plan::Weekday::fri, cycle_plan::Weekday::sat, cycle_plan::Weekday::sun};
constexpr std::array<MetconFormat, 6> kMetconFormats = {MetconFormat::for_time, MetconFormat::amrap,
                                                          MetconFormat::emom,    MetconFormat::intervals,
                                                          MetconFormat::ladder,  MetconFormat::chipper};

enum class ListAction : std::uint8_t { none, remove, move_up, move_down };

struct RowResult {
    bool select_clicked = false;
    ListAction action = ListAction::none;
};

// One row: a selectable label plus Up/Down/Remove buttons. The caller applies the
// action *after* walking the whole list (see draw_session_list/draw_block_list) --
// mutating the vector being iterated would invalidate every index the rest of the
// loop still needs.
RowResult draw_list_row(const char* label, bool selected, bool can_move_up, bool can_move_down) {
    RowResult result;
    if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_None, ImVec2(-96.0f, 0.0f)))
        result.select_clicked = true;
    ImGui::SameLine();
    ImGui::BeginDisabled(!can_move_up);
    if (ImGui::SmallButton("^")) result.action = ListAction::move_up;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!can_move_down);
    if (ImGui::SmallButton("v")) result.action = ListAction::move_down;
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::SmallButton("x")) result.action = ListAction::remove;
    return result;
}

void draw_cycle_picker(State& state) {
    ImGui::TextUnformatted("Cycles");
    ImGui::Separator();
    if (state.file.cycles.empty()) {
        ImGui::TextDisabled("No cycles in cycles.json.");
        return;
    }
    for (const auto& c : state.file.cycles) {
        ImGui::PushID(c.id.c_str());
        const bool selected = state.selected_cycle_id.has_value() && *state.selected_cycle_id == c.id;
        const std::string label = c.id + "  (" + std::to_string(c.sessions.size()) + ")";
        if (ImGui::Selectable(label.c_str(), selected)) {
            state.selected_cycle_id = c.id;
            state.selected_session_index.reset();
            state.selected_block_index.reset();
            state.forget_session_ui();
        }
        ImGui::PopID();
    }
    // No add/remove here: the core CRUD (workoutlog::cycle_plan, #16) covers
    // sessions and blocks within an existing cycle, not the top-level cycle list
    // itself -- so neither does this screen (see cycle_editor.hpp).
}

void draw_session_list(cycle_plan::Cycle& cycle, State& state) {
    ImGui::TextUnformatted("Sessions");
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Add")) {
        cycle_plan::Session blank;
        blank.cycle_day = "A1"; // placeholder; save() will flag it if it collides
        cycle_plan::insert_session(cycle, cycle.sessions.size(), std::move(blank));
        state.selected_session_index = cycle.sessions.size() - 1;
        state.selected_block_index.reset();
    }

    ImGui::BeginChild("session_list");
    std::optional<std::size_t> remove_index;
    std::optional<std::pair<std::size_t, std::size_t>> move;
    for (std::size_t i = 0; i < cycle.sessions.size(); i++) {
        ImGui::PushID(static_cast<int>(i));
        const cycle_plan::Session& s = cycle.sessions[i];
        std::string label = (s.cycle_day.empty() ? std::string("(no day)") : s.cycle_day) + "  " +
                             cycle_plan::to_string(s.type);
        if (s.title.has_value()) label += "  " + *s.title;
        const auto row = draw_list_row(label.c_str(), state.selected_session_index == i, i > 0,
                                        i + 1 < cycle.sessions.size());
        if (row.select_clicked) {
            state.selected_session_index = i;
            state.selected_block_index.reset();
        }
        if (row.action == ListAction::remove) remove_index = i;
        if (row.action == ListAction::move_up) move = std::pair{i, i - 1};
        if (row.action == ListAction::move_down) move = std::pair{i, i + 1};
        ImGui::PopID();
    }
    ImGui::EndChild();

    if (move.has_value()) {
        cycle_plan::move_session(cycle, move->first, move->second);
        if (state.selected_session_index == move->first)
            state.selected_session_index = move->second;
        else if (state.selected_session_index == move->second)
            state.selected_session_index = move->first;
    }
    if (remove_index.has_value()) {
        cycle_plan::remove_session(cycle, *remove_index);
        if (state.selected_session_index == remove_index) {
            state.selected_session_index.reset();
            state.selected_block_index.reset();
        } else if (state.selected_session_index.has_value() && *state.selected_session_index > *remove_index) {
            *state.selected_session_index -= 1;
        }
    }
}

void draw_session_fields(cycle_plan::Session& session) {
    ImGui::TextUnformatted("Session");
    const float w = theme::tokens::field_max_width;

    if (ImGui::BeginTable("session_fields", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        widgets::labeled_field_prefix("Cycle day");
        ImGui::SetNextItemWidth(w);
        ImGui::InputText("##cycle_day", &session.cycle_day);
        ImGui::TableSetColumnIndex(1);
        widgets::labeled_field_prefix("Type");
        ImGui::SetNextItemWidth(w);
        enum_combo<cycle_plan::SessionType>("##session_type", session.type,
                                             std::span<const cycle_plan::SessionType>(kSessionTypes),
                                             cycle_plan::to_string);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        widgets::labeled_field_prefix("Week");
        ImGui::SetNextItemWidth(w);
        optional_int_field("##week", session.week);
        ImGui::TableSetColumnIndex(1);
        widgets::labeled_field_prefix("Weekday");
        ImGui::SetNextItemWidth(w);
        optional_enum_combo<cycle_plan::Weekday>("##weekday", session.weekday,
                                                  std::span<const cycle_plan::Weekday>(kWeekdays),
                                                  cycle_plan::to_string);

        ImGui::EndTable();
    }

    widgets::labeled_field_prefix("Title");
    ImGui::SetNextItemWidth(-1.0f);
    widgets::optional_text_field("##title", session.title);
    widgets::labeled_field_prefix("Notes");
    ImGui::SetNextItemWidth(-1.0f);
    widgets::optional_text_field("##session_notes", session.session_notes);
}

void draw_block_list(cycle_plan::Session& session, State& state) {
    ImGui::TextUnformatted("Blocks");
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Add")) {
        cycle_plan::insert_block(session, session.blocks.size(), cycle_plan::Block{});
        state.selected_block_index = session.blocks.size() - 1;
    }

    ImGui::BeginChild("block_list");
    std::optional<std::size_t> remove_index;
    std::optional<std::pair<std::size_t, std::size_t>> move;
    for (std::size_t i = 0; i < session.blocks.size(); i++) {
        ImGui::PushID(static_cast<int>(i));
        const cycle_plan::Block& b = session.blocks[i];
        std::string label = cycle_plan::to_string(b.type) + " / " + cycle_plan::to_string(b.role);
        if (b.exercise.has_value() && !b.exercise->empty())
            label += "  " + *b.exercise;
        else if (b.machine.has_value() && !b.machine->empty())
            label += "  " + *b.machine;
        const auto row =
            draw_list_row(label.c_str(), state.selected_block_index == i, i > 0, i + 1 < session.blocks.size());
        if (row.select_clicked) state.selected_block_index = i;
        if (row.action == ListAction::remove) remove_index = i;
        if (row.action == ListAction::move_up) move = std::pair{i, i - 1};
        if (row.action == ListAction::move_down) move = std::pair{i, i + 1};
        ImGui::PopID();
    }
    ImGui::EndChild();

    // Reorder/remove can leave a block's BlockUiState (tint flags) attributed to
    // the wrong index for one frame; every tint is recomputed from that frame's
    // own text before it's read (widgets.hpp), so this self-corrects immediately
    // and isn't worth re-keying the map for.
    if (move.has_value()) {
        cycle_plan::move_block(session, move->first, move->second);
        if (state.selected_block_index == move->first)
            state.selected_block_index = move->second;
        else if (state.selected_block_index == move->second)
            state.selected_block_index = move->first;
    }
    if (remove_index.has_value()) {
        cycle_plan::remove_block(session, *remove_index);
        if (state.selected_block_index == remove_index) {
            state.selected_block_index.reset();
        } else if (state.selected_block_index.has_value() && *state.selected_block_index > *remove_index) {
            *state.selected_block_index -= 1;
        }
    }
}

void draw_block_exercises(cycle_plan::Block& block, BlockUiState& ui) {
    ImGui::TextUnformatted("Exercises");
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Add##exercise")) {
        if (!block.exercises.has_value()) block.exercises = std::vector<cycle_plan::BlockExercise>{};
        block.exercises->push_back(cycle_plan::BlockExercise{});
    }
    if (!block.exercises.has_value() || block.exercises->empty()) {
        ImGui::TextDisabled("none");
        return;
    }

    std::optional<std::size_t> remove_index;
    for (std::size_t i = 0; i < block.exercises->size(); i++) {
        ImGui::PushID(static_cast<int>(i));
        cycle_plan::BlockExercise& ex = (*block.exercises)[i];
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText("##name", &ex.name);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        widgets::optional_number_field("##weight_kg", ex.weight_kg, ui.exercise_weight_fields[i]);
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) remove_index = i;
        ImGui::PopID();
    }
    if (remove_index.has_value())
        block.exercises->erase(block.exercises->begin() + static_cast<std::ptrdiff_t>(*remove_index));
    if (block.exercises->empty()) block.exercises.reset();
}

void draw_block_fields(cycle_plan::Block& block, BlockUiState& ui) {
    ImGui::TextUnformatted("Block");
    const float w = theme::tokens::field_max_width;

    if (ImGui::BeginTable("block_fields", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        widgets::labeled_field_prefix("Type");
        ImGui::SetNextItemWidth(w);
        enum_combo<cycle_plan::BlockType>("##block_type", block.type, std::span<const cycle_plan::BlockType>(kBlockTypes),
                                           cycle_plan::to_string);
        ImGui::TableSetColumnIndex(1);
        widgets::labeled_field_prefix("Role");
        ImGui::SetNextItemWidth(w);
        enum_combo<cycle_plan::BlockRole>("##block_role", block.role, std::span<const cycle_plan::BlockRole>(kBlockRoles),
                                           cycle_plan::to_string);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        widgets::labeled_field_prefix("Machine");
        ImGui::SetNextItemWidth(w);
        widgets::optional_text_field("##machine", block.machine);
        ImGui::TableSetColumnIndex(1);
        widgets::labeled_field_prefix("Duration min");
        ImGui::SetNextItemWidth(w);
        widgets::optional_number_field("##duration_min", block.duration_min, ui.duration_field);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        widgets::labeled_field_prefix("Exercise");
        ImGui::SetNextItemWidth(w);
        widgets::optional_text_field("##exercise", block.exercise);
        ImGui::TableSetColumnIndex(1);
        widgets::labeled_field_prefix("Sets/reps");
        ImGui::SetNextItemWidth(w);
        optional_int_list_field("##sets_reps", block.sets_reps);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        widgets::labeled_field_prefix("Format");
        ImGui::SetNextItemWidth(w);
        optional_enum_combo<MetconFormat>("##format", block.format, std::span<const MetconFormat>(kMetconFormats),
                                           to_string);
        ImGui::TableSetColumnIndex(1);
        widgets::labeled_field_prefix("Scheme");
        ImGui::SetNextItemWidth(w);
        optional_int_list_field("##scheme", block.scheme);

        ImGui::EndTable();
    }

    widgets::labeled_field_prefix("Notes");
    ImGui::SetNextItemWidth(-1.0f);
    widgets::optional_text_field("##block_notes", block.notes);

    ImGui::Separator();
    draw_block_exercises(block, ui);
}

} // namespace

void draw(AppModel& model, State& state) {
    if (!state.attempted_load) load(model, state);

    if (ImGui::Button("Reload")) {
        reset_selection(state);
        load(model, state);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(state.load_error.has_value());
    if (ImGui::Button("Save")) save(model, state);
    ImGui::EndDisabled();
    if (!state.status.empty()) {
        ImGui::SameLine();
        ImGui::TextUnformatted(state.status.c_str());
    }
    ImGui::Separator();

    if (state.load_error.has_value()) {
        ImGui::TextColored(ImVec4(0.85f, 0.30f, 0.30f, 1.0f), "%s", state.load_error->c_str());
        return;
    }

    cycle_plan::Cycle* cycle =
        state.selected_cycle_id.has_value() ? cycle_plan::find_cycle(state.file, *state.selected_cycle_id) : nullptr;

    const float avail_h = ImGui::GetContentRegionAvail().y;

    ImGui::BeginChild("cep_col1", ImVec2(260.0f, avail_h), ImGuiChildFlags_Borders);
    draw_cycle_picker(state);
    if (cycle != nullptr) {
        ImGui::Separator();
        draw_session_list(*cycle, state);
    }
    ImGui::EndChild();
    ImGui::SameLine();

    cycle_plan::Session* session = nullptr;
    if (cycle != nullptr && state.selected_session_index.has_value() &&
        *state.selected_session_index < cycle->sessions.size())
        session = &cycle->sessions[*state.selected_session_index];

    ImGui::BeginChild("cep_col2", ImVec2(360.0f, avail_h), ImGuiChildFlags_Borders);
    if (session != nullptr) {
        draw_session_fields(*session);
        ImGui::Separator();
        draw_block_list(*session, state);
    } else {
        ImGui::TextDisabled("Select a session.");
    }
    ImGui::EndChild();
    ImGui::SameLine();

    cycle_plan::Block* block = nullptr;
    if (session != nullptr && state.selected_block_index.has_value() &&
        *state.selected_block_index < session->blocks.size())
        block = &session->blocks[*state.selected_block_index];

    ImGui::BeginChild("cep_col3", ImVec2(0.0f, avail_h), ImGuiChildFlags_Borders);
    if (block != nullptr) {
        BlockUiState& ui = state.sessions[*state.selected_session_index].blocks[*state.selected_block_index];
        draw_block_fields(*block, ui);
    } else {
        ImGui::TextDisabled("Select a block.");
    }
    ImGui::EndChild();
}

} // namespace workoutlog::ui::cycle_editor
