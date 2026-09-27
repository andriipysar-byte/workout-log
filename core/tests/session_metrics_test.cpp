#include <doctest/doctest.h>

#include <cmath>

#include "workoutlog/analytics.hpp"

using namespace wl;

namespace {

// The session from docs/02-data-model.md, a real transcription: if the metrics
// cannot read that shape they cannot read the archive.
Session example_session() {
    return Session::from_json(Json::parse(R"({
      "date": "2026-08-10", "cycle_day": "F1", "start_time": "08:02", "kind": "training",
      "blocks": [
        {"type": "cardio", "machine": "велотренажер", "duration_min": 15, "distance_m": 6440,
         "end_time": "08:18"},
        {"type": "strength", "exercise": "гіперекстензія",
         "sets": [{"reps": 12, "weight_kg": 15}, {"reps": 12, "weight_kg": 30}], "end_time": "08:28"},
        {"type": "metcon", "scheme": [21, 15, 9], "format": "for_time", "start_time": "08:30",
         "exercises": [{"name": "трастери", "weight_kg": 50}, {"name": "бьорпі"}],
         "rounds": [
           {"round": 1, "reps": 21, "split_cumulative_sec": 463, "heart_rate": 156},
           {"round": 2, "reps": 15, "split_cumulative_sec": 1155, "heart_rate": 178},
           {"round": 3, "reps": 9, "split_cumulative_sec": 1450, "heart_rate": 189}],
         "end_time": "08:54"},
        {"type": "strength", "exercise": "присід фронтальний",
         "sets": [{"reps": 6, "weight_kg": 90}, {"reps": 3, "weight_kg": 110},
                  {"reps": 6, "weight_kg": 70, "is_backoff": true}],
         "end_time": "09:12"},
        {"type": "cooldown", "end_time": "09:20"}
      ]})"));
}

} // namespace

TEST_CASE("sums tonnage and separates back-off sets") {
    auto metrics = SessionMetrics::of(example_session());
    CHECK(metrics.tonnage_kg == 12 * 15 + 12 * 30 + 6 * 90 + 3 * 110 + 6 * 70);
    CHECK(metrics.sets == 5);
    CHECK(metrics.backoff_sets == 1);
    CHECK(metrics.top_sets() == 4);
}

TEST_CASE("counts rep bands by set and by slot") {
    auto metrics = SessionMetrics::of(example_session());
    CHECK(metrics.band_sets[RepBand::volume] == 2);
    CHECK(metrics.band_sets[RepBand::base] == 2);
    CHECK(metrics.band_sets[RepBand::heavy] == 1);
    // The front squat slot is two sixes to one triple: a base slot.
    CHECK(metrics.band_slots[RepBand::base] == 1);
    CHECK(metrics.band_slots[RepBand::volume] == 1);
}

TEST_CASE("derives density from the bracket timestamps") {
    auto density = SessionMetrics::of(example_session()).density;
    CHECK(density.total_min == 78);
    // 16 + 10 + 24 + 18 + 8, with the two minutes before the metcon a transition.
    CHECK(density.work_min == 76);
    CHECK(density.transition_min == 2);
    REQUIRE(density.work_share());
    CHECK(std::abs(*density.work_share() - 0.974) <= 0.001);
}

TEST_CASE("differences the cumulative metcon splits") {
    auto metrics = SessionMetrics::of(example_session());
    REQUIRE(metrics.metcons.size() == 1);
    const auto& metcon = metrics.metcons[0];
    std::vector<std::optional<double>> splits;
    for (const auto& r : metcon.rounds) splits.push_back(r.split_sec);
    CHECK(splits == std::vector<std::optional<double>>{463, 692, 295});
    REQUIRE(metcon.rounds[0].seconds_per_rep());
    CHECK(std::abs(*metcon.rounds[0].seconds_per_rep() - 22.05) <= 0.01);
    CHECK(metcon.total_sec == 1450);
    CHECK(metcon.domain() == TimeDomain::aerobic);
    CHECK(metcon.peak_heart_rate() == 189);
    CHECK(metcon.pace_decay_percent() == 48.7);
}

TEST_CASE("a planned session with no times and no weights reads as empty") {
    auto metrics = SessionMetrics::of(Session::from_json(Json::parse(R"({
      "date": "2026-08-04", "cycle_day": "D1",
      "blocks": [{"type": "strength", "exercise": "ривок", "sets": []}]})")));
    CHECK(metrics.tonnage_kg == 0);
    CHECK(metrics.band_slots.empty());
    CHECK_FALSE(metrics.density.total_min.has_value());
    CHECK_FALSE(metrics.to_json()["density"].contains("total_min"));
}

TEST_CASE("a cardio block with no timestamps still contributes its minutes") {
    auto metrics = SessionMetrics::of(Session::from_json(Json::parse(R"({
      "date": "2026-08-04", "cycle_day": "D1",
      "blocks": [{"type": "cardio", "machine": "", "duration_min": 10}]})")));
    CHECK(metrics.density.work_min == 10);
}
