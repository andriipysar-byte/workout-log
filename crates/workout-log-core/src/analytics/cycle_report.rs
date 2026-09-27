//! What a stretch of sessions says as a block rather than one at a time: band
//! distribution, pattern frequency, combined load, and the alerts those raise.

use std::collections::{BTreeMap, BTreeSet};

use chrono::NaiveDate;
use serde_json::{Map, Value, json};

use crate::analytics::progress::pattern_name;
use crate::analytics::session_metrics::{BandCounts, SessionMetrics, TimeDomain, kind_name};
use crate::models::block::Block;
use crate::models::exercise::{Catalogue, MovementPattern};
use crate::models::session::{Kind, Session};
use crate::models::work_set::{RepBand, WorkSet};

fn round(value: f64) -> f64 {
    (value * 10.0).round() / 10.0
}

/// Dart prints an integral double as `1.0`, and the alert text is part of the
/// contract, so the formatting has to match rather than print `1`.
fn dart_double(value: f64) -> String {
    if value.is_finite() && value.fract() == 0.0 {
        format!("{value:.1}")
    } else {
        format!("{value}")
    }
}

/// A finding worth a training decision, tagged with the principle it comes from.
/// The system reports; the programming decisions stay the athlete's.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TrainingAlert {
    /// `P3`, `P5`, `P8`, `P10` — see `docs/01-training-principles.md`.
    pub principle: String,
    pub message: String,
    pub sessions: Vec<String>,
}

impl TrainingAlert {
    pub fn to_json(&self) -> Value {
        let mut json = Map::new();
        json.insert("principle".into(), json!(self.principle));
        json.insert("message".into(), json!(self.message));
        if !self.sessions.is_empty() {
            json.insert("sessions".into(), json!(self.sessions));
        }
        Value::Object(json)
    }
}

/// How often a movement pattern was trained — the frequency variety is bought
/// with (P8). Below roughly one slot per cycle, linear progression is dead.
#[derive(Debug, Clone, PartialEq)]
pub struct PatternFrequency {
    pub pattern: MovementPattern,
    pub slots: usize,
    pub sessions: usize,
    pub per_cycle: f64,
}

impl PatternFrequency {
    pub fn to_json(&self) -> Value {
        json!({
            "pattern": pattern_name(self.pattern),
            "slots": self.slots,
            "sessions": self.sessions,
            "per_cycle": self.per_cycle,
        })
    }
}

/// Strength and conditioning side by side, never fused into one score: fatigue
/// is additive and the point is to see when both rise together (P7).
#[derive(Debug, Clone, PartialEq)]
pub struct LoadPoint {
    pub date: String,
    pub cycle_day: String,
    pub kind: Kind,
    pub tonnage_kg: f64,
    pub sets: usize,
    pub metcon_sec: Option<f64>,
    pub work_min: Option<f64>,
}

impl LoadPoint {
    pub fn to_json(&self) -> Value {
        let mut json = Map::new();
        json.insert("date".into(), json!(self.date));
        json.insert("cycle_day".into(), json!(self.cycle_day));
        json.insert("kind".into(), json!(kind_name(self.kind)));
        json.insert("tonnage_kg".into(), json!(self.tonnage_kg));
        json.insert("sets".into(), json!(self.sets));
        if let Some(v) = self.metcon_sec {
            json.insert("metcon_sec".into(), json!(v));
        }
        if let Some(v) = self.work_min {
            json.insert("work_min".into(), json!(v));
        }
        Value::Object(json)
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct TrainingReport {
    pub from: Option<String>,
    pub to: Option<String>,
    pub days: Option<i64>,
    pub cycle_length_days: i64,
    pub session_count: usize,
    pub kinds: BTreeMap<Kind, usize>,
    pub band_sets: BandCounts,
    pub band_slots: BandCounts,
    pub patterns: Vec<PatternFrequency>,
    pub load: Vec<LoadPoint>,
    pub time_domains: BTreeMap<String, usize>,
    pub alerts: Vec<TrainingAlert>,
    /// Names no catalogue entry resolves: surfaced, never silently dropped (ADR-006).
    pub unknown_exercises: Vec<String>,
}

const BAND_ORDER: [RepBand; 3] = [RepBand::Heavy, RepBand::Base, RepBand::Volume];

fn band_index(band: RepBand) -> usize {
    match band {
        RepBand::Heavy => 0,
        RepBand::Base => 1,
        RepBand::Volume => 2,
    }
}

fn band_name(band: RepBand) -> &'static str {
    crate::analytics::progress::band_name(band)
}

fn band_json(counts: &BandCounts) -> Value {
    let mut json = Map::new();
    for band in BAND_ORDER {
        let count = counts[band_index(band)];
        if count > 0 {
            json.insert(band_name(band).into(), json!(count));
        }
    }
    Value::Object(json)
}

impl TrainingReport {
    pub fn total_tonnage_kg(&self) -> f64 {
        round(self.load.iter().map(|p| p.tonnage_kg).sum())
    }

