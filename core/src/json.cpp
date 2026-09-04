#include "workoutlog/json.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace workoutlog::json {

using nl = nlohmann::json;

namespace {

// ---------------------------------------------------------------- writer helpers ---

nl num(double v) {
    if (std::isfinite(v) && v == std::trunc(v) && std::fabs(v) < 9007199254740992.0)
        return nl(static_cast<std::int64_t>(v));
    return nl(v);
}

template <typename T>
void set_if(nl& j, const char* key, const std::optional<T>& v) {
    if (v) j[key] = *v;
}

void set_num_if(nl& j, const char* key, const std::optional<double>& v) {
    if (v) j[key] = num(*v);
}

// ---------------------------------------------------------------- reader helpers ---

[[noreturn]] void fail(const std::string& msg) {
    throw std::runtime_error(msg);
}

const nl& require(const nl& j, const char* key, const char* ctx) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null())
        fail(std::string(ctx) + ": missing required key \"" + key + "\"");
    return *it;
}

template <typename T>
T require_as(const nl& j, const char* key, const char* ctx) {
    return require(j, key, ctx).get<T>();
}

template <typename T>
std::optional<T> get_opt(const nl& j, const char* key) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return std::nullopt;
    return it->get<T>();
}

std::optional<double> get_opt_double(const nl& j, const char* key) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return std::nullopt;
    return it->get<double>();
}

template <typename E>
std::optional<E> get_opt_enum(const nl& j, const char* key,
                               std::optional<E> (*parse)(std::string_view), const char* ctx) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return std::nullopt;
    auto s = it->get<std::string>();
    auto v = parse(s);
    if (!v) fail(std::string(ctx) + ": invalid value \"" + s + "\" for \"" + key + "\"");
    return v;
}

// --------------------------------------------------------------------- WorkSet ---

nl encode(const WorkSet& s) {
    nl j = nl::object();
    set_num_if(j, "weight_kg", s.weight_kg);
    set_if(j, "reps", s.reps);
    set_num_if(j, "duration_sec", s.duration_sec);
    if (s.cluster) j["cluster"] = *s.cluster;
    set_if(j, "total_reps", s.total_reps);
    set_num_if(j, "rir", s.rir);
    if (s.rep_band) j["rep_band"] = to_string(*s.rep_band);
    if (s.bar_speed) j["bar_speed"] = to_string(*s.bar_speed);
    set_if(j, "is_backoff", s.is_backoff);
    set_if(j, "planned_reps", s.planned_reps);
    set_if(j, "notes", s.notes);
    return j;
}

WorkSet decode_work_set(const nl& j) {
    WorkSet s;
    s.weight_kg = get_opt_double(j, "weight_kg");
    s.reps = get_opt<int>(j, "reps");
    s.duration_sec = get_opt_double(j, "duration_sec");
    s.cluster = get_opt<std::vector<int>>(j, "cluster");
    s.total_reps = get_opt<int>(j, "total_reps");
    s.rir = get_opt_double(j, "rir");
    s.rep_band = get_opt_enum<RepBand>(j, "rep_band", rep_band_from_string, "WorkSet");
    s.bar_speed = get_opt_enum<BarSpeed>(j, "bar_speed", bar_speed_from_string, "WorkSet");
    s.is_backoff = get_opt<bool>(j, "is_backoff");
    s.planned_reps = get_opt<int>(j, "planned_reps");
    s.notes = get_opt<std::string>(j, "notes");
    return s;
}

// -------------------------------------------------------------------- CardioBlock ---

nl encode(const CardioBlock& b) {
    nl j = nl::object();
    j["type"] = "cardio";
    j["machine"] = b.machine;
    set_num_if(j, "duration_min", b.duration_min);
    set_num_if(j, "distance_m", b.distance_m);
    set_if(j, "end_time", b.end_time);
    return j;
}

CardioBlock decode_cardio(const nl& j) {
    CardioBlock b;
    b.machine = require_as<std::string>(j, "machine", "CardioBlock");
    b.duration_min = get_opt_double(j, "duration_min");
    b.distance_m = get_opt_double(j, "distance_m");
    b.end_time = get_opt<std::string>(j, "end_time");
    return b;
}

// ------------------------------------------------------------------ StrengthBlock ---

nl encode(const StrengthBlock& b) {
    nl j = nl::object();
    j["type"] = "strength";
    j["exercise"] = b.exercise;
    nl sets = nl::array();
    for (const auto& s : b.sets) sets.push_back(encode(s));
    j["sets"] = std::move(sets);
    set_if(j, "start_time", b.start_time);
    set_if(j, "end_time", b.end_time);
    set_if(j, "notes", b.notes);
    return j;
}

