#include "workoutlog/validation.hpp"

#include <algorithm>
#include <numeric>

#include "workoutlog/cycle_planning.hpp"
#include "workoutlog/date.hpp"
#include "workoutlog/utf8.hpp"
#include "workoutlog/wall_clock.hpp"

namespace wl {

Json ValidationIssue::to_json() const {
    Json json = Json::object();
    json["severity"] = severity == IssueSeverity::error ? "error" : "warning";
    json["path"] = path;
    json["message"] = message;
    return json;
}

bool has_errors(const std::vector<ValidationIssue>& issues) {
    return std::any_of(issues.begin(), issues.end(),
                       [](const auto& i) { return i.severity == IssueSeverity::error; });
}

namespace {

struct Checker {
    const Catalogue* catalogue;
    std::vector<ValidationIssue> issues;

    void error(std::string path, std::string message) {
        issues.push_back({IssueSeverity::error, std::move(path), std::move(message)});
    }
    void warning(std::string path, std::string message) {
        issues.push_back({IssueSeverity::warning, std::move(path), std::move(message)});
    }

    // Dart interpolates a null as the word "null".
    static std::string text(const std::optional<std::string>& s) { return s ? *s : "null"; }

    std::optional<int> check_times(const Block& block, const std::string& path, std::optional<int> previous_end) {
        auto start = block_start_time(block);
        auto end = block_end_time(block);
        for (auto [key, value] : {std::pair{"start_time", &start}, std::pair{"end_time", &end}})
            if (!wall_clock::is_valid(*value))
                error(path + "." + key, "\"" + text(*value) + "\" is not a wall-clock time (HH:MM)");
        auto start_min = wall_clock::minutes(start);
        auto end_min = wall_clock::minutes(end);
        if (start_min && end_min && *end_min < *start_min) warning(path + ".end_time", "ends before it starts");
        auto first_min = start_min ? start_min : end_min;
        if (previous_end && first_min && *first_min < *previous_end)
            warning(path, "starts before the previous block ended (" + wall_clock::format(*first_min) + " after " +
                              wall_clock::format(*previous_end) + ")");
        return end_min;
    }

    void check_known(const std::string& name, const std::string& path) {
        if (!catalogue || utf8::trim(name).empty()) return;
        if (catalogue->resolve(name)) return;
        warning(path, "\"" + name +
                          "\" is in no catalogue entry — add it to exercises.json or it contributes nothing "
                          "to the muscle map");
    }

    void check(const CardioBlock& b, const std::string& path) {
        if (utf8::trim(b.machine).empty()) warning(path + ".machine", "no machine recorded");
        if (b.duration_min.value_or(1) <= 0) error(path + ".duration_min", "must be positive");
        if (b.distance_m.value_or(1) <= 0) error(path + ".distance_m", "must be positive");
    }

    void check(const StrengthBlock& b, const std::string& path) {
        if (utf8::trim(b.exercise).empty())
            error(path + ".exercise", "is empty");
        else
            check_known(b.exercise, path + ".exercise");
        if (b.sets.empty()) warning(path + ".sets", "no sets recorded for \"" + b.exercise + "\"");
        for (size_t s = 0; s < b.sets.size(); ++s) {
            const WorkSet& set = b.sets[s];
            std::string set_path = path + ".sets[" + std::to_string(s) + "]";
            if (set.weight_kg.value_or(0) < 0) error(set_path + ".weight_kg", "is negative");
            if (set.reps.value_or(1) <= 0) error(set_path + ".reps", "must be positive");
            if (set.duration_sec.value_or(1) <= 0) error(set_path + ".duration_sec", "must be positive");
            if (set.cluster && std::any_of(set.cluster->begin(), set.cluster->end(), [](auto r) { return r <= 0; }))
                error(set_path + ".cluster", "every rep in the chain must be positive");
            if (set.cluster && set.total_reps) {
                auto sum = std::accumulate(set.cluster->begin(), set.cluster->end(), std::int64_t{0});
                if (sum != *set.total_reps)
                    warning(set_path + ".total_reps", std::to_string(*set.total_reps) +
                                                          " does not match the chain sum " + std::to_string(sum));
            }
        }
    }

    void check(const MetconBlock& b, const std::string& path) {
        if (b.exercises.empty()) warning(path + ".exercises", "the metcon has no movements");
        for (size_t e = 0; e < b.exercises.size(); ++e)
            check_known(b.exercises[e].name, path + ".exercises[" + std::to_string(e) + "].name");
        if (b.scheme && b.rounds && b.rounds->size() > b.scheme->size())
            warning(path + ".rounds", std::to_string(b.rounds->size()) + " rounds recorded against a " +
                                          std::to_string(b.scheme->size()) + "-round scheme");
        if (!b.rounds) return;
        std::optional<double> previous_split;
        for (size_t r = 0; r < b.rounds->size(); ++r) {
            const MetconRound& round = (*b.rounds)[r];
            std::string round_path = path + ".rounds[" + std::to_string(r) + "]";
            auto split = round.split_cumulative_sec;
            if (split && previous_split && *split < *previous_split)
                warning(round_path + ".split_cumulative_sec",
                        "goes backwards: " + dart_double(*split) + " after " + dart_double(*previous_split) +
                            " — splits are cumulative, not per round");
            if (split) previous_split = split;
            if (round.round != static_cast<std::int64_t>(r) + 1)
                warning(round_path + ".round",
                        "is " + std::to_string(round.round) + " at position " + std::to_string(r + 1));
        }
    }

    void check(const CooldownBlock&, const std::string&) {}
};

} // namespace

std::vector<ValidationIssue> validate_session(const Session& session, const Catalogue* catalogue) {
    Checker c{catalogue, {}};

    // Dart's DateTime.tryParse rolls 2026-02-30 over rather than rejecting it, so
    // the shape is the whole check.
    if (!is_iso_date_shape(session.date))
        c.error("date", "\"" + session.date + "\" is not an ISO date (YYYY-MM-DD)");
    if (utf8::trim(session.cycle_day).empty())
        c.error("cycle_day", "is empty");
    else if (!CycleDay::try_parse(session.cycle_day))
        c.warning("cycle_day", "\"" + session.cycle_day + "\" does not follow the A1…F2 convention");
    if (!wall_clock::is_valid(session.start_time))
        c.error("start_time", "\"" + Checker::text(session.start_time) + "\" is not a wall-clock time (HH:MM)");
    if (session.bodyweight_kg.value_or(1) <= 0) c.error("bodyweight_kg", "must be positive");
    if (session.blocks.empty()) c.warning("blocks", "the session is empty");

    auto previous_end = wall_clock::minutes(session.start_time);
    for (size_t i = 0; i < session.blocks.size(); ++i) {
        const Block& block = session.blocks[i];
        std::string path = "blocks[" + std::to_string(i) + "]";
        auto end = c.check_times(block, path, previous_end);
        if (end) previous_end = end;
        std::visit([&](const auto& b) { c.check(b, path); }, block);
    }
    return std::move(c.issues);
}

} // namespace wl
