#include "workoutlog/cycle.hpp"

namespace wl {

namespace {

Json starting_from(const Json& extras) {
    return extras.is_object() ? extras : Json::object();
}

Json strings(const std::vector<std::string>& values) {
    Json out = Json::array();
    for (const auto& v : values) out.push_back(v);
    return out;
}

} // namespace

BlockTemplate BlockTemplate::from_json(const Json& json) {
    as_object(json, "block template");
    BlockTemplate b;
    b.type = req_string(json, "type");
    b.role = opt_string(json, "role");
    b.machine = opt_string(json, "machine");
    b.duration_min = opt_double(json, "duration_min");
    b.exercise = opt_string(json, "exercise");
    b.sets_reps = opt_int_list(json, "sets_reps").value_or(IntList{});
    b.notes = opt_string(json, "notes");
    b.format = opt_enum<MetconFormat>(json, "format");
    b.scheme = opt_int_list(json, "scheme");
    if (const Json* list = opt_array(json, "exercises"))
        for (const auto& e : *list) b.exercises.push_back(MetconExercise::from_json(e));
    b.extras = unmodelled_keys(json, {"type", "role", "machine", "duration_min", "exercise",
                                      "sets_reps", "notes", "format", "scheme", "exercises"});
    for (auto it = json.begin(); it != json.end(); ++it) b.present_keys.insert(it.key());
    return b;
}

Json BlockTemplate::to_json() const {
    Json json = starting_from(extras);
    json["type"] = type;
    put(json, "role", role);
    put(json, "machine", machine);
    put(json, "duration_min", duration_min);
    put(json, "exercise", exercise);
    if (!sets_reps.empty() || present_keys.contains("sets_reps")) json["sets_reps"] = wl::to_json(sets_reps);
    put(json, "notes", notes);
    if (format) json["format"] = metcon_format_wire(*format);
    if (scheme) json["scheme"] = wl::to_json(*scheme);
    if (!exercises.empty() || present_keys.contains("exercises")) {
        Json list = Json::array();
        for (const auto& e : exercises) list.push_back(e.to_json());
        json["exercises"] = std::move(list);
    }
    return json;
}

CycleSession CycleSession::from_json(const Json& json) {
    as_object(json, "cycle session");
    CycleSession s;
    s.cycle_day = req_string(json, "cycle_day");
    s.week = opt_int(json, "week");
    s.weekday = opt_string(json, "weekday");
    s.type = opt_string(json, "type");
    s.title = opt_string(json, "title");
    s.session_notes = opt_string(json, "session_notes");
    for (const auto& b : req_array(json, "blocks")) s.blocks.push_back(BlockTemplate::from_json(b));
    s.extras = unmodelled_keys(json, {"cycle_day", "week", "weekday", "type", "title", "session_notes", "blocks"});
    return s;
}

Json CycleSession::to_json() const {
    Json json = starting_from(extras);
    json["cycle_day"] = cycle_day;
    put(json, "week", week);
    put(json, "weekday", weekday);
    put(json, "type", type);
    put(json, "title", title);
    put(json, "session_notes", session_notes);
    Json list = Json::array();
    for (const auto& b : blocks) list.push_back(b.to_json());
    json["blocks"] = std::move(list);
    return json;
}

Cycle Cycle::from_json(const Json& json) {
    as_object(json, "cycle");
    Cycle c;
    c.id = req_string(json, "id");
    c.name = opt_string(json, "name").value_or(c.id);
    c.training_days = string_list(json, "training_days");
    c.start_date = req_string(json, "start_date");
    for (const auto& s : req_array(json, "sessions")) c.sessions.push_back(CycleSession::from_json(s));
    c.extras = unmodelled_keys(json, {"id", "name", "training_days", "start_date", "sessions"});
    return c;
}

Json Cycle::to_json() const {
    Json json = starting_from(extras);
    json["id"] = id;
    json["name"] = name;
    json["training_days"] = strings(training_days);
    json["start_date"] = start_date;
    Json list = Json::array();
    for (const auto& s : sessions) list.push_back(s.to_json());
    json["sessions"] = std::move(list);
    return json;
}

const Cycle* CycleCatalogue::by_id(std::string_view id) const {
    for (const auto& c : cycles)
        if (c.id == id) return &c;
    return nullptr;
}

Cycle* CycleCatalogue::by_id(std::string_view id) {
    for (auto& c : cycles)
        if (c.id == id) return &c;
    return nullptr;
}

CycleCatalogue CycleCatalogue::from_json(const Json& json) {
    as_object(json, "cycles");
    CycleCatalogue c;
    c.comment = opt_string(json, "$comment");
    for (const auto& cycle : req_array(json, "cycles")) c.cycles.push_back(Cycle::from_json(cycle));
    return c;
}

Json CycleCatalogue::to_json() const {
    Json json = Json::object();
    put(json, "$comment", comment);
    Json list = Json::array();
    for (const auto& c : cycles) list.push_back(c.to_json());
    json["cycles"] = std::move(list);
    return json;
}

CycleCatalogue decode_cycles(std::string_view text) {
    return CycleCatalogue::from_json(parse_json(text));
}

} // namespace wl
