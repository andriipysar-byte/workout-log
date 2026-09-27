#include "cycle_view.hpp"

#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedLayout>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>

#include "app_model.hpp"
#include "muscle_map_widget.hpp"
#include "theme.hpp"
#include "widgets.hpp"

using namespace wl;

namespace {

constexpr int kGroupsRole = Qt::UserRole + 1;
constexpr int kPresentRole = Qt::UserRole + 2;

std::vector<MuscleGroup> groups_of(const QModelIndex& index) {
    std::vector<MuscleGroup> out;
    for (const auto& v : index.data(kGroupsRole).toList()) out.push_back(static_cast<MuscleGroup>(v.toInt()));
    return out;
}

class CellDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        auto groups = groups_of(index);
        bool is_name = index.column() == 0;
        bool present = index.data(kPresentRole).toBool();
        painter->save();
        if (is_name || present)
            painter->fillRect(option.rect, theme::group_gradient(groups, is_name ? 0.22 : 0.16, option.rect));
        painter->restore();
        if (is_name) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        if (!present) return;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(groups.empty() ? theme::kUnclassified : theme::group_color(groups.front()));
        painter->drawEllipse(QRectF(option.rect).center(), 4.5, 4.5);
        painter->restore();
    }
};

} // namespace

CycleTable::CycleTable(QWidget* parent) : QTableWidget(parent) {
    setItemDelegate(new CellDelegate(this));
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::NoSelection);
    setShowGrid(false);
    verticalHeader()->hide();
    verticalHeader()->setDefaultSectionSize(33);
    setTextElideMode(Qt::ElideRight);
    setFocusPolicy(Qt::NoFocus);
}

void CycleTable::set_matrix(const CycleMatrix& matrix) {
    clear();
    setRowCount(static_cast<int>(matrix.exercises.size()));
    setColumnCount(static_cast<int>(matrix.days.size()) + 1);
    QStringList headers{"Exercise"};
    for (const auto& d : matrix.days) headers << qs(d);
    setHorizontalHeaderLabels(headers);
    horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setColumnWidth(0, 240);
    for (int c = 1; c < columnCount(); ++c) setColumnWidth(c, 46);

    for (size_t r = 0; r < matrix.exercises.size(); ++r) {
        QVariantList groups;
        for (auto g : matrix.groups[r]) groups << static_cast<int>(g);
        // Ukrainian names are routinely wider than the column, so they elide
        // with the full name behind a tooltip.
        auto* name = new QTableWidgetItem(qs(matrix.exercises[r]));
        name->setToolTip(qs(matrix.exercises[r]));
        name->setData(kGroupsRole, groups);
        setItem(static_cast<int>(r), 0, name);
        for (size_t c = 0; c < matrix.days.size(); ++c) {
            auto* cell = new QTableWidgetItem;
            cell->setData(kGroupsRole, groups);
            cell->setData(kPresentRole, static_cast<bool>(matrix.cells[r][c]));
            setItem(static_cast<int>(r), static_cast<int>(c + 1), cell);
        }
    }
}

