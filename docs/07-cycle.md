# 07 — The 8-session cycle

Supersedes the 12-day A1–F2 cycle. Designed against the principles in `01`.

## Calendar structure

Training days are **Tuesday, Thursday, Sunday**. The cycle is **8 sessions plus
one skipped slot = 9 slots = exactly 3 weeks**, so every session keeps a fixed
weekday and the cycle never drifts.

| | Tuesday | Thursday | Sunday |
|---|---|---|---|
| **Week 1** | S1 · Metcon *(ривок)* | S2 · **Присід на плечах** — triples | S3 · Metcon *(взяття)* |
| **Week 2** | S4 · **Жим лежачи** — sixes | S5 · Metcon *(протяжка)* | S6 · **Присід фронтальний** — sixes |
| **Week 3** | S7 · Metcon *(поштовх / застрибування)* | S8 · **Жим стоячи / нахил** — eights | *skipped* |

Metcon and heavy alternate without a break in the pattern.

## Why this structure

**Pattern frequency, not lift frequency.** — P8
Each named lift recurs once per 21 days, which looks sparse. The number that
matters is the pattern:

- squat pattern — back squat (S2) + front squat (S6) → every ~10 days
- press pattern — bench (S4) + overhead/incline (S8) → every ~10 days

Two variants per pattern, each carrying a different rep band. Pattern frequency
and independent progress tracks are satisfied at the same time.

**Rep-band spread.** — P5, P6
Triples (S2), sixes (S4, S6), eights (S8). One volume slot out of four, so the
"never hold all slots in the heavy volume range" rule holds by construction. Four
independent progress tracks, one per slot.

**Recovery is built in.** — P10
The skipped Sunday puts a **5-day gap** between S8 (Thursday) and the next S1
(Tuesday). A mini-deload every 3 weeks, already part of the practice rather than
something to remember. The heaviest session (S2, back squat triples) sits on
Thursday, ahead of the longest mid-week gap.

**Load is unchanged.** — P7
The metcon:heavy ratio stays 1:1 and session frequency is untouched. This is a
structural change, not a dose increase — deliberately not a repeat of the
all-eights block.

## Pattern coverage

Four main slots cover squat and press only. The gaps — hinge, vertical pull,
vertical press — are closed deliberately elsewhere:

| Pattern | Where it lives |
|---|---|
| Squat | S2, S6 (main) |
| Horizontal press | S4 (main) |
| Vertical press | S8 (main, if жим стоячи) — otherwise accessory |
| Hinge | Explosive slot on metcon days (ривок, взяття, протяжка) |
| Vertical pull | Accessory on S2 and S8 (підтягування з вагою) |
| Grip | Accessory, most sessions (вис, прокручування кисті) |

**The explosive slot is structural, not decorative.** The olympic lifts are the
only hinge-pattern work under real load in the cycle. Choose them to fill the
gap, not by preference.

## Session templates

### Heavy session
1. Cardio warm-up, 10–15 min
2. Гіперекстензія 12 × 2
3. **Main lift** in its rep band — ramp to top set + 1–2 back-off sets
4. Accessory 1 (pattern gap)
5. Accessory 2
6. Grip / core

Accessories by session:

| Session | Main | Accessories |
|---|---|---|
| S2 | Присід на плечах | Підтягування з вагою · Згинання ніг |
| S4 | Жим лежачи | Тяга блочна · Середня дельта |
| S6 | Присід фронтальний | Румунська тяга · Підйоми на носки |
| S8 | Жим стоячи | Жим на нахилі · Підтягування |

### Metcon session
1. Cardio warm-up, 10 min
2. **Explosive lift: 3–5 × 2–3**, full recovery, stop on bar-speed drop — P3
3. Комплекс
4. 1–2 accessories (delts / grip)
5. Заминка

**Time budget — the real constraint.** The logged F1 session ran 08:02→09:38
(1h36) *before* the explosive slot existed. To stay inside 1.5h on metcon days,
drop гіперекстензія (already on every heavy session) and cut the cardio warm-up
to 10 min. Otherwise the new slot silently eats the session cap.

## Progression

**Method.** Ramp to a top set in the slot's rep band, plus back-off sets. Log the
top set as the progress signal; back-offs are volume, never averaged in. — P2

**Explosive lifts** run at 2–3 reps regardless of the slot's band, stopping on
bar-speed drop rather than on rep count. — P3

**When a slot stalls,** change the band rather than adding sets: drop to 3–4 and
push weight, or move up to 8–10, then return. Volume is the expensive lever and
is not the answer here. — P4

## Two runs before vacation (6 weeks)

**Run 1 — calibration.** Find working weights in the new bands. Back squat
triples and overhead press eights are unfamiliar formats — set them
conservatively, hold RIR 2–3, record everything. Explosive lifts: technique and
bar speed, weight secondary.

**Run 2 — progression.** Add weight wherever run 1 left 2+ reps in reserve. Same
scheme, no structural changes. **Do not test maxes on S8 of run 2** — entering the
vacation unfatigued is worth more than one PR.

## Vacation block

Available: kettlebells, pull-up bar, bodyweight, running. Enough to hold every
pattern at low absolute load, which is exactly what a deload is: stimulus kept,
joint load dropped. Useful in particular for the squat pattern, which the cycle
loads every ~10 days.

| Pattern | Vacation work |
|---|---|
| Hinge / explosive | Махи, ривок гирі, поштовх |
| Vertical pull | Підтягування, вис на час (comparable to logged hang seconds) |
| Squat | Гоблет, пістолети |
| Press | Жим гирі стоячи, віджимання |
| Conditioning | Running — spread across time domains (P9), especially the short end |

Keep the alternation rhythm: conditioning day ↔ strength day.

**Retest 3–5 days after returning**, not before leaving. That is a clean anchor
for the next block and the first real point the analytics can measure a cycle
against. — P10

## What to watch

- **Squat pattern loads every ~10 days.** Knees and lower back are where fatigue
  will show first. This is the intended cost of higher frequency, but it is the
  thing to monitor.
- **Fixed main lifts go stale.** Rotate variants every 6–8 weeks, or rotate rep
  bands between cycles. The Sunday S6/S8 slots are the natural place for this.
- **The explosive slot must stay small.** If it grows into a workout, it stops
  being free and starts competing with the metcon.

## Data implications

Progress now lives at the **pattern** level, not the exercise-name level. Back
squat compares against its own previous instance (every 21 days), while pattern
loading is viewed across both squat variants together. This is what the `pattern`
field in `doc/02-data-model.md` exists for — and the reason exercise names alone
would show scatter rather than progress.
