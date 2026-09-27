#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "workoutlog/catalogue.hpp"
#include "workoutlog/cycle.hpp"
#include "workoutlog/models.hpp"

// Tests read the real archive, catalogue and cycles — a golden set that is also
// the thing a regression would damage.
namespace fixtures {

inline std::filesystem::path repo() { return WL_REPO_ROOT; }

inline std::string read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline std::vector<std::filesystem::path> session_files() {
    std::vector<std::filesystem::path> out;
    for (const auto& e : std::filesystem::directory_iterator(repo() / "data"))
        if (e.path().extension() == ".json") out.push_back(e.path());
    std::sort(out.begin(), out.end());
    return out;
}

inline std::vector<wl::Session> sessions() {
    std::vector<wl::Session> out;
    for (const auto& p : session_files()) out.push_back(wl::decode_session(read(p)));
    return out;
}

inline wl::Catalogue catalogue() { return wl::decode_catalogue(read(repo() / "exercises.json")); }
inline wl::CycleCatalogue cycles() { return wl::decode_cycles(read(repo() / "cycles.json")); }
inline std::string svg_template() { return read(repo() / "app" / "resources" / "muscle-map.svg"); }

} // namespace fixtures
