#include "cycle_view.hpp"

#include "theme.hpp"
#include "widgets.hpp"
#include "workoutlog/muscle_activation.hpp"
#include "workoutlog/muscle_group.hpp"

#include <algorithm>
#include <span>

namespace workoutlog::ui::cycle_view {

namespace {

// CycleTable in Views.swift is a plain two-axis ScrollView with no pinned
// header/name column -- reproduced as-is here, not "improved" with a freeze pane
// the original doesn't have.
constexpr float kRowVerticalPadding = 6.0f; // matches .padding(.vertical, 6) per cell
constexpr float kCellHorizontalPadding = 8.0f;
constexpr float kPresenceDotRadius = 4.5f; // 9px circle

void draw_cell_text(ImDrawList& draw_list, ImVec2 cell_p0, ImVec2 cell_size, const char* text, bool centered) {
    const ImVec2 text_size = ImGui::CalcTextSize(text);
    const float x = centered ? cell_p0.x + (cell_size.x - text_size.x) * 0.5f : cell_p0.x + kCellHorizontalPadding;
    const float y = cell_p0.y + (cell_size.y - text_size.y) * 0.5f;
    draw_list.AddText(ImVec2(x, y), ImGui::GetColorU32(ImGuiCol_Text), text);
}

void draw_hairline(ImDrawList& draw_list, ImVec2 p0, ImVec2 p1) {
    draw_list.AddLine(p0, p1, ImGui::GetColorU32(ImGuiCol_Separator), 0.5f);
}

void draw_cycle_grid(AppModel& model) {
    const auto& matrix = model.cycle_index().cycle;
    if (matrix.days.empty()) {
        ImGui::TextDisabled("No sessions in this folder.");
        return;
    }

    ImGui::BeginChild("cycle_grid", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

    const float name_w = theme::tokens::cycle_name_column_width;
    const float day_w = theme::tokens::cycle_day_column_width;
    const float row_h = ImGui::GetTextLineHeight() + kRowVerticalPadding * 2.0f;
    const float total_w = name_w + day_w * static_cast<float>(matrix.days.size());
    const ImU32 header_bg = ImGui::GetColorU32(ImGuiCol_ChildBg);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    {
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        draw_list->AddRectFilled(p0, ImVec2(p0.x + total_w, p0.y + row_h), header_bg);
        draw_cell_text(*draw_list, p0, ImVec2(name_w, row_h), "Exercise", false);
        draw_hairline(*draw_list, ImVec2(p0.x, p0.y + row_h), ImVec2(p0.x + total_w, p0.y + row_h));
        float x = p0.x + name_w;
        for (const auto& day : matrix.days) {
            draw_cell_text(*draw_list, ImVec2(x, p0.y), ImVec2(day_w, row_h), day.c_str(), true);
            x += day_w;
        }
        ImGui::Dummy(ImVec2(total_w, row_h));
    }

    // One row per exercise, in first-seen order (matching CycleTable.body's
    // ForEach over matrix.exercises).
    for (size_t ei = 0; ei < matrix.exercises.size(); ei++) {
        ImGui::PushID(static_cast<int>(ei));
        const std::span<const MuscleGroup> groups = matrix.groups[ei];
        const ImVec2 p0 = ImGui::GetCursorScreenPos();

        const auto name_wash = theme::group_gradient(groups, 0.22);
        draw_list->AddRectFilledMultiColor(p0, ImVec2(p0.x + name_w, p0.y + row_h), name_wash.top_left,
                                            name_wash.top_right, name_wash.bottom_right, name_wash.bottom_left);
        draw_cell_text(*draw_list, p0, ImVec2(name_w, row_h), matrix.exercises[ei].c_str(), false);

        float x = p0.x + name_w;
        for (size_t di = 0; di < matrix.days.size(); di++) {
            const bool present = matrix.cells[ei][di];
            if (present) {
                const auto mark_wash = theme::group_gradient(groups, 0.16);
                draw_list->AddRectFilledMultiColor(ImVec2(x, p0.y), ImVec2(x + day_w, p0.y + row_h), mark_wash.top_left,
                                                    mark_wash.top_right, mark_wash.bottom_right, mark_wash.bottom_left);
                const ImU32 dot_color =
                    groups.empty() ? theme::rgba(theme::kAccentHex) : theme::rgba(workoutlog::hex(groups.front()));
                draw_list->AddCircleFilled(ImVec2(x + day_w * 0.5f, p0.y + row_h * 0.5f), kPresenceDotRadius, dot_color);
            }
            x += day_w;
        }

        draw_hairline(*draw_list, ImVec2(p0.x, p0.y + row_h), ImVec2(p0.x + total_w, p0.y + row_h));
        ImGui::Dummy(ImVec2(total_w, row_h));
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void draw_cycle_muscle_map(SDL_Renderer& renderer, AppModel& model, SvgTexture& texture) {
    ImGui::TextUnformatted("Cycle muscle map");

    WeightingMode mode = model.mode();
    if (widgets::segmented<WeightingMode>("cycle_mode", std::span<const WeightingMode>(kWeightingModeOrder), mode,
                                           [](WeightingMode m) { return label(m); })) {
        model.set_mode(mode);
    }

    if (auto svg = model.cycle_map_svg()) {
        const float box_w = ImGui::GetContentRegionAvail().x;
        texture.update(renderer, *svg, static_cast<int>(box_w), 260);
        if (texture.id() != 0) ImGui::Image(texture.id(), texture.size());
    } else {
        ImGui::TextDisabled("Muscle map unavailable (no sessions or catalogue missing).");
    }
}

} // namespace

bool draw(SDL_Renderer& renderer, AppModel& model, State& state) {
    // The bottom panel's natural content height (mode picker + 260px map + 12px
    // gap + 320px calendar, each with a little padding); Views.swift lets this
    // section take whatever it needs and the grid ScrollView fill what's left
    // (.frame(maxWidth: .infinity, maxHeight: .infinity)). On a window too short
    // for both, the grid still keeps a minimum visible area and the bottom panel
    // scrolls internally instead of pushing the grid to nothing.
    constexpr float kDesiredBottomPanelHeight = 12.0f + 24.0f + 6.0f + 24.0f + 12.0f + 260.0f + 12.0f + 320.0f;
    constexpr float kMinGridHeight = 120.0f;
    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float bottom_panel_h = std::min(kDesiredBottomPanelHeight, std::max(0.0f, avail_h - kMinGridHeight));

    ImGui::BeginChild("cycle_grid_region", ImVec2(0.0f, avail_h - bottom_panel_h));
    draw_cycle_grid(model);
    ImGui::EndChild();

    ImGui::Separator();

    ImGui::BeginChild("cycle_bottom", ImVec2(0.0f, bottom_panel_h));
    ImGui::BeginChild("cycle_left_col", ImVec2(320.0f, 0.0f));

    draw_cycle_muscle_map(renderer, model, state.cycle_map);
    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    ImGui::BeginChild("cycle_calendar", ImVec2(300.0f, 320.0f));
    const bool day_clicked = calendar_view::draw(model, state.calendar);
    ImGui::EndChild();

    ImGui::EndChild(); // cycle_left_col
    ImGui::EndChild(); // cycle_bottom

    return day_clicked;
}

} // namespace workoutlog::ui::cycle_view
