#include "workoutlog/prescription.hpp"

#include <algorithm>
#include <charconv>

#include "workoutlog/cycle_planning.hpp"
#include "workoutlog/utf8.hpp"

namespace wl {

namespace {

Json starting_from(const Json& extras) {
    return extras.is_object() ? extras : Json::object();
}

// Every spelling of "times" becomes `x`, so the parser below sees one symbol.
// `х` is the Cyrillic letter, the one a Ukrainian keyboard types.
std::string with_times_as_x(std::string_view text) {
    std::string out(text);
    for (std::string_view sign : {"×", "х", "Х", "X", "*"}) {
        for (size_t at = out.find(sign); at != std::string::npos; at = out.find(sign, at + 1))
            out.replace(at, sign.size(), "x");
    }
    return out;
}

std::optional<std::int64_t> positive_int(std::string_view text) {
    std::string trimmed = utf8::trim(text);
    std::int64_t value = 0;
    auto [end, error] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value);
    if (error != std::errc{} || end != trimmed.data() + trimmed.size() || value <= 0) return std::nullopt;
    return value;
}

} // namespace

PrescribedDay PrescribedDay::from_json(const Json& json) {
    as_object(json, "prescribed day");
    PrescribedDay d;
    d.cycle_day = req_string(json, "cycle_day");
    d.date = req_string(json, "date");
    d.title = opt_string(json, "title");
    d.notes = opt_string(json, "notes");
    for (const auto& b : req_array(json, "blocks")) d.blocks.push_back(block_from_json(b));
    d.extras = unmodelled_keys(json, {"cycle_day", "date", "title", "notes", "blocks"});
    return d;
}

Json PrescribedDay::to_json() const {
    Json json = starting_from(extras);
    json["cycle_day"] = cycle_day;
    json["date"] = date;
    put(json, "title", title);
    put(json, "notes", notes);
    Json list = Json::array();
    for (const auto& b : blocks) list.push_back(block_to_json(b));
    json["blocks"] = std::move(list);
    return json;
}

std::string Prescription::file_name() const {
    return std::string(kPrescriptionDirectory) + "/" + cycle_id + "-v" + std::to_string(cycle_version) + "_" +
           start_date + ".json";
}

Prescription Prescription::from_json(const Json& json) {
    as_object(json, "prescription");
    Prescription p;
    p.cycle_id = req_string(json, "cycle");
    p.cycle_version = opt_int(json, "cycle_version").value_or(1);
    p.start_date = req_string(json, "start_date");
    for (const auto& d : req_array(json, "days")) p.days.push_back(PrescribedDay::from_json(d));
    p.extras = unmodelled_keys(json, {"cycle", "cycle_version", "start_date", "days"});
    return p;
}

Json Prescription::to_json() const {
    Json json = starting_from(extras);
    json["cycle"] = cycle_id;
    json["cycle_version"] = cycle_version;
    json["start_date"] = start_date;
    Json list = Json::array();
    for (const auto& d : days) list.push_back(d.to_json());
    json["days"] = std::move(list);
    return json;
}

Prescription decode_prescription(std::string_view text) {
    return Prescription::from_json(parse_json(text));
}

std::string encode_prescription(const Prescription& prescription) {
    return encode_file(prescription.to_json());
}

Prescription prescribe(const Cycle& cycle, Date start) {
    Prescription p;
    p.cycle_id = cycle.id;
    p.cycle_version = cycle.version_number();
    p.start_date = iso_date(start);
    auto dates = training_dates(start, cycle.training_days, cycle.sessions.size());
    for (size_t i = 0; i < cycle.sessions.size(); ++i) {
        const CycleSession& tmpl = cycle.sessions[i];
        Session s = session_from_template(tmpl, dates[i]);
        p.days.push_back(PrescribedDay{.cycle_day = s.cycle_day,
                                       .date = s.date,
                                       .title = tmpl.title,
                                       .notes = s.notes,
                                       .blocks = std::move(s.blocks)});
    }
    return p;
}

Session session_for(const PrescribedDay& day) {
    Session s;
    s.date = day.date;
    s.cycle_day = day.cycle_day;
    s.notes = day.notes;
    s.blocks = day.blocks;
    return s;
}

PlannedLoad& PlannedLoad::operator+=(const PlannedLoad& other) {
    sets += other.sets;
    reps += other.reps;
    metcon_rounds += other.metcon_rounds;
    tonnage_kg += other.tonnage_kg;
    return *this;
}

PlannedLoad planned_load(const std::vector<Block>& blocks) {
    PlannedLoad load;
    for (const auto& block : blocks) {
        if (const auto* strength = std::get_if<StrengthBlock>(&block)) {
            for (const auto& set : strength->sets) {
                ++load.sets;
                load.reps += set.recorded_reps().value_or(0);
                load.tonnage_kg += set.tonnage_kg();
            }
        } else if (const auto* metcon = std::get_if<MetconBlock>(&block)) {
            load.metcon_rounds += metcon->scheme ? static_cast<std::int64_t>(metcon->scheme->size()) : 0;
            for (const auto& e : metcon->exercises) {
                if (!e.weight_kg) continue;
                for (auto reps : e.scheme_within(metcon->scheme))
                    load.tonnage_kg += static_cast<double>(reps) * *e.weight_kg;
            }
        }
    }
    return load;
}

std::optional<IntList> parse_scheme(std::string_view text) {
    std::string normal = with_times_as_x(utf8::trim(text));
    if (normal.empty()) return std::nullopt;
    if (auto x = normal.find('x'); x != std::string::npos) {
        auto rounds = positive_int(std::string_view(normal).substr(0, x));
        auto reps = positive_int(std::string_view(normal).substr(x + 1));
        if (!rounds || !reps) return std::nullopt;
        return IntList(static_cast<size_t>(*rounds), *reps);
    }
    IntList out;
    size_t from = 0;
    while (from <= normal.size()) {
        size_t to = normal.find_first_of("-, ", from);
        if (to == std::string::npos) to = normal.size();
        if (to > from) {
            auto reps = positive_int(std::string_view(normal).substr(from, to - from));
            if (!reps) return std::nullopt;
            out.push_back(*reps);
        }
        from = to + 1;
    }
    if (out.empty()) return std::nullopt;
    return out;
}

std::string format_scheme(const IntList& scheme) {
    if (scheme.empty()) return "";
    bool equal = std::all_of(scheme.begin(), scheme.end(), [&](auto r) { return r == scheme.front(); });
    if (equal && scheme.size() > 1) return std::to_string(scheme.size()) + "×" + std::to_string(scheme.front());
    std::string out;
    for (size_t i = 0; i < scheme.size(); ++i) {
        if (i) out += "-";
        out += std::to_string(scheme[i]);
    }
    return out;
}

} // namespace wl
