#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// Minimal UTF-8 helpers. Notation.swift scans Swift `Character`s (grapheme clusters)
// and deliberately mixes Latin and Cyrillic tokens (multiplier `×`/`х`, seconds suffix
// `c`/`с`); a byte-wise C++ scan silently breaks on those. This decodes to code points
// instead, which is equivalent for this notation because none of it uses combining
// marks or multi-codepoint graphemes.
namespace workoutlog::utf8 {

// Decodes one code point starting at `pos` and advances `pos` past it. Malformed
// sequences decode as U+FFFD and advance by one byte.
char32_t decode_next(std::string_view s, size_t& pos);

std::u32string to_u32(std::string_view utf8);
std::string from_u32(const std::u32string& s);
std::string from_u32(char32_t cp);

// Cyrillic case folding is range-dependent, not a uniform +0x20: the block
// U+0400-U+040F (which holds Ukrainian `І`/`Є`/`Ї`) folds by +0x50, while the main
// U+0410-U+042F block folds by +0x20. Getting this wrong is silent and specific to
// Ukrainian letters, which this catalogue uses throughout.
char32_t to_lower_cp(char32_t cp);
std::u32string to_lower(const std::u32string& s);
std::string to_lower(std::string_view utf8);

bool is_ascii_digit(char32_t cp);

// Matches Swift's `.whitespacesAndNewlines`: space, \t\n\r\v\f, plus U+00A0.
bool is_trim_whitespace(char32_t cp);
std::u32string trim(const std::u32string& s);
std::string trim(std::string_view utf8);

} // namespace workoutlog::utf8
