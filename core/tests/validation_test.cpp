#include <doctest/doctest.h>

#include <algorithm>

#include "fixtures.hpp"
#include "workoutlog/validation.hpp"

using namespace wl;

namespace {

Session session_with(const std::string& overrides) {
    Json json = Json::parse(R"({"date": "2026-08-10", "cycle_day": "F1", "blocks": []})");
    Json patch = Json::parse(overrides);
    for (const auto& [key, value] : patch.items()) json[key] = value;
    return Session::from_json(json);
}

std::vector<std::string> paths(const std::vector<ValidationIssue>& issues, IssueSeverity severity) {
    std::vector<std::string> out;
    for (const auto& issue : issues)
        if (issue.severity == severity) out.push_back(issue.path);
    return out;
}

bool contains(const std::vector<std::string>& list, const std::string& item) {
    return std::find(list.begin(), list.end(), item) != list.end();
}

std::string dump(const std::vector<ValidationIssue>& issues) {
    Json out = Json::array();
    for (const auto& i : issues) out.push_back(i.to_json());
    return out.dump();
}

} // namespace

TEST_CASE("a session file from the archive is clean of errors") {
    auto catalogue = fixtures::catalogue();
    for (const auto& file : fixtures::session_files()) {
        auto issues = validate_session(decode_session(fixtures::read(file)), &catalogue);
        CHECK_MESSAGE(!has_errors(issues), (file.filename().string() + ": " + dump(issues)));
    }
}

TEST_CASE("a malformed header is an error") {
    auto issues = validate_session(session_with(R"({"date": "10.08.2026", "start_time": "8h02"})"));
    CHECK(paths(issues, IssueSeverity::error) == std::vector<std::string>{"date", "start_time"});
}

TEST_CASE("an off-convention cycle day is a warning, not a refusal") {
    auto issues = validate_session(session_with(R"({"cycle_day": "Z9"})"));
    CHECK_FALSE(has_errors(issues));
    CHECK(contains(paths(issues, IssueSeverity::warning), "cycle_day"));
}

TEST_CASE("an unfinished plan warns but never errors") {
    auto issues = validate_session(session_with(R"({"blocks": [
        {"type": "strength", "exercise": "присід фронтальний", "sets": []}]})"));
    CHECK_FALSE(has_errors(issues));
    CHECK(paths(issues, IssueSeverity::warning) == std::vector<std::string>{"blocks[0].sets"});
}

TEST_CASE("a strength block with no exercise cannot be written") {
    auto issues = validate_session(session_with(R"({"blocks": [
        {"type": "strength", "exercise": "  ", "sets": []}]})"));
    CHECK(paths(issues, IssueSeverity::error) == std::vector<std::string>{"blocks[0].exercise"});
}

TEST_CASE("a cluster whose total contradicts its chain is a warning") {
    auto issues = validate_session(session_with(R"({"blocks": [
        {"type": "strength", "exercise": "підтягування",
         "sets": [{"cluster": [5, 5, 4, 3, 3], "total_reps": 19}]}]})"));
    CHECK(contains(paths(issues, IssueSeverity::warning), "blocks[0].sets[0].total_reps"));
}

TEST_CASE("splits that go backwards are caught — they are cumulative") {
    auto issues = validate_session(session_with(R"({"blocks": [
        {"type": "metcon", "exercises": [{"name": "трастери"}],
         "rounds": [{"round": 1, "split_cumulative_sec": 463},
                    {"round": 2, "split_cumulative_sec": 292}]}]})"));
    CHECK(contains(paths(issues, IssueSeverity::warning), "blocks[0].rounds[1].split_cumulative_sec"));
}

TEST_CASE("blocks that overlap in time are caught") {
    auto issues = validate_session(session_with(R"({"start_time": "08:02", "blocks": [
        {"type": "cardio", "machine": "велотренажер", "end_time": "08:30"},
        {"type": "strength", "exercise": "присід фронтальний", "sets": [{"reps": 6}],
         "start_time": "08:20", "end_time": "08:50"}]})"));
    CHECK(contains(paths(issues, IssueSeverity::warning), "blocks[1]"));
}

TEST_CASE("an exercise outside the catalogue is a warning with the name in it") {
    auto catalogue = fixtures::catalogue();
    auto issues = validate_session(session_with(R"({"blocks": [
        {"type": "strength", "exercise": "вправа якої немає", "sets": [{"reps": 6}]}]})"),
                                   &catalogue);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].message.find("вправа якої немає") != std::string::npos);
    CHECK(issues[0].severity == IssueSeverity::warning);
}
