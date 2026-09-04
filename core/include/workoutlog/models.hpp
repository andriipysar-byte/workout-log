#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

// Port of app/Sources/WorkoutLogCore/Models/*.swift. Files on disk are the source of
// truth (ADR-001); this is a lossless projection, same shape as the Swift structs.
namespace workoutlog {

enum class Kind { training, deload, retest };
enum class RepBand { heavy, base, volume };   // heavy <=3, base 4-6, volume 7+
enum class BarSpeed { fast, ok, slow, grind };
enum class MetconFormat { for_time, amrap, emom, intervals, ladder, chipper };
enum class ExerciseCategory { strength, power, speed, longevity };
enum class Pattern { squat, hinge, press, pull, olympic, carry, core, grip };
enum class Modality { barbell, dumbbell, kettlebell, bodyweight, machine };

std::string to_string(Kind);
std::string to_string(RepBand);
std::string to_string(BarSpeed);
std::string to_string(MetconFormat);
std::string to_string(ExerciseCategory);
std::string to_string(Pattern);
std::string to_string(Modality);

// Each returns nullopt for an unrecognised raw value (caller decides whether that's
// an error, matching Swift's throwing RawRepresentable init behaving as "invalid").
std::optional<Kind> kind_from_string(std::string_view);
std::optional<RepBand> rep_band_from_string(std::string_view);
std::optional<BarSpeed> bar_speed_from_string(std::string_view);
std::optional<MetconFormat> metcon_format_from_string(std::string_view);
std::optional<ExerciseCategory> exercise_category_from_string(std::string_view);
std::optional<Pattern> pattern_from_string(std::string_view);
std::optional<Modality> modality_from_string(std::string_view);

// Named WorkSet (not Set) for the same reason as the Swift original: avoid clashing
// with the standard library. All fields optional so a weightless / bodyweight / timed
// set round-trips without inventing values.
struct WorkSet {
    std::optional<double> weight_kg;
    std::optional<int> reps;
    std::optional<double> duration_sec;
    std::optional<std::vector<int>> cluster;
    std::optional<int> total_reps;
    std::optional<double> rir;
    std::optional<RepBand> rep_band;
    std::optional<BarSpeed> bar_speed;
    std::optional<bool> is_backoff;
    std::optional<int> planned_reps;
    std::optional<std::string> notes;

    bool operator==(const WorkSet&) const = default;
};

// Every block carries end_time -- the bracket timestamp from the paper log -- which is
// what makes duration, transition time and session density computable.
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
    std::optional<double> weight_kg;
    std::optional<std::vector<int>> reps_override;

    bool operator==(const MetconExercise&) const = default;
};

// Splits are stored cumulative (as on paper); per-round splits are derived at read
// time, never stored.
struct MetconRound {
    int round = 0;
    std::optional<int> reps;
    std::optional<double> split_cumulative_sec;
    std::optional<int> heart_rate;

    bool operator==(const MetconRound&) const = default;
};

struct MetconBlock {
    std::optional<MetconFormat> format;
    std::optional<std::vector<int>> scheme;
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

// Flat tagged union on "type", same shape as Swift's `enum Block`.
using Block = std::variant<CardioBlock, StrengthBlock, MetconBlock, CooldownBlock>;

struct Session {
    std::string date;
    std::string cycle_day;
    std::optional<std::string> start_time;
    Kind kind = Kind::training;
    std::optional<double> bodyweight_kg;
    std::optional<std::string> notes;
    std::vector<Block> blocks;

    bool operator==(const Session&) const = default;
};

// pattern groups variants of one movement so conjugate rotation reads as progress
// rather than scatter (P8, docs/01-training-principles.md).
struct Exercise {
    std::string name;   // canonical, Ukrainian
    std::vector<std::string> aliases;
    std::optional<Pattern> pattern;
    std::optional<Modality> modality;
    ExerciseCategory category = ExerciseCategory::strength;
    std::vector<std::string> primary_muscles;
    std::vector<std::string> secondary_muscles;
    std::optional<std::string> notes;

    // The P3 bar-speed rule applies to explosive movements -- both power and speed work.
    bool is_explosive() const { return category == ExerciseCategory::power || category == ExerciseCategory::speed; }

    bool operator==(const Exercise&) const = default;
};

struct Catalogue {
    std::vector<Exercise> exercises;

    bool operator==(const Catalogue&) const = default;
};

} // namespace workoutlog
