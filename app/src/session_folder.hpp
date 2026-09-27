#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "workoutlog/session_store.hpp"

// exercises.json and cycles.json live one level above the session folder —
// the repository layout, `<repo>/data/*.json` beside `<repo>/*.json`.
class ReferenceStore {
public:
    virtual ~ReferenceStore() = default;
    virtual bool can_write() const = 0;
    virtual std::optional<std::string> read(const std::string& name) const = 0;
    virtual void write(const std::string& name, const std::string& contents) = 0;
    virtual std::string label() const = 0;
};

class FileReferenceStore final : public ReferenceStore {
public:
    explicit FileReferenceStore(std::filesystem::path directory) : directory_(std::move(directory)) {}
    bool can_write() const override;
    std::optional<std::string> read(const std::string& name) const override;
    void write(const std::string& name, const std::string& contents) override;
    std::string label() const override { return directory_.string(); }

private:
    std::filesystem::path directory_;
};

class MemoryReferenceStore final : public ReferenceStore {
public:
    explicit MemoryReferenceStore(std::map<std::string, std::string> files = {}, bool writable = true)
        : files_(std::move(files)), writable_(writable) {}
    bool can_write() const override { return writable_; }
    std::optional<std::string> read(const std::string& name) const override;
    void write(const std::string& name, const std::string& contents) override { files_[name] = contents; }
    std::string label() const override { return "in-memory"; }
    const std::map<std::string, std::string>& files() const { return files_; }

private:
    std::map<std::string, std::string> files_;
    bool writable_;
};

struct SessionFolder {
    std::unique_ptr<wl::SessionStorage> storage;
    std::unique_ptr<ReferenceStore> references;
};

SessionFolder folder_at(const std::filesystem::path& directory);

// Desktop resolution order (ADR-007): $WORKOUTLOG_DATA, the folder chosen last
// time if it still exists, data/ in the repository around the working
// directory, then ~/Documents/WorkoutLog.
std::filesystem::path default_session_directory(const std::optional<std::string>& remembered);
