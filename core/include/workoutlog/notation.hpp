#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "workoutlog/models.hpp"

// Port of app/Sources/WorkoutLogCore/Parsing/Notation.swift. Parses the terse
// paper-log notation into model sets; see docs/03-log-notation.md for the grammar.
// Warn-never-block: a line we can't fully parse still yields the sets it can.
//
// Supported strength forms:
//   6 x [70, 80, 90, 100, 110]          fixed reps x ascending weights -> 5 sets
//   6 x [70, 80] + 6 x [30]             trailing groups are back-off sets
//   6 x [30, 60, 80(4), 86(2), 90(1)]   per-set rep override in parentheses
//   5+5+4+3+3 (20)                      cluster set: chain + total in parens
//   4 x [54c, 40c, 36c, 42c]            timed holds (c/c-cyrillic = seconds)
//   {60c, 70c, 30c, 35c}                bare bracketed list, no count prefix
namespace workoutlog::notation {

struct ParsedSets {
    std::vector<WorkSet> sets;
    std::vector<std::string> warnings;
};

ParsedSets parse_strength_sets(std::string_view raw);

} // namespace workoutlog::notation
