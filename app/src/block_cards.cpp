#include "block_cards.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include "app_model.hpp"
#include "muscle_map_widget.hpp"
#include "theme.hpp"
#include "widgets.hpp"
#include "workoutlog/notation.hpp"

using namespace wl;

namespace {

QWidget* header(const QString& glyph, const QString& title) {
    auto* label = new QLabel(QString("<b>%1&nbsp;&nbsp;%2</b>").arg(glyph, title.toHtmlEscaped()));
    label->setTextFormat(Qt::RichText);
    return label;
}

QLabel* caption(const QString& text) {
    auto* label = new QLabel(text);
    label->setStyleSheet(small_style());
    label->setWordWrap(true);
    return label;
}

QFrame* card() {
    auto* frame = new QFrame;
    frame->setObjectName("blockCard");
    frame->setStyleSheet(QString("#blockCard { background: %1; border-radius: 8px; }").arg(theme::card_surface().name()));
    return frame;
}

QString join_summaries(const std::vector<WorkSet>& sets, const char* separator) {
    QStringList parts;
    for (const auto& s : sets) parts << qs(set_summary(s));
    return parts.join(separator);
}

void cardio(QVBoxLayout* layout, CardioBlock& b, std::function<void()> changed) {
    layout->addWidget(header("🏃", "Cardio"));
    auto* row = new QHBoxLayout;
    row->addWidget(new OptionalField(
        "machine", b.machine.empty() ? std::nullopt : std::optional(b.machine),
        [&b, changed](auto v) { b.machine = v.value_or(""); changed(); }, 200));
    row->addWidget(new NumberField("min", b.duration_min, [&b, changed](auto v) { b.duration_min = v; changed(); }, 80));
    row->addWidget(
        new NumberField("distance m", b.distance_m, [&b, changed](auto v) { b.distance_m = v; changed(); }, 110));
    row->addWidget(new OptionalField("end", b.end_time, [&b, changed](auto v) { b.end_time = v; changed(); }, 80));
    row->addStretch();
    layout->addLayout(row);
}

void cooldown(QVBoxLayout* layout, CooldownBlock& b, std::function<void()> changed) {
    layout->addWidget(header("🌬", "Cooldown"));
    auto* row = new QHBoxLayout;
    row->addWidget(new OptionalField("end", b.end_time, [&b, changed](auto v) { b.end_time = v; changed(); }, 90));
    row->addWidget(new OptionalField("notes", b.notes, [&b, changed](auto v) { b.notes = v; changed(); }));
    layout->addLayout(row);
}

// Read-only by design: rounds and splits are transcribed from paper, and an
// editor for them is not in scope.
void metcon(QVBoxLayout* layout, const MetconBlock& b) {
    layout->addWidget(header("🔥", "Metcon"));
    layout->addWidget(make_metcon_table(b.format, b.scheme, b.exercises));
    if (b.rounds) layout->addWidget(caption(QString("rounds: %1").arg(b.rounds->size())));
    if (b.notes) layout->addWidget(caption(qs(*b.notes)));
}

} // namespace

QWidget* make_metcon_table(const std::optional<MetconFormat>& format, const std::optional<IntList>& scheme,
                           const std::vector<MetconExercise>& exercises, bool show_header) {
    auto* box = new QWidget;
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    if (show_header) {
        auto* title = caption(QString("<b>wod: %1</b>").arg(format ? qs(metcon_format_wire(*format)) : "—"));
        title->setTextFormat(Qt::RichText);
        layout->addWidget(title);
    }
    if (exercises.empty()) {
        auto* empty = caption("<i>no movements yet</i>");
        empty->setTextFormat(Qt::RichText);
        layout->addWidget(empty);
        return box;
    }
    size_t rounds = 0;
    for (const auto& e : exercises) rounds = std::max(rounds, e.scheme_within(scheme).size());

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(0);
    grid->setVerticalSpacing(2);
    grid->setColumnStretch(0, 1);
    for (size_t r = 0; r < exercises.size(); ++r) {
        const auto& e = exercises[r];
        QString text = qs(e.name).toHtmlEscaped();
        if (auto p = e.prescription())
            text += QString("&nbsp;&nbsp;<span style='color:%1'>(%2)</span>")
                        .arg(theme::outline().name(), qs(*p).toHtmlEscaped());
        auto* name = caption(text);
        name->setTextFormat(Qt::RichText);
        grid->addWidget(name, static_cast<int>(r), 0);
        auto own = e.scheme_within(scheme);
        for (size_t c = 0; c < rounds; ++c) {
            auto* cell = caption(c < own.size() ? QString::number(own[c]) : QString());
            cell->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            cell->setFixedWidth(32);
            grid->addWidget(cell, static_cast<int>(r), static_cast<int>(c + 1));
        }
    }
    layout->addLayout(grid);
    return box;
}

