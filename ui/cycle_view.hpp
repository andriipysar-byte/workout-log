#pragma once

#include "app_model.hpp"
#include "calendar_view.hpp"
#include "svg_texture.hpp"

struct SDL_Renderer;

// The Cycle tab, ported from CycleView/CycleTable/CycleMuscleMap in Views.swift:
// the cycle-day x exercise presence grid on top, and a mode picker + cycle
// muscle map + embedded month calendar below.
namespace workoutlog::ui::cycle_view {

// Persists across frames: the embedded calendar's own state, and the cycle map's
// rasterized texture (SvgTexture is itself a cache -- see ui/svg_texture.hpp).
struct State {
    calendar_view::State calendar;
    SvgTexture cycle_map;
};

// Returns true the frame a calendar day is clicked (forwarded from the embedded
// calendar_view::draw), so root_view can switch the sidebar back to List.
bool draw(SDL_Renderer& renderer, AppModel& model, State& state);

} // namespace workoutlog::ui::cycle_view
