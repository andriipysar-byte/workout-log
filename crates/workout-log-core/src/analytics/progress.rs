//! Best set over time for one lift, one track per rep band.

use std::collections::BTreeMap;

use serde_json::{Map, Value, json};

use crate::analytics::session_metrics::kind_name;
use crate::models::block::{Block, StrengthBlock};
use crate::models::exercise::{Catalogue, MovementPattern};
use crate::models::session::{Kind, Session};
use crate::models::work_set::{BarSpeed, RepBand, WorkSet};

fn round(value: f64) -> f64 {
    (value * 10.0).round() / 10.0
}

/// Estimated one-rep max: a common scale for comparing a six against a triple.
/// A trend line, never a truth — both formulas drift badly above ~10 reps.
pub mod one_rep_max {
    pub fn epley(weight_kg: Option<f64>, reps: Option<i64>) -> Option<f64> {
        let (w, r) = (weight_kg?, reps?);
        if r < 1 || w <= 0.0 {
            return None;
        }
        Some(if r == 1 {
            w
        } else {
            w * (1.0 + r as f64 / 30.0)
        })
    }

    pub fn brzycki(weight_kg: Option<f64>, reps: Option<i64>) -> Option<f64> {
        let (w, r) = (weight_kg?, reps?);
        if !(1..37).contains(&r) || w <= 0.0 {
            return None;
        }
        Some(w * 36.0 / (37 - r) as f64)
    }

    /// The mean of the two: they bracket the real number from either side, and
    /// picking one would import its bias into every chart.
    pub fn estimate(weight_kg: Option<f64>, reps: Option<i64>) -> Option<f64> {
        let e = epley(weight_kg, reps)?;
        Some(match brzycki(weight_kg, reps) {
            Some(b) => super::round((e + b) / 2.0),
            None => super::round(e),
        })
    }
}

/// One session's contribution to one (variant, rep band) track.
#[derive(Debug, Clone, PartialEq)]
pub struct ProgressPoint {
    pub date: String,
    pub cycle_day: String,
    pub kind: Kind,
    /// The top set: heaviest non-back-off set of the slot (P2).
    pub weight_kg: Option<f64>,
    pub reps: Option<i64>,
    pub rir: Option<f64>,
    pub bar_speed: Option<BarSpeed>,
    /// Back-off work is reported beside the top set, never averaged into it (P2).
    pub backoff_sets: usize,
    pub backoff_tonnage_kg: f64,
    pub sets: usize,
}

impl ProgressPoint {
    pub fn estimated_1rm_kg(&self) -> Option<f64> {
        one_rep_max::estimate(self.weight_kg, self.reps)
    }

    pub fn to_json(&self) -> Value {
        let mut json = Map::new();
        json.insert("date".into(), json!(self.date));
        json.insert("cycle_day".into(), json!(self.cycle_day));
        if self.kind != Kind::Training {
            json.insert("kind".into(), json!(kind_name(self.kind)));
        }
        if let Some(v) = self.weight_kg {
            json.insert("weight_kg".into(), json!(v));
        }
        if let Some(v) = self.reps {
            json.insert("reps".into(), json!(v));
        }
        if let Some(v) = self.estimated_1rm_kg() {
            json.insert("estimated_1rm_kg".into(), json!(v));
        }
        if let Some(v) = self.rir {
            json.insert("rir".into(), json!(v));
        }
        if let Some(v) = self.bar_speed {
            json.insert("bar_speed".into(), json!(bar_speed_name(v)));
        }
        json.insert("sets".into(), json!(self.sets));
        if self.backoff_sets > 0 {
            json.insert("backoff_sets".into(), json!(self.backoff_sets));
            json.insert(
                "backoff_tonnage_kg".into(),
                json!(round(self.backoff_tonnage_kg)),
            );
        }
        Value::Object(json)
    }
}

fn bar_speed_name(speed: BarSpeed) -> &'static str {
    match speed {
        BarSpeed::Fast => "fast",
        BarSpeed::Ok => "ok",
        BarSpeed::Slow => "slow",
        BarSpeed::Grind => "grind",
    }
}

pub fn band_name(band: RepBand) -> &'static str {
    match band {
        RepBand::Heavy => "heavy",
        RepBand::Base => "base",
        RepBand::Volume => "volume",
    }
}

/// A single progress track. Rep bands are independent tracks on the same lift
/// (P6), so a stall in one is visibly separate from the others.
#[derive(Debug, Clone, PartialEq)]
pub struct ProgressSeries {
    pub exercise: String,
    pub band: Option<RepBand>,
    pub points: Vec<ProgressPoint>,
}

impl ProgressSeries {
    /// Change in estimated 1RM from the first point that has one to the last.
    pub fn trend_kg(&self) -> Option<f64> {
        let estimates: Vec<f64> = self
            .points
            .iter()
            .filter_map(ProgressPoint::estimated_1rm_kg)
            .collect();
        (estimates.len() >= 2).then(|| round(estimates[estimates.len() - 1] - estimates[0]))
    }

