#pragma once

#include <imgui.h>

#include <optional>
#include <span>
#include <string>

#include "workoutlog/muscle_group.hpp"

// Small, cross-screen ImGui building blocks -- the C++ stand-ins for the SwiftUI
// helpers in Views.swift (card backgrounds, the segmented picker, the optional
// Binding<String?> adapter, MuscleGroupLegend). Stock ImGui has no equivalent for
// most of these.
namespace workoutlog::ui::widgets {

// A rounded, tinted panel matching RoundedRectangle(cornerRadius: 10) + card
// padding in Views.swift's BlockCard/DayMuscleMap. BeginChild()'s pairing rule is
// unlike other Begin/End calls in ImGui: EndChild() must always be called, even
// if a hypothetical caller wanted to skip content -- so this pair takes no bool
// return and the caller always draws between them.
void card_begin(const char* str_id, float width = 0.0f);
void card_end();

// Draws a filled pill behind `text` at `top_left` (screen-space coordinates),
// matching Capsule().fill(...) in CalendarView.swift (the cycle-day badge under a
// calendar day number). A pure ImDrawList primitive -- it never touches the
// ImGui layout cursor, so it composes with a caller drawing inline (pass
// ImGui::GetCursorScreenPos(), then ImGui::Dummy(the returned size) to reserve
// the space) or, like calendar_view's day cells, positioning it inside an
// already-reserved rect. Returns the pill's size.
ImVec2 capsule_badge(ImDrawList& draw_list, ImVec2 top_left, const char* text, ImU32 fill_color,
                      ImVec2 padding = ImVec2(5.0f, 1.0f));

// The right-aligned caption label at the session editor's fixed column width
// (theme::tokens::field_label_width), then SameLine() so the caller's own field
// widget follows immediately -- the C++ shape of `labeled()` in Views.swift, which
// wraps arbitrary field content the same way.
void labeled_field_prefix(const char* label);

// Binds an optional<string> to a text field: empty text <-> nullopt, the same
// convention as `optional(_:)` in Views.swift. Safe to rebuild the scratch string
// from value_or("") every frame: while the field is active, ImGui owns the live
// edit buffer (misc/cpp/imgui_stdlib.cpp keeps `text` synced to it on every call),
// so this never fights the user's keystrokes -- it only supplies the *initial*
// text and reads back what's now on screen.
bool optional_text_field(const char* label, std::optional<std::string>& value, const char* hint = nullptr);

// Per-field persistent state for optional_number_field, owned by the caller (one
// instance per numeric field) so it survives across frames the way SwiftUI's
// @State would. Whether the currently-typed text parses can only be known *after*
// the field draws, but the invalid-input tint has to be decided *before* -- so the
// tint always lags the parse check by exactly one frame, which is the standard
// immediate-mode answer to this ordering problem.
struct NumberFieldState {
    bool unparseable = false;
};

// Binds an optional<double> to a text field, parsed with
// workoutlog::notation::parse_double_strict (never std::stod: it accepts trailing
// garbage a hand-typed "7." or "-" would otherwise silently coerce). The model is
// left untouched while the field is being edited -- a transient invalid state like
// "7." or "-" must not clobber it -- and only committed on
// ImGui::IsItemDeactivatedAfterEdit(); an unparseable value on deactivation is
// simply discarded back to the last committed one, never coerced to a default
// (AGENTS.md 1.2.2). Returns true the frame a new value commits.
bool optional_number_field(const char* label, std::optional<double>& value, NumberFieldState& state);

// The stand-in for SwiftUI's .pickerStyle(.segmented): a row of `options.size()`
// mutually-exclusive buttons sharing `width` (or the available width). `label_for`
// must return something usable as `.c_str()` (e.g. workoutlog::label(T) or
// std::to_string) -- called and consumed within the same expression as the widget
// call, so a temporary's lifetime is never an issue. Returns true the frame the
// selection changes.
template <typename T, typename LabelFn>
bool segmented(const char* str_id, std::span<const T> options, T& current, LabelFn&& label_for,
               float width = 0.0f) {
    bool changed = false;
    ImGui::PushID(str_id);
    const float avail = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    const float each = avail / static_cast<float>(options.size());
    for (size_t i = 0; i < options.size(); i++) {
        if (i > 0) ImGui::SameLine(0.0f, 0.0f);
        const bool selected = options[i] == current;
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button(label_for(options[i]).c_str(), ImVec2(each, 0.0f))) {
            changed = options[i] != current;
            current = options[i];
        }
        ImGui::PopID();
        if (selected) ImGui::PopStyleColor();
    }
    ImGui::PopID();
    return changed;
}

// The 6-swatch key for MuscleGroup colours, matching MuscleGroupLegend in
// Views.swift: an adaptive wrap of an 11x11 rounded swatch plus a caption label
// per group, in kMuscleGroupOrder.
void muscle_group_legend();

} // namespace workoutlog::ui::widgets
