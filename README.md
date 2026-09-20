# WorkoutLog2

A personal hybrid-training system: a workout journal and an analytics engine,
built around a **file-based, human-readable, dependency-free** data format.

## What this is

I train a hybrid strength + conditioning practice on a 12-day cycle (A1–F2),
alternating heavy work with CrossFit-style conditioning. I have several years of
handwritten and CSV training logs. This project turns that into a queryable
system that answers real programming questions instead of just storing rows.

## Design principles

1. **Files are the source of truth.** One JSON file per session. No database.
   Human-readable, git-friendly, greppable, survives every framework I might use.
2. **The analytics engine is the product.** Storage is boring on purpose; the
   value is in the metrics (see `docs/01-training-principles.md`).
3. **The core owns the domain.** No business logic in the UI layer, so a port to
   another toolkit is a re-skin, not a rewrite.
4. **The format tolerates history.** The importer must read the real notation I
   used over years, which drifted. See `docs/03-log-notation.md`.

Principle 3 has now been cashed in once. The app began as SwiftUI over a Swift
core and was rewritten in Flutter over a Dart core. The files did not change —
`data/`, `exercises.json` and `cycles.json` were read by the new implementation
unmodified. The core was *re-expressed* rather than reused, though: sharing it
across toolkits would have meant a Rust core and a binding layer (ADR-005), and
an ~800-line domain was cheaper to port than to bind. The portable asset was the
format, exactly as ADR-001 claimed; the code was not.

## Repository layout

```
data/                     one JSON file per session — the source of truth
exercises.json            exercise catalogue: canonical names, aliases, muscles
cycles.json               reusable cycle definitions (session templates)
*.schema.json             JSON Schema for a session file and for cycles
docs/                     design docs and decisions
flutter/                  the app — Flutter, all six targets
flutter/packages/workout_log_core/   the domain core — pure Dart, no Flutter
```

## Documents

| Doc | Purpose |
|---|---|
| `docs/01-training-principles.md` | The training methodology the analytics must serve |
| `docs/02-data-model.md` | Domain model and JSON session format |
| `docs/03-log-notation.md` | How my handwritten/CSV notation maps to the model |
| `docs/04-analytics.md` | The metrics the engine computes and why |
| `docs/05-architecture.md` | Architecture decisions (ADR-style) |
| `docs/06-roadmap.md` | Build phases |

## Running it

```
cd flutter
WORKOUTLOG_DATA=../data flutter run -d macos      # or -d chrome
flutter analyze && flutter test
cd packages/workout_log_core && dart test         # the domain suite
```

## Current phase

**Phase 1** — entry app: enter sessions, write JSON files. Backfill the
paper/CSV history. Analytics comes next, once real data is in the format.
