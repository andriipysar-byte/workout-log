#include "app_model.hpp"

#include <QDate>
#include <QFile>

#include <algorithm>
#include <filesystem>
#include <set>

#include "workoutlog/directory_storage.hpp"
#include "workoutlog/utf8.hpp"

namespace fs = std::filesystem;
using namespace wl;

AppModel::AppModel(QObject* parent) : QObject(parent) {}

void AppModel::set_status(std::string status) {
    status_ = std::move(status);
}

std::string AppModel::folder_label() const {
    return folder_.storage ? folder_.storage->label() : "";
}

bool AppModel::can_edit_plan() const {
    return folder_.references && folder_.references->can_write();
}

void AppModel::start(SessionFolder folder) {
    load_template();
    folder_ = std::move(folder);
    store_.emplace(*folder_.storage);
    load_reference_files();
    ready_ = true;
    refresh();
}

void AppModel::open_folder(SessionFolder folder) {
    folder_ = std::move(folder);
    store_.emplace(*folder_.storage);
    load_reference_files();
    cache_.clear();
    session_.reset();
    ++session_generation_;
    selection_.reset();
    loaded_id_.reset();
    refresh();
    set_status("Folder: " + folder_label());
    emit changed();
}

void AppModel::load_template() {
    QFile file(":/muscle-map.svg");
    if (!file.open(QIODevice::ReadOnly)) {
        set_status("Asset load failed: muscle-map.svg");
        return;
    }
    map_template_ = file.readAll().toStdString();
}

// A missing file starts an empty catalogue rather than failing: the first
// exercise or cycle added creates it.
void AppModel::load_reference_files() {
    try {
        auto exercises = folder_.references->read("exercises.json");
        catalogue_ = exercises ? decode_catalogue(*exercises) : Catalogue{};
        auto cycles = folder_.references->read("cycles.json");
        cycles_ = cycles ? decode_cycles(*cycles) : CycleCatalogue{};
        exercise_map_cache_.clear();
        if (!exercises) set_status("No exercises.json beside " + folder_label());
    } catch (const std::exception& e) {
        catalogue_.reset();
        set_status(std::string("Catalogue load failed: ") + e.what());
    }
}

void AppModel::refresh(bool from_disk) {
    if (!store_) return;
    if (from_disk) cache_.clear();
    try {
        files_ = store_->list_ids();
    } catch (const std::exception& e) {
        // An unreadable folder is a status message, not a crash: the app must
        // still come up so the user can point it somewhere else.
        files_.clear();
        set_status("Cannot read " + folder_label() + ": " + e.what());
        cache_.clear();
        rebuild_derived();
        emit changed();
        return;
    }
    for (const auto& id : files_)
        if (!cache_.contains(id)) cache_[id] = analyse(id);
    std::erase_if(cache_, [&](const auto& entry) {
        return std::find(files_.begin(), files_.end(), entry.first) == files_.end();
    });
    rebuild_derived();
    emit changed();
}

AppModel::FileInfo AppModel::analyse(const std::string& id) const {
    try {
        Session loaded = store_->load(id);
        FileInfo info;
        info.is_metcon = std::any_of(loaded.blocks.begin(), loaded.blocks.end(),
                                     [](const Block& b) { return std::holds_alternative<MetconBlock>(b); });
        if (catalogue_)
            info.group =
                dominant_group(muscle_activation::for_session(loaded, *catalogue_, WeightingMode::set_count));
        info.session = std::move(loaded);
        return info;
    } catch (const std::exception&) {
        return {};
    }
}

namespace {

// `2026-08-06_D2.json` → (`2026-08-06`, `D2`); empty when the name breaks the convention.
std::optional<std::pair<std::string, std::string>> split_id(const std::string& id) {
    std::string base = id.ends_with(".json") ? id.substr(0, id.size() - 5) : id;
    auto sep = base.find('_');
    if (sep == std::string::npos) return std::nullopt;
    return std::pair{base.substr(0, sep), base.substr(sep + 1)};
}

std::vector<std::string> exercise_names(const Block& block) {
    if (const auto* s = std::get_if<StrengthBlock>(&block)) return {s->exercise};
    std::vector<std::string> names;
    if (const auto* m = std::get_if<MetconBlock>(&block))
        for (const auto& e : m->exercises) names.push_back(e.name);
    return names;
}

} // namespace

