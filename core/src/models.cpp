#include "workoutlog/models.hpp"

namespace workoutlog {

std::string to_string(Kind v) {
    switch (v) {
        case Kind::training: return "training";
        case Kind::deload: return "deload";
        case Kind::retest: return "retest";
    }
    return {};
}

std::optional<Kind> kind_from_string(std::string_view s) {
    if (s == "training") return Kind::training;
    if (s == "deload") return Kind::deload;
    if (s == "retest") return Kind::retest;
    return std::nullopt;
}

std::string to_string(RepBand v) {
    switch (v) {
        case RepBand::heavy: return "heavy";
        case RepBand::base: return "base";
        case RepBand::volume: return "volume";
    }
    return {};
}

std::optional<RepBand> rep_band_from_string(std::string_view s) {
    if (s == "heavy") return RepBand::heavy;
    if (s == "base") return RepBand::base;
    if (s == "volume") return RepBand::volume;
    return std::nullopt;
}

std::string to_string(BarSpeed v) {
    switch (v) {
        case BarSpeed::fast: return "fast";
        case BarSpeed::ok: return "ok";
        case BarSpeed::slow: return "slow";
        case BarSpeed::grind: return "grind";
    }
    return {};
}

std::optional<BarSpeed> bar_speed_from_string(std::string_view s) {
    if (s == "fast") return BarSpeed::fast;
    if (s == "ok") return BarSpeed::ok;
    if (s == "slow") return BarSpeed::slow;
    if (s == "grind") return BarSpeed::grind;
    return std::nullopt;
}

std::string to_string(MetconFormat v) {
    switch (v) {
        case MetconFormat::for_time: return "for_time";
        case MetconFormat::amrap: return "amrap";
        case MetconFormat::emom: return "emom";
        case MetconFormat::intervals: return "intervals";
        case MetconFormat::ladder: return "ladder";
        case MetconFormat::chipper: return "chipper";
    }
    return {};
}

std::optional<MetconFormat> metcon_format_from_string(std::string_view s) {
    if (s == "for_time") return MetconFormat::for_time;
    if (s == "amrap") return MetconFormat::amrap;
    if (s == "emom") return MetconFormat::emom;
    if (s == "intervals") return MetconFormat::intervals;
    if (s == "ladder") return MetconFormat::ladder;
    if (s == "chipper") return MetconFormat::chipper;
    return std::nullopt;
}

std::string to_string(ExerciseCategory v) {
    switch (v) {
        case ExerciseCategory::strength: return "strength";
        case ExerciseCategory::power: return "power";
        case ExerciseCategory::speed: return "speed";
        case ExerciseCategory::longevity: return "longevity";
    }
    return {};
}

std::optional<ExerciseCategory> exercise_category_from_string(std::string_view s) {
    if (s == "strength") return ExerciseCategory::strength;
    if (s == "power") return ExerciseCategory::power;
    if (s == "speed") return ExerciseCategory::speed;
    if (s == "longevity") return ExerciseCategory::longevity;
    return std::nullopt;
}

std::string to_string(Pattern v) {
    switch (v) {
        case Pattern::squat: return "squat";
        case Pattern::hinge: return "hinge";
        case Pattern::press: return "press";
        case Pattern::pull: return "pull";
        case Pattern::olympic: return "olympic";
        case Pattern::carry: return "carry";
        case Pattern::core: return "core";
        case Pattern::grip: return "grip";
    }
    return {};
}

std::optional<Pattern> pattern_from_string(std::string_view s) {
    if (s == "squat") return Pattern::squat;
    if (s == "hinge") return Pattern::hinge;
    if (s == "press") return Pattern::press;
    if (s == "pull") return Pattern::pull;
    if (s == "olympic") return Pattern::olympic;
    if (s == "carry") return Pattern::carry;
    if (s == "core") return Pattern::core;
    if (s == "grip") return Pattern::grip;
    return std::nullopt;
}

std::string to_string(Modality v) {
    switch (v) {
        case Modality::barbell: return "barbell";
        case Modality::dumbbell: return "dumbbell";
        case Modality::kettlebell: return "kettlebell";
        case Modality::bodyweight: return "bodyweight";
        case Modality::machine: return "machine";
    }
    return {};
}

std::optional<Modality> modality_from_string(std::string_view s) {
    if (s == "barbell") return Modality::barbell;
    if (s == "dumbbell") return Modality::dumbbell;
    if (s == "kettlebell") return Modality::kettlebell;
    if (s == "bodyweight") return Modality::bodyweight;
    if (s == "machine") return Modality::machine;
    return std::nullopt;
}

} // namespace workoutlog
