#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "workoutlog/models.hpp"
#include "workoutlog/muscle_group.hpp"
#include "workoutlog/store.hpp"

// Port of AppModel.refresh() / buildCycle() / primaryGroups() in
// app/Sources/WorkoutLogApp/WorkoutLogApp.swift. This was domain logic sitting in the
// SwiftUI layer (an ADR-004 leak) -- moving it here is what makes the SDL3 UI's
// calendar and cycle table a re-skin instead of a second implementation.
namespace workoutlog::cycle {

struct DayInfo {
    std::filesystem::path path;
    std::string cycle_day;
    bool is_metcon = false;
    std::optional<MuscleGroup> group;
};

// cells[exercise_index][day_index]; groups[exercise_index] is that exercise's
// distinct primary muscle groups, in listed order.
struct CycleMatrix {
    std::vector<std::string> days;
    std::vector<std::string> exercises;
    std::vector<std::vector<bool>> cells;
    std::vector<std::vector<MuscleGroup>> groups;
};

struct Index {
    std::map<std::string, DayInfo> calendar; // keyed "yyyy-MM-dd"
    CycleMatrix cycle;
    std::vector<Session> cycle_sessions; // one per cycle day (most recent), sorted by cycle day
};

// Walks every *.json in `store`'s folder. Date/cycle_day come from the filename, so a
// file that fails to parse still gets a calendar entry (matching Swift's `try?
// store.load(url)` -> nil still producing a DayInfo) -- only its is_metcon/group and
// its contribution to the cycle matrix are skipped.
Index build_index(const SessionStore& store, const std::optional<Catalogue>& catalogue);

// Distinct primary muscle groups of an exercise, in listed order (empty if the name
// isn't in the catalogue or no catalogue was given).
std::vector<MuscleGroup> primary_groups(const std::string& exercise_name, const std::optional<Catalogue>& catalogue);

} // namespace workoutlog::cycle
