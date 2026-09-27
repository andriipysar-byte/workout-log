#include "plan_view.hpp"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "app_model.hpp"
#include "block_cards.hpp"
#include "exercise_picker.hpp"
#include "muscle_map_widget.hpp"
#include "plan_dialogs.hpp"
#include "theme.hpp"
#include "widgets.hpp"
#include "workoutlog/date.hpp"

using namespace wl;

namespace {

constexpr int kWorkoutWidth = 380;
constexpr int kTotalWidth = 300;
constexpr int kAddWidth = 200;

QFrame* card(int width) {
    auto* frame = new QFrame;
    frame->setObjectName("planCard");
    frame->setStyleSheet(QString("#planCard { background: %1; border-radius: 8px; }").arg(theme::card_surface().name()));
    if (width > 0) frame->setFixedWidth(width);
    return frame;
}

QLabel* caption(const QString& text) {
    auto* label = new QLabel(text);
    label->setStyleSheet(small_style());
    label->setWordWrap(true);
    return label;
}

QToolButton* compact(const QString& glyph, const QString& tooltip) {
    auto* button = new QToolButton;
    button->setText(glyph);
    button->setToolTip(tooltip);
    button->setAutoRaise(true);
    button->setFixedSize(26, 26);
    return button;
}

QString glyph_for(const std::string& type) {
    if (type == "cardio") return "🏃";
    if (type == "strength") return "🏋";
    if (type == "metcon") return "🔥";
    return "🌬";
}

} // namespace

PlanView::PlanView(AppModel& model, QWidget* parent) : QWidget(parent), model_(model) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(8, 8, 8, 8);
    cycle_box_ = new QComboBox;
    cycle_box_->setFixedWidth(260);
    new_ = new QPushButton("+ New cycle");
    clone_ = new QPushButton("Clone");
    edit_ = new QPushButton("Edit");
    version_ = new QPushButton("New version");
    version_->setToolTip("Copy this cycle as its next version; the current one stays as it is.");
    save_ = new QPushButton("Save cycles.json");
    lock_ = new QLabel("🔒");
    lock_->setToolTip("cycles.json is read-only: the folder above the session folder is missing.");
    bar->addWidget(new QLabel("Cycle"));
    bar->addWidget(cycle_box_);
    for (auto* b : {new_, clone_, edit_, version_, save_}) {
        b->setAutoDefault(false);
        bar->addWidget(b);
    }
    bar->addWidget(lock_);
    bar->addStretch();
    layout->addLayout(bar);
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    layout->addWidget(line);
    detail_ = new QVBoxLayout;
    layout->addLayout(detail_, 1);

    connect(cycle_box_, &QComboBox::activated, this, [this](int i) { select(static_cast<size_t>(i)); });
    connect(new_, &QPushButton::clicked, this, [this] {
        CycleDialog dialog(model_, CycleDialog::Mode::create, std::nullopt, this);
        if (dialog.exec() == QDialog::Accepted) select(dialog.result_index());
    });
    connect(clone_, &QPushButton::clicked, this, [this] {
        CycleDialog dialog(model_, CycleDialog::Mode::clone, selected(), this);
        if (dialog.exec() == QDialog::Accepted) select(dialog.result_index());
    });
    connect(edit_, &QPushButton::clicked, this, [this] {
        CycleDialog dialog(model_, CycleDialog::Mode::edit, selected(), this);
        if (dialog.exec() == QDialog::Accepted) select(dialog.result_index());
    });
    connect(version_, &QPushButton::clicked, this, [this] {
        if (auto current = selected()) select(model_.new_version(*current));
    });
    connect(save_, &QPushButton::clicked, this, [this] { model_.save_cycles(); });
    refresh();
}

std::optional<size_t> PlanView::selected() const {
    const auto& cycles = model_.cycles();
    if (cycles.empty()) return std::nullopt;
    for (size_t i = 0; i < cycles.size(); ++i)
        if (cycles[i].id == selected_id_ && cycles[i].version_number() == selected_version_) return i;
    return 0;
}

void PlanView::select(std::optional<size_t> index) {
    bool valid = index && *index < model_.cycles().size();
    selected_id_ = valid ? model_.cycles()[*index].id : "";
    selected_version_ = valid ? model_.cycles()[*index].version_number() : 1;
    refresh();
}

