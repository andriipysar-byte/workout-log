// Rewrites every session file in canonical form: sorted keys, two-space indent,
// no nulls, integral weights without `.0`, trailing newline.
//
// Decoding before encoding means this cannot change what a file means — only
// how it is spelled. Run it after hand-editing so the app's next save produces
// no incidental diff.
//
// Usage: wl_fmt [--check] [<dir>]   (default: <repo>/data)

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "workoutlog/directory_storage.hpp"
#include "workoutlog/models.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    bool check_only = false;
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--check")
            check_only = true;
        else if (!arg.starts_with("--"))
            positional.push_back(arg);
    }

    fs::path dir;
    if (!positional.empty()) {
        dir = positional.front();
    } else if (auto root = wl::find_repo_root()) {
        dir = *root / "data";
    } else {
        std::cerr << "error: no <dir> given and no cycles.json above the working directory\n";
        return 2;
    }
    if (!fs::is_directory(dir)) {
        std::cerr << "error: no such directory: " << dir.string() << "\n";
        return 2;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir))
        if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(entry.path());
    std::sort(files.begin(), files.end());

    int changed = 0;
    for (const auto& file : files) {
        std::string name = file.filename().string();
        std::string original = wl::read_file(file);
        std::string canonical;
        try {
            canonical = wl::encode_session(wl::decode_session(original));
        } catch (const std::exception& e) {
            std::cerr << "error: " << name << ": " << e.what() << "\n";
            return 2;
        }
        if (canonical == original) continue;
        ++changed;
        if (check_only) {
            std::cout << "would reformat " << name << "\n";
            continue;
        }
        wl::write_file_atomically(file, canonical);
        std::cout << "reformatted " << name << "\n";
    }

    if (changed == 0)
        std::cout << "all files already canonical\n";
    else
        std::cout << changed << " file(s)\n";
    return check_only && changed > 0 ? 1 : 0;
}
