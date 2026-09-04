#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

// Port of app/Sources/WorkoutLogCore/Analytics/MuscleGroup.swift. Coarse categorical
// grouping of the fine-grained catalogue muscles, each with a stable colour code.
// Distinct from the sequential heat-map ramp (MuscleMapSVG): this answers "which
// region" (categorical), not "how hard" (intensity).
namespace workoutlog {

enum class MuscleGroup { chest, back, shoulders, arms, legs, core };

// Declaration order, used as the deterministic tie-break in `dominant`.
inline constexpr MuscleGroup kMuscleGroupOrder[] = {
    MuscleGroup::chest, MuscleGroup::back, MuscleGroup::shoulders,
    MuscleGroup::arms,  MuscleGroup::legs, MuscleGroup::core,
};

std::string label(MuscleGroup);
std::string hex(MuscleGroup);

std::optional<MuscleGroup> muscle_group_of(std::string_view muscle);

// The group carrying the most activation across a [muscle: score] map. Swift's
// `Dictionary.max` breaks ties by randomised hash-iteration order (non-deterministic
// across runs); this deliberately breaks ties by declaration order instead
// (`kMuscleGroupOrder`), a documented divergence rather than a port error.
std::optional<MuscleGroup> dominant(const std::map<std::string, double>& scores);

} // namespace workoutlog
