#pragma once

#include <map>
#include <string>
#include <vector>

#include "workoutlog/models.hpp"

namespace wl {

// The byte-level backend a SessionStore sits on. Declared here, implemented
// per platform: the seam that keeps the filesystem out of the core (ADR-004).
// An id is a bare filename such as `2026-08-06_D2.json`.
class SessionStorage {
public:
    virtual ~SessionStorage() = default;
    virtual std::vector<std::string> list_ids() const = 0;
    virtual std::string read(const std::string& id) const = 0;
    virtual void write(const std::string& id, const std::string& contents) = 0;
    virtual void remove(const std::string& id) = 0;
    // A human-readable name for the backing location, for a status line.
    virtual std::string label() const = 0;
};

class MemoryStorage final : public SessionStorage {
public:
    explicit MemoryStorage(std::map<std::string, std::string> seed = {}, std::string label = "in-memory")
        : files_(std::move(seed)), label_(std::move(label)) {}

    std::vector<std::string> list_ids() const override;
    std::string read(const std::string& id) const override;
    void write(const std::string& id, const std::string& contents) override { files_[id] = contents; }
    void remove(const std::string& id) override { files_.erase(id); }
    std::string label() const override { return label_; }

    const std::map<std::string, std::string>& snapshot() const { return files_; }

private:
    std::map<std::string, std::string> files_;
    std::string label_;
};

struct LoadFailure {
    std::string id;
    std::string error;
};

struct LoadResult {
    std::vector<Session> sessions;
    std::vector<LoadFailure> failures;
};

// Files are the source of truth (ADR-001). Filename: `YYYY-MM-DD_<cycleDay>.json`.
class SessionStore {
public:
    explicit SessionStore(SessionStorage& storage) : storage_(storage) {}

    SessionStorage& storage() { return storage_; }

    // Sorting and the `.json` filter live here so every backend lists alike.
    std::vector<std::string> list_ids() const;
    Session load(const std::string& id) const;
    // A corrupted file costs one session, never the archive.
    LoadResult load_all() const;
    bool exists(const std::string& id) const;
    // Writes under the derived id and, when the header moved it, removes the old
    // file — otherwise editing the date silently orphans it.
    std::string save(const Session& session, const std::optional<std::string>& previous_id = std::nullopt);
    void remove(const std::string& id) { storage_.remove(id); }

    static std::string id_for(const Session& session);

private:
    SessionStorage& storage_;
};

} // namespace wl
