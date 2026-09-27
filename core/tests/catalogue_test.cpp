#include <doctest/doctest.h>

#include <set>

#include "fixtures.hpp"
#include "workoutlog/muscles.hpp"

using namespace wl;

namespace {

Scores scores(std::initializer_list<std::pair<std::string, double>> entries) {
    Scores out;
    for (const auto& [muscle, score] : entries) out[muscle] = score;
    return out;
}

} // namespace

TEST_CASE("catalogue is non-empty") {
    CHECK_FALSE(fixtures::catalogue().exercises.empty());
}

TEST_CASE("canonical name resolves with its pattern") {
    auto catalogue = fixtures::catalogue();
    const auto* e = catalogue.resolve("присід фронтальний");
    REQUIRE(e);
    CHECK(e->pattern == MovementPattern::squat);
}

TEST_CASE("alias resolves to the canonical name") {
    auto catalogue = fixtures::catalogue();
    REQUIRE(catalogue.resolve("фр. присід"));
    CHECK(catalogue.resolve("фр. присід")->name == "присід фронтальний");
    REQUIRE(catalogue.resolve("гирі"));
    CHECK(catalogue.resolve("гирі")->name == "махи гирею");
}

TEST_CASE("resolution is case- and whitespace-insensitive") {
    auto catalogue = fixtures::catalogue();
    const auto* e = catalogue.resolve("  ПРИСІД ФРОНТАЛЬНИЙ ");
    REQUIRE(e);
    CHECK(e->name == "присід фронтальний");
}

TEST_CASE("an unknown name returns null (warn-not-block)") {
    CHECK(fixtures::catalogue().resolve("невідома вправа") == nullptr);
}

TEST_CASE("жим лежачи carries chest and triceps as primaries") {
    auto catalogue = fixtures::catalogue();
    const auto* e = catalogue.resolve("жим лежачи");
    REQUIRE(e);
    CHECK(e->primary_muscles == std::vector<std::string>{"chest", "triceps"});
}

TEST_CASE("every exercise has primary muscles") {
    std::vector<std::string> without;
    for (const auto& e : fixtures::catalogue().exercises)
        if (e.primary_muscles.empty()) without.push_back(e.name);
    CHECK(without.empty());
}

TEST_CASE("every catalogue muscle maps to a group") {
    std::set<std::string> ungrouped;
    for (const auto& e : fixtures::catalogue().exercises) {
        for (const auto& m : e.primary_muscles)
            if (!group_of(m)) ungrouped.insert(m);
        for (const auto& m : e.secondary_muscles)
            if (!group_of(m)) ungrouped.insert(m);
    }
    CHECK(ungrouped.empty());
}

TEST_CASE("dominant group is the highest-scoring region") {
    CHECK(dominant_group(scores({{"chest", 3}, {"biceps", 1}})) == MuscleGroup::chest);
    CHECK_FALSE(dominant_group(Scores{}));
    CHECK_FALSE(dominant_group(scores({{"not_a_muscle", 9}})));
}

TEST_CASE("category decodes and drives is_explosive") {
    auto catalogue = fixtures::catalogue();
    const auto* snatch = catalogue.resolve("ривок");
    REQUIRE(snatch);
    CHECK(snatch->category == ExerciseCategory::power);
    CHECK(snatch->is_explosive());
    REQUIRE(catalogue.resolve("присід фронтальний"));
    CHECK_FALSE(catalogue.resolve("присід фронтальний")->is_explosive());
}

TEST_CASE("explosive movements are exactly the power and speed categories") {
    std::set<std::string> explosive, by_category;
    for (const auto& e : fixtures::catalogue().exercises) {
        if (e.is_explosive()) explosive.insert(e.name);
        if (e.category == ExerciseCategory::power || e.category == ExerciseCategory::speed)
            by_category.insert(e.name);
    }
    CHECK(explosive == by_category);
}