void AppModel::rebuild_derived() {
    std::map<std::string, DayInfo> by_date;
    std::map<std::string, std::pair<std::string, const Session*>> latest;

    for (const auto& id : files_) {
        auto split = split_id(id);
        if (!split) continue;
        const auto& [date, cycle_day] = *split;
        const FileInfo& info = cache_.at(id);
        by_date[date] = DayInfo{id, date, cycle_day, info.is_metcon, info.group};
        if (info.session && (!latest.contains(cycle_day) || date > latest[cycle_day].first))
            latest[cycle_day] = {date, &*info.session};
    }

    calendar_ = std::move(by_date);

    // One column per cycle day from its most recent session; rows are exercises
    // in first-seen order.
    CycleMatrix matrix;
    std::map<std::string, std::set<std::string>> by_day;
    for (const auto& [day, entry] : latest) {
        matrix.days.push_back(day);
        auto& present = by_day[day];
        for (const auto& block : entry.second->blocks)
            for (auto& n : exercise_names(block)) {
                present.insert(n);
                if (std::find(matrix.exercises.begin(), matrix.exercises.end(), n) == matrix.exercises.end())
                    matrix.exercises.push_back(n);
            }
    }
    for (const auto& ex : matrix.exercises) {
        std::vector<bool> row;
        for (const auto& day : matrix.days) row.push_back(by_day[day].contains(ex));
        matrix.cells.push_back(std::move(row));
        matrix.groups.push_back(primary_groups(ex));
    }
    cycle_ = std::move(matrix);

    cycle_sessions_.clear();
    for (const auto& [_, entry] : latest) cycle_sessions_.push_back(*entry.second);
    cycle_map_.reset();
    day_map_.reset();
}

std::vector<MuscleGroup> AppModel::primary_groups(const std::string& name) const {
    std::vector<MuscleGroup> seen;
    const Exercise* exercise = catalogue_ ? catalogue_->resolve(name) : nullptr;
    if (!exercise) return seen;
    for (const auto& muscle : exercise->primary_muscles)
        if (auto group = group_of(muscle); group && std::find(seen.begin(), seen.end(), *group) == seen.end())
            seen.push_back(*group);
    return seen;
}

std::optional<std::string> AppModel::day_map_svg() {
    if (!session_ || !catalogue_ || map_template_.empty()) return std::nullopt;
    if (!day_map_)
        day_map_ = muscle_map_svg::colorize(map_template_, muscle_activation::for_session(*session_, *catalogue_, mode_));
    return day_map_;
}

std::optional<std::string> AppModel::cycle_map_svg() {
    if (!catalogue_ || map_template_.empty() || cycle_sessions_.empty()) return std::nullopt;
    if (!cycle_map_)
        cycle_map_ =
            muscle_map_svg::colorize(map_template_, muscle_activation::for_sessions(cycle_sessions_, *catalogue_, mode_));
    return cycle_map_;
}

std::optional<std::string> AppModel::exercise_map_svg(const std::string& name) {
    const Exercise* exercise = catalogue_ ? catalogue_->resolve(name) : nullptr;
    if (!exercise || map_template_.empty()) return std::nullopt;
    auto it = exercise_map_cache_.find(exercise->name);
    if (it == exercise_map_cache_.end())
        it = exercise_map_cache_
                 .emplace(exercise->name,
                          muscle_map_svg::colorize(map_template_, muscle_activation::for_exercise(*exercise)))
                 .first;
    return it->second;
}

std::optional<std::string> AppModel::plan_map_svg(const CycleSession& workout) const {
    if (!catalogue_ || map_template_.empty()) return std::nullopt;
    auto scores = muscle_activation::for_session(preview_session(workout), *catalogue_, WeightingMode::set_count);
    if (scores.empty()) return std::nullopt;
    return muscle_map_svg::colorize(map_template_, scores);
}

std::optional<std::string> AppModel::plan_cycle_map_svg(const Cycle& cycle) const {
    if (!catalogue_ || map_template_.empty()) return std::nullopt;
    std::vector<Session> previews;
    for (const auto& w : cycle.sessions) previews.push_back(preview_session(w));
    auto scores = muscle_activation::for_sessions(previews, *catalogue_, WeightingMode::set_count);
    if (scores.empty()) return std::nullopt;
    return muscle_map_svg::colorize(map_template_, scores);
}

