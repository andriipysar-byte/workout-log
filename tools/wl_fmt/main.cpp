// Canonical JSON reformatter: 2-space indent, sorted keys, raw UTF-8, unescaped
// slashes, integral doubles as integers, trailing newline (see core/src/json.cpp).
// Run once over data/*.json, exercises.json and cycles.json as its own commit with no
// behaviour change (see docs/05-architecture.md, ADR-007) -- after that, wl_verify's
// write(parse(f)) == bytes(f) check holds exactly.
// Usage: wl_fmt [--check] <path.json...>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "workoutlog/json.hpp"

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void write_file(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot open " + path.string() + " for writing");
    out << contents;
}

} // namespace

int main(int argc, char** argv) {
    bool check_only = false;
    std::vector<std::filesystem::path> paths;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--check")
            check_only = true;
        else
            paths.emplace_back(std::move(arg));
    }
    if (paths.empty()) {
        std::cerr << "usage: wl_fmt [--check] <path.json...>\n";
        return 2;
    }

    bool any_changed = false;
    int errors = 0;
    for (const auto& path : paths) {
        try {
            auto original = read_file(path);
            auto canonical = workoutlog::json::canonicalize(original);
            if (canonical != original) {
                any_changed = true;
                if (check_only) {
                    std::cout << "would reformat: " << path.string() << "\n";
                } else {
                    write_file(path, canonical);
                    std::cout << "reformatted: " << path.string() << "\n";
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "error: " << path.string() << ": " << e.what() << "\n";
            errors++;
        }
    }

    if (errors > 0) return 1;
    if (check_only && any_changed) return 1;
    return 0;
}
