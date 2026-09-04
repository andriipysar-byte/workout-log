#include "workoutlog/notation.hpp"

#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <utility>

#include "workoutlog/utf8.hpp"

namespace workoutlog::notation {

using workoutlog::utf8::is_ascii_digit;
using workoutlog::utf8::is_trim_whitespace;

// Strict numeric parsing shared with the UI (declared in notation.hpp): std::stod
// and the pre-C++20 std::strtod path both accept trailing garbage, leading/trailing
// whitespace and hex floats, which Swift's Double(String) rejects, so parsing this
// grammar (and the UI's optional-numeric fields) needs the stricter guard below.
// std::from_chars gives that directly; the conservative subset (see CMakeLists.txt)
// keeps the strtod fallback because Apple libc++ has no floating-point from_chars.
std::optional<double> parse_double_strict(std::string_view s) {
    if (s.empty()) return std::nullopt;
#if defined(__cpp_lib_to_chars)
    double v{};
    auto res = std::from_chars(s.data(), s.data() + s.size(), v);
    if (res.ec != std::errc{} || res.ptr != s.data() + s.size()) return std::nullopt;
    return v;
#else
    for (char c : s) {
        bool ok = std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '+';
        if (!ok) return std::nullopt;
    }
    errno = 0;
    char* end = nullptr;
    const std::string owned(s); // strtod needs a null terminator; a string_view has none
    double v = std::strtod(owned.c_str(), &end);
    if (end != owned.c_str() + owned.size() || errno == ERANGE) return std::nullopt;
    return v;
#endif
}

std::optional<int> parse_int_strict(std::string_view s) {
    if (s.empty()) return std::nullopt;
    size_t start = (s[0] == '+' || s[0] == '-') ? 1 : 0;
    if (start == s.size()) return std::nullopt;
    for (size_t i = start; i < s.size(); i++)
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return std::nullopt;
    int v{};
    auto res = std::from_chars(s.data(), s.data() + s.size(), v);
    if (res.ec != std::errc{} || res.ptr != s.data() + s.size()) return std::nullopt;
    return v;
}

namespace {

// U+00D7 (multiplication sign), Latin x/X, Cyrillic х/Х (U+0445/U+0425), '*'.
const std::u32string MULTIPLIERS = U"×xXхХ*";
// Latin c/C, Cyrillic с/С (U+0441/U+0421).
const std::u32string SECONDS_SUFFIX = U"cCсС";

bool contains_any(const std::u32string& s, const std::u32string& set) {
    for (char32_t c : s)
        if (set.find(c) != std::u32string::npos) return true;
    return false;
}

std::string narrow(const std::u32string& s) { return workoutlog::utf8::from_u32(s); }

// Swift: `\(\s*\d+\s*\)\s*$` -- trailing "(" ws* digits ws* ")" ws*, anchored at the
// end of the string. On match, extracts the digits and erases the whole suffix.
std::optional<int> strip_trailing_paren_number(std::u32string& s) {
    size_t i = s.size();
    while (i > 0 && is_trim_whitespace(s[i - 1])) i--;
    if (i == 0 || s[i - 1] != U')') return std::nullopt;
    size_t close = i - 1;

    size_t k = close;
    while (k > 0 && is_trim_whitespace(s[k - 1])) k--;
    size_t digits_end = k;
    while (k > 0 && is_ascii_digit(s[k - 1])) k--;
    size_t digits_start = k;
    if (digits_start == digits_end) return std::nullopt;
    while (k > 0 && is_trim_whitespace(s[k - 1])) k--;
    if (k == 0 || s[k - 1] != U'(') return std::nullopt;
    size_t open_paren = k - 1;

    auto value = parse_int_strict(narrow(s.substr(digits_start, digits_end - digits_start)));
    if (!value) return std::nullopt;
    s.erase(open_paren);
    return value;
}

// Trims chars belonging to `set` from both ends only (matches
// String.trimmingCharacters(in:), not a substring strip).
std::u32string strip_chars(const std::u32string& s, const std::u32string& set) {
    size_t begin = 0, end = s.size();
    while (begin < end && set.find(s[begin]) != std::u32string::npos) begin++;
    while (end > begin && set.find(s[end - 1]) != std::u32string::npos) end--;
    return s.substr(begin, end - begin);
}

std::vector<std::u32string> split(const std::u32string& s, char32_t sep) {
    std::vector<std::u32string> parts;
    std::u32string current;
    for (char32_t ch : s) {
        if (ch == sep) {
            parts.push_back(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    parts.push_back(current);
    return parts;
}

// Splits on `separator` at bracket depth 0, so weights inside [...]/{...} stay intact;
// trims and drops empty pieces, matching Swift's splitTopLevel.
std::vector<std::u32string> split_top_level(const std::u32string& chars, char32_t separator) {
    std::vector<std::u32string> raw;
    std::u32string current;
    int depth = 0;
    for (char32_t ch : chars) {
        if (ch == U'[' || ch == U'{') {
            depth++;
            current += ch;
        } else if (ch == U']' || ch == U'}') {
            depth = depth > 0 ? depth - 1 : 0;
            current += ch;
        } else if (ch == separator && depth == 0) {
            raw.push_back(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    raw.push_back(current);

    std::vector<std::u32string> out;
    for (auto& r : raw) {
        auto t = workoutlog::utf8::trim(r);
        if (!t.empty()) out.push_back(std::move(t));
    }
    return out;
}

// "54c" / Cyrillic-suffixed equivalent -> 54 seconds; nullopt if not a duration token.
std::optional<double> parse_duration(const std::u32string& token) {
    if (token.empty()) return std::nullopt;
    char32_t last = token.back();
    if (SECONDS_SUFFIX.find(last) == std::u32string::npos) return std::nullopt;
    auto num_part = workoutlog::utf8::trim(token.substr(0, token.size() - 1));
    for (auto& c : num_part)
        if (c == U',') c = U'.';
    return parse_double_strict(narrow(num_part));
}

// "90" or "90(2)" or "47.5" -> (weight, optional rep override).
std::optional<std::pair<double, std::optional<int>>> parse_weight(const std::u32string& token) {
    std::u32string t = token;
    auto reps = strip_trailing_paren_number(t);
    t = workoutlog::utf8::trim(t);
    for (auto& c : t)
        if (c == U',') c = U'.';
    auto weight = parse_double_strict(narrow(t));
    if (!weight) return std::nullopt;
    return std::make_pair(*weight, reps);
}

std::vector<WorkSet> parse_group(const std::u32string& group, bool is_backoff,
                                  std::vector<std::string>& warnings) {
    std::optional<int> count;
    std::u32string list_part = group;

    auto m_idx = group.find_first_of(MULTIPLIERS);
    if (m_idx != std::u32string::npos) {
        auto count_str = workoutlog::utf8::trim(group.substr(0, m_idx));
        if (auto c = parse_int_strict(narrow(count_str))) {
            count = c;
        } else {
            warnings.push_back("unrecognised rep count \"" + narrow(count_str) + "\" in \"" + narrow(group) + "\"");
        }
        list_part = group.substr(m_idx + 1);
    }

    list_part = strip_chars(list_part, U"[]{} \t");

    std::vector<std::u32string> items;
    for (auto& raw_item : split(list_part, U',')) {
        auto t = workoutlog::utf8::trim(raw_item);
        if (!t.empty()) items.push_back(std::move(t));
    }

    if (items.empty()) {
        warnings.push_back("no values in \"" + narrow(group) + "\"");
        return {};
    }

    std::vector<WorkSet> out;
    for (const auto& item : items) {
        if (auto seconds = parse_duration(item)) {
            WorkSet s;
            s.duration_sec = seconds;
            if (is_backoff) s.is_backoff = true;
            out.push_back(s);
        } else if (auto w = parse_weight(item)) {
            WorkSet s;
            s.weight_kg = w->first;
            s.reps = w->second ? w->second : count;
            if (is_backoff) s.is_backoff = true;
            out.push_back(s);
        } else {
            warnings.push_back("unparseable value \"" + narrow(item) + "\" in \"" + narrow(group) + "\"");
        }
    }
    return out;
}

ParsedSets parse_cluster(const std::u32string& line, std::vector<std::string> warnings) {
    std::u32string body = line;
    auto total = strip_trailing_paren_number(body);

    std::vector<std::u32string> parts;
    for (auto& raw_part : split(body, U'+')) {
        if (!raw_part.empty()) parts.push_back(raw_part);
    }
    for (auto& p : parts) p = workoutlog::utf8::trim(p);

    std::vector<int> reps;
    bool ok = true;
    for (auto& p : parts) {
        auto v = parse_int_strict(narrow(p));
        if (!v) {
            ok = false;
            break;
        }
        reps.push_back(*v);
    }

    if (!ok || reps.empty()) {
        warnings.push_back("could not parse cluster: " + narrow(line));
        return {{}, std::move(warnings)};
    }

    WorkSet set;
    set.cluster = reps;
    int sum = 0;
    for (int r : reps) sum += r;
    set.total_reps = total ? *total : sum;
    return {{set}, std::move(warnings)};
}

} // namespace

ParsedSets parse_strength_sets(std::string_view raw) {
    std::vector<std::string> warnings;
    auto line = workoutlog::utf8::trim(workoutlog::utf8::to_u32(raw));
    if (line.empty()) return {};

    bool has_multiplier = contains_any(line, MULTIPLIERS);
    bool has_bracket = line.find(U'[') != std::u32string::npos || line.find(U'{') != std::u32string::npos;

    if (!has_multiplier && !has_bracket && line.find(U'+') != std::u32string::npos) {
        return parse_cluster(line, std::move(warnings));
    }

    std::vector<WorkSet> sets;
    auto groups = split_top_level(line, U'+');
    for (size_t idx = 0; idx < groups.size(); idx++) {
        auto g = parse_group(groups[idx], idx > 0, warnings);
        sets.insert(sets.end(), g.begin(), g.end());
    }
    if (sets.empty() && warnings.empty()) warnings.push_back("could not parse: " + narrow(line));
    return {sets, warnings};
}

namespace {

// A NaN/inf or a magnitude beyond exact double-to-int64 representation would make
// llround's result meaningless; the display falls back to "?" rather than feeding
// that through a narrowing cast (AGENTS.md 2.6). Untrusted input (a hand-edited
// session file) is the only realistic way to reach that branch.
std::optional<long long> round_for_display(double v) {
    if (!std::isfinite(v) || std::fabs(v) >= 9007199254740992.0) return std::nullopt;
    return std::llround(v);
}

} // namespace

std::string format_set(const WorkSet& set) {
    std::ostringstream out;

    if (set.cluster.has_value()) {
        const auto& cluster = *set.cluster;
        for (size_t i = 0; i < cluster.size(); i++) {
            if (i > 0) out << '+';
            out << cluster[i];
        }
        if (set.total_reps.has_value()) out << " (" << *set.total_reps << ")";
        return out.str();
    }

    if (set.duration_sec.has_value()) {
        if (auto seconds = round_for_display(*set.duration_sec)) {
            out << *seconds << 'c';
        } else {
            out << "?c";
        }
        return out.str();
    }

    if (set.reps.has_value()) out << *set.reps << "×";
    if (set.weight_kg.has_value()) {
        if (auto weight = round_for_display(*set.weight_kg)) {
            out << *weight;
        } else {
            out << "?";
        }
    } else {
        out << "bw";
    }
    if (set.is_backoff.has_value() && *set.is_backoff) out << '*';
    return out.str();
}

} // namespace workoutlog::notation
