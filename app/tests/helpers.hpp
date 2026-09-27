#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "app_model.hpp"
#include "session_folder.hpp"
#include "workoutlog/session_store.hpp"

namespace helpers {

inline std::filesystem::path repo() { return WL_REPO_ROOT; }

inline std::string read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline std::map<std::string, std::string> real_sessions() {
    std::map<std::string, std::string> out;
    for (const auto& e : std::filesystem::directory_iterator(repo() / "data"))
        if (e.path().extension() == ".json") out[e.path().filename().string()] = read(e.path());
    return out;
}

inline std::map<std::string, std::string> real_references() {
    return {{"exercises.json", read(repo() / "exercises.json")}, {"cycles.json", read(repo() / "cycles.json")}};
}

// A model over memory stores, so nothing a test does can touch data/.
struct MemoryApp {
    wl::MemoryStorage* storage;
    MemoryReferenceStore* references;
    AppModel model;

    explicit MemoryApp(std::map<std::string, std::string> sessions = {}, bool writable = true)
        : MemoryApp(std::move(sessions), real_references(), writable) {}

    MemoryApp(std::map<std::string, std::string> sessions, std::map<std::string, std::string> refs, bool writable) {
        auto s = std::make_unique<wl::MemoryStorage>(std::move(sessions));
        auto r = std::make_unique<MemoryReferenceStore>(std::move(refs), writable);
        storage = s.get();
        references = r.get();
        model.start({std::move(s), std::move(r)});
    }
};

inline wl::Session sample(std::string date = "2026-08-06", std::string day = "D2") {
    wl::Session s{.date = std::move(date), .cycle_day = std::move(day)};
    s.blocks.push_back(wl::StrengthBlock{.exercise = "присід фронтальний", .sets = {{.weight_kg = 70.0, .reps = 6}}});
    return s;
}

} // namespace helpers
