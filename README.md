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

Principle 3 has now been cashed in twice. The app began as SwiftUI over a Swift
core; reaching Linux and Windows added a second UI (Dear ImGui over SDL3) over a
second domain core, in C++ — two implementations of one set of rules, the exact
drift ADR-004 exists to prevent. Flutter over a Dart core replaced all of it, and
Rust over a Rust core has now replaced that. Each time the domain was ported
against its own test suite and the old tree was deleted rather than kept, which
is the only version of this that stays honest.

The files never moved: `data/`, `exercises.json` and `cycles.json` were read by
each implementation unmodified. The portable asset was the format, exactly as
ADR-001 claimed. The code was not — it was re-expressed three times.

## Repository layout

```
data/                     one JSON file per session — the source of truth
exercises.json            exercise catalogue: canonical names, aliases, muscles
cycles.json               reusable cycle definitions (session templates)
*.schema.json             JSON Schema for a session file and for cycles
docs/                     design docs and decisions
assets/muscle-map.svg     the anatomical map the analytics colour in
crates/workout-log-core/  the domain core — pure Rust, no I/O, no UI
crates/workout-log-app/   the app — Dioxus + Rust/UI, desktop and web
crates/workout-log-mcp/   an MCP server over the same core
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
cargo test -p workout-log-core -p workout-log-mcp      # the domain + server suites
cd crates/workout-log-app
WORKOUTLOG_DATA=../../data dx serve --platform desktop # the app
```

## The three screens

- **List** — the session editor: header fields, block cards, and the notation
  field that turns `6 × [70, 80, 90, 100, 110]` into sets.
- **Cycle** — what actually happened: exercises by cycle day, a month calendar,
  and the muscle map of the whole cycle.
- **Plan** — what is *going* to happen: create, clone and edit cycles in
  `cycles.json`, add workouts following the A1/A2 convention, pick exercises
  from the catalogue or add new ones, and see each planned day's muscle map.

## Talking to it

`mcp/` serves the log over MCP, so an assistant can enter a session, fill a slot
from the paper notation, and ask the archive the questions in
`docs/04-analytics.md` — rep-band distribution, pattern frequency, top set
against back-off volume, density, metcon splits. It is a surface over
`workout-log-core`, not a second implementation of it (ADR-008); it writes the
same canonical JSON the app writes, and refuses anything that would not parse
back. The repo ships a `.mcp.json`, so a client started here picks it up:

```
cargo test -p workout-log-mcp
```

## Current phase

**Phase 1** — entry app: enter sessions, write JSON files. Backfill the
paper/CSV history. Analytics comes next, once real data is in the format.