    pub fn to_json(&self) -> Value {
        let mut json = Map::new();
        json.insert("exercise".into(), json!(self.exercise));
        if let Some(band) = self.band {
            json.insert("rep_band".into(), json!(band_name(band)));
        }
        if let Some(trend) = self.trend_kg() {
            json.insert("trend_1rm_kg".into(), json!(trend));
        }
        json.insert(
            "points".into(),
            Value::Array(self.points.iter().map(ProgressPoint::to_json).collect()),
        );
        Value::Object(json)
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct ProgressReport {
    pub query: String,
    /// The canonical name the query resolved to, or None when the catalogue does
    /// not know it — the series are still built (warn, never block).
    pub resolved: Option<String>,
    pub pattern: Option<MovementPattern>,
    /// Every exercise included: one name, or the whole pattern when asked (P8).
    pub variants: Vec<String>,
    pub series: Vec<ProgressSeries>,
}

impl ProgressReport {
    pub fn to_json(&self) -> Value {
        let mut json = Map::new();
        json.insert("query".into(), json!(self.query));
        if let Some(resolved) = &self.resolved {
            json.insert("resolved".into(), json!(resolved));
        }
        if let Some(pattern) = self.pattern {
            json.insert("pattern".into(), json!(pattern_name(pattern)));
        }
        if self.variants.len() > 1 {
            json.insert("variants".into(), json!(self.variants));
        }
        json.insert(
            "series".into(),
            Value::Array(self.series.iter().map(ProgressSeries::to_json).collect()),
        );
        Value::Object(json)
    }
}

pub fn pattern_name(pattern: MovementPattern) -> &'static str {
    use MovementPattern as P;
    match pattern {
        P::Squat => "squat",
        P::Hinge => "hinge",
        P::Press => "press",
        P::Pull => "pull",
        P::Olympic => "olympic",
        P::Carry => "carry",
        P::Core => "core",
        P::Grip => "grip",
    }
}

/// `by_pattern` widens the question from "this lift" to "this pattern" — the
/// conjugate view P8 asks for, where a front squat and an overhead squat are two
/// variants of one thing rather than two sparse, hopeless trend lines.
pub fn report(
    sessions: &[Session],
    exercise: &str,
    catalogue: Option<&Catalogue>,
    by_pattern: bool,
) -> ProgressReport {
    let resolved = catalogue.and_then(|c| c.resolve(exercise));
    let pattern = if by_pattern {
        resolved.and_then(|e| e.pattern)
    } else {
        None
    };

    let matches = |name: &str| -> bool {
        if let Some(pattern) = pattern {
            return catalogue
                .and_then(|c| c.resolve(name))
                .and_then(|e| e.pattern)
                == Some(pattern);
        }
        let canonical = catalogue.and_then(|c| c.resolve(name));
        match (canonical, resolved) {
            (Some(canonical), Some(resolved)) => canonical.name == resolved.name,
            _ => name.trim().to_lowercase() == exercise.trim().to_lowercase(),
        }
    };

    let mut tracks: BTreeMap<(String, Option<RepBand>), Vec<ProgressPoint>> = BTreeMap::new();
    let mut variants: Vec<String> = Vec::new();
    let mut ordered: Vec<&Session> = sessions.iter().collect();
    ordered.sort_by(|a, b| a.date.cmp(&b.date));

    for session in ordered {
        for block in &session.blocks {
            let Block::Strength(strength) = block else {
                continue;
            };
            if !matches(&strength.exercise) {
                continue;
            }
            let name = catalogue
                .and_then(|c| c.resolve(&strength.exercise))
                .map_or_else(|| strength.exercise.clone(), |e| e.name.clone());
            if !variants.contains(&name) {
                variants.push(name.clone());
            }
            for (band, point) in slot_points(session, strength) {
                tracks.entry((name.clone(), band)).or_default().push(point);
            }
        }
    }

    variants.sort();
    ProgressReport {
        query: exercise.to_string(),
        resolved: resolved.map(|e| e.name.clone()),
        pattern: pattern.or_else(|| resolved.and_then(|e| e.pattern)),
        variants,
        series: tracks
            .into_iter()
            .map(|((exercise, band), points)| ProgressSeries {
                exercise,
                band,
                points,
            })
            .collect(),
    }
}

/// One slot can contribute to several bands at once — a ramp of sixes topped
/// off with a triple is a point on both tracks, each with its own top set.
fn slot_points(
    session: &Session,
    block: &StrengthBlock,
) -> BTreeMap<Option<RepBand>, ProgressPoint> {
    let mut by_band: BTreeMap<Option<RepBand>, Vec<&WorkSet>> = BTreeMap::new();
    for set in &block.sets {
        by_band.entry(set.band()).or_default().push(set);
    }
    by_band
        .into_iter()
        .map(|(band, sets)| (band, point_for(session, &sets)))
        .collect()
}

fn point_for(session: &Session, sets: &[&WorkSet]) -> ProgressPoint {
    let mut top: Option<&WorkSet> = None;
    let mut backoff_sets = 0;
    let mut backoff_tonnage = 0.0;
    for set in sets {
        if set.is_backoff.unwrap_or(false) {
            backoff_sets += 1;
            backoff_tonnage += set.tonnage_kg();
            continue;
        }
        if top.is_none_or(|t| set.weight_kg.unwrap_or(0.0) >= t.weight_kg.unwrap_or(0.0)) {
            top = Some(set);
        }
    }
    // A slot that is nothing but back-offs still has a heaviest set; reporting
    // no top set at all would drop the session from the track entirely.
    let top = top.unwrap_or_else(|| {
        sets.iter()
            .copied()
            .reduce(|a, b| {
                if b.weight_kg.unwrap_or(0.0) >= a.weight_kg.unwrap_or(0.0) {
                    b
                } else {
                    a
                }
            })
            .expect("a band group is never empty")
    });
    ProgressPoint {
        date: session.date.clone(),
        cycle_day: session.cycle_day.clone(),
        kind: session.kind,
        weight_kg: top.weight_kg,
        reps: top.recorded_reps(),
        rir: top.rir,
        bar_speed: top.bar_speed,
        backoff_sets,
        backoff_tonnage_kg: backoff_tonnage,
        sets: sets.len(),
    }
}
