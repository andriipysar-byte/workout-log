#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace wl {

using Date = std::chrono::sys_days;

// `YYYY-MM-DD`, strictly: four, two and two ASCII digits.
bool is_iso_date_shape(std::string_view text);

// Dart's `DateTime.parse` for a date: an out-of-range month or day rolls over
// (`2026-02-30` is 2 March) instead of failing. Anything not date-shaped fails.
std::optional<Date> parse_date(std::string_view text);

std::string iso_date(Date date);

// 1 = Monday … 7 = Sunday, as Dart's `DateTime.weekday`.
int iso_weekday(Date date);

} // namespace wl
