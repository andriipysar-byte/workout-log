#pragma once

#include <QDialog>

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "workoutlog/cycle.hpp"
#include "workoutlog/cycle_planning.hpp"

class AppModel;
class QLabel;
class QLineEdit;
class QComboBox;
class QVBoxLayout;

// Create a cycle, clone one, or edit an existing one's header. `result_index()`
// is the cycle created or edited; empty after Cancel or Remove.
class CycleDialog : public QDialog {
    Q_OBJECT

public:
    enum class Mode { create, clone, edit };

    CycleDialog(AppModel& model, Mode mode, std::optional<size_t> source = std::nullopt, QWidget* parent = nullptr);

    std::optional<size_t> result_index() const { return result_; }
    QLineEdit* id_field() const { return id_; }
    QLineEdit* name_field() const { return name_; }
    QLabel* error_label() const { return error_; }
    void submit();

private:
    AppModel& model_;
    Mode mode_;
    std::optional<size_t> source_;
    QLineEdit* id_;
    QLineEdit* name_;
    QLineEdit* start_;
    QLabel* error_;
    std::set<std::string> days_;
    std::optional<size_t> result_;
};

// Letter and number are picked rather than typed, which keeps the A1/A2
// convention intact across a cycle.
std::optional<wl::CycleDay> ask_retitle(QWidget* parent, const std::string& current);

std::optional<std::string> ask_block_type(QWidget* parent);

bool confirm(QWidget* parent, const QString& title, const QString& message, const QString& action);

// The fields of one planned block; which exist depends on the block type.
class BlockDialog : public QDialog {
    Q_OBJECT

public:
    BlockDialog(wl::BlockTemplate& block, QWidget* parent = nullptr);
    void apply();

private:
    struct MovementRow {
        QWidget* row;
        QLineEdit* name;
        QLineEdit* load;
        QLineEdit* reps;
        // Carried through untouched: the numeric load tonnage reads, which this
        // dialog does not show. Rebuilding movements from names alone dropped it.
        std::optional<double> weight_kg;
    };

    void add_movement(const wl::MetconExercise* source);

    wl::BlockTemplate& block_;
    QLineEdit* role_;
    QLineEdit* notes_;
    QLineEdit* machine_ = nullptr;
    QLineEdit* duration_ = nullptr;
    QLineEdit* sets_reps_ = nullptr;
    QComboBox* format_ = nullptr;
    QLineEdit* scheme_ = nullptr;
    QVBoxLayout* movements_layout_ = nullptr;
    std::vector<MovementRow> movements_;
};

// `6, 6 6+6` → [6, 6, 6, 6]: split on commas, whitespace and plus signs.
wl::IntList parse_numbers(const QString& text);
