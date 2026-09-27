#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "workoutlog/session_store.hpp"

// The session folder as a real directory (ADR-007's desktop case). Only byte
// plumbing lives here; every rule about what the bytes mean stays in the core.
namespace wl {

class DirectoryStorage final : public SessionStorage {
public:
    explicit DirectoryStorage(std::filesystem::path directory) : directory_(std::move(directory)) {}

    const std::filesystem::path& directory() const { return directory_; }

    std::vector<std::string> list_ids() const override;
    std::string read(const std::string& id) const override;
    void write(const std::string& id, const std::string& contents) override;
    void remove(const std::string& id) override;
    std::string label() const override { return directory_.string(); }

private:
    std::filesystem::path file(const std::string& id) const;

    std::filesystem::path directory_;
};

std::string read_file(const std::filesystem::path& path);

// Write-then-rename: a crash mid-write leaves the previous file intact rather
// than a truncated one.
void write_file_atomically(const std::filesystem::path& path, const std::string& contents);

// The nearest ancestor of `start` holding cycles.json — the repository root.
std::optional<std::filesystem::path> find_repo_root(std::filesystem::path start = std::filesystem::current_path());

std::filesystem::path expand_tilde(const std::string& path);

} // namespace wl
