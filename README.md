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

Principle 3 has now been cashed in, and the bill came with it. The app began as
SwiftUI over a Swift core; reaching Linux and Windows added a second UI (Dear
ImGui over SDL3) over a second domain core, in C++. That is two implementations
of one set of rules — the exact drift ADR-004 exists to prevent, arrived at by
honouring ADR-004's letter in each tree separately. Flutter over a Dart core
replaced all of it, then a Rust workspace with a Dioxus webview UI replaced
that. The current stack is C++ with Qt Widgets — core, tools and UI moved
together, and each earlier tree was deleted rather than kept beside the next.

The files never moved: `data/`, `exercises.json` and `cycles.json` were read by
each implementation unmodified. The portable asset was the format, exactly as
ADR-001 claimed. The code was not — it was re-expressed five times.

## Repository layout

```
data/                     one JSON file per session — the source of truth
exercises.json            exercise catalogue: canonical names, aliases, muscles
cycles.json               reusable cycle definitions (session templates)
*.schema.json             JSON Schema for a session file and for cycles
docs/                     design docs and decisions
core/                     the domain core — C++20, no Qt, no filesystem
storage/                  the session folder on disk
tools/                    wl_fmt, wl_gen_cycle
app/                      the app — Qt 6 Widgets, macOS and Linux
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
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
WORKOUTLOG_DATA=$PWD/data build/dev/app/WorkoutLog.app/Contents/MacOS/WorkoutLog   # macOS
WORKOUTLOG_DATA=$PWD/data build/dev/app/WorkoutLog                                 # Linux
```

Needs CMake, Ninja, a C++20 compiler and Qt 6.4+ with Svg. The `headless`
preset builds the core and tools without Qt.

## The three screens

- **List** — the session editor: header fields, block cards, and the notation
  field that turns `6 × [70, 80, 90, 100, 110]` into sets.
- **Cycle** — what actually happened: exercises by cycle day, a month calendar,
  and the muscle map of the whole cycle.
- **Plan** — what is *going* to happen: create, clone and edit cycles in
  `cycles.json`, add workouts following the A1/A2 convention, pick exercises
  from the catalogue or add new ones, and see each planned day's muscle map.

## Current phase

**Phase 1** — entry app: enter sessions, write JSON files. Backfill the
paper/CSV history. Analytics comes next, once real data is in the format.
