#pragma once

#include <QSvgRenderer>
#include <QWidget>

#include <optional>
#include <string>

// Paints a finished, already-coloured SVG from the core (ADR-004), aspect-fit.
// With no SVG it shows why instead.
class MuscleMapWidget : public QWidget {
    Q_OBJECT

public:
    explicit MuscleMapWidget(int height = 280, QWidget* parent = nullptr);

    void set_svg(const std::optional<std::string>& svg, const QString& unavailable = {});
    void set_alignment(Qt::Alignment alignment) { alignment_ = alignment; }
    bool has_map() const { return renderer_.isValid(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QSvgRenderer renderer_;
    QString unavailable_;
    Qt::Alignment alignment_ = Qt::AlignCenter;
};
