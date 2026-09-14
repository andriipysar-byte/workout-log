#include "workoutlog/cycle_plan.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>

#include "workoutlog/json.hpp"
#include "workoutlog/store.hpp"

namespace workoutlog::cycle_plan {

std::string to_string(Weekday v) {
    switch (v) {
        case Weekday::mon: return "Mon";
        case Weekday::tue: return "Tue";
        case Weekday::wed: return "Wed";
        case Weekday::thu: return "Thu";
        case Weekday::fri: return "Fri";
        case Weekday::sat: return "Sat";
        case Weekday::sun: return "Sun";
    }
    return {};
}

std::optional<Weekday> weekday_from_string(std::string_view s) {
    if (s == "Mon") return Weekday::mon;
    if (s == "Tue") return Weekday::tue;
    if (s == "Wed") return Weekday::wed;
    if (s == "Thu") return Weekday::thu;
    if (s == "Fri") return Weekday::fri;
    if (s == "Sat") return Weekday::sat;
    if (s == "Sun") return Weekday::sun;
    return std::nullopt;
}

std::string to_string(BlockType v) {
    switch (v) {
        case BlockType::cardio: return "cardio";
        case BlockType::strength: return "strength";
        case BlockType::metcon: return "metcon";
        case BlockType::cooldown: return "cooldown";
    }
    return {};
}

std::optional<BlockType> block_type_from_string(std::string_view s) {
    if (s == "cardio") return BlockType::cardio;
    if (s == "strength") return BlockType::strength;
    if (s == "metcon") return BlockType::metcon;
    if (s == "cooldown") return BlockType::cooldown;
    return std::nullopt;
}

std::string to_string(BlockRole v) {
    switch (v) {
        case BlockRole::warmup: return "warmup";
        case BlockRole::explosive: return "explosive";
        case BlockRole::main: return "main";
        case BlockRole::accessory: return "accessory";
        case BlockRole::grip: return "grip";
        case BlockRole::core: return "core";
        case BlockRole::metcon: return "metcon";
        case BlockRole::cooldown: return "cooldown";
    }
    return {};
}

std::optional<BlockRole> block_role_from_string(std::string_view s) {
    if (s == "warmup") return BlockRole::warmup;
    if (s == "explosive") return BlockRole::explosive;
    if (s == "main") return BlockRole::main;
    if (s == "accessory") return BlockRole::accessory;
    if (s == "grip") return BlockRole::grip;
    if (s == "core") return BlockRole::core;
    if (s == "metcon") return BlockRole::metcon;
    if (s == "cooldown") return BlockRole::cooldown;
    return std::nullopt;
}

std::string to_string(SessionType v) {
    switch (v) {
        case SessionType::metcon: return "metcon";
        case SessionType::heavy: return "heavy";
    }
    return {};
}

std::optional<SessionType> session_type_from_string(std::string_view s) {
    if (s == "metcon") return SessionType::metcon;
    if (s == "heavy") return SessionType::heavy;
    return std::nullopt;
}

namespace {

template <typename T>
void checked_insert(std::vector<T>& v, std::size_t index, T item, const char* what) {
    if (index > v.size()) throw std::out_of_range(std::string(what) + ": insert index " + std::to_string(index) +
                                                    " out of range (size " + std::to_string(v.size()) + ")");
    v.insert(v.begin() + static_cast<std::ptrdiff_t>(index), std::move(item));
}

template <typename T>
void checked_remove(std::vector<T>& v, std::size_t index, const char* what) {
    if (index >= v.size()) throw std::out_of_range(std::string(what) + ": remove index " + std::to_string(index) +
                                                     " out of range (size " + std::to_string(v.size()) + ")");
    v.erase(v.begin() + static_cast<std::ptrdiff_t>(index));
}

template <typename T>
void checked_move(std::vector<T>& v, std::size_t from, std::size_t to, const char* what) {
    if (from >= v.size() || to >= v.size())
        throw std::out_of_range(std::string(what) + ": move index out of range (from " + std::to_string(from) +
                                 ", to " + std::to_string(to) + ", size " + std::to_string(v.size()) + ")");
    if (from == to) return;
    auto first = v.begin() + static_cast<std::ptrdiff_t>(from);
    if (from < to)
        std::rotate(first, first + 1, v.begin() + static_cast<std::ptrdiff_t>(to) + 1);
    else
        std::rotate(v.begin() + static_cast<std::ptrdiff_t>(to), first, first + 1);
}

bool is_valid_cycle_day(const std::string& day) {
    return day.size() == 2 && day[0] >= 'A' && day[0] <= 'F' && (day[1] == '1' || day[1] == '2');
}

} // namespace

void insert_session(Cycle& cycle, std::size_t index, Session session) {
    checked_insert(cycle.sessions, index, std::move(session), "cycle_plan::insert_session");
}

void remove_session(Cycle& cycle, std::size_t index) {
    checked_remove(cycle.sessions, index, "cycle_plan::remove_session");
}

void move_session(Cycle& cycle, std::size_t from, std::size_t to) {
    checked_move(cycle.sessions, from, to, "cycle_plan::move_session");
}

void insert_block(Session& session, std::size_t index, Block block) {
    checked_insert(session.blocks, index, std::move(block), "cycle_plan::insert_block");
}

void remove_block(Session& session, std::size_t index) {
    checked_remove(session.blocks, index, "cycle_plan::remove_block");
}

void move_block(Session& session, std::size_t from, std::size_t to) {
    checked_move(session.blocks, from, to, "cycle_plan::move_block");
}

Cycle* find_cycle(File& file, std::string_view id) {
    for (auto& c : file.cycles)
        if (c.id == id) return &c;
    return nullptr;
}

std::vector<std::string> validate(const File& file) {
    std::vector<std::string> violations;
    std::set<std::string> seen_ids;

    for (const auto& cycle : file.cycles) {
        if (!seen_ids.insert(cycle.id).second) violations.push_back("duplicate cycle id \"" + cycle.id + "\"");

        std::set<std::string> seen_days;
        for (const auto& session : cycle.sessions) {
            if (!is_valid_cycle_day(session.cycle_day))
                violations.push_back("cycle \"" + cycle.id + "\": cycle_day \"" + session.cycle_day +
                                      "\" does not match ^[A-F][12]$");
            if (!seen_days.insert(session.cycle_day).second)
                violations.push_back("cycle \"" + cycle.id + "\": duplicate cycle_day \"" + session.cycle_day + "\"");
        }
    }

    return violations;
}

File CyclePlanStore::load() const { return json::decode_cycle_plan(read_file(path_)); }

void CyclePlanStore::save(const File& file) const {
    auto violations = validate(file);
    if (!violations.empty()) {
        std::ostringstream msg;
        msg << "cycle_plan::CyclePlanStore::save: " << path_.string() << " would be invalid:\n";
        for (const auto& v : violations) msg << "  - " << v << "\n";
        throw std::runtime_error(msg.str());
    }
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    write_file_atomic(path_, json::encode_cycle_plan(file));
}

} // namespace workoutlog::cycle_plan
