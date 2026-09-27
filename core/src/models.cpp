#include "workoutlog/models.hpp"

#include <cmath>
#include <numeric>

namespace wl {

std::optional<RepBand> rep_band_for(std::optional<std::int64_t> reps) {
    if (!reps || *reps <= 0) return std::nullopt;
    if (*reps <= 3) return RepBand::heavy;
    if (*reps <= 6) return RepBand::base;
    return RepBand::volume;
}

std::optional<std::int64_t> WorkSet::recorded_reps() const {
    if (reps) return reps;
    if (total_reps) return total_reps;
    if (cluster) return std::accumulate(cluster->begin(), cluster->end(), std::int64_t{0});
    return std::nullopt;
}

std::optional<RepBand> WorkSet::band() const {
    return rep_band ? rep_band : rep_band_for(recorded_reps());
}

double WorkSet::tonnage_kg() const {
    return static_cast<double>(recorded_reps().value_or(0)) * weight_kg.value_or(0);
}

WorkSet WorkSet::from_json(const Json& json) {
    as_object(json, "set");
    WorkSet s;
    s.weight_kg = opt_double(json, "weight_kg");
    s.reps = opt_int(json, "reps");
    s.duration_sec = opt_double(json, "duration_sec");
    s.cluster = opt_int_list(json, "cluster");
    s.total_reps = opt_int(json, "total_reps");
    s.rir = opt_double(json, "rir");
    s.rep_band = opt_enum<RepBand>(json, "rep_band");
    s.bar_speed = opt_enum<BarSpeed>(json, "bar_speed");
    s.is_backoff = opt_bool(json, "is_backoff");
    s.planned_reps = opt_int(json, "planned_reps");
    s.notes = opt_string(json, "notes");
    return s;
}

Json WorkSet::to_json() const {
    Json json = Json::object();
    put(json, "weight_kg", weight_kg);
    put(json, "reps", reps);
    put(json, "duration_sec", duration_sec);
    if (cluster) json["cluster"] = wl::to_json(*cluster);
    put(json, "total_reps", total_reps);
    put(json, "rir", rir);
    put_enum(json, "rep_band", rep_band);
    put_enum(json, "bar_speed", bar_speed);
    put(json, "is_backoff", is_backoff);
    put(json, "planned_reps", planned_reps);
    put(json, "notes", notes);
    return json;
}

std::string metcon_format_wire(MetconFormat format) {
    return std::string(name(format));
}

std::optional<std::string> MetconExercise::prescription() const {
    if (load) return load;
    if (!weight_kg) return std::nullopt;
    double kg = *weight_kg;
    return (kg == std::round(kg) ? std::to_string(static_cast<std::int64_t>(kg)) : dart_double(kg)) + " kg";
}

IntList MetconExercise::scheme_within(const std::optional<IntList>& block_scheme) const {
    if (reps_override) return *reps_override;
    if (block_scheme) return *block_scheme;
    return {};
}

MetconExercise MetconExercise::from_json(const Json& json) {
    as_object(json, "exercise");
    MetconExercise e;
    e.name = req_string(json, "name");
    e.load = opt_string(json, "load");
    e.weight_kg = opt_double(json, "weight_kg");
    e.reps_override = opt_int_list(json, "reps_override");
    return e;
}

Json MetconExercise::to_json() const {
    Json json = Json::object();
    json["name"] = name;
    put(json, "load", load);
    put(json, "weight_kg", weight_kg);
    if (reps_override) json["reps_override"] = wl::to_json(*reps_override);
    return json;
}

MetconRound MetconRound::from_json(const Json& json) {
    as_object(json, "round");
    MetconRound r;
    auto round = opt_int(json, "round");
    if (!round) throw FormatError("\"round\" is required");
    r.round = *round;
    r.reps = opt_int(json, "reps");
    r.split_cumulative_sec = opt_double(json, "split_cumulative_sec");
    r.heart_rate = opt_int(json, "heart_rate");
    return r;
}

Json MetconRound::to_json() const {
    Json json = Json::object();
    json["round"] = round;
    put(json, "reps", reps);
    put(json, "split_cumulative_sec", split_cumulative_sec);
    put(json, "heart_rate", heart_rate);
    return json;
}

namespace {

struct TypeName {
    std::string_view operator()(const CardioBlock&) const { return "cardio"; }
    std::string_view operator()(const StrengthBlock&) const { return "strength"; }
    std::string_view operator()(const MetconBlock&) const { return "metcon"; }
    std::string_view operator()(const CooldownBlock&) const { return "cooldown"; }
};

struct ToJson {
    Json operator()(const CardioBlock& b) const {
        Json json = Json::object();
        json["type"] = "cardio";
        json["machine"] = b.machine;
        put(json, "duration_min", b.duration_min);
        put(json, "distance_m", b.distance_m);
        put(json, "end_time", b.end_time);
        return json;
    }
    Json operator()(const StrengthBlock& b) const {
        Json json = Json::object();
        json["type"] = "strength";
        json["exercise"] = b.exercise;
        Json sets = Json::array();
        for (const auto& s : b.sets) sets.push_back(s.to_json());
        json["sets"] = std::move(sets);
        put(json, "start_time", b.start_time);
        put(json, "end_time", b.end_time);
        put(json, "notes", b.notes);
        return json;
    }
    Json operator()(const MetconBlock& b) const {
        Json json = Json::object();
        json["type"] = "metcon";
        if (b.format) json["format"] = metcon_format_wire(*b.format);
        if (b.scheme) json["scheme"] = wl::to_json(*b.scheme);
        Json exercises = Json::array();
        for (const auto& e : b.exercises) exercises.push_back(e.to_json());
        json["exercises"] = std::move(exercises);
        if (b.rounds) {
            Json rounds = Json::array();
            for (const auto& r : *b.rounds) rounds.push_back(r.to_json());
            json["rounds"] = std::move(rounds);
        }
        put(json, "start_time", b.start_time);
        put(json, "end_time", b.end_time);
        put(json, "notes", b.notes);
        return json;
    }
    Json operator()(const CooldownBlock& b) const {
        Json json = Json::object();
        json["type"] = "cooldown";
        put(json, "end_time", b.end_time);
        put(json, "notes", b.notes);
        return json;
    }
};

} // namespace

std::string_view block_type(const Block& block) {
    return std::visit(TypeName{}, block);
}

Json block_to_json(const Block& block) {
    return std::visit(ToJson{}, block);
}

std::optional<std::string> block_start_time(const Block& block) {
    if (auto* s = std::get_if<StrengthBlock>(&block)) return s->start_time;
    if (auto* m = std::get_if<MetconBlock>(&block)) return m->start_time;
    return std::nullopt;
}

std::optional<std::string> block_end_time(const Block& block) {
    return std::visit([](const auto& b) { return b.end_time; }, block);
}

Block block_from_json(const Json& json) {
    as_object(json, "block");
    std::string type = req_string(json, "type");
    if (type == "cardio") {
        CardioBlock b;
        b.machine = req_string(json, "machine");
        b.duration_min = opt_double(json, "duration_min");
        b.distance_m = opt_double(json, "distance_m");
        b.end_time = opt_string(json, "end_time");
        return b;
    }
    if (type == "strength") {
        StrengthBlock b;
        b.exercise = req_string(json, "exercise");
        for (const auto& s : req_array(json, "sets")) b.sets.push_back(WorkSet::from_json(s));
        b.start_time = opt_string(json, "start_time");
        b.end_time = opt_string(json, "end_time");
        b.notes = opt_string(json, "notes");
        return b;
    }
    if (type == "metcon") {
        MetconBlock b;
        b.format = opt_enum<MetconFormat>(json, "format");
        b.scheme = opt_int_list(json, "scheme");
        if (const Json* list = opt_array(json, "exercises"))
            for (const auto& e : *list) b.exercises.push_back(MetconExercise::from_json(e));
        if (const Json* list = opt_array(json, "rounds")) {
            b.rounds.emplace();
            for (const auto& r : *list) b.rounds->push_back(MetconRound::from_json(r));
        }
        b.start_time = opt_string(json, "start_time");
        b.end_time = opt_string(json, "end_time");
        b.notes = opt_string(json, "notes");
        return b;
    }
    if (type == "cooldown") {
        CooldownBlock b;
        b.end_time = opt_string(json, "end_time");
        b.notes = opt_string(json, "notes");
        return b;
    }
    throw FormatError("Unknown block type \"" + type + "\"");
}

Session Session::from_json(const Json& json) {
    as_object(json, "session");
    Session s;
    s.date = req_string(json, "date");
    s.cycle_day = req_string(json, "cycle_day");
    s.start_time = opt_string(json, "start_time");
    s.kind = opt_enum<Kind>(json, "kind").value_or(Kind::training);
    s.bodyweight_kg = opt_double(json, "bodyweight_kg");
    s.notes = opt_string(json, "notes");
    for (const auto& b : req_array(json, "blocks")) s.blocks.push_back(block_from_json(b));
    return s;
}

Json Session::to_json() const {
    Json json = Json::object();
    json["date"] = date;
    json["cycle_day"] = cycle_day;
    put(json, "start_time", start_time);
    json["kind"] = std::string(name(kind));
    put(json, "bodyweight_kg", bodyweight_kg);
    put(json, "notes", notes);
    Json list = Json::array();
    for (const auto& b : blocks) list.push_back(block_to_json(b));
    json["blocks"] = std::move(list);
    return json;
}

Session decode_session(std::string_view text) {
    return Session::from_json(parse_json(text));
}

std::string encode_session(const Session& session) {
    return encode_file(session.to_json());
}

} // namespace wl