StrengthBlock decode_strength(const nl& j) {
    StrengthBlock b;
    b.exercise = require_as<std::string>(j, "exercise", "StrengthBlock");
    const nl& sets = require(j, "sets", "StrengthBlock");
    if (!sets.is_array()) fail("StrengthBlock: \"sets\" must be an array");
    for (const auto& sj : sets) b.sets.push_back(decode_work_set(sj));
    b.start_time = get_opt<std::string>(j, "start_time");
    b.end_time = get_opt<std::string>(j, "end_time");
    b.notes = get_opt<std::string>(j, "notes");
    return b;
}

// ---------------------------------------------------------------- MetconExercise ---

nl encode(const MetconExercise& e) {
    nl j = nl::object();
    j["name"] = e.name;
    set_num_if(j, "weight_kg", e.weight_kg);
    if (e.reps_override) j["reps_override"] = *e.reps_override;
    return j;
}

MetconExercise decode_metcon_exercise(const nl& j) {
    MetconExercise e;
    e.name = require_as<std::string>(j, "name", "MetconExercise");
    e.weight_kg = get_opt_double(j, "weight_kg");
    e.reps_override = get_opt<std::vector<int>>(j, "reps_override");
    return e;
}

// ------------------------------------------------------------------- MetconRound ---

nl encode(const MetconRound& r) {
    nl j = nl::object();
    j["round"] = r.round;
    set_if(j, "reps", r.reps);
    set_num_if(j, "split_cumulative_sec", r.split_cumulative_sec);
    set_if(j, "heart_rate", r.heart_rate);
    return j;
}

MetconRound decode_metcon_round(const nl& j) {
    MetconRound r;
    r.round = require_as<int>(j, "round", "MetconRound");
    r.reps = get_opt<int>(j, "reps");
    r.split_cumulative_sec = get_opt_double(j, "split_cumulative_sec");
    r.heart_rate = get_opt<int>(j, "heart_rate");
    return r;
}

// -------------------------------------------------------------------- MetconBlock ---

nl encode(const MetconBlock& b) {
    nl j = nl::object();
    j["type"] = "metcon";
    if (b.format) j["format"] = to_string(*b.format);
    if (b.scheme) j["scheme"] = *b.scheme;
    nl exs = nl::array();
    for (const auto& e : b.exercises) exs.push_back(encode(e));
    j["exercises"] = std::move(exs);
    if (b.rounds) {
        nl rs = nl::array();
        for (const auto& r : *b.rounds) rs.push_back(encode(r));
        j["rounds"] = std::move(rs);
    }
    set_if(j, "start_time", b.start_time);
    set_if(j, "end_time", b.end_time);
    set_if(j, "notes", b.notes);
    return j;
}

MetconBlock decode_metcon(const nl& j) {
    MetconBlock b;
    b.format = get_opt_enum<MetconFormat>(j, "format", metcon_format_from_string, "MetconBlock");
    b.scheme = get_opt<std::vector<int>>(j, "scheme");
    const nl& exs = require(j, "exercises", "MetconBlock");
    if (!exs.is_array()) fail("MetconBlock: \"exercises\" must be an array");
    for (const auto& ej : exs) b.exercises.push_back(decode_metcon_exercise(ej));
    if (auto it = j.find("rounds"); it != j.end() && !it->is_null()) {
        if (!it->is_array()) fail("MetconBlock: \"rounds\" must be an array");
        std::vector<MetconRound> rs;
        for (const auto& rj : *it) rs.push_back(decode_metcon_round(rj));
        b.rounds = std::move(rs);
    }
    b.start_time = get_opt<std::string>(j, "start_time");
    b.end_time = get_opt<std::string>(j, "end_time");
    b.notes = get_opt<std::string>(j, "notes");
    return b;
}

// ------------------------------------------------------------------ CooldownBlock ---

nl encode(const CooldownBlock& b) {
    nl j = nl::object();
    j["type"] = "cooldown";
    set_if(j, "end_time", b.end_time);
    set_if(j, "notes", b.notes);
    return j;
}

CooldownBlock decode_cooldown(const nl& j) {
    CooldownBlock b;
    b.end_time = get_opt<std::string>(j, "end_time");
    b.notes = get_opt<std::string>(j, "notes");
    return b;
}

// ------------------------------------------------------------------------- Block ---
// Flat tagged union on "type" -- the payload decodes from the same object, matching
// Swift's Block.init(from:) (which peeks "type" then decodes the concrete struct from
// the same decoder rather than a nested one).

nl encode(const Block& b) {
    return std::visit([](const auto& v) { return encode(v); }, b);
}

