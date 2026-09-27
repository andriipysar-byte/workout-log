#include <doctest/doctest.h>

#include "fixtures.hpp"
#include "workoutlog/muscles.hpp"
#include "workoutlog/session_store.hpp"

using namespace wl;

TEST_CASE("data/ holds at least one session") {
    CHECK_FALSE(fixtures::session_files().empty());
}

TEST_CASE("every session file round-trips byte for byte") {
    for (const auto& file : fixtures::session_files()) {
        CAPTURE(file.filename().string());
        std::string text = fixtures::read(file);
        Session first = decode_session(text);
        std::string encoded = encode_session(first);
        CHECK(decode_session(encoded) == first);
        CHECK(encoded == text);
    }
}

TEST_CASE("every session file decodes through the store") {
    std::map<std::string, std::string> seed;
    for (const auto& f : fixtures::session_files()) seed[f.filename().string()] = fixtures::read(f);
    MemoryStorage storage(seed);
    auto result = SessionStore(storage).load_all();
    CHECK(result.failures.empty());
    CHECK(result.sessions.size() == seed.size());
}

TEST_CASE("integral doubles encode without a .0 suffix") {
    Session session{.date = "2026-01-01", .cycle_day = "A1", .bodyweight_kg = 82.0};
    StrengthBlock block{.exercise = "присід фронтальний"};
    block.sets.push_back(WorkSet{.weight_kg = 70.0, .reps = 6});
    block.sets.push_back(WorkSet{.weight_kg = 47.5});
    session.blocks.push_back(block);
    std::string encoded = encode_session(session);
    CHECK(encoded.find("\"weight_kg\": 70\n") != std::string::npos);
    CHECK(encoded.find("\"weight_kg\": 47.5") != std::string::npos);
    CHECK(encoded.find("\"bodyweight_kg\": 82,") != std::string::npos);
    CHECK(encoded.find("70.0") == std::string::npos);
    CHECK(encoded.find("82.0") == std::string::npos);
}

TEST_CASE("keys are sorted at every depth and the file ends with a newline") {
    Session session{.date = "2026-01-01", .cycle_day = "A1", .start_time = "7:45", .notes = "n"};
    session.blocks.push_back(CardioBlock{.machine = "гребля", .duration_min = 10.0, .end_time = "8:00"});
    std::string encoded = encode_session(session);
    CHECK(encoded.ends_with("}\n"));
    CHECK(encoded ==
          "{\n  \"blocks\": [\n    {\n      \"duration_min\": 10,\n      \"end_time\": \"8:00\",\n"
          "      \"machine\": \"гребля\",\n      \"type\": \"cardio\"\n    }\n  ],\n  \"cycle_day\": \"A1\",\n"
          "  \"date\": \"2026-01-01\",\n  \"kind\": \"training\",\n  \"notes\": \"n\",\n"
          "  \"start_time\": \"7:45\"\n}\n");
}

TEST_CASE("optional fields are omitted, never written as null") {
    Session session{.date = "2026-01-01", .cycle_day = "A1"};
    session.blocks.push_back(CooldownBlock{});
    std::string encoded = encode_session(session);
    CHECK(encoded.find("null") == std::string::npos);
    CHECK(encoded.find("start_time") == std::string::npos);
    CHECK(encoded.find("bodyweight_kg") == std::string::npos);
}

TEST_CASE("an unknown block type is rejected rather than silently dropped") {
    CHECK_THROWS_AS(decode_session(R"({"date":"2026-01-01","cycle_day":"A1","blocks":[{"type":"yoga"}]})"),
                    FormatError);
}

TEST_CASE("kind defaults to training when absent") {
    CHECK(decode_session(R"({"date":"2026-01-01","cycle_day":"A1","blocks":[]})").kind == Kind::training);
}

TEST_CASE("the encoder writes numbers and strings the way Dart does") {
    CHECK(encode_pretty(Json::parse("[0.1, 1e-7, 1e21, 123456.789, -0.5, 2.0]")) ==
          "[\n  0.1,\n  1e-7,\n  1e+21,\n  123456.789,\n  -0.5,\n  2\n]");
    CHECK(encode_pretty(Json("a\"b\\c\n\x01/й")) == "\"a\\\"b\\\\c\\n\\u0001/й\"");
    CHECK(encode_pretty(Json::object()) == "{}");
    CHECK(encode_pretty(Json::array()) == "[]");
    CHECK(dart_double(300) == "300.0");
    CHECK(dart_double(0.30000000000000004) == "0.30000000000000004");
    CHECK(dart_double(1.5e-7) == "1.5e-7");
    CHECK(dart_double(0.000001) == "0.000001");
}

