#include "run_view.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include "app_model.hpp"
#include "block_cards.hpp"
#include "plan_dialogs.hpp"
#include "theme.hpp"
#include "widgets.hpp"
#include "workoutlog/cycle_planning.hpp"
#include "workoutlog/date.hpp"
#include "workoutlog/notation.hpp"

using namespace wl;

namespace {

constexpr int kDayWidth = 360;

QFrame* card(int width) {
    auto* frame = new QFrame;
    frame->setObjectName("runCard");
    frame->setStyleSheet(QString("#runCard { background: %1; border-radius: 8px; }").arg(theme::card_surface().name()));
    if (width > 0) frame->setFixedWidth(width);
    return frame;
}

QLabel* caption(const QString& text) {
    auto* label = new QLabel(text);
    label->setStyleSheet(small_style());
    label->setWordWrap(true);
    return label;
}

QString load_text(const PlannedLoad& load) {
    QStringList parts;
    parts << QString("%1 sets").arg(load.sets) << QString("%1 reps").arg(load.reps);
    if (load.metcon_rounds) parts << QString("%1 wod rounds").arg(load.metcon_rounds);
    parts << NumberField::format(load.tonnage_kg) + " kg";
    return parts.join(" · ");
}

// A logged set without weight is bodyweight (`6×bw`); a planned one is a weight
// not decided yet, so it prints as a gap to fill.
QString sets_text(const std::vector<WorkSet>& sets) {
    if (sets.empty()) return "no sets yet";
    QStringList parts;
    for (const auto& s : sets) {
        bool undecided = !s.weight_kg && s.reps && !s.duration_sec && !s.cluster;
        parts << (undecided ? QString("%1×—").arg(*s.reps) : qs(set_summary(s)));
    }
    return parts.join("  ");
}

std::optional<size_t> ask_new_run(AppModel& model, QWidget* parent) {
    const auto& cycles = model.cycles();
    if (cycles.empty()) return std::nullopt;
    QDialog dialog(parent);
    dialog.setWindowTitle("New run");
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* cycle = new QComboBox;
    for (const auto& c : cycles) cycle->addItem(qs(cycle_label(c)));
    cycle->setCurrentIndex(static_cast<int>(cycles.size()) - 1);
    auto* start = new QLineEdit(qs(cycles.back().start_date));
    start->setPlaceholderText("YYYY-MM-DD");
    QObject::connect(cycle, &QComboBox::currentIndexChanged, &dialog, [&, start](int i) {
        if (i >= 0) start->setText(qs(cycles[static_cast<size_t>(i)].start_date));
    });
    form->addRow("Cycle", cycle);
    form->addRow("First day", start);
    layout->addLayout(form);
    auto* error = new QLabel;
    error->setStyleSheet("color:#d32f2f;");
    error->setWordWrap(true);
    error->hide();
    layout->addWidget(error);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    buttons->addButton("Start run", QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);

    std::optional<size_t> started;
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        auto when = parse_date(ss(start->text().trimmed()));
        if (!when) {
            error->setText("First day must be YYYY-MM-DD.");
            error->show();
            return;
        }
        started = model.start_run(static_cast<size_t>(cycle->currentIndex()), *when);
        if (!started) {
            error->setText(qs(model.status()));
            error->show();
            return;
        }
        dialog.accept();
    });
    dialog.exec();
    return started;
}

} // namespace

