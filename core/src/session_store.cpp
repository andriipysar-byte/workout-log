#include "workoutlog/session_store.hpp"

#include <algorithm>
#include <stdexcept>

namespace wl {

std::vector<std::string> MemoryStorage::list_ids() const {
    std::vector<std::string> ids;
    for (const auto& [id, _] : files_) ids.push_back(id);
    return ids;
}

std::string MemoryStorage::read(const std::string& id) const {
    auto it = files_.find(id);
    if (it == files_.end()) throw std::runtime_error("no such session: " + id);
    return it->second;
}

std::vector<std::string> SessionStore::list_ids() const {
    std::vector<std::string> ids;
    for (auto& id : storage_.list_ids())
        if (id.size() >= 5 && id.ends_with(".json")) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

Session SessionStore::load(const std::string& id) const {
    return decode_session(storage_.read(id));
}

LoadResult SessionStore::load_all() const {
    LoadResult result;
    for (const auto& id : list_ids()) {
        try {
            result.sessions.push_back(load(id));
        } catch (const std::exception& e) {
            result.failures.push_back({id, e.what()});
        }
    }
    return result;
}

bool SessionStore::exists(const std::string& id) const {
    auto ids = list_ids();
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

std::string SessionStore::save(const Session& session, const std::optional<std::string>& previous_id) {
    std::string id = id_for(session);
    storage_.write(id, encode_session(session));
    if (previous_id && *previous_id != id) storage_.remove(*previous_id);
    return id;
}

std::string SessionStore::id_for(const Session& session) {
    return session.date + "_" + session.cycle_day + ".json";
}

} // namespace wl
