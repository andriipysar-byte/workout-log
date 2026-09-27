#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "workoutlog/catalogue.hpp"
#include "workoutlog/models.hpp"

// Derivations over sessions. Nothing here is stored — the file holds only what
// was observed (ADR-001). The system reports; programming decisions stay the
// athlete's.
namespace wl {

// Which energy system a metcon taxed, by total duration (P9) — the clock, not
// the work intervals, which live in the block's notes.
enum class TimeDomain { alactic, glycolytic, aerobic };
std::optional<TimeDomain> time_domain_for(std::optional<double> seconds);
std::string_view time_domain_name(TimeDomain domain);

using BandCounts = std::map<RepBand, std::int64_t>;

// Work time against clock time, from the bracket timestamps (P1).
struct SessionDensity {
    double work_min = 0;
    double transition_min = 0;
    // Null when nothing was written down to derive it from — never guessed.
    std::optional<double> total_min;

    std::optional<double> work_share() const;
    Json to_json() const;
};

struct BlockMetrics {
    size_t index = 0;
    std::string type;
    std::optional<std::string> label;
    std::optional<double> duration_min;
    std::optional<double> transition_min;
    std::int64_t sets = 0;
    std::int64_t backoff_sets = 0;
    std::int64_t reps = 0;
    double tonnage_kg = 0;
    // The band most of the slot's sets sit in (P6).
    std::optional<RepBand> band;

    Json to_json() const;
};

struct RoundSplit {
    std::int64_t round = 0;
    std::optional<std::int64_t> reps;
    std::optional<double> cumulative_sec;
    std::optional<double> split_sec;
    std::optional<std::int64_t> heart_rate;

    std::optional<double> seconds_per_rep() const;
    Json to_json() const;
};

struct MetconMetrics {
    size_t block_index = 0;
    std::optional<std::string> format;
    std::optional<IntList> scheme;
    std::vector<std::string> exercises;
    std::optional<double> total_sec;
    std::vector<RoundSplit> rounds;

    std::optional<TimeDomain> domain() const { return time_domain_for(total_sec); }
    // Last round against first, per rep, as a percentage (positive = faded).
    std::optional<double> pace_decay_percent() const;
    std::optional<std::int64_t> peak_heart_rate() const;
    Json to_json() const;
};

struct SessionMetrics {
    std::string date;
    std::string cycle_day;
    Kind kind = Kind::training;
    std::vector<BlockMetrics> blocks;
    std::vector<MetconMetrics> metcons;
    SessionDensity density;
    double tonnage_kg = 0;
    std::int64_t sets = 0;
    std::int64_t backoff_sets = 0;
    std::int64_t reps = 0;
    // Both matter: P6 tracks sets, P4's "2-3 volume slots per cycle" counts slots.
    BandCounts band_sets;
    BandCounts band_slots;

    // The top set is the signal, back-offs are volume (P2): never averaged together.
    std::int64_t top_sets() const { return sets - backoff_sets; }

    static SessionMetrics of(const Session& session);
    Json to_json() const;
};

struct TrainingAlert {
    std::string principle; // P3, P5, P8, P10 — docs/01-training-principles.md
    std::string message;
    std::vector<std::string> sessions;
    Json to_json() const;
};

// Below roughly one slot per cycle, linear progression on a pattern is dead (P8).
struct PatternFrequency {
    MovementPattern pattern;
    std::int64_t slots;
    std::int64_t sessions;
    double per_cycle;
    Json to_json() const;
};

// Strength and conditioning side by side, never fused into one score (P7).
struct LoadPoint {
    std::string date;
    std::string cycle_day;
    Kind kind;
    double tonnage_kg;
    std::int64_t sets;
    std::optional<double> metcon_sec;
    std::optional<double> work_min;
    Json to_json() const;
};

struct TrainingReportOptions {
    std::int64_t cycle_length_days = 12;
    // Guards P5, the failure that actually happened: a block where volume slots
    // took over left nothing light to recover against and ended in an
    // involuntary deload. P4 rations volume to 2-3 slots per cycle.
    double volume_slot_share_threshold = 0.3;
};

struct TrainingReport {
    std::optional<std::string> from;
    std::optional<std::string> to;
    std::optional<std::int64_t> days;
    std::int64_t cycle_length_days = 12;
    std::int64_t session_count = 0;
    std::map<Kind, std::int64_t> kinds;
    BandCounts band_sets;
    BandCounts band_slots;
    std::vector<PatternFrequency> patterns;
    std::vector<LoadPoint> load;
    std::map<TimeDomain, std::int64_t> time_domains;
    std::vector<TrainingAlert> alerts;
    // Surfaced, never silently dropped (ADR-006).
    std::vector<std::string> unknown_exercises;

    double total_tonnage_kg() const;

    static TrainingReport of(const std::vector<Session>& sessions, const Catalogue& catalogue,
                             TrainingReportOptions options = {});
    Json to_json() const;
};

// A trend line, never a truth: both formulas drift badly above ~10 reps.
namespace one_rep_max {
std::optional<double> epley(std::optional<double> weight_kg, std::optional<std::int64_t> reps);
std::optional<double> brzycki(std::optional<double> weight_kg, std::optional<std::int64_t> reps);
// The mean of the two: they bracket the real number from either side.
std::optional<double> estimate(std::optional<double> weight_kg, std::optional<std::int64_t> reps);
} // namespace one_rep_max

struct ProgressPoint {
    std::string date;
    std::string cycle_day;
    Kind kind = Kind::training;
    // The top set: heaviest non-back-off set of the slot (P2).
    std::optional<double> weight_kg;
    std::optional<std::int64_t> reps;
    std::optional<double> rir;
    std::optional<BarSpeed> bar_speed;
    std::int64_t backoff_sets = 0;
    double backoff_tonnage_kg = 0;
    std::int64_t sets = 0;

    std::optional<double> estimated_1rm_kg() const { return one_rep_max::estimate(weight_kg, reps); }
    Json to_json() const;
};

// Rep bands are independent tracks on one lift (P6).
struct ProgressSeries {
    std::string exercise;
    std::optional<RepBand> band;
    std::vector<ProgressPoint> points;

    std::optional<double> trend_kg() const;
    Json to_json() const;
};

struct ProgressReport {
    std::string query;
    std::optional<std::string> resolved;
    std::optional<MovementPattern> pattern;
    std::vector<std::string> variants;
    std::vector<ProgressSeries> series;

    // `by_pattern` widens "this lift" to "this pattern" — the conjugate view P8
    // asks for, where variants are one thing rather than sparse separate lines.
    static ProgressReport of(const std::vector<Session>& sessions, const std::string& exercise,
                             const Catalogue* catalogue, bool by_pattern = false);
    Json to_json() const;
};

} // namespace wl