RunView::RunView(AppModel& model, QWidget* parent, Confirm confirm_overwrite)
    : QWidget(parent), model_(model), confirm_(std::move(confirm_overwrite)) {
    if (!confirm_)
        confirm_ = [this](const std::vector<std::string>& ids) {
            QStringList names;
            for (const auto& id : ids) names << qs(id);
            return confirm(this, "Overwrite sessions",
                           QString("Already generated:\n%1\n\nOverwrite with the prescription? Anything logged in "
                                   "them is replaced.")
                               .arg(names.join("\n")),
                           "Overwrite");
        };

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(8, 8, 8, 8);
    run_box_ = new QComboBox;
    run_box_->setFixedWidth(360);
    new_ = new QPushButton("+ New run");
    save_ = new QPushButton("Save run");
    generate_all_ = new QPushButton("Generate all days");
    bar->addWidget(new QLabel("Run"));
    bar->addWidget(run_box_);
    for (auto* b : {new_, save_, generate_all_}) {
        b->setAutoDefault(false);
        bar->addWidget(b);
    }
    bar->addStretch();
    layout->addLayout(bar);
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    layout->addWidget(line);
    detail_ = new QVBoxLayout;
    layout->addLayout(detail_, 1);

    connect(run_box_, &QComboBox::activated, this, [this](int i) { select(static_cast<size_t>(i)); });
    connect(new_, &QPushButton::clicked, this, [this] {
        if (auto started = ask_new_run(model_, this)) select(started);
    });
    connect(save_, &QPushButton::clicked, this, [this] {
        if (auto current = selected()) model_.save_run(*current);
    });
    connect(generate_all_, &QPushButton::clicked, this, [this] {
        auto current = selected();
        if (!current) return;
        std::vector<size_t> all(model_.prescriptions()[*current].days.size());
        for (size_t i = 0; i < all.size(); ++i) all[i] = i;
        generate(std::move(all));
    });
    refresh();
}

std::optional<size_t> RunView::selected() const {
    const auto& runs = model_.prescriptions();
    if (runs.empty()) return std::nullopt;
    for (size_t i = 0; i < runs.size(); ++i)
        if (runs[i].file_name() == selected_file_) return i;
    return runs.size() - 1;
}

void RunView::select(std::optional<size_t> index) {
    selected_file_ = index && *index < model_.prescriptions().size() ? model_.prescriptions()[*index].file_name() : "";
    refresh();
}

void RunView::refresh() {
    auto current = selected();
    if (current) selected_file_ = model_.prescriptions()[*current].file_name();
    run_box_->clear();
    for (const auto& run : model_.prescriptions()) run_box_->addItem(qs(model_.run_label(run)));
    if (current) run_box_->setCurrentIndex(static_cast<int>(*current));
    bool editable = model_.can_edit_plan();
    new_->setEnabled(editable && !model_.cycles().empty());
    save_->setEnabled(editable && current && model_.run_unsaved(*current));
    generate_all_->setEnabled(current.has_value());
    rebuild_detail();
}

void RunView::generate(std::vector<size_t> days) {
    auto current = selected();
    if (!current) return;
    std::vector<std::string> existing;
    for (size_t day : days)
        if (model_.exists(model_.day_file(*current, day))) existing.push_back(model_.day_file(*current, day));
    if (!existing.empty() && !confirm_(existing)) return;
    model_.generate_days(*current, days);
}

void RunView::edited(size_t run) {
    model_.run_edited(run);
    save_->setEnabled(model_.can_edit_plan());
    update_totals();
}

void RunView::update_totals() {
    auto current = selected();
    if (!current || !run_totals_) return;
    const Prescription& run = model_.prescriptions()[*current];
    PlannedLoad total;
    for (size_t i = 0; i < run.days.size(); ++i) {
        PlannedLoad day = planned_load(run.days[i].blocks);
        total += day;
        if (i < day_totals_.size()) day_totals_[i]->setText(load_text(day));
    }
    run_totals_->setText(QString("%1 days · %2").arg(run.days.size()).arg(load_text(total)));
}

void RunView::rebuild_detail() {
    if (columns_) scroll_x_ = columns_->horizontalScrollBar()->value();
    if (detail_host_) {
        detail_->removeWidget(detail_host_);
        detail_host_->deleteLater();
    }
    columns_ = nullptr;
    run_totals_ = nullptr;
    day_totals_.clear();
    detail_host_ = new QWidget;
    detail_->addWidget(detail_host_);
    auto* layout = new QVBoxLayout(detail_host_);

    auto current = selected();
    if (!current) {
        layout->addStretch();
        auto* empty = new QLabel("No runs yet. A run lays a cycle onto dates so each day's reps, rounds and "
                                 "weights can be set before its session is generated.");
        empty->setWordWrap(true);
        empty->setAlignment(Qt::AlignCenter);
        layout->addWidget(empty);
        auto* start = new QPushButton("+ New run");
        start->setEnabled(new_->isEnabled());
        connect(start, &QPushButton::clicked, new_, &QPushButton::click);
        layout->addWidget(start, 0, Qt::AlignCenter);
        layout->addStretch();
        return;
    }
    const size_t ri = *current;
    const Prescription& run = model_.prescriptions()[ri];

    auto* summary = card(0);
    auto* text = new QVBoxLayout(summary);
    text->addWidget(new QLabel("<b>" + qs(model_.run_label(run)).toHtmlEscaped() + "</b>"));
    run_totals_ = caption("");
    text->addWidget(run_totals_);
    text->addWidget(caption(qs(run.file_name())));
    layout->addWidget(summary);

    columns_ = new QScrollArea;
    columns_->setWidgetResizable(true);
    columns_->setFrameShape(QFrame::NoFrame);
    columns_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    columns_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* strip = new QWidget;
    auto* row = new QHBoxLayout(strip);
    row->setContentsMargins(0, 0, 0, 0);
    for (size_t i = 0; i < run.days.size(); ++i) row->addWidget(day_card(ri, i));
    row->addStretch();
    columns_->setWidget(strip);
    layout->addWidget(columns_, 1);
    columns_->horizontalScrollBar()->setValue(scroll_x_);
    update_totals();
}

