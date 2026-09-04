#include "calendar_view.hpp"

#include "theme.hpp"
#include "widgets.hpp"
#include "workoutlog/calendar.hpp"
#include "workoutlog/muscle_group.hpp"
#include "workoutlog/notation.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace workoutlog::ui::calendar_view {

namespace {

// Calendar.firstWeekday is locale-dependent in Swift (Sunday in en_US, Monday in
// uk_UA); the C++ port has no locale plumbing, so this is a fixed, documented
// choice matching the Ukrainian training data rather than the reader's system
// locale. calendar::month_cells's convention: 1=Sunday..7=Saturday.
constexpr int kFirstWeekday = 2; // Monday

constexpr std::array<const char*, 8> kWeekdayShort = {
    "", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", // index 0 unused; 1=Sunday..7=Saturday
};

constexpr std::array<const char*, 13> kMonthNames = {
    "",        "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",  "September", "October",  "November", "December",
};

constexpr float kCellMinHeight = 34.0f;
constexpr float kGridSpacing = 4.0f;
constexpr ImVec2 kBadgePadding(5.0f, 1.0f);

std::pair<int, int> todays_year_month() {
    using namespace std::chrono;
    const auto today = floor<days>(system_clock::now());
    const year_month_day ymd{today};
    return {static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month())};
}

std::string format_date_key(int year, int month, int day) {
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << year << '-' << std::setw(2) << month << '-' << std::setw(2) << day;
    return out.str();
}

// The calendar map's keys come straight from a filename's part before the first
// '_' (workoutlog::cycle::build_index has no format check on it), so a stray file
// in the folder can produce anything here -- parse defensively and skip rather
// than crash (warn-not-block, matching the rest of this codebase).
std::optional<std::array<int, 3>> parse_date_key(const std::string& key) {
    if (key.size() != 10 || key[4] != '-' || key[7] != '-') return std::nullopt;
    auto y = notation::parse_int_strict(key.substr(0, 4));
    auto m = notation::parse_int_strict(key.substr(5, 2));
    auto d = notation::parse_int_strict(key.substr(8, 2));
    if (!y.has_value() || !m.has_value() || !d.has_value()) return std::nullopt;
    if (*m < 1 || *m > 12 || *d < 1 || *d > 31) return std::nullopt;
    return std::array<int, 3>{*y, *m, *d};
}

void anchor_to_latest_session(AppModel& model, State& state) {
    const auto& cal = model.cycle_index().calendar;
    if (cal.empty()) {
        std::tie(state.year, state.month) = todays_year_month();
        return;
    }
    // std::map keys sort ascending, so the last entry is the latest date --
    // matching `model.calendar.keys.max()` in CalendarView.swift.
    const auto& latest_key = cal.rbegin()->first;
    if (auto parsed = parse_date_key(latest_key)) {
        state.year = (*parsed)[0];
        state.month = (*parsed)[1];
    } else {
        std::tie(state.year, state.month) = todays_year_month();
    }
}

void shift_month(State& state, int delta) {
    int m = state.month + delta;
    int y = state.year;
    while (m < 1) {
        m += 12;
        y--;
    }
    while (m > 12) {
        m -= 12;
        y++;
    }
    state.month = m;
    state.year = y;
}

bool draw_header(State& state) {
    // ImGui::SameLine's offset_from_start_x is measured from the start of the
    // line (the content region's left edge), so the row width has to be read
    // *before* anything on this row is drawn -- reading GetContentRegionAvail()
    // partway through would give the width remaining after the left button, not
    // the full row.
    const float row_w = ImGui::GetContentRegionAvail().x;
    const float button_w = ImGui::GetFrameHeight(); // an ArrowButton is one frame-height square

    bool changed = false;
    if (ImGui::ArrowButton("##prev_month", ImGuiDir_Left)) {
        shift_month(state, -1);
        changed = true;
    }

    const std::string title =
        std::string(kMonthNames.at(static_cast<size_t>(state.month))) + " " + std::to_string(state.year);
    const float title_w = ImGui::CalcTextSize(title.c_str()).x;
    ImGui::SameLine((row_w - title_w) * 0.5f, 0.0f);
    ImGui::TextUnformatted(title.c_str());

    ImGui::SameLine(row_w - button_w, 0.0f);
    if (ImGui::ArrowButton("##next_month", ImGuiDir_Right)) {
        shift_month(state, 1);
        changed = true;
    }
    return changed;
}

