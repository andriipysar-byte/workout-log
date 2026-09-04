# 04 — Analytics

Each metric answers a programming question that the principles in `01` raise.
If a metric doesn't change a training decision, it doesn't ship.

## Strength

**Progress track per (pattern, variant, rep_band).** — P6
Best set over time, sliced by rep band. Triples, sixes and eights are three
independent series on the same lift. A stall in one must be visibly separate from
the others.

**Top-set vs back-off separation.** — P2
The ramp inflates apparent volume. Report the top set as the progress signal and
back-off sets as accumulated volume; never average them together.

**Estimated 1RM per variant** (Epley/Brzycki) — a common scale to compare a
6-rep front squat against a 3-rep one. Use as a trend, not as truth.

**Bar-speed / rep-decay flag.** — P3
For explosive movements, warn when reps exceed 3 or when reps collapse across the
ramp (the Швунг `4 / 2 / 1` pattern). That's the signal the lift is being trained
in the wrong zone.

## Cycle structure

**Rep-band distribution per cycle.** — P5, the guard that failed before
Count slots by band. **Alert when the share of volume (7+) slots exceeds the
threshold** — that is precisely the state that produced the involuntary deload.
This is the single most valuable safety metric in the system.

**Pattern frequency per cycle.** — P8, P9
How often each movement pattern is trained per 12-day cycle. When variety pushes
a pattern below ~1× per cycle, linear progression on it is dead and the display
should say so rather than draw a hopeful trend line.

## Fatigue and recovery

**Postponed / skipped sessions.** — P1, P10
The primary fatigue signal, recorded explicitly. Overlay against rep-band
distribution and combined load: postponements should visibly cluster after
high-volume stretches. Confirming that in the data validates P4/P5 with my own
history rather than someone else's theory.

**Combined session load.** — P7
Strength tonnage *and* conditioning cost in the same view, since fatigue is
additive. Not a single fused "score" — two series side by side, because the point
is to see when both rise together.

**Session density.** — P1
From the bracket timestamps: work time vs total time, and transition time between
blocks. Density progression (same work, less time) is a real stimulus lever that
costs no extra exercises and no extra minutes.

**RIR trend on volume days.** — P10
Rising effort at constant load = accumulating fatigue.

## Conditioning

**Pacing decay within a metcon.** — from the derived round splits
Round 1 vs round 3 pace on a 21-15-9. Splits are cumulative on paper, so the
difference is where the information lives.

**Heart-rate response per round.** (156 → 178 → 189)
Track the same benchmark complex over months: the same work at a lower heart rate
is improved conditioning, and it is invisible without this data.

**Time-domain coverage.** — P9
Distribution of metcons across time domains (short alactic / glycolytic / longer
aerobic). Variety on the conditioning side is supposed to come from *formats and
domains*, so show whether it actually does.

## Anchors

**Deload and retest markers.** — P10
Rendered as vertical lines on every chart. Progress between anchors is the unit
of evaluation; a PR the week before a deload means something different from one
straight after.

## Explicit non-goals

- No "readiness score", no HRV-style single number, no gamification.
- No social features, no export to fitness platforms.
- No prescriptive AI coach. The system reports; the programming decisions stay mine.
