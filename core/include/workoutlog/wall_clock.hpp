#pragma once

#include <optional>
#include <string>
#include <string_view>

// The `HH:MM` bracket timestamps from the paper log, as minutes since midnight.
// Times carry no date, so they compare inside one session only.
namespace wl::wall_clock {

std::optional<int> minutes(const std::optional<std::string>& text);
std::optional<int> minutes(std::string_view text);
bool is_valid(const std::optional<std::string>& text);
std::string format(int minutes);

} // namespace wl::wall_clock
