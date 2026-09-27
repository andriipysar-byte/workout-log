#pragma once

#include <QObject>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "session_folder.hpp"
#include "workoutlog/catalogue.hpp"
#include "workoutlog/cycle.hpp"
#include "workoutlog/cycle_planning.hpp"
#include "workoutlog/models.hpp"
#include "workoutlog/muscles.hpp"
#include "workoutlog/session_store.hpp"

struct DayInfo {
    std::string id;
    std::string date;
    std::string cycle_day;
    bool is_metcon = false;
    std::optional<wl::MuscleGroup> group;
};

// Which exercises appear on which cycle day: presence only, no load.
struct CycleMatrix {
    std::vector<std::string> days;
    std::vector<std::string> exercises;
    std::vector<std::vector<bool>> cells;                  // [exercise][day]
    std::vector<std::vector<wl::MuscleGroup>> groups;     // [exercise] primary groups
    bool empty() const { return days.empty(); }
};

// All domain work is delegated to the core (ADR-004): no parsing, validation or
// analytics here, only state and the calls that change it.
class AppModel : public QObject {
    Q_OBJECT

public:
    explicit AppModel(QObject* parent = nullptr);

    // `folder` is injected so tests can run against memory stores.
    void start(SessionFolder folder);
    void open_folder(SessionFolder folder);

    const std::vector<std::string>& files() const { return files_; }
    const std::optional<std::string>& selection() const { return selection_; }
    wl::Session* session() { return session_ ? &*session_ : nullptr; }
    // Bumped whenever a different document is loaded into `session()`, so views
    // bound to its fields know to rebuild rather than edit a replaced object.
    unsigned session_generation() const { return session_generation_; }
    const std::string& status() const { return status_; }
    std::string folder_label() const;
    bool ready() const { return ready_; }
    wl::WeightingMode mode() const { return mode_; }
    const std::map<std::string, DayInfo>& calendar() const { return calendar_; }
    const CycleMatrix& cycle_matrix() const { return cycle_; }
    const std::vector<wl::Session>& cycle_sessions() const { return cycle_sessions_; }
    const wl::Catalogue* catalogue() const { return catalogue_ ? &*catalogue_ : nullptr; }
    std::vector<wl::Cycle>& cycles() { return cycles_.cycles; }
    bool can_edit_plan() const;

    std::vector<wl::MuscleGroup> primary_groups(const std::string& exercise) const;

    std::optional<std::string> day_map_svg();
    std::optional<std::string> cycle_map_svg();
    std::optional<std::string> exercise_map_svg(const std::string& name);
    // A plan carries no weights, so its maps are always weighted by set count:
    // tonnage would read zero and rep volume would miss undecided lifts.
    std::optional<std::string> plan_map_svg(const wl::CycleSession& workout) const;
    std::optional<std::string> plan_cycle_map_svg(const wl::Cycle& cycle) const;
    std::optional<wl::MuscleGroup> plan_dominant_group(const wl::CycleSession& workout) const;

    void set_mode(wl::WeightingMode mode);
    // The editor mutates the session in place; this marks what derives from it stale.
    void session_edited();

    void refresh(bool from_disk = false);
    void open(const std::string& id);
    void save();
    void create(wl::Session created);
    void remove(const std::string& id);
    bool exists(const std::string& id) const;
    std::string read_raw(const std::string& id) const;

    // Plan edits stay in memory until save_cycles(), so an abandoned edit costs nothing.
    void cycle_edited();
    size_t create_cycle(const std::string& id, const std::string& name);
    size_t clone_cycle(size_t source, const std::string& id, const std::string& name);
    void delete_cycle(size_t index);
    bool cycle_id_taken(const std::string& id) const;
    size_t add_workout(size_t cycle, std::optional<wl::CycleDay> day = std::nullopt);
    void remove_workout(size_t cycle, size_t workout);
    void move_workout(size_t cycle, size_t from, long to);
    void retitle_workout(size_t cycle, size_t workout, wl::CycleDay day);
    std::optional<wl::Date> planned_date(const wl::Cycle& cycle, size_t index) const;
    void save_cycles();
    void add_exercise(wl::Exercise exercise);

    struct ImportFile {
        std::string name;
        std::string contents;
    };
    void import_sessions(const std::vector<ImportFile>& files);
    // Returns how many files were written.
    size_t export_sessions(const std::string& directory);

signals:
    void changed();

private:
    struct FileInfo {
        // Empty when the file failed to decode: a corrupted file costs one
        // session, never the archive.
        std::optional<wl::Session> session;
        bool is_metcon = false;
        std::optional<wl::MuscleGroup> group;
    };

    void load_template();
    void load_reference_files();
    FileInfo analyse(const std::string& id) const;
    void rebuild_derived();
    std::optional<std::string> weekday_for(const wl::Cycle& cycle, size_t index) const;
    void resync_weekdays(wl::Cycle& cycle);
    void set_status(std::string status);

    SessionFolder folder_;
    std::optional<wl::SessionStore> store_;
    std::optional<wl::Catalogue> catalogue_;
    wl::CycleCatalogue cycles_;
    std::string map_template_;

    // Decoded sessions keyed by file id, so a refresh does work proportional to
    // what changed rather than re-decoding the archive.
    std::map<std::string, FileInfo> cache_;
    std::map<std::string, std::string> exercise_map_cache_;

    std::vector<std::string> files_;
    std::optional<std::string> selection_;
    std::optional<wl::Session> session_;
    unsigned session_generation_ = 0;
    // Where the loaded session came from, so a header edit moves its file
    // instead of orphaning it.
    std::optional<std::string> loaded_id_;
    std::string status_;
    wl::WeightingMode mode_ = wl::WeightingMode::set_count;
    std::map<std::string, DayInfo> calendar_;
    CycleMatrix cycle_;
    std::vector<wl::Session> cycle_sessions_;
    bool ready_ = false;
    std::optional<std::string> day_map_;
    std::optional<std::string> cycle_map_;
};
