#include "workoutlog/store.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "workoutlog/json.hpp"

namespace workoutlog {

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void write_file_atomic(const std::filesystem::path& path, const std::string& contents) {
    auto tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot open " + tmp.string() + " for writing");
        out << contents;
    }
    std::error_code ec;
    // std::filesystem::rename can fail to replace an existing file on some platforms
    // (notably Windows); fall back to remove-then-rename.
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        std::filesystem::rename(tmp, path, ec);
        if (ec) throw std::runtime_error("cannot rename " + tmp.string() + " to " + path.string());
    }
}

} // namespace

std::vector<std::filesystem::path> SessionStore::list_urls() const {
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    if (!std::filesystem::exists(folder_, ec)) return out;
    for (const auto& entry : std::filesystem::directory_iterator(folder_, ec)) {
        if (entry.path().extension() == ".json") out.push_back(entry.path());
    }
    std::sort(out.begin(), out.end(),
              [](const auto& a, const auto& b) { return a.filename().string() < b.filename().string(); });
    return out;
}

Session SessionStore::load(const std::filesystem::path& path) const {
    return json::decode_session(read_file(path));
}

SessionStore::LoadAllResult SessionStore::load_all() const {
    LoadAllResult result;
    for (const auto& url : list_urls()) {
        try {
            result.sessions.push_back(load(url));
        } catch (const std::exception& e) {
            result.failed.push_back({url, e.what()});
        }
    }
    return result;
}

std::string SessionStore::filename_for(const Session& s) const { return s.date + "_" + s.cycle_day + ".json"; }

std::filesystem::path SessionStore::url_for(const Session& s) const { return folder_ / filename_for(s); }

std::filesystem::path SessionStore::save(const Session& s) const {
    std::error_code ec;
    std::filesystem::create_directories(folder_, ec);
    auto target = url_for(s);
    write_file_atomic(target, json::encode_session(s));
    return target;
}

} // namespace workoutlog
