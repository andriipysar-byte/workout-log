#include "workoutlog/utf8.hpp"

namespace workoutlog::utf8 {

char32_t decode_next(std::string_view s, size_t& pos) {
    if (pos >= s.size()) return 0;
    unsigned char b0 = static_cast<unsigned char>(s[pos]);

    auto continuation = [&](size_t i) -> bool {
        return i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80;
    };

    if (b0 < 0x80) {
        pos += 1;
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0 && continuation(pos + 1)) {
        char32_t cp = (static_cast<char32_t>(b0 & 0x1F) << 6) |
                      (static_cast<unsigned char>(s[pos + 1]) & 0x3F);
        pos += 2;
        return cp;
    }
    if ((b0 & 0xF0) == 0xE0 && continuation(pos + 1) && continuation(pos + 2)) {
        char32_t cp = (static_cast<char32_t>(b0 & 0x0F) << 12) |
                      (static_cast<char32_t>(static_cast<unsigned char>(s[pos + 1]) & 0x3F) << 6) |
                      (static_cast<unsigned char>(s[pos + 2]) & 0x3F);
        pos += 3;
        return cp;
    }
    if ((b0 & 0xF8) == 0xF0 && continuation(pos + 1) && continuation(pos + 2) && continuation(pos + 3)) {
        char32_t cp = (static_cast<char32_t>(b0 & 0x07) << 18) |
                      (static_cast<char32_t>(static_cast<unsigned char>(s[pos + 1]) & 0x3F) << 12) |
                      (static_cast<char32_t>(static_cast<unsigned char>(s[pos + 2]) & 0x3F) << 6) |
                      (static_cast<unsigned char>(s[pos + 3]) & 0x3F);
        pos += 4;
        return cp;
    }
    pos += 1;
    return 0xFFFD;
}

std::u32string to_u32(std::string_view s) {
    std::u32string out;
    out.reserve(s.size());
    size_t pos = 0;
    while (pos < s.size()) out.push_back(decode_next(s, pos));
    return out;
}

std::string from_u32(char32_t cp) {
    std::string out;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

std::string from_u32(const std::u32string& s) {
    std::string out;
    out.reserve(s.size());
    for (char32_t cp : s) out += from_u32(cp);
    return out;
}

char32_t to_lower_cp(char32_t cp) {
    if (cp >= U'A' && cp <= U'Z') return cp + 0x20;
    if (cp >= 0x0410 && cp <= 0x042F) return cp + 0x20;       // А-Я -> а-я
    if (cp >= 0x0400 && cp <= 0x040F) return cp + 0x50;       // Ѐ-Џ (incl. І Є Ї) -> ѐ-џ
    if (cp == 0x0490) return 0x0491;                          // Ґ -> ґ
    return cp;
}

std::u32string to_lower(const std::u32string& s) {
    std::u32string out;
    out.reserve(s.size());
    for (char32_t cp : s) out.push_back(to_lower_cp(cp));
    return out;
}

std::string to_lower(std::string_view s) {
    return from_u32(to_lower(to_u32(s)));
}

bool is_ascii_digit(char32_t cp) {
    return cp >= U'0' && cp <= U'9';
}

bool is_trim_whitespace(char32_t cp) {
    switch (cp) {
        case U' ': case U'\t': case U'\n': case U'\r': case U'\v': case U'\f':
        case 0x00A0:
            return true;
        default:
            return false;
    }
}

std::u32string trim(const std::u32string& s) {
    size_t begin = 0, end = s.size();
    while (begin < end && is_trim_whitespace(s[begin])) begin++;
    while (end > begin && is_trim_whitespace(s[end - 1])) end--;
    return s.substr(begin, end - begin);
}

std::string trim(std::string_view s) {
    return from_u32(trim(to_u32(s)));
}

} // namespace workoutlog::utf8
