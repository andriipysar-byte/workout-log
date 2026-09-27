#include <doctest/doctest.h>

#include "helpers.hpp"
#include "workoutlog/cycle_planning.hpp"
#include "workoutlog/date.hpp"

using namespace wl;
using helpers::MemoryApp;

TEST_CASE("create writes the file and selects it") {
    MemoryApp app;
    app.model.create(helpers::sample());
    CHECK(app.storage->snapshot().contains("2026-08-06_D2.json"));
    CHECK(app.model.selection() == "2026-08-06_D2.json");
    CHECK(app.model.status() == "Created 2026-08-06_D2.json");
    CHECK(app.model.files() == std::vector<std::string>{"2026-08-06_D2.json"});
}

TEST_CASE("delete removes the file and clears the selection") {
    MemoryApp app;
    app.model.create(helpers::sample());
    app.model.remove("2026-08-06_D2.json");
    CHECK(app.storage->snapshot().empty());
    CHECK_FALSE(app.model.selection());
    CHECK(app.model.session() == nullptr);
}

TEST_CASE("a header edit renames the file instead of orphaning it") {
    MemoryApp app;
    app.model.create(helpers::sample());
    app.model.session()->date = "2026-08-07";
    app.model.session_edited();
    app.model.save();
    CHECK(app.model.files() == std::vector<std::string>{"2026-08-07_D2.json"});
    CHECK(app.model.status() == "Renamed 2026-08-06_D2.json → 2026-08-07_D2.json");
}

TEST_CASE("a plain save says so and writes canonical bytes") {
    MemoryApp app;
    app.model.create(helpers::sample());
    app.model.save();
    CHECK(app.model.status() == "Saved 2026-08-06_D2.json");
    CHECK(app.model.read_raw("2026-08-06_D2.json") == encode_session(helpers::sample()));
}

TEST_CASE("an unreadable file costs one session, not the archive") {
    auto sessions = helpers::real_sessions();
    sessions["2026-01-01_A1.json"] = "{ broken";
    MemoryApp app(sessions);
    CHECK(app.model.files().size() == sessions.size());
    CHECK_FALSE(app.model.calendar().empty());
    app.model.open("2026-01-01_A1.json");
    CHECK(app.model.status().starts_with("Load failed"));
}

TEST_CASE("the real archive builds a cycle matrix, a calendar and maps") {
    auto sessions = helpers::real_sessions();
    MemoryApp app(sessions);
    CHECK(app.model.files().size() == sessions.size());
    const auto& matrix = app.model.cycle_matrix();
    CHECK_FALSE(matrix.empty());
    CHECK(matrix.cells.size() == matrix.exercises.size());
    CHECK(app.model.cycle_map_svg().has_value());
    for (const auto& [date, info] : app.model.calendar()) CHECK_MESSAGE(info.group.has_value(), date);

    app.model.open(app.model.files().front());
    REQUIRE(app.model.session());
    auto map = app.model.day_map_svg();
    REQUIRE(map);
    CHECK(map->find("style=\"fill:#") != std::string::npos);
}

TEST_CASE("re-saving a real session keeps its bytes") {
    auto sessions = helpers::real_sessions();
    MemoryApp app(sessions);
    for (const auto& [id, bytes] : sessions) {
        app.model.open(id);
        app.model.save();
        CHECK_MESSAGE(app.model.read_raw(id) == bytes, id);
    }
}

TEST_CASE("switching the weighting mode changes the day map") {
    MemoryApp app(helpers::real_sessions());
    app.model.open(app.model.files().back());
    auto by_sets = app.model.day_map_svg();
    app.model.set_mode(WeightingMode::tonnage);
    CHECK(app.model.day_map_svg() != by_sets);
}

TEST_CASE("import rejects a malformed file at the door") {
    MemoryApp app;
    app.model.import_sessions({{"2026-01-01_A1.json", encode_session(helpers::sample("2026-01-01", "A1"))},
                               {"2026-01-02_A1.json", "{ nope"}});
    CHECK(app.model.files() == std::vector<std::string>{"2026-01-01_A1.json"});
    CHECK(app.model.status() == "Imported 1 of 2 file(s)");
}

TEST_CASE("the plan loads the real cycles.json") {
    MemoryApp app;
    REQUIRE(app.model.cycles().size() == 2);
    CHECK(app.model.cycles()[0].sessions.size() == 8);
    CHECK(app.model.cycles()[1].sessions.size() == 8);
    CHECK(app.model.can_edit_plan());
    for (const auto& w : app.model.cycles()[0].sessions) CHECK_MESSAGE(app.model.plan_map_svg(w), w.cycle_day);
    CHECK(app.model.plan_dominant_group(app.model.cycles()[0].sessions.front()));
}

TEST_CASE("the cycle total map differs from any one day, and an empty cycle has none") {
    MemoryApp app;
    const Cycle& cycle = app.model.cycles()[0];
    auto total = app.model.plan_cycle_map_svg(cycle);
    REQUIRE(total);
    CHECK(total != app.model.plan_map_svg(cycle.sessions.front()));
    Cycle empty{.id = "e", .name = "e", .training_days = {"Mon"}, .start_date = "2026-01-01"};
    CHECK_FALSE(app.model.plan_cycle_map_svg(empty));
}

