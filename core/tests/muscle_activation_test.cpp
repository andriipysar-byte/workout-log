#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

#include "fixtures.hpp"
#include "workoutlog/muscles.hpp"

using namespace wl;

namespace {

Session real_session() {
    for (const auto& f : fixtures::session_files())
        if (f.filename() == "2026-06-23_C2.json") return decode_session(fixtures::read(f));
    FAIL("2026-06-23_C2.json not found in data/");
    return {};
}

bool has(const Scores& scores, std::string_view muscle) {
    return std::any_of(scores.begin(), scores.end(), [&](const auto& e) { return e.first == muscle; });
}

// First maximum wins, as the Dart reduce `a.value >= b.value ? a : b` keeps it.
std::string peak(const Scores& scores) {
    auto it = scores.begin();
    for (auto e = scores.begin(); e != scores.end(); ++e)
        if (e->second > it->second) it = e;
    return it->first;
}

} // namespace

TEST_CASE("per-exercise map: primaries 1.0, secondaries 0.5") {
    auto catalogue = fixtures::catalogue();
    const auto* front_squat = catalogue.resolve("присід фронтальний");
    REQUIRE(front_squat);
    auto map = muscle_activation::for_exercise(*front_squat);
    CHECK(map.get("quads") == 1.0);
    CHECK(map.get("glutes") == 1.0);
    CHECK(map.get("spinal_erectors") == 0.5);
}

TEST_CASE("over a real session: each mode normalizes to a peak of 1.0") {
    auto catalogue = fixtures::catalogue();
    auto session = real_session();
    for (auto mode : kWeightingModes) {
        CAPTURE(weighting_wire(mode));
        auto map = muscle_activation::for_session(session, catalogue, mode);
        REQUIRE_FALSE(map.empty());
        double max = std::max_element(map.begin(), map.end(), [](const auto& a, const auto& b) {
                         return a.second < b.second;
                     })->second;
        CHECK(std::abs(max - 1.0) <= 1e-9);
    }
}

TEST_CASE("over a real session: the modes surface different hottest muscles") {
    auto catalogue = fixtures::catalogue();
    auto session = real_session();
    auto peak_of = [&](WeightingMode mode) {
        auto map = muscle_activation::for_session(session, catalogue, mode);
        REQUIRE_FALSE(map.empty());
        return peak(map);
    };
    CHECK_MESSAGE(peak_of(WeightingMode::set_count) == "forearms", "grip-set heavy day");
    auto rep_volume = peak_of(WeightingMode::rep_volume);
    CHECK((rep_volume == "quads" || rep_volume == "glutes"));
    auto tonnage = peak_of(WeightingMode::tonnage);
    CHECK((tonnage == "quads" || tonnage == "glutes"));
    CHECK(peak_of(WeightingMode::set_count) != tonnage);
}

TEST_CASE("an empty session produces an empty map") {
    auto map = muscle_activation::for_session(Session{.date = "2026-01-01", .cycle_day = "A1"},
                                              fixtures::catalogue(), WeightingMode::set_count);
    CHECK(map.empty());
}

TEST_CASE("unknown exercise names contribute nothing") {
    Session session{.date = "2026-01-01", .cycle_day = "A1"};
    session.blocks.push_back(StrengthBlock{.exercise = "вправа якої немає", .sets = {WorkSet{.reps = 5}}});
    CHECK(muscle_activation::for_session(session, fixtures::catalogue(), WeightingMode::set_count).empty());
}

TEST_CASE("cycle totals sum raw volume before normalizing once") {
    Session heavy{.date = "2026-01-01", .cycle_day = "A1"};
    heavy.blocks.push_back(StrengthBlock{.exercise = "присід фронтальний",
                                         .sets = std::vector<WorkSet>(10, WorkSet{.weight_kg = 100.0, .reps = 6})});
    Session light{.date = "2026-01-02", .cycle_day = "A2"};
    light.blocks.push_back(StrengthBlock{.exercise = "жим лежачи", .sets = {WorkSet{.weight_kg = 40.0, .reps = 6}}});
    auto map = muscle_activation::for_sessions({heavy, light}, fixtures::catalogue(), WeightingMode::tonnage);
    CHECK(map.get("quads") == 1.0);
    REQUIRE(has(map, "chest"));
    CHECK(map.get("chest") < 0.1);
}

TEST_CASE("WeightingMode round-trips through its wire name") {
    for (auto mode : kWeightingModes) CHECK(weighting_from_wire(weighting_wire(mode)) == mode);
    CHECK_FALSE(weighting_from_wire("nonsense"));
}
