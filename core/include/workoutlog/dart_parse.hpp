#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

// Dart's `int.tryParse` / `double.tryParse` acceptance, so a notation or an MCP
// argument that parsed before still parses (and one that did not, still fails).
namespace wl {

// Optional sign, then decimal digits or a 0x-prefixed hex run; surrounding
// whitespace allowed; overflow is a failure.
std::optional<std::int64_t> dart_int_parse(std::string_view text);

// Decimal with optional fraction and exponent, or NaN / Infinity; no hex.
std::optional<double> dart_double_parse(std::string_view text);

} // namespace wl
