#pragma once

#include <map>
#include <span>
#include <string>

#include "workoutlog/models.hpp"

// Port of app/Sources/WorkoutLogCore/Analytics/MuscleActivation.swift. Produces
// [muscle: score] maps a renderer colours, normalized so the most-worked muscle is
// 1.0.
namespace workoutlog {

// The three modes deliberately surface different hottest muscles.
enum class WeightingMode {
    set_count,  // one point per working set -- robust across modalities
    rep_volume, // sets x reps -- favours high-rep accessory work
    tonnage,    // sets x reps x weight -- 0 for bodyweight/timed work
};

// Declaration order, for the UI's mode picker to iterate -- the equivalent of
// Swift's WeightingMode.allCases, and the same pattern as kMuscleGroupOrder in
// muscle_group.hpp.
inline constexpr WeightingMode kWeightingModeOrder[] = {
    WeightingMode::set_count,
    WeightingMode::rep_volume,
    WeightingMode::tonnage,
};

std::string to_string(WeightingMode);
std::string label(WeightingMode);

class MuscleActivation {
public:
    explicit MuscleActivation(double primary_weight = 1.0, double secondary_weight = 0.5)
        : primary_weight_(primary_weight), secondary_weight_(secondary_weight) {}

    // Single exercise: primary muscles at 1.0, secondary at 0.5.
    std::map<std::string, double> for_exercise(const Exercise&) const;

    // Unknown exercise names are skipped (warn-not-block: they contribute nothing).
    std::map<std::string, double> for_session(const Session&, const Catalogue&, WeightingMode) const;

    // Whole-cycle map: raw volumes sum across sessions, normalized once so a busy day
    // can't drown a light one the way summing per-session (already-normalized) maps
    // would.
    std::map<std::string, double> for_sessions(std::span<const Session>, const Catalogue&, WeightingMode) const;

private:
    void accumulate(const Session&, std::map<std::string, double>& raw, const Catalogue&, WeightingMode) const;

    double primary_weight_;
    double secondary_weight_;
};

} // namespace workoutlog
