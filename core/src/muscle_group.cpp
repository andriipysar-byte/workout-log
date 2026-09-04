#include "workoutlog/muscle_group.hpp"

namespace workoutlog {

std::string label(MuscleGroup g) {
    switch (g) {
        case MuscleGroup::chest: return "Chest";
        case MuscleGroup::back: return "Back";
        case MuscleGroup::shoulders: return "Shoulders";
        case MuscleGroup::arms: return "Arms";
        case MuscleGroup::legs: return "Legs";
        case MuscleGroup::core: return "Core";
    }
    return {};
}

std::string hex(MuscleGroup g) {
    switch (g) {
        case MuscleGroup::chest: return "#d1495b";
        case MuscleGroup::back: return "#00798c";
        case MuscleGroup::shoulders: return "#edae49";
        case MuscleGroup::arms: return "#8e5ea2";
        case MuscleGroup::legs: return "#30638e";
        case MuscleGroup::core: return "#58a65c";
    }
    return {};
}

std::optional<MuscleGroup> muscle_group_of(std::string_view muscle) {
    if (muscle == "chest") return MuscleGroup::chest;
    if (muscle == "lats" || muscle == "rhomboids" || muscle == "traps" || muscle == "spinal_erectors")
        return MuscleGroup::back;
    if (muscle == "front_delts" || muscle == "side_delts" || muscle == "rear_delts")
        return MuscleGroup::shoulders;
    if (muscle == "biceps" || muscle == "triceps" || muscle == "forearms") return MuscleGroup::arms;
    if (muscle == "quads" || muscle == "hamstrings" || muscle == "glutes" || muscle == "calves" ||
        muscle == "adductors" || muscle == "hip_flexors")
        return MuscleGroup::legs;
    if (muscle == "abs" || muscle == "obliques") return MuscleGroup::core;
    return std::nullopt;
}

std::optional<MuscleGroup> dominant(const std::map<std::string, double>& scores) {
    std::map<MuscleGroup, double> by_group;
    for (const auto& [muscle, score] : scores) {
        auto g = muscle_group_of(muscle);
        if (!g) continue;
        by_group[*g] += score;
    }
    if (by_group.empty()) return std::nullopt;

    std::optional<MuscleGroup> best;
    double best_score = 0;
    for (MuscleGroup g : kMuscleGroupOrder) {
        auto it = by_group.find(g);
        if (it == by_group.end()) continue;
        if (!best || it->second > best_score) {
            best = g;
            best_score = it->second;
        }
    }
    return best;
}

} // namespace workoutlog