std::optional<MuscleGroup> AppModel::plan_dominant_group(const CycleSession& workout) const {
    if (!catalogue_) return std::nullopt;
    return dominant_group(muscle_activation::for_session(preview_session(workout), *catalogue_, WeightingMode::set_count));
}

void AppModel::set_mode(WeightingMode mode) {
    if (mode_ == mode) return;
    mode_ = mode;
    day_map_.reset();
    cycle_map_.reset();
    emit changed();
}

void AppModel::session_edited() {
    day_map_.reset();
    emit changed();
}

void AppModel::open(const std::string& id) {
    try {
        session_ = store_->load(id);
        ++session_generation_;
        selection_ = id;
        loaded_id_ = id;
        day_map_.reset();
        set_status("Loaded " + id);
    } catch (const std::exception& e) {
        set_status(std::string("Load failed: ") + e.what());
    }
    emit changed();
}

void AppModel::save() {
    if (!session_) return;
    try {
        auto previous = loaded_id_;
        std::string id = store_->save(*session_, previous);
        cache_.erase(id);
        if (previous) cache_.erase(*previous);
        loaded_id_ = id;
        selection_ = id;
        set_status(previous && *previous != id ? "Renamed " + *previous + " → " + id : "Saved " + id);
        refresh();
    } catch (const std::exception& e) {
        set_status(std::string("Save failed: ") + e.what());
        emit changed();
    }
}

bool AppModel::exists(const std::string& id) const {
    return store_ && store_->exists(id);
}

std::string AppModel::read_raw(const std::string& id) const {
    return folder_.storage->read(id);
}

void AppModel::create(Session created) {
    try {
        std::string id = store_->save(created);
        cache_.erase(id);
        session_ = std::move(created);
        ++session_generation_;
        selection_ = id;
        loaded_id_ = id;
        day_map_.reset();
        set_status("Created " + id);
        refresh();
    } catch (const std::exception& e) {
        set_status(std::string("Create failed: ") + e.what());
        emit changed();
    }
}

void AppModel::remove(const std::string& id) {
    try {
        store_->remove(id);
        cache_.erase(id);
        if (selection_ == id) {
            selection_.reset();
            session_.reset();
            ++session_generation_;
            loaded_id_.reset();
            day_map_.reset();
        }
        set_status("Deleted " + id);
        refresh();
    } catch (const std::exception& e) {
        set_status(std::string("Delete failed: ") + e.what());
        emit changed();
    }
}

void AppModel::cycle_edited() {
    cycle_map_.reset();
    emit changed();
}

size_t AppModel::create_cycle(const std::string& id, const std::string& name) {
    Cycle cycle;
    cycle.id = id;
    cycle.name = name;
    cycle.training_days = {"Tue", "Thu", "Sun"};
    cycle.start_date = QDate::currentDate().toString(Qt::ISODate).toStdString();
    cycles_.cycles.push_back(std::move(cycle));
    set_status("Created cycle \"" + name + "\"");
    emit changed();
    return cycles_.cycles.size() - 1;
}

size_t AppModel::clone_cycle(size_t source, const std::string& id, const std::string& name) {
    Cycle clone = Cycle::from_json(cycles_.cycles.at(source).to_json());
    clone.id = id;
    clone.name = name;
    std::string source_name = cycles_.cycles[source].name;
    cycles_.cycles.push_back(std::move(clone));
    set_status("Cloned \"" + source_name + "\" as \"" + name + "\"");
    emit changed();
    return cycles_.cycles.size() - 1;
}

void AppModel::delete_cycle(size_t index) {
    std::string removed = cycles_.cycles.at(index).name;
    cycles_.cycles.erase(cycles_.cycles.begin() + static_cast<long>(index));
    set_status("Removed cycle \"" + removed + "\"");
    emit changed();
}

bool AppModel::cycle_id_taken(const std::string& id) const {
    std::string key = utf8::to_lower(utf8::trim(id));
    return std::any_of(cycles_.cycles.begin(), cycles_.cycles.end(),
                       [&](const Cycle& c) { return utf8::to_lower(c.id) == key; });
}

std::optional<std::string> AppModel::weekday_for(const Cycle& cycle, size_t index) const {
    auto date = planned_date(cycle, index);
    if (!date) return std::nullopt;
    return std::string(weekday_abbreviation(*date));
}

