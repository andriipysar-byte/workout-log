# 02 — Data model

## Storage layout

One JSON file per session. Filename encodes date and cycle day, so the directory
sorts chronologically and every session is independently addressable:

```
sessions/
  2026-06-23_C2.json
  2026-06-25_D1.json
  2026-06-28_D2.json
  2026-07-05_F1.json
  2026-07-07_F2.json
  2026-08-10_F1.json
exercises.json          catalogue: name → movement pattern, aliases
cycles.json             cycle definitions (A1…F2), deloads, retests
```

**Why one file per session:** append-only in practice, no merge conflicts, git
diffs are readable, a corrupted file costs one session and not the archive, and
sync (iCloud Drive / git / anything) is a folder sync with no schema migration.

## Core entities

### Session
| Field | Notes |
|---|---|
| `date` | ISO 8601 |
| `cycle_day` | `A1`…`F2` |
| `start_time` | wall clock |
| `end_time` | derived from last block |
| `kind` | `training` \| `deload` \| `retest` |
| `bodyweight` | optional |
| `notes` | free text |
| `blocks[]` | ordered |

### Block (tagged union on `type`)
Every block carries `end_time` — the bracket timestamp from the paper log. This
is what makes duration, transition time, and **density** computable.

- `cardio` — `machine`, `duration_min`, `distance_m`
- `strength` — `exercise`, `sets[]`
- `metcon` — `scheme`, `exercises[]`, `rounds[]`
- `cooldown`

### Set
The atomic record. Polymorphic on how effort is measured:

| Field | Notes |
|---|---|
| `weight_kg` | optional (bodyweight movements) |
| `reps` | when the unit is repetitions |
| `duration_sec` | when the unit is a hold |
| `cluster` | `[5,5,4,3,3]` for cluster sets |
| `rir` | reps in reserve — **the P10 fatigue guard** |
| `rep_band` | `heavy` (≤3) \| `base` (4–6) \| `volume` (7+) — **P6** |
| `bar_speed` | optional, for explosive lifts — **P3** |
| `is_backoff` | distinguishes back-off from ramp sets — **P2** |
| `planned` vs actual | where the paper log shows a struck-through value |

### Metcon round
| Field | Notes |
|---|---|
| `round` | index |
| `reps` | 21 / 15 / 9 |
| `split_cumulative_sec` | as written on paper |
| `split_round_sec` | derived by differencing |
| `heart_rate` | ЧСС at end of round |

### Exercise (catalogue)
| Field | Notes |
|---|---|
| `name` | canonical, Ukrainian |
| `aliases[]` | historical spellings across years of logs |
| `pattern` | `squat` \| `hinge` \| `press` \| `pull` \| `olympic` \| `carry` \| `core` \| `grip` — **P8, the key grouping** |
| `category` | `power` \| `speed` (both drive the P3 rule) \| `strength` \| `longevity` — force–velocity / training emphasis |
| `modality` | `barbell` \| `dumbbell` \| `kettlebell` \| `bodyweight` \| `machine` |
| `primary_muscles[]` / `secondary_muscles[]` | muscle targets (English snake_case vocab) for analytics grouping |

**Pattern is the load-bearing field.** Front squat, overhead squat and back squat
are three *variants of one pattern*. Without this, conjugate rotation looks like
a scatter of unrelated exercises with no progress — the exact failure P8 warns of.

## Example session

`2026-08-10_F1.json` — transcribed from the paper log. See `examples/`.

```jsonc
{
  "date": "2026-08-10",
  "cycle_day": "F1",
  "start_time": "08:02",
  "kind": "training",
  "blocks": [
    {
      "type": "cardio",
      "machine": "велотренажер",
      "duration_min": 15,
      "distance_m": 6440,
      "end_time": "08:18"
    },
    {
      "type": "strength",
      "exercise": "гіперекстензія",
      "sets": [
        { "reps": 12, "weight_kg": 15 },
        { "reps": 12, "weight_kg": 30 }
      ],
      "end_time": "08:28"
    },
    {
      "type": "metcon",
      "scheme": [21, 15, 9],
      "start_time": "08:30",
      "exercises": [
        { "name": "трастери", "weight_kg": 50 },
        { "name": "бьорпі" },
        { "name": "перекидання м'яча" }
      ],
      "rounds": [
        { "round": 1, "reps": 21, "split_cumulative_sec": 463, "heart_rate": 156 },
        { "round": 2, "reps": 15, "split_cumulative_sec": 1155, "heart_rate": 178 },
        { "round": 3, "reps": 9,  "split_cumulative_sec": 1450, "heart_rate": 189 }
      ],
      "end_time": "08:54"
    },
    {
      "type": "strength",
      "exercise": "підйом на середню дельту",
      "sets": [
        { "reps": 6, "weight_kg": 12 },
        { "reps": 6, "weight_kg": 14 },
        { "reps": 6, "weight_kg": 16 },
        { "reps": 6, "weight_kg": 18 }
      ],
      "end_time": "09:12"
    },
    {
      "type": "strength",
      "exercise": "підтягування з паузами",
      "sets": [
        { "cluster": [5, 5, 4, 3, 3], "total_reps": 20 }
      ],
      "end_time": "09:28"
    },
    { "type": "cooldown", "end_time": "09:38" }
  ]
}
```

## Deliberate omissions

- **No IDs, no foreign keys.** Files are the identity. Exercise names resolve
  against `exercises.json` by name + alias.
- **No sync metadata in the session file.** Sync is a folder concern, not a
  document concern — keeps the format portable to Android/Linux later.
- **No computed fields stored.** Round splits, tonnage, density and 1RM estimates
  are derived at read time. The file holds only what was observed.
