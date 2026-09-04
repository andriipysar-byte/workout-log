#pragma once

#include <map>
#include <string>

// Port of app/Sources/WorkoutLogCore/Analytics/MuscleMapSVG.swift. Recolours an SVG
// template: every element tagged data-muscle="<token>" gets an inline `fill` from its
// activation score. A `zero` colour marks unworked muscles distinctly from
// "barely worked".
namespace workoutlog::muscle_map_svg {

std::string colorize(const std::string& svg_template, const std::map<std::string, double>& scores,
                      const std::string& low_color = "#86b6ef", const std::string& high_color = "#0d366b",
                      const std::string& zero_color = "#e8e8e3");

// Blends the sequential ramp by score; score <= 0 -> the neutral zero colour.
std::string color(double score, const std::string& low, const std::string& high, const std::string& zero);

} // namespace workoutlog::muscle_map_svg
