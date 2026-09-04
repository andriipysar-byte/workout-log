#include "widgets.hpp"

#include <imgui_stdlib.h>

#include <sstream>

#include "theme.hpp"
#include "workoutlog/notation.hpp"

namespace workoutlog::ui::widgets {

namespace {

// General stream formatting (no fixed precision) gives a compact, round-trip-ish
// text for the values this app edits (kilograms, minutes, metres) without the
// fixed six trailing zeros std::to_string(double) would add, and without <format>
// (outside the conservative subset core/ requires -- ui/ doesn't need that
// constraint, but there's no reason to reach for it over an ostringstream here).
std::string double_to_text(double v) {
    std::ostringstream out;
    out << v;
    return out.str();
}

} // namespace

void card_begin(const char* str_id, float width) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme::tokens::card_rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    ImGui::BeginChild(str_id, ImVec2(width, 0.0f),
                       ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
}

void card_end() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

ImVec2 capsule_badge(ImDrawList& draw_list, ImVec2 top_left, const char* text, ImU32 fill_color, ImVec2 padding) {
    const ImVec2 text_size = ImGui::CalcTextSize(text);
    const ImVec2 size(text_size.x + padding.x * 2.0f, text_size.y + padding.y * 2.0f);
    const ImVec2 bottom_right(top_left.x + size.x, top_left.y + size.y);

    draw_list.AddRectFilled(top_left, bottom_right, fill_color, size.y * 0.5f);
    const ImU32 white = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    draw_list.AddText(ImVec2(top_left.x + padding.x, top_left.y + padding.y), white, text);

    return size;
}

void labeled_field_prefix(const char* label) {
    const float w = theme::tokens::field_label_width;
    const ImVec2 text_size = ImGui::CalcTextSize(label);
    ImGui::Dummy(ImVec2(w, text_size.y));
    ImGui::SameLine(w - text_size.x, 0.0f); // right-aligned within the fixed column
    ImGui::TextUnformatted(label);
    ImGui::SameLine(0.0f, 6.0f);
}

bool optional_text_field(const char* label, std::optional<std::string>& value, const char* hint) {
    std::string text = value.value_or("");
    const bool edited = (hint != nullptr) ? ImGui::InputTextWithHint(label, hint, &text) : ImGui::InputText(label, &text);
    if (edited) value = text.empty() ? std::nullopt : std::optional<std::string>(text);
    return edited;
}

bool optional_number_field(const char* label, std::optional<double>& value, NumberFieldState& state) {
    std::string text = value.has_value() ? double_to_text(*value) : std::string();

    const bool tint = state.unparseable;
    if (tint) {
        const ImVec4 warn(0.45f, 0.10f, 0.10f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, warn);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, warn);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, warn);
    }

    ImGui::InputText(label, &text);

    if (tint) ImGui::PopStyleColor(3);

    // `text` now holds whatever is on screen this frame (imgui_stdlib.cpp keeps it
    // synced whether or not the item is still active) -- recompute parseability
    // from it so next frame's tint matches what the user is looking at now.
    state.unparseable = !text.empty() && !notation::parse_double_strict(text).has_value();

    bool committed = false;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (text.empty()) {
            value = std::nullopt;
            committed = true;
        } else if (auto parsed = notation::parse_double_strict(text)) {
            value = parsed;
            committed = true;
        }
        state.unparseable = false; // the visible text reverts to the last committed value either way
    }
    return committed;
}

void muscle_group_legend() {
    constexpr float kGap = 8.0f;
    constexpr ImVec2 kSwatch(11.0f, 11.0f);
    const float avail = ImGui::GetContentRegionAvail().x;

    float row_x = 0.0f;
    bool is_first_item = true;
    for (MuscleGroup g : kMuscleGroupOrder) {
        const std::string text = label(g);
        const float item_w = kSwatch.x + 5.0f + ImGui::CalcTextSize(text.c_str()).x;

        if (is_first_item) {
            is_first_item = false;
        } else if (row_x + kGap + item_w > avail) {
            row_x = 0.0f; // wraps: no SameLine call, so ImGui starts a new row on its own
        } else {
            ImGui::SameLine(0.0f, kGap);
            row_x += kGap;
        }

        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p0, ImVec2(p0.x + kSwatch.x, p0.y + kSwatch.y),
                                                   theme::rgba(workoutlog::hex(g)), theme::tokens::legend_swatch_rounding);
        ImGui::Dummy(kSwatch);
        ImGui::SameLine(0.0f, 5.0f);
        ImGui::TextUnformatted(text.c_str());

        row_x += item_w;
    }
}

} // namespace workoutlog::ui::widgets