class CalendarView::Grid : public QWidget {
public:
    Grid(CalendarView& owner) : owner_(owner) { setMinimumHeight(6 * 44 + 20); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QLocale locale;
        int first = static_cast<int>(locale.firstDayOfWeek());
        double cell_w = width() / 7.0;
        QFont small = font();
        small.setPointSizeF(small.pointSizeF() * 0.85);
        painter.setFont(small);
        for (int i = 0; i < 7; ++i) {
            int day = (first - 1 + i) % 7 + 1;
            painter.setPen(palette().color(QPalette::PlaceholderText));
            painter.drawText(QRectF(i * cell_w, 0, cell_w, 18), Qt::AlignCenter,
                             locale.dayName(day, QLocale::NarrowFormat));
        }
        cells_.clear();
        QDate month = owner_.anchor_;
        int leading = (month.dayOfWeek() - first + 7) % 7;
        for (int d = 1; d <= month.daysInMonth(); ++d) {
            int slot = leading + d - 1;
            QRectF rect(slot % 7 * cell_w + 2, 20 + slot / 7 * 44 + 2, cell_w - 4, 40);
            QDate date(month.year(), month.month(), d);
            auto key = ss(date.toString(Qt::ISODate));
            auto it = owner_.model_.calendar().find(key);
            const DayInfo* info = it == owner_.model_.calendar().end() ? nullptr : &it->second;
            bool selected = info && owner_.model_.selection() == info->id;
            if (selected) {
                QColor tint = palette().color(QPalette::Highlight);
                painter.setPen(tint);
                tint.setAlphaF(0.18f);
                painter.setBrush(tint);
                painter.drawRoundedRect(rect, 6, 6);
            }
            painter.setPen(info ? palette().color(QPalette::Text) : palette().color(QPalette::PlaceholderText));
            painter.drawText(QRectF(rect.left(), rect.top() + 2, rect.width(), 16), Qt::AlignCenter,
                             QString::number(d));
            if (info) {
                QFont bold = small;
                bold.setBold(true);
                bold.setPointSizeF(small.pointSizeF() * 0.9);
                painter.setFont(bold);
                QString code = qs(info->cycle_day);
                double badge_w = std::min(rect.width() - 4, QFontMetricsF(bold).horizontalAdvance(code) + 10);
                QRectF badge(rect.center().x() - badge_w / 2, rect.top() + 20, badge_w, 15);
                painter.setPen(Qt::NoPen);
                painter.setBrush(info->group ? theme::group_color(*info->group) : theme::kUnclassified);
                painter.drawRoundedRect(badge, 7, 7);
                painter.setPen(Qt::white);
                painter.drawText(badge, Qt::AlignCenter, code);
                painter.setFont(small);
                cells_.push_back({rect, info->id});
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        for (const auto& [rect, id] : cells_)
            if (rect.contains(event->position())) {
                owner_.model_.open(id);
                return;
            }
    }

private:
    CalendarView& owner_;
    std::vector<std::pair<QRectF, std::string>> cells_;
};

CalendarView::CalendarView(AppModel& model, QWidget* parent) : QWidget(parent), model_(model) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* nav = new QHBoxLayout;
    auto* prev = new QPushButton("‹");
    auto* next = new QPushButton("›");
    prev->setFixedWidth(32);
    next->setFixedWidth(32);
    title_ = new QLabel;
    title_->setAlignment(Qt::AlignCenter);
    nav->addWidget(prev);
    nav->addWidget(title_, 1);
    nav->addWidget(next);
    layout->addLayout(nav);
    grid_ = new Grid(*this);
    layout->addWidget(grid_);
    layout->addWidget(new MuscleGroupLegend);
    connect(prev, &QPushButton::clicked, this, [this] { shift_month(-1); });
    connect(next, &QPushButton::clicked, this, [this] { shift_month(1); });
    refresh();
}

void CalendarView::shift_month(int delta) {
    anchored_to_data_ = true;
    anchor_ = anchor_.addMonths(delta);
    refresh();
}

void CalendarView::refresh() {
    if (!anchored_to_data_) {
        const auto& calendar = model_.calendar();
        anchored_to_data_ = !calendar.empty();
        QDate latest = calendar.empty() ? QDate() : QDate::fromString(qs(calendar.rbegin()->first), Qt::ISODate);
        QDate base = latest.isValid() ? latest : QDate::currentDate();
        anchor_ = QDate(base.year(), base.month(), 1);
    }
    title_->setText("<b>" + QLocale().standaloneMonthName(anchor_.month()) + " " + QString::number(anchor_.year()) +
                    "</b>");
    grid_->update();
}

CycleView::CycleView(AppModel& model, QWidget* parent) : QWidget(parent), model_(model) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* table_host = new QWidget;
    table_stack_ = new QStackedLayout(table_host);
    table_ = new CycleTable;
    empty_ = new QLabel("No sessions in this folder.");
    empty_->setAlignment(Qt::AlignCenter);
    table_stack_->addWidget(table_);
    table_stack_->addWidget(empty_);
    layout->addWidget(table_host, 45);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* bottom = new QWidget;
    auto* row = new QHBoxLayout(bottom);
    auto* side = new QWidget;
    side->setFixedWidth(340);
    auto* column = new QVBoxLayout(side);
    column->addWidget(new QLabel("<b>Cycle muscle map</b>"));
    picker_ = new WeightingModePicker(model_);
    column->addWidget(picker_);
    map_ = new MuscleMapWidget(260);
    column->addWidget(map_);
    calendar_ = new CalendarView(model_);
    column->addWidget(calendar_);
    column->addStretch();
    row->addWidget(side);
    row->addStretch();
    scroll->setWidget(bottom);
    layout->addWidget(scroll, 55);
    refresh();
}

void CycleView::refresh() {
    table_->set_matrix(model_.cycle_matrix());
    table_stack_->setCurrentWidget(model_.cycle_matrix().empty() ? static_cast<QWidget*>(empty_) : table_);
    picker_->sync();
    map_->set_svg(model_.cycle_map_svg(), "Muscle map unavailable (no sessions or catalogue missing).");
    calendar_->refresh();
}
