#include "workoutlog/date.hpp"

#include <cstdio>

namespace wl {

namespace {

bool digits(std::string_view s) {
    for (char c : s)
        if (c < '0' || c > '9') return false;
    return !s.empty();
}

} // namespace

bool is_iso_date_shape(std::string_view s) {
    return s.size() == 10 && s[4] == '-' && s[7] == '-' && digits(s.substr(0, 4)) &&
           digits(s.substr(5, 2)) && digits(s.substr(8, 2));
}

std::optional<Date> parse_date(std::string_view s) {
    if (!is_iso_date_shape(s)) return std::nullopt;
    int year = std::stoi(std::string(s.substr(0, 4)));
    int month = std::stoi(std::string(s.substr(5, 2)));
    int day = std::stoi(std::string(s.substr(8, 2)));
    using namespace std::chrono;
    // Month 0 or 13+ and day 0 or 32+ roll over the way Dart's DateTime does.
    year_month first = year_month{std::chrono::year{year}, January} + months{month - 1};
    return sys_days{first / 1} + days{day - 1};
}

std::string iso_date(Date date) {
    std::chrono::year_month_day ymd{date};
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d-%02u-%02u", static_cast<int>(ymd.year()),
                  static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()));
    return buf;
}

int iso_weekday(Date date) {
    return static_cast<int>(std::chrono::weekday{date}.iso_encoding());
}

} // namespace wl
