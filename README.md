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
   value is in the metrics (see `doc/01-training-principles.md`).
3. **The core owns the domain.** No business logic in the UI layer, so a later
   Linux/Windows/Android port is a re-skin, not a rewrite.
4. **The format tolerates history.** The importer must read the real notation I
   used over years, which drifted. See `doc/03-log-notation.md`.

## Repository layout

```
doc/         design docs and decisions
schema/      JSON schema for a session file
examples/    real sessions transcribed from the paper journal
```

## Documents

| Doc | Purpose |
|---|---|
| `doc/01-training-principles.md` | The training methodology the analytics must serve |
| `doc/02-data-model.md` | Domain model and JSON session format |
| `doc/03-log-notation.md` | How my handwritten/CSV notation maps to the model |
| `doc/04-analytics.md` | The metrics the engine computes and why |
| `doc/05-architecture.md` | Architecture decisions (ADR-style) |
| `doc/06-roadmap.md` | Build phases |

## Current phase

**Phase 1** — macOS entry app: enter sessions, write JSON files. Backfill the
paper/CSV history. Analytics comes next, once real data is in the format.
