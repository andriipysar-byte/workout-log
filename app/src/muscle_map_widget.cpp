#include "muscle_map_widget.hpp"

#include <QPainter>

MuscleMapWidget::MuscleMapWidget(int height, QWidget* parent) : QWidget(parent) {
    if (height > 0) setFixedHeight(height);
    setSizePolicy(QSizePolicy::Expanding, height > 0 ? QSizePolicy::Fixed : QSizePolicy::Expanding);
}

void MuscleMapWidget::set_svg(const std::optional<std::string>& svg, const QString& unavailable) {
    if (svg)
        renderer_.load(QByteArray::fromStdString(*svg));
    else
        renderer_.load(QByteArray());
    unavailable_ = unavailable;
    update();
}

void MuscleMapWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    if (!renderer_.isValid()) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(rect().adjusted(4, 4, -4, -4), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, unavailable_);
        return;
    }
    QSizeF size = renderer_.viewBoxF().size();
    size.scale(QSizeF(rect().size()), Qt::KeepAspectRatio);
    QRectF target(QPointF(0, 0), size);
    if (alignment_ & Qt::AlignTop)
        target.moveTopLeft(QPointF((width() - size.width()) / 2, 0));
    else
        target.moveCenter(QRectF(rect()).center());
    painter.setRenderHint(QPainter::Antialiasing);
    renderer_.render(&painter, target);
}
