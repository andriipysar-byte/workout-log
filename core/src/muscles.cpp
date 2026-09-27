#include "workoutlog/muscles.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace wl {

double Scores::get(std::string_view muscle) const {
    for (const auto& [k, v] : entries_)
        if (k == muscle) return v;
    return 0;
}

double& Scores::operator[](const std::string& muscle) {
    for (auto& [k, v] : entries_)
        if (k == muscle) return v;
    entries_.emplace_back(muscle, 0.0);
    return entries_.back().second;
}

std::string_view weighting_wire(WeightingMode mode) {
    switch (mode) {
        case WeightingMode::set_count: return "set_count";
        case WeightingMode::rep_volume: return "rep_volume";
        case WeightingMode::tonnage: return "tonnage";
    }
    return "";
}

std::string_view weighting_label(WeightingMode mode) {
    switch (mode) {
        case WeightingMode::set_count: return "Sets";
        case WeightingMode::rep_volume: return "Reps";
        case WeightingMode::tonnage: return "Tonnage";
    }
    return "";
}

std::optional<WeightingMode> weighting_from_wire(std::string_view wire) {
    for (auto mode : kWeightingModes)
        if (weighting_wire(mode) == wire) return mode;
    return std::nullopt;
}

namespace muscle_activation {

namespace {

constexpr double kPrimary = 1.0;
constexpr double kSecondary = 0.5;

Scores normalize(Scores raw) {
    if (raw.empty()) return raw;
    double peak = -std::numeric_limits<double>::infinity();
    for (const auto& [_, v] : raw) peak = std::max(peak, v);
    if (peak <= 0) return raw;
    for (auto& [_, v] : raw) v /= peak;
    return raw;
}

double reps(const WorkSet& set) { return static_cast<double>(set.recorded_reps().value_or(0)); }

double strength_volume(const std::vector<WorkSet>& sets, WeightingMode mode) {
    double sum = 0;
    switch (mode) {
        case WeightingMode::set_count: return static_cast<double>(sets.size());
        case WeightingMode::rep_volume:
            for (const auto& s : sets) sum += reps(s);
            return sum;
        case WeightingMode::tonnage:
            for (const auto& s : sets) sum += reps(s) * s.weight_kg.value_or(0);
            return sum;
    }
    return 0;
}

double metcon_volume(const MetconBlock& block, const MetconExercise& exercise, WeightingMode mode) {
    IntList scheme = exercise.scheme_within(block.scheme);
    size_t rounds = block.rounds ? block.rounds->size() : scheme.size();
    std::int64_t scheme_reps = 0;
    for (auto r : scheme) scheme_reps += r;
    switch (mode) {
        case WeightingMode::set_count:
            return static_cast<double>(std::max(rounds, scheme.empty() ? size_t{1} : scheme.size()));
        case WeightingMode::rep_volume: return static_cast<double>(scheme_reps);
        case WeightingMode::tonnage: return static_cast<double>(scheme_reps) * exercise.weight_kg.value_or(0);
    }
    return 0;
}

void accumulate(const Session& session, Scores& raw, const Catalogue& catalogue, WeightingMode mode) {
    auto add = [&](const Exercise& exercise, double volume) {
        if (volume <= 0) return;
        for (const auto& m : exercise.primary_muscles) raw[m] += volume * kPrimary;
        for (const auto& m : exercise.secondary_muscles) raw[m] += volume * kSecondary;
    };
    for (const auto& block : session.blocks) {
        if (const auto* strength = std::get_if<StrengthBlock>(&block)) {
            if (const Exercise* resolved = catalogue.resolve(strength->exercise))
                add(*resolved, strength_volume(strength->sets, mode));
        } else if (const auto* metcon = std::get_if<MetconBlock>(&block)) {
            for (const auto& entry : metcon->exercises)
                if (const Exercise* resolved = catalogue.resolve(entry.name))
                    add(*resolved, metcon_volume(*metcon, entry, mode));
        }
    }
}

} // namespace

Scores for_exercise(const Exercise& exercise) {
    Scores raw;
    for (const auto& m : exercise.primary_muscles) raw[m] = std::max(raw[m], kPrimary);
    for (const auto& m : exercise.secondary_muscles) raw[m] = std::max(raw[m], kSecondary);
    return normalize(std::move(raw));
}

Scores for_session(const Session& session, const Catalogue& catalogue, WeightingMode mode) {
    Scores raw;
    accumulate(session, raw, catalogue, mode);
    return normalize(std::move(raw));
}

Scores for_sessions(const std::vector<Session>& sessions, const Catalogue& catalogue, WeightingMode mode) {
    Scores raw;
    for (const auto& s : sessions) accumulate(s, raw, catalogue, mode);
    return normalize(std::move(raw));
}

} // namespace muscle_activation

