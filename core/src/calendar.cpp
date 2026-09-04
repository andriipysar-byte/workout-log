#include "workoutlog/calendar.hpp"

#include <array>

namespace workoutlog::calendar {

namespace {

// Howard Hinnant's days-from-civil (public domain): days since 1970-01-01 for a
// proleptic Gregorian y/m/d. http://howardhinnant.github.io/date_algorithms.html
long long days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = static_cast<unsigned>(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

bool is_leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }

} // namespace

int weekday_sun1(int year, int month, int day) {
    long long days = days_from_civil(year, month, day);
    // 1970-01-01 (days == 0) was a Thursday, the 5th day in a 1=Sunday week.
    long long raw = ((days % 7) + 7) % 7;
    return static_cast<int>((raw + 4) % 7) + 1;
}

int days_in_month(int year, int month) {
    static constexpr std::array<int, 12> kDays = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap(year)) return 29;
    return kDays.at(static_cast<size_t>(month - 1)); // bounds-checked: month is caller-supplied
}

std::vector<std::optional<int>> month_cells(int year, int month, int first_weekday) {
    int weekday_of_first = weekday_sun1(year, month, 1);
    int leading = ((weekday_of_first - first_weekday) % 7 + 7) % 7;

    std::vector<std::optional<int>> cells(static_cast<size_t>(leading), std::nullopt);
    int n = days_in_month(year, month);
    for (int day = 1; day <= n; day++) cells.emplace_back(day);
    return cells;
}

} // namespace workoutlog::calendar
