#include "widgets.hpp"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include <cmath>

#include "app_model.hpp"
#include "theme.hpp"
#include "workoutlog/dart_parse.hpp"
#include "workoutlog/json.hpp"

OptionalField::OptionalField(const QString& placeholder, const std::optional<std::string>& value,
                             std::function<void(std::optional<std::string>)> on_changed, int width, QWidget* parent)
    : QLineEdit(parent) {
    setPlaceholderText(placeholder);
    setText(value ? qs(*value) : QString());
    if (width > 0) setFixedWidth(width);
    connect(this, &QLineEdit::textEdited, this, [on_changed](const QString& text) {
        on_changed(text.isEmpty() ? std::nullopt : std::optional<std::string>(ss(text)));
    });
}

QString NumberField::format(double value) {
    return qs(value == std::round(value) ? std::to_string(static_cast<long long>(value)) : wl::dart_double(value));
}

NumberField::NumberField(const QString& placeholder, std::optional<double> value,
                         std::function<void(std::optional<double>)> on_changed, int width, QWidget* parent)
    : OptionalField(
          placeholder, value ? std::optional<std::string>(ss(format(*value))) : std::nullopt,
          [on_changed](std::optional<std::string> text) {
              on_changed(text ? wl::dart_double_parse(*text) : std::nullopt);
          },
          width, parent) {}

WeightingModePicker::WeightingModePicker(AppModel& model, QWidget* parent) : QWidget(parent), model_(model) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setToolTip("How each exercise is weighted into the map");
    for (auto mode : wl::kWeightingModes) {
        auto* button = new QPushButton(qs(std::string(wl::weighting_label(mode))));
        button->setCheckable(true);
        button->setAutoDefault(false);
        group_.addButton(button, static_cast<int>(mode));
        layout->addWidget(button);
    }
    group_.setExclusive(true);
    connect(&group_, &QButtonGroup::idClicked, this,
            [this](int id) { model_.set_mode(static_cast<wl::WeightingMode>(id)); });
    sync();
}

void WeightingModePicker::sync() {
    if (auto* button = group_.button(static_cast<int>(model_.mode()))) button->setChecked(true);
}

MuscleGroupLegend::MuscleGroupLegend(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 4);
    for (auto group : wl::kMuscleGroups) {
        auto* swatch = new QLabel;
        swatch->setFixedSize(10, 10);
        swatch->setStyleSheet(QString("background:%1;border-radius:2px;").arg(theme::group_color(group).name()));
        auto* label = new QLabel(qs(wl::group_label(group)));
        label->setStyleSheet(small_style());
        layout->addWidget(swatch);
        layout->addWidget(label);
        layout->addSpacing(6);
    }
    layout->addStretch();
}

QFont monospace_font() {
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

QString small_style() {
    return "font-size: 11px;";
}
