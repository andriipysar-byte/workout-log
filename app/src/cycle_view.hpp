#pragma once

#include <QDate>
#include <QTableWidget>
#include <QWidget>

class AppModel;
class MuscleMapWidget;
class WeightingModePicker;
class QLabel;
class QStackedLayout;
struct CycleMatrix;

class CycleTable : public QTableWidget {
    Q_OBJECT

public:
    explicit CycleTable(QWidget* parent = nullptr);
    void set_matrix(const CycleMatrix& matrix);
};

// Month grid that opens on the month of the latest session; a day with a
// session shows its cycle-day badge in the day's dominant group colour.
class CalendarView : public QWidget {
    Q_OBJECT

public:
    explicit CalendarView(AppModel& model, QWidget* parent = nullptr);
    void refresh();
    QDate anchor() const { return anchor_; }
    void shift_month(int delta);

private:
    class Grid;
    AppModel& model_;
    QDate anchor_;
    // The month is chosen from the data once there is some; until the folder
    // loads, the calendar has nothing to open on.
    bool anchored_to_data_ = false;
    QLabel* title_;
    Grid* grid_;
};

class CycleView : public QWidget {
    Q_OBJECT

public:
    explicit CycleView(AppModel& model, QWidget* parent = nullptr);
    void refresh();
    CycleTable* table() const { return table_; }

private:
    AppModel& model_;
    CycleTable* table_;
    QLabel* empty_;
    QStackedLayout* table_stack_;
    MuscleMapWidget* map_;
    WeightingModePicker* picker_;
    CalendarView* calendar_;
};
