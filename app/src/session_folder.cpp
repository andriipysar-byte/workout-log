#include "session_folder.hpp"

#include <algorithm>
#include <cstdlib>

#include "workoutlog/directory_storage.hpp"

namespace fs = std::filesystem;

bool FileReferenceStore::can_write() const {
    std::error_code ec;
    return fs::is_directory(directory_, ec);
}

std::optional<std::string> FileReferenceStore::read(const std::string& name) const {
    fs::path file = directory_ / name;
    std::error_code ec;
    if (!fs::exists(file, ec)) return std::nullopt;
    return wl::read_file(file);
}

void FileReferenceStore::write(const std::string& name, const std::string& contents) {
    fs::path file = directory_ / name;
    fs::create_directories(file.parent_path());
    wl::write_file_atomically(file, contents);
}

std::vector<std::string> FileReferenceStore::list(const std::string& directory) const {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(directory_ / directory, ec))
        if (entry.path().extension() == ".json") out.push_back(directory + "/" + entry.path().filename().string());
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> MemoryReferenceStore::list(const std::string& directory) const {
    std::vector<std::string> out;
    std::string prefix = directory + "/";
    for (const auto& [name, contents] : files_)
        if (name.starts_with(prefix) && name.ends_with(".json")) out.push_back(name);
    return out;
}

std::optional<std::string> MemoryReferenceStore::read(const std::string& name) const {
    auto it = files_.find(name);
    if (it == files_.end()) return std::nullopt;
    return it->second;
}

SessionFolder folder_at(const fs::path& directory) {
    fs::path absolute = fs::weakly_canonical(fs::absolute(directory));
    return {std::make_unique<wl::DirectoryStorage>(absolute),
            std::make_unique<FileReferenceStore>(absolute.parent_path())};
}

fs::path default_session_directory(const std::optional<std::string>& remembered) {
    if (const char* env = std::getenv("WORKOUTLOG_DATA"); env && *env) return wl::expand_tilde(env);
    std::error_code ec;
    if (remembered && fs::is_directory(*remembered, ec)) return *remembered;
    if (auto root = wl::find_repo_root(); root && fs::is_directory(*root / "data", ec)) return *root / "data";
    const char* home = std::getenv("HOME");
    return fs::path(home ? home : ".") / "Documents" / "WorkoutLog";
}
