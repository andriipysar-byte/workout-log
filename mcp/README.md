# workout_log_mcp

An MCP server over the training log: create sessions, edit them, and ask the
archive the questions `docs/04-analytics.md` says are worth asking.

It is a surface, not a second implementation. Every rule it applies —
the notation grammar, validation, tonnage, rep bands, density, activation —
lives in `workout_log_core` (ADR-004), which is also what the Flutter app uses.
This package adds `dart:io` (the session folder) and the protocol.

## Running it

```
cd mcp && dart pub get
dart run bin/workout_log_mcp.dart [--repo <dir>] [--data <dir>]
```

`--repo` is the directory holding `exercises.json` and `cycles.json`, and
defaults to the nearest ancestor of the working directory that has
`cycles.json`. `--data` is the session folder, and defaults to
`$WORKOUTLOG_DATA`, else `<repo>/data`.

The repository ships a `.mcp.json` at its root, so a client started in the
repository picks the server up with no further configuration.

## Tools

| Tool | What it does |
|---|---|
| `list_sessions` | Summaries, filtered by date range, cycle day, kind or exercise |
| `read_session` | One session's JSON as it is on disk, plus its validation issues |
| `create_session` | A new file, from the cycle template for that day when one exists |
| `write_session` | Replace a session with an edited document; renames on a header move |
| `log_sets` | Fill a strength slot from the notation: `6 × [70, 80, 90, 100, 110]` |
| `delete_session` | Remove a file; requires `confirm` |
| `parse_notation` | What a notation line means, without writing anything |
| `list_exercises` | The catalogue, filtered, with the muscle vocabulary it uses |
| `add_exercise` | Add or replace an entry in `exercises.json` |
| `list_cycles` | The cycle definitions — the plan side |
| `generate_cycle` | Expand a cycle onto the calendar as session stubs |
| `analyze_session` | Tonnage, rep bands, top sets vs back-offs, density, metcon splits |
| `analyze_training` | Band distribution, pattern frequency, combined load, principle alerts |
| `exercise_progress` | Best set over time per rep band, with estimated 1RM |
| `muscle_activation` | Muscle scores for a session or a range, and the group rollup |
| `validate_archive` | Everything questionable across the whole folder |

## What it will not do

- Write a session that would not parse back. Errors refuse the write; warnings
  travel with the result, because a half-written plan is a normal state of a
  file (ADR-006).
- Delete without `confirm`: the folder is the only copy (ADR-001).
- Invent a load. The files hold what was observed; every derived number —
  splits, tonnage, density, 1RM estimates — is computed at read time and stored
  nowhere.

## Verify

```
cd mcp && dart analyze && dart test
```