void draw_weekday_row(float column_width) {
    // Drawn directly rather than through SameLine()'s column games: each label
    // needs to be centred within its own column, and ImGui::SameLine's
    // offset_from_start_x is an absolute position from the line's start, not a
    // per-column relative one -- a raw ImDrawList pass at explicitly-computed
    // column rects is simpler and matches how the day cells below are drawn.
    const ImVec2 row_p0 = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float line_h = ImGui::GetTextLineHeight();
    const ImU32 secondary = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    for (int i = 0; i < 7; i++) {
        const int weekday_1_7 = ((kFirstWeekday - 1 + i) % 7) + 1;
        const char* label = kWeekdayShort.at(static_cast<size_t>(weekday_1_7));
        const float text_w = ImGui::CalcTextSize(label).x;
        const float col_x = row_p0.x + static_cast<float>(i) * (column_width + kGridSpacing);
        draw_list->AddText(ImVec2(col_x + (column_width - text_w) * 0.5f, row_p0.y), secondary, label);
    }
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, line_h));
}

// day + column_width bundled into one aggregate rather than two adjacent
// primitive parameters (bugprone-easily-swappable-parameters: an int and a float
// are implicitly convertible either way).
struct DayCell {
    int day;
    float column_width;
};

// Draws one day cell at the current cursor position and returns the file that
// was clicked, if any -- the caller (draw()) is the one that calls model.open(),
// keeping this a pure rendering helper.
std::optional<std::filesystem::path> draw_day_cell(const AppModel& model, const State& state, const DayCell& cell) {
    ImGui::PushID(cell.day);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 cell_size(cell.column_width, kCellMinHeight);
    const bool clicked = ImGui::InvisibleButton("cell", cell_size);
    ImGui::PopID();

    const ImVec2 p1(p0.x + cell_size.x, p0.y + cell_size.y);
    const std::string key = format_date_key(state.year, state.month, cell.day);
    const auto& calendar = model.cycle_index().calendar;
    const auto it = calendar.find(key);
    const bool has_info = it != calendar.end();
    const auto& selection = model.selection();
    const bool selected = has_info && selection.has_value() && selection.value() == it->second.path;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (selected) {
        const ImU32 accent_fill = theme::rgba(theme::kAccentHex, 0.18f);
        const ImU32 accent_line = theme::rgba(theme::kAccentHex, 1.0f);
        draw_list->AddRectFilled(p0, p1, accent_fill, theme::tokens::day_cell_rounding);
        draw_list->AddRect(p0, p1, accent_line, theme::tokens::day_cell_rounding, 0, 1.0f);
    }

    const std::string day_text = std::to_string(cell.day);
    const ImVec2 day_text_size = ImGui::CalcTextSize(day_text.c_str());
    const ImU32 day_color = ImGui::GetColorU32(has_info ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    draw_list->AddText(ImVec2(p0.x + (cell_size.x - day_text_size.x) * 0.5f, p0.y + 2.0f), day_color,
                        day_text.c_str());

    if (has_info) {
        const auto& group = it->second.group;
        const std::string hex = group.has_value() ? workoutlog::hex(group.value()) : "#2a78d6";
        const ImU32 badge_color = theme::rgba(hex);
        const char* badge_text = it->second.cycle_day.c_str();
        // capsule_badge draws from a top-left corner; measure first (the same way
        // it will internally) so the badge lands centred under the day number,
        // matching the SwiftUI VStack(spacing: 1) original, without a second draw.
        const ImVec2 badge_text_size = ImGui::CalcTextSize(badge_text);
        const float badge_w = badge_text_size.x + kBadgePadding.x * 2.0f;
        const ImVec2 badge_pos(p0.x + (cell_size.x - badge_w) * 0.5f, p0.y + day_text_size.y + 3.0f);
        widgets::capsule_badge(*draw_list, badge_pos, badge_text, badge_color, kBadgePadding);
    }

    if (clicked && has_info) return it->second.path;
    return std::nullopt;
}

} // namespace

bool draw(AppModel& model, State& state) {
    if (!state.anchored) {
        anchor_to_latest_session(model, state);
        state.anchored = true;
    }

    ImGui::BeginGroup();
    draw_header(state);

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float column_width = (avail_w - kGridSpacing * 6.0f) / 7.0f;

    draw_weekday_row(column_width);

    bool day_clicked = false;
    const auto cells = calendar::month_cells(state.year, state.month, kFirstWeekday);
    for (size_t i = 0; i < cells.size(); i++) {
        const size_t col = i % 7;
        if (col > 0) {
            ImGui::SameLine(0.0f, kGridSpacing);
        }
        const auto& cell = cells[i];
        if (!cell.has_value()) {
            ImGui::Dummy(ImVec2(column_width, kCellMinHeight));
            continue;
        }
        if (auto clicked_path = draw_day_cell(model, state, DayCell{cell.value(), column_width})) {
            model.open(*clicked_path);
            day_clicked = true;
        }
    }

    ImGui::Spacing();
    widgets::muscle_group_legend();
    ImGui::EndGroup();

    return day_clicked;
}

} // namespace workoutlog::ui::calendar_view
