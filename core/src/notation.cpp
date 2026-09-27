#include "workoutlog/notation.hpp"

#include <algorithm>
#include <numeric>
#include <optional>

#include "workoutlog/dart_parse.hpp"
#include "workoutlog/utf8.hpp"

namespace wl {

namespace {

// The Cyrillic х/Х and с/С are distinct code points from their Latin lookalikes,
// and the paper log uses both.
bool is_multiplier(char32_t cp) {
    return cp == U'×' || cp == U'x' || cp == U'X' || cp == U'х' || cp == U'Х' || cp == U'*';
}

bool is_seconds_suffix(char32_t cp) {
    return cp == U'c' || cp == U'C' || cp == U'с' || cp == U'С';
}

bool is_ascii_digit(char32_t cp) { return cp >= U'0' && cp <= U'9'; }

// `\(\s*\d+\s*\)\s*$`: where the match starts and the number inside it.
struct TrailingParen {
    size_t start;
    std::int64_t value;
};

std::optional<TrailingParen> trailing_paren_number(const std::u32string& s) {
    size_t i = s.size();
    while (i > 0 && utf8::is_whitespace(s[i - 1])) --i;
    if (i == 0 || s[i - 1] != U')') return std::nullopt;
    --i;
    while (i > 0 && utf8::is_whitespace(s[i - 1])) --i;
    size_t digits_end = i;
    while (i > 0 && is_ascii_digit(s[i - 1])) --i;
    if (i == digits_end) return std::nullopt;
    size_t digits_start = i;
    while (i > 0 && utf8::is_whitespace(s[i - 1])) --i;
    if (i == 0 || s[i - 1] != U'(') return std::nullopt;
    std::int64_t value = 0;
    for (size_t k = digits_start; k < digits_end; ++k) value = value * 10 + (s[k] - U'0');
    return TrailingParen{i - 1, value};
}

std::vector<std::string> split(std::string_view s, char separator) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t at = s.find(separator, start);
        out.emplace_back(s.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start));
        if (at == std::string_view::npos) break;
        start = at + 1;
    }
    return out;
}

bool is_bracket_trim(char32_t cp) {
    return cp == U'[' || cp == U']' || cp == U'{' || cp == U'}' || cp == U' ' || cp == U'\t';
}

std::string strip_brackets(std::string_view s) {
    std::u32string cps = utf8::to_u32(s);
    size_t b = 0, e = cps.size();
    while (b < e && is_bracket_trim(cps[b])) ++b;
    while (e > b && is_bracket_trim(cps[e - 1])) --e;
    return utf8::from_u32(std::u32string_view(cps).substr(b, e - b));
}

std::string replace_commas(std::string s) {
    std::replace(s.begin(), s.end(), ',', '.');
    return s;
}

