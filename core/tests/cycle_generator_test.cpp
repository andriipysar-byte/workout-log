#include <doctest/doctest.h>

#include <regex>

#include "fixtures.hpp"
#include "workoutlog/cycle_planning.hpp"
#include "workoutlog/session_store.hpp"

using namespace wl;
using namespace std::chrono;

namespace {

Cycle hybrid8(std::int64_t version) {
    auto catalogue = fixtures::cycles();
    const Cycle* cycle = catalogue.by_id("hybrid-8", version);
    REQUIRE(cycle);
    return *cycle;
}

constexpr Date on_2026(unsigned m, unsigned d) { return Date{year{2026} / month{m} / d}; }

} // namespace

TEST_CASE("training dates are the first N on/after start on a training weekday") {
    std::vector<std::string> iso;
    for (auto d : training_dates(on_2026(7, 21), {"Tue", "Thu", "Sun"}, 4)) iso.push_back(iso_date(d));
    CHECK(iso == std::vector<std::string>{"2026-07-21", "2026-07-23", "2026-07-26", "2026-07-28"});
}

TEST_CASE("a start date that is itself a training day counts as the first") {
    auto dates = training_dates(on_2026(7, 20), {"Tue"}, 1);
    REQUIRE(dates.size() == 1);
    CHECK(iso_date(dates[0]) == "2026-07-21");
}

TEST_CASE("unrecognised training days fail instead of looping forever") {
    CHECK_THROWS_AS(training_dates(on_2026(7, 21), {"Xyz"}, 1), CycleGeneratorError);
}

TEST_CASE("a template weekday that disagrees with the calendar is a hard error") {
    CHECK_THROWS_AS(session_from_template(CycleSession{.cycle_day = "A1", .weekday = "Mon"}, on_2026(7, 21)),
                    CycleGeneratorError);
}

TEST_CASE("a single session may opt out of the weekday check") {
    auto session = session_from_template(CycleSession{.cycle_day = "A1", .weekday = "Mon"}, on_2026(7, 21), false);
    CHECK(session.date == "2026-07-21");
}

TEST_CASE("hybrid-8 v1 expands onto the dates already in data/") {
    std::vector<std::string> ids;
    for (const auto& s : generate_cycle(hybrid8(1))) ids.push_back(SessionStore::id_for(s));
    CHECK(ids == std::vector<std::string>{"2026-07-21_A1.json", "2026-07-23_A2.json", "2026-07-26_B1.json",
                                          "2026-07-28_B2.json", "2026-07-30_C1.json", "2026-08-02_C2.json",
                                          "2026-08-04_D1.json", "2026-08-06_D2.json"});
}

TEST_CASE("hybrid-8 v2 puts both squats on a Sunday and never follows a workout with its own letter") {
    auto sessions = generate_cycle(hybrid8(2));
    std::vector<std::string> ids;
    for (const auto& s : sessions) ids.push_back(SessionStore::id_for(s));
    CHECK(ids == std::vector<std::string>{"2026-09-29_A1.json", "2026-10-01_B2.json", "2026-10-04_A2.json",
                                          "2026-10-06_B1.json", "2026-10-08_D2.json", "2026-10-11_C2.json",
                                          "2026-10-13_D1.json", "2026-10-15_C1.json"});
    for (size_t i = 1; i < sessions.size(); ++i)
        CHECK_MESSAGE(sessions[i].cycle_day[0] != sessions[i - 1].cycle_day[0], sessions[i].cycle_day);
}

TEST_CASE("generated stubs are schema-shaped and re-encode unchanged") {
    const std::regex iso(R"(^\d{4}-\d{2}-\d{2}$)");
    for (const auto& session : generate_cycle(hybrid8(2))) {
        CHECK(session.kind == Kind::training);
        CHECK(std::regex_search(session.date, iso));
        CHECK_FALSE(session.blocks.empty());
        CHECK(decode_session(encode_session(session)) == session);
    }
}

TEST_CASE("planning-only fields never reach the output") {
    for (const auto& session : generate_cycle(hybrid8(2))) {
        auto encoded = encode_session(session);
        CHECK(encoded.find("sets_reps") == std::string::npos);
        CHECK(encoded.find("\"role\"") == std::string::npos);
        CHECK(encoded.find("weekday") == std::string::npos);
        CHECK(encoded.find("\"title\"") == std::string::npos);
    }
}

TEST_CASE("sets_reps becomes one rep-only set each") {
    auto block = std::get<StrengthBlock>(
        block_from_template(BlockTemplate{.type = "strength", .exercise = "гіперекстензія", .sets_reps = {12, 12}}));
    REQUIRE(block.sets.size() == 2);
    for (const auto& s : block.sets) {
        CHECK(s.reps == 12);
        CHECK_FALSE(s.weight_kg.has_value());
    }
}

TEST_CASE("a strength template with no sets_reps still emits an empty sets list") {
    auto json = block_to_json(block_from_template(BlockTemplate{.type = "strength", .exercise = "жим стоячи"}));
    REQUIRE(json.contains("sets"));
    CHECK(json["sets"].is_array());
    CHECK(json["sets"].empty());
}

TEST_CASE("metcon templates carry format, scheme and exercises through") {
    Block block = block_from_template(BlockTemplate{.type = "metcon",
                                                    .format = MetconFormat::for_time,
                                                    .scheme = IntList{21, 15, 9},
                                                    .exercises = {MetconExercise{.name = "трастери"}}});
    const auto& metcon = std::get<MetconBlock>(block);
    CHECK(metcon.format == MetconFormat::for_time);
    CHECK(metcon.scheme == IntList{21, 15, 9});
    CHECK(block_to_json(block)["format"] == "for_time");
}

TEST_CASE("an unknown template block type is rejected") {
    CHECK_THROWS_AS(block_from_template(BlockTemplate{.type = "yoga"}), CycleGeneratorError);
}

TEST_CASE("generating twice is deterministic") {
    auto cycle = hybrid8(2);
    std::vector<std::string> first, second;
    for (const auto& s : generate_cycle(cycle)) first.push_back(encode_session(s));
    for (const auto& s : generate_cycle(cycle)) second.push_back(encode_session(s));
    CHECK(first == second);
}
