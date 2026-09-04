#pragma once

#include <optional>
#include <vector>

// Port of the month-grid arithmetic in app/Sources/WorkoutLogApp/CalendarView.swift
// (`monthCells`), moved into the core because it's domain arithmetic, not
// presentation, and the SDL3/ImGui calendar needs the identical grid.
namespace workoutlog::calendar {

// 1=Sunday .. 7=Saturday (matches Foundation's Calendar.component(.weekday:)), using
// a proleptic Gregorian civil-date algorithm rather than <chrono>'s calendar types
// (see core/CMakeLists.txt for why: Apple libc++ availability).
int weekday_sun1(int year, int month, int day);

int days_in_month(int year, int month);

// Cells for one month: nullopt for the leading blanks needed to align the first day
// under the right weekday column, then day-of-month 1..N. `first_weekday` uses the
// same 1=Sunday..7=Saturday convention (Calendar.firstWeekday).
std::vector<std::optional<int>> month_cells(int year, int month, int first_weekday);

} // namespace workoutlog::calendar
