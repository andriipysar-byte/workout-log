#include "workoutlog/analytics.hpp"

#include <algorithm>
#include <cmath>
#include <set>

#include "workoutlog/date.hpp"
#include "workoutlog/utf8.hpp"
#include "workoutlog/wall_clock.hpp"

namespace wl {

namespace {

double round_to(double value, int digits = 1) {
    static constexpr double factors[] = {1, 10, 100, 1000};
    double factor = factors[digits];
    return std::round(value * factor) / factor;
}

std::optional<double> round_to(std::optional<double> value, int digits = 1) {
    if (!value) return std::nullopt;
    return round_to(*value, digits);
}

Json band_json(const BandCounts& counts) {
    Json json = Json::object();
    for (size_t i = 0; i < enum_count<RepBand>(); ++i) {
        auto band = static_cast<RepBand>(i);
        auto it = counts.find(band);
        if (it != counts.end() && it->second > 0) json[std::string(name(band))] = it->second;
    }
    return json;
}

// Ties go to the heavier band: a slot of half sixes and half eights is not
// evidence for the volume dose P4 rations.
std::optional<RepBand> dominant_band(const BandCounts& counts) {
    std::optional<RepBand> best;
    std::int64_t best_count = 0;
    for (size_t i = 0; i < enum_count<RepBand>(); ++i) {
        auto band = static_cast<RepBand>(i);
        auto it = counts.find(band);
        std::int64_t count = it == counts.end() ? 0 : it->second;
        if (count > best_count) {
            best = band;
            best_count = count;
        }
    }
    return best;
}

std::vector<Session> by_date(const std::vector<Session>& sessions) {
    std::vector<Session> ordered = sessions;
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.date < b.date; });
    return ordered;
}

Json strings(const std::vector<std::string>& values) {
    Json out = Json::array();
    for (const auto& v : values) out.push_back(v);
    return out;
}

// The reps a round carries when it does not say: its scheme position
// (21-15-9 → round 2 is 15).
std::optional<std::int64_t> scheme_reps(const MetconBlock& block, std::int64_t round) {
    if (!block.scheme || round < 1 || round > static_cast<std::int64_t>(block.scheme->size())) return std::nullopt;
    return (*block.scheme)[static_cast<size_t>(round - 1)];
}

MetconMetrics metcon_metrics(size_t index, const MetconBlock& block, std::optional<int> start_min,
                             std::optional<int> end_min) {
    MetconMetrics m;
    m.block_index = index;
    std::optional<double> previous;
    if (block.rounds) {
        for (const auto& round : *block.rounds) {
            RoundSplit split;
            split.round = round.round;
            split.reps = round.reps ? round.reps : scheme_reps(block, round.round);
            split.cumulative_sec = round.split_cumulative_sec;
            if (round.split_cumulative_sec) {
                double cumulative = *round.split_cumulative_sec;
                split.split_sec = previous ? cumulative - *previous : cumulative;
                previous = cumulative;
            }
            split.heart_rate = round.heart_rate;
            m.rounds.push_back(split);
        }
    }
    std::optional<double> from_rounds = m.rounds.empty() ? std::nullopt : m.rounds.back().cumulative_sec;
    std::optional<double> from_clock;
    if (start_min && end_min && *end_min >= *start_min) from_clock = (*end_min - *start_min) * 60.0;
    if (block.format) m.format = metcon_format_wire(*block.format);
    m.scheme = block.scheme;
    for (const auto& e : block.exercises) m.exercises.push_back(e.name);
    m.total_sec = from_rounds ? from_rounds : from_clock;
    return m;
}

} // namespace

std::optional<TimeDomain> time_domain_for(std::optional<double> seconds) {
    if (!seconds || *seconds <= 0) return std::nullopt;
    if (*seconds <= 300) return TimeDomain::alactic;
    if (*seconds <= 1200) return TimeDomain::glycolytic;
    return TimeDomain::aerobic;
}

