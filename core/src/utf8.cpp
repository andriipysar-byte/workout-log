#include "workoutlog/utf8.hpp"

namespace wl::utf8 {

char32_t decode_next(std::string_view s, size_t& pos) {
    if (pos >= s.size()) return 0;
    auto byte = [&](size_t i) { return static_cast<unsigned char>(s[i]); };
    auto continuation = [&](size_t i) { return i < s.size() && (byte(i) & 0xC0) == 0x80; };

    unsigned char b0 = byte(pos);
    if (b0 < 0x80) {
        pos += 1;
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0 && continuation(pos + 1)) {
        char32_t cp = (char32_t(b0 & 0x1F) << 6) | (byte(pos + 1) & 0x3F);
        pos += 2;
        return cp;
    }
    if ((b0 & 0xF0) == 0xE0 && continuation(pos + 1) && continuation(pos + 2)) {
        char32_t cp = (char32_t(b0 & 0x0F) << 12) | (char32_t(byte(pos + 1) & 0x3F) << 6) |
                      (byte(pos + 2) & 0x3F);
        pos += 3;
        return cp;
    }
    if ((b0 & 0xF8) == 0xF0 && continuation(pos + 1) && continuation(pos + 2) && continuation(pos + 3)) {
        char32_t cp = (char32_t(b0 & 0x07) << 18) | (char32_t(byte(pos + 1) & 0x3F) << 12) |
                      (char32_t(byte(pos + 2) & 0x3F) << 6) | (byte(pos + 3) & 0x3F);
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

std::string from_u32(std::u32string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char32_t cp : s) out += from_u32(cp);
    return out;
}

namespace {

char32_t lower(char32_t cp) {
    if (cp >= U'A' && cp <= U'Z') return cp + 0x20;
    if (cp < 0x80) return cp;
    if ((cp >= 0xC0 && cp <= 0xDE) && cp != 0xD7) return cp + 0x20;
    if (cp >= 0x0391 && cp <= 0x03AB && cp != 0x03A2) return cp + 0x20;
    if (cp >= 0x0410 && cp <= 0x042F) return cp + 0x20;
    if (cp >= 0x0400 && cp <= 0x040F) return cp + 0x50;
    if (cp == 0x04C0) return 0x04CF;
    if (cp >= 0x04C1 && cp <= 0x04CE) return cp % 2 == 1 ? cp + 1 : cp;
    if (cp >= 0x0460 && cp <= 0x04FF && cp % 2 == 0 && !(cp >= 0x0482 && cp <= 0x0489))
        return cp + 1;
    return cp;
}

} // namespace

std::string to_lower(std::string_view s) {
    std::u32string cps = to_u32(s);
    for (char32_t& cp : cps) cp = lower(cp);
    return from_u32(cps);
}

bool is_whitespace(char32_t cp) {
    return (cp >= 0x09 && cp <= 0x0D) || cp == 0x20 || cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 || cp == 0x202F ||
           cp == 0x205F || cp == 0x3000 || cp == 0xFEFF;
}

std::string trim(std::string_view s) {
    std::u32string cps = to_u32(s);
    size_t begin = 0, end = cps.size();
    while (begin < end && is_whitespace(cps[begin])) ++begin;
    while (end > begin && is_whitespace(cps[end - 1])) --end;
    return from_u32(std::u32string_view(cps).substr(begin, end - begin));
}

} // namespace wl::utf8