std::string_view group_name(MuscleGroup group) {
    switch (group) {
        case MuscleGroup::chest: return "chest";
        case MuscleGroup::back: return "back";
        case MuscleGroup::shoulders: return "shoulders";
        case MuscleGroup::arms: return "arms";
        case MuscleGroup::legs: return "legs";
        case MuscleGroup::core: return "core";
    }
    return "";
}

std::string group_label(MuscleGroup group) {
    std::string label(group_name(group));
    label[0] = static_cast<char>(label[0] - 32);
    return label;
}

std::string_view group_hex(MuscleGroup group) {
    switch (group) {
        case MuscleGroup::chest: return "#d1495b";
        case MuscleGroup::back: return "#00798c";
        case MuscleGroup::shoulders: return "#edae49";
        case MuscleGroup::arms: return "#8e5ea2";
        case MuscleGroup::legs: return "#30638e";
        case MuscleGroup::core: return "#58a65c";
    }
    return "";
}

std::optional<MuscleGroup> group_of(std::string_view m) {
    if (m == "chest") return MuscleGroup::chest;
    if (m == "lats" || m == "rhomboids" || m == "traps" || m == "spinal_erectors") return MuscleGroup::back;
    if (m == "front_delts" || m == "side_delts" || m == "rear_delts") return MuscleGroup::shoulders;
    if (m == "biceps" || m == "triceps" || m == "forearms") return MuscleGroup::arms;
    if (m == "quads" || m == "hamstrings" || m == "glutes" || m == "calves" || m == "adductors" || m == "hip_flexors")
        return MuscleGroup::legs;
    if (m == "abs" || m == "obliques") return MuscleGroup::core;
    return std::nullopt;
}

std::optional<MuscleGroup> dominant_group(const Scores& scores) {
    double by_group[6] = {};
    bool seen[6] = {};
    for (const auto& [muscle, score] : scores) {
        auto group = group_of(muscle);
        if (!group) continue;
        by_group[static_cast<int>(*group)] += score;
        seen[static_cast<int>(*group)] = true;
    }
    std::optional<MuscleGroup> best;
    double best_score = -std::numeric_limits<double>::infinity();
    for (auto group : kMuscleGroups) {
        int i = static_cast<int>(group);
        if (seen[i] && by_group[i] > best_score) {
            best = group;
            best_score = by_group[i];
        }
    }
    return best;
}

namespace muscle_map_svg {

namespace {

struct Rgb {
    int r, g, b;
};

Rgb rgb(std::string_view hex) {
    if (!hex.empty() && hex.front() == '#') hex.remove_prefix(1);
    unsigned v = static_cast<unsigned>(std::stoul(std::string(hex), nullptr, 16));
    return {static_cast<int>((v >> 16) & 0xff), static_cast<int>((v >> 8) & 0xff), static_cast<int>(v & 0xff)};
}

int lerp(int a, int b, double t) { return static_cast<int>(std::lround(a + (b - a) * t)); }

bool is_token_char(char c) { return (c >= 'a' && c <= 'z') || c == '_'; }

} // namespace

std::string color(double score) {
    if (score <= 0) return std::string(kZero);
    double t = std::clamp(score, 0.0, 1.0);
    Rgb lo = rgb(kLow), hi = rgb(kHigh);
    char buf[8];
    std::snprintf(buf, sizeof buf, "#%02x%02x%02x", lerp(lo.r, hi.r, t), lerp(lo.g, hi.g, t), lerp(lo.b, hi.b, t));
    return buf;
}

std::string colorize(std::string_view tmpl, const Scores& scores) {
    static constexpr std::string_view kAttr = "data-muscle=\"";
    std::string out;
    out.reserve(tmpl.size() + 4096);
    size_t cursor = 0, search = 0;
    while (true) {
        size_t at = tmpl.find(kAttr, search);
        if (at == std::string_view::npos) break;
        size_t token_start = at + kAttr.size();
        size_t i = token_start;
        while (i < tmpl.size() && is_token_char(tmpl[i])) ++i;
        if (i == token_start || i >= tmpl.size() || tmpl[i] != '"') {
            search = at + 1;
            continue;
        }
        size_t match_end = i + 1;
        std::string_view token = tmpl.substr(token_start, i - token_start);
        out.append(tmpl.substr(cursor, match_end - cursor));
        out += " style=\"fill:" + color(scores.get(token)) + "\"";
        cursor = search = match_end;
    }
    out.append(tmpl.substr(cursor));
    return out;
}

} // namespace muscle_map_svg

} // namespace wl
