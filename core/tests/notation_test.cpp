#include <doctest/doctest.h>

#include "workoutlog/notation.hpp"

using namespace wl;

namespace {

std::vector<std::optional<double>> weights(const ParsedSets& parsed) {
    std::vector<std::optional<double>> out;
    for (const auto& s : parsed.sets) out.push_back(s.weight_kg);
    return out;
}

std::vector<std::optional<std::int64_t>> reps(const ParsedSets& parsed) {
    std::vector<std::optional<std::int64_t>> out;
    for (const auto& s : parsed.sets) out.push_back(s.reps);
    return out;
}

std::vector<std::optional<double>> durations(const ParsedSets& parsed) {
    std::vector<std::optional<double>> out;
    for (const auto& s : parsed.sets) out.push_back(s.duration_sec);
    return out;
}

using Weights = std::vector<std::optional<double>>;
using Reps = std::vector<std::optional<std::int64_t>>;

} // namespace

TEST_CASE("ramp expands to one set per weight at the fixed rep count") {
    auto parsed = parse_strength_sets("6 × [70, 80, 90, 100, 110]");
    CHECK(parsed.sets.size() == 5);
    for (const auto& s : parsed.sets) CHECK(s.reps == 6);
    CHECK(weights(parsed) == Weights{70, 80, 90, 100, 110});
    CHECK(parsed.warnings.empty());
}

TEST_CASE("a trailing group is marked as back-off sets") {
    auto parsed = parse_strength_sets("6 × [70, 80, 90, 100, 110] + 6 × [30]");
    REQUIRE(parsed.sets.size() == 6);
    CHECK(parsed.sets.back().weight_kg == 30);
    CHECK(parsed.sets.back().is_backoff == true);
    for (size_t i = 0; i < 5; ++i) CHECK_FALSE(parsed.sets[i].is_backoff.has_value());
}

TEST_CASE("per-set rep overrides beat the group count") {
    CHECK(reps(parse_strength_sets("6 × [30, 60, 80(4), 86(2), 90(1)]")) == Reps{6, 6, 4, 2, 1});
}

TEST_CASE("cluster with an explicit total") {
    auto parsed = parse_strength_sets("5+5+4+3+3 (20)");
    REQUIRE(parsed.sets.size() == 1);
    CHECK(parsed.sets[0].cluster == IntList{5, 5, 4, 3, 3});
    CHECK(parsed.sets[0].total_reps == 20);
}

TEST_CASE("cluster total is derived from the sum when unstated") {
    auto parsed = parse_strength_sets("4+4+3+2+2+2+2+1");
    REQUIRE(parsed.sets.size() == 1);
    CHECK(parsed.sets[0].total_reps == 20);
}

TEST_CASE("timed holds become duration sets with no reps") {
    auto parsed = parse_strength_sets("4 × [54c, 40c, 36c, 42c]");
    CHECK(parsed.sets.size() == 4);
    CHECK(durations(parsed) == Weights{54, 40, 36, 42});
    for (const auto& s : parsed.sets) CHECK_FALSE(s.reps.has_value());
}

TEST_CASE("a bare brace list with the Cyrillic seconds suffix parses") {
    auto parsed = parse_strength_sets("{60с, 70с, 30с, 35с}");
    CHECK(parsed.sets.size() == 4);
    CHECK(durations(parsed) == Weights{60, 70, 30, 35});
}

TEST_CASE("all six multiplier characters are accepted") {
    for (std::string multiplier : {"×", "x", "X", "х", "Х", "*"}) {
        CAPTURE(multiplier);
        auto parsed = parse_strength_sets("6 " + multiplier + " [70, 80]");
        CHECK(weights(parsed) == Weights{70, 80});
        for (const auto& s : parsed.sets) CHECK(s.reps == 6);
    }
}

TEST_CASE("all four seconds suffixes are accepted") {
    for (std::string suffix : {"c", "C", "с", "С"}) {
        CAPTURE(suffix);
        auto parsed = parse_strength_sets("3 × [40" + suffix + "]");
        REQUIRE(parsed.sets.size() == 1);
        CHECK(parsed.sets[0].duration_sec == 40);
    }
}

TEST_CASE("decimal weights use a dot; a comma always separates values") {
    auto dotted = parse_strength_sets("1 × [47.5]");
    REQUIRE(dotted.sets.size() == 1);
    CHECK(dotted.sets[0].weight_kg == 47.5);
    CHECK(weights(parse_strength_sets("1 × [47,5]")) == Weights{47, 5});
}

TEST_CASE("empty input yields nothing and warns about nothing") {
    auto parsed = parse_strength_sets("   ");
    CHECK(parsed.sets.empty());
    CHECK(parsed.warnings.empty());
}

TEST_CASE("warn-never-block: a bad value still yields its siblings") {
    auto parsed = parse_strength_sets("6 × [70, банан, 90]");
    CHECK(weights(parsed) == Weights{70, 90});
    REQUIRE(parsed.warnings.size() == 1);
    CHECK(parsed.warnings[0].find("банан") != std::string::npos);
}

TEST_CASE("an unparseable line warns instead of throwing") {
    auto parsed = parse_strength_sets("нічого корисного");
    CHECK(parsed.sets.empty());
    CHECK_FALSE(parsed.warnings.empty());
}

TEST_CASE("parsed sets survive a JSON round-trip inside a session") {
    auto parsed = parse_strength_sets("6 × [70, 80]");
    Session session{.date = "2026-07-21", .cycle_day = "A1"};
    session.blocks.push_back(StrengthBlock{.exercise = "присід фронтальний", .sets = parsed.sets});
    auto decoded = decode_session(encode_session(session));
    REQUIRE(decoded.blocks.size() == 1);
    const auto& block = std::get<StrengthBlock>(decoded.blocks[0]);
    REQUIRE(block.sets.size() == 2);
    CHECK(block.sets[0].weight_kg == 70);
    CHECK(block.sets[0].reps == 6);
}
