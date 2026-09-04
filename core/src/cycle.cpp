#include "workoutlog/cycle.hpp"

#include <algorithm>
#include <set>

#include "workoutlog/catalogue.hpp"
#include "workoutlog/muscle_activation.hpp"

namespace workoutlog::cycle {

std::vector<MuscleGroup> primary_groups(const std::string& name, const std::optional<Catalogue>& catalogue) {
    std::vector<MuscleGroup> seen;
    if (!catalogue) return seen;
    const Exercise* ex = catalogue::resolve(*catalogue, name);
    if (!ex) return seen;
    for (const auto& m : ex->primary_muscles) {
        auto g = muscle_group_of(m);
        if (g && std::find(seen.begin(), seen.end(), *g) == seen.end()) seen.push_back(*g);
    }
    return seen;
}

namespace {

std::vector<std::string> block_exercise_names(const Block& block) {
    if (const auto* sb = std::get_if<StrengthBlock>(&block)) return {sb->exercise};
    if (const auto* mb = std::get_if<MetconBlock>(&block)) {
        std::vector<std::string> names;
        for (const auto& e : mb->exercises) names.push_back(e.name);
        return names;
    }
    return {};
}

CycleMatrix build_cycle_matrix(const std::map<std::string, Session>& latest_by_day,
                                const std::optional<Catalogue>& catalogue) {
    CycleMatrix matrix;
    for (const auto& [day, _] : latest_by_day) matrix.days.push_back(day); // std::map keys: sorted ascending

    std::map<std::string, std::set<std::string>> present_by_day;
    for (const auto& day : matrix.days) {
        std::set<std::string> present;
        for (const auto& block : latest_by_day.at(day).blocks) {
            for (const auto& name : block_exercise_names(block)) {
                present.insert(name);
                if (std::find(matrix.exercises.begin(), matrix.exercises.end(), name) == matrix.exercises.end())
                    matrix.exercises.push_back(name);
            }
        }
        present_by_day[day] = std::move(present);
    }

    matrix.cells.resize(matrix.exercises.size());
    matrix.groups.resize(matrix.exercises.size());
    for (size_t ei = 0; ei < matrix.exercises.size(); ei++) {
        matrix.cells[ei].resize(matrix.days.size());
        for (size_t di = 0; di < matrix.days.size(); di++)
            matrix.cells[ei][di] = present_by_day[matrix.days[di]].count(matrix.exercises[ei]) > 0;
        matrix.groups[ei] = primary_groups(matrix.exercises[ei], catalogue);
    }
    return matrix;
}

} // namespace

Index build_index(const SessionStore& store, const std::optional<Catalogue>& catalogue) {
    Index idx;
    std::map<std::string, std::string> latest_date_by_day; // cycle_day -> the date of its most recent session
    std::map<std::string, Session> latest_session_by_day;

    for (const auto& path : store.list_urls()) {
        auto base = path.stem().string();
        auto sep = base.find('_');
        if (sep == std::string::npos) continue;

        DayInfo info;
        info.path = path;
        std::string date = base.substr(0, sep);
        info.cycle_day = base.substr(sep + 1);

        try {
            Session s = store.load(path);
            info.is_metcon =
                std::any_of(s.blocks.begin(), s.blocks.end(),
                            [](const Block& b) { return std::holds_alternative<MetconBlock>(b); });
            if (catalogue) {
                auto scores = MuscleActivation().for_session(s, *catalogue, WeightingMode::set_count);
                info.group = dominant(scores);
            }
            auto it = latest_date_by_day.find(info.cycle_day);
            if (it == latest_date_by_day.end() || date > it->second) {
                latest_date_by_day[info.cycle_day] = date;
                latest_session_by_day[info.cycle_day] = std::move(s);
            }
        } catch (...) {
            // A parse failure still gets a calendar cell (from the filename alone);
            // it just doesn't feed the cycle matrix or carry a muscle group.
        }

        idx.calendar[date] = info;
    }

    idx.cycle = build_cycle_matrix(latest_session_by_day, catalogue);
    for (const auto& day : idx.cycle.days) idx.cycle_sessions.push_back(latest_session_by_day.at(day));

    return idx;
}

} // namespace workoutlog::cycle
