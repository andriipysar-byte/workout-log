#include "main_window.hpp"

#include <QButtonGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

#include "app_model.hpp"
#include "cycle_view.hpp"
#include "new_session_dialog.hpp"
#include "plan_view.hpp"
#include "run_view.hpp"
#include "session_editor.hpp"
#include "theme.hpp"
#include "workoutlog/directory_storage.hpp"

namespace {

constexpr const char* kLastFolderKey = "workout_log.folder";

} // namespace

MainWindow::MainWindow(AppModel& model, QWidget* parent) : QMainWindow(parent), model_(model) {
    setWindowTitle("WorkoutLog");
    resize(1280, 820);

    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto* folder = toolbar->addAction("Choose folder…", this, &MainWindow::choose_folder);
    folder->setToolTip("Choose session folder");
    toolbar->addAction("Import…", this, &MainWindow::import_files)->setToolTip("Import session files");
    toolbar->addAction("Export…", this, &MainWindow::export_files)->setToolTip("Export all sessions");
    toolbar->addAction("Reload", this, [this] { model_.refresh(true); })->setToolTip("Reload folder");
    toolbar->addSeparator();
    delete_ = toolbar->addAction("Delete", this, &MainWindow::delete_session);
    delete_->setToolTip("Delete session");
    save_ = toolbar->addAction("Save", this, [this] {
        if (model_.session()) model_.save();
    });
    save_->setShortcut(QKeySequence::Save);
    save_->setToolTip("Save (" + QKeySequence(QKeySequence::Save).toString(QKeySequence::NativeText) + ")");

    auto* splitter = new QSplitter;
    auto* sidebar = new QWidget;
    sidebar->setMinimumWidth(theme::kSidebarWidth);
    sidebar->setMaximumWidth(theme::kSidebarWidth * 2);
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(8, 8, 8, 8);
    auto* top = new QHBoxLayout;
    tabs_ = new QButtonGroup(this);
    auto* segments = new QHBoxLayout;
    segments->setSpacing(0);
    int id = 0;
    for (const char* label : {"List", "Cycle", "Plan", "Run"}) {
        auto* b = new QPushButton(label);
        b->setCheckable(true);
        tabs_->addButton(b, id++);
        segments->addWidget(b);
    }
    tabs_->button(0)->setChecked(true);
    top->addLayout(segments, 1);
    auto* add = new QPushButton("＋");
    add->setToolTip("New session");
    add->setFixedWidth(32);
    top->addWidget(add);
    side->addLayout(top);
    list_ = new QListWidget;
    empty_list_ = new QLabel;
    empty_list_->setAlignment(Qt::AlignCenter);
    empty_list_->setWordWrap(true);
    side->addWidget(list_, 1);
    side->addWidget(empty_list_, 1);
    splitter->addWidget(sidebar);

    detail_ = new QStackedWidget;
    placeholder_ = new QLabel("Select a session to edit");
    static_cast<QLabel*>(placeholder_)->setAlignment(Qt::AlignCenter);
    cycle_ = new CycleView(model_);
    plan_ = new PlanView(model_);
    run_ = new RunView(model_);
    detail_->addWidget(placeholder_);
    detail_->addWidget(cycle_);
    detail_->addWidget(plan_);
    detail_->addWidget(run_);
    splitter->addWidget(detail_);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    status_ = new QLabel;
    statusBar()->addWidget(status_, 1);

    connect(tabs_, &QButtonGroup::idClicked, this, [this](int i) { set_tab(static_cast<Tab>(i)); });
    connect(add, &QPushButton::clicked, this, &MainWindow::new_session);
    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        model_.open(ss(item->data(Qt::UserRole).toString()));
    });
    connect(&model_, &AppModel::changed, this, &MainWindow::sync);
    sync();
}

void MainWindow::set_tab(Tab tab) {
    tab_ = tab;
    tabs_->button(static_cast<int>(tab))->setChecked(true);
    sync();
}

