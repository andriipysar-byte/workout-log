#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "workoutlog/models.hpp"

// The terse paper-log notation (docs/03-log-notation.md):
//   6 × [70, 80, 90, 100, 110]          fixed reps × ascending weights → 5 sets
//   6 × [70, 80] + 6 × [30]             trailing groups are back-off sets
//   6 × [30, 60, 80(4), 86(2), 90(1)]   per-set rep override in parentheses
//   5+5+4+3+3 (20)                      cluster set: chain + total in parens
//   4 × [54c, 40c, 36c, 42c]            timed holds (c/с = seconds) → duration sets
//   {60c, 70c, 30c, 35c}                bare bracketed list, no count prefix
namespace wl {

// Warn, never block: a line that only partly parses still yields what it can.
struct ParsedSets {
    std::vector<WorkSet> sets;
    std::vector<std::string> warnings;
    bool operator==(const ParsedSets&) const = default;
};

ParsedSets parse_strength_sets(std::string_view raw);

// One set as the UI and MCP print it: `5+5+4 (14)`, `54c`, `6×90`, `bw`, `*` for back-off.
std::string set_summary(const WorkSet& set);

} // namespace wl
