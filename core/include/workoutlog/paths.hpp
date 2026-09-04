#pragma once

#include <filesystem>
#include <string>

// Replaces the #filePath-walked-up-four-levels repo-root discovery duplicated across
// wl-verify/main.swift, wl-map/main.swift, WorkoutLogApp.swift and
// RoundTripTests.swift -- all of which bake in a build-machine path.
namespace workoutlog::paths {

// Resolves the repository root, trying in order:
//   1. an explicit hint
//   2. $WORKOUTLOG_ROOT
//   3. the parent of $WORKOUTLOG_DATA
//   4. walking up from the current working directory for a marker
//      (exercises.json AND data/ both present)
//   5. walking up from the running executable's own path
// Throws std::runtime_error listing every attempt if none succeed.
std::filesystem::path resolve_repo_root(const std::string& hint = {});

} // namespace workoutlog::paths