    /// `volume_slot_share_threshold` guards P5, the failure that actually
    /// happened: a block with every slot in the volume range left no light
    /// session to recover against and ended in an involuntary deload.
    pub fn of(
        sessions: &[Session],
        catalogue: &Catalogue,
        cycle_length_days: i64,
        volume_slot_share_threshold: f64,
    ) -> Self {
        let mut ordered: Vec<&Session> = sessions.iter().collect();
        ordered.sort_by(|a, b| a.date.cmp(&b.date));

        let mut band_sets: BandCounts = [0; 3];
        let mut band_slots: BandCounts = [0; 3];
        let mut pattern_slots: BTreeMap<MovementPattern, usize> = BTreeMap::new();
        let mut pattern_sessions: BTreeMap<MovementPattern, BTreeSet<String>> = BTreeMap::new();
        let mut time_domains: BTreeMap<String, usize> = BTreeMap::new();
        let mut kinds: BTreeMap<Kind, usize> = BTreeMap::new();
        let mut load = Vec::new();
        let mut unknown: BTreeSet<String> = BTreeSet::new();
        let mut alerts = Vec::new();
        let mut explosive_offenders: BTreeSet<String> = BTreeSet::new();
        let mut decay_offenders: BTreeSet<String> = BTreeSet::new();

        for session in &ordered {
            let metrics = SessionMetrics::of(session);
            *kinds.entry(session.kind).or_insert(0) += 1;
            for index in 0..3 {
                band_sets[index] += metrics.band_sets[index];
                band_slots[index] += metrics.band_slots[index];
            }
            for metcon in &metrics.metcons {
                if let Some(domain) = metcon.domain() {
                    *time_domains.entry(domain.name().to_string()).or_insert(0) += 1;
                }
            }
            load.push(LoadPoint {
                date: session.date.clone(),
                cycle_day: session.cycle_day.clone(),
                kind: session.kind,
                tonnage_kg: metrics.tonnage_kg,
                sets: metrics.sets,
                metcon_sec: (!metrics.metcons.is_empty()).then(|| {
                    metrics
                        .metcons
                        .iter()
                        .map(|m| m.total_sec.unwrap_or(0.0))
                        .sum()
                }),
                work_min: (metrics.density.work_min > 0.0).then_some(metrics.density.work_min),
            });

            for block in &session.blocks {
                let names: Vec<String> = match block {
                    Block::Strength(s) => vec![s.exercise.clone()],
                    Block::Metcon(m) => m.exercises.iter().map(|e| e.name.clone()).collect(),
                    _ => Vec::new(),
                };
                for name in &names {
                    let Some(resolved) = catalogue.resolve(name) else {
                        if !name.trim().is_empty() {
                            unknown.insert(name.clone());
                        }
                        continue;
                    };
                    let Some(pattern) = resolved.pattern else {
                        continue;
                    };
                    if matches!(block, Block::Strength(_)) {
                        *pattern_slots.entry(pattern).or_insert(0) += 1;
                    }
                    pattern_sessions
                        .entry(pattern)
                        .or_default()
                        .insert(session.date.clone());
                }

                if let Block::Strength(strength) = block
                    && let Some(resolved) = catalogue.resolve(&strength.exercise)
                    && resolved.is_explosive()
                {
                    let label = format!("{} {}", session.date, strength.exercise);
                    if strength
                        .sets
                        .iter()
                        .any(|s| s.recorded_reps().unwrap_or(0) > 3)
                    {
                        explosive_offenders.insert(label.clone());
                    }
                    if reps_collapse(&strength.sets) {
                        decay_offenders.insert(label);
                    }
                }
            }
        }

        let from = ordered.first().map(|s| s.date.clone());
        let to = ordered.last().map(|s| s.date.clone());
        let days = days_between(from.as_deref(), to.as_deref());
        let cycles = days.map(|d| d as f64 / cycle_length_days as f64);

        let patterns: Vec<PatternFrequency> = MovementPattern::ALL
            .into_iter()
            .filter(|p| {
                pattern_slots.get(p).copied().unwrap_or(0) > 0
                    || pattern_sessions.get(p).is_some_and(|s| !s.is_empty())
            })
            .map(|pattern| {
                let slots = pattern_slots.get(&pattern).copied().unwrap_or(0);
                PatternFrequency {
                    pattern,
                    slots,
                    sessions: pattern_sessions.get(&pattern).map_or(0, BTreeSet::len),
                    per_cycle: match cycles {
                        Some(c) if c > 0.0 => round(slots as f64 / c),
                        _ => 0.0,
                    },
                }
            })
            .collect();

        let total_slots: usize = band_slots.iter().sum();
        let volume_slots = band_slots[band_index(RepBand::Volume)];
        if total_slots > 0 && volume_slots as f64 / total_slots as f64 > volume_slot_share_threshold
        {
            alerts.push(TrainingAlert {
                principle: "P5".into(),
                message: format!(
                    "volume (7+) slots are {}% of {total_slots} slots, over the {}% guard — the state that produced the involuntary deload",
                    (volume_slots as f64 / total_slots as f64 * 100.0).round(),
                    (volume_slot_share_threshold * 100.0).round()
                ),
                sessions: Vec::new(),
            });
        }
        if !explosive_offenders.is_empty() {
            alerts.push(TrainingAlert {
                principle: "P3".into(),
                message: "explosive lifts logged above 3 reps — bar speed decays and the lift is trained in the wrong zone".into(),
                sessions: explosive_offenders.into_iter().collect(),
            });
        }
        if !decay_offenders.is_empty() {
            alerts.push(TrainingAlert {
                principle: "P3".into(),
                message:
                    "reps collapse across the ramp on an explosive lift (the 4 / 2 / 1 pattern)"
                        .into(),
                sessions: decay_offenders.into_iter().collect(),
            });
        }
        if let Some(cycles) = cycles
            && cycles >= 1.0
        {
            let starved: Vec<String> = patterns
                .iter()
                .filter(|p| p.per_cycle < 1.0)
                .map(|p| {
                    format!(
                        "{} ({}/cycle)",
                        pattern_name(p.pattern),
                        dart_double(p.per_cycle)
                    )
                })
                .collect();
            if !starved.is_empty() {
                alerts.push(TrainingAlert {
                    principle: "P8".into(),
                    message: format!(
                        "trained below once per {cycle_length_days}-day cycle, so linear progression on them is dead: {}",
                        starved.join(", ")
                    ),
                    sessions: Vec::new(),
                });
            }
        }
        if let (Some(cycles), Some(days)) = (cycles, days)
            && cycles >= 2.0
            && kinds.get(&Kind::Deload).copied().unwrap_or(0) == 0
            && kinds.get(&Kind::Retest).copied().unwrap_or(0) == 0
        {
            alerts.push(TrainingAlert {
                principle: "P10".into(),
                message: format!(
                    "no deload or retest in {days} days — deloads are hygiene, and progress between anchors is the unit of evaluation"
                ),
                sessions: Vec::new(),
            });
        }

        Self {
            from,
            to,
            days,
            cycle_length_days,
            session_count: ordered.len(),
            kinds,
            band_sets,
            band_slots,
            patterns,
            load,
            time_domains,
            alerts,
            unknown_exercises: unknown.into_iter().collect(),
        }
    }