void PlanView::refresh() {
    auto current = selected();
    if (current) {
        selected_id_ = model_.cycles()[*current].id;
        selected_version_ = model_.cycles()[*current].version_number();
    }
    cycle_box_->clear();
    for (const auto& c : model_.cycles()) cycle_box_->addItem(qs(cycle_label(c)));
    if (current) cycle_box_->setCurrentIndex(static_cast<int>(*current));

    bool editable = model_.can_edit_plan();
    new_->setEnabled(editable);
    clone_->setEnabled(editable && current.has_value());
    edit_->setEnabled(editable && current.has_value());
    version_->setEnabled(editable && current.has_value());
    save_->setEnabled(editable);
    lock_->setVisible(!editable);
    rebuild_detail();
}

void PlanView::rebuild_detail() {
    if (columns_) scroll_x_ = columns_->horizontalScrollBar()->value();
    if (detail_host_) {
        detail_->removeWidget(detail_host_);
        detail_host_->deleteLater();
    }
    columns_ = nullptr;
    detail_host_ = new QWidget;
    detail_->addWidget(detail_host_);
    auto* layout = new QVBoxLayout(detail_host_);

    auto current = selected();
    if (!current) {
        layout->addStretch();
        auto* empty = new QLabel("No cycles yet.");
        empty->setAlignment(Qt::AlignCenter);
        layout->addWidget(empty);
        auto* create = new QPushButton("+ New cycle");
        create->setEnabled(model_.can_edit_plan());
        connect(create, &QPushButton::clicked, new_, &QPushButton::click);
        layout->addWidget(create, 0, Qt::AlignCenter);
        layout->addStretch();
        return;
    }
    const size_t ci = *current;
    const Cycle& cycle = model_.cycles()[ci];

    auto* summary = card(0);
    auto* summary_row = new QHBoxLayout(summary);
    auto* text = new QVBoxLayout;
    auto* title = new QLabel("<b>" + qs(cycle_label(cycle)).toHtmlEscaped() + "</b>");
    QStringList days;
    for (const auto& d : cycle.training_days) days << qs(d);
    text->addWidget(title);
    text->addWidget(caption(QString("%1 workouts · starts %2 · %3")
                                .arg(cycle.sessions.size())
                                .arg(qs(cycle.start_date), days.join(", "))));
    summary_row->addLayout(text, 1);
    summary_row->addWidget(new MuscleGroupLegend);
    layout->addWidget(summary);

    auto* body = new QHBoxLayout;
    body->addWidget(total_column(ci));
    columns_ = new QScrollArea;
    columns_->setWidgetResizable(true);
    columns_->setFrameShape(QFrame::NoFrame);
    columns_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    columns_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* strip = new QWidget;
    auto* row = new QHBoxLayout(strip);
    row->setContentsMargins(0, 0, 0, 0);
    for (size_t i = 0; i < cycle.sessions.size(); ++i) row->addWidget(workout_card(ci, i));

    std::vector<std::string> codes;
    for (const auto& s : cycle.sessions) codes.push_back(s.cycle_day);
    auto* add_host = new QWidget;
    add_host->setFixedWidth(kAddWidth);
    auto* add_layout = new QVBoxLayout(add_host);
    auto* add = new QPushButton(qs("+ Add " + cycle_templates::next_day(codes).code()));
    add->setEnabled(model_.can_edit_plan());
    connect(add, &QPushButton::clicked, this, [this, ci] {
        model_.add_workout(ci);
        refresh();
    });
    add_layout->addWidget(add);
    add_layout->addStretch();
    row->addWidget(add_host);
    row->addStretch();
    columns_->setWidget(strip);
    body->addWidget(columns_, 1);
    layout->addLayout(body, 1);
    columns_->horizontalScrollBar()->setValue(scroll_x_);
}

// The whole cycle's activation, beside the per-day maps it is the sum of.
QWidget* PlanView::total_column(size_t ci) {
    const Cycle& cycle = model_.cycles()[ci];
    auto* frame = card(kTotalWidth);
    auto* layout = new QVBoxLayout(frame);
    layout->addWidget(new QLabel("<b>Cycle total</b>"));
    layout->addWidget(caption(QString("%1 workouts · by set count").arg(cycle.sessions.size())));
    auto* map = new MuscleMapWidget(0);
    map->set_alignment(Qt::AlignTop);
    map->set_svg(model_.plan_cycle_map_svg(cycle), "No exercises chosen yet — nothing to map.");
    layout->addWidget(map, 1);
    return frame;
}