std::string_view time_domain_name(TimeDomain domain) {
    switch (domain) {
        case TimeDomain::alactic: return "alactic";
        case TimeDomain::glycolytic: return "glycolytic";
        case TimeDomain::aerobic: return "aerobic";
    }
    return "";
}

std::optional<double> SessionDensity::work_share() const {
    if (!total_min || *total_min <= 0) return std::nullopt;
    return round_to(work_min / *total_min, 3);
}

Json SessionDensity::to_json() const {
    Json json = Json::object();
    json["work_min"] = round_to(work_min);
    json["transition_min"] = round_to(transition_min);
    put(json, "total_min", round_to(total_min));
    put(json, "work_share", work_share());
    return json;
}

Json BlockMetrics::to_json() const {
    Json json = Json::object();
    json["index"] = index;
    json["type"] = type;
    put(json, "label", label);
    put(json, "duration_min", round_to(duration_min));
    put(json, "transition_min", round_to(transition_min));
    if (sets > 0) json["sets"] = sets;
    if (backoff_sets > 0) json["backoff_sets"] = backoff_sets;
    if (reps > 0) json["reps"] = reps;
    if (tonnage_kg > 0) json["tonnage_kg"] = round_to(tonnage_kg);
    put_enum(json, "rep_band", band);
    return json;
}

std::optional<double> RoundSplit::seconds_per_rep() const {
    if (!split_sec || !reps || *reps <= 0) return std::nullopt;
    return round_to(*split_sec / static_cast<double>(*reps), 2);
}

Json RoundSplit::to_json() const {
    Json json = Json::object();
    json["round"] = round;
    put(json, "reps", reps);
    put(json, "cumulative_sec", round_to(cumulative_sec));
    put(json, "split_sec", round_to(split_sec));
    put(json, "sec_per_rep", seconds_per_rep());
    put(json, "heart_rate", heart_rate);
    return json;
}

std::optional<double> MetconMetrics::pace_decay_percent() const {
    std::vector<double> paces;
    for (const auto& r : rounds)
        if (auto p = r.seconds_per_rep()) paces.push_back(*p);
    if (paces.size() < 2 || paces.front() <= 0) return std::nullopt;
    return round_to((paces.back() - paces.front()) / paces.front() * 100, 1);
}

std::optional<std::int64_t> MetconMetrics::peak_heart_rate() const {
    std::optional<std::int64_t> peak;
    for (const auto& r : rounds)
        if (r.heart_rate && (!peak || *r.heart_rate > *peak)) peak = r.heart_rate;
    return peak;
}

Json MetconMetrics::to_json() const {
    Json json = Json::object();
    json["block_index"] = block_index;
    put(json, "format", format);
    if (scheme) json["scheme"] = wl::to_json(*scheme);
    if (!exercises.empty()) json["exercises"] = strings(exercises);
    put(json, "total_sec", round_to(total_sec));
    if (auto d = domain()) json["time_domain"] = std::string(time_domain_name(*d));
    put(json, "pace_decay_percent", pace_decay_percent());
    put(json, "peak_heart_rate", peak_heart_rate());
    if (!rounds.empty()) {
        Json list = Json::array();
        for (const auto& r : rounds) list.push_back(r.to_json());
        json["rounds"] = std::move(list);
    }
    return json;
}

