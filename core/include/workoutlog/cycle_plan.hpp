#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "workoutlog/models.hpp"

// The planning-template side of cycles.json/cycles.schema.json -- an authorable
// cycle *definition* (weightless session stubs, later expanded onto the calendar by
// scripts/generate_cycle.py). Not to be confused with workoutlog::cycle
// (core/include/workoutlog/cycle.hpp), which derives a read-only coverage matrix from
// already-logged sessions in data/; the two "cycle" concepts share a name in the docs
// but nothing else, so they live in separate namespaces.
namespace workoutlog::cycle_plan {

enum class Weekday { mon, tue, wed, thu, fri, sat, sun };
enum class BlockType { cardio, strength, metcon, cooldown };
enum class BlockRole { warmup, explosive, main, accessory, grip, core, metcon, cooldown };
enum class SessionType { metcon, heavy };

std::string to_string(Weekday);
std::string to_string(BlockType);
std::string to_string(BlockRole);
std::string to_string(SessionType);

std::optional<Weekday> weekday_from_string(std::string_view);
std::optional<BlockType> block_type_from_string(std::string_view);
std::optional<BlockRole> block_role_from_string(std::string_view);
std::optional<SessionType> session_type_from_string(std::string_view);

struct BlockExercise {
    std::string name;
    std::optional<double> weight_kg;

    bool operator==(const BlockExercise&) const = default;
};

// One flat shape for every role, matching cycles.schema.json's "block" -- unlike
// workoutlog::Block (models.hpp), which is a tagged union because a logged session's
// blocks carry incompatible payloads (sets of real WorkSets vs. metcon rounds). A
// template block is just planning metadata: which optional fields are meaningful
// follows from type/role, but the schema itself never forbids the others.
struct Block {
    BlockType type = BlockType::cardio;
    BlockRole role = BlockRole::warmup;
    std::optional<std::string> machine;
    std::optional<double> duration_min;
    std::optional<std::string> exercise;
    std::optional<std::vector<int>> sets_reps;
    std::optional<MetconFormat> format;
    std::optional<std::vector<int>> scheme;
    std::optional<std::vector<BlockExercise>> exercises;
    std::optional<std::string> notes;

    bool operator==(const Block&) const = default;
};

struct Session {
    std::string cycle_day;
    std::optional<int> week;
    std::optional<Weekday> weekday;
    SessionType type = SessionType::heavy;
    std::optional<std::string> title;
    std::optional<std::string> session_notes;
    std::vector<Block> blocks;

    bool operator==(const Session&) const = default;
};

struct Skip {
    std::optional<int> week;
    std::optional<Weekday> weekday;
    std::string reason;

    bool operator==(const Skip&) const = default;
};

struct Cycle {
    std::string id;
    std::string name;
    std::optional<std::vector<Weekday>> training_days;
    std::optional<std::string> start_date;
    std::vector<Session> sessions;
    std::optional<std::vector<Skip>> skipped;

    bool operator==(const Cycle&) const = default;
};

struct File {
    std::optional<std::string> comment; // top-level "$comment"
    std::vector<Cycle> cycles;

    bool operator==(const File&) const = default;
};

// Bounds-checked structural edits, shared by every caller that mutates a cycle (the
// wl_cycle CLI, both UI editors) so index arithmetic isn't reimplemented at each call
// site. Each throws std::out_of_range naming the operation and the bad index --
// every index here can originate from external input (a CLI argument, a UI drag).
void insert_session(Cycle& cycle, std::size_t index, Session session);
void remove_session(Cycle& cycle, std::size_t index);
void move_session(Cycle& cycle, std::size_t from, std::size_t to);

void insert_block(Session& session, std::size_t index, Block block);
void remove_block(Session& session, std::size_t index);
void move_block(Session& session, std::size_t from, std::size_t to);

// Checks the invariant cycles.schema.json's session.cycle_day pattern (^[A-F][12]$)
// implies, plus cycle-id and cycle_day uniqueness across a cycle -- not in the
// schema, but a duplicate would silently overwrite a slot the next time
// scripts/generate_cycle.py expands the cycle onto the calendar. Empty return means
// valid; every entry is one human-readable violation.
std::vector<std::string> validate(const File& file);

// Loads/saves the whole cycles.json file atomically. save() throws
// std::runtime_error (validate()'s messages joined) rather than writing an invalid
// file -- the destination is regenerated into real dated stubs by
// scripts/generate_cycle.py, so a bad write there is worse than a rejected edit.
class CyclePlanStore {
public:
    explicit CyclePlanStore(std::filesystem::path path) : path_(std::move(path)) {}

    File load() const; // throws std::runtime_error
    void save(const File&) const; // throws std::runtime_error on validation failure

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace workoutlog::cycle_plan
