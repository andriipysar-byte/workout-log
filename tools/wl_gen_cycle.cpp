// Generates dated session-stub files from a cycle in cycles.json.
//
// Usage: wl_gen_cycle [--cycle <id>] [--version <n>] [--out <dir>] [--repo <dir>] [--force]

#include <filesystem>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

#include "workoutlog/cycle.hpp"
#include "workoutlog/cycle_planning.hpp"
#include "workoutlog/directory_storage.hpp"
#include "workoutlog/session_store.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kUsage = "usage: wl_gen_cycle [--cycle <id>] [--version <n>] [--out <dir>] [--repo <dir>] [--force]";

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "error: " << message << "\n";
    std::exit(2);
}

} // namespace

int main(int argc, char** argv) {
    std::string cycle_id = "hybrid-8";
    std::optional<std::int64_t> version;
    std::string out_dir_name = "data";
    fs::path repo = wl::find_repo_root().value_or(fs::current_path());
    bool force = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const std::string& flag) -> std::string {
            if (i + 1 >= argc) fail("missing value for " + flag);
            return argv[++i];
        };
        if (arg == "--force") {
            force = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << kUsage << "\n";
            return 0;
        } else if (arg == "--cycle") {
            cycle_id = next(arg);
        } else if (arg == "--version") {
            std::string value = next(arg);
            try {
                version = std::stoll(value);
            } catch (const std::exception&) {
                fail("--version expects a number, got \"" + value + "\"");
            }
        } else if (arg == "--out") {
            out_dir_name = next(arg);
        } else if (arg == "--repo") {
            repo = next(arg);
        } else {
            fail("unknown argument \"" + arg + "\"\n" + kUsage);
        }
    }

    fs::path cycles_file = repo / "cycles.json";
    if (!fs::exists(cycles_file)) fail("no cycles.json under " + repo.string() + " (pass --repo <dir>)");

    try {
        auto catalogue = wl::decode_cycles(wl::read_file(cycles_file));
        const wl::Cycle* cycle = version ? catalogue.by_id(cycle_id, *version) : catalogue.by_id(cycle_id);
        if (!cycle) {
            std::string wanted = cycle_id + (version ? " v" + std::to_string(*version) : "");
            fail("cycle \"" + wanted + "\" not found in " + cycles_file.string());
        }

        fs::path out_dir = repo / out_dir_name;
        fs::create_directories(out_dir);
        for (const auto& session : wl::generate_cycle(*cycle)) {
            std::string name = wl::SessionStore::id_for(session);
            fs::path file = out_dir / name;
            if (fs::exists(file) && !force) {
                std::cout << "skip (exists): " << name << "  — pass --force to overwrite\n";
                continue;
            }
            wl::write_file_atomically(file, wl::encode_session(session));
            std::cout << "wrote " << name << "\n";
        }
    } catch (const std::exception& e) {
        fail(e.what());
    }
    return 0;
}
