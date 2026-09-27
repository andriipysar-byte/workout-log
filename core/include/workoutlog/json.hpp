#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace wl {

// Insertion-ordered so an analytics report reads in the order it was built;
// the file writer sorts on the way out instead.
using Json = nlohmann::ordered_json;

struct FormatError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

Json parse_json(std::string_view text);

// The on-disk canon: keys sorted at every level, integral doubles written as
// integers, two-space indent, raw UTF-8, trailing newline.
std::string encode_file(const Json& value);

// The same layout in insertion order and without the trailing newline — what the
// MCP server hands a client.
std::string encode_pretty(const Json& value);

// Dart's `double.toString()`: shortest round-trip digits, `.0` on integral
// values, exponent form outside [1e-6, 1e21).
std::string dart_double(double value);

// A number the way Dart string interpolation prints it: ints bare, doubles via
// `dart_double` (so `300.0`, not `300`).
std::string dart_num(const Json& number);

Json unmodelled_keys(const Json& object, std::initializer_list<std::string_view> modelled);

// Readers mirror Dart's `as T?` casts: absent or null is nullopt, a present value
// of the wrong type throws FormatError naming the key.
const Json* field(const Json& object, std::string_view key);
std::string req_string(const Json& object, std::string_view key);
std::optional<std::string> opt_string(const Json& object, std::string_view key);
std::optional<double> opt_double(const Json& object, std::string_view key);
std::optional<std::int64_t> opt_int(const Json& object, std::string_view key);
std::optional<bool> opt_bool(const Json& object, std::string_view key);
std::optional<std::vector<std::int64_t>> opt_int_list(const Json& object, std::string_view key);
std::vector<std::string> string_list(const Json& object, std::string_view key);
const Json& req_array(const Json& object, std::string_view key);
const Json* opt_array(const Json& object, std::string_view key);
void as_object(const Json& value, std::string_view what);

double to_double(const Json& number, std::string_view what);
std::int64_t to_int(const Json& number, std::string_view what);

template <typename T>
void put(Json& object, std::string_view key, const std::optional<T>& value) {
    if (value) object[std::string(key)] = *value;
}

Json to_json(const std::vector<std::int64_t>& values);

} // namespace wl
