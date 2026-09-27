#include "exercise_picker.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "app_model.hpp"
#include "theme.hpp"
#include "workoutlog/utf8.hpp"

using namespace wl;

namespace {

QIcon dot(const std::vector<MuscleGroup>& groups) {
    QPixmap pixmap(12, 12);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(groups.empty() ? theme::outline() : theme::group_color(groups.front()));
    painter.drawEllipse(0, 0, 12, 12);
    return QIcon(pixmap);
}

bool contains(const std::string& haystack, const std::string& needle) {
    return utf8::to_lower(haystack).find(needle) != std::string::npos;
}

} // namespace

ExercisePicker::ExercisePicker(AppModel& model, const std::optional<std::string>& current, QWidget* parent)
    : QDialog(parent), model_(model), current_(current) {
    setWindowTitle("Choose exercise");
    resize(460, 460);
    auto* layout = new QVBoxLayout(this);
    query_ = new QLineEdit;
    query_->setPlaceholderText("search name or alias");
    list_ = new QListWidget;
    list_->setIconSize(QSize(12, 12));
    layout->addWidget(query_);
    layout->addWidget(list_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto* add = buttons->addButton("New exercise", QDialogButtonBox::ActionRole);
    add->setEnabled(model_.can_edit_plan());
    layout->addWidget(buttons);

    connect(query_, &QLineEdit::textChanged, this, &ExercisePicker::filter);
    connect(list_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        chosen_ = ss(item->data(Qt::UserRole).toString());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(add, &QPushButton::clicked, this, [this] {
        NewExerciseDialog dialog(model_, query_->text().trimmed(), this);
        if (dialog.exec() == QDialog::Accepted && dialog.added()) {
            chosen_ = dialog.added();
            accept();
        }
    });
    filter();
}

void ExercisePicker::filter() {
    list_->clear();
    const Catalogue* catalogue = model_.catalogue();
    std::string query = utf8::to_lower(utf8::trim(ss(query_->text())));
    if (!catalogue) return;
    for (const auto& e : catalogue->exercises) {
        bool match = query.empty() || contains(e.name, query) ||
                     std::any_of(e.aliases.begin(), e.aliases.end(), [&](const auto& a) { return contains(a, query); });
        if (!match) continue;
        QStringList muscles;
        for (const auto& m : e.primary_muscles) muscles << qs(m);
        auto* item = new QListWidgetItem(dot(model_.primary_groups(e.name)), qs(e.name) + "\n" + muscles.join(", "));
        item->setData(Qt::UserRole, qs(e.name));
        list_->addItem(item);
        if (current_ == e.name) list_->setCurrentItem(item);
    }
    if (list_->count() == 0) {
        auto* item = new QListWidgetItem("Nothing matches — use “New exercise”.");
        item->setFlags(Qt::NoItemFlags);
        list_->addItem(item);
    }
}

template <typename E>
static QComboBox* enum_combo(bool with_none) {
    auto* box = new QComboBox;
    if (with_none) box->addItem("—", -1);
    for (size_t i = 0; i < enum_count<E>(); ++i)
        box->addItem(qs(std::string(name(static_cast<E>(i)))), static_cast<int>(i));
    return box;
}

NewExerciseDialog::NewExerciseDialog(AppModel& model, const QString& suggested_name, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle("New exercise");
    setMinimumWidth(520);
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    name_ = new QLineEdit(suggested_name);
    name_->setPlaceholderText("присід фронтальний");
    aliases_ = new QLineEdit;
    aliases_->setPlaceholderText("comma separated, optional");
    category_ = enum_combo<ExerciseCategory>(false);
    pattern_ = enum_combo<MovementPattern>(true);
    modality_ = enum_combo<Modality>(true);
    form->addRow("Canonical name", name_);
    form->addRow("Aliases", aliases_);
    auto* enums = new QHBoxLayout;
    enums->addWidget(category_);
    enums->addWidget(pattern_);
    enums->addWidget(modality_);
    form->addRow("Category · pattern · modality", enums);
    layout->addLayout(form);
    layout->addWidget(muscle_picker("Primary muscles", primary_));
    layout->addWidget(muscle_picker("Secondary muscles", secondary_));
    error_ = new QLabel;
    error_->setStyleSheet("color:#d32f2f;");
    error_->hide();
    layout->addWidget(error_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    buttons->addButton("Add", QDialogButtonBox::AcceptRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &NewExerciseDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void NewExerciseDialog::toggle(std::set<std::string>& set, const std::string& muscle) {
    if (!set.erase(muscle)) set.insert(muscle);
}

QWidget* NewExerciseDialog::muscle_picker(const QString& title, std::set<std::string>& selected) {
    auto* box = new QWidget;
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel("<b>" + title + "</b>"));
    auto* grid = new QGridLayout;
    grid->setSpacing(4);
    const Catalogue* catalogue = model_.catalogue();
    std::vector<std::string> muscles = catalogue ? catalogue->known_muscles() : std::vector<std::string>{};
    for (size_t i = 0; i < muscles.size(); ++i) {
        const std::string muscle = muscles[i];
        auto* chip = new QPushButton(qs(muscle));
        chip->setCheckable(true);
        chip->setAutoDefault(false);
        if (auto group = group_of(muscle)) {
            QColor c = theme::group_color(*group);
            c.setAlphaF(0.35f);
            chip->setStyleSheet(QString("QPushButton{font-size:11px;padding:2px 6px;}"
                                        "QPushButton:checked{background:%1;}")
                                    .arg(c.name(QColor::HexArgb)));
        }
        connect(chip, &QPushButton::toggled, this, [&selected, muscle](bool) { toggle(selected, muscle); });
        grid->addWidget(chip, static_cast<int>(i / 5), static_cast<int>(i % 5));
    }
    layout->addLayout(grid);
    return box;
}

void NewExerciseDialog::save() {
    auto fail = [this](const QString& message) {
        error_->setText(message);
        error_->show();
    };
    std::string name = utf8::trim(ss(name_->text()));
    if (name.empty()) return fail("A name is required.");
    if (model_.catalogue() && model_.catalogue()->resolve(name))
        return fail(QString("\"%1\" already resolves to an exercise.").arg(qs(name)));
    if (primary_.empty()) return fail("Pick at least one primary muscle.");

    Exercise exercise;
    exercise.name = name;
    for (const auto& alias : aliases_->text().split(',')) {
        QString trimmed = alias.trimmed();
        if (!trimmed.isEmpty()) exercise.aliases.push_back(ss(trimmed));
    }
    exercise.category = static_cast<ExerciseCategory>(category_->currentData().toInt());
    if (int p = pattern_->currentData().toInt(); p >= 0) exercise.pattern = static_cast<MovementPattern>(p);
    if (int m = modality_->currentData().toInt(); m >= 0) exercise.modality = static_cast<Modality>(m);
    exercise.primary_muscles.assign(primary_.begin(), primary_.end());
    exercise.secondary_muscles.assign(secondary_.begin(), secondary_.end());
    model_.add_exercise(std::move(exercise));
    added_ = name;
    accept();
}