SessionMetrics SessionMetrics::of(const Session& session) {
    SessionMetrics m;
    m.date = session.date;
    m.cycle_day = session.cycle_day;
    m.kind = session.kind;

    auto session_start = wall_clock::minutes(session.start_time);
    std::optional<int> cursor = session_start;
    std::optional<int> last_end;

    for (size_t index = 0; index < session.blocks.size(); ++index) {
        const Block& block = session.blocks[index];
        auto start = wall_clock::minutes(block_start_time(block));
        if (!start) start = cursor;
        auto end = wall_clock::minutes(block_end_time(block));

        std::optional<double> duration;
        if (start && end && *end >= *start)
            duration = static_cast<double>(*end - *start);
        else if (const auto* cardio = std::get_if<CardioBlock>(&block))
            duration = cardio->duration_min;

        std::optional<double> transition;
        if (cursor && start && *start > *cursor) transition = static_cast<double>(*start - *cursor);

        if (duration) m.density.work_min += *duration;
        if (transition) m.density.transition_min += *transition;

        if (end) {
            cursor = end;
            last_end = end;
        } else if (start && duration) {
            cursor = *start + static_cast<int>(std::lround(*duration));
            last_end = cursor;
        }

        BlockMetrics bm;
        bm.index = index;
        bm.type = std::string(block_type(block));
        bm.duration_min = duration;
        bm.transition_min = transition;

        if (const auto* strength = std::get_if<StrengthBlock>(&block)) {
            BandCounts counts;
            for (const auto& set : strength->sets) {
                if (auto band = set.band()) {
                    ++counts[*band];
                    ++m.band_sets[*band];
                }
                bm.reps += set.recorded_reps().value_or(0);
                bm.tonnage_kg += set.tonnage_kg();
                if (set.backoff()) ++bm.backoff_sets;
            }
            bm.band = dominant_band(counts);
            if (bm.band) ++m.band_slots[*bm.band];
            bm.sets = static_cast<std::int64_t>(strength->sets.size());
            bm.label = strength->exercise;
            m.sets += bm.sets;
            m.backoff_sets += bm.backoff_sets;
            m.reps += bm.reps;
            m.tonnage_kg += bm.tonnage_kg;
        } else if (const auto* metcon = std::get_if<MetconBlock>(&block)) {
            MetconMetrics mm = metcon_metrics(index, *metcon, start, end);
            bm.label = metcon->format ? metcon_format_wire(*metcon->format) : "metcon";
            if (!duration && mm.total_sec) bm.duration_min = *mm.total_sec / 60;
            m.metcons.push_back(std::move(mm));
        } else if (const auto* cardio = std::get_if<CardioBlock>(&block)) {
            if (!cardio->machine.empty()) bm.label = cardio->machine;
        }
        m.blocks.push_back(std::move(bm));
    }

    if (session_start && last_end) m.density.total_min = static_cast<double>(*last_end - *session_start);
    return m;
}

Json SessionMetrics::to_json() const {
    Json json = Json::object();
    json["date"] = date;
    json["cycle_day"] = cycle_day;
    json["kind"] = std::string(name(kind));
    Json strength = Json::object();
    strength["tonnage_kg"] = round_to(tonnage_kg);
    strength["sets"] = sets;
    strength["top_sets"] = top_sets();
    strength["backoff_sets"] = backoff_sets;
    strength["reps"] = reps;
    strength["rep_band_sets"] = band_json(band_sets);
    strength["rep_band_slots"] = band_json(band_slots);
    json["strength"] = std::move(strength);
    json["density"] = density.to_json();
    Json list = Json::array();
    for (const auto& b : blocks) list.push_back(b.to_json());
    json["blocks"] = std::move(list);
    if (!metcons.empty()) {
        Json ms = Json::array();
        for (const auto& mm : metcons) ms.push_back(mm.to_json());
        json["metcons"] = std::move(ms);
    }
    return json;
}

Json TrainingAlert::to_json() const {
    Json json = Json::object();
    json["principle"] = principle;
    json["message"] = message;
    if (!sessions.empty()) json["sessions"] = strings(sessions);
    return json;
}

Json PatternFrequency::to_json() const {
    Json json = Json::object();
    json["pattern"] = std::string(name(pattern));
    json["slots"] = slots;
    json["sessions"] = sessions;
    json["per_cycle"] = per_cycle;
    return json;
}

