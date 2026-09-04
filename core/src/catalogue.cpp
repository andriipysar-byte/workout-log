#include "workoutlog/catalogue.hpp"

#include "workoutlog/utf8.hpp"

namespace workoutlog::catalogue {

const Exercise* resolve(const Catalogue& cat, std::string_view typed) {
    const std::string key = utf8::to_lower(utf8::trim(typed));
    for (const auto& ex : cat.exercises) {
        if (utf8::to_lower(ex.name) == key) return &ex;
        for (const auto& alias : ex.aliases) {
            if (utf8::to_lower(alias) == key) return &ex;
        }
    }
    return nullptr;
}

} // namespace workoutlog::catalogue