QWidget* PlanView::workout_card(size_t ci, size_t wi) {
    Cycle& cycle = model_.cycles()[ci];
    CycleSession& workout = cycle.sessions[wi];
    bool editable = model_.can_edit_plan();
    auto* frame = card(kWorkoutWidth);
    auto* layout = new QVBoxLayout(frame);

    auto* header = new QHBoxLayout;
    auto group = model_.plan_dominant_group(workout);
    auto* badge = new QLabel(qs(workout.cycle_day));
    badge->setStyleSheet(QString("background:%1;color:white;font-weight:bold;border-radius:8px;padding:3px 8px;")
                             .arg((group ? theme::group_color(*group) : theme::kUnclassified).name()));
    header->addWidget(badge);
    auto* titles = new QVBoxLayout;
    auto day = CycleDay::try_parse(workout.cycle_day);
    titles->addWidget(new QLabel("<b>" + (day ? qs(std::string(day_kind_label(day->kind))) : "custom") + "</b>"));
    QStringList line;
    if (auto date = model_.planned_date(cycle, wi)) line << qs(iso_date(*date));
    if (workout.weekday) line << qs(*workout.weekday);
    if (workout.title) line << qs(*workout.title);
    auto* sub = caption(line.join(" · "));
    sub->setWordWrap(false);
    titles->addWidget(sub);
    header->addLayout(titles, 1);

    bool map_shown = !hidden_maps_.contains(workout.cycle_day);
    auto* eye = compact(map_shown ? "◉" : "○", map_shown ? "Hide muscle map" : "Show muscle map");
    std::string code = workout.cycle_day;
    connect(eye, &QToolButton::clicked, this, [this, code] {
        if (!hidden_maps_.erase(code)) hidden_maps_.insert(code);
        rebuild_detail();
    });
    header->addWidget(eye);

    // Four icon buttons do not fit a column this narrow, so the rarer actions
    // collapse into a menu.
    if (editable) {
        auto* more = compact("⋮", "Workout actions");
        more->setPopupMode(QToolButton::InstantPopup);
        auto* menu = new QMenu(more);
        auto* left = menu->addAction("← Move left");
        auto* right = menu->addAction("→ Move right");
        auto* rename = menu->addAction("# Rename workout");
        auto* remove = menu->addAction("✕ Remove workout");
        left->setEnabled(wi > 0);
        right->setEnabled(wi + 1 < cycle.sessions.size());
        connect(left, &QAction::triggered, this, [this, ci, wi] {
            model_.move_workout(ci, wi, static_cast<long>(wi) - 1);
            refresh();
        });
        connect(right, &QAction::triggered, this, [this, ci, wi] {
            model_.move_workout(ci, wi, static_cast<long>(wi) + 1);
            refresh();
        });
        connect(rename, &QAction::triggered, this, [this, ci, wi] {
            if (auto chosen = ask_retitle(this, model_.cycles()[ci].sessions[wi].cycle_day)) {
                model_.retitle_workout(ci, wi, *chosen);
                refresh();
            }
        });
        connect(remove, &QAction::triggered, this, [this, ci, wi] {
            if (confirm(this, "Remove workout",
                        qs("Remove " + model_.cycles()[ci].sessions[wi].cycle_day + " from this cycle?"), "Remove")) {
                model_.remove_workout(ci, wi);
                refresh();
            }
        });
        more->setMenu(menu);
        header->addWidget(more);
    }
    layout->addLayout(header);
    auto* line_sep = new QFrame;
    line_sep->setFrameShape(QFrame::HLine);
    layout->addWidget(line_sep);

    // A column is height-bounded by the row, so a long block list scrolls inside
    // its own card rather than pushing the row taller.
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget;
    auto* blocks = new QVBoxLayout(content);
    blocks->setContentsMargins(0, 0, 0, 0);
    if (map_shown) {
        auto* map = new MuscleMapWidget(230);
        map->set_svg(model_.plan_map_svg(workout), "No exercises chosen yet — nothing to map.");
        blocks->addWidget(map);
    }
    for (size_t bi = 0; bi < workout.blocks.size(); ++bi) blocks->addWidget(block_row(ci, wi, bi, editable));
    auto* add = new QPushButton("+ Add block");
    add->setFlat(true);
    add->setEnabled(editable);
    connect(add, &QPushButton::clicked, this, [this, ci, wi] {
        if (auto type = ask_block_type(this)) {
            BlockTemplate b;
            b.type = *type;
            model_.cycles()[ci].sessions[wi].blocks.push_back(std::move(b));
            model_.cycle_edited();
            refresh();
        }
    });
    blocks->addWidget(add, 0, Qt::AlignLeft);
    blocks->addStretch();
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);
    return frame;
}