TEST_CASE("adding workouts continues the A1/A2 progression on real training dates") {
    MemoryApp app;
    CHECK(app.model.cycles()[0].sessions.back().cycle_day == "D2");
    size_t added = app.model.add_workout(0);
    const Cycle& cycle = app.model.cycles()[0];
    CHECK(cycle.sessions[added].cycle_day == "E1");
    CHECK(iso_date(*app.model.planned_date(cycle, added)) == "2026-08-09");
    CHECK(cycle.sessions[added].weekday == "Sun");
    CHECK(app.model.cycles()[0].sessions[app.model.add_workout(0)].cycle_day == "E2");
    CHECK(app.model.cycles()[0].sessions[app.model.add_workout(0)].cycle_day == "F1");
}

TEST_CASE("a new CrossFit day and a new hard day are prefilled differently") {
    MemoryApp app;
    auto roles = [&](size_t i) {
        std::vector<std::string> out;
        for (const auto& b : app.model.cycles()[0].sessions[i].blocks) out.push_back(b.type + ":" + b.role.value_or(""));
        return out;
    };
    auto conditioning = roles(app.model.add_workout(0));
    auto heavy = roles(app.model.add_workout(0));
    auto has = [](const auto& v, const char* s) { return std::find(v.begin(), v.end(), s) != v.end(); };
    CHECK(has(conditioning, "metcon:metcon"));
    CHECK(has(conditioning, "strength:explosive"));
    CHECK(has(heavy, "strength:main"));
    CHECK_FALSE(has(heavy, "metcon:metcon"));
}

TEST_CASE("removing or moving a workout re-derives weekdays so the cycle still generates") {
    MemoryApp app;
    app.model.remove_workout(0, 0);
    const Cycle& cycle = app.model.cycles()[0];
    CHECK(cycle.sessions.size() == 7);
    for (size_t i = 0; i < cycle.sessions.size(); ++i)
        CHECK(cycle.sessions[i].weekday == std::string(weekday_abbreviation(*app.model.planned_date(cycle, i))));
    CHECK_NOTHROW(generate_cycle(cycle));

    std::string second = cycle.sessions[1].cycle_day;
    app.model.move_workout(0, 1, 0);
    CHECK(app.model.cycles()[0].sessions[0].cycle_day == second);
    CHECK_NOTHROW(generate_cycle(app.model.cycles()[0]));
}

TEST_CASE("renaming a workout updates its code and its day type") {
    MemoryApp app;
    app.model.retitle_workout(0, 0, CycleDay{'C', DayKind::heavy});
    CHECK(app.model.cycles()[0].sessions[0].cycle_day == "C2");
    CHECK(app.model.cycles()[0].sessions[0].type == "heavy");
}

TEST_CASE("cloning copies the workouts without aliasing the original; ids clash case-insensitively") {
    MemoryApp app;
    size_t clone = app.model.clone_cycle(0, "copy", "Copy");
    REQUIRE(app.model.cycles().size() == 3);
    app.model.cycles()[clone].sessions[0].cycle_day = "Z1";
    CHECK(app.model.cycles()[0].sessions[0].cycle_day != "Z1");
    CHECK(app.model.cycle_id_taken("hybrid-8"));
    CHECK(app.model.cycle_id_taken("  HYBRID-8 "));
    CHECK_FALSE(app.model.cycle_id_taken("something-else"));
    CHECK_FALSE(app.model.cycles()[app.model.clone_cycle(1, "copy-2", "Copy")].version);
}

TEST_CASE("a new version copies the cycle under its id, and the earlier versions stay") {
    MemoryApp app;
    size_t v3 = app.model.new_version(1);
    const Cycle& cycle = app.model.cycles()[v3];
    CHECK(cycle.id == "hybrid-8");
    CHECK(cycle.version == 3);
    CHECK(cycle_label(cycle) == "8-session hybrid cycle · v3");
    CHECK(app.model.status() == "Started 8-session hybrid cycle · v3");
    CHECK(app.model.cycles()[0].version == 1);
    CHECK(app.model.cycles()[1].to_json()["sessions"] == cycle.to_json()["sessions"]);
}

TEST_CASE("saving cycles.json keeps every hand-written key, and stubs still generate the same") {
    MemoryApp app;
    auto before_stubs = generate_cycle(app.model.cycles()[0]);
    app.model.save_cycles();
    CHECK(app.model.status() == "Saved cycles.json");
    std::string original = helpers::read(helpers::repo() / "cycles.json");
    CHECK(app.references->files().at("cycles.json") == encode_file(parse_json(original)));
    CHECK(generate_cycle(decode_cycles(app.references->files().at("cycles.json")).cycles.at(0)) == before_stubs);
}

TEST_CASE("a new exercise is written into exercises.json and reaches the plan map") {
    MemoryApp app;
    size_t before = app.model.catalogue()->exercises.size();
    app.model.add_exercise(Exercise{.name = "тяга сумо",
                                    .aliases = {"сумо"},
                                    .pattern = MovementPattern::hinge,
                                    .primary_muscles = {"glutes", "hamstrings"},
                                    .secondary_muscles = {"spinal_erectors"}});
    CHECK(app.model.catalogue()->resolve("сумо")->name == "тяга сумо");
    auto written = decode_catalogue(app.references->files().at("exercises.json"));
    CHECK(written.resolve("тяга сумо"));
    CHECK(written.comment);
    CHECK(written.exercises.size() == before + 1);

    CycleSession workout{.cycle_day = "A1"};
    workout.blocks.push_back(BlockTemplate{.type = "strength", .exercise = "тяга сумо"});
    CHECK(app.model.plan_map_svg(workout));
    CHECK(app.model.plan_dominant_group(workout) == MuscleGroup::legs);
}
