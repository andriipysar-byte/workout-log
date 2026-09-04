#pragma once

#include "app_model.hpp"

// The month calendar, ported from CalendarView.swift: a month header with shift
// buttons, a weekday row, a day grid with cycle-day badges, and the muscle-group
// legend.
namespace workoutlog::ui::calendar_view {

// Persists across frames -- the C++ equivalent of CalendarView's
// `@State private var anchor`.
struct State {
    int year = 0;
    int month = 0;
    // Anchoring to the latest session's month happens once, matching .onAppear in
    // CalendarView.swift, not on every draw (the user must be able to navigate
    // away with the chevrons without being snapped back).
    bool anchored = false;
};

// Draws the header, weekday row, day grid and legend into the available content
// region. Clicking a day with a session calls model.open() directly (matching the
// SwiftUI .onTapGesture) and returns true so the caller can react -- root_view
// switches the sidebar back to the List tab, which the SwiftUI original doesn't
// do (Cycle and List are different panes there, so clicking a day appears to do
// nothing until the user flips the tab themselves); that's a one-line, obviously
// better fix, not a divergence worth preserving.
bool draw(AppModel& model, State& state);

} // namespace workoutlog::ui::calendar_view