QWidget* PlanView::block_row(size_t ci, size_t wi, size_t bi, bool enabled) {
    BlockTemplate& block = model_.cycles()[ci].sessions[wi].blocks[bi];
    auto* host = new QWidget;
    auto* outer = new QVBoxLayout(host);
    outer->setContentsMargins(0, 2, 0, 2);
    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(glyph_for(block.type)));

    auto* text = new QVBoxLayout;
    bool strength = block.type == "strength";
    bool blank = strength && (!block.exercise || block.exercise->empty());
    QString title = strength ? (blank ? "choose exercise…" : qs(*block.exercise)) : qs(block.type);
    auto* title_label = new QLabel(blank ? "<i>" + title + "</i>" : title.toHtmlEscaped());
    if (blank) title_label->setStyleSheet(QString("color:%1;").arg(theme::outline().name()));
    auto* title_row = new QHBoxLayout;
    title_row->addWidget(title_label);
    if (strength && !blank && (!model_.catalogue() || !model_.catalogue()->resolve(*block.exercise))) {
        auto* warn = new QLabel("⚠");
        warn->setStyleSheet("color:#e65100;");
        warn->setToolTip(
            qs("\"" + *block.exercise + "\" is not in the catalogue, so it cannot reach the muscle map."));
        title_row->addWidget(warn);
    }
    title_row->addStretch();
    text->addLayout(title_row);

    QString subtitle;
    if (strength) {
        QStringList parts;
        if (block.role) parts << qs(*block.role);
        if (!block.sets_reps.empty()) {
            QStringList reps;
            for (auto r : block.sets_reps) reps << QString::number(r);
            parts << reps.join("+");
        }
        subtitle = parts.join(" · ");
    } else if (block.type == "cardio") {
        subtitle = (block.duration_min ? QString::number(static_cast<long long>(*block.duration_min)) : "—") + " min";
    } else if (block.type != "metcon") {
        subtitle = qs(block.role.value_or(""));
    }
    if (!subtitle.isEmpty()) text->addWidget(caption(subtitle));
    row->addLayout(text, 1);

    if (enabled) {
        if (strength) {
            auto* pick = compact("🔍", "Choose exercise");
            connect(pick, &QToolButton::clicked, this, [this, ci, wi, bi] {
                BlockTemplate& b = model_.cycles()[ci].sessions[wi].blocks[bi];
                ExercisePicker picker(model_, b.exercise, this);
                if (picker.exec() == QDialog::Accepted && picker.chosen()) {
                    b.exercise = picker.chosen();
                    model_.cycle_edited();
                    refresh();
                }
            });
            row->addWidget(pick);
        }
        auto* edit = compact("⚙", "Edit block");
        connect(edit, &QToolButton::clicked, this, [this, ci, wi, bi] {
            BlockDialog dialog(model_.cycles()[ci].sessions[wi].blocks[bi], this);
            if (dialog.exec() == QDialog::Accepted) {
                model_.cycle_edited();
                refresh();
            }
        });
        auto* remove = compact("✕", "Remove block");
        connect(remove, &QToolButton::clicked, this, [this, ci, wi, bi] {
            auto& blocks = model_.cycles()[ci].sessions[wi].blocks;
            blocks.erase(blocks.begin() + static_cast<long>(bi));
            model_.cycle_edited();
            refresh();
        });
        row->addWidget(edit);
        row->addWidget(remove);
    }
    outer->addLayout(row);
    // A metcon is a table of movements, not a one-line summary.
    if (block.type == "metcon") {
        auto* table = make_metcon_table(block.format, block.scheme, block.exercises);
        table->setContentsMargins(30, 0, 0, 0);
        outer->addWidget(table);
    }
    return host;
}