Json LoadPoint::to_json() const {
    Json json = Json::object();
    json["date"] = date;
    json["cycle_day"] = cycle_day;
    json["kind"] = std::string(name(kind));
    json["tonnage_kg"] = tonnage_kg;
    json["sets"] = sets;
    put(json, "metcon_sec", metcon_sec);
    put(json, "work_min", work_min);
    return json;
}

namespace {

// The 4 / 2 / 1 shape: three or more recorded sets whose reps only ever fall.
bool reps_collapse(const std::vector<WorkSet>& sets) {
    std::vector<std::int64_t> reps;
    for (const auto& s : sets)
        if (s.recorded_reps().value_or(0) > 0) reps.push_back(*s.recorded_reps());
    if (reps.size() < 3) return false;
    for (size_t i = 1; i < reps.size(); ++i)
        if (reps[i] >= reps[i - 1]) return false;
    return true;
}

// Calendar days, not elapsed hours: the Dart original measured local
// DateTimes, which came out a day short across a DST change.
std::optional<std::int64_t> days_between(const std::optional<std::string>& from,
                                         const std::optional<std::string>& to) {
    if (!from || !to) return std::nullopt;
    auto start = parse_date(*from);
    auto end = parse_date(*to);
    if (!start || !end) return std::nullopt;
    return (*end - *start).count() + 1;
}

} // namespace

double TrainingReport::total_tonnage_kg() const {
    double sum = 0;
    for (const auto& p : load) sum += p.tonnage_kg;
    return round_to(sum);
}

