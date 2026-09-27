#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "workoutlog/cycle.hpp"
#include "workoutlog/date.hpp"
#include "workoutlog/models.hpp"

// One run of a cycle version with its numbers decided before any day is
// generated: reps and weights per set, rounds and loads per metcon.
//
// The template stays weightless and reusable; each run gets its own file, so a
// calibration run and the progression run after it sit side by side. A run is a
// snapshot: editing the template later does not reach into a run already
// started. Blocks use the session file's own shape, so generating a day is a
// copy, not a translation.
namespace wl {

struct PrescribedDay {
    std::string cycle_day;
    std::string date;
    std::optional<std::string> title;
    std::optional<std::string> notes;
    std::vector<Block> blocks;
    Json extras = Json::object();

    static PrescribedDay from_json(const Json& json);
    Json to_json() const;
};

struct Prescription {
    std::string cycle_id;
    std::int64_t cycle_version = 1;
    std::string start_date;
    std::vector<PrescribedDay> days;
    Json extras = Json::object();

    // `prescriptions/<cycle>-v<version>_<start>.json`, relative to the folder
    // that holds cycles.json.
    std::string file_name() const;

    static Prescription from_json(const Json& json);
    Json to_json() const;
};

inline constexpr std::string_view kPrescriptionDirectory = "prescriptions";

Prescription decode_prescription(std::string_view text);
std::string encode_prescription(const Prescription& prescription);

// Lays the cycle onto the calendar from `start`; throws CycleGeneratorError
// when a template's weekday disagrees with the date it lands on.
Prescription prescribe(const Cycle& cycle, Date start);

Session session_for(const PrescribedDay& day);

// What a day asks for, summed: the full picture before anything is generated.
struct PlannedLoad {
    std::int64_t sets = 0;
    std::int64_t reps = 0;
    std::int64_t metcon_rounds = 0;
    double tonnage_kg = 0;

    PlannedLoad& operator+=(const PlannedLoad& other);
};

PlannedLoad planned_load(const std::vector<Block>& blocks);

// A metcon's rounds as typed: `21-15-9`, `21, 15, 9`, or `8×10` (x, х and *
// work too). Null when the text is not a scheme.
std::optional<IntList> parse_scheme(std::string_view text);
// The inverse: equal rounds print as `8×10`, anything else as `21-15-9`.
std::string format_scheme(const IntList& scheme);

} // namespace wl
