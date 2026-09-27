#include "plan_dialogs.hpp"

#include <QButtonGroup>
#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include "app_model.hpp"
#include "theme.hpp"
#include "workoutlog/dart_parse.hpp"
#include "workoutlog/date.hpp"
#include "workoutlog/utf8.hpp"

using namespace wl;

wl::IntList parse_numbers(const QString& text) {
    IntList out;
    for (const auto& token : text.split(QRegularExpression("[,\\s+]+"), Qt::SkipEmptyParts))
        if (auto n = dart_int_parse(ss(token))) out.push_back(*n);
    return out;
}

bool confirm(QWidget* parent, const QString& title, const QString& message, const QString& action) {
    QMessageBox box(QMessageBox::Question, title, message, QMessageBox::Cancel, parent);
    auto* go = box.addButton(action, QMessageBox::AcceptRole);
    box.exec();
    return box.clickedButton() == go;
}

namespace {

QString join(const IntList& values) {
    QStringList parts;
    for (auto v : values) parts << QString::number(v);
    return parts.join(", ");
}

std::optional<std::string> or_null(QLineEdit* field) {
    QString text = field->text().trimmed();
    return text.isEmpty() ? std::nullopt : std::optional(ss(text));
}

} // namespace

CycleDialog::CycleDialog(AppModel& model, Mode mode, std::optional<size_t> source, QWidget* parent)
    : QDialog(parent), model_(model), mode_(mode), source_(source) {
    const Cycle* from = source ? &model_.cycles().at(*source) : nullptr;
    setWindowTitle(mode == Mode::edit ? "Edit cycle" : mode == Mode::clone ? "Clone cycle" : "New cycle");
    setMinimumWidth(520);

    QString id, name, start = QDate::currentDate().toString(Qt::ISODate);
    if (from && mode == Mode::edit) {
        id = qs(from->id);
        name = qs(from->name);
    } else if (from) {
        id = qs(from->id + "-copy");
        name = qs(from->name + " (copy)");
    }
    if (from) {
        start = qs(from->start_date);
        days_.insert(from->training_days.begin(), from->training_days.end());
    } else {
        days_ = {"Tue", "Thu", "Sun"};
    }

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    name_ = new QLineEdit(name);
    name_->setPlaceholderText("8-session hybrid cycle");
    id_ = new QLineEdit(id);
    id_->setPlaceholderText("hybrid-8");
    start_ = new QLineEdit(start);
    start_->setPlaceholderText("YYYY-MM-DD");
    form->addRow("Name", name_);
    form->addRow("Id", id_);
    form->addRow("Start date", start_);
    auto* chips = new QHBoxLayout;
    for (auto day : kWeekdayAbbreviations) {
        std::string d(day);
        auto* chip = new QPushButton(qs(d));
        chip->setCheckable(true);
        chip->setAutoDefault(false);
        chip->setChecked(days_.contains(d));
        connect(chip, &QPushButton::toggled, this, [this, d](bool on) { on ? (void)days_.insert(d) : (void)days_.erase(d); });
        chips->addWidget(chip);
    }
    form->addRow("Training days", chips);
    layout->addLayout(form);
    error_ = new QLabel;
    error_->setStyleSheet("color:#d32f2f;");
    error_->hide();
    layout->addWidget(error_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    buttons->addButton(mode == Mode::edit ? "Apply" : "Create", QDialogButtonBox::AcceptRole);
    if (mode == Mode::edit) {
        auto* remove = buttons->addButton("Remove", QDialogButtonBox::DestructiveRole);
        connect(remove, &QPushButton::clicked, this, [this] {
            const Cycle& cycle = model_.cycles().at(*source_);
            if (!confirm(this, "Remove cycle",
                         QString("Remove \"%1\" and its %2 workouts? Nothing is written until you save cycles.json.")
                             .arg(qs(cycle.name))
                             .arg(cycle.sessions.size()),
                         "Remove"))
                return;
            model_.delete_cycle(*source_);
            reject();
        });
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &CycleDialog::submit);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void CycleDialog::submit() {
    auto fail = [this](const QString& message) {
        error_->setText(message);
        error_->show();
    };
    std::string id = utf8::trim(ss(id_->text()));
    std::string name = utf8::trim(ss(name_->text()));
    std::string start = utf8::trim(ss(start_->text()));
    if (id.empty()) return fail("An id is required.");
    if (name.empty()) return fail("A name is required.");
    if (!is_iso_date_shape(start)) return fail("Start date must be YYYY-MM-DD.");
    if (days_.empty()) return fail("Pick at least one training day.");
    bool clashes = mode_ != Mode::edit || id != model_.cycles().at(*source_).id;
    if (clashes && model_.cycle_id_taken(id)) return fail(QString("The id \"%1\" is already used.").arg(qs(id)));

    // Calendar order, so the generated dates read in the order a week happens.
    std::vector<std::string> ordered;
    for (auto day : kWeekdayAbbreviations)
        if (days_.contains(std::string(day))) ordered.emplace_back(day);

    size_t index;
    if (mode_ == Mode::edit)
        index = *source_;
    else if (mode_ == Mode::clone)
        index = model_.clone_cycle(*source_, id, name);
    else
        index = model_.create_cycle(id, name);
    Cycle& cycle = model_.cycles().at(index);
    cycle.id = id;
    cycle.name = name;
    cycle.start_date = start;
    cycle.training_days = ordered;
    model_.cycle_edited();
    result_ = index;
    accept();
}

std::optional<CycleDay> ask_retitle(QWidget* parent, const std::string& current) {
    CycleDay day = CycleDay::try_parse(current).value_or(CycleDay{});
    QDialog dialog(parent);
    dialog.setWindowTitle("Rename workout");
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("<b>Muscle group</b>"));
    auto* letters = new QHBoxLayout;
    QButtonGroup letter_group;
    for (char c = 'A'; c <= 'F'; ++c) {
        auto* chip = new QPushButton(QString(QChar(c)));
        chip->setCheckable(true);
        chip->setAutoDefault(false);
        chip->setChecked(day.letter == c);
        letter_group.addButton(chip, c);
        letters->addWidget(chip);
    }
    layout->addLayout(letters);
    layout->addWidget(new QLabel("<b>Day type</b>"));
    QButtonGroup kind_group;
    for (auto kind : {DayKind::conditioning, DayKind::heavy}) {
        auto* radio = new QRadioButton(
            QString("%1 — %2").arg(static_cast<int>(kind)).arg(qs(std::string(day_kind_label(kind)))));
        radio->setChecked(day.kind == kind);
        kind_group.addButton(radio, static_cast<int>(kind));
        layout->addWidget(radio);
    }
    auto* becomes = new QLabel;
    layout->addWidget(becomes);
    auto update = [&] {
        day.letter = static_cast<char>(letter_group.checkedId());
        day.kind = static_cast<DayKind>(kind_group.checkedId());
        becomes->setText(qs("Becomes " + day.code()));
    };
    QObject::connect(&letter_group, &QButtonGroup::idClicked, &dialog, update);
    QObject::connect(&kind_group, &QButtonGroup::idClicked, &dialog, update);
    update();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    buttons->addButton("Rename", QDialogButtonBox::AcceptRole);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    return day;
}

std::optional<std::string> ask_block_type(QWidget* parent) {
    bool ok = false;
    QString type = QInputDialog::getItem(parent, "Add block", "Type", {"cardio", "strength", "metcon", "cooldown"}, 1,
                                         false, &ok);
    if (!ok) return std::nullopt;
    return ss(type);
}

BlockDialog::BlockDialog(BlockTemplate& block, QWidget* parent) : QDialog(parent), block_(block) {
    setWindowTitle(qs("Edit " + block.type + " block"));
    setMinimumWidth(520);
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    role_ = new QLineEdit(qs(block.role.value_or("")));
    role_->setPlaceholderText("warmup, main, accessory, grip…");
    form->addRow("Role", role_);

    if (block.type == "cardio") {
        machine_ = new QLineEdit(qs(block.machine.value_or("")));
        duration_ = new QLineEdit(block.duration_min ? QString::number(static_cast<long long>(*block.duration_min)) : "");
        form->addRow("Machine", machine_);
        form->addRow("Duration (min)", duration_);
    } else if (block.type == "strength") {
        sets_reps_ = new QLineEdit(join(block.sets_reps));
        sets_reps_->setPlaceholderText("6, 6, 6, 6, 6");
        form->addRow("Planned reps per set", sets_reps_);
    } else if (block.type == "metcon") {
        format_ = new QComboBox;
        format_->addItem("—", -1);
        for (size_t i = 0; i < enum_count<MetconFormat>(); ++i)
            format_->addItem(qs(metcon_format_wire(static_cast<MetconFormat>(i))), static_cast<int>(i));
        if (block.format) format_->setCurrentIndex(static_cast<int>(*block.format) + 1);
        scheme_ = new QLineEdit(block.scheme ? join(*block.scheme) : "");
        scheme_->setPlaceholderText("21, 15, 9");
        form->addRow("Format", format_);
        form->addRow("Scheme", scheme_);
    }
    layout->addLayout(form);

    if (block.type == "metcon") {
        auto* header = new QHBoxLayout;
        header->addWidget(new QLabel("Movement"), 4);
        header->addWidget(new QLabel("Load"), 3);
        // Blank reps mean "follows the workout's scheme".
        header->addWidget(new QLabel("Reps"), 2);
        header->addSpacing(36);
        layout->addLayout(header);
        movements_layout_ = new QVBoxLayout;
        layout->addLayout(movements_layout_);
        for (const auto& e : block.exercises) add_movement(&e);
        auto* add = new QPushButton("+ Add movement");
        add->setAutoDefault(false);
        connect(add, &QPushButton::clicked, this, [this] { add_movement(nullptr); });
        layout->addWidget(add, 0, Qt::AlignLeft);
    }

    notes_ = new QLineEdit(qs(block.notes.value_or("")));
    auto* notes_form = new QFormLayout;
    notes_form->addRow("Notes", notes_);
    layout->addLayout(notes_form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    buttons->addButton("Apply", QDialogButtonBox::AcceptRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &BlockDialog::apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void BlockDialog::add_movement(const MetconExercise* source) {
    MovementRow m;
    m.row = new QWidget;
    auto* row = new QHBoxLayout(m.row);
    row->setContentsMargins(0, 0, 0, 0);
    m.name = new QLineEdit(source ? qs(source->name) : "");
    m.name->setPlaceholderText("трастери");
    m.load = new QLineEdit(source && source->load ? qs(*source->load) : "");
    m.load->setPlaceholderText("24kg+24kg");
    m.reps = new QLineEdit(source && source->reps_override ? join(*source->reps_override) : "");
    m.reps->setPlaceholderText("scheme");
    m.weight_kg = source ? source->weight_kg : std::nullopt;
    auto* remove = new QPushButton("✕");
    remove->setFixedWidth(30);
    remove->setAutoDefault(false);
    row->addWidget(m.name, 4);
    row->addWidget(m.load, 3);
    row->addWidget(m.reps, 2);
    row->addWidget(remove);
    QWidget* widget = m.row;
    connect(remove, &QPushButton::clicked, this, [this, widget] {
        std::erase_if(movements_, [&](const MovementRow& r) { return r.row == widget; });
        widget->deleteLater();
    });
    movements_layout_->addWidget(m.row);
    movements_.push_back(m);
}

void BlockDialog::apply() {
    block_.role = or_null(role_);
    block_.notes = or_null(notes_);
    if (block_.type == "cardio") {
        block_.machine = ss(machine_->text().trimmed());
        block_.duration_min = dart_double_parse(ss(duration_->text().trimmed()));
    } else if (block_.type == "strength") {
        block_.sets_reps = parse_numbers(sets_reps_->text());
    } else if (block_.type == "metcon") {
        int format = format_->currentData().toInt();
        block_.format = format < 0 ? std::nullopt : std::optional(static_cast<MetconFormat>(format));
        IntList scheme = parse_numbers(scheme_->text());
        block_.scheme = scheme.empty() ? std::nullopt : std::optional(scheme);
        block_.exercises.clear();
        for (const auto& m : movements_) {
            if (m.name->text().trimmed().isEmpty()) continue;
            MetconExercise e;
            e.name = ss(m.name->text().trimmed());
            e.load = or_null(m.load);
            e.weight_kg = m.weight_kg;
            IntList reps = parse_numbers(m.reps->text());
            if (!reps.empty()) e.reps_override = reps;
            block_.exercises.push_back(std::move(e));
        }
    }
    accept();
}
