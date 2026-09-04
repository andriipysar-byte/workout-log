#include "workoutlog/paths.hpp"

#include <cstdlib>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace workoutlog::paths {

namespace {

bool has_marker(const std::filesystem::path& dir) {
    std::error_code ec;
    bool exercises = std::filesystem::is_regular_file(dir / "exercises.json", ec);
    bool data_dir = std::filesystem::is_directory(dir / "data", ec);
    return exercises && data_dir;
}

std::optional<std::filesystem::path> find_marker_upward(const std::filesystem::path& start) {
    std::error_code ec;
    auto dir = std::filesystem::weakly_canonical(start, ec);
    if (ec) return std::nullopt;
    while (true) {
        if (has_marker(dir)) return dir;
        auto parent = dir.parent_path();
        if (parent == dir) return std::nullopt; // reached filesystem root
        dir = parent;
    }
}

std::optional<std::filesystem::path> executable_path() {
#if defined(__linux__)
    std::error_code ec;
    auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) return p;
#endif
    return std::nullopt;
}

} // namespace

std::filesystem::path resolve_repo_root(const std::string& hint) {
    std::vector<std::string> attempts;

    if (!hint.empty()) {
        if (has_marker(hint)) return std::filesystem::path(hint);
        attempts.push_back("hint: " + hint);
    }

    if (const char* root = std::getenv("WORKOUTLOG_ROOT")) {
        if (has_marker(root)) return std::filesystem::path(root);
        attempts.push_back(std::string("$WORKOUTLOG_ROOT: ") + root);
    }

    if (const char* data = std::getenv("WORKOUTLOG_DATA")) {
        auto parent = std::filesystem::path(data).parent_path();
        if (has_marker(parent)) return parent;
        attempts.push_back("parent of $WORKOUTLOG_DATA: " + parent.string());
    }

    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (!ec) {
        if (auto found = find_marker_upward(cwd)) return *found;
        attempts.push_back("walk up from cwd: " + cwd.string());
    }

    if (auto exe = executable_path()) {
        if (auto found = find_marker_upward(exe->parent_path())) return *found;
        attempts.push_back("walk up from executable: " + exe->string());
    }

    std::ostringstream msg;
    msg << "could not resolve the WorkoutLog2 repo root (looking for exercises.json + data/). Tried:\n";
    for (const auto& a : attempts) msg << "  - " << a << "\n";
    throw std::runtime_error(msg.str());
}

} // namespace workoutlog::paths
