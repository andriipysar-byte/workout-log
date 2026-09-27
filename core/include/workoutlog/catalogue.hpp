#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "workoutlog/enums.hpp"
#include "workoutlog/json.hpp"

namespace wl {

// exercises.json is hand-maintained and written back by the app, so every key
// the model does not know is carried in `extras`, and `present_keys` remembers
// which empty lists the file spelled out — dropping either would delete it on save.
struct Exercise {
    std::string name;
    std::vector<std::string> aliases;
    std::optional<MovementPattern> pattern;
    std::optional<Modality> modality;
    ExerciseCategory category = ExerciseCategory::strength;
    std::vector<std::string> primary_muscles;
    std::vector<std::string> secondary_muscles;
    std::optional<std::string> notes;
    Json extras = Json::object();
    std::set<std::string> present_keys;

    bool is_explosive() const {
        return category == ExerciseCategory::power || category == ExerciseCategory::speed;
    }

    static Exercise from_json(const Json& json);
    Json to_json() const;
};

struct Catalogue {
    std::vector<Exercise> exercises;
    std::optional<std::string> comment;

    // Appends, or replaces the entry with the same canonical name (case-insensitive).
    Catalogue with_exercise(Exercise exercise) const;
    // Every muscle token in use, sorted: the vocabulary a new exercise picks from.
    std::vector<std::string> known_muscles() const;
    // Canonical names and aliases, case-insensitive; null when unknown (callers
    // warn, never block).
    const Exercise* resolve(std::string_view typed) const;

    static Catalogue from_json(const Json& json);
    Json to_json() const;
};

Catalogue decode_catalogue(std::string_view text);

} // namespace wl
