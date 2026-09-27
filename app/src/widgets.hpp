#pragma once

#include <QButtonGroup>
#include <QLineEdit>
#include <QWidget>

#include <functional>
#include <optional>
#include <string>

#include "workoutlog/muscles.hpp"

class AppModel;

// A text field over a `std::optional<std::string>`: empty text maps to nullopt,
// so the key stays out of the file rather than being written as "".
class OptionalField : public QLineEdit {
    Q_OBJECT

public:
    OptionalField(const QString& placeholder, const std::optional<std::string>& value,
                  std::function<void(std::optional<std::string>)> on_changed, int width = 0,
                  QWidget* parent = nullptr);
};

class NumberField : public OptionalField {
    Q_OBJECT

public:
    NumberField(const QString& placeholder, std::optional<double> value,
                std::function<void(std::optional<double>)> on_changed, int width = 0, QWidget* parent = nullptr);

    // Integral values without a `.0`, as the files spell them.
    static QString format(double value);
};

// Segmented Sets / Reps / Tonnage switch bound to the model's weighting mode.
class WeightingModePicker : public QWidget {
    Q_OBJECT

public:
    explicit WeightingModePicker(AppModel& model, QWidget* parent = nullptr);
    void sync();

private:
    AppModel& model_;
    QButtonGroup group_;
};

class MuscleGroupLegend : public QWidget {
    Q_OBJECT

public:
    explicit MuscleGroupLegend(QWidget* parent = nullptr);
};

QFont monospace_font();
QString small_style();
