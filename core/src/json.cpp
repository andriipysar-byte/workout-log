#include "workoutlog/json.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>

namespace wl {

Json parse_json(std::string_view text) {
    try {
        return Json::parse(text.begin(), text.end());
    } catch (const nlohmann::json::exception& e) {
        throw FormatError(e.what());
    }
}

std::string dart_double(double value) {
    if (std::isnan(value)) return "NaN";
    if (std::isinf(value)) return value < 0 ? "-Infinity" : "Infinity";
    if (value == 0) return std::signbit(value) ? "-0.0" : "0.0";

    char buf[64];
    auto result = std::to_chars(buf, buf + sizeof buf, value, std::chars_format::scientific);
    std::string_view sci(buf, static_cast<size_t>(result.ptr - buf));

    std::string out;
    if (sci.front() == '-') {
        out.push_back('-');
        sci.remove_prefix(1);
    }
    auto e = sci.find('e');
    std::string digits;
    for (char c : sci.substr(0, e))
        if (c != '.') digits.push_back(c);
    int exponent = std::stoi(std::string(sci.substr(e + 1)));
    int k = static_cast<int>(digits.size());
    int n = exponent + 1;

    if (k <= n && n <= 21) {
        out += digits;
        out.append(static_cast<size_t>(n - k), '0');
        out += ".0";
    } else if (0 < n && n <= 21) {
        out += digits.substr(0, static_cast<size_t>(n));
        out.push_back('.');
        out += digits.substr(static_cast<size_t>(n));
    } else if (-6 < n && n <= 0) {
        out += "0.";
        out.append(static_cast<size_t>(-n), '0');
        out += digits;
    } else {
        out.push_back(digits[0]);
        if (k > 1) {
            out.push_back('.');
            out += digits.substr(1);
        }
        out.push_back('e');
        out += n - 1 >= 0 ? "+" : "-";
        out += std::to_string(std::abs(n - 1));
    }
    return out;
}

std::string dart_num(const Json& number) {
    if (number.is_number_float()) return dart_double(number.get<double>());
    return number.dump();
}

namespace {

void write_string(std::string& out, std::string_view s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\f': out += "\\f"; break;
            case '\r': out += "\\r"; break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    std::snprintf(esc, sizeof esc, "\\u%04x", c);
                    out += esc;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

void write_number(std::string& out, const Json& v) {
    if (v.is_number_integer()) {
        out += v.dump();
        return;
    }
    double d = v.get<double>();
    if (!std::isfinite(d)) throw FormatError("cannot encode a non-finite number");
    if (d == std::round(d) && std::fabs(d) < 9.2e18) {
        out += std::to_string(static_cast<std::int64_t>(d));
        return;
    }
    out += dart_double(d);
}

void newline(std::string& out, int depth) {
    out.push_back('\n');
    out.append(static_cast<size_t>(depth) * 2, ' ');
}

void write(std::string& out, const Json& v, int depth, bool sorted) {
    switch (v.type()) {
        case Json::value_t::object: {
            if (v.empty()) {
                out += "{}";
                return;
            }
            std::vector<std::string_view> keys;
            keys.reserve(v.size());
            for (auto it = v.begin(); it != v.end(); ++it) keys.emplace_back(it.key());
            if (sorted)
                std::sort(keys.begin(), keys.end());
            out.push_back('{');
            bool first = true;
            for (auto key : keys) {
                if (!first) out.push_back(',');
                first = false;
                newline(out, depth + 1);
                write_string(out, key);
                out += ": ";
                write(out, v.at(std::string(key)), depth + 1, sorted);
            }
            newline(out, depth);
            out.push_back('}');
            return;
        }
        case Json::value_t::array: {
            if (v.empty()) {
                out += "[]";
                return;
            }
            out.push_back('[');
            bool first = true;
            for (const auto& item : v) {
                if (!first) out.push_back(',');
                first = false;
                newline(out, depth + 1);
                write(out, item, depth + 1, sorted);
            }
            newline(out, depth);
            out.push_back(']');
            return;
        }
        case Json::value_t::string: write_string(out, v.get_ref<const std::string&>()); return;
        case Json::value_t::boolean: out += v.get<bool>() ? "true" : "false"; return;
        case Json::value_t::null: out += "null"; return;
        case Json::value_t::number_integer:
        case Json::value_t::number_unsigned:
        case Json::value_t::number_float: write_number(out, v); return;
        default: throw FormatError("cannot encode a binary or discarded value");
    }
}

[[noreturn]] void wrong_type(std::string_view key, std::string_view expected) {
    throw FormatError("\"" + std::string(key) + "\" must be " + std::string(expected));
}

} // namespace

std::string encode_file(const Json& value) {
    std::string out;
    write(out, value, 0, true);
    out.push_back('\n');
    return out;
}

std::string encode_pretty(const Json& value) {
    std::string out;
    write(out, value, 0, false);
    return out;
}

Json unmodelled_keys(const Json& object, std::initializer_list<std::string_view> modelled) {
    Json out = Json::object();
    for (auto it = object.begin(); it != object.end(); ++it)
        if (std::find(modelled.begin(), modelled.end(), it.key()) == modelled.end())
            out[it.key()] = it.value();
    return out;
}

const Json* field(const Json& object, std::string_view key) {
    auto it = object.find(key);
    if (it == object.end() || it->is_null()) return nullptr;
    return &*it;
}

std::string req_string(const Json& object, std::string_view key) {
    const Json* v = field(object, key);
    if (!v || !v->is_string()) wrong_type(key, "a string");
    return v->get<std::string>();
}

std::optional<std::string> opt_string(const Json& object, std::string_view key) {
    const Json* v = field(object, key);
    if (!v) return std::nullopt;
    if (!v->is_string()) wrong_type(key, "a string");
    return v->get<std::string>();
}

double to_double(const Json& number, std::string_view what) {
    if (!number.is_number()) wrong_type(what, "a number");
    return number.get<double>();
}

std::int64_t to_int(const Json& number, std::string_view what) {
    if (number.is_number_integer()) return number.get<std::int64_t>();
    if (!number.is_number()) wrong_type(what, "a number");
    double d = number.get<double>();
    if (!std::isfinite(d)) wrong_type(what, "a finite number");
    return static_cast<std::int64_t>(std::trunc(d));
}

std::optional<double> opt_double(const Json& object, std::string_view key) {
    const Json* v = field(object, key);
    if (!v) return std::nullopt;
    return to_double(*v, key);
}

std::optional<std::int64_t> opt_int(const Json& object, std::string_view key) {
    const Json* v = field(object, key);
    if (!v) return std::nullopt;
    return to_int(*v, key);
}

std::optional<bool> opt_bool(const Json& object, std::string_view key) {
    const Json* v = field(object, key);
    if (!v) return std::nullopt;
    if (!v->is_boolean()) wrong_type(key, "a boolean");
    return v->get<bool>();
}

const Json& req_array(const Json& object, std::string_view key) {
    const Json* v = field(object, key);
    if (!v || !v->is_array()) wrong_type(key, "a list");
    return *v;
}

const Json* opt_array(const Json& object, std::string_view key) {
    const Json* v = field(object, key);
    if (v && !v->is_array()) wrong_type(key, "a list");
    return v;
}

void as_object(const Json& value, std::string_view what) {
    if (!value.is_object()) wrong_type(what, "an object");
}

std::optional<std::vector<std::int64_t>> opt_int_list(const Json& object, std::string_view key) {
    const Json* list = opt_array(object, key);
    if (!list) return std::nullopt;
    std::vector<std::int64_t> out;
    out.reserve(list->size());
    for (const auto& e : *list) out.push_back(to_int(e, key));
    return out;
}

std::vector<std::string> string_list(const Json& object, std::string_view key) {
    const Json* list = opt_array(object, key);
    std::vector<std::string> out;
    if (!list) return out;
    for (const auto& e : *list) {
        if (!e.is_string()) wrong_type(key, "a list of strings");
        out.push_back(e.get<std::string>());
    }
    return out;
}

Json to_json(const std::vector<std::int64_t>& values) {
    Json out = Json::array();
    for (auto v : values) out.push_back(v);
    return out;
}

} // namespace wl
