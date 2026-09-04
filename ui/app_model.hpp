#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "workoutlog/cycle.hpp"
#include "workoutlog/models.hpp"
#include "workoutlog/muscle_activation.hpp"

// The C++ port of AppModel in app/Sources/WorkoutLogApp/WorkoutLogApp.swift: all
// state plus the calls into workoutlog:: that back it. Zero ImGui here -- every
// screen reads this, none of them own domain state (ADR-004 in
// docs/05-architecture.md).
namespace workoutlog::ui {

class AppModel {
public:
    // Resolves the repo root, loads the catalogue and the muscle-map SVG template
    // (failure degrades to std::nullopt for either, never throws -- matching
    // AppModel.loadAssets()'s `try?`), picks the starting folder ($WORKOUTLOG_DATA
    // if set, else <repo_root>/data) and calls refresh().
    AppModel();

    // Re-lists the folder and rebuilds the calendar/cycle index
    // (workoutlog::cycle::build_index) -- the equivalent of AppModel.refresh().
    void refresh();

    // Loads a session file and makes it the selection. Failure leaves the previous
    // session in place and reports the error in status().
    void open(const std::filesystem::path& file);

    // Writes the current session (SessionStore::save is atomic) and refreshes.
    // Preserves the Swift app's behaviour on purpose: editing date/cycle_day
    // changes the target filename, so this can write a *new* file and leave the
    // old one behind -- the status line names the file that was actually written
    // so that isn't silent.
    void save();

    // Switches the session folder, clears the current selection, and refreshes.
    void set_folder(const std::filesystem::path& folder);

    void set_mode(WeightingMode mode) { mode_ = mode; }
    WeightingMode mode() const { return mode_; }

    const std::filesystem::path& folder() const { return folder_; }
    const std::vector<std::filesystem::path>& files() const { return files_; }
    const std::optional<std::filesystem::path>& selection() const { return selection_; }
    const std::string& status() const { return status_; }
    const cycle::Index& cycle_index() const { return cycle_index_; }
    const std::optional<Catalogue>& catalogue() const { return catalogue_; }
    const std::optional<std::string>& map_template() const { return map_template_; }

    bool has_session() const { return session_.has_value(); }
    // Precondition: has_session(). Callers only reach the session editor after
    // checking that, matching how RootView only shows SessionEditorView when
    // model.session != nil in Views.swift; .value() turns a violation into a
    // clear std::bad_optional_access rather than undefined behaviour (AGENTS.md
    // 2.2 -- never unwrap an optional without first proving it holds a value).
    const Session& session() const { return session_.value(); }
    // Same precondition as session(); the single write accessor every editor binds
    // to. No cache-invalidation bookkeeping needed here: ui::SvgTexture compares
    // the *colorized SVG string* it's given each frame against what it last
    // rasterized, and day_map_svg()/cycle_map_svg() are cheap enough (a linear
    // scan of the ~90KB template, no lunasvg involved) to simply call every frame
    // and let that comparison do the deduplication -- see ui/svg_texture.hpp.
    Session& mutable_session() { return session_.value(); }

    // The three muscle maps, ported from AppModel.dayMapSVG/cycleMapSVG/
    // exerciseMapSVG: std::nullopt when the catalogue or the SVG template failed
    // to load, or (day/cycle) when there's nothing to score yet.
    std::optional<std::string> day_map_svg() const;
    std::optional<std::string> cycle_map_svg() const;
    std::optional<std::string> exercise_map_svg(const std::string& exercise_name) const;

private:
    std::filesystem::path repo_root_;
    std::filesystem::path folder_;
    std::vector<std::filesystem::path> files_;
    std::optional<std::filesystem::path> selection_;
    std::optional<Session> session_;
    std::string status_;
    WeightingMode mode_ = WeightingMode::set_count;

    std::optional<Catalogue> catalogue_;
    std::optional<std::string> map_template_;

    cycle::Index cycle_index_;
};

} // namespace workoutlog::ui
