# WorkoutLog2 — agent guide

## Comments

Write a comment ONLY when the code uses a trick or a solution that is not obvious
from reading it — a non-obvious algorithm or invariant, a workaround for a platform
quirk, or *why* an approach was chosen over an alternative. Do NOT write comments
that restate what the code plainly does, label sections, or narrate steps. Prefer
clear names over comments. When in doubt, leave it out.

## Build & verify

- Domain core (pure Dart, no Flutter, no `dart:io`):
  `cd flutter/packages/workout_log_core && dart analyze && dart test`.
- App: `cd flutter && flutter analyze && flutter test`.
- Run it: `cd flutter && WORKOUTLOG_DATA=../data flutter run -d macos`.
- Generate session stubs from a cycle template (replaces the old Python script):
  `cd flutter/packages/workout_log_core && dart run bin/wl_gen_cycle.dart [--force]`.
- Normalise session files after hand-editing them (sorted keys, no incidental
  diffs on the next save): `dart run bin/wl_fmt.dart` from the core package,
  or `--check` to fail without writing.
- After editing `exercises.json` or `cycles.json`, re-sync the bundled copies:
  `cd flutter && dart run tool/sync_assets.dart`. A test fails if they drift.

Buildable and verifiable on this machine: macOS and web. Android needs an SDK that
is not installed; Linux and Windows need those hosts. iOS builds require a
simulator/device run that has not been exercised here — configured and analyzed,
not proven.

- Data files in `data/` are the source of truth (ADR-001), except in the browser,
  which holds a working copy and imports/exports (ADR-007).
- The core carries all domain logic and the Flutter layer stays pure presentation
  (ADR-004); `test/purity_test.dart` enforces the boundary mechanically.
