//! Everything one session says about itself once the derivations are run:
//! tonnage, rep bands, density and metcon splits. Nothing here is stored — the
//! file holds only what was observed (ADR-001).

use serde_json::{Map, Value, json};

use crate::models::block::Block;
use crate::models::metcon::MetconBlock;
use crate::models::session::{Kind, Session};
use crate::models::work_set::RepBand;
use crate::wall_clock;

/// Which energy system a metcon taxed, by how long it lasted (P9).
///
/// Classified on total duration, which is all the file records. An interval
/// workout of short efforts therefore lands in the domain its *clock* says.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TimeDomain {
    Alactic,
    Glycolytic,
    Aerobic,
}

impl TimeDomain {
    pub const ALL: [Self; 3] = [Self::Alactic, Self::Glycolytic, Self::Aerobic];

    pub fn name(self) -> &'static str {
        match self {
            Self::Alactic => "alactic",
            Self::Glycolytic => "glycolytic",
            Self::Aerobic => "aerobic",
        }
    }

    pub fn for_seconds(seconds: Option<f64>) -> Option<Self> {
        match seconds {
            Some(s) if s <= 0.0 => None,
            Some(s) if s <= 300.0 => Some(Self::Alactic),
            Some(s) if s <= 1200.0 => Some(Self::Glycolytic),
            Some(_) => Some(Self::Aerobic),
            None => None,
        }
    }
}

fn round(value: f64, digits: u32) -> f64 {
    let factor = 10f64.powi(digits as i32);
    (value * factor).round() / factor
}

fn round_opt(value: Option<f64>, digits: u32) -> Option<f64> {
    value.map(|v| round(v, digits))
}

fn put(json: &mut Map<String, Value>, key: &str, value: Option<Value>) {
    if let Some(value) = value {
        json.insert(key.to_string(), value);
    }
}

/// Work time against clock time, from the bracket timestamps (P1).
#[derive(Debug, Clone, PartialEq)]
pub struct SessionDensity {
    pub work_min: f64,
    pub transition_min: f64,
    /// None when the session has no start time or nothing that ends: density is
    /// derived from what was written down, never guessed.
    pub total_min: Option<f64>,
}

impl SessionDensity {
    pub fn work_share(&self) -> Option<f64> {
        match self.total_min {
            Some(total) if total > 0.0 => Some(round(self.work_min / total, 3)),
            _ => None,
        }
    }

