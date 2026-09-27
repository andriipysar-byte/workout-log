#pragma once

#include <QScrollArea>

class AppModel;
class MuscleMapWidget;
class WeightingModePicker;

class SessionEditor : public QScrollArea {
    Q_OBJECT

public:
    explicit SessionEditor(AppModel& model, QWidget* parent = nullptr);

    // Re-renders what derives from the session (its map) without rebuilding the
    // fields, which would steal focus mid-keystroke.
    void refresh_map();

private:
    AppModel& model_;
    MuscleMapWidget* map_;
    WeightingModePicker* picker_;
};
