#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "workoutlog/models.hpp"

// Port of app/Sources/WorkoutLogCore/IO/SessionStore.swift. Files are the source of
// truth (ADR-001); filename is YYYY-MM-DD_<cycleDay>.json.
namespace workoutlog {

struct LoadFailure {
    std::filesystem::path path;
    std::string error;
};

class SessionStore {
public:
    explicit SessionStore(std::filesystem::path folder) : folder_(std::move(folder)) {}

    // *.json in the folder, sorted by filename -- which is chronological order given
    // the YYYY-MM-DD_<cycleDay>.json convention.
    std::vector<std::filesystem::path> list_urls() const;

    Session load(const std::filesystem::path&) const; // throws std::runtime_error

    struct LoadAllResult {
        std::vector<Session> sessions;
        std::vector<LoadFailure> failed;
    };
    // A corrupted file costs one session, never the archive: parse failures are
    // collected, not thrown.
    LoadAllResult load_all() const;

    std::string filename_for(const Session&) const;
    std::filesystem::path url_for(const Session&) const;
    std::filesystem::path save(const Session&) const; // atomic write; returns the path

    const std::filesystem::path& folder() const { return folder_; }

private:
    std::filesystem::path folder_;
};

} // namespace workoutlog
