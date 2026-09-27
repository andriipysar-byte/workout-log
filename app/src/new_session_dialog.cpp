#include "new_session_dialog.hpp"

#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "app_model.hpp"
#include "theme.hpp"
#include "workoutlog/cycle_planning.hpp"
#include "workoutlog/date.hpp"
#include "workoutlog/session_store.hpp"

using namespace wl;

NewSessionDialog::NewSessionDialog(AppModel& model, QWidget* parent, Confirm confirm_overwrite)
    : QDialog(parent), model_(model), confirm_(std::move(confirm_overwrite)) {
    setWindowTitle("New session");
    setMinimumWidth(420);
    if (!confirm_)
        confirm_ = [this](const QString& id) {
            return QMessageBox::question(this, "Session already exists",
                                         id + " is already in this folder. Overwrite it?") == QMessageBox::Yes;
        };

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    if (!model_.cycles().empty()) {
        cycle_ = new QComboBox;
        for (const auto& c : model_.cycles()) cycle_->addItem(qs(cycle_label(c)));
        template_ = new QComboBox;
        form->addRow("Cycle", cycle_);
        form->addRow("Template", template_);
        connect(cycle_, &QComboBox::currentIndexChanged, this, [this] { fill_templates(); });
        connect(template_, &QComboBox::activated, this, &NewSessionDialog::select_template);
        fill_templates();
    }
    date_ = new QLineEdit(QDate::currentDate().toString(Qt::ISODate));
    date_->setPlaceholderText("YYYY-MM-DD");
    day_ = new QLineEdit;
    day_->setPlaceholderText("A1");
    form->addRow("Date", date_);
    form->addRow("Cycle day", day_);
    layout->addLayout(form);

    error_ = new QLabel;
    error_->setStyleSheet("color:#d32f2f;");
    error_->hide();
    layout->addWidget(error_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    buttons->addButton("Create", QDialogButtonBox::AcceptRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &NewSessionDialog::create);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void NewSessionDialog::fill_templates() {
    template_->clear();
    template_->addItem("Blank session");
    int index = cycle_->currentIndex();
    if (index < 0) return;
    for (const auto& t : model_.cycles()[static_cast<size_t>(index)].sessions)
        template_->addItem(qs(t.cycle_day + " · " + t.title.value_or("session")));
}

// Picking a template also moves the date and cycle day to that template's
// scheduled slot — the answer wanted nine times in ten.
void NewSessionDialog::select_template(int index) {
    error_->hide();
    if (index <= 0) return;
    const Cycle& cycle = model_.cycles()[static_cast<size_t>(cycle_->currentIndex())];
    const CycleSession& tmpl = cycle.sessions[static_cast<size_t>(index - 1)];
    day_->setText(qs(tmpl.cycle_day));
    if (auto date = model_.planned_date(cycle, static_cast<size_t>(index - 1))) date_->setText(qs(iso_date(*date)));
}

void NewSessionDialog::create() {
    auto fail = [this](const QString& message) {
        error_->setText(message);
        error_->show();
    };
    std::string date = ss(date_->text().trimmed());
    std::string cycle_day = ss(day_->text().trimmed());
    if (!is_iso_date_shape(date)) return fail("Date must be YYYY-MM-DD.");
    if (cycle_day.empty()) return fail("Cycle day is required.");

    Session session;
    int picked = template_ ? template_->currentIndex() : 0;
    if (picked <= 0) {
        session.date = date;
        session.cycle_day = cycle_day;
    } else {
        const Cycle& cycle = model_.cycles()[static_cast<size_t>(cycle_->currentIndex())];
        try {
            auto when = parse_date(date);
            if (!when) return fail("Date must be YYYY-MM-DD.");
            session = session_from_template(cycle.sessions[static_cast<size_t>(picked - 1)], *when, false);
            session.cycle_day = cycle_day;
        } catch (const std::exception& e) {
            return fail(e.what());
        }
    }

    std::string id = SessionStore::id_for(session);
    if (model_.exists(id) && !confirm_(qs(id))) return;
    created_ = std::move(session);
    accept();
}