QWidget* RunView::day_card(size_t ri, size_t di) {
    const PrescribedDay& day = model_.prescriptions()[ri].days[di];
    auto* frame = card(kDayWidth);
    auto* layout = new QVBoxLayout(frame);

    auto* header = new QHBoxLayout;
    auto parsed = CycleDay::try_parse(day.cycle_day);
    bool heavy = parsed && parsed->kind == DayKind::heavy;
    auto* badge = new QLabel(qs(day.cycle_day));
    badge->setStyleSheet(QString("background:%1;color:white;font-weight:bold;border-radius:8px;padding:3px 8px;")
                             .arg(heavy ? "#37474f" : "#e65100"));
    header->addWidget(badge);
    auto* titles = new QVBoxLayout;
    auto* title = new QLabel("<b>" + qs(day.title.value_or(day.cycle_day)).toHtmlEscaped() + "</b>");
    title->setWordWrap(true);
    titles->addWidget(title);
    QString when = qs(day.date);
    if (auto date = parse_date(day.date)) when += " · " + qs(std::string(weekday_abbreviation(*date)));
    titles->addWidget(caption(when));
    header->addLayout(titles, 1);

    bool generated = model_.exists(model_.day_file(ri, di));
    auto* generate_button = new QPushButton(generated ? "Regenerate" : "Generate");
    generate_button->setAutoDefault(false);
    generate_button->setToolTip(generated ? qs(model_.day_file(ri, di) + " exists; regenerating overwrites it")
                                          : qs("Write " + model_.day_file(ri, di)));
    connect(generate_button, &QPushButton::clicked, this, [this, di] { generate({di}); });
    header->addWidget(generate_button);
    layout->addLayout(header);

    auto* totals = caption("");
    totals->setStyleSheet(small_style() + "font-weight:bold;");
    day_totals_.push_back(totals);
    layout->addWidget(totals);
    if (generated) layout->addWidget(caption("✓ generated"));
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    layout->addWidget(line);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget;
    auto* blocks = new QVBoxLayout(content);
    blocks->setContentsMargins(0, 0, 0, 0);
    for (size_t bi = 0; bi < day.blocks.size(); ++bi) blocks->addWidget(block_row(ri, di, bi));
    blocks->addStretch();
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);
    return frame;
}

