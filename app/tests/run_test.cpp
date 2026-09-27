#include <doctest/doctest.h>

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "helpers.hpp"
#include "run_view.hpp"
#include "workoutlog/notation.hpp"

using namespace wl;
using helpers::MemoryApp;
using namespace std::chrono;

namespace {

constexpr Date kTuesday = Date{year{2026} / 9 / 29};
constexpr size_t kV2 = 1;
constexpr size_t kBackSquatDay = 2;

StrengthBlock& strength(Prescription& run, size_t day, std::string_view exercise) {
    for (auto& b : run.days.at(day).blocks)
        if (auto* s = std::get_if<StrengthBlock>(&b); s && s->exercise == exercise) return *s;
    FAIL("no " << std::string(exercise) << " on day " << day);
    throw;
}

} // namespace

TEST_CASE("starting a run writes its file beside cycles.json, and starting it again finds it") {
    MemoryApp app;
    auto run = app.model.start_run(kV2, kTuesday);
    REQUIRE(run);
    CHECK(app.references->files().contains("prescriptions/hybrid-8-v2_2026-09-29.json"));
    CHECK_FALSE(app.model.run_unsaved(*run));
    CHECK(app.model.run_label(app.model.prescriptions()[*run]) == "8-session hybrid cycle · v2 — from 2026-09-29");
    CHECK(app.model.start_run(kV2, kTuesday) == run);
    CHECK(app.model.prescriptions().size() == 1);
}

TEST_CASE("a run that does not fit the calendar is refused with the reason") {
    MemoryApp app;
    CHECK_FALSE(app.model.start_run(kV2, Date{year{2026} / 9 / 30}));
    CHECK(app.model.status().find("Could not start the run") == 0);
    CHECK(app.model.prescriptions().empty());
}

TEST_CASE("runs load at start, and one unreadable file does not hide the others") {
    auto refs = helpers::real_references();
    MemoryApp seed;
    seed.model.start_run(kV2, kTuesday);
    refs["prescriptions/hybrid-8-v2_2026-09-29.json"] =
        seed.references->files().at("prescriptions/hybrid-8-v2_2026-09-29.json");
    refs["prescriptions/broken.json"] = "{";
    MemoryApp app({}, refs, true);
    REQUIRE(app.model.prescriptions().size() == 1);
    CHECK(app.model.prescriptions()[0].days.size() == 8);
    CHECK(app.model.status().find("prescriptions/broken.json") != std::string::npos);
}

TEST_CASE("generating a day saves the run first and writes the prescribed weights into the session") {
    MemoryApp app;
    size_t run = *app.model.start_run(kV2, kTuesday);
    strength(app.model.prescriptions()[run], kBackSquatDay, "присід на плечах").sets =
        parse_strength_sets("3 × [60, 80, 100, 110]").sets;
    app.model.run_edited(run);
    CHECK(app.model.run_unsaved(run));

    app.model.generate_days(run, {kBackSquatDay});
    CHECK_FALSE(app.model.run_unsaved(run));
    auto saved = decode_prescription(app.references->files().at("prescriptions/hybrid-8-v2_2026-09-29.json"));
    CHECK(strength(saved, kBackSquatDay, "присід на плечах").sets.size() == 4);

    REQUIRE(app.model.day_file(run, kBackSquatDay) == "2026-10-04_A2.json");
    Session session = decode_session(app.storage->snapshot().at("2026-10-04_A2.json"));
    const auto* squat = std::get_if<StrengthBlock>(&session.blocks.at(2));
    REQUIRE(squat);
    CHECK(squat->exercise == "присід на плечах");
    CHECK(squat->sets.back().weight_kg == 110);
    CHECK(squat->sets.back().reps == 3);
    CHECK(app.model.status() == "Generated 2026-10-04_A2.json");
}

TEST_CASE("regenerating the day that is open reloads it, so the editor never shows a stale copy") {
    MemoryApp app;
    size_t run = *app.model.start_run(kV2, kTuesday);
    app.model.generate_days(run, {kBackSquatDay});
    app.model.open("2026-10-04_A2.json");
    unsigned before = app.model.session_generation();

    strength(app.model.prescriptions()[run], kBackSquatDay, "присід на плечах").sets =
        parse_strength_sets("3 × [120]").sets;
    app.model.run_edited(run);
    app.model.generate_days(run, {kBackSquatDay});
    CHECK(app.model.session_generation() > before);
    const auto* squat = std::get_if<StrengthBlock>(&app.model.session()->blocks.at(2));
    REQUIRE(squat);
    CHECK(squat->sets.at(0).weight_kg == 120);
}

TEST_CASE("the run view totals typed sets live and asks before overwriting a generated day") {
    MemoryApp app;
    size_t run = *app.model.start_run(kV2, kTuesday);
    std::vector<std::vector<std::string>> asked;
    bool answer = false;
    RunView view(app.model, nullptr, [&](const std::vector<std::string>& ids) {
        asked.push_back(ids);
        return answer;
    });
    CHECK(view.run_box()->count() == 1);
    CHECK_FALSE(view.save_button()->isEnabled());

    QLineEdit* squat_line = nullptr;
    for (auto* edit : view.findChildren<QLineEdit*>())
        if (edit->placeholderText().startsWith("3 × [")) {
            auto* holder = edit->parentWidget();
            for (auto* label : holder->findChildren<QLabel*>())
                if (label->text().contains("присід на плечах")) squat_line = edit;
        }
    REQUIRE(squat_line);
    squat_line->setText("3 × [100, 110]");
    emit squat_line->returnPressed();
    CHECK(view.save_button()->isEnabled());
    CHECK(strength(app.model.prescriptions()[run], kBackSquatDay, "присід на плечах").sets.size() == 2);
    auto labels = view.findChildren<QLabel*>();
    CHECK(std::any_of(labels.begin(), labels.end(), [](QLabel* l) { return l->text().contains("630 kg"); }));

    view.generate({kBackSquatDay});
    CHECK(asked.empty());
    std::string first = app.storage->snapshot().at("2026-10-04_A2.json");

    strength(app.model.prescriptions()[run], kBackSquatDay, "присід на плечах").sets =
        parse_strength_sets("3 × [140]").sets;
    app.model.run_edited(run);
    view.generate({kBackSquatDay});
    REQUIRE(asked.size() == 1);
    CHECK(asked[0] == std::vector<std::string>{"2026-10-04_A2.json"});
    CHECK(app.storage->snapshot().at("2026-10-04_A2.json") == first);

    answer = true;
    view.generate({kBackSquatDay});
    CHECK(app.storage->snapshot().at("2026-10-04_A2.json") != first);
}
