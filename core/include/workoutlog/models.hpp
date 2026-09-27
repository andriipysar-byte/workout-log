#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "workoutlog/enums.hpp"
#include "workoutlog/json.hpp"

// Files on disk are the source of truth (ADR-001); these are lossless projections
// of them, and `to_json` is the only thing that decides what a file looks like.
namespace wl {

using IntList = std::vector<std::int64_t>;

struct WorkSet {
    std::optional<double> weight_kg;
    std::optional<std::int64_t> reps;
    std::optional<double> duration_sec;
    std::optional<IntList> cluster;
    std::optional<std::int64_t> total_reps;
    std::optional<double> rir;
    std::optional<RepBand> rep_band;
    std::optional<BarSpeed> bar_speed;
    std::optional<bool> is_backoff;
    std::optional<std::int64_t> planned_reps;
    std::optional<std::string> notes;

    // However the reps were written down: a count, a cluster total, or the
    // cluster itself. Empty for a hold or an unfilled plan slot.
    std::optional<std::int64_t> recorded_reps() const;
    // What the log says when it says, else what the reps imply (P6).
    std::optional<RepBand> band() const;
    double tonnage_kg() const;
    bool backoff() const { return is_backoff.value_or(false); }

    static WorkSet from_json(const Json& json);
    Json to_json() const;
    bool operator==(const WorkSet&) const = default;
};

std::optional<RepBand> rep_band_for(std::optional<std::int64_t> reps);

struct CardioBlock {
    std::string machine;
    std::optional<double> duration_min;
    std::optional<double> distance_m;
    std::optional<std::string> end_time;
    bool operator==(const CardioBlock&) const = default;
};

struct StrengthBlock {
    std::string exercise;
    std::vector<WorkSet> sets;
    std::optional<std::string> start_time;
    std::optional<std::string> end_time;
    std::optional<std::string> notes;
    bool operator==(const StrengthBlock&) const = default;
};

struct MetconExercise {
    std::string name;
    // The whiteboard prescription ("24kg+24kg", "60 cm"); weight_kg stays the
    // machine-readable load tonnage reads.
    std::optional<std::string> load;
    std::optional<double> weight_kg;
    std::optional<IntList> reps_override;

    std::optional<std::string> prescription() const;
    IntList scheme_within(const std::optional<IntList>& block_scheme) const;

    static MetconExercise from_json(const Json& json);
    Json to_json() const;
    bool operator==(const MetconExercise&) const = default;
};

// Splits are stored cumulative, as on paper; per-round splits are derived.
struct MetconRound {
    std::int64_t round = 0;
    std::optional<std::int64_t> reps;
    std::optional<double> split_cumulative_sec;
    std::optional<std::int64_t> heart_rate;

    static MetconRound from_json(const Json& json);
    Json to_json() const;
    bool operator==(const MetconRound&) const = default;
};

struct MetconBlock {
    std::optional<MetconFormat> format;
    std::optional<IntList> scheme;
    std::vector<MetconExercise> exercises;
    std::optional<std::vector<MetconRound>> rounds;
    std::optional<std::string> start_time;
    std::optional<std::string> end_time;
    std::optional<std::string> notes;
    bool operator==(const MetconBlock&) const = default;
};

struct CooldownBlock {
    std::optional<std::string> end_time;
    std::optional<std::string> notes;
    bool operator==(const CooldownBlock&) const = default;
};

// A flat tagged union on disk: `type` sits beside the payload, not around it.
using Block = std::variant<CardioBlock, StrengthBlock, MetconBlock, CooldownBlock>;

std::string_view block_type(const Block& block);
Block block_from_json(const Json& json);
Json block_to_json(const Block& block);
std::optional<std::string> block_start_time(const Block& block);
std::optional<std::string> block_end_time(const Block& block);

struct Session {
    std::string date;
    std::string cycle_day;
    std::optional<std::string> start_time;
    Kind kind = Kind::training;
    std::optional<double> bodyweight_kg;
    std::optional<std::string> notes;
    std::vector<Block> blocks;

    static Session from_json(const Json& json);
    Json to_json() const;
    bool operator==(const Session&) const = default;
};

Session decode_session(std::string_view text);
std::string encode_session(const Session& session);

std::string metcon_format_wire(MetconFormat format);

} // namespace wl
