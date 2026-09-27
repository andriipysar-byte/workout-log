#include "workoutlog/cycle_planning.hpp"

#include <algorithm>
#include <set>

#include "workoutlog/utf8.hpp"

namespace wl {

std::string_view day_kind_label(DayKind kind) {
    return kind == DayKind::conditioning ? "CrossFit" : "Hard work";
}

std::optional<CycleDay> CycleDay::try_parse(std::string_view raw) {
    std::string code = utf8::trim(raw);
    if (code.size() != 2) return std::nullopt;
    char letter = code[0];
    bool alpha = (letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z');
    if (!alpha || (code[1] != '1' && code[1] != '2')) return std::nullopt;
    if (letter >= 'a') letter = static_cast<char>(letter - 32);
    return CycleDay{letter, code[1] == '1' ? DayKind::conditioning : DayKind::heavy};
}

std::string CycleDay::code() const {
    return std::string(1, letter) + (kind == DayKind::conditioning ? "1" : "2");
}

CycleDay CycleDay::next() const {
    if (kind == DayKind::conditioning) return {letter, DayKind::heavy};
    return {static_cast<char>(letter + 1), DayKind::conditioning};
}

std::string_view weekday_abbreviation(Date date) {
    return kWeekdayAbbreviations[static_cast<size_t>(iso_weekday(date) - 1)];
}

namespace {

std::string list_text(const std::vector<std::string>& items) {
    std::string out = "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += ", ";
        out += items[i];
    }
    return out + "]";
}

} // namespace

std::vector<Date> training_dates(Date start, const std::vector<std::string>& training_days, size_t count) {
    std::set<std::string_view> valid;
    for (const auto& d : training_days)
        if (std::find(kWeekdayAbbreviations.begin(), kWeekdayAbbreviations.end(), d) != kWeekdayAbbreviations.end())
            valid.insert(d);
    if (valid.empty() && count > 0) {
        std::vector<std::string> expected(kWeekdayAbbreviations.begin(), kWeekdayAbbreviations.end());
        throw CycleGeneratorError("no recognised training days in " + list_text(training_days) +
                                  " (expected any of " + list_text(expected) + ")");
    }
    std::vector<Date> out;
    for (Date day = start; out.size() < count; day += std::chrono::days{1})
        if (valid.contains(weekday_abbreviation(day))) out.push_back(day);
    return out;
}

std::vector<Session> generate_cycle(const Cycle& cycle) {
    auto start = parse_date(cycle.start_date);
    if (!start) throw CycleGeneratorError("Invalid date format\n" + cycle.start_date);
    auto dates = training_dates(*start, cycle.training_days, cycle.sessions.size());
    std::vector<Session> out;
    for (size_t i = 0; i < cycle.sessions.size(); ++i) out.push_back(session_from_template(cycle.sessions[i], dates[i]));
    return out;
}

namespace {

std::optional<std::string> non_empty(const std::optional<std::string>& s) {
    return s && !s->empty() ? s : std::nullopt;
}

} // namespace

Session session_from_template(const CycleSession& tmpl, Date when, bool enforce_weekday) {
    std::string_view actual = weekday_abbreviation(when);
    if (enforce_weekday && tmpl.weekday && *tmpl.weekday != actual)
        throw CycleGeneratorError(tmpl.cycle_day + ": calendar says " + std::string(actual) +
                                  " but template says " + *tmpl.weekday);
    Session s;
    s.date = iso_date(when);
    s.cycle_day = tmpl.cycle_day;
    s.notes = non_empty(tmpl.session_notes);
    for (const auto& b : tmpl.blocks) s.blocks.push_back(block_from_template(b));
    return s;
}

Block block_from_template(const BlockTemplate& tmpl) {
    auto notes = non_empty(tmpl.notes);
    if (tmpl.type == "cardio") return CardioBlock{tmpl.machine.value_or(""), tmpl.duration_min, std::nullopt, std::nullopt};
    if (tmpl.type == "strength") {
        if (!tmpl.exercise || tmpl.exercise->empty())
            throw CycleGeneratorError("a strength block in this cycle has no exercise yet");
        StrengthBlock b;
        b.exercise = *tmpl.exercise;
        for (auto reps : tmpl.sets_reps) {
            WorkSet set;
            set.reps = reps;
            b.sets.push_back(set);
        }
        b.notes = notes;
        return b;
    }
    if (tmpl.type == "metcon") {
        MetconBlock b;
        b.format = tmpl.format;
        b.scheme = tmpl.scheme;
        b.exercises = tmpl.exercises;
        b.notes = notes;
        return b;
    }
    if (tmpl.type == "cooldown") return CooldownBlock{};
    throw CycleGeneratorError("unknown block type: \"" + tmpl.type + "\"");
}

Session preview_session(const CycleSession& tmpl, std::optional<Date> on) {
    using namespace std::chrono;
    Date when = on.value_or(sys_days{year{2000} / January / 1});
    Session s;
    s.date = iso_date(when);
    s.cycle_day = tmpl.cycle_day;
    for (const auto& b : tmpl.blocks) {
        try {
            Block built = block_from_template(b);
            if (auto* strength = std::get_if<StrengthBlock>(&built); strength && strength->sets.empty())
                strength->sets.push_back(WorkSet{});
            s.blocks.push_back(std::move(built));
        } catch (const CycleGeneratorError&) {
        }
    }
    return s;
}

namespace cycle_templates {

namespace {

BlockTemplate tmpl(std::string type, std::string role) {
    BlockTemplate b;
    b.type = std::move(type);
    b.role = std::move(role);
    return b;
}

BlockTemplate with_sets(BlockTemplate b, IntList sets) {
    b.sets_reps = std::move(sets);
    return b;
}

BlockTemplate cardio(double minutes) {
    BlockTemplate b = tmpl("cardio", "warmup");
    b.machine = "";
    b.duration_min = minutes;
    return b;
}

} // namespace

// Lifted from the shape of the hybrid-8 cycle: a 1 day is short warm-up,
// explosive lift, metcon; a 2 day is a longer warm-up, hyperextension, a main
// lift and accessories.
std::vector<BlockTemplate> blocks_for(DayKind kind) {
    if (kind == DayKind::conditioning)
        return {cardio(10),
                tmpl("strength", "explosive"),
                tmpl("metcon", "metcon"),
                with_sets(tmpl("strength", "accessory"), {6, 6, 6, 6}),
                with_sets(tmpl("strength", "grip"), {8, 8, 8, 8}),
                tmpl("cooldown", "cooldown")};
    BlockTemplate hyper = with_sets(tmpl("strength", "warmup"), {12, 12});
    hyper.exercise = "гіперекстензія";
    return {cardio(15),
            hyper,
            tmpl("strength", "main"),
            with_sets(tmpl("strength", "accessory"), {6, 6, 6, 6, 6}),
            with_sets(tmpl("strength", "accessory"), {6, 6, 6, 6, 6}),
            with_sets(tmpl("strength", "grip"), {8, 8, 8, 8}),
            tmpl("cooldown", "cooldown")};
}

CycleSession session(CycleDay day, std::optional<std::int64_t> week, std::optional<std::string> weekday) {
    CycleSession s;
    s.cycle_day = day.code();
    s.week = week;
    s.weekday = std::move(weekday);
    s.type = day.kind == DayKind::conditioning ? "metcon" : "heavy";
    s.blocks = blocks_for(day.kind);
    return s;
}

CycleDay next_day(const std::vector<std::string>& existing_codes) {
    std::optional<CycleDay> highest;
    for (const auto& code : existing_codes)
        if (auto day = CycleDay::try_parse(code); day && (!highest || *highest < *day)) highest = day;
    return highest ? highest->next() : CycleDay{'A', DayKind::conditioning};
}

} // namespace cycle_templates

} // namespace wl