// One sync for every model change. The session editor is rebuilt only when a
// different session is loaded — rebuilding on every keystroke would steal focus.
void MainWindow::sync() {
    const auto& files = model_.files();
    {
        QSignalBlocker block(list_);
        list_->clear();
        for (const auto& id : files) {
            QString label = qs(id.ends_with(".json") ? id.substr(0, id.size() - 5) : id);
            auto* item = new QListWidgetItem(label);
            item->setData(Qt::UserRole, qs(id));
            list_->addItem(item);
            if (model_.selection() == id) item->setSelected(true), list_->setCurrentItem(item);
        }
    }
    list_->setVisible(!files.empty());
    empty_list_->setVisible(files.empty());
    empty_list_->setText(model_.ready() ? "No sessions here yet." : "Opening folder…");

    const void* session = model_.session();
    if (editor_ && (editor_generation_ != model_.session_generation() || !session)) {
        detail_->removeWidget(editor_);
        editor_->deleteLater();
        editor_ = nullptr;
    }
    if (!editor_ && session) {
        editor_ = new SessionEditor(model_);
        editor_generation_ = model_.session_generation();
        detail_->addWidget(editor_);
    } else if (editor_) {
        editor_->refresh_map();
    }

    switch (tab_) {
        case Tab::list: detail_->setCurrentWidget(editor_ ? static_cast<QWidget*>(editor_) : placeholder_); break;
        case Tab::cycle:
            cycle_->refresh();
            detail_->setCurrentWidget(cycle_);
            break;
        case Tab::plan:
            plan_->refresh();
            detail_->setCurrentWidget(plan_);
            break;
        case Tab::run:
            run_->refresh();
            detail_->setCurrentWidget(run_);
            break;
    }

    bool not_plan = tab_ != Tab::plan && tab_ != Tab::run;
    save_->setEnabled(session && not_plan);
    delete_->setEnabled(model_.selection().has_value() && not_plan);
    status_->setText(qs(model_.status().empty() ? model_.folder_label() : model_.status()));
    const char* titles[] = {"Sessions — WorkoutLog", "Cycle — WorkoutLog", "Plan — WorkoutLog", "Run — WorkoutLog"};
    setWindowTitle(titles[static_cast<int>(tab_)]);
}

void MainWindow::new_session() {
    NewSessionDialog dialog(model_, this);
    if (dialog.exec() != QDialog::Accepted || !dialog.created()) return;
    model_.create(*dialog.created());
    set_tab(Tab::list);
}

void MainWindow::delete_session() {
    auto id = model_.selection();
    if (!id) return;
    QMessageBox box(QMessageBox::Warning, "Delete session", qs("Delete " + *id + "? This removes the file."),
                    QMessageBox::Cancel, this);
    auto* go = box.addButton("Delete", QMessageBox::DestructiveRole);
    box.exec();
    if (box.clickedButton() == go) model_.remove(*id);
}

void MainWindow::choose_folder() {
    QString path = QFileDialog::getExistingDirectory(this, "Choose session folder", qs(model_.folder_label()));
    if (path.isEmpty()) return;
    QSettings().setValue(kLastFolderKey, path);
    model_.open_folder(folder_at(ss(path)));
}

void MainWindow::import_files() {
    QStringList paths = QFileDialog::getOpenFileNames(this, "Import session files", {}, "Sessions (*.json)");
    std::vector<AppModel::ImportFile> files;
    for (const auto& path : paths)
        files.push_back({ss(QFileInfo(path).fileName()), wl::read_file(ss(path))});
    model_.import_sessions(files);
}

void MainWindow::export_files() {
    if (model_.files().empty()) {
        model_.export_sessions("");
        return;
    }
    QString dir = QFileDialog::getExistingDirectory(this, "Export sessions to");
    if (!dir.isEmpty()) model_.export_sessions(ss(dir));
}
