#include "workoutlog/dart_parse.hpp"

#include <charconv>
#include <cmath>
#include <limits>
#include <string>

#include "workoutlog/utf8.hpp"

namespace wl {

namespace {

bool is_digit(char c) { return c >= '0' && c <= '9'; }

} // namespace

std::optional<std::int64_t> dart_int_parse(std::string_view raw) {
    std::string text = utf8::trim(raw);
    std::string_view s = text;
    bool negative = false;
    if (!s.empty() && (s.front() == '+' || s.front() == '-')) {
        negative = s.front() == '-';
        s.remove_prefix(1);
    }
    int base = 10;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s.remove_prefix(2);
    }
    if (s.empty()) return std::nullopt;
    std::uint64_t magnitude = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), magnitude, base);
    if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
    constexpr auto max = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (!negative && magnitude > max) return std::nullopt;
    if (negative && magnitude > max + 1) return std::nullopt;
    return negative ? static_cast<std::int64_t>(0 - magnitude) : static_cast<std::int64_t>(magnitude);
}

std::optional<double> dart_double_parse(std::string_view raw) {
    std::string text = utf8::trim(raw);
    std::string_view s = text;
    bool negative = false;
    if (!s.empty() && (s.front() == '+' || s.front() == '-')) {
        negative = s.front() == '-';
        s.remove_prefix(1);
    }
    if (s == "NaN") return std::numeric_limits<double>::quiet_NaN();
    if (s == "Infinity")
        return negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();

    size_t i = 0, int_digits = 0, frac_digits = 0;
    while (i < s.size() && is_digit(s[i])) ++i, ++int_digits;
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && is_digit(s[i])) ++i, ++frac_digits;
    }
    if (int_digits + frac_digits == 0) return std::nullopt;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
        size_t exp_digits = 0;
        while (i < s.size() && is_digit(s[i])) ++i, ++exp_digits;
        if (exp_digits == 0) return std::nullopt;
    }
    if (i != s.size()) return std::nullopt;

    double value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec == std::errc::result_out_of_range) {
        bool huge = false;
        auto e = s.find_first_of("eE");
        if (e != std::string_view::npos) huge = s[e + 1] != '-';
        value = huge ? std::numeric_limits<double>::infinity() : 0.0;
    } else if (ec != std::errc{} || ptr != s.data() + s.size()) {
        return std::nullopt;
    }
    return negative ? -value : value;
}

} // namespace wl