    pub fn to_json(&self) -> Value {
        let mut json = Map::new();
        json.insert("work_min".into(), json!(round(self.work_min, 1)));
        json.insert(
            "transition_min".into(),
            json!(round(self.transition_min, 1)),
        );
        put(
            &mut json,
            "total_min",
            round_opt(self.total_min, 1).map(|v| json!(v)),
        );
        put(&mut json, "work_share", self.work_share().map(|v| json!(v)));
        Value::Object(json)
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct BlockMetrics {
    pub index: usize,
    pub block_type: String,
    /// The exercise, the machine, or the metcon's format — whatever names the block.
    pub label: Option<String>,
    pub duration_min: Option<f64>,
    pub transition_min: Option<f64>,
    pub sets: usize,
    pub backoff_sets: usize,
    pub reps: i64,
    pub tonnage_kg: f64,
    /// The slot's own rep band (P6): the band most of its sets sit in.
    pub band: Option<RepBand>,
}

impl BlockMetrics {
    pub fn to_json(&self) -> Value {
        let mut json = Map::new();
        json.insert("index".into(), json!(self.index));
        json.insert("type".into(), json!(self.block_type));
        put(&mut json, "label", self.label.clone().map(|v| json!(v)));
        put(
            &mut json,
            "duration_min",
            round_opt(self.duration_min, 1).map(|v| json!(v)),
        );
        put(
            &mut json,
            "transition_min",
            round_opt(self.transition_min, 1).map(|v| json!(v)),
        );
        if self.sets > 0 {
            json.insert("sets".into(), json!(self.sets));
        }
        if self.backoff_sets > 0 {
            json.insert("backoff_sets".into(), json!(self.backoff_sets));
        }
        if self.reps > 0 {
            json.insert("reps".into(), json!(self.reps));
        }
        if self.tonnage_kg > 0.0 {
            json.insert("tonnage_kg".into(), json!(round(self.tonnage_kg, 1)));
        }
        put(
            &mut json,
            "rep_band",
            self.band.map(|b| json!(band_name(b))),
        );
        Value::Object(json)
    }
}

/// One round of a metcon with its split differenced out of the cumulative
/// clock the paper log records.
#[derive(Debug, Clone, PartialEq)]
pub struct RoundSplit {
    pub round: i64,
    pub reps: Option<i64>,
    pub cumulative_sec: Option<f64>,
    pub split_sec: Option<f64>,
    pub heart_rate: Option<i64>,
}

impl RoundSplit {
    pub fn seconds_per_rep(&self) -> Option<f64> {
        let split = self.split_sec?;
        let count = self.reps?;
        (count > 0).then(|| round(split / count as f64, 2))
    }

    pub fn to_json(&self) -> Value {
        let mut json = Map::new();
        json.insert("round".into(), json!(self.round));
        put(&mut json, "reps", self.reps.map(|v| json!(v)));
        put(
            &mut json,
            "cumulative_sec",
            round_opt(self.cumulative_sec, 1).map(|v| json!(v)),
        );
        put(
            &mut json,
            "split_sec",
            round_opt(self.split_sec, 1).map(|v| json!(v)),
        );
        put(
            &mut json,
            "sec_per_rep",
            self.seconds_per_rep().map(|v| json!(v)),
        );
        put(&mut json, "heart_rate", self.heart_rate.map(|v| json!(v)));
        Value::Object(json)
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct MetconMetrics {
    pub block_index: usize,
    pub format: Option<String>,
    pub scheme: Option<Vec<i64>>,
    pub exercises: Vec<String>,
    pub total_sec: Option<f64>,
    pub rounds: Vec<RoundSplit>,
}

impl MetconMetrics {
    pub fn domain(&self) -> Option<TimeDomain> {
        TimeDomain::for_seconds(self.total_sec)
    }

    /// How much slower the last round ran than the first, per rep, as a
    /// percentage — the pacing question a 21-15-9 is asked (positive = faded).
    pub fn pace_decay_percent(&self) -> Option<f64> {
        let paces: Vec<f64> = self
            .rounds
            .iter()
            .filter_map(RoundSplit::seconds_per_rep)
            .collect();
        let (first, last) = (*paces.first()?, *paces.last()?);
        (paces.len() >= 2 && first > 0.0).then(|| round((last - first) / first * 100.0, 1))
    }

    pub fn peak_heart_rate(&self) -> Option<i64> {
        self.rounds.iter().filter_map(|r| r.heart_rate).max()
    }

    pub fn to_json(&self) -> Value {
        let mut json = Map::new();
        json.insert("block_index".into(), json!(self.block_index));
        put(&mut json, "format", self.format.clone().map(|v| json!(v)));
        put(&mut json, "scheme", self.scheme.clone().map(|v| json!(v)));
        if !self.exercises.is_empty() {
            json.insert("exercises".into(), json!(self.exercises));
        }
        put(
            &mut json,
            "total_sec",
            round_opt(self.total_sec, 1).map(|v| json!(v)),
        );
        put(
            &mut json,
            "time_domain",
            self.domain().map(|d| json!(d.name())),
        );
        put(
            &mut json,
            "pace_decay_percent",
            self.pace_decay_percent().map(|v| json!(v)),
        );
        put(
            &mut json,
            "peak_heart_rate",
            self.peak_heart_rate().map(|v| json!(v)),
        );
        if !self.rounds.is_empty() {
            json.insert(
                "rounds".into(),
                Value::Array(self.rounds.iter().map(RoundSplit::to_json).collect()),
            );
        }
        Value::Object(json)
    }
}

pub type BandCounts = [usize; 3];

fn band_index(band: RepBand) -> usize {
    match band {
        RepBand::Heavy => 0,
        RepBand::Base => 1,
        RepBand::Volume => 2,
    }
}

fn band_name(band: RepBand) -> &'static str {
    match band {
        RepBand::Heavy => "heavy",
        RepBand::Base => "base",
        RepBand::Volume => "volume",
    }
}

const BAND_ORDER: [RepBand; 3] = [RepBand::Heavy, RepBand::Base, RepBand::Volume];

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

/// Ties go to the heavier band: a slot that is half sixes and half eights is
/// not evidence for the volume dose P4 rations.
fn dominant_band(counts: &BandCounts) -> Option<RepBand> {
    let mut best = None;
    let mut best_count = 0;
    for band in BAND_ORDER {
        let count = counts[band_index(band)];
        if count > best_count {
            best = Some(band);
            best_count = count;
        }
    }
    best
}

#[derive(Debug, Clone, PartialEq)]
pub struct SessionMetrics {
    pub date: String,
    pub cycle_day: String,
    pub kind: Kind,
    pub blocks: Vec<BlockMetrics>,
    pub metcons: Vec<MetconMetrics>,
    pub density: SessionDensity,
    pub tonnage_kg: f64,
    pub sets: usize,
    pub backoff_sets: usize,
    pub reps: i64,
    /// Sets per band, and slots (strength blocks) per band. Both matter: P6
    /// tracks sets, P4's "2-3 volume slots per cycle" counts slots.
    pub band_sets: BandCounts,
    pub band_slots: BandCounts,
}

impl SessionMetrics {
    /// The ramp's top set is the progress signal, back-offs are volume (P2), so
    /// they are never averaged together.
    pub fn top_sets(&self) -> usize {
        self.sets - self.backoff_sets
    }

    pub fn of(session: &Session) -> Self {
        let mut blocks = Vec::new();
        let mut metcons = Vec::new();
        let mut band_sets: BandCounts = [0; 3];
        let mut band_slots: BandCounts = [0; 3];
        let (mut tonnage, mut sets, mut backoff_sets, mut reps) = (0.0, 0usize, 0usize, 0i64);
        let (mut work_min, mut transition_min) = (0.0, 0.0);

        let session_start = wall_clock::minutes(session.start_time.as_deref());
        let mut cursor = session_start;
        let mut last_end: Option<i64> = None;

        for (index, block) in session.blocks.iter().enumerate() {
            let start = wall_clock::minutes(block.start_time()).or(cursor);
            let end = wall_clock::minutes(block.end_time());

            let duration = match (start, end) {
                (Some(s), Some(e)) if e >= s => Some((e - s) as f64),
                _ => match block {
                    Block::Cardio(cardio) => cardio.duration_min,
                    _ => None,
                },
            };
            let transition = match (cursor, start) {
                (Some(c), Some(s)) if s > c => Some((s - c) as f64),
                _ => None,
            };

            if let Some(d) = duration {
                work_min += d;
            }
            if let Some(t) = transition {
                transition_min += t;
            }

            if let Some(e) = end {
                cursor = Some(e);
                last_end = Some(e);
            } else if let (Some(s), Some(d)) = (start, duration) {
                cursor = Some(s + d.round() as i64);
                last_end = cursor;
            }

            match block {
                Block::Strength(strength) => {
                    let mut counts: BandCounts = [0; 3];
                    let (mut block_reps, mut block_tonnage, mut block_backoff) =
                        (0i64, 0.0, 0usize);
                    for set in &strength.sets {
                        if let Some(band) = set.band() {
                            counts[band_index(band)] += 1;
                            band_sets[band_index(band)] += 1;
                        }
                        block_reps += set.recorded_reps().unwrap_or(0);
                        block_tonnage += set.tonnage_kg();
                        if set.is_backoff.unwrap_or(false) {
                            block_backoff += 1;
                        }
                    }
                    let slot_band = dominant_band(&counts);
                    if let Some(band) = slot_band {
                        band_slots[band_index(band)] += 1;
                    }
                    sets += strength.sets.len();
                    backoff_sets += block_backoff;
                    reps += block_reps;
                    tonnage += block_tonnage;
                    blocks.push(BlockMetrics {
                        index,
                        block_type: "strength".into(),
                        label: Some(strength.exercise.clone()),
                        duration_min: duration,
                        transition_min: transition,
                        sets: strength.sets.len(),
                        backoff_sets: block_backoff,
                        reps: block_reps,
                        tonnage_kg: block_tonnage,
                        band: slot_band,
                    });
                }
                Block::Metcon(metcon) => {
                    let derived = metcon_metrics(index, metcon, start, end);
                    let duration_min = duration.or(derived.total_sec.map(|s| s / 60.0));
                    blocks.push(BlockMetrics {
                        index,
                        block_type: "metcon".into(),
                        label: Some(
                            metcon
                                .format
                                .map_or("metcon".to_string(), |f| format_wire(f).to_string()),
                        ),
                        duration_min,
                        transition_min: transition,
                        ..empty_block(index, "metcon")
                    });
                    metcons.push(derived);
                }
                Block::Cardio(cardio) => blocks.push(BlockMetrics {
                    label: (!cardio.machine.is_empty()).then(|| cardio.machine.clone()),
                    duration_min: duration,
                    transition_min: transition,
                    ..empty_block(index, "cardio")
                }),
                Block::Cooldown(_) => blocks.push(BlockMetrics {
                    duration_min: duration,
                    transition_min: transition,
                    ..empty_block(index, "cooldown")
                }),
            }
        }

        let total_min = match (session_start, last_end) {
            (Some(start), Some(end)) => Some((end - start) as f64),
            _ => None,
        };

        Self {
            date: session.date.clone(),
            cycle_day: session.cycle_day.clone(),
            kind: session.kind,
            blocks,
            metcons,
            density: SessionDensity {
                work_min,
                transition_min,
                total_min,
            },
            tonnage_kg: tonnage,
            sets,
            backoff_sets,
            reps,
            band_sets,
            band_slots,
        }
    }

    pub fn to_json(&self) -> Value {
        let mut json = Map::new();
        json.insert("date".into(), json!(self.date));
        json.insert("cycle_day".into(), json!(self.cycle_day));
        json.insert("kind".into(), json!(kind_name(self.kind)));
        json.insert(
            "strength".into(),
            json!({
                "tonnage_kg": round(self.tonnage_kg, 1),
                "sets": self.sets,
                "top_sets": self.top_sets(),
                "backoff_sets": self.backoff_sets,
                "reps": self.reps,
                "rep_band_sets": band_json(&self.band_sets),
                "rep_band_slots": band_json(&self.band_slots),
            }),
        );
        json.insert("density".into(), self.density.to_json());
        json.insert(
            "blocks".into(),
            Value::Array(self.blocks.iter().map(BlockMetrics::to_json).collect()),
        );
        if !self.metcons.is_empty() {
            json.insert(
                "metcons".into(),
                Value::Array(self.metcons.iter().map(MetconMetrics::to_json).collect()),
            );
        }
        Value::Object(json)
    }
}

fn empty_block(index: usize, block_type: &str) -> BlockMetrics {
    BlockMetrics {
        index,
        block_type: block_type.into(),
        label: None,
        duration_min: None,
        transition_min: None,
        sets: 0,
        backoff_sets: 0,
        reps: 0,
        tonnage_kg: 0.0,
        band: None,
    }
}

pub fn kind_name(kind: Kind) -> &'static str {
    match kind {
        Kind::Training => "training",
        Kind::Deload => "deload",
        Kind::Retest => "retest",
    }
}

pub fn format_wire(format: crate::models::metcon::MetconFormat) -> &'static str {
    use crate::models::metcon::MetconFormat as F;
    match format {
        F::ForTime => "for_time",
        F::Amrap => "amrap",
        F::Emom => "emom",
        F::Intervals => "intervals",
        F::Ladder => "ladder",
        F::Chipper => "chipper",
    }
}

fn metcon_metrics(
    index: usize,
    block: &MetconBlock,
    start_min: Option<i64>,
    end_min: Option<i64>,
) -> MetconMetrics {
    let mut rounds = Vec::new();
    let mut previous: Option<f64> = None;
    for round in block.rounds.iter().flatten() {
        let cumulative = round.split_cumulative_sec;
        let split = cumulative.map(|c| previous.map_or(c, |p| c - p));
        if cumulative.is_some() {
            previous = cumulative;
        }
        rounds.push(RoundSplit {
            round: round.round,
            reps: round.reps.or_else(|| scheme_reps(block, round.round)),
            cumulative_sec: cumulative,
            split_sec: split,
            heart_rate: round.heart_rate,
        });
    }

    let from_rounds = rounds.last().and_then(|r| r.cumulative_sec);
    let from_clock = match (start_min, end_min) {
        (Some(s), Some(e)) if e >= s => Some((e - s) as f64 * 60.0),
        _ => None,
    };

    MetconMetrics {
        block_index: index,
        format: block.format.map(|f| format_wire(f).to_string()),
        scheme: block.scheme.clone(),
        exercises: block.exercises.iter().map(|e| e.name.clone()).collect(),
        total_sec: from_rounds.or(from_clock),
        rounds,
    }
}

/// The reps a round carries when the round itself does not say: the scheme
/// position it matches (21-15-9 → round 2 is 15).
fn scheme_reps(block: &MetconBlock, round: i64) -> Option<i64> {
    let scheme = block.scheme.as_ref()?;
    if round < 1 || round as usize > scheme.len() {
        return None;
    }
    Some(scheme[round as usize - 1])
}