std::optional<Date> AppModel::planned_date(const Cycle& cycle, size_t index) const {
    auto start = parse_date(cycle.start_date);
    if (!start) return std::nullopt;
    try {
        return training_dates(*start, cycle.training_days, index + 1).at(index);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

size_t AppModel::add_workout(size_t index, std::optional<CycleDay> day) {
    Cycle& cycle = cycles_.cycles.at(index);
    std::vector<std::string> codes;
    for (const auto& s : cycle.sessions) codes.push_back(s.cycle_day);
    CycleDay next = day.value_or(cycle_templates::next_day(codes));
    std::int64_t days_per_week = std::max<std::int64_t>(1, static_cast<std::int64_t>(cycle.training_days.size()));
    auto workout = cycle_templates::session(next, static_cast<std::int64_t>(cycle.sessions.size()) / days_per_week + 1,
                                            weekday_for(cycle, cycle.sessions.size()));
    set_status("Added " + workout.cycle_day);
    cycle.sessions.push_back(std::move(workout));
    emit changed();
    return cycle.sessions.size() - 1;
}

// A workout's position decides its date, and its template records the weekday
// it expects — so moving or removing one must re-derive them, or the generator
// refuses the whole cycle.
void AppModel::resync_weekdays(Cycle& cycle) {
    for (size_t i = 0; i < cycle.sessions.size(); ++i)
        if (auto weekday = weekday_for(cycle, i)) cycle.sessions[i].weekday = weekday;
}

void AppModel::remove_workout(size_t index, size_t workout) {
    Cycle& cycle = cycles_.cycles.at(index);
    std::string code = cycle.sessions.at(workout).cycle_day;
    cycle.sessions.erase(cycle.sessions.begin() + static_cast<long>(workout));
    resync_weekdays(cycle);
    set_status("Removed " + code);
    emit changed();
}

void AppModel::move_workout(size_t index, size_t from, long to) {
    Cycle& cycle = cycles_.cycles.at(index);
    if (to < 0 || to >= static_cast<long>(cycle.sessions.size())) return;
    CycleSession moved = std::move(cycle.sessions.at(from));
    cycle.sessions.erase(cycle.sessions.begin() + static_cast<long>(from));
    cycle.sessions.insert(cycle.sessions.begin() + to, std::move(moved));
    resync_weekdays(cycle);
    emit changed();
}

void AppModel::retitle_workout(size_t index, size_t workout, CycleDay day) {
    CycleSession& session = cycles_.cycles.at(index).sessions.at(workout);
    session.cycle_day = day.code();
    session.type = day.kind == DayKind::conditioning ? "metcon" : "heavy";
    emit changed();
}

void AppModel::save_cycles() {
    try {
        folder_.references->write("cycles.json", encode_file(cycles_.to_json()));
        set_status("Saved cycles.json");
    } catch (const std::exception& e) {
        set_status(std::string("Save failed: ") + e.what());
    }
    emit changed();
}

void AppModel::add_exercise(Exercise exercise) {
    if (!catalogue_) return;
    std::string added = exercise.name;
    catalogue_ = catalogue_->with_exercise(std::move(exercise));
    exercise_map_cache_.erase(added);
    try {
        folder_.references->write("exercises.json", encode_file(catalogue_->to_json()));
        set_status("Added \"" + added + "\" to the catalogue");
    } catch (const std::exception& e) {
        set_status(std::string("Catalogue save failed: ") + e.what());
    }
    emit changed();
}

void AppModel::import_sessions(const std::vector<ImportFile>& picked) {
    if (picked.empty()) return;
    size_t written = 0;
    for (const auto& file : picked) {
        try {
            // Decoding first rejects a malformed pick at the door rather than
            // landing it in the archive as an unreadable file.
            decode_session(file.contents);
            folder_.storage->write(file.name, file.contents);
            cache_.erase(file.name);
            ++written;
        } catch (const std::exception& e) {
            set_status("Skipped " + file.name + ": " + e.what());
        }
    }
    refresh();
    set_status("Imported " + std::to_string(written) + " of " + std::to_string(picked.size()) + " file(s)");
    emit changed();
}

size_t AppModel::export_sessions(const std::string& directory) {
    size_t written = 0;
    try {
        for (const auto& id : files_) {
            write_file_atomically(fs::path(directory) / id, folder_.storage->read(id));
            ++written;
        }
        set_status(written == 0 ? "Nothing to export" : "Exported to " + directory);
    } catch (const std::exception& e) {
        set_status(std::string("Export failed: ") + e.what());
    }
    emit changed();
    return written;
}
