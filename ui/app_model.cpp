#include "app_model.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "workoutlog/catalogue.hpp"
#include "workoutlog/json.hpp"
#include "workoutlog/muscle_map_svg.hpp"
#include "workoutlog/paths.hpp"
#include "workoutlog/store.hpp"

namespace workoutlog::ui {

namespace {

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

// Swift's defaultFolder() (WorkoutLogApp.swift) expands a leading "~/" by hand
// before handing the path to Foundation; std::filesystem does no such expansion,
// so this is the C++ equivalent of that one step.
std::filesystem::path expand_tilde(std::string_view raw) {
    const bool has_tilde = !raw.empty() && raw.front() == '~' && (raw.size() == 1 || raw[1] == '/');
    if (!has_tilde) return std::filesystem::path(raw);
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') return std::filesystem::path(raw);
    return std::filesystem::path(home) / std::filesystem::path(std::string(raw.substr(raw.size() > 1 ? 2 : 1)));
}

// $WORKOUTLOG_DATA if set, else <repo_root>/data -- matches
// AppModel.defaultFolder() in WorkoutLogApp.swift exactly, just resolved against
// the repo root paths::resolve_repo_root() already found rather than the current
// working directory (which is what makes `WORKOUTLOG_DATA=../data swift run` and
// the C++ build's ad-hoc CWD both work without a magic relative path, AGENTS.md 2.5).
std::filesystem::path default_session_folder(const std::filesystem::path& repo_root) {
    if (const char* data = std::getenv("WORKOUTLOG_DATA"); data != nullptr && *data != '\0') {
        return expand_tilde(data);
    }
    return repo_root / "data";
}

} // namespace

AppModel::AppModel() {
    try {
        repo_root_ = paths::resolve_repo_root();
    } catch (const std::exception& e) {
        // Nothing else can proceed sensibly without a repo root. files_/cycle_index_
        // stay empty and catalogue_/map_template_ stay nullopt; every screen already
        // has an "unavailable" fallback for exactly that state.
        status_ = e.what();
        return;
    }

    try {
        catalogue_ = json::decode_catalogue(read_text_file(paths::catalogue_path(repo_root_)));
    } catch (const std::exception&) {
        catalogue_ = std::nullopt;
    }

    try {
        map_template_ = read_text_file(paths::muscle_map_template_path(repo_root_));
    } catch (const std::exception&) {
        map_template_ = std::nullopt;
    }

    folder_ = default_session_folder(repo_root_);
    refresh();
}

void AppModel::refresh() {
    SessionStore store(folder_);
    files_ = store.list_urls();
    // build_index re-walks the folder itself rather than taking `files_`; a second
    // directory listing on every refresh() is cheap at this corpus size (a few
    // hundred sessions/year, per ADR-001) and keeps cycle::build_index's own
    // contract (it takes a SessionStore, not a file list) the single source of
    // truth for what "the folder" means.
    cycle_index_ = cycle::build_index(store, catalogue_);
}

void AppModel::open(const std::filesystem::path& file) {
    SessionStore store(folder_);
    try {
        session_ = store.load(file);
        selection_ = file;
        status_ = "Loaded " + file.filename().string();
    } catch (const std::exception& e) {
        status_ = std::string("Load failed: ") + e.what();
    }
}

void AppModel::save() {
    if (!session_.has_value()) return;
    SessionStore store(folder_);
    try {
        auto written = store.save(*session_);
        status_ = "Saved " + written.filename().string();
        refresh();
        selection_ = written;
    } catch (const std::exception& e) {
        status_ = std::string("Save failed: ") + e.what();
    }
}

void AppModel::set_folder(const std::filesystem::path& folder) {
    folder_ = folder;
    session_ = std::nullopt;
    selection_ = std::nullopt;
    status_ = "Folder: " + folder.string();
    refresh();
}

std::optional<std::string> AppModel::day_map_svg() const {
    if (!session_.has_value() || !catalogue_.has_value() || !map_template_.has_value()) return std::nullopt;
    auto scores = MuscleActivation().for_session(*session_, *catalogue_, mode_);
    return muscle_map_svg::colorize(*map_template_, scores);
}

std::optional<std::string> AppModel::cycle_map_svg() const {
    if (!catalogue_.has_value() || !map_template_.has_value() || cycle_index_.cycle_sessions.empty())
        return std::nullopt;
    auto scores = MuscleActivation().for_sessions(cycle_index_.cycle_sessions, *catalogue_, mode_);
    return muscle_map_svg::colorize(*map_template_, scores);
}

std::optional<std::string> AppModel::exercise_map_svg(const std::string& exercise_name) const {
    if (!catalogue_.has_value() || !map_template_.has_value()) return std::nullopt;
    const Exercise* ex = catalogue::resolve(*catalogue_, exercise_name);
    if (ex == nullptr) return std::nullopt;
    auto scores = MuscleActivation().for_exercise(*ex);
    return muscle_map_svg::colorize(*map_template_, scores);
}

} // namespace workoutlog::ui
