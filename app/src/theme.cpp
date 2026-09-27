#include "theme.hpp"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace theme {

QColor group_color(wl::MuscleGroup group) {
    return QColor(QString::fromLatin1(wl::group_hex(group).data(), static_cast<int>(wl::group_hex(group).size())));
}

QLinearGradient group_gradient(const std::vector<wl::MuscleGroup>& groups, double base, const QRectF& rect) {
    QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
    auto with_alpha = [](QColor c, double a) {
        c.setAlphaF(static_cast<float>(std::clamp(a, 0.0, 1.0)));
        return c;
    };
    if (groups.empty()) {
        gradient.setColorAt(0, Qt::transparent);
        gradient.setColorAt(1, Qt::transparent);
    } else if (groups.size() == 1) {
        gradient.setColorAt(0, with_alpha(group_color(groups[0]), base * 1.4));
        gradient.setColorAt(1, with_alpha(group_color(groups[0]), base * 0.5));
    } else {
        for (size_t i = 0; i < groups.size(); ++i)
            gradient.setColorAt(static_cast<double>(i) / static_cast<double>(groups.size() - 1),
                                with_alpha(group_color(groups[i]), base));
    }
    return gradient;
}

QColor card_surface() {
    QPalette p = QApplication::palette();
    return p.color(QPalette::AlternateBase);
}

QColor outline() {
    return QApplication::palette().color(QPalette::PlaceholderText);
}

void apply(QApplication& app) {
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette palette = app.palette();
    palette.setColor(QPalette::Highlight, kUnclassified);
    palette.setColor(QPalette::Link, kUnclassified);
    app.setPalette(palette);
}

} // namespace theme
