#pragma once

#include <QWidget>

#include <optional>
#include <set>
#include <string>

class AppModel;
class QComboBox;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QLabel;

// Cycle planning: the template side of the app, editing cycles.json rather
// than a session file. Workouts run left to right, one column each, the way a
// cycle reads on paper.
class PlanView : public QWidget {
    Q_OBJECT

public:
    explicit PlanView(AppModel& model, QWidget* parent = nullptr);
    void refresh();

    std::optional<size_t> selected() const;
    QComboBox* cycle_box() const { return cycle_box_; }
    QPushButton* new_button() const { return new_; }
    QPushButton* save_button() const { return save_; }
    QPushButton* clone_button() const { return clone_; }
    QPushButton* edit_button() const { return edit_; }

private:
    void select(std::optional<size_t> index);
    void rebuild_detail();
    QWidget* workout_card(size_t cycle, size_t index);
    QWidget* block_row(size_t cycle, size_t workout, size_t block, bool enabled);
    QWidget* total_column(size_t cycle);

    AppModel& model_;
    std::string selected_id_;
    QComboBox* cycle_box_;
    QPushButton* new_;
    QPushButton* clone_;
    QPushButton* edit_;
    QPushButton* save_;
    QLabel* lock_;
    QVBoxLayout* detail_;
    QWidget* detail_host_ = nullptr;
    QScrollArea* columns_ = nullptr;
    int scroll_x_ = 0;
    // Cycle-day codes whose map is collapsed; survives rebuilds.
    std::set<std::string> hidden_maps_;
};
