#include "workoutlog/directory_storage.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace wl {

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.string());
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void write_file_atomically(const fs::path& path, const std::string& contents) {
    fs::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot write " + temporary.string());
        out << contents;
        out.flush();
        if (!out) throw std::runtime_error("cannot write " + temporary.string());
    }
    fs::rename(temporary, path);
}

std::vector<std::string> DirectoryStorage::list_ids() const {
    std::vector<std::string> ids;
    std::error_code ec;
    if (!fs::is_directory(directory_, ec)) return ids;
    for (const auto& entry : fs::directory_iterator(directory_))
        if (entry.is_regular_file()) ids.push_back(entry.path().filename().string());
    return ids;
}

std::string DirectoryStorage::read(const std::string& id) const {
    return read_file(file(id));
}

void DirectoryStorage::write(const std::string& id, const std::string& contents) {
    fs::create_directories(directory_);
    write_file_atomically(file(id), contents);
}

void DirectoryStorage::remove(const std::string& id) {
    fs::remove(file(id));
}

fs::path DirectoryStorage::file(const std::string& id) const {
    if (id.find('/') != std::string::npos || id.find("..") != std::string::npos)
        throw std::invalid_argument("\"" + id + "\" must be a bare file name");
    return directory_ / id;
}

std::optional<fs::path> find_repo_root(const fs::path& start) {
    fs::path dir = fs::absolute(start);
    while (true) {
        if (fs::exists(dir / "cycles.json")) return dir;
        if (dir == dir.parent_path()) return std::nullopt;
        dir = dir.parent_path();
    }
}

fs::path expand_tilde(const std::string& path) {
    if (path.starts_with("~/"))
        if (const char* home = std::getenv("HOME")) return fs::path(home) / path.substr(2);
    return path;
}

} // namespace wl