// Split on '+' at bracket depth 0, so weights inside [...] stay intact.
std::vector<std::string> split_top_level(std::string_view input) {
    std::vector<std::string> result;
    std::string current;
    int depth = 0;
    for (char c : input) {
        if (c == '[' || c == '{') {
            ++depth;
            current.push_back(c);
        } else if (c == ']' || c == '}') {
            depth = depth > 0 ? depth - 1 : 0;
            current.push_back(c);
        } else if (c == '+' && depth == 0) {
            result.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    result.push_back(current);
    std::vector<std::string> out;
    for (auto& s : result) {
        std::string t = utf8::trim(s);
        if (!t.empty()) out.push_back(std::move(t));
    }
    return out;
}

ParsedSets parse_cluster(const std::string& line) {
    ParsedSets out;
    std::u32string cps = utf8::to_u32(line);
    std::optional<std::int64_t> total;
    if (auto paren = trailing_paren_number(cps)) {
        total = paren->value;
        cps.resize(paren->start);
    }
    std::vector<std::string> parts;
    for (auto& p : split(utf8::from_u32(cps), '+'))
        if (!p.empty()) parts.push_back(utf8::trim(p));
    IntList reps;
    for (const auto& p : parts)
        if (auto n = dart_int_parse(p)) reps.push_back(*n);
    if (reps.empty() || reps.size() != parts.size()) {
        out.warnings.push_back("could not parse cluster: " + line);
        return out;
    }
    WorkSet set;
    set.total_reps = total ? *total : std::accumulate(reps.begin(), reps.end(), std::int64_t{0});
    set.cluster = std::move(reps);
    out.sets.push_back(std::move(set));
    return out;
}

std::optional<double> parse_duration(const std::string& token) {
    std::u32string cps = utf8::to_u32(token);
    if (cps.empty() || !is_seconds_suffix(cps.back())) return std::nullopt;
    cps.pop_back();
    return dart_double_parse(replace_commas(utf8::trim(utf8::from_u32(cps))));
}

struct Weight {
    double kg;
    std::optional<std::int64_t> reps;
};

std::optional<Weight> parse_weight(const std::string& token) {
    std::u32string cps = utf8::to_u32(token);
    std::optional<std::int64_t> reps;
    if (auto paren = trailing_paren_number(cps)) {
        reps = paren->value;
        cps.resize(paren->start);
    }
    auto kg = dart_double_parse(replace_commas(utf8::trim(utf8::from_u32(cps))));
    if (!kg) return std::nullopt;
    return Weight{*kg, reps};
}

std::vector<WorkSet> parse_group(const std::string& group, bool is_backoff, std::vector<std::string>& warnings) {
    std::u32string cps = utf8::to_u32(group);
    std::optional<std::int64_t> count;
    std::string list_part = group;

    auto multiplier = std::find_if(cps.begin(), cps.end(), is_multiplier);
    if (multiplier != cps.end()) {
        std::string count_text = utf8::trim(utf8::from_u32(std::u32string(cps.begin(), multiplier)));
        if (auto parsed = dart_int_parse(count_text))
            count = parsed;
        else
            warnings.push_back("unrecognised rep count \"" + count_text + "\" in \"" + group + "\"");
        list_part = utf8::from_u32(std::u32string(multiplier + 1, cps.end()));
    }

    std::vector<std::string> items;
    for (auto& s : split(strip_brackets(list_part), ',')) {
        if (s.empty()) continue;
        std::string t = utf8::trim(s);
        if (!t.empty()) items.push_back(std::move(t));
    }
    if (items.empty()) {
        warnings.push_back("no values in \"" + group + "\"");
        return {};
    }

    std::vector<WorkSet> out;
    std::optional<bool> backoff = is_backoff ? std::optional<bool>(true) : std::nullopt;
    for (const auto& item : items) {
        if (auto seconds = parse_duration(item)) {
            WorkSet set;
            set.duration_sec = seconds;
            set.is_backoff = backoff;
            out.push_back(std::move(set));
            continue;
        }
        if (auto weight = parse_weight(item)) {
            WorkSet set;
            set.weight_kg = weight->kg;
            set.reps = weight->reps ? weight->reps : count;
            set.is_backoff = backoff;
            out.push_back(std::move(set));
        } else {
            warnings.push_back("unparseable value \"" + item + "\" in \"" + group + "\"");
        }
    }
    return out;
}

} // namespace

ParsedSets parse_strength_sets(std::string_view raw) {
    std::string line = utf8::trim(raw);
    if (line.empty()) return {};

    std::u32string cps = utf8::to_u32(line);
    bool has_multiplier = std::any_of(cps.begin(), cps.end(), is_multiplier);
    bool has_bracket = line.find('[') != std::string::npos || line.find('{') != std::string::npos;
    if (!has_multiplier && !has_bracket && line.find('+') != std::string::npos) return parse_cluster(line);

    ParsedSets out;
    auto groups = split_top_level(line);
    for (size_t i = 0; i < groups.size(); ++i) {
        auto sets = parse_group(groups[i], i > 0, out.warnings);
        out.sets.insert(out.sets.end(), sets.begin(), sets.end());
    }
    if (out.sets.empty() && out.warnings.empty()) out.warnings.push_back("could not parse: " + line);
    return out;
}

} // namespace wl

namespace wl {

std::string set_summary(const WorkSet& set) {
    if (set.cluster) {
        std::string out;
        for (size_t i = 0; i < set.cluster->size(); ++i) {
            if (i) out.push_back('+');
            out += std::to_string((*set.cluster)[i]);
        }
        if (set.total_reps) out += " (" + std::to_string(*set.total_reps) + ")";
        return out;
    }
    if (set.duration_sec) return std::to_string(static_cast<std::int64_t>(*set.duration_sec)) + "c";
    std::string reps = set.reps ? std::to_string(*set.reps) + "×" : "";
    std::string weight = set.weight_kg ? std::to_string(static_cast<std::int64_t>(*set.weight_kg)) : "bw";
    return reps + weight + (set.backoff() ? "*" : "");
}

} // namespace wl
