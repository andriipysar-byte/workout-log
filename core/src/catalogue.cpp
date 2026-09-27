#include "workoutlog/catalogue.hpp"

#include <algorithm>

#include "workoutlog/utf8.hpp"

namespace wl {

namespace {

Json string_array(const std::vector<std::string>& values) {
    Json out = Json::array();
    for (const auto& v : values) out.push_back(v);
    return out;
}

} // namespace

Exercise Exercise::from_json(const Json& json) {
    as_object(json, "exercise");
    Exercise e;
    e.name = req_string(json, "name");
    e.aliases = string_list(json, "aliases");
    e.pattern = opt_enum<MovementPattern>(json, "pattern");
    e.modality = opt_enum<Modality>(json, "modality");
    e.category = opt_enum<ExerciseCategory>(json, "category").value_or(ExerciseCategory::strength);
    e.primary_muscles = string_list(json, "primary_muscles");
    e.secondary_muscles = string_list(json, "secondary_muscles");
    e.notes = opt_string(json, "notes");
    e.extras = unmodelled_keys(json, {"name", "aliases", "pattern", "modality", "category",
                                      "primary_muscles", "secondary_muscles", "notes"});
    for (auto it = json.begin(); it != json.end(); ++it) e.present_keys.insert(it.key());
    return e;
}

Json Exercise::to_json() const {
    Json json = extras;
    json["name"] = name;
    auto put_list = [&](const char* key, const std::vector<std::string>& value) {
        if (!value.empty() || present_keys.contains(key)) json[key] = string_array(value);
    };
    put_list("aliases", aliases);
    put_enum(json, "pattern", pattern);
    put_enum(json, "modality", modality);
    json["category"] = std::string(wl::name(category));
    json["primary_muscles"] = string_array(primary_muscles);
    put_list("secondary_muscles", secondary_muscles);
    put(json, "notes", notes);
    return json;
}

Catalogue Catalogue::with_exercise(Exercise exercise) const {
    Catalogue next = *this;
    std::string key = utf8::to_lower(exercise.name);
    auto it = std::find_if(next.exercises.begin(), next.exercises.end(),
                           [&](const Exercise& e) { return utf8::to_lower(e.name) == key; });
    if (it != next.exercises.end())
        *it = std::move(exercise);
    else
        next.exercises.push_back(std::move(exercise));
    return next;
}

std::vector<std::string> Catalogue::known_muscles() const {
    std::set<std::string> all;
    for (const auto& e : exercises) {
        all.insert(e.primary_muscles.begin(), e.primary_muscles.end());
        all.insert(e.secondary_muscles.begin(), e.secondary_muscles.end());
    }
    return {all.begin(), all.end()};
}

const Exercise* Catalogue::resolve(std::string_view typed) const {
    std::string key = utf8::to_lower(utf8::trim(typed));
    for (const auto& ex : exercises) {
        if (utf8::to_lower(ex.name) == key) return &ex;
        for (const auto& alias : ex.aliases)
            if (utf8::to_lower(alias) == key) return &ex;
    }
    return nullptr;
}

Catalogue Catalogue::from_json(const Json& json) {
    as_object(json, "catalogue");
    Catalogue c;
    c.comment = opt_string(json, "$comment");
    for (const auto& e : req_array(json, "exercises")) c.exercises.push_back(Exercise::from_json(e));
    return c;
}

Json Catalogue::to_json() const {
    Json json = Json::object();
    put(json, "$comment", comment);
    Json list = Json::array();
    for (const auto& e : exercises) list.push_back(e.to_json());
    json["exercises"] = std::move(list);
    return json;
}

Catalogue decode_catalogue(std::string_view text) {
    return Catalogue::from_json(parse_json(text));
}

} // namespace wl
