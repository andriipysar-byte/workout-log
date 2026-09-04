#pragma once

#include <string_view>

#include "workoutlog/models.hpp"

// Port of Catalogue.resolve in app/Sources/WorkoutLogCore/Models/Exercise.swift.
namespace workoutlog::catalogue {

// Matches canonical names and aliases, case-insensitively (Unicode-correct, not just
// ASCII -- see workoutlog::utf8::to_lower_cp). Returns nullptr when unknown: the
// caller warns, never blocks (ADR-006).
const Exercise* resolve(const Catalogue&, std::string_view typed);

} // namespace workoutlog::catalogue