TrainingReport TrainingReport::of(const std::vector<Session>& sessions, const Catalogue& catalogue,
                                  TrainingReportOptions options) {
    TrainingReport r;
    r.cycle_length_days = options.cycle_length_days;
    auto ordered = by_date(sessions);
    std::map<MovementPattern, std::int64_t> pattern_slots;
    std::map<MovementPattern, std::set<std::string>> pattern_sessions;
    std::set<std::string> unknown, explosive_offenders, decay_offenders;

    for (const auto& session : ordered) {
        auto metrics = SessionMetrics::of(session);
        ++r.kinds[session.kind];
        for (auto [band, n] : metrics.band_sets) r.band_sets[band] += n;
        for (auto [band, n] : metrics.band_slots) r.band_slots[band] += n;
        for (const auto& metcon : metrics.metcons)
            if (auto d = metcon.domain()) ++r.time_domains[*d];

        LoadPoint point{session.date, session.cycle_day, session.kind, metrics.tonnage_kg, metrics.sets, {}, {}};
        if (!metrics.metcons.empty()) {
            double sum = 0;
            for (const auto& mm : metrics.metcons) sum += mm.total_sec.value_or(0);
            point.metcon_sec = sum;
        }
        if (metrics.density.work_min > 0) point.work_min = metrics.density.work_min;
        r.load.push_back(point);

        for (const auto& block : session.blocks) {
            std::vector<std::string> names;
            const auto* strength = std::get_if<StrengthBlock>(&block);
            if (strength)
                names.push_back(strength->exercise);
            else if (const auto* metcon = std::get_if<MetconBlock>(&block))
                for (const auto& e : metcon->exercises) names.push_back(e.name);

            for (const auto& n : names) {
                const Exercise* resolved = catalogue.resolve(n);
                if (!resolved) {
                    if (!utf8::trim(n).empty()) unknown.insert(n);
                    continue;
                }
                if (!resolved->pattern) continue;
                if (strength) ++pattern_slots[*resolved->pattern];
                pattern_sessions[*resolved->pattern].insert(session.date);
            }

            if (strength) {
                const Exercise* resolved = catalogue.resolve(strength->exercise);
                if (resolved && resolved->is_explosive()) {
                    std::string label = session.date + " " + strength->exercise;
                    if (std::any_of(strength->sets.begin(), strength->sets.end(),
                                    [](const auto& s) { return s.recorded_reps().value_or(0) > 3; }))
                        explosive_offenders.insert(label);
                    if (reps_collapse(strength->sets)) decay_offenders.insert(label);
                }
            }
        }
    }

    if (!ordered.empty()) {
        r.from = ordered.front().date;
        r.to = ordered.back().date;
    }
    r.days = days_between(r.from, r.to);
    std::optional<double> cycles;
    if (r.days) cycles = static_cast<double>(*r.days) / static_cast<double>(options.cycle_length_days);

    for (size_t i = 0; i < enum_count<MovementPattern>(); ++i) {
        auto pattern = static_cast<MovementPattern>(i);
        std::int64_t slots = pattern_slots.contains(pattern) ? pattern_slots[pattern] : 0;
        std::int64_t session_count =
            pattern_sessions.contains(pattern) ? static_cast<std::int64_t>(pattern_sessions[pattern].size()) : 0;
        if (slots <= 0 && session_count == 0) continue;
        double per_cycle = !cycles || *cycles <= 0 ? 0.0 : round_to(static_cast<double>(slots) / *cycles);
        r.patterns.push_back({pattern, slots, session_count, per_cycle});
    }

    std::int64_t total_slots = 0;
    for (auto [_, n] : r.band_slots) total_slots += n;
    std::int64_t volume_slots = r.band_slots.contains(RepBand::volume) ? r.band_slots[RepBand::volume] : 0;
    double share = total_slots > 0 ? static_cast<double>(volume_slots) / static_cast<double>(total_slots) : 0;
    if (total_slots > 0 && share > options.volume_slot_share_threshold)
        r.alerts.push_back({"P5",
                            "volume (7+) slots are " + std::to_string(std::llround(share * 100)) + "% of " +
                                std::to_string(total_slots) + " slots, over the " +
                                std::to_string(std::llround(options.volume_slot_share_threshold * 100)) +
                                "% guard — the state that produced the involuntary deload",
                            {}});
    if (!explosive_offenders.empty())
        r.alerts.push_back({"P3",
                            "explosive lifts logged above 3 reps — bar speed decays and the lift is trained in "
                            "the wrong zone",
                            {explosive_offenders.begin(), explosive_offenders.end()}});
    if (!decay_offenders.empty())
        r.alerts.push_back({"P3", "reps collapse across the ramp on an explosive lift (the 4 / 2 / 1 pattern)",
                            {decay_offenders.begin(), decay_offenders.end()}});
    if (cycles && *cycles >= 1) {
        std::string starved;
        for (const auto& p : r.patterns) {
            if (p.per_cycle >= 1) continue;
            if (!starved.empty()) starved += ", ";
            starved += std::string(name(p.pattern)) + " (" + dart_double(p.per_cycle) + "/cycle)";
        }
        if (!starved.empty())
            r.alerts.push_back({"P8",
                                "trained below once per " + std::to_string(options.cycle_length_days) +
                                    "-day cycle, so linear progression on them is dead: " + starved,
                                {}});
    }
    if (cycles && *cycles >= 2 && !r.kinds.contains(Kind::deload) && !r.kinds.contains(Kind::retest))
        r.alerts.push_back({"P10",
                            "no deload or retest in " + std::to_string(*r.days) +
                                " days — deloads are hygiene, and progress between anchors is the unit of evaluation",
                            {}});

    r.session_count = static_cast<std::int64_t>(ordered.size());
    r.unknown_exercises.assign(unknown.begin(), unknown.end());
    return r;
}

