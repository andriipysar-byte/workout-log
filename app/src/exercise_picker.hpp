#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>

#include <optional>
#include <set>
#include <string>

class AppModel;
class QComboBox;
class QLabel;

// Choose an exercise from the catalogue, or add one. Yields the canonical name,
// so a plan always stores something the muscle map can resolve.
class ExercisePicker : public QDialog {
    Q_OBJECT

public:
    ExercisePicker(AppModel& model, const std::optional<std::string>& current, QWidget* parent = nullptr);

    const std::optional<std::string>& chosen() const { return chosen_; }
    QLineEdit* query() const { return query_; }
    QListWidget* list() const { return list_; }

private:
    void filter();

    AppModel& model_;
    std::optional<std::string> current_;
    QLineEdit* query_;
    QListWidget* list_;
    std::optional<std::string> chosen_;
};

// Adds an exercise to exercises.json. Muscles are required: without them it
// would resolve but contribute nothing to any muscle map.
class NewExerciseDialog : public QDialog {
    Q_OBJECT

public:
    NewExerciseDialog(AppModel& model, const QString& suggested_name, QWidget* parent = nullptr);

    const std::optional<std::string>& added() const { return added_; }
    void save();

    QLineEdit* name_field() const { return name_; }
    void toggle_primary(const std::string& muscle) { toggle(primary_, muscle); }
    QLabel* error_label() const { return error_; }

private:
    QWidget* muscle_picker(const QString& title, std::set<std::string>& selected);
    static void toggle(std::set<std::string>& set, const std::string& muscle);

    AppModel& model_;
    QLineEdit* name_;
    QLineEdit* aliases_;
    QComboBox* category_;
    QComboBox* pattern_;
    QComboBox* modality_;
    QLabel* error_;
    std::set<std::string> primary_;
    std::set<std::string> secondary_;
    std::optional<std::string> added_;
};
