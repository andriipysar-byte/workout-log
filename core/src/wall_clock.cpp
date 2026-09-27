#include "workoutlog/wall_clock.hpp"

#include <cstdio>

#include "workoutlog/utf8.hpp"

namespace wl::wall_clock {

std::optional<int> minutes(std::string_view raw) {
    std::string text = utf8::trim(raw);
    auto colon = text.find(':');
    if (colon == std::string::npos || colon < 1 || colon > 2 || text.size() - colon - 1 != 2)
        return std::nullopt;
    for (size_t i = 0; i < text.size(); ++i)
        if (i != colon && (text[i] < '0' || text[i] > '9')) return std::nullopt;
    int hour = std::stoi(text.substr(0, colon));
    int minute = std::stoi(text.substr(colon + 1));
    if (hour > 23 || minute > 59) return std::nullopt;
    return hour * 60 + minute;
}

std::optional<int> minutes(const std::optional<std::string>& text) {
    if (!text) return std::nullopt;
    return minutes(std::string_view(*text));
}

bool is_valid(const std::optional<std::string>& text) {
    return !text || minutes(text).has_value();
}

std::string format(int minutes) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", minutes / 60, minutes % 60);
    return buf;
}

} // namespace wl::wall_clock
