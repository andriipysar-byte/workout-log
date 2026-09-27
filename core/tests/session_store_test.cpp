#include <doctest/doctest.h>

#include "workoutlog/session_store.hpp"

using namespace wl;

// The Dart suite's temp-directory case lives with the filesystem backend in storage/.
namespace {

Session sample() {
    Session session{.date = "2026-07-21", .cycle_day = "A1"};
    session.blocks.push_back(CardioBlock{.machine = "гребля", .duration_min = 10.0});
    StrengthBlock strength{.exercise = "присід фронтальний"};
    strength.sets.push_back(WorkSet{.weight_kg = 70.0, .reps = 6});
    session.blocks.push_back(strength);
    return session;
}

using Ids = std::vector<std::string>;

} // namespace

TEST_CASE("the id follows YYYY-MM-DD_<cycleDay>.json") {
    CHECK(SessionStore::id_for(sample()) == "2026-07-21_A1.json");
}

TEST_CASE("save then reload is identical") {
    MemoryStorage storage;
    SessionStore store(storage);
    auto id = store.save(sample());
    CHECK(store.load(id) == sample());
}

TEST_CASE("listing is sorted and ignores non-JSON files") {
    MemoryStorage storage({{"b.json", "{}"}, {"a.json", "{}"}, {"notes.txt", "x"}});
    CHECK(SessionStore(storage).list_ids() == Ids{"a.json", "b.json"});
}

TEST_CASE("a corrupted file costs one session, never the archive") {
    MemoryStorage storage({{"good.json", encode_session(sample())}, {"bad.json", "{ not json"}});
    auto result = SessionStore(storage).load_all();
    CHECK(result.sessions.size() == 1);
    REQUIRE(result.failures.size() == 1);
    CHECK(result.failures[0].id == "bad.json");
}

TEST_CASE("saving after a header edit removes the file it moved from") {
    MemoryStorage storage;
    SessionStore store(storage);
    auto original = store.save(sample());

    auto moved = sample();
    moved.date = "2026-07-22";
    auto renamed = store.save(moved, original);

    CHECK(renamed == "2026-07-22_A1.json");
    CHECK(store.list_ids() == Ids{renamed});
}

TEST_CASE("saving without a header edit keeps the single file") {
    MemoryStorage storage;
    SessionStore store(storage);
    auto id = store.save(sample());
    CHECK(store.save(sample(), id) == id);
    CHECK(store.list_ids() == Ids{id});
}

TEST_CASE("delete removes just that session") {
    MemoryStorage storage;
    SessionStore store(storage);
    auto id = store.save(sample());
    auto other = sample();
    other.cycle_day = "A2";
    store.save(other);
    store.remove(id);
    CHECK(store.list_ids() == Ids{"2026-07-21_A2.json"});
}

TEST_CASE("exists reports collisions before a create overwrites one") {
    MemoryStorage storage;
    SessionStore store(storage);
    CHECK_FALSE(store.exists("2026-07-21_A1.json"));
    store.save(sample());
    CHECK(store.exists("2026-07-21_A1.json"));
}
