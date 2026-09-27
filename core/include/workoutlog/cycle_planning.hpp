#pragma once

#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "workoutlog/cycle.hpp"
#include "workoutlog/date.hpp"
#include "workoutlog/models.hpp"

namespace wl {

enum class DayKind { conditioning = 1, heavy = 2 };

std::string_view day_kind_label(DayKind kind);

// A workout code such as `A1`: the letter groups workouts by emphasis, the
// number says which kind of day it is.
struct CycleDay {
    char letter = 'A';
    DayKind kind = DayKind::conditioning;

    // Null rather than throwing: a hand-written cycle may break the convention,
    // and reading it must never fail.
    static std::optional<CycleDay> try_parse(std::string_view code);

    std::string code() const;
    // A1 → A2 → B1: the next slot in the progression.
    CycleDay next() const;

    auto operator<=>(const CycleDay& other) const { return code() <=> other.code(); }
    bool operator==(const CycleDay&) const = default;
};

struct CycleGeneratorError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

inline constexpr std::array<std::string_view, 7> kWeekdayAbbreviations{"Mon", "Tue", "Wed", "Thu",
                                                                      "Fri", "Sat", "Sun"};

std::string_view weekday_abbreviation(Date date);

// The first `count` dates on or after `start` whose weekday is a training day.
std::vector<Date> training_dates(Date start, const std::vector<std::string>& training_days, size_t count);

// Expands a cycle onto the calendar: one session stub per template. The
// planning-only `role` and `sets_reps` are consumed here and never written.
std::vector<Session> generate_cycle(const Cycle& cycle);

// `enforce_weekday` separates the callers: expanding a whole cycle must fail
// loudly when the calendar has drifted, but one session created on a day that
// suits the athlete is not an error.
Session session_from_template(const CycleSession& tmpl, Date when, bool enforce_weekday = true);

Block block_from_template(const BlockTemplate& tmpl);

// For display only: blank blocks are skipped rather than failing, and a chosen
// main lift with no planned sets counts as one set so it shows on the map.
Session preview_session(const CycleSession& tmpl, std::optional<Date> on = std::nullopt);

namespace cycle_templates {

std::vector<BlockTemplate> blocks_for(DayKind kind);
CycleSession session(CycleDay day, std::optional<std::int64_t> week = std::nullopt,
                     std::optional<std::string> weekday = std::nullopt);
// One past the highest conventional code, or A1 for an empty or free-form cycle.
CycleDay next_day(const std::vector<std::string>& existing_codes);

} // namespace cycle_templates

} // namespace wl
