#pragma once

#include <QWidget>

#include <functional>
#include <optional>
#include <string>
#include <vector>

class AppModel;
class QComboBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

// The step between planning and generation: one run of a cycle, every day side
// by side with its reps, rounds and weights, so the whole run can be read and
// balanced before a single session file exists.
class RunView : public QWidget {
    Q_OBJECT

public:
    // Asked before a generated day overwrites a session file; receives the ids.
    using Confirm = std::function<bool(const std::vector<std::string>&)>;

    explicit RunView(AppModel& model, QWidget* parent = nullptr, Confirm confirm_overwrite = {});
    void refresh();

    std::optional<size_t> selected() const;
    void select(std::optional<size_t> index);
    void generate(std::vector<size_t> days);
    QComboBox* run_box() const { return run_box_; }
    QPushButton* save_button() const { return save_; }
    QPushButton* generate_all_button() const { return generate_all_; }

private:
    void rebuild_detail();
    QWidget* day_card(size_t run, size_t day);
    QWidget* block_row(size_t run, size_t day, size_t block);
    void edited(size_t run);
    void update_totals();

    AppModel& model_;
    Confirm confirm_;
    std::string selected_file_;
    QComboBox* run_box_;
    QPushButton* new_;
    QPushButton* save_;
    QPushButton* generate_all_;
    QVBoxLayout* detail_;
    QWidget* detail_host_ = nullptr;
    QScrollArea* columns_ = nullptr;
    int scroll_x_ = 0;
    QLabel* run_totals_ = nullptr;
    std::vector<QLabel*> day_totals_;
};