    pub fn to_json(&self) -> Value {
        let mut json = Map::new();
        if let Some(from) = &self.from {
            json.insert("from".into(), json!(from));
        }
        if let Some(to) = &self.to {
            json.insert("to".into(), json!(to));
        }
        if let Some(days) = self.days {
            json.insert("days".into(), json!(days));
        }
        json.insert("sessions".into(), json!(self.session_count));
        json.insert(
            "kinds".into(),
            Value::Object(
                Kind::ALL
                    .into_iter()
                    .filter_map(|kind| {
                        let count = self.kinds.get(&kind).copied().unwrap_or(0);
                        (count > 0).then(|| (kind_name(kind).to_string(), json!(count)))
                    })
                    .collect(),
            ),
        );
        json.insert(
            "rep_bands".into(),
            json!({"sets": band_json(&self.band_sets), "slots": band_json(&self.band_slots)}),
        );
        json.insert(
            "patterns".into(),
            Value::Array(
                self.patterns
                    .iter()
                    .map(PatternFrequency::to_json)
                    .collect(),
            ),
        );
        json.insert(
            "time_domains".into(),
            Value::Object(
                TimeDomain::ALL
                    .into_iter()
                    .filter_map(|domain| {
                        let count = self.time_domains.get(domain.name()).copied().unwrap_or(0);
                        (count > 0).then(|| (domain.name().to_string(), json!(count)))
                    })
                    .collect(),
            ),
        );
        json.insert("total_tonnage_kg".into(), json!(self.total_tonnage_kg()));
        json.insert(
            "load".into(),
            Value::Array(self.load.iter().map(LoadPoint::to_json).collect()),
        );
        json.insert(
            "alerts".into(),
            Value::Array(self.alerts.iter().map(TrainingAlert::to_json).collect()),
        );
        if !self.unknown_exercises.is_empty() {
            json.insert("unknown_exercises".into(), json!(self.unknown_exercises));
        }
        Value::Object(json)
    }
}

/// The 4 / 2 / 1 shape: three or more recorded sets whose reps only ever fall.
fn reps_collapse(sets: &[WorkSet]) -> bool {
    let reps: Vec<i64> = sets
        .iter()
        .filter_map(|s| s.recorded_reps().filter(|&r| r > 0))
        .collect();
    reps.len() >= 3 && reps.windows(2).all(|pair| pair[1] < pair[0])
}

fn days_between(from: Option<&str>, to: Option<&str>) -> Option<i64> {
    let parse = |t: &str| NaiveDate::parse_from_str(t, "%Y-%m-%d").ok();
    let (start, end) = (parse(from?)?, parse(to?)?);
    Some((end - start).num_days() + 1)
}
