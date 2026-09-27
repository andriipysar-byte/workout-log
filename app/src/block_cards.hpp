#pragma once

#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include <functional>

#include "workoutlog/models.hpp"

class AppModel;
class MuscleMapWidget;

// Each card edits its block in place and calls `on_changed`, mirroring the
// session being a mutable document until Save.
QFrame* make_block_card(wl::Block& block, AppModel& model, std::function<void()> on_changed);

// The whiteboard view of a metcon: one row per movement, one column per round.
// A movement with its own reps_override prints that instead of the scheme,
// which is why rounds are columns rather than one shared line.
QWidget* make_metcon_table(const std::optional<wl::MetconFormat>& format, const std::optional<wl::IntList>& scheme,
                           const std::vector<wl::MetconExercise>& exercises, bool show_header = true);

class StrengthEditor : public QWidget {
    Q_OBJECT

public:
    StrengthEditor(wl::StrengthBlock& block, AppModel& model, std::function<void()> on_changed,
                   QWidget* parent = nullptr);

    QLineEdit* notation() const { return notation_; }
    QPushButton* apply_button() const { return apply_; }
    QLabel* preview() const { return preview_; }
    QLabel* summary() const { return summary_; }

private:
    void update_preview();
    void apply();
    void toggle_map(bool open);

    wl::StrengthBlock& block_;
    AppModel& model_;
    std::function<void()> on_changed_;
    QLabel* summary_;
    QLineEdit* notation_;
    QPushButton* apply_;
    QLabel* preview_;
    QWidget* warnings_;
    MuscleMapWidget* map_ = nullptr;
    QWidget* map_holder_;
};
