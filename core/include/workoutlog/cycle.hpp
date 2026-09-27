#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "workoutlog/models.hpp"

// A reusable cycle: ordered session templates with no weights. `role` and
// `sets_reps` are planning-only and never reach a session file.
//
// The planner writes cycles.json back, so every level keeps the keys it does
// not model (`skipped`, `role`, a `$comment`) — anything dropped on read would
// be deleted from the user's file on the next save.
namespace wl {

struct BlockTemplate {
    std::string type;
    std::optional<std::string> role;
    std::optional<std::string> machine;
    std::optional<double> duration_min;
    std::optional<std::string> exercise;
    IntList sets_reps;
    std::optional<std::string> notes;
    std::optional<MetconFormat> format;
    std::optional<IntList> scheme;
    std::vector<MetconExercise> exercises;
    Json extras = Json::object();
    std::set<std::string> present_keys;

    static BlockTemplate from_json(const Json& json);
    Json to_json() const;
};

struct CycleSession {
    std::string cycle_day;
    std::optional<std::int64_t> week;
    std::optional<std::string> weekday;
    std::optional<std::string> type;
    std::optional<std::string> title;
    std::optional<std::string> session_notes;
    std::vector<BlockTemplate> blocks;
    Json extras = Json::object();

    static CycleSession from_json(const Json& json);
    Json to_json() const;
};

// Versions share an id: a revised scheme is a new version, not a new cycle, and
// the earlier version stays in the file because logged sessions came from it.
struct Cycle {
    std::string id;
    std::string name;
    std::optional<std::int64_t> version;
    std::vector<std::string> training_days;
    std::string start_date;
    std::vector<CycleSession> sessions;
    Json extras = Json::object();

    // A cycle written before versioning is its own first version.
    std::int64_t version_number() const { return version.value_or(1); }

    static Cycle from_json(const Json& json);
    Json to_json() const;
};

struct CycleCatalogue {
    std::vector<Cycle> cycles;
    std::optional<std::string> comment;

    // The latest version of `id`.
    const Cycle* by_id(std::string_view id) const;
    Cycle* by_id(std::string_view id);
    const Cycle* by_id(std::string_view id, std::int64_t version) const;
    std::int64_t next_version(std::string_view id) const;

    static CycleCatalogue from_json(const Json& json);
    Json to_json() const;
};

CycleCatalogue decode_cycles(std::string_view text);

} // namespace wl
