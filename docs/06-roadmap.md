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

## Phase 3 — The platforms that are configured but unproven

One Flutter codebase reaches all six, but the tree only carries runners for four:
macOS, iOS, Linux and web. Two are real today; the other two are scaffolded and
analyzed but have never been built or run.

- [x] macOS — builds and runs against the real `data/`
- [x] Web — builds; holds a working copy and imports/exports (ADR-007)
- [ ] iOS — capture at the gym: rest timer, metcon round splits, heart rate,
      and previous performance on this lift visible while lifting
- [ ] Android — same, once an SDK is installed; its runner was dropped as
      untouched scaffolding and comes back with
      `flutter create --platforms=android .`
- [ ] Linux and Windows — need those hosts to build on, and Windows needs its
      runner regenerated the same way
- [ ] Portable folder sync to replace iCloud Drive (ADR-003 makes this a
      configuration choice, not a code change)

## Phase 4 — Things the current UI cannot do

- [ ] Per-muscle hit-testing and tooltips on the muscle map. This needs the SVG
      parsed into paths and painted by a `CustomPainter` rather than handed to
      `flutter_svg` as a string — a significant chunk of work, deferred on purpose.
- [ ] Block and set editing beyond the notation field
- [ ] Metcon round and split entry

## Sequencing rationale

The critical path is **the format plus the migrated history**, not any app. Once
the archive is in the format, everything else is a view over it — and the archive
is the asset that survives every framework decision I might later regret. The
Flutter port was that claim being tested: the files were read unchanged by a new
implementation in a different language. They were.
