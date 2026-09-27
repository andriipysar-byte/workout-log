#pragma once

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>

#include <functional>
#include <optional>

#include "workoutlog/models.hpp"

class AppModel;

// Create a session: blank, or expanded from a cycles.json template. `result()`
// is empty when the user backed out, including declining an overwrite.
class NewSessionDialog : public QDialog {
    Q_OBJECT

public:
    using Confirm = std::function<bool(const QString& id)>;

    NewSessionDialog(AppModel& model, QWidget* parent = nullptr, Confirm confirm_overwrite = {});

    const std::optional<wl::Session>& created() const { return created_; }
    QComboBox* cycle_box() const { return cycle_; }
    QComboBox* template_box() const { return template_; }
    QLineEdit* date_field() const { return date_; }
    QLineEdit* day_field() const { return day_; }
    QLabel* error_label() const { return error_; }
    void create();

private:
    void fill_templates();
    void select_template(int index);

    AppModel& model_;
    Confirm confirm_;
    QComboBox* cycle_ = nullptr;
    QComboBox* template_ = nullptr;
    QLineEdit* date_;
    QLineEdit* day_;
    QLabel* error_;
    std::optional<wl::Session> created_;
};
