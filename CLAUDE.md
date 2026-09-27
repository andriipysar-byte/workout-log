# WorkoutLog — agent guide

## Comments

Write a comment ONLY when the code uses a trick or a solution that is not obvious
from reading it — a non-obvious algorithm or invariant, a workaround for a platform
quirk, or *why* an approach was chosen over an alternative. Do NOT write comments
that restate what the code plainly does, label sections, or narrate steps. Prefer
clear names over comments. When in doubt, leave it out.

## Layout

One C++20 CMake project:

- `core/` — library `wlcore`, the whole domain: models, the JSON canon, the
  notation parser, validation, cycle planning, analytics, muscle maps. Its only
  dependency is nlohmann/json. It may not include Qt, `<filesystem>`,
  `<fstream>` or `<iostream>`; the `purity` ctest fails if it does (ADR-004).
- `storage/` — library `wlfs`: `DirectoryStorage` (write-then-rename) and repo
  discovery. The one place the filesystem meets the core.
- `tools/` — `wl_fmt` and `wl_gen_cycle`.
- `app/` — the Qt 6 Widgets app. `AppModel` holds state and calls the core;
  widgets hold no domain rules.

## Build & verify

- Toolchain: CMake ≥ 3.24, Ninja, a C++20 compiler, Qt 6.4+ with Svg
  (`brew install qt` / `apt install qt6-base-dev qt6-svg-dev`).
- `cmake --preset dev && cmake --build --preset dev && ctest --preset dev`
  runs everything: core, purity, storage, `wl_fmt --check` over `data/`, and the
  app's model and widget tests offscreen.
- `asan` is the same with ASan + UBSan; `headless` skips the Qt app (for a box
  without Qt); `release` for a real build.
- Run it: `WORKOUTLOG_DATA=$PWD/data build/dev/app/WorkoutLog.app/Contents/MacOS/WorkoutLog`
  on macOS, `build/dev/app/WorkoutLog` on Linux.
- Generate session stubs from a cycle template:
  `build/dev/tools/wl_gen_cycle [--cycle <id>] [--force]`.
- Normalise session files after hand-editing them:
  `build/dev/tools/wl_fmt` (or `--check` to fail without writing).

Buildable and verifiable on this machine: macOS. Linux builds in CI.

## Data-file integrity

- Data files in `data/` are the source of truth (ADR-001).
- The on-disk spelling is fixed: keys sorted at every depth, integral doubles
  without `.0`, Dart-style number formatting, raw UTF-8, trailing newline, no
  nulls. `round_trip_test.cpp` checks every file in `data/` re-encodes byte for
  byte; a change to `core/src/json.cpp` that breaks that breaks every user file
  on its next save.
- `cycles.json` and `exercises.json` are hand-maintained and the app writes them
  back, so the models keep every key they do not model (`extras` /
  `present_keys`). The round-trip tests pin this: drop a key and a save would
  silently delete it from the user's file.
- Tests never write to `data/`: the app tests run against `MemoryStorage` and
  `MemoryReferenceStore`, the storage tests against a temp directory.

## C++ conventions

- Nullable fields are `std::optional`; a key absent from a file stays absent.
- Blocks are a `std::variant`; dispatch with `std::visit` or `std::get_if`, so a
  new block type fails to compile everywhere it is not handled.
- Text is UTF-8 in `std::string`; anything that scans or case-folds it goes
  through `wl::utf8` (the catalogue is Ukrainian, the notation mixes Latin and
  Cyrillic look-alikes).
- Errors at the file boundary throw `wl::FormatError`; a bulk load collects
  failures rather than throwing (a corrupt file costs one session, never the
  archive).
