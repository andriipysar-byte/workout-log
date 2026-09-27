#include <doctest/doctest.h>

#include <algorithm>

#include "fixtures.hpp"
#include "workoutlog/cycle_planning.hpp"
#include "workoutlog/muscles.hpp"

using namespace wl;
using namespace std::chrono;

namespace {

bool has(const Scores& scores, std::string_view muscle) {
    return std::any_of(scores.begin(), scores.end(), [&](const auto& e) { return e.first == muscle; });
}

CycleDay parse(std::string_view code) {
    auto day = CycleDay::try_parse(code);
    REQUIRE(day);
    return *day;
}

template <typename T>
bool contains(const std::vector<T>& list, const T& item) {
    return std::find(list.begin(), list.end(), item) != list.end();
}

} // namespace

TEST_CASE("CycleDay parses the letter-and-number convention") {
    auto day = parse("B2");
    CHECK(day.letter == 'B');
    CHECK(day.kind == DayKind::heavy);
    CHECK(day.code() == "B2");
}

TEST_CASE("CycleDay: 1 is conditioning and 2 is hard work") {
    CHECK(parse("A1").kind == DayKind::conditioning);
    CHECK(parse("A2").kind == DayKind::heavy);
    CHECK(day_kind_label(DayKind::conditioning) == "CrossFit");
    CHECK(day_kind_label(DayKind::heavy) == "Hard work");
}

TEST_CASE("CycleDay: a lowercase code is accepted and normalised") {
    REQUIRE(CycleDay::try_parse("c1"));
    CHECK(CycleDay::try_parse("c1")->code() == "C1");
    REQUIRE(CycleDay::try_parse(" a2 "));
    CHECK(CycleDay::try_parse(" a2 ")->code() == "A2");
}

TEST_CASE("CycleDay: a code outside the convention returns null rather than throwing") {
    for (std::string code : {"A3", "AA1", "1A", "A", "", "деньА1"}) {
        CAPTURE(code);
        CHECK_FALSE(CycleDay::try_parse(code));
    }
}

TEST_CASE("CycleDay: next walks A1 → A2 → B1 → B2") {
    auto day = parse("A1");
    std::vector<std::string> walk{day.code()};
    for (int i = 0; i < 3; ++i) {
        day = day.next();
        walk.push_back(day.code());
    }
    CHECK(walk == std::vector<std::string>{"A1", "A2", "B1", "B2"});
}

TEST_CASE("next day: an empty cycle starts at A1") {
    CHECK(cycle_templates::next_day({}).code() == "A1");
}

TEST_CASE("next day: hybrid-8 v1 ends at D2, so the next workout is E1") {
    auto cycles = fixtures::cycles();
    const Cycle* cycle = cycles.by_id("hybrid-8", 1);
    REQUIRE(cycle);
    std::vector<std::string> codes;
    for (const auto& s : cycle->sessions) codes.push_back(s.cycle_day);
    CHECK(cycle_templates::next_day(codes).code() == "E1");
}

TEST_CASE("next day: codes outside the convention are ignored when picking the next") {
    CHECK(cycle_templates::next_day({"деload", "A1"}).code() == "A2");
    CHECK(cycle_templates::next_day({"деload"}).code() == "A1");
}

TEST_CASE("prefilled blocks: a CrossFit day gets an explosive lift and a metcon") {
    auto blocks = cycle_templates::blocks_for(DayKind::conditioning);
    REQUIRE_FALSE(blocks.empty());
    std::vector<std::optional<std::string>> roles;
    std::vector<std::string> types;
    for (const auto& b : blocks) {
        roles.push_back(b.role);
        types.push_back(b.type);
    }
    CHECK(contains(roles, std::optional<std::string>("explosive")));
    CHECK(contains(types, std::string("metcon")));
    CHECK(blocks.front().type == "cardio");
    CHECK(blocks.back().type == "cooldown");
}

TEST_CASE("prefilled blocks: a hard-work day gets a main lift and no metcon") {
    auto blocks = cycle_templates::blocks_for(DayKind::heavy);
    REQUIRE_FALSE(blocks.empty());
    std::vector<std::optional<std::string>> roles;
    std::vector<std::string> types;
    for (const auto& b : blocks) {
        roles.push_back(b.role);
        types.push_back(b.type);
    }
    CHECK(contains(roles, std::optional<std::string>("main")));
    CHECK_FALSE(contains(types, std::string("metcon")));
    CHECK_MESSAGE(blocks.front().duration_min == 15, "longer warm-up");
}

TEST_CASE("prefilled blocks: a new session carries its code and kind") {
    auto session = cycle_templates::session(parse("C1"));
    CHECK(session.cycle_day == "C1");
    CHECK(session.type == "metcon");
    CHECK_FALSE(session.blocks.empty());
}

TEST_CASE("plan preview: a planned exercise counts even with no sets planned") {
    CycleSession session{.cycle_day = "A2"};
    session.blocks.push_back(BlockTemplate{.type = "strength", .role = "main", .exercise = "жим лежачи"});
    auto scores =
        muscle_activation::for_session(preview_session(session), fixtures::catalogue(), WeightingMode::set_count);
    CHECK(scores.get("chest") == 1.0);
}

TEST_CASE("plan preview: skips blocks with no exercise chosen yet") {
    auto preview = preview_session(cycle_templates::session(parse("A2")));
    // Only the pre-named hyperextension survives; the blank slots do not.
    std::vector<std::string> named;
    size_t cardio = 0;
    for (const auto& b : preview.blocks) {
        if (const auto* s = std::get_if<StrengthBlock>(&b)) named.push_back(s->exercise);
        if (std::holds_alternative<CardioBlock>(b)) ++cardio;
    }
    CHECK(named == std::vector<std::string>{"гіперекстензія"});
    CHECK(cardio == 1);
}

TEST_CASE("plan preview: generating an incomplete plan fails loudly instead") {
    auto session = cycle_templates::session(parse("A2"));
    CHECK_THROWS_AS(session_from_template(session, Date{year{2026} / 7 / 21}), CycleGeneratorError);
}

TEST_CASE("plan preview: a real planned day produces a muscle map") {
    auto cycles = fixtures::cycles();
    const Cycle* cycle = cycles.by_id("hybrid-8");
    REQUIRE(cycle);
    auto c2 = std::find_if(cycle->sessions.begin(), cycle->sessions.end(),
                           [](const auto& s) { return s.cycle_day == "C2"; });
    REQUIRE(c2 != cycle->sessions.end());
    auto scores = muscle_activation::for_session(preview_session(*c2), fixtures::catalogue(), WeightingMode::set_count);
    CHECK_FALSE(scores.empty());
    CHECK_MESSAGE(has(scores, "quads"), "the planned main lift must reach the map");
    CHECK(has(scores, "calves"));
    CHECK(dominant_group(scores) == MuscleGroup::legs);
}

TEST_CASE("plan preview: an empty plan produces an empty map rather than failing") {
    auto scores = muscle_activation::for_session(preview_session(CycleSession{.cycle_day = "A1"}),
                                                 fixtures::catalogue(), WeightingMode::set_count);
    CHECK(scores.empty());
}
