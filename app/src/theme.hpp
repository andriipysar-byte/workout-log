#pragma once

#include <QColor>
#include <QLinearGradient>
#include <QString>

#include <string>
#include <vector>

#include "workoutlog/muscles.hpp"

inline QString qs(const std::string& s) { return QString::fromStdString(s); }
inline std::string ss(const QString& s) { return s.toStdString(); }

namespace theme {

inline constexpr int kSidebarWidth = 240;
// The calendar badge colour for a day whose dominant group is unknown, and the
// palette seed.
inline const QColor kUnclassified{0x2a, 0x78, 0xd6};

QColor group_color(wl::MuscleGroup group);

// A light wash of an exercise's primary group colour(s), top-left to
// bottom-right; transparent when the exercise is not in the catalogue.
QLinearGradient group_gradient(const std::vector<wl::MuscleGroup>& groups, double base, const QRectF& rect);

QColor card_surface();
QColor outline();

void apply(class QApplication& app);

} // namespace theme
