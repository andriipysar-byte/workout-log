#include <doctest/doctest.h>

#include "fixtures.hpp"
#include "workoutlog/analytics.hpp"

using namespace wl;

namespace {

Session squat_session(const std::string& date, const std::string& cycle_day, const std::string& sets,
                      const std::string& exercise = "присід фронтальний") {
    Json json{{"date", date}, {"cycle_day", cycle_day}};
    json["blocks"] = Json::array({Json{{"type", "strength"}, {"exercise", exercise}, {"sets", Json::parse(sets)}}});
    return Session::from_json(json);
}

} // namespace

TEST_CASE("one_rep_max: a single is its own maximum") {
    CHECK(one_rep_max::epley(100.0, 1) == 100);
    CHECK(one_rep_max::estimate(100.0, 1) == 100);
}

TEST_CASE("one_rep_max: estimates sit between Epley and Brzycki") {
    auto epley = one_rep_max::epley(100.0, 6);
    auto brzycki = one_rep_max::brzycki(100.0, 6);
    auto estimate = one_rep_max::estimate(100.0, 6);
    REQUIRE(epley);
    REQUIRE(brzycki);
    REQUIRE(estimate);
    CHECK(*estimate > *brzycki);
    CHECK(*estimate < *epley);
}

TEST_CASE("one_rep_max: a bodyweight or unrecorded set has no estimate") {
    CHECK_FALSE(one_rep_max::estimate(std::nullopt, 6));
    CHECK_FALSE(one_rep_max::estimate(100.0, std::nullopt));
    CHECK_FALSE(one_rep_max::estimate(100.0, 0));
}

TEST_CASE("progress: splits one lift into a track per rep band (P6)") {
    auto catalogue = fixtures::catalogue();
    auto report = ProgressReport::of(
        {squat_session("2026-07-01", "C2", R"([{"reps": 6, "weight_kg": 100}, {"reps": 3, "weight_kg": 110}])"),
         squat_session("2026-07-13", "C2", R"([{"reps": 6, "weight_kg": 105}, {"reps": 3, "weight_kg": 112.5}])")},
        "присід фронтальний", &catalogue);

    std::vector<std::optional<RepBand>> bands;
    for (const auto& s : report.series) bands.push_back(s.band);
    CHECK(bands == std::vector<std::optional<RepBand>>{RepBand::heavy, RepBand::base});
    const ProgressSeries* base = nullptr;
    for (const auto& s : report.series)
        if (s.band == RepBand::base) base = &s;
    REQUIRE(base);
    std::vector<std::optional<double>> weights;
    for (const auto& p : base->points) weights.push_back(p.weight_kg);
    CHECK(weights == std::vector<std::optional<double>>{100, 105});
    REQUIRE(base->trend_kg());
    CHECK(*base->trend_kg() > 0);
}

TEST_CASE("progress: the top set is the signal, back-offs are volume beside it (P2)") {
    auto catalogue = fixtures::catalogue();
    auto report = ProgressReport::of({squat_session("2026-07-01", "C2", R"([
        {"reps": 6, "weight_kg": 90}, {"reps": 6, "weight_kg": 100},
        {"reps": 6, "weight_kg": 70, "is_backoff": true}])")},
                                     "присід фронтальний", &catalogue);
    REQUIRE(report.series.size() == 1);
    REQUIRE(report.series[0].points.size() == 1);
    const auto& point = report.series[0].points[0];
    CHECK(point.weight_kg == 100);
    CHECK(point.backoff_sets == 1);
    CHECK(point.backoff_tonnage_kg == 420);
    CHECK(point.sets == 3);
}

TEST_CASE("progress: an alias resolves to the canonical lift") {
    auto catalogue = fixtures::catalogue();
    auto report = ProgressReport::of(
        {squat_session("2026-07-01", "C2", R"([{"reps": 6, "weight_kg": 100}])", "фр. присід")},
        "присід фронтальний", &catalogue);
    CHECK(report.resolved == "присід фронтальний");
    REQUIRE(report.series.size() == 1);
    CHECK(report.series[0].points.size() == 1);
}

TEST_CASE("progress: by_pattern gathers the variants of one pattern (P8)") {
    auto catalogue = fixtures::catalogue();
    std::vector<Session> sessions{
        squat_session("2026-07-01", "C2", R"([{"reps": 6, "weight_kg": 100}])"),
        squat_session("2026-07-08", "C2", R"([{"reps": 6, "weight_kg": 80}])", "присід на плечах"),
    };
    auto single = ProgressReport::of(sessions, "присід фронтальний", &catalogue);
    auto by_pattern = ProgressReport::of(sessions, "присід фронтальний", &catalogue, true);

    CHECK(single.variants == std::vector<std::string>{"присід фронтальний"});
    CHECK(by_pattern.pattern == MovementPattern::squat);
    CHECK(by_pattern.variants.size() == 2);
    CHECK(by_pattern.series.size() == 2);
}

TEST_CASE("progress: an exercise the catalogue does not know is still tracked") {
    auto catalogue = fixtures::catalogue();
    auto report = ProgressReport::of(
        {squat_session("2026-07-01", "C2", R"([{"reps": 5, "weight_kg": 60}])", "вправа якої немає")},
        "вправа якої немає", &catalogue);
    CHECK_FALSE(report.resolved.has_value());
    REQUIRE(report.series.size() == 1);
    REQUIRE(report.series[0].points.size() == 1);
    CHECK(report.series[0].points[0].weight_kg == 60);
}
