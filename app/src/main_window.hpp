#pragma once

#include <QMainWindow>

#include <optional>
#include <string>

class AppModel;
class CycleView;
class PlanView;
class SessionEditor;
class QAction;
class QButtonGroup;
class QLabel;
class QListWidget;
class QStackedWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    enum class Tab { list, cycle, plan };

    explicit MainWindow(AppModel& model, QWidget* parent = nullptr);

    Tab tab() const { return tab_; }
    void set_tab(Tab tab);
    QListWidget* session_list() const { return list_; }
    SessionEditor* editor() const { return editor_; }
    PlanView* plan_view() const { return plan_; }
    QAction* save_action() const { return save_; }
    QAction* delete_action() const { return delete_; }

    void new_session();
    void delete_session();

private:
    void sync();
    void choose_folder();
    void import_files();
    void export_files();

    AppModel& model_;
    Tab tab_ = Tab::list;
    QButtonGroup* tabs_;
    QListWidget* list_;
    QLabel* empty_list_;
    QStackedWidget* detail_;
    QWidget* placeholder_;
    SessionEditor* editor_ = nullptr;
    unsigned editor_generation_ = 0;
    CycleView* cycle_;
    PlanView* plan_;
    QLabel* status_;
    QAction* save_;
    QAction* delete_;
};