StrengthEditor::StrengthEditor(StrengthBlock& block, AppModel& model, std::function<void()> on_changed,
                               QWidget* parent)
    : QWidget(parent), block_(block), model_(model), on_changed_(std::move(on_changed)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(header("🏋", qs(block_.exercise)));
    if (block_.notes) layout->addWidget(caption(qs(*block_.notes)));

    summary_ = new QLabel;
    summary_->setFont(monospace_font());
    summary_->setWordWrap(true);
    layout->addWidget(summary_);

    auto* row = new QHBoxLayout;
    notation_ = new QLineEdit;
    notation_->setFont(monospace_font());
    notation_->setPlaceholderText("6 × [70, 80, 90, 100, 110]");
    apply_ = new QPushButton("Apply");
    apply_->setAutoDefault(false);
    row->addWidget(notation_);
    row->addWidget(apply_);
    layout->addLayout(row);

    preview_ = new QLabel;
    preview_->setWordWrap(true);
    layout->addWidget(preview_);
    warnings_ = new QWidget;
    new QVBoxLayout(warnings_);
    warnings_->layout()->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(warnings_);

    auto* toggle = new QToolButton;
    toggle->setText("Muscle map");
    toggle->setCheckable(true);
    toggle->setArrowType(Qt::RightArrow);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setAutoRaise(true);
    layout->addWidget(toggle);
    map_holder_ = new QWidget;
    new QVBoxLayout(map_holder_);
    map_holder_->layout()->setContentsMargins(0, 0, 0, 0);
    map_holder_->hide();
    layout->addWidget(map_holder_);

    connect(notation_, &QLineEdit::textChanged, this, &StrengthEditor::update_preview);
    connect(notation_, &QLineEdit::returnPressed, this, &StrengthEditor::apply);
    connect(apply_, &QPushButton::clicked, this, &StrengthEditor::apply);
    connect(toggle, &QToolButton::toggled, this, [this, toggle](bool open) {
        toggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        toggle_map(open);
    });
    update_preview();
}

void StrengthEditor::update_preview() {
    summary_->setText(block_.sets.empty() ? "<span style='font-size:11px'>no sets yet</span>"
                                          : join_summaries(block_.sets, "   ").toHtmlEscaped());
    summary_->setTextFormat(Qt::RichText);

    QString text = notation_->text();
    auto parsed = parse_strength_sets(ss(text));
    apply_->setEnabled(!parsed.sets.empty());

    QLayoutItem* item;
    while ((item = warnings_->layout()->takeAt(0))) {
        delete item->widget();
        delete item;
    }
    if (text.isEmpty()) {
        preview_->hide();
        return;
    }
    preview_->show();
    bool ok = !parsed.sets.empty();
    preview_->setText("→ " + (ok ? join_summaries(parsed.sets, "  ") : QString("—")));
    preview_->setStyleSheet(small_style() + (ok ? "color:#2e7d32;" : "color:#d32f2f;"));
    for (const auto& w : parsed.warnings) {
        auto* label = new QLabel("⚠ " + qs(w));
        label->setWordWrap(true);
        label->setStyleSheet(small_style() + "color:#e65100;");
        warnings_->layout()->addWidget(label);
    }
}

void StrengthEditor::apply() {
    auto parsed = parse_strength_sets(ss(notation_->text()));
    if (parsed.sets.empty()) return;
    block_.sets = std::move(parsed.sets);
    notation_->clear();
    update_preview();
    on_changed_();
}

// Built only when opened, so a long session does not colourize one SVG per
// exercise up front.
void StrengthEditor::toggle_map(bool open) {
    map_holder_->setVisible(open);
    if (!open || map_) return;
    map_ = new MuscleMapWidget(200);
    map_->set_svg(model_.exercise_map_svg(block_.exercise),
                  QString("\"%1\" not in catalogue — no map.").arg(qs(block_.exercise)));
    map_holder_->layout()->addWidget(map_);
}

QFrame* make_block_card(Block& block, AppModel& model, std::function<void()> on_changed) {
    QFrame* frame = card();
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(12, 12, 12, 12);
    std::visit(
        [&](auto& b) {
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, CardioBlock>)
                cardio(layout, b, on_changed);
            else if constexpr (std::is_same_v<T, StrengthBlock>)
                layout->addWidget(new StrengthEditor(b, model, on_changed));
            else if constexpr (std::is_same_v<T, MetconBlock>)
                metcon(layout, b);
            else
                cooldown(layout, b, on_changed);
        },
        block);
    return frame;
}
