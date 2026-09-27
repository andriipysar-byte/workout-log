#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>

#include "workoutlog/directory_storage.hpp"

namespace fs = std::filesystem;
using namespace wl;

namespace {

struct TempDir {
    fs::path path = fs::temp_directory_path() / ("wlfs-" + std::to_string(std::rand()));
    TempDir() { fs::create_directories(path); }
    ~TempDir() { fs::remove_all(path); }
};

Session sample(std::string date) {
    Session s{.date = std::move(date), .cycle_day = "A1"};
    s.blocks.push_back(CooldownBlock{});
    return s;
}

} // namespace

TEST_CASE("a directory store saves, lists, loads and deletes") {
    TempDir dir;
    DirectoryStorage storage(dir.path / "data");
    SessionStore store(storage);
    CHECK(store.list_ids().empty());

    auto id = store.save(sample("2026-01-01"));
    CHECK(id == "2026-01-01_A1.json");
    CHECK(store.list_ids() == std::vector<std::string>{id});
    CHECK(store.load(id) == sample("2026-01-01"));
    CHECK_FALSE(fs::exists(dir.path / "data" / (id + ".tmp")));

    store.remove(id);
    CHECK(store.list_ids().empty());
}

TEST_CASE("moving a session's date renames its file instead of orphaning it") {
    TempDir dir;
    DirectoryStorage storage(dir.path);
    SessionStore store(storage);
    auto old_id = store.save(sample("2026-01-01"));
    auto new_id = store.save(sample("2026-01-02"), old_id);
    CHECK(store.list_ids() == std::vector<std::string>{new_id});
}

TEST_CASE("an id that escapes the folder is refused") {
    TempDir dir;
    DirectoryStorage storage(dir.path);
    CHECK_THROWS(storage.write("../escape.json", "{}"));
    CHECK_THROWS(storage.read("a/b.json"));
}

TEST_CASE("non-json files are not sessions, and unreadable ones are reported") {
    TempDir dir;
    DirectoryStorage storage(dir.path);
    storage.write("notes.txt", "hello");
    storage.write("2026-01-01_A1.json", "{ not json");
    SessionStore store(storage);
    auto result = store.load_all();
    CHECK(result.sessions.empty());
    REQUIRE(result.failures.size() == 1);
    CHECK(result.failures[0].id == "2026-01-01_A1.json");
}

TEST_CASE("the repo root is the nearest ancestor with cycles.json") {
    TempDir dir;
    fs::create_directories(dir.path / "a" / "b");
    write_file_atomically(dir.path / "cycles.json", "{}");
    auto root = find_repo_root(dir.path / "a" / "b");
    REQUIRE(root);
    CHECK(fs::equivalent(*root, dir.path));
}
