# 06 — Roadmap

## Phase 1 — macOS entry app *(current)*

Goal: **get years of training out of the notebook and into the format.**

- [ ] `exercises.json` catalogue: canonical names, aliases, movement patterns
- [ ] Session model + `Codable` (see `doc/02-data-model.md`)
- [ ] JSON read/write to a session folder, one file per session
- [ ] Entry UI: session header → blocks → sets
- [ ] Block editors: cardio, strength (ramp), metcon, cooldown
- [ ] Notation shortcuts that mirror the paper log — this is the make-or-break
      usability point, see below
- [ ] Validation: warn, never block. A weird session is still a real session.
- [ ] Backfill the paper journal and the CSV history

**Why the macOS entry app first, before iOS.** Backfilling years of history is a
keyboard job, not a phone job. It also forces the schema to meet real data
immediately — every gap in the model shows up while transcribing, which is the
cheapest possible time to find it. Expect the schema to change during this phase.
That is the phase working, not failing.

**The critical usability requirement.** Entry must be as fast as writing
`6 × [70, 80, 90, 100, 110]`. If entering a session is slower than the notebook,
the app is dead and the notebook wins — correctly. Terse text input that parses
the real notation beats a grid of dropdowns. Type the line, get the sets.

## Phase 2 — Analytics (macOS)

Real data already exists in the format, so charts can be validated against
sessions I remember.

- [ ] Load the whole folder into memory; derive, don't store
- [ ] Progress tracks per (pattern, variant, rep_band)
- [ ] **Rep-band distribution alert** — the P5 guard, highest-value metric
- [ ] Pattern frequency per cycle
- [ ] Session density from bracket timestamps
- [ ] Metcon pacing decay + heart-rate response
- [ ] Postponed-session overlay against load
- [ ] Deload/retest anchors on every chart

## Phase 3 — iOS journal

Capture at the gym. Shares the format and the core with macOS; iCloud Drive syncs
the folder.

- [ ] Session entry, glanceable mid-workout
- [ ] Rest timer, metcon round splits, heart-rate capture
- [ ] Previous performance on this lift, visible while lifting

## Phase 4 — Cross-platform

- [ ] Extract core to Rust (`uniffi`) if not already — see ADR-005
- [ ] Android journal (Compose)
- [ ] Linux/Windows analytics (UI re-skin, same core)
- [ ] Replace iCloud Drive with a portable folder sync

## Sequencing rationale

The critical path is **the format plus the migrated history**, not any app. Once
the archive is in the format, everything else is a view over it — and the archive
is the asset that survives every framework decision I might later regret.
