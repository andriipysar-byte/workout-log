#pragma once

#include <string>

#include "workoutlog/models.hpp"

// Port of app/Sources/WorkoutLogCore/Coding.swift. Both platforms write through this
// writer now, so byte-compatibility with Swift's JSONEncoder isn't required -- but the
// corpus has zero fractional numbers, so the integral-double rule below produces
// identical output to Swift's on every existing file.
namespace workoutlog::json {

// Throws std::runtime_error, with a message naming the offending key, on malformed
// JSON or a schema violation (unknown block "type", missing required key, etc).
Session decode_session(const std::string& utf8_json);
Catalogue decode_catalogue(const std::string& utf8_json);

// Canonical writer: 2-space indent, sorted keys, raw UTF-8 (never \uXXXX), unescaped
// slashes, integral doubles written as integers, absent optionals omitted (never
// null), trailing newline.
std::string encode_session(const Session&);

// Rewrites arbitrary JSON (not just a Session -- also exercises.json, cycles.json)
// through the same writer policy without going through a typed model, so unmodelled
// fields like exercises.json's top-level "$comment" survive untouched. Used by wl_fmt
// for the one-time reformat commit (see docs/05-architecture.md, ADR-007).
std::string canonicalize(const std::string& utf8_json);

} // namespace workoutlog::json