Json TrainingReport::to_json() const {
    Json json = Json::object();
    put(json, "from", from);
    put(json, "to", to);
    put(json, "days", days);
    json["sessions"] = session_count;
    Json kinds_json = Json::object();
    for (size_t i = 0; i < enum_count<Kind>(); ++i) {
        auto kind = static_cast<Kind>(i);
        if (auto it = kinds.find(kind); it != kinds.end() && it->second > 0)
            kinds_json[std::string(name(kind))] = it->second;
    }
    json["kinds"] = std::move(kinds_json);
    Json bands = Json::object();
    bands["sets"] = band_json(band_sets);
    bands["slots"] = band_json(band_slots);
    json["rep_bands"] = std::move(bands);
    Json patterns_json = Json::array();
    for (const auto& p : patterns) patterns_json.push_back(p.to_json());
    json["patterns"] = std::move(patterns_json);
    Json domains = Json::object();
    for (auto d : {TimeDomain::alactic, TimeDomain::glycolytic, TimeDomain::aerobic})
        if (auto it = time_domains.find(d); it != time_domains.end() && it->second > 0)
            domains[std::string(time_domain_name(d))] = it->second;
    json["time_domains"] = std::move(domains);
    json["total_tonnage_kg"] = total_tonnage_kg();
    Json load_json = Json::array();
    for (const auto& p : load) load_json.push_back(p.to_json());
    json["load"] = std::move(load_json);
    Json alerts_json = Json::array();
    for (const auto& a : alerts) alerts_json.push_back(a.to_json());
    json["alerts"] = std::move(alerts_json);
    if (!unknown_exercises.empty()) json["unknown_exercises"] = strings(unknown_exercises);
    return json;
}

namespace one_rep_max {

std::optional<double> epley(std::optional<double> w, std::optional<std::int64_t> reps) {
    if (!w || !reps || *reps < 1 || *w <= 0) return std::nullopt;
    return *reps == 1 ? *w : *w * (1 + static_cast<double>(*reps) / 30);
}

std::optional<double> brzycki(std::optional<double> w, std::optional<std::int64_t> reps) {
    if (!w || !reps || *reps < 1 || *reps >= 37 || *w <= 0) return std::nullopt;
    return *w * 36 / static_cast<double>(37 - *reps);
}

std::optional<double> estimate(std::optional<double> w, std::optional<std::int64_t> reps) {
    auto e = epley(w, reps);
    auto b = brzycki(w, reps);
    if (!e) return std::nullopt;
    if (!b) return round_to(*e);
    return round_to((*e + *b) / 2);
}

} // namespace one_rep_max

Json ProgressPoint::to_json() const {
    Json json = Json::object();
    json["date"] = date;
    json["cycle_day"] = cycle_day;
    if (kind != Kind::training) json["kind"] = std::string(name(kind));
    put(json, "weight_kg", weight_kg);
    put(json, "reps", reps);
    put(json, "estimated_1rm_kg", estimated_1rm_kg());
    put(json, "rir", rir);
    put_enum(json, "bar_speed", bar_speed);
    json["sets"] = sets;
    if (backoff_sets > 0) {
        json["backoff_sets"] = backoff_sets;
        json["backoff_tonnage_kg"] = round_to(backoff_tonnage_kg);
    }
    return json;
}

std::optional<double> ProgressSeries::trend_kg() const {
    std::vector<double> estimates;
    for (const auto& p : points)
        if (auto e = p.estimated_1rm_kg()) estimates.push_back(*e);
    if (estimates.size() < 2) return std::nullopt;
    return round_to(estimates.back() - estimates.front());
}

Json ProgressSeries::to_json() const {
    Json json = Json::object();
    json["exercise"] = exercise;
    put_enum(json, "rep_band", band);
    put(json, "trend_1rm_kg", trend_kg());
    Json list = Json::array();
    for (const auto& p : points) list.push_back(p.to_json());
    json["points"] = std::move(list);
    return json;
}

