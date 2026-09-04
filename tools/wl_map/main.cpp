// Port of app/Sources/wl-map/main.swift. Exports a muscle heatmap SVG for a session.
// Usage: wl_map <session.json> [set_count|rep_volume|tonnage] [out.svg]

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

#include "workoutlog/catalogue.hpp"
#include "workoutlog/json.hpp"
#include "workoutlog/muscle_activation.hpp"
#include "workoutlog/muscle_map_svg.hpp"
#include "workoutlog/paths.hpp"
#include "workoutlog/store.hpp"

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::optional<workoutlog::WeightingMode> mode_from_string(std::string_view s) {
    if (s == "set_count") return workoutlog::WeightingMode::set_count;
    if (s == "rep_volume") return workoutlog::WeightingMode::rep_volume;
    if (s == "tonnage") return workoutlog::WeightingMode::tonnage;
    return std::nullopt;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: wl_map <session.json> [set_count|rep_volume|tonnage] [out.svg]\n";
        return 2;
    }

    std::filesystem::path session_path;
    workoutlog::WeightingMode mode = workoutlog::WeightingMode::set_count;
    std::filesystem::path repo_root;
    try {
        session_path = argv[1];
        if (argc >= 3) {
            auto opt_mode = mode_from_string(argv[2]);
            if (!opt_mode.has_value()) {
                std::cerr << "error: unrecognised mode '" << argv[2] << "'. available modes: set_count, rep_volume, tonnage\n";
                return 2;
            }
            mode = opt_mode.value();
        }
        repo_root = workoutlog::paths::resolve_repo_root();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    auto template_path = workoutlog::paths::muscle_map_template_path(repo_root);
    auto catalogue_path = workoutlog::paths::catalogue_path(repo_root);

    std::filesystem::path out_path;
    if (argc >= 4) {
        out_path = argv[3];
    } else {
        out_path = session_path;
        out_path.replace_extension();
        out_path += "." + workoutlog::to_string(mode) + ".svg";
    }

    try {
        auto catalogue = workoutlog::json::decode_catalogue(read_file(catalogue_path));
        auto session = workoutlog::json::decode_session(read_file(session_path));
        auto tmpl = read_file(template_path);

        auto scores = workoutlog::MuscleActivation().for_session(session, catalogue, mode);
        auto svg = workoutlog::muscle_map_svg::colorize(tmpl, scores);
        workoutlog::write_file_atomic(out_path, svg);

        std::vector<std::pair<std::string, double>> ranked(scores.begin(), scores.end());
        std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        if (ranked.size() > 5) ranked.resize(5);

        std::ostringstream top;
        for (size_t i = 0; i < ranked.size(); i++) {
            if (i) top << ", ";
            top << ranked[i].first << " " << std::fixed << std::setprecision(2) << ranked[i].second;
        }
        std::cout << "wrote " << out_path.filename().string() << "  [" << workoutlog::to_string(mode) << "]  top: " << top.str()
                  << "\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
