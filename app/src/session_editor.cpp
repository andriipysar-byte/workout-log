#include "session_editor.hpp"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "app_model.hpp"
#include "block_cards.hpp"
#include "muscle_map_widget.hpp"
#include "theme.hpp"
#include "widgets.hpp"

using namespace wl;

namespace {

QFrame* divider() {
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

} // namespace

SessionEditor::SessionEditor(AppModel& model, QWidget* parent) : QScrollArea(parent), model_(model) {
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 20, 20, 20);

    Session& session = *model_.session();
    auto changed = [this] { model_.session_edited(); };

    auto* header = new QWidget;
    auto* rows = new QVBoxLayout(header);
    rows->setContentsMargins(0, 0, 0, 0);
    auto* flow = new QHBoxLayout;
    auto* second = new QHBoxLayout;
    rows->addLayout(flow);
    rows->addLayout(second);
    auto add = [](QHBoxLayout* row, const QString& title, QWidget* field, int stretch = 0) {
        auto* label = new QLabel(title);
        label->setStyleSheet(small_style());
        row->addWidget(label);
        row->addWidget(field, stretch);
        row->addSpacing(12);
    };
    add(flow, "Date",
        new OptionalField("YYYY-MM-DD", session.date,
                          [&session, changed](auto v) { session.date = v.value_or(""); changed(); }, 110));
    add(flow, "Cycle day",
        new OptionalField("A1", session.cycle_day,
                          [&session, changed](auto v) { session.cycle_day = v.value_or(""); changed(); }, 60));
    add(flow, "Start",
        new OptionalField("H:MM", session.start_time,
                          [&session, changed](auto v) { session.start_time = v; changed(); }, 60));
    auto* kind = new QComboBox;
    for (size_t i = 0; i < enum_count<Kind>(); ++i) kind->addItem(qs(std::string(name(static_cast<Kind>(i)))));
    kind->setCurrentIndex(static_cast<int>(session.kind));
    connect(kind, &QComboBox::currentIndexChanged, this, [&session, changed](int i) {
        session.kind = static_cast<Kind>(i);
        changed();
    });
    add(flow, "Kind", kind);
    flow->addStretch();
    add(second, "Bodyweight",
        new NumberField("kg", session.bodyweight_kg, [&session, changed](auto v) { session.bodyweight_kg = v; changed(); },
                        70));
    add(second, "Notes",
        new OptionalField("", session.notes, [&session, changed](auto v) { session.notes = v; changed(); }), 1);
    layout->addWidget(header);
    layout->addWidget(divider());

    for (auto& block : session.blocks) layout->addWidget(make_block_card(block, model_, changed));

    layout->addWidget(divider());
    auto* map_header = new QHBoxLayout;
    auto* title = new QLabel("<b>Muscle map</b>");
    map_header->addWidget(title);
    map_header->addStretch();
    picker_ = new WeightingModePicker(model_);
    map_header->addWidget(picker_);
    layout->addLayout(map_header);
    map_ = new MuscleMapWidget(280);
    layout->addWidget(map_);
    layout->addStretch();
    setWidget(content);
    refresh_map();
}

void SessionEditor::refresh_map() {
    picker_->sync();
    map_->set_svg(model_.day_map_svg(), "Muscle map unavailable (catalogue or template not found).");
}