namespace {

ProgressPoint point_for(const Session& session, const std::vector<const WorkSet*>& sets) {
    const WorkSet* top = nullptr;
    ProgressPoint p;
    for (const WorkSet* set : sets) {
        if (set->backoff()) {
            ++p.backoff_sets;
            p.backoff_tonnage_kg += set->tonnage_kg();
            continue;
        }
        if (!top || set->weight_kg.value_or(0) >= top->weight_kg.value_or(0)) top = set;
    }
    // A slot of nothing but back-offs still has a heaviest set; reporting none
    // would drop the session from the track entirely.
    if (!top) {
        top = sets.front();
        for (size_t i = 1; i < sets.size(); ++i)
            if (sets[i]->weight_kg.value_or(0) >= top->weight_kg.value_or(0)) top = sets[i];
    }
    p.date = session.date;
    p.cycle_day = session.cycle_day;
    p.kind = session.kind;
    p.weight_kg = top->weight_kg;
    p.reps = top->recorded_reps();
    p.rir = top->rir;
    p.bar_speed = top->bar_speed;
    p.sets = static_cast<std::int64_t>(sets.size());
    return p;
}

} // namespace

ProgressReport ProgressReport::of(const std::vector<Session>& sessions, const std::string& exercise,
                                  const Catalogue* catalogue, bool by_pattern) {
    const Exercise* resolved = catalogue ? catalogue->resolve(exercise) : nullptr;
    std::optional<MovementPattern> pattern = by_pattern && resolved ? resolved->pattern : std::nullopt;
    std::string query_key = utf8::to_lower(utf8::trim(exercise));

    auto matches = [&](const std::string& n) {
        const Exercise* canonical = catalogue ? catalogue->resolve(n) : nullptr;
        if (pattern) return canonical && canonical->pattern == pattern;
        if (canonical && resolved) return canonical->name == resolved->name;
        return utf8::to_lower(utf8::trim(n)) == query_key;
    };

    // Keyed (name, band index) with null as -1: the same order Dart's sort gave.
    std::map<std::pair<std::string, int>, std::vector<ProgressPoint>> tracks;
    std::set<std::string> variants;

    for (const auto& session : by_date(sessions)) {
        for (const auto& block : session.blocks) {
            const auto* strength = std::get_if<StrengthBlock>(&block);
            if (!strength || !matches(strength->exercise)) continue;
            const Exercise* canonical = catalogue ? catalogue->resolve(strength->exercise) : nullptr;
            std::string variant = canonical ? canonical->name : strength->exercise;
            variants.insert(variant);

            // One slot can feed several bands: a ramp of sixes topped with a
            // triple is a point on both tracks, each with its own top set.
            std::vector<std::pair<std::optional<RepBand>, std::vector<const WorkSet*>>> by_band;
            for (const auto& set : strength->sets) {
                auto band = set.band();
                auto it = std::find_if(by_band.begin(), by_band.end(), [&](auto& e) { return e.first == band; });
                if (it == by_band.end()) {
                    by_band.push_back({band, {}});
                    it = std::prev(by_band.end());
                }
                it->second.push_back(&set);
            }
            for (const auto& [band, sets] : by_band)
                tracks[{variant, band ? static_cast<int>(*band) : -1}].push_back(point_for(session, sets));
        }
    }

    ProgressReport report;
    report.query = exercise;
    if (resolved) report.resolved = resolved->name;
    report.pattern = pattern ? pattern : (resolved ? resolved->pattern : std::nullopt);
    report.variants.assign(variants.begin(), variants.end());
    for (auto& [key, points] : tracks) {
        std::optional<RepBand> band;
        if (key.second >= 0) band = static_cast<RepBand>(key.second);
        report.series.push_back({key.first, band, std::move(points)});
    }
    return report;
}

Json ProgressReport::to_json() const {
    Json json = Json::object();
    json["query"] = query;
    put(json, "resolved", resolved);
    put_enum(json, "pattern", pattern);
    if (variants.size() > 1) json["variants"] = strings(variants);
    Json list = Json::array();
    for (const auto& s : series) list.push_back(s.to_json());
    json["series"] = std::move(list);
    return json;
}

} // namespace wl
