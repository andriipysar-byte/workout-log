#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "workoutlog/catalogue.hpp"
#include "workoutlog/models.hpp"

namespace wl {

// `[muscle: score]` in first-seen order. The order is part of the output (the
// MCP server prints it) and the summation order fixes the exact doubles.
class Scores {
public:
    double get(std::string_view muscle) const;
    double& operator[](const std::string& muscle);
    bool empty() const { return entries_.empty(); }
    size_t size() const { return entries_.size(); }
    auto begin() const { return entries_.begin(); }
    auto end() const { return entries_.end(); }
    auto begin() { return entries_.begin(); }
    auto end() { return entries_.end(); }

private:
    std::vector<std::pair<std::string, double>> entries_;
};

// The three modes deliberately surface different hottest muscles.
enum class WeightingMode { set_count, rep_volume, tonnage };

std::string_view weighting_wire(WeightingMode mode);
std::string_view weighting_label(WeightingMode mode);
std::optional<WeightingMode> weighting_from_wire(std::string_view wire);
inline constexpr WeightingMode kWeightingModes[] = {WeightingMode::set_count, WeightingMode::rep_volume,
                                                   WeightingMode::tonnage};

// Scores normalized so the most-worked muscle is 1.0. Primary muscles weigh 1.0,
// secondary 0.5; unknown exercise names contribute nothing (warn, never block).
namespace muscle_activation {

Scores for_exercise(const Exercise& exercise);
Scores for_session(const Session& session, const Catalogue& catalogue, WeightingMode mode);
// Raw volumes sum across sessions and normalize once, so a busy day cannot drown
// a light one the way summing already-normalized maps would.
Scores for_sessions(const std::vector<Session>& sessions, const Catalogue& catalogue, WeightingMode mode);

} // namespace muscle_activation

// Coarse categorical regions with stable colours — "which region", distinct from
// the heat ramp's "how hard".
enum class MuscleGroup { chest, back, shoulders, arms, legs, core };
inline constexpr MuscleGroup kMuscleGroups[] = {MuscleGroup::chest, MuscleGroup::back, MuscleGroup::shoulders,
                                               MuscleGroup::arms,  MuscleGroup::legs, MuscleGroup::core};

std::string_view group_name(MuscleGroup group);
std::string group_label(MuscleGroup group);
std::string_view group_hex(MuscleGroup group);
std::optional<MuscleGroup> group_of(std::string_view muscle);
// Ties resolve by declaration order so a balanced day's badge does not flicker.
std::optional<MuscleGroup> dominant_group(const Scores& scores);

namespace muscle_map_svg {

inline constexpr std::string_view kLow = "#86b6ef";
inline constexpr std::string_view kHigh = "#0d366b";
inline constexpr std::string_view kZero = "#e8e8e3";

// Every element tagged `data-muscle="<token>"` gets an inline fill from its
// score; the rest of the template passes through byte for byte.
std::string colorize(std::string_view svg_template, const Scores& scores);
// Score ≤ 0 is the neutral zero colour, so "unworked" differs from "barely worked".
std::string color(double score);

} // namespace muscle_map_svg

} // namespace wl
