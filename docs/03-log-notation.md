# 03 — Log notation

Derived from the paper journal (photos of sessions C1, C2, D1, D2, F1, F2) and
the CSV plan. The importer and the entry app must both speak this notation,
because it is how I actually think while training.

> Some readings below are my best interpretation of the handwriting and are
> marked **(verify)**. Correct them before the importer is written.

## Session header

```
10.08.26   F1 (8:02)
```
`date` · `cycle_day` (A1…F2) · `start_time` (wall clock).

## The bracket timestamps — the hidden gem

Every block ends with a wall-clock time in square brackets:

```
Велотренажер   15 хв   6.44 км            [8:18]
Гіперекстенз.  12 × [15, 30]              [8:28]
Комплекс: (8:30)
  ...                                      [8:54]
Підйом на сер. дельту  6 × [12,14,16,18]  [9:12]
Підтяг. з паузами (20) 5+5+4+3+3          [9:28]
Заминка                                    [9:38]
```

These are **block end times**. They give, for free and across years:
- duration of every block,
- rest/transition time between blocks,
- total session duration,
- **session density** — the P1 lever, already measured, never analysed.

This must be preserved. `end_time` on every block.

## Block types

### 1. Cardio warm-up
```
Велотренажер  15 хв  6.44 км
Біг           15 хв  1.9 км
Гребля        10 хв  2348 м
Орбітрек      15 хв  2.24
```
→ `machine`, `duration`, `distance`. Distance units vary (km vs m) — normalise.

### 2. Ramp / straight sets (the dominant strength notation)
```
Присід фронтальний  6 × [70, 80, 90, 100, 110]
Кисть з гант.       6 × [12, 14, 16, 18, 20]
Тяга блочна сид.    6 × [50, 55, 60, 65, 70] + 6 × [30]
```
Reads: **fixed reps × list of ascending weights**, one set per weight.
The trailing `+ 6 × [30]` is a back-off set. Expands to N sets, each with its own
weight, all at the stated rep count.

### 3. Ramp with per-set rep overrides
```
Швунг  6 × [30, 60, 80, 86, 90]
                    4   2   1      ← reps written above the weight
```
When a set breaks the fixed rep count, the actual reps are written **above** that
weight. This is P3 in the wild: reps collapse as the bar gets heavy.
→ Each set needs its own optional `reps` override.

### 4. Cluster sets
```
Підтяг. з паузами (20)  5+5+4+3+3
Підтягування      (24)  4+4+3+2+2+2+2+1
```
Reps in a chain, total in parentheses. → `cluster: [5,5,4,3,3]`, `total: 20`.

### 5. Timed holds
```
Вис   4 × [54c, 40c, 36c, 42c]
Вис з упором в кол.  {60c, 70c, 30c, 35c}
```
`c` = секунди. → sets measured in **seconds**, not reps. The unit of a set is not
always "reps" — the model must allow `duration_sec` as the metric.

### 6. Комплекс (metcon) — the richest block
```
Комплекс: (8:30)
  Трастери (50кг)     21   15   9        ЧСС 156
  Бьорпі              21   15   9            178
  Перекидання м'яча   21   15   9            189
                      7'43"  19'15"  24'10"        [8:54]
```
Structure:
- a **rep scheme** across rounds (21-15-9; also seen 22-16-10),
- several **exercises** performed each round, some with load `(50 кг)`,
- a **split time per round** — these are **cumulative** (7:43 → 19:15 → 24:10),
  so per-round times must be derived by differencing,
- a **heart rate per round** (156 / 178 / 189) — rising across rounds.

This is a genuine conditioning dataset: pacing decay + cardiac response per round.
Nothing off-the-shelf captures it.

### 7. Cooldown
```
Заминка   [9:38]
```

## Cross-cutting notation

| Mark | Meaning |
|---|---|
| numbers struck through | in-session revision — the crossed value was planned, the written one was done |
| `(50 кг)` after a name | external load for that movement |
| `—//—` | separator between the complex and accessory work |
| `ч 156` | heart rate (ЧСС) |
| `c` suffix | seconds |
| `(20)`, `(24)` | cluster total |

## Import consequences

1. **The format drifted over years** (4-day → 8-day → 12-day cycles; CSV columns
   changed). The importer must tolerate several historical shapes, not one.
2. **Struck-through values** mean the paper log contains both intent and outcome.
   Where recoverable, keep `planned` vs `actual`.
3. **Cumulative splits** must be differenced on import, and both forms stored.
4. **Set unit is polymorphic**: reps, seconds, distance, or calories.