TEST_CASE("metcon movement prescription") {
    SUBCASE("load survives a round trip and keeps weight_kg beside it") {
        MetconBlock block{.format = MetconFormat::for_time, .scheme = IntList{21, 15, 9}};
        block.exercises = {{.name = "гирі", .load = "24kg+24kg"},
                           {.name = "стрибки на тумбу", .load = "60 cm"},
                           {.name = "трастери", .weight_kg = 42.5},
                           {.name = "бьорпі"}};
        Block original = block;
        CHECK(block_from_json(parse_json(encode_file(block_to_json(original)))) == original);
    }
    SUBCASE("prescriptions") {
        CHECK_FALSE(MetconExercise{.name = "бьорпі"}.prescription());
        CHECK(MetconExercise{.name = "гирі", .load = "24kg+24kg", .weight_kg = 24.0}.prescription() == "24kg+24kg");
        CHECK(MetconExercise{.name = "трастери", .weight_kg = 42.0}.prescription() == "42 kg");
        CHECK(MetconExercise{.name = "трастери", .weight_kg = 42.5}.prescription() == "42.5 kg");
    }
    SUBCASE("a movement follows the workout scheme unless it overrides it") {
        IntList scheme{21, 15, 9};
        CHECK(MetconExercise{.name = "бьорпі"}.scheme_within(scheme) == scheme);
        CHECK(MetconExercise{.name = "скакалка", .reps_override = IntList{65, 45, 30}}.scheme_within(scheme) ==
              IntList{65, 45, 30});
        CHECK(MetconExercise{.name = "бьорпі"}.scheme_within(std::nullopt).empty());
    }
}

TEST_CASE("cycles.json round-trips without losing a single key") {
    std::string text = fixtures::read(fixtures::repo() / "cycles.json");
    CHECK(encode_file(CycleCatalogue::from_json(parse_json(text)).to_json()) == encode_file(parse_json(text)));
}

TEST_CASE("cycles.json keeps role, cycle-level extras and its comment") {
    auto catalogue = fixtures::cycles();
    const Cycle* cycle = catalogue.by_id("hybrid-8");
    REQUIRE(cycle);
    bool explosive = false, grip = false;
    for (const auto& s : cycle->sessions)
        for (const auto& b : s.blocks) {
            REQUIRE(b.role);
            explosive |= *b.role == "explosive";
            grip |= *b.role == "grip";
        }
    CHECK(explosive);
    CHECK(grip);
    CHECK(cycle->extras.contains("skipped"));
    CHECK(cycle->to_json().contains("skipped"));
    REQUIRE(catalogue.comment);
    CHECK(catalogue.comment->find("Reusable cycle definitions") != std::string::npos);
    CHECK(catalogue.to_json()["$comment"] == *catalogue.comment);
}

TEST_CASE("exercises.json round-trips without losing a single key") {
    std::string text = fixtures::read(fixtures::repo() / "exercises.json");
    CHECK(encode_file(Catalogue::from_json(parse_json(text)).to_json()) == encode_file(parse_json(text)));
    auto catalogue = fixtures::catalogue();
    REQUIRE(catalogue.comment);
    CHECK(catalogue.comment->find("Exercise catalogue") != std::string::npos);
}

TEST_CASE("adding an exercise appends; an existing name replaces") {
    auto before = fixtures::catalogue();
    Exercise added{.name = "тест вправа", .primary_muscles = {"chest"}};
    auto after = before.with_exercise(added);
    REQUIRE(after.exercises.size() == before.exercises.size() + 1);
    REQUIRE(after.resolve("тест вправа"));
    CHECK(after.resolve("тест вправа")->primary_muscles == std::vector<std::string>{"chest"});
    CHECK(after.comment == before.comment);
    for (size_t i = 0; i < before.exercises.size(); ++i)
        CHECK(after.exercises[i].to_json() == before.exercises[i].to_json());

    auto replaced = before.with_exercise(Exercise{.name = "Жим лежачи", .primary_muscles = {"chest"}});
    CHECK(replaced.exercises.size() == before.exercises.size());
    CHECK(replaced.resolve("жим лежачи")->primary_muscles == std::vector<std::string>{"chest"});
}

TEST_CASE("known_muscles is the sorted vocabulary, and every token has a group") {
    auto muscles = fixtures::catalogue().known_muscles();
    CHECK(std::find(muscles.begin(), muscles.end(), "quads") != muscles.end());
    CHECK(std::find(muscles.begin(), muscles.end(), "spinal_erectors") != muscles.end());
    CHECK(std::is_sorted(muscles.begin(), muscles.end()));
    for (const auto& m : muscles) CHECK_MESSAGE(group_of(m).has_value(), m);
}
