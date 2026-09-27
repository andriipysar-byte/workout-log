#include <doctest/doctest.h>

#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSvgRenderer>
#include <QTest>

#include "block_cards.hpp"
#include "cycle_view.hpp"
#include "helpers.hpp"
#include "main_window.hpp"
#include "new_session_dialog.hpp"
#include "plan_view.hpp"
#include "session_editor.hpp"
#include "theme.hpp"
#include "workoutlog/notation.hpp"
#include "workoutlog/muscles.hpp"

using namespace wl;
using helpers::MemoryApp;

namespace {

QImage render(const std::string& svg) {
    QSvgRenderer renderer(QByteArray::fromStdString(svg));
    QImage image(500, 640, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    renderer.render(&painter);
    return image;
}

size_t count(const QImage& image, QColor color) {
    size_t n = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixelColor(x, y) == color) ++n;
    return n;
}

} // namespace

TEST_CASE("QtSvg honours the inline fill the colourizer adds over the template's own") {
    QFile file(":/muscle-map.svg");
    REQUIRE(file.open(QIODevice::ReadOnly));
    std::string tmpl = file.readAll().toStdString();
    Scores scores;
    for (const char* m : {"quads", "chest", "lats"}) scores[m] = 1.0;
    QImage hot = render(muscle_map_svg::colorize(tmpl, scores));
    QImage cold = render(muscle_map_svg::colorize(tmpl, Scores{}));
    QColor high(QString::fromLatin1(muscle_map_svg::kHigh.data(), 7));
    CHECK(count(hot, high) > 1000);
    CHECK(count(cold, high) == 0);
    CHECK(count(cold, QColor(QString::fromLatin1(muscle_map_svg::kZero.data(), 7))) > 1000);
}

TEST_CASE("the notation preview goes green, red, and Apply writes the sets") {
    MemoryApp app;
    app.model.create(helpers::sample());
    auto& block = std::get<StrengthBlock>(app.model.session()->blocks[0]);
    int changes = 0;
    StrengthEditor editor(block, app.model, [&] { ++changes; });

    QTest::keyClicks(editor.notation(), "6 × [70, 80]");
    CHECK(editor.preview()->text() == "→ 6×70  6×80");
    CHECK(editor.preview()->styleSheet().contains("#2e7d32"));
    CHECK(editor.apply_button()->isEnabled());

    editor.notation()->setText("??");
    CHECK(editor.preview()->text() == "→ —");
    CHECK(editor.preview()->styleSheet().contains("#d32f2f"));
    CHECK_FALSE(editor.apply_button()->isEnabled());

    editor.notation()->setText("5 × [100] + 5 × [80]");
    QTest::keyClick(editor.notation(), Qt::Key_Return);
    CHECK(block.sets.size() == 2);
    CHECK(block.sets[1].backoff());
    CHECK(editor.notation()->text().isEmpty());
    CHECK(editor.summary()->text().contains("5×80*"));
    CHECK(changes == 1);
}

TEST_CASE("set summaries: cluster, hold, bodyweight and back-off") {
    CHECK(set_summary(WorkSet{.cluster = IntList{5, 5, 4}, .total_reps = 14}) == "5+5+4 (14)");
    CHECK(set_summary(WorkSet{.duration_sec = 54.0}) == "54c");
    CHECK(set_summary(WorkSet{.reps = 10}) == "10×bw");
    CHECK(set_summary(WorkSet{.weight_kg = 80.0, .reps = 5, .is_backoff = true}) == "5×80*");
}

TEST_CASE("the shell lists sessions, opens one into an editor, and keeps it while typing") {
    MemoryApp app(helpers::real_sessions());
    MainWindow window(app.model);
    CHECK(window.session_list()->count() == static_cast<int>(app.model.files().size()));
    CHECK_FALSE(window.save_action()->isEnabled());

    app.model.open(app.model.files().front());
    REQUIRE(window.editor());
    SessionEditor* editor = window.editor();
    CHECK(window.save_action()->isEnabled());
    app.model.session_edited();
    CHECK(window.editor() == editor);

    app.model.open(app.model.files().back());
    CHECK(window.editor() != editor);
}

TEST_CASE("the new-session dialog: blank, from a template, bad date, overwrite prompt") {
    MemoryApp app;
    bool asked = false;
    NewSessionDialog dialog(app.model, nullptr, [&](const QString&) {
        asked = true;
        return false;
    });
    dialog.date_field()->setText("2026-13");
    dialog.day_field()->setText("A1");
    dialog.create();
    CHECK(dialog.error_label()->text() == "Date must be YYYY-MM-DD.");
    CHECK_FALSE(dialog.created());

    dialog.template_box()->setCurrentIndex(1);
    emit dialog.template_box()->activated(1);
    CHECK(dialog.day_field()->text() == "A1");
    CHECK(dialog.date_field()->text() == "2026-07-21");
    dialog.create();
    REQUIRE(dialog.created());
    CHECK(dialog.created()->blocks.size() > 1);

    app.model.create(*dialog.created());
    NewSessionDialog again(app.model, nullptr, [&](const QString&) {
        asked = true;
        return false;
    });
    again.date_field()->setText("2026-07-21");
    again.day_field()->setText("A1");
    again.create();
    CHECK(asked);
    CHECK_FALSE(again.created());
}

TEST_CASE("the Cycle tab builds its table from the real archive") {
    MemoryApp app(helpers::real_sessions());
    CycleView view(app.model);
    const auto& matrix = app.model.cycle_matrix();
    CHECK(view.table()->rowCount() == static_cast<int>(matrix.exercises.size()));
    CHECK(view.table()->columnCount() == static_cast<int>(matrix.days.size()) + 1);
    CHECK(view.table()->item(0, 0)->toolTip() == qs(matrix.exercises[0]));
}

TEST_CASE("the Plan tab shows the cycle, and a read-only folder disables every editing control") {
    MemoryApp writable;
    PlanView plan(writable.model);
    CHECK(plan.cycle_box()->count() == 1);
    CHECK(plan.save_button()->isEnabled());
    auto labels = plan.findChildren<QLabel*>();
    CHECK(std::any_of(labels.begin(), labels.end(), [](QLabel* l) {
        return l->text() == "8 workouts · starts 2026-07-21 · Tue, Thu, Sun";
    }));

    MemoryApp locked({}, helpers::real_references(), false);
    PlanView read_only(locked.model);
    CHECK_FALSE(locked.model.can_edit_plan());
    for (auto* b : {read_only.new_button(), read_only.save_button(), read_only.clone_button(), read_only.edit_button()})
        CHECK_FALSE(b->isEnabled());
    auto buttons = read_only.findChildren<QPushButton*>();
    for (auto* b : buttons)
        if (b->text().startsWith("+ Add")) CHECK_FALSE(b->isEnabled());
}
