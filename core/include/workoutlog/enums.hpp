#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "workoutlog/json.hpp"

namespace wl {

template <typename E>
struct EnumNames;

#define WL_ENUM_NAMES(E, ...)                                                          \
    template <>                                                                        \
    struct EnumNames<E> {                                                              \
        static constexpr std::array names{__VA_ARGS__};                                \
    };

enum class Kind { training, deload, retest };
WL_ENUM_NAMES(Kind, std::string_view("training"), std::string_view("deload"), std::string_view("retest"))

// Declaration order is heaviest first; tie-breaks rely on it.
enum class RepBand { heavy, base, volume };
WL_ENUM_NAMES(RepBand, std::string_view("heavy"), std::string_view("base"), std::string_view("volume"))

enum class BarSpeed { fast, ok, slow, grind };
WL_ENUM_NAMES(BarSpeed, std::string_view("fast"), std::string_view("ok"), std::string_view("slow"),
              std::string_view("grind"))

enum class MetconFormat { for_time, amrap, emom, intervals, ladder, chipper };
WL_ENUM_NAMES(MetconFormat, std::string_view("for_time"), std::string_view("amrap"),
              std::string_view("emom"), std::string_view("intervals"), std::string_view("ladder"),
              std::string_view("chipper"))

// power and speed are the explosive categories that drive the P3 bar-speed rule.
enum class ExerciseCategory { strength, power, speed, longevity };
WL_ENUM_NAMES(ExerciseCategory, std::string_view("strength"), std::string_view("power"),
              std::string_view("speed"), std::string_view("longevity"))

enum class MovementPattern { squat, hinge, press, pull, olympic, carry, core, grip };
WL_ENUM_NAMES(MovementPattern, std::string_view("squat"), std::string_view("hinge"),
              std::string_view("press"), std::string_view("pull"), std::string_view("olympic"),
              std::string_view("carry"), std::string_view("core"), std::string_view("grip"))

enum class Modality { barbell, dumbbell, kettlebell, bodyweight, machine };
WL_ENUM_NAMES(Modality, std::string_view("barbell"), std::string_view("dumbbell"),
              std::string_view("kettlebell"), std::string_view("bodyweight"), std::string_view("machine"))

#undef WL_ENUM_NAMES

template <typename E>
constexpr std::string_view name(E value) {
    return EnumNames<E>::names[static_cast<size_t>(value)];
}

template <typename E>
constexpr size_t enum_count() {
    return EnumNames<E>::names.size();
}

template <typename E>
std::optional<E> try_parse(std::string_view text) {
    const auto& names = EnumNames<E>::names;
    for (size_t i = 0; i < names.size(); ++i)
        if (names[i] == text) return static_cast<E>(i);
    return std::nullopt;
}

// Dart's `byName`: an unknown value is an error, not an absent one.
template <typename E>
std::optional<E> opt_enum(const Json& object, std::string_view key) {
    auto text = opt_string(object, key);
    if (!text) return std::nullopt;
    auto value = try_parse<E>(*text);
    if (!value) throw FormatError("\"" + std::string(key) + "\" has no value \"" + *text + "\"");
    return value;
}

template <typename E>
void put_enum(Json& object, std::string_view key, const std::optional<E>& value) {
    if (value) object[std::string(key)] = std::string(name(*value));
}

} // namespace wl
