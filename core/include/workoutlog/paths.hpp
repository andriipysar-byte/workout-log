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

// The two asset paths under a resolved repo root, spelled out once. Previously
// duplicated across tools/wl_map/main.cpp and tools/wl_verify/main.cpp; the
// SDL3/ImGui UI is the third site to need them, which is the signal to extract
// them here instead of copying the literals again.
std::filesystem::path catalogue_path(const std::filesystem::path& repo_root);
std::filesystem::path muscle_map_template_path(const std::filesystem::path& repo_root);

} // namespace workoutlog::paths
