#pragma once

#include <imgui.h>

#include <span>
#include <string_view>

#include "workoutlog/muscle_group.hpp"

// The visual layer over stock ImGui: style, colours and the design tokens carried
// over from the SwiftUI source (Views.swift / CalendarView.swift). Same screens
// and behaviour as the macOS app, themed to approximate its look -- not a pixel
// clone (see the plan).
namespace workoutlog::ui::theme {

// Apple's system blue in dark mode -- the closest stand-in for SwiftUI's
// Color.accentColor, which this app never overrides, so it renders as this on
// every macOS install the SwiftUI original runs on. Exposed (not just used
// inside apply()) because the calendar's selection ring needs the same colour
// SwiftUI's Color.accentColor produces there, not a second hand-picked one.
inline constexpr std::string_view kAccentHex = "#0a84ff";

// Applies rounding, spacing and the colour palette to an ImGui style. Call once,
// after the font atlas is built and display_scale is known.
void apply(ImGuiStyle& style, float display_scale);

// Parses "#rrggbb" (the format workoutlog::hex(MuscleGroup) returns) into a packed
// colour at `alpha` (0-1). Malformed input (wrong length or a non-hex digit)
// degrades to an obviously-wrong magenta rather than reading out of bounds or
// throwing -- this is UI-constant input today, but it costs nothing to be
// defensive at a string boundary (AGENTS.md 1.2.1). Routes every colour in ui/
// through ImGui::ColorConvertFloat4ToU32 instead of the IM_COL32 macro.
ImU32 rgba(std::string_view hex, float alpha = 1.0f);

// Corner colours for ImDrawList::AddRectFilledMultiColor(p0, p1, top_left,
// top_right, bottom_right, bottom_left), reproducing groupGradient in Views.swift:
// a light topLeading -> bottomTrailing wash of a cell's primary muscle group
// colour(s). AddRectFilledMultiColor bilinearly interpolates, so top_left = a,
// bottom_right = b, top_right = bottom_left = the average of a and b gives an
// exact linear gradient along that diagonal for a square cell.
struct CornerColors {
    ImU32 top_left;
    ImU32 top_right;
    ImU32 bottom_right;
    ImU32 bottom_left;
};

// Empty `groups` -> fully transparent (SwiftUI's [.clear, .clear]). One group ->
// a two-stop wash of that colour, base*1.4 fading to base*0.5, matching Views.swift
// exactly. Two or more -> SwiftUI draws a genuine N-stop gradient (one colour per
// group, no gap), which 4 corners can't reproduce; the approximation here is the
// first and last group at `base` opacity, which is exact for the swap case (2
// groups) and a documented simplification beyond that -- see how many exercises in
// exercises.json actually have 3+ primary muscle groups before spending more on it.
CornerColors group_gradient(std::span<const MuscleGroup> groups, double base_opacity);

// Layout constants mirrored from the SwiftUI source, read from one place instead
// of being repeated as literals across every screen.
namespace tokens {
inline constexpr int window_min_width = 820;
inline constexpr int window_min_height = 560;
inline constexpr float sidebar_min_width = 260.0f;
inline constexpr float card_rounding = 10.0f;
inline constexpr float day_cell_rounding = 6.0f;
inline constexpr float legend_swatch_rounding = 3.0f;
inline constexpr float cycle_name_column_width = 240.0f;
inline constexpr float cycle_day_column_width = 46.0f;
inline constexpr float calendar_min_cell_height = 34.0f;
inline constexpr float field_label_width = 78.0f;
inline constexpr float field_max_width = 220.0f;
} // namespace tokens

} // namespace workoutlog::ui::theme
