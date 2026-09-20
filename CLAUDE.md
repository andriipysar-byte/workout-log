# WorkoutLog2 — agent guide

## Comments

Write a comment ONLY when the code uses a trick or a solution that is not obvious
from reading it — a non-obvious algorithm or invariant, a workaround for a platform
quirk, or *why* an approach was chosen over an alternative. Do NOT write comments
that restate what the code plainly does, label sections, or narrate steps. Prefer
clear names over comments. When in doubt, leave it out.

## Build & verify

- The three packages — `flutter`, `flutter/packages/workout_log_core` and `mcp` —
  are one pub workspace rooted at the repo root, so `flutter pub get` from any of
  them resolves all three against the single `pubspec.lock` at the root. That one
  resolution includes the app's `sdk: flutter` dependency, which is why even the
  two pure-Dart packages need the Flutter SDK to `pub get`.
- Domain core (pure Dart, no Flutter, no `dart:io`):
  `cd flutter/packages/workout_log_core && dart analyze && dart test`.
- App: `cd flutter && flutter analyze && flutter test`.
- MCP server (pure Dart over the core, with `dart:io`):
  `cd mcp && dart analyze && dart test`. Run it by hand with
  `dart run bin/workout_log_mcp.dart --repo ..`; it speaks MCP on stdio, so
  nothing in it may ever write to stdout.
- Run it: `cd flutter && WORKOUTLOG_DATA=../data flutter run -d macos`.
- Generate session stubs from a cycle template (replaces the old Python script):
  `cd flutter/packages/workout_log_core && dart run bin/wl_gen_cycle.dart [--force]`.
- Normalise session files after hand-editing them (sorted keys, no incidental
  diffs on the next save): `dart run bin/wl_fmt.dart` from the core package,
  or `--check` to fail without writing.
- After editing `exercises.json` or `cycles.json` — by hand *or* through the
  app's Plan tab, which writes them — re-sync the bundled copies:
  `cd flutter && dart run tool/sync_assets.dart`. A test fails if they drift.

Buildable and verifiable on this machine: macOS and web. The tree carries runners
for macOS, iOS, Linux and web only — Android and Windows were generated scaffolding
that nothing ever touched, and `cd flutter && flutter create --platforms=android .`
brings either back byte-identical the day it is wanted. Linux needs that host to
build on. iOS builds require a simulator/device run that has not been exercised
here — configured and analyzed, not proven.

- Data files in `data/` are the source of truth (ADR-001), except in the browser,
  which holds a working copy and imports/exports (ADR-007).
- The core carries all domain logic and the Flutter layer stays pure presentation
  (ADR-004); `test/purity_test.dart` enforces the boundary mechanically.
- `cycles.json` and `exercises.json` are hand-maintained and the app writes them
  back, so the models keep every key they do not themselves model (`extras` /
  `presentKeys`). `reference_files_test.dart` pins this: drop a key and a save
  would silently delete it from the user's file.
