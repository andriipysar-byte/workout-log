#include <doctest/doctest.h>

#include <algorithm>

#include "fixtures.hpp"
#include "workoutlog/analytics.hpp"

using namespace wl;

namespace {

Session session_of(const std::string& date, const std::string& cycle_day, Json blocks,
                   const std::string& kind = "training") {
    return Session::from_json(
        Json{{"date", date}, {"cycle_day", cycle_day}, {"kind", kind}, {"blocks", std::move(blocks)}});
}

Json strength(const std::string& exercise, std::vector<int> reps, double weight) {
    Json sets = Json::array();
    for (int r : reps) sets.push_back(Json{{"reps", r}, {"weight_kg", weight}});
    return Json{{"type", "strength"}, {"exercise", exercise}, {"sets", sets}};
}

std::vector<std::string> principles(const TrainingReport& report) {
    std::vector<std::string> out;
    for (const auto& a : report.alerts) out.push_back(a.principle);
    return out;
}

bool contains(const std::vector<std::string>& list, const std::string& item) {
    return std::find(list.begin(), list.end(), item) != list.end();
}

} // namespace

TEST_CASE("training report: flags a block where volume slots take over (P5)") {
    auto report = TrainingReport::of(
        {session_of("2026-07-01", "A2",
                    Json::array({strength("присід фронтальний", {8, 8, 8}, 80),
                                 strength("румунська тяга", {8, 8, 8}, 70)})),
         session_of("2026-07-03", "B2", Json::array({strength("жим лежачи", {8, 8, 8}, 60)}))},
        fixtures::catalogue());

    auto alert = std::find_if(report.alerts.begin(), report.alerts.end(),
                              [](const auto& a) { return a.principle == "P5"; });
    REQUIRE(alert != report.alerts.end());
    CHECK(alert->message.find("100%") != std::string::npos);
    CHECK(report.band_slots[RepBand::volume] == 3);
}

TEST_CASE("training report: a mixed block stays quiet") {
    auto report = TrainingReport::of(
        {session_of("2026-07-01", "A2",
                    Json::array({strength("присід фронтальний", {3, 3, 3}, 110),
                                 strength("румунська тяга", {6, 6, 6}, 90),
                                 strength("жим лежачи", {6, 6, 6}, 70)}))},
        fixtures::catalogue());
    CHECK_FALSE(contains(principles(report), "P5"));
}

TEST_CASE("training report: flags an explosive lift trained at six reps (P3)") {
    auto report = TrainingReport::of(
        {session_of("2026-07-01", "A1", Json::array({strength("ривок", {6, 6}, 60)}))}, fixtures::catalogue());
    CHECK(contains(principles(report), "P3"));
    REQUIRE_FALSE(report.alerts.empty());
    CHECK(report.alerts.front().sessions == std::vector<std::string>{"2026-07-01 ривок"});
}

TEST_CASE("training report: flags the 4 / 2 / 1 rep collapse (P3)") {
    auto report = TrainingReport::of(
        {session_of("2026-07-01", "A1", Json::array({strength("швунг", {3, 2, 1}, 70)}))}, fixtures::catalogue());
    auto collapse = std::count_if(report.alerts.begin(), report.alerts.end(),
                                  [](const auto& a) { return a.message.find("collapse") != std::string::npos; });
    CHECK(collapse == 1);
}

TEST_CASE("training report: counts pattern frequency per cycle and flags the starved ones (P8)") {
    auto report = TrainingReport::of(
        {session_of("2026-07-01", "A2", Json::array({strength("присід фронтальний", {6, 6}, 100)})),
         // 24 days is two 12-day cycles, so one squat slot is 0.5 per cycle.
         session_of("2026-07-24", "C2", Json::array({strength("присід фронтальний", {6, 6}, 100)}))},
        fixtures::catalogue());

    auto squat = std::find_if(report.patterns.begin(), report.patterns.end(),
                              [](const auto& p) { return p.pattern == MovementPattern::squat; });
    REQUIRE(squat != report.patterns.end());
    CHECK(squat->slots == 2);
    CHECK(squat->per_cycle == 1.0);
    CHECK(report.days == 24);
    CHECK(contains(principles(report), "P10"));
}

TEST_CASE("training report: combined load keeps strength and conditioning apart (P7)") {
    Json metcon = Json::parse(R"({
      "type": "metcon", "scheme": [21, 15, 9], "exercises": [{"name": "трастери"}],
      "rounds": [{"round": 1, "split_cumulative_sec": 200}, {"round": 2, "split_cumulative_sec": 400},
                 {"round": 3, "split_cumulative_sec": 600}]})");
    auto report = TrainingReport::of(
        {session_of("2026-07-01", "A1", Json::array({strength("присід фронтальний", {6}, 100), metcon}))},
        fixtures::catalogue());

    REQUIRE(report.load.size() == 1);
    const auto& point = report.load[0];
    CHECK(point.tonnage_kg == 600);
    CHECK(point.metcon_sec == 600);
    CHECK(report.time_domains[TimeDomain::glycolytic] == 1);
    CHECK(report.total_tonnage_kg() == 600);
}

TEST_CASE("training report: names the catalogue does not know are surfaced, not dropped") {
    auto report = TrainingReport::of(
        {session_of("2026-07-01", "A2", Json::array({strength("вправа якої немає", {6}, 50)}))},
        fixtures::catalogue());
    CHECK(report.unknown_exercises == std::vector<std::string>{"вправа якої немає"});
}

TEST_CASE("training report: the real archive reports without throwing") {
    auto sessions = fixtures::sessions();
    auto report = TrainingReport::of(sessions, fixtures::catalogue());
    CHECK(report.session_count == static_cast<std::int64_t>(sessions.size()));
    CHECK(report.to_json()["load"].size() == sessions.size());
    // Not asserted empty: the archive really does carry names the catalogue has
    // no entry or alias for, which is the report's job to say out loud.
    CHECK_FALSE(contains(report.unknown_exercises, ""));
}