QWidget* RunView::block_row(size_t ri, size_t di, size_t bi) {
    Block& block = model_.prescriptions()[ri].days[di].blocks[bi];
    auto* host = new QWidget;
    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(2);

    std::visit(
        [&](auto& b) {
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, CardioBlock>) {
                QString minutes = b.duration_min ? NumberField::format(*b.duration_min) + " min" : "—";
                layout->addWidget(caption("🏃 cardio · " + minutes));
            } else if constexpr (std::is_same_v<T, CooldownBlock>) {
                layout->addWidget(caption("🌬 cooldown"));
            } else if constexpr (std::is_same_v<T, StrengthBlock>) {
                layout->addWidget(new QLabel("🏋 <b>" + qs(b.exercise).toHtmlEscaped() + "</b>"));
                if (b.notes) layout->addWidget(caption(qs(*b.notes)));
                auto* sets = new QLabel(sets_text(b.sets));
                sets->setFont(monospace_font());
                sets->setWordWrap(true);
                layout->addWidget(sets);
                auto* notation = new QLineEdit;
                notation->setFont(monospace_font());
                notation->setPlaceholderText("3 × [60, 80, 100] ⏎");
                notation->setToolTip("Replaces this block's sets. Same notation as the session editor.");
                auto* warning = caption("");
                warning->setStyleSheet(small_style() + "color:#e65100;");
                warning->hide();
                layout->addWidget(notation);
                layout->addWidget(warning);
                connect(notation, &QLineEdit::returnPressed, this, [this, ri, di, bi, notation, sets, warning] {
                    auto parsed = parse_strength_sets(ss(notation->text()));
                    QStringList problems;
                    for (const auto& w : parsed.warnings) problems << "⚠ " + qs(w);
                    if (parsed.sets.empty() && problems.isEmpty()) problems << "⚠ nothing to read in that line";
                    warning->setText(problems.join("\n"));
                    warning->setVisible(!problems.isEmpty());
                    if (parsed.sets.empty()) return;
                    auto& target = std::get<StrengthBlock>(model_.prescriptions()[ri].days[di].blocks[bi]);
                    target.sets = std::move(parsed.sets);
                    sets->setText(sets_text(target.sets));
                    notation->clear();
                    edited(ri);
                });
            } else {
                QString format = b.format ? qs(metcon_format_wire(*b.format)) : "wod";
                layout->addWidget(new QLabel("🔥 <b>" + format.toHtmlEscaped() + "</b>"));
                auto* rounds_row = new QHBoxLayout;
                rounds_row->addWidget(caption("rounds × reps"));
                auto* scheme = new QLineEdit(b.scheme ? qs(format_scheme(*b.scheme)) : QString());
                scheme->setPlaceholderText("21-15-9 or 8×10");
                scheme->setFixedWidth(140);
                rounds_row->addWidget(scheme);
                rounds_row->addStretch();
                layout->addLayout(rounds_row);

                auto* table_holder = new QWidget;
                auto* table_layout = new QVBoxLayout(table_holder);
                table_layout->setContentsMargins(0, 0, 0, 0);
                auto redraw_table = [this, ri, di, bi, table_layout] {
                    if (auto* old = table_layout->takeAt(0)) {
                        delete old->widget();
                        delete old;
                    }
                    const auto& m = std::get<MetconBlock>(model_.prescriptions()[ri].days[di].blocks[bi]);
                    table_layout->addWidget(make_metcon_table(m.format, m.scheme, m.exercises, false));
                };
                connect(scheme, &QLineEdit::textEdited, this, [this, ri, di, bi, scheme, redraw_table](const QString& t) {
                    auto& m = std::get<MetconBlock>(model_.prescriptions()[ri].days[di].blocks[bi]);
                    auto parsed = parse_scheme(ss(t));
                    bool valid = parsed || t.trimmed().isEmpty();
                    scheme->setStyleSheet(valid ? "" : "color:#d32f2f;");
                    if (!valid) return;
                    m.scheme = parsed;
                    redraw_table();
                    edited(ri);
                });

                for (size_t ei = 0; ei < b.exercises.size(); ++ei) {
                    auto* row = new QHBoxLayout;
                    auto* name = caption(qs(b.exercises[ei].name));
                    name->setWordWrap(false);
                    row->addWidget(name, 1);
                    auto exercise = [this, ri, di, bi, ei]() -> MetconExercise& {
                        return std::get<MetconBlock>(model_.prescriptions()[ri].days[di].blocks[bi]).exercises[ei];
                    };
                    row->addWidget(new NumberField(
                        "kg", b.exercises[ei].weight_kg,
                        [this, ri, exercise, redraw_table](std::optional<double> v) {
                            exercise().weight_kg = v;
                            redraw_table();
                            edited(ri);
                        },
                        56));
                    row->addWidget(new OptionalField(
                        "load (24kg+24kg)", b.exercises[ei].load,
                        [this, ri, exercise, redraw_table](std::optional<std::string> v) {
                            exercise().load = std::move(v);
                            redraw_table();
                            edited(ri);
                        },
                        110));
                    layout->addLayout(row);
                }
                layout->addWidget(table_holder);
                redraw_table();
                if (b.notes) layout->addWidget(caption(qs(*b.notes)));
            }
        },
        block);
    return host;
}
