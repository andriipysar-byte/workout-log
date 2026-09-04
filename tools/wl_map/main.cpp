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

using namespace workoutlog;

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::optional<WeightingMode> mode_from_string(std::string_view s) {
    if (s == "set_count") return WeightingMode::set_count;
    if (s == "rep_volume") return WeightingMode::rep_volume;
    if (s == "tonnage") return WeightingMode::tonnage;
    return std::nullopt;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: wl_map <session.json> [set_count|rep_volume|tonnage] [out.svg]\n";
        return 2;
    }

    std::filesystem::path session_path;
    WeightingMode mode = WeightingMode::set_count;
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
        repo_root = paths::resolve_repo_root();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    auto template_path = paths::muscle_map_template_path(repo_root);
    auto catalogue_path = paths::catalogue_path(repo_root);

    std::filesystem::path out_path;
    if (argc >= 4) {
        out_path = argv[3];
    } else {
        out_path = session_path;
        out_path.replace_extension();
        out_path += "." + to_string(mode) + ".svg";
    }

    try {
        auto catalogue = json::decode_catalogue(read_file(catalogue_path));
        auto session = json::decode_session(read_file(session_path));
        auto tmpl = read_file(template_path);

        auto scores = MuscleActivation().for_session(session, catalogue, mode);
        auto svg = muscle_map_svg::colorize(tmpl, scores);
        write_file_atomic(out_path, svg);

        std::vector<std::pair<std::string, double>> ranked(scores.begin(), scores.end());
        std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        if (ranked.size() > 5) ranked.resize(5);

        std::ostringstream top;
        for (size_t i = 0; i < ranked.size(); i++) {
            if (i) top << ", ";
            top << ranked[i].first << " " << std::fixed << std::setprecision(2) << ranked[i].second;
        }
        std::cout << "wrote " << out_path.filename().string() << "  [" << to_string(mode) << "]  top: " << top.str()
                  << "\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