Block decode_block(const nl& j) {
    auto type = require_as<std::string>(j, "type", "Block");
    if (type == "cardio") return Block{decode_cardio(j)};
    if (type == "strength") return Block{decode_strength(j)};
    if (type == "metcon") return Block{decode_metcon(j)};
    if (type == "cooldown") return Block{decode_cooldown(j)};
    fail("Block: unknown type \"" + type + "\"");
}

// ---------------------------------------------------------------------- Session ---

nl encode(const Session& s) {
    nl j = nl::object();
    j["date"] = s.date;
    j["cycle_day"] = s.cycle_day;
    set_if(j, "start_time", s.start_time);
    j["kind"] = to_string(s.kind);
    set_num_if(j, "bodyweight_kg", s.bodyweight_kg);
    set_if(j, "notes", s.notes);
    nl blocks = nl::array();
    for (const auto& b : s.blocks) blocks.push_back(encode(b));
    j["blocks"] = std::move(blocks);
    return j;
}

// kind defaults to training when the key is absent (Session.init(from:) in Session.swift).
Session decode_session_json(const nl& j) {
    Session s;
    s.date = require_as<std::string>(j, "date", "Session");
    s.cycle_day = require_as<std::string>(j, "cycle_day", "Session");
    s.start_time = get_opt<std::string>(j, "start_time");
    s.kind = get_opt_enum<Kind>(j, "kind", kind_from_string, "Session").value_or(Kind::training);
    s.bodyweight_kg = get_opt_double(j, "bodyweight_kg");
    s.notes = get_opt<std::string>(j, "notes");
    const nl& blocks = require(j, "blocks", "Session");
    if (!blocks.is_array()) fail("Session: \"blocks\" must be an array");
    for (const auto& bj : blocks) s.blocks.push_back(decode_block(bj));
    return s;
}

// -------------------------------------------------------------- Exercise/Catalogue ---

Exercise decode_exercise(const nl& j) {
    Exercise e;
    e.name = require_as<std::string>(j, "name", "Exercise");
    e.aliases = get_opt<std::vector<std::string>>(j, "aliases").value_or(std::vector<std::string>{});
    e.pattern = get_opt_enum<Pattern>(j, "pattern", pattern_from_string, "Exercise");
    e.modality = get_opt_enum<Modality>(j, "modality", modality_from_string, "Exercise");
    e.category = get_opt_enum<ExerciseCategory>(j, "category", exercise_category_from_string, "Exercise")
                     .value_or(ExerciseCategory::strength);
    e.primary_muscles = get_opt<std::vector<std::string>>(j, "primary_muscles").value_or(std::vector<std::string>{});
    e.secondary_muscles =
        get_opt<std::vector<std::string>>(j, "secondary_muscles").value_or(std::vector<std::string>{});
    e.notes = get_opt<std::string>(j, "notes");
    return e;
}

// The file carries a top-level "$comment"; decode only "exercises" (Catalogue.init(from:)).
Catalogue decode_catalogue_json(const nl& j) {
    Catalogue c;
    const nl& exs = require(j, "exercises", "Catalogue");
    if (!exs.is_array()) fail("Catalogue: \"exercises\" must be an array");
    for (const auto& ej : exs) c.exercises.push_back(decode_exercise(ej));
    return c;
}

nl parse(const std::string& utf8_json) {
    try {
        return nl::parse(utf8_json);
    } catch (const nl::parse_error& e) {
        fail(std::string("JSON parse error: ") + e.what());
    }
}

// Recursively applies the integral-double writer rule to an untyped JSON value,
// leaving already-integer/string/bool/null values untouched.
nl canonicalize_value(const nl& v) {
    if (v.is_number_float()) return num(v.get<double>());
    if (v.is_object()) {
        nl out = nl::object();
        for (auto it = v.begin(); it != v.end(); ++it) out[it.key()] = canonicalize_value(it.value());
        return out;
    }
    if (v.is_array()) {
        nl out = nl::array();
        for (const auto& e : v) out.push_back(canonicalize_value(e));
        return out;
    }
    return v;
}

} // namespace

Session decode_session(const std::string& utf8_json) {
    return decode_session_json(parse(utf8_json));
}

Catalogue decode_catalogue(const std::string& utf8_json) {
    return decode_catalogue_json(parse(utf8_json));
}

std::string encode_session(const Session& s) {
    return encode(s).dump(2) + "\n";
}

std::string canonicalize(const std::string& utf8_json) {
    return canonicalize_value(parse(utf8_json)).dump(2) + "\n";
}

} // namespace workoutlog::json
