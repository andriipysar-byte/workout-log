#include "session_editor.hpp"

#include <imgui_stdlib.h>

#include <array>
#include <cmath>
#include <span>
#include <variant>

#include "theme.hpp"
#include "workoutlog/muscle_activation.hpp"
#include "workoutlog/notation.hpp"

namespace workoutlog::ui::session_editor {

namespace {

void draw_kind_combo(Kind& kind) {
    static constexpr std::array<Kind, 3> kKinds = {Kind::training, Kind::deload, Kind::retest};
    const std::string current = to_string(kind);
    if (ImGui::BeginCombo("##kind", current.c_str())) {
        for (Kind k : kKinds) {
            const bool selected = k == kind;
            if (ImGui::Selectable(to_string(k).c_str(), selected)) kind = k;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void draw_header(Session& session, widgets::NumberFieldState& bodyweight_state) {
    if (!ImGui::BeginTable("header_grid", 2, ImGuiTableFlags_SizingFixedFit)) return;

    const float w = theme::tokens::field_max_width;

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    widgets::labeled_field_prefix("Date");
    ImGui::SetNextItemWidth(w);
    ImGui::InputText("##date", &session.date);
    ImGui::TableSetColumnIndex(1);
    widgets::labeled_field_prefix("Cycle day");
    ImGui::SetNextItemWidth(w);
    ImGui::InputText("##cycle_day", &session.cycle_day);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    widgets::labeled_field_prefix("Start");
    ImGui::SetNextItemWidth(w);
    widgets::optional_text_field("##start", session.start_time, "H:MM");
    ImGui::TableSetColumnIndex(1);
    widgets::labeled_field_prefix("Kind");
    ImGui::SetNextItemWidth(w);
    draw_kind_combo(session.kind);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    widgets::labeled_field_prefix("Bodyweight");
    ImGui::SetNextItemWidth(w);
    widgets::optional_number_field("##bodyweight", session.bodyweight_kg, bodyweight_state);
    ImGui::TableSetColumnIndex(1);
    widgets::labeled_field_prefix("Notes");
    ImGui::SetNextItemWidth(w);
    widgets::optional_text_field("##notes", session.notes);

    ImGui::EndTable();
}

void draw_cardio_block(CardioBlock& block, BlockUiState& ui) {
    ImGui::TextUnformatted("Cardio");
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("##machine", &block.machine);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    widgets::optional_number_field("##duration_min", block.duration_min, ui.cardio_duration_field);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    widgets::optional_number_field("##distance_m", block.distance_m, ui.cardio_distance_field);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    widgets::optional_text_field("##end", block.end_time);
}

void draw_cooldown_block(CooldownBlock& block) {
    ImGui::TextUnformatted("Cooldown");
    ImGui::SetNextItemWidth(80.0f);
    widgets::optional_text_field("##end", block.end_time);
    ImGui::SameLine();
    widgets::optional_text_field("##notes", block.notes);
}

// Read-only, matching MetconEditor in Views.swift -- the SwiftUI original never
// binds any of this block's fields either.
void draw_metcon_block(const MetconBlock& block) {
    ImGui::TextUnformatted("Metcon");
    ImGui::TextDisabled("format: %s", block.format.has_value() ? to_string(*block.format).c_str() : "—");

    if (block.scheme.has_value()) {
        std::string scheme_text = "scheme: ";
        for (std::size_t i = 0; i < block.scheme->size(); i++) {
            if (i > 0) scheme_text += "-";
            scheme_text += std::to_string(block.scheme->at(i));
        }
        ImGui::TextUnformatted(scheme_text.c_str());
    }

    for (const auto& exercise : block.exercises) {
        std::string line = "• " + exercise.name;
        if (exercise.weight_kg.has_value() && std::isfinite(*exercise.weight_kg)) {
            line += " (" + std::to_string(std::llround(*exercise.weight_kg)) + " kg)";
        }
        ImGui::TextUnformatted(line.c_str());
    }

    if (block.notes.has_value()) ImGui::TextDisabled("%s", block.notes->c_str());
}

std::string format_sets(std::span<const WorkSet> sets, const char* separator) {
    std::string out;
    for (std::size_t i = 0; i < sets.size(); i++) {
        if (i > 0) out += separator;
        out += notation::format_set(sets[i]);
    }
    return out;
}

void draw_strength_block(SDL_Renderer& renderer, AppModel& model, StrengthBlock& block, BlockUiState& ui,
                          ImFont* mono_font) {
    ImGui::TextUnformatted(block.exercise.c_str());
    if (block.notes.has_value()) ImGui::TextDisabled("%s", block.notes->c_str());

    if (block.sets.empty()) {
        ImGui::TextDisabled("no sets yet");
    } else {
        ImGui::PushFont(mono_font);
        ImGui::TextUnformatted(format_sets(block.sets, "   ").c_str());
        ImGui::PopFont();
    }

    ImGui::SetNextItemWidth(-70.0f); // leaves room for the Apply button
    ImGui::PushFont(mono_font);
    const bool submitted =
        ImGui::InputTextWithHint("##notation", "6 × [70, 80, 90, 100, 110]", &ui.notation_text,
                                  ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopFont();
    ImGui::SameLine();
    const auto parsed = notation::parse_strength_sets(ui.notation_text);
    ImGui::BeginDisabled(parsed.sets.empty());
    const bool apply_clicked = ImGui::Button("Apply");
    ImGui::EndDisabled();
    if ((submitted || apply_clicked) && !parsed.sets.empty()) {
        block.sets = parsed.sets;
        ui.notation_text.clear();
    }

    if (!ui.notation_text.empty()) {
        if (parsed.sets.empty()) {
            ImGui::TextColored(ImVec4(0.85f, 0.30f, 0.30f, 1.0f), "→ —");
        } else {
            ImGui::TextColored(ImVec4(0.35f, 0.80f, 0.40f, 1.0f), "→ %s", format_sets(parsed.sets, "  ").c_str());
        }
        for (const auto& warning : parsed.warnings) {
            ImGui::TextColored(ImVec4(0.90f, 0.60f, 0.20f, 1.0f), "%s", warning.c_str());
        }
    }

    if (ImGui::CollapsingHeader("Muscle map")) {
        if (auto svg = model.exercise_map_svg(block.exercise)) {
            const int box_w = static_cast<int>(ImGui::GetContentRegionAvail().x);
            ui.exercise_map.update(renderer, *svg, box_w, 200);
            if (ui.exercise_map.id() != 0) ImGui::Image(ui.exercise_map.id(), ui.exercise_map.size());
        } else {
            ImGui::TextDisabled("\"%s\" not in catalogue — no map.", block.exercise.c_str());
        }
    }
}

void draw_day_muscle_map(SDL_Renderer& renderer, AppModel& model, SvgTexture& texture) {
    ImGui::Text("Muscle map");
    WeightingMode mode = model.mode();
    if (widgets::segmented<WeightingMode>("day_mode", std::span<const WeightingMode>(kWeightingModeOrder), mode,
                                           [](WeightingMode m) { return label(m); }, 220.0f)) {
        model.set_mode(mode);
    }
    if (auto svg = model.day_map_svg()) {
        const int box_w = static_cast<int>(ImGui::GetContentRegionAvail().x);
        texture.update(renderer, *svg, box_w, 280);
        if (texture.id() != 0) ImGui::Image(texture.id(), texture.size());
    } else {
        ImGui::TextDisabled("Muscle map unavailable (catalogue or template not found).");
    }
}

} // namespace

void draw(SDL_Renderer& renderer, AppModel& model, State& state, ImFont* mono_font) {
    const auto current_selection = model.selection().value_or(std::filesystem::path{});
    if (current_selection != state.last_selection) {
        state.forget_blocks();
        state.last_selection = current_selection;
    }

    if (ImGui::Button("Save")) model.save();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)) model.save();
    ImGui::Separator();

    ImGui::BeginChild("session_scroll");

    Session& session = model.mutable_session();
    draw_header(session, state.bodyweight_field);
    ImGui::Separator();

    for (std::size_t i = 0; i < session.blocks.size(); i++) {
        ImGui::PushID(static_cast<int>(i));
        widgets::card_begin("block_card");
        BlockUiState& ui = state.blocks[i];
        std::visit(
            [&](auto& block) {
                using T = std::decay_t<decltype(block)>;
                if constexpr (std::is_same_v<T, CardioBlock>) {
                    draw_cardio_block(block, ui);
                } else if constexpr (std::is_same_v<T, StrengthBlock>) {
                    draw_strength_block(renderer, model, block, ui, mono_font);
                } else if constexpr (std::is_same_v<T, MetconBlock>) {
                    draw_metcon_block(block);
                } else if constexpr (std::is_same_v<T, CooldownBlock>) {
                    draw_cooldown_block(block);
                } else {
                    static_assert(!sizeof(T*), "unhandled Block alternative -- add a branch above");
                }
            },
            session.blocks[i]);
        widgets::card_end();
        ImGui::PopID();
    }

    ImGui::Separator();
    draw_day_muscle_map(renderer, model, state.day_map);

    ImGui::EndChild();
}

} // namespace workoutlog::ui::session_editor
