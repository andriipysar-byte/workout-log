#pragma once

#include <string>
#include <string_view>

// The notation mixes Latin and Cyrillic look-alikes (`×`/`х`, `c`/`с`) and the
// catalogue is Ukrainian, so anything that scans or folds text works on code
// points, never bytes.
namespace wl::utf8 {

// Malformed sequences decode as U+FFFD and advance by one byte.
char32_t decode_next(std::string_view s, size_t& pos);

std::u32string to_u32(std::string_view utf8);
std::string from_u32(std::u32string_view s);
std::string from_u32(char32_t cp);

// Latin, Latin-1, Greek and Cyrillic — every script the catalogue could hold.
// Cyrillic is not a uniform +0x20: U+0400–U+040F (Ukrainian І, Є, Ї) fold by
// +0x50, and Ґ sits alone at U+0490.
std::string to_lower(std::string_view utf8);

// Dart's `String.trim()` set, which is wider than ASCII whitespace.
bool is_whitespace(char32_t cp);
std::string trim(std::string_view utf8);

} // namespace wl::utf8
