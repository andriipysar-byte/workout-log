// Port of app/Sources/wl-verify/main.swift. Lightweight, no-test-framework verifier,
// runnable on a machine with no Swift toolchain -- this is the acceptance gate for the
// whole C++ core (see docs/05-architecture.md, ADR-007).

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "workoutlog/calendar.hpp"
#include "workoutlog/catalogue.hpp"
#include "workoutlog/cycle.hpp"
#include "workoutlog/json.hpp"
#include "workoutlog/models.hpp"
#include "workoutlog/muscle_activation.hpp"
#include "workoutlog/muscle_group.hpp"
#include "workoutlog/muscle_map_svg.hpp"
#include "workoutlog/notation.hpp"
#include "workoutlog/paths.hpp"
#include "workoutlog/store.hpp"

using namespace workoutlog;

namespace {

int g_failures = 0;

void check(bool passed, const std::string& message) {
    std::cout << (passed ? "  ok   " : "  FAIL ") << message << "\n";
    if (!passed) g_failures++;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string max_key(const std::map<std::string, double>& m) {
    std::string best;
    double best_v = -1.0;
    for (const auto& [k, v] : m)
        if (v > best_v) {
            best = k;
            best_v = v;
        }
    return best;
}

std::set<std::string> extract_muscle_tokens(const std::string& svg) {
    static const std::string kNeedle = "data-muscle=\"";
    std::set<std::string> tokens;
    size_t pos = 0;
    while (true) {
        size_t found = svg.find(kNeedle, pos);
        if (found == std::string::npos) break;
        size_t start = found + kNeedle.size();
        size_t i = start;
        while (i < svg.size() && ((svg[i] >= 'a' && svg[i] <= 'z') || svg[i] == '_')) i++;
        if (i > start && i < svg.size() && svg[i] == '"') {
            tokens.insert(svg.substr(start, i - start));
            pos = i + 1;
        } else {
            pos = found + kNeedle.size();
        }
    }
    return tokens;
}

std::string join_sorted(const std::set<std::string>& s) {
    std::string out;
    for (const auto& v : s) {
        if (!out.empty()) out += ", ";
        out += v;
    }
    return out;
}

} // namespace

int main() {
    std::filesystem::path repo_root;
    try {
        repo_root = paths::resolve_repo_root();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    auto data_dir = repo_root / "data";

    // ---- 1. Round-trip over data/*.json --------------------------------------
    {
        SessionStore store(data_dir);
        auto files = store.list_urls();
        std::cout << "Round-trip over " << files.size() << " session file(s) in " << data_dir.string() << ":\n";

        static const std::vector<std::string> kStubNames = {
            "2026-07-21_A1", "2026-07-23_A2", "2026-07-26_B1", "2026-07-28_B2",
            "2026-07-30_C1", "2026-08-02_C2", "2026-08-04_D1", "2026-08-06_D2",
        };
        std::set<std::string> present;
        for (const auto& f : files) present.insert(f.stem().string());
        bool all_stubs = true;
        for (const auto& name : kStubNames)
            if (!present.count(name)) all_stubs = false;
        check(all_stubs, "all 8 generated cycle stubs present");

        for (const auto& f : files) {
            try {
                auto original = read_file(f);
                auto s1 = json::decode_session(original);
                auto enc1 = json::encode_session(s1);
                auto s2 = json::decode_session(enc1);
                auto enc2 = json::encode_session(s2);
                check(s1 == s2 && enc1 == enc2, "round-trip " + f.filename().string());
                // Strictly stronger than the idempotence check above: after the
                // wl_fmt reformat commit, every file on disk IS the canonical writer's
                // output, so re-emitting it must be byte-identical to what's already
                // there. Idempotence alone can't catch a writer that's stable but wrong.
                check(enc1 == original, "canonical (byte-identical to disk) " + f.filename().string());
            } catch (const std::exception& e) {
                check(false, "decode " + f.filename().string() + ": " + e.what());
            }
        }
    }

    // ---- 2. Catalogue (exercises.json) ----------------------------------------
    std::optional<Catalogue> catalogue;
    std::cout << "Catalogue (exercises.json):\n";
    try {
        auto cat = json::decode_catalogue(read_file(repo_root / "exercises.json"));
        catalogue = cat;

        check(!cat.exercises.empty(), "catalogue non-empty (" + std::to_string(cat.exercises.size()) + " exercises)");

        auto front_squat = catalogue::resolve(cat, "присід фронтальний");
        check(front_squat && front_squat->pattern == Pattern::squat, "canonical name resolves");

        auto alias = catalogue::resolve(cat, "фр. присід");
        check(alias && alias->name == "присід фронтальний", "alias resolves to canonical");

        check(catalogue::resolve(cat, "невідома вправа") == nullptr, "unknown name returns nil (warn-not-block)");

        auto bench = catalogue::resolve(cat, "жим лежачи");
        check(bench && bench->primary_muscles == std::vector<std::string>{"chest", "triceps"},
              "жим лежачи resolves with primary muscles");

        auto kb = catalogue::resolve(cat, "гирі");
        check(kb && kb->name == "махи гирею", "metcon alias 'гирі' resolves to махи гирею");

        bool all_have_primary = true;
        for (const auto& ex : cat.exercises)
            if (ex.primary_muscles.empty()) all_have_primary = false;
        check(all_have_primary, "every exercise has primary muscles");

        std::set<std::string> ungrouped;
        for (const auto& ex : cat.exercises) {
            for (const auto& m : ex.primary_muscles)
                if (!muscle_group_of(m)) ungrouped.insert(m);
            for (const auto& m : ex.secondary_muscles)
                if (!muscle_group_of(m)) ungrouped.insert(m);
        }
        check(ungrouped.empty(), "every catalogue muscle maps to a group (ungrouped: " + join_sorted(ungrouped) + ")");

        check(dominant({{"chest", 3.0}, {"biceps", 1.0}}) == MuscleGroup::chest,
              "dominant group is the highest-scoring region");

        auto ryvok = catalogue::resolve(cat, "ривок");
        check(ryvok && ryvok->category == ExerciseCategory::power && ryvok->is_explosive(),
              "category enum decodes (ривок = power)");

        std::map<ExerciseCategory, int> by_category;
        for (const auto& ex : cat.exercises) by_category[ex.category]++;
        check(by_category[ExerciseCategory::power] == 6 && by_category[ExerciseCategory::speed] == 3,
              "6 power + 3 speed = former explosive set (power:" +
                  std::to_string(by_category[ExerciseCategory::power]) +
                  " speed:" + std::to_string(by_category[ExerciseCategory::speed]) + ")");

        int explosive_count = 0;
        for (const auto& ex : cat.exercises)
            if (ex.is_explosive()) explosive_count++;
        check(explosive_count == 9, "9 movements are explosive (power + speed)");
    } catch (const std::exception& e) {
        check(false, std::string("catalogue load: ") + e.what());
    }

    // ---- 3. MuscleActivation ---------------------------------------------------
    std::cout << "MuscleActivation:\n";
    try {
        if (!catalogue) throw std::runtime_error("catalogue not loaded");
        MuscleActivation activation;

        if (auto* fs = catalogue::resolve(*catalogue, "присід фронтальний")) {
            auto m = activation.for_exercise(*fs);
            check(m.count("quads") && m.count("glutes") && m.count("spinal_erectors") && m["quads"] == 1.0 &&
                      m["glutes"] == 1.0 && m["spinal_erectors"] == 0.5,
                  "per-exercise map: front squat quads/glutes=1.0, erectors=0.5");
        } else {
            check(false, "front squat missing from catalogue");
        }

        SessionStore store(data_dir);
        auto c2 = store.load(data_dir / "2026-06-23_C2.json");

        std::map<WeightingMode, std::string> peaks;
        for (auto mode : {WeightingMode::set_count, WeightingMode::rep_volume, WeightingMode::tonnage}) {
            auto map = activation.for_session(c2, *catalogue, mode);
            double peak = 0;
            for (const auto& [k, v] : map) peak = std::max(peak, v);
            check(std::fabs(peak - 1.0) < 1e-9, to_string(mode) + ": normalized peak = 1.0");
            peaks[mode] = max_key(map);
        }
        check(peaks[WeightingMode::set_count] == "forearms",
              "set_count peak = forearms (grip-set heavy): " + peaks[WeightingMode::set_count]);
        check(peaks[WeightingMode::rep_volume] == "quads" || peaks[WeightingMode::rep_volume] == "glutes",
              "rep_volume peak is squat pattern: " + peaks[WeightingMode::rep_volume]);
        check(peaks[WeightingMode::tonnage] == "quads" || peaks[WeightingMode::tonnage] == "glutes",
              "tonnage peak is squat pattern: " + peaks[WeightingMode::tonnage]);
        check(peaks[WeightingMode::set_count] != peaks[WeightingMode::tonnage],
              "weighting modes differ (set_count != tonnage)");
    } catch (const std::exception& e) {
        check(false, std::string("MuscleActivation: ") + e.what());
    }

    // ---- 4. MuscleMapSVG colorizer ---------------------------------------------
    std::cout << "MuscleMapSVG colorizer:\n";
    try {
        if (!catalogue) throw std::runtime_error("catalogue not loaded");
        auto tmpl = read_file(repo_root / "app/Resources/muscle-map.svg");

        std::set<std::string> used_muscles;
        for (const auto& ex : catalogue->exercises) {
            for (const auto& m : ex.primary_muscles) used_muscles.insert(m);
            for (const auto& m : ex.secondary_muscles) used_muscles.insert(m);
        }
        auto template_muscles = extract_muscle_tokens(tmpl);
        std::set<std::string> missing;
        for (const auto& m : used_muscles)
            if (!template_muscles.count(m)) missing.insert(m);
        check(missing.empty(), "SVG template covers every catalogue muscle (missing: " + join_sorted(missing) + ")");

        auto svg = muscle_map_svg::colorize(tmpl, {{"quads", 1.0}, {"chest", 0.4}});
        check(svg.find(R"(data-muscle="quads" style="fill:#0d366b")") != std::string::npos,
              "peak muscle (quads=1.0) filled with high colour");
        check(svg.find(R"(data-muscle="forearms" style="fill:#e8e8e3")") != std::string::npos,
              "unworked muscle filled with zero colour");
        check(muscle_map_svg::color(0, "#86b6ef", "#0d366b", "#e8e8e3") == "#e8e8e3", "score 0 -> zero colour");
        check(muscle_map_svg::color(0.5, "#86b6ef", "#0d366b", "#e8e8e3") == "#4a76ad",
              "score 0.5 -> midpoint colour (round-half-away-from-zero)");
        check(muscle_map_svg::color(1, "#86b6ef", "#0d366b", "#e8e8e3") == "#0d366b", "score 1 -> high colour");
    } catch (const std::exception& e) {
        check(false, std::string("MuscleMapSVG: ") + e.what());
    }

    // ---- 5. SessionStore (save -> reload) ---------------------------------------
    std::cout << "SessionStore (save -> reload):\n";
    try {
        SessionStore store(data_dir);
        auto urls = store.list_urls();
        if (!urls.empty()) {
            auto loaded = store.load(urls.front());
            auto tmp_dir = std::filesystem::temp_directory_path() / ("wl-verify-" + loaded.cycle_day);
            SessionStore tmp_store(tmp_dir);
            auto written = tmp_store.save(loaded);
            auto reloaded = tmp_store.load(written);
            check(reloaded == loaded, "save then reload is identical (" + written.filename().string() + ")");
            check(written.filename().string() == loaded.date + "_" + loaded.cycle_day + ".json",
                  "filename follows YYYY-MM-DD_<cycleDay>.json");
            std::error_code ec;
            std::filesystem::remove_all(tmp_dir, ec);
        } else {
            check(false, "no session files to exercise SessionStore");
        }
    } catch (const std::exception& e) {
        check(false, std::string("SessionStore: ") + e.what());
    }

    // ---- 6. Notation parser (docs/03-log-notation.md examples) -----------------
    std::cout << "Notation parser (docs/03-log-notation.md examples):\n";
    {
        auto ramp = notation::parse_strength_sets("6 × [70, 80, 90, 100, 110]");
        bool ramp_ok = ramp.sets.size() == 5;
        for (const auto& s : ramp.sets) ramp_ok = ramp_ok && s.reps == 6;
        std::vector<std::optional<double>> weights;
        for (const auto& s : ramp.sets) weights.push_back(s.weight_kg);
        ramp_ok = ramp_ok && weights == std::vector<std::optional<double>>{70, 80, 90, 100, 110};
        check(ramp_ok, "ramp expands to 5 sets at reps 6");

        auto backoff = notation::parse_strength_sets("6 × [70, 80, 90, 100, 110] + 6 × [30]");
        bool backoff_ok = backoff.sets.size() == 6 && backoff.sets.back().weight_kg == 30 &&
                           backoff.sets.back().is_backoff == true;
        for (size_t i = 0; i < 5 && backoff_ok; i++) backoff_ok = backoff_ok && !backoff.sets[i].is_backoff;
        check(backoff_ok, "trailing group is a back-off set");

        auto override_sets = notation::parse_strength_sets("6 × [30, 60, 80(4), 86(2), 90(1)]");
        std::vector<std::optional<int>> reps;
        for (const auto& s : override_sets.sets) reps.push_back(s.reps);
        check(reps == std::vector<std::optional<int>>{6, 6, 4, 2, 1}, "per-set rep overrides applied");

        auto cluster = notation::parse_strength_sets("5+5+4+3+3 (20)");
        check(cluster.sets.size() == 1 && cluster.sets[0].cluster == std::vector<int>{5, 5, 4, 3, 3} &&
                  cluster.sets[0].total_reps == 20,
              "cluster with explicit total");

        auto cluster2 = notation::parse_strength_sets("4+4+3+2+2+2+2+1");
        check(!cluster2.sets.empty() && cluster2.sets[0].total_reps == 20, "cluster total derived from sum");

        auto holds = notation::parse_strength_sets("4 × [54c, 40c, 36c, 42c]");
        bool holds_ok = holds.sets.size() == 4;
        std::vector<std::optional<double>> durations;
        for (const auto& s : holds.sets) durations.push_back(s.duration_sec);
        holds_ok = holds_ok && durations == std::vector<std::optional<double>>{54, 40, 36, 42};
        for (const auto& s : holds.sets) holds_ok = holds_ok && !s.reps;
        check(holds_ok, "timed holds -> duration sets, no reps");

        auto braces = notation::parse_strength_sets("{60с, 70с, 30с, 35с}");
        std::vector<std::optional<double>> brace_durations;
        for (const auto& s : braces.sets) brace_durations.push_back(s.duration_sec);
        check(braces.sets.size() == 4 && brace_durations == std::vector<std::optional<double>>{60, 70, 30, 35},
              "bare brace list with Cyrillic 'с' suffix");

        // Adversarial cases beyond the original Swift suite -- ported here because
        // they're what the UTF-8 rework (landmine #2) and strict number parsing are
        // actually for.
        check(notation::parse_strength_sets("6 × []").sets.empty(), "empty bracket list yields no sets");

        auto cyr_x = notation::parse_strength_sets("6 х [70]");
        check(cyr_x.sets.size() == 1 && cyr_x.sets[0].weight_kg == 70 && cyr_x.sets[0].reps == 6,
              "Cyrillic multiplier 'х' recognised");

        auto star = notation::parse_strength_sets("6 * [70]");
        check(star.sets.size() == 1 && star.sets[0].weight_kg == 70 && star.sets[0].reps == 6,
              "'*' multiplier recognised");

        auto backoff_no_count = notation::parse_strength_sets("6 X [70,80] + [30]");
        check(backoff_no_count.sets.size() == 3 && backoff_no_count.sets.back().weight_kg == 30 &&
                  backoff_no_count.sets.back().is_backoff == true && !backoff_no_count.sets.back().reps,
              "back-off group with no count prefix has no reps");

        // A comma inside a bracket list is always an item separator (matching Swift's
        // `.split(separator: ",")` on the list), never a decimal point -- so "47,5"
        // here is two values, not one. The comma-to-period swap in parse_weight only
        // ever sees a token after that split has already happened.
        auto comma_in_list = notation::parse_strength_sets("1 × [47,5]");
        check(comma_in_list.sets.size() == 2 && comma_in_list.sets[0].weight_kg == 47 &&
                  comma_in_list.sets[1].weight_kg == 5,
              "comma inside a bracket list is an item separator, not a decimal point");

        auto standalone_paren = notation::parse_strength_sets("1 × [90(4)]");
        check(standalone_paren.sets.size() == 1 && standalone_paren.sets[0].weight_kg == 90 &&
                  standalone_paren.sets[0].reps == 4,
              "standalone rep override parses");

        check(notation::parse_strength_sets("   ").sets.empty(), "whitespace-only input yields no sets");

        auto mixed_seconds = notation::parse_strength_sets("4 × [54c, 40с]");
        std::vector<std::optional<double>> mixed_durations;
        for (const auto& s : mixed_seconds.sets) mixed_durations.push_back(s.duration_sec);
        check(mixed_seconds.sets.size() == 2 && mixed_durations == std::vector<std::optional<double>>{54, 40},
              "Latin 'c' and Cyrillic 'с' seconds suffix in one list");

        auto compose = notation::parse_strength_sets("6 × [70, 80]");
        StrengthBlock block;
        block.exercise = "присід фронтальний";
        block.sets = compose.sets;
        Session session;
        session.date = "2026-07-21";
        session.cycle_day = "A1";
        session.blocks.emplace_back(std::move(block));
        bool round_ok = false;
        try {
            auto data = json::encode_session(session);
            auto round = json::decode_session(data);
            if (auto* sb = std::get_if<StrengthBlock>(&round.blocks.front()))
                round_ok = sb->sets.size() == 2 && sb->sets[0].weight_kg == 70;
        } catch (...) {
            round_ok = false;
        }
        check(round_ok, "parsed sets survive a JSON round-trip in a session");
    }

    // ---- 7. Ukrainian case-folding (landmine: naive +0x20 fold breaks these) ---
    std::cout << "Ukrainian case-folding (Catalogue.resolve):\n";
    try {
        if (!catalogue) throw std::runtime_error("catalogue not loaded");
        auto upper1 = catalogue::resolve(*catalogue, "ПРИСІД ФРОНТАЛЬНИЙ");
        check(upper1 && upper1->name == "присід фронтальний", "resolves ALL-CAPS with 'І' (U+0406, folds by +0x50)");

        auto upper2 = catalogue::resolve(*catalogue, "ЖИМ ЛЕЖАЧИ");
        check(upper2 && upper2->name == "жим лежачи", "resolves ALL-CAPS 'ЖИМ ЛЕЖАЧИ'");
    } catch (const std::exception& e) {
        check(false, std::string("case-folding: ") + e.what());
    }

    // ---- 8. cycle::build_index / primary_groups (moved out of AppModel -- ADR-004) --
    std::cout << "cycle::build_index (moved out of AppModel.swift, ADR-004):\n";
    try {
        if (!catalogue) throw std::runtime_error("catalogue not loaded");
        SessionStore store(data_dir);
        auto idx = cycle::build_index(store, catalogue);

        check(idx.calendar.size() == 9, "calendar has one entry per file, including duplicate cycle days (" +
                                             std::to_string(idx.calendar.size()) + ")");
        check(idx.calendar.count("2026-06-23") && idx.calendar.count("2026-08-02"),
              "both same-cycle-day files keep independent calendar entries");

        static const std::vector<std::string> kExpectedDays = {"A1", "A2", "B1", "B2", "C1", "C2", "D1", "D2"};
        check(idx.cycle.days == kExpectedDays, "cycle matrix collapses duplicate C2 files to one column, sorted");
        check(idx.cycle_sessions.size() == 8, "one session per cycle day in the matrix");

        auto ci = std::find(idx.cycle.exercises.begin(), idx.cycle.exercises.end(), "присід фронтальний");
        check(ci != idx.cycle.exercises.end(), "front squat appears in the cycle exercise list");
        if (ci != idx.cycle.exercises.end()) {
            size_t ei = static_cast<size_t>(ci - idx.cycle.exercises.begin());
            auto di = std::find(idx.cycle.days.begin(), idx.cycle.days.end(), "C2");
            size_t day_i = static_cast<size_t>(di - idx.cycle.days.begin());
            check(idx.cycle.cells[ei][day_i], "front squat is present on the C2 column (from the 08-02 stub, the "
                                               "later of the two C2 files)");
        }

        auto groups = cycle::primary_groups("присід фронтальний", catalogue);
        check(groups.size() == 1 && groups[0] == MuscleGroup::legs,
              "front squat's primary muscles (quads+glutes) collapse to one group (legs)");
    } catch (const std::exception& e) {
        check(false, std::string("cycle::build_index: ") + e.what());
    }

    // ---- 9. calendar::month_cells -----------------------------------------------
    std::cout << "calendar::month_cells:\n";
    {
        check(calendar::weekday_sun1(2026, 8, 1) == 7, "Aug 1 2026 is a Saturday (sun1=7)");
        check(calendar::days_in_month(2026, 2) == 28, "Feb 2026 is not a leap year");
        check(calendar::days_in_month(2024, 2) == 29, "Feb 2024 is a leap year");

        auto cells_sun_first = calendar::month_cells(2026, 8, 1); // first_weekday = Sunday
        check(cells_sun_first.size() == 37, "Aug 2026, Sunday-first: 6 leading blanks + 31 days = 37 cells");
        check(!cells_sun_first[0].has_value() && cells_sun_first[6] == 1,
              "6 leading blanks before the 1st, Sunday-first");

        auto cells_mon_first = calendar::month_cells(2026, 8, 2); // first_weekday = Monday
        check(cells_mon_first.size() == 36, "Aug 2026, Monday-first: 5 leading blanks + 31 days = 36 cells");
        check(!cells_mon_first[0].has_value() && cells_mon_first[5] == 1,
              "5 leading blanks before the 1st, Monday-first");
    }

    std::cout << (g_failures == 0 ? "\n\xE2\x9C\x85 ALL CHECKS PASSED"
                                  : "\n\xE2\x9D\x8C " + std::to_string(g_failures) + " CHECK(S) FAILED")
              << "\n";
    return g_failures == 0 ? 0 : 1;
}
