#include "workoutlog/muscle_activation.hpp"

#include <algorithm>

#include "workoutlog/catalogue.hpp"

namespace workoutlog {

std::string to_string(WeightingMode m) {
    switch (m) {
        case WeightingMode::set_count: return "set_count";
        case WeightingMode::rep_volume: return "rep_volume";
        case WeightingMode::tonnage: return "tonnage";
    }
    return {};
}

std::string label(WeightingMode m) {
    switch (m) {
        case WeightingMode::set_count: return "Sets";
        case WeightingMode::rep_volume: return "Reps";
        case WeightingMode::tonnage: return "Tonnage";
    }
    return {};
}

namespace {

int reps_of(const WorkSet& s) {
    if (s.reps) return *s.reps;
    if (s.total_reps) return *s.total_reps;
    if (s.cluster) {
        int sum = 0;
        for (int r : *s.cluster) sum += r;
        return sum;
    }
    return 0;
}

double strength_volume(const std::vector<WorkSet>& sets, WeightingMode mode) {
    switch (mode) {
        case WeightingMode::set_count:
            return static_cast<double>(sets.size());
        case WeightingMode::rep_volume: {
            double total = 0;
            for (const auto& s : sets) total += reps_of(s);
            return total;
        }
        case WeightingMode::tonnage: {
            double total = 0;
            for (const auto& s : sets) total += reps_of(s) * s.weight_kg.value_or(0.0);
            return total;
        }
    }
    return 0;
}

// Preserves a known quirk of the Swift original: this reads block.scheme / the
// exercise's reps_override, never MetconRound::reps, so an AMRAP's actual completed
// rounds don't feed the map. Not fixed here deliberately -- see docs/05-architecture.md.
double metcon_volume(const MetconBlock& block, const MetconExercise& exercise, WeightingMode mode) {
    std::vector<int> scheme = exercise.reps_override.value_or(block.scheme.value_or(std::vector<int>{}));
    size_t rounds = block.rounds ? block.rounds->size() : scheme.size();

    switch (mode) {
        case WeightingMode::set_count: {
            size_t s = scheme.empty() ? 1 : scheme.size();
            return static_cast<double>(std::max(rounds, s));
        }
        case WeightingMode::rep_volume: {
            double total = 0;
            for (int r : scheme) total += r;
            return total;
        }
        case WeightingMode::tonnage: {
            double sum = 0;
            for (int r : scheme) sum += r;
            return sum * exercise.weight_kg.value_or(0.0);
        }
    }
    return 0;
}

std::map<std::string, double> normalize(std::map<std::string, double> raw) {
    double peak = 0;
    for (const auto& [k, v] : raw) peak = std::max(peak, v);
    if (peak <= 0) return raw;
    for (auto& [k, v] : raw) v /= peak;
    return raw;
}

} // namespace

std::map<std::string, double> MuscleActivation::for_exercise(const Exercise& exercise) const {
    std::map<std::string, double> raw;
    for (const auto& m : exercise.primary_muscles) raw[m] = std::max(raw[m], primary_weight_);
    for (const auto& m : exercise.secondary_muscles) raw[m] = std::max(raw[m], secondary_weight_);
    return normalize(std::move(raw));
}

void MuscleActivation::accumulate(const Session& session, std::map<std::string, double>& raw,
                                   const Catalogue& catalogue, WeightingMode mode) const {
    auto add = [&](const Exercise& exercise, double volume) {
        if (volume <= 0) return;
        for (const auto& m : exercise.primary_muscles) raw[m] += volume * primary_weight_;
        for (const auto& m : exercise.secondary_muscles) raw[m] += volume * secondary_weight_;
    };

    for (const auto& block : session.blocks) {
        if (const auto* s = std::get_if<StrengthBlock>(&block)) {
            const Exercise* ex = catalogue::resolve(catalogue, s->exercise);
            if (!ex) continue;
            add(*ex, strength_volume(s->sets, mode));
        } else if (const auto* m = std::get_if<MetconBlock>(&block)) {
            for (const auto& me : m->exercises) {
                const Exercise* ex = catalogue::resolve(catalogue, me.name);
                if (!ex) continue;
                add(*ex, metcon_volume(*m, me, mode));
            }
        }
        // cardio, cooldown: no muscle contribution.
    }
}

std::map<std::string, double> MuscleActivation::for_session(const Session& session, const Catalogue& catalogue,
                                                              WeightingMode mode) const {
    std::map<std::string, double> raw;
    accumulate(session, raw, catalogue, mode);
    return normalize(std::move(raw));
}

std::map<std::string, double> MuscleActivation::for_sessions(std::span<const Session> sessions,
                                                               const Catalogue& catalogue, WeightingMode mode) const {
    std::map<std::string, double> raw;
    for (const auto& session : sessions) accumulate(session, raw, catalogue, mode);
    return normalize(std::move(raw));
}

} // namespace workoutlog
