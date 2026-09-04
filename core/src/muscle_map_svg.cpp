#include "workoutlog/muscle_map_svg.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace workoutlog::muscle_map_svg {

namespace {

int lerp(int a, int b, double t) {
    return static_cast<int>(std::lround(static_cast<double>(a) + (static_cast<double>(b) - a) * t));
}

std::array<int, 3> rgb(const std::string& hex) {
    std::string h = hex;
    if (!h.empty() && h[0] == '#') h.erase(0, 1);
    unsigned long v = 0;
    try {
        v = std::stoul(h, nullptr, 16);
    } catch (...) {
        v = 0;
    }
    return {static_cast<int>((v >> 16) & 0xff), static_cast<int>((v >> 8) & 0xff), static_cast<int>(v & 0xff)};
}

} // namespace

std::string color(double score, const std::string& low, const std::string& high, const std::string& zero) {
    if (!(score > 0)) return zero;
    double t = std::min(std::max(score, 0.0), 1.0);
    auto [r1, g1, b1] = rgb(low);
    auto [r2, g2, b2] = rgb(high);
    int r = lerp(r1, r2, t), g = lerp(g1, g2, t), b = lerp(b1, b2, t);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return std::string(buf);
}

// The Swift original spliced insertions in reverse over an NSMutableString so earlier
// insertions wouldn't shift later regex match ranges. Scanning forward and building a
// fresh output string sidesteps that problem entirely and needs no such trick.
std::string colorize(const std::string& tmpl, const std::map<std::string, double>& scores,
                      const std::string& low_color, const std::string& high_color, const std::string& zero_color) {
    static const std::string kNeedle = "data-muscle=\"";
    std::string out;
    out.reserve(tmpl.size() + 64);

    size_t pos = 0;
    while (true) {
        size_t found = tmpl.find(kNeedle, pos);
        if (found == std::string::npos) {
            out.append(tmpl, pos, std::string::npos);
            break;
        }

        size_t token_start = found + kNeedle.size();
        size_t i = token_start;
        while (i < tmpl.size() && ((tmpl[i] >= 'a' && tmpl[i] <= 'z') || tmpl[i] == '_')) i++;

        if (i > token_start && i < tmpl.size() && tmpl[i] == '"') {
            out.append(tmpl, pos, i + 1 - pos); // through the closing quote, inclusive
            std::string token = tmpl.substr(token_start, i - token_start);
            auto it = scores.find(token);
            double score_value = it != scores.end() ? it->second : 0.0;
            out += " style=\"fill:" + color(score_value, low_color, high_color, zero_color) + "\"";
            pos = i + 1;
        } else {
            out.append(tmpl, pos, found + kNeedle.size() - pos);
            pos = found + kNeedle.size();
        }
    }
    return out;
}

} // namespace workoutlog::muscle_map_svg
