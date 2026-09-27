# 06 — Roadmap

## Phase 1 — Entry app *(current)*

Goal: **get years of training out of the notebook and into the format.**

- [x] `exercises.json` catalogue: canonical names, aliases, movement patterns
- [x] Session model + codecs (see `docs/02-data-model.md`)
- [x] JSON read/write to a session folder, one file per session
- [x] Entry UI: session header → blocks → sets
- [x] Block editors: cardio, strength (ramp), metcon (read-only), cooldown
- [x] Notation shortcuts that mirror the paper log — this is the make-or-break
      usability point, see below
- [x] Validation: warn, never block. A weird session is still a real session.
- [x] Create and delete a session; new sessions expand from a `cycles.json` template
- [x] Cycle planning: create, clone and edit cycles; add, reorder, rename and
      remove workouts; choose exercises from the catalogue or add new ones; see
      each planned day's muscle map
- [ ] Backfill the paper journal and the CSV history

**Why a desktop entry app first, before the phone.** Backfilling years of history is
a keyboard job, not a phone job. It also forces the schema to meet real data
immediately — every gap in the model shows up while transcribing, which is the
cheapest possible time to find it. Expect the schema to change during this phase.
That is the phase working, not failing.

**The critical usability requirement.** Entry must be as fast as writing
`6 × [70, 80, 90, 100, 110]`. If entering a session is slower than the notebook,
the app is dead and the notebook wins — correctly. Terse text input that parses
the real notation beats a grid of dropdowns. Type the line, get the sets.

**The workout code.** A workout is `<letter><number>`: the letter groups workouts
by main muscle emphasis, the number is `1` for CrossFit and `2` for hard work —
A1, A2, B1, B2, … The planner picks both from a control rather than a text field,
so the convention holds across a cycle, and it uses the number to pre-fill the
day's blocks: a `1` day starts as warm-up, explosive lift, metcon, accessory,
grip, cooldown; a `2` day as warm-up, hyperextension, main lift, accessories,
grip, cooldown. Everything stays editable. `CycleDay.tryParse` returns null
rather than throwing, so a hand-written code outside the convention still reads.

**Not in scope, deliberately.** In the *session* editor: adding, reordering or
deleting a block; editing an individual set outside the notation field; editing a
metcon. Sessions are edited by retyping the notation line, and metcons are
transcribed by hand. Planned blocks, by contrast, are fully editable — that is
where structure is decided.

## Phase 2 — Analytics *(next)*

Nothing in `docs/04-analytics.md` is implemented yet. This is the next real work,
and the reason the project exists: the format and the app are in place, so the
metrics now have something to run against and sessions I remember to be validated
against.

- [ ] Load the whole folder into memory; derive, don't store
- [ ] 1RM estimates per (pattern, variant)
- [ ] Progress tracks per (pattern, variant, rep_band)
- [ ] **Rep-band distribution alert** — the P5 guard, highest-value metric
- [ ] Pattern frequency per cycle
- [ ] Session density from bracket timestamps
- [ ] Acute:chronic workload ratio
- [ ] Metcon pacing decay + heart-rate response
- [ ] Postponed-session overlay against load
- [ ] Deload/retest anchors on every chart

## Phase 3 — Platforms

The app is Qt Widgets over the C++ core, for the desktop (ADR-005, amendment 2).

- [x] macOS — builds and runs against the real `data/`
- [ ] Linux — builds and tests in CI; not yet run on a Linux desktop
- [ ] iOS capture at the gym (rest timer, metcon splits, heart rate, previous
      performance on this lift) — left with the Flutter app; would return as a
      separate surface over the same core and files
- [ ] Portable folder sync to replace iCloud Drive (ADR-003 makes this a
      configuration choice, not a code change)
- [ ] MCP server over `wlcore` (ADR-008), deleted with the Dart core

## Phase 4 — Things the current UI cannot do

- [ ] Per-muscle hit-testing and tooltips on the muscle map. QtSvg can render
      one element by id, so this needs ids on the template's paths and a hover
      pass over them — deferred on purpose.
- [ ] Block and set editing beyond the notation field
- [ ] Metcon round and split entry

## Sequencing rationale

The critical path is **the format plus the migrated history**, not any app. Once
the archive is in the format, everything else is a view over it — and the archive
is the asset that survives every framework decision I might later regret. The
Flutter port and then the C++ port were that claim being tested: the files were
read unchanged by new implementations in different languages. They were — the
C++ port re-encodes every one of them byte for byte.
