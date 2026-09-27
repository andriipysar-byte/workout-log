#include <doctest/doctest.h>

#include <regex>
#include <set>

#include "fixtures.hpp"
#include "workoutlog/muscles.hpp"

using namespace wl;

namespace {

size_t count_matches(const std::string& text, const std::regex& pattern) {
    return static_cast<size_t>(
        std::distance(std::sregex_iterator(text.begin(), text.end(), pattern), std::sregex_iterator()));
}

} // namespace

TEST_CASE("the template covers every muscle the catalogue uses") {
    auto svg = fixtures::svg_template();
    std::set<std::string> in_template;
    const std::regex tag(R"re(data-muscle="([a-z_]+)")re");
    for (auto it = std::sregex_iterator(svg.begin(), svg.end(), tag); it != std::sregex_iterator(); ++it)
        in_template.insert((*it)[1]);
    std::set<std::string> missing;
    for (const auto& e : fixtures::catalogue().exercises) {
        for (const auto& m : e.primary_muscles)
            if (!in_template.contains(m)) missing.insert(m);
        for (const auto& m : e.secondary_muscles)
            if (!in_template.contains(m)) missing.insert(m);
    }
    CHECK(missing.empty());
}

TEST_CASE("peak and unworked muscles get the ramp endpoints") {
    Scores scores;
    scores["quads"] = 1.0;
    scores["chest"] = 0.4;
    auto svg = muscle_map_svg::colorize(fixtures::svg_template(), scores);
    CHECK(svg.find(R"(data-muscle="quads" style="fill:#0d366b")") != std::string::npos);
    CHECK(svg.find(R"(data-muscle="forearms" style="fill:#e8e8e3")") != std::string::npos);
}

TEST_CASE("every tagged element is given exactly one inline fill") {
    auto svg_template = fixtures::svg_template();
    auto tagged = count_matches(svg_template, std::regex(R"re(data-muscle="[a-z_]+")re"));
    auto svg = muscle_map_svg::colorize(svg_template, Scores{});
    CHECK(count_matches(svg, std::regex(R"re(style="fill:#[0-9a-f]{6}")re")) == tagged);
}

TEST_CASE("the ramp endpoints and midpoint are exact hexes") {
    CHECK(muscle_map_svg::color(0) == "#e8e8e3");
    CHECK(muscle_map_svg::color(-1) == "#e8e8e3");
    CHECK(muscle_map_svg::color(1) == "#0d366b");
    CHECK_MESSAGE(muscle_map_svg::color(2) == "#0d366b", "scores clamp at 1");
    CHECK(muscle_map_svg::color(0.000001) == "#86b6ef");
    CHECK(muscle_map_svg::color(0.5) == "#4a76ad");
}

TEST_CASE("colorize leaves the rest of the template byte-identical") {
    auto svg_template = fixtures::svg_template();
    auto svg = muscle_map_svg::colorize(svg_template, Scores{});
    CHECK(std::regex_replace(svg, std::regex(R"re( style="fill:#[0-9a-f]{6}")re"), "") == svg_template);
}
