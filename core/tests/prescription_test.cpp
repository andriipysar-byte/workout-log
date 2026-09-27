#include <doctest/doctest.h>

#include "fixtures.hpp"
#include "workoutlog/cycle_planning.hpp"
#include "workoutlog/notation.hpp"
#include "workoutlog/prescription.hpp"

using namespace wl;
using namespace std::chrono;

namespace {

Prescription v2_run() {
    auto cycles = fixtures::cycles();
    const Cycle* cycle = cycles.by_id("hybrid-8", 2);
    REQUIRE(cycle);
    return prescribe(*cycle, Date{year{2026} / 9 / 29});
}

StrengthBlock* strength(PrescribedDay& day, std::string_view exercise) {
    for (auto& b : day.blocks)
        if (auto* s = std::get_if<StrengthBlock>(&b); s && s->exercise == exercise) return s;
    return nullptr;
}

} // namespace

TEST_CASE("a run lays the cycle onto dates and names its file after cycle, version and start") {
    auto run = v2_run();
    CHECK(run.cycle_id == "hybrid-8");
    CHECK(run.cycle_version == 2);
    CHECK(run.file_name() == "prescriptions/hybrid-8-v2_2026-09-29.json");
    REQUIRE(run.days.size() == 8);
    CHECK(run.days[2].cycle_day == "A2");
    CHECK(run.days[2].date == "2026-10-04");
    CHECK(run.days[2].title == "Важкий · присід на плечах (трійки)");
}

TEST_CASE("a run keeps the template's planned reps and starts without weights") {
    auto run = v2_run();
    auto* curls = strength(run.days[2], "згинання ніг стоячи");
    REQUIRE(curls);
    CHECK(curls->sets.size() == 5);
    CHECK(curls->sets[0].reps == 6);
    CHECK_FALSE(curls->sets[0].weight_kg);
}

TEST_CASE("a run starting on a day the template does not train on is refused") {
    auto cycles = fixtures::cycles();
    CHECK_THROWS_AS(prescribe(*cycles.by_id("hybrid-8", 2), Date{year{2026} / 9 / 30}), CycleGeneratorError);
}

TEST_CASE("prescribed numbers survive the file and reach the generated session") {
    auto run = v2_run();
    auto* squat = strength(run.days[2], "присід на плечах");
    REQUIRE(squat);
    squat->sets = parse_strength_sets("3 × [60, 80, 100, 110]").sets;

    auto read = decode_prescription(encode_prescription(run));
    CHECK(encode_prescription(read) == encode_prescription(run));

    Session session = session_for(read.days[2]);
    CHECK(session.date == "2026-10-04");
    CHECK(session.cycle_day == "A2");
    auto* generated = strength(read.days[2], "присід на плечах");
    REQUIRE(generated);
    CHECK(generated->sets.size() == 4);
    CHECK(generated->sets.back().weight_kg == 110);
    CHECK(decode_session(encode_session(session)) == session);
    CHECK(encode_session(session).find("title") == std::string::npos);
}

TEST_CASE("keys a run does not model are kept") {
    auto json = v2_run().to_json();
    json["$comment"] = "run 1 — calibration";
    json["days"][0]["mood"] = "fresh";
    auto back = Prescription::from_json(json).to_json();
    CHECK(back["$comment"] == "run 1 — calibration");
    CHECK(back["days"][0]["mood"] == "fresh");
}

TEST_CASE("planned load sums strength sets and weighted metcon rounds") {
    std::vector<Block> blocks;
    blocks.emplace_back(StrengthBlock{.exercise = "жим лежачи", .sets = parse_strength_sets("6 × [60, 80]").sets});
    MetconBlock metcon;
    metcon.scheme = IntList{21, 15, 9};
    metcon.exercises = {MetconExercise{.name = "трастери", .weight_kg = 40}, MetconExercise{.name = "бьорпі"}};
    blocks.emplace_back(metcon);
    auto load = planned_load(blocks);
    CHECK(load.sets == 2);
    CHECK(load.reps == 12);
    CHECK(load.metcon_rounds == 3);
    CHECK(load.tonnage_kg == doctest::Approx(6 * 60 + 6 * 80 + 45 * 40));
}

TEST_CASE("schemes parse the ways they are typed and print back") {
    CHECK(parse_scheme("21-15-9") == IntList{21, 15, 9});
    CHECK(parse_scheme("21, 15, 9") == IntList{21, 15, 9});
    CHECK(parse_scheme("8×10") == IntList(8, 10));
    CHECK(parse_scheme("8 x 10") == IntList(8, 10));
    CHECK(parse_scheme("8х10") == IntList(8, 10));
    CHECK(parse_scheme("5") == IntList{5});
    CHECK_FALSE(parse_scheme(""));
    CHECK_FALSE(parse_scheme("abc"));
    CHECK_FALSE(parse_scheme("0x10"));
    CHECK(format_scheme({21, 15, 9}) == "21-15-9");
    CHECK(format_scheme(IntList(8, 10)) == "8×10");
    CHECK(format_scheme({5}) == "5");
}
