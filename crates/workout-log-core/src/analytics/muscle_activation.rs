use std::collections::BTreeMap;

use serde::{Deserialize, Serialize};

use crate::models::block::Block;
use crate::models::exercise::{Catalogue, Exercise};
use crate::models::metcon::{MetconBlock, MetconExercise};
use crate::models::session::Session;
use crate::models::work_set::WorkSet;

pub type Scores = BTreeMap<String, f64>;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum WeightingMode {
    #[default]
    SetCount,
    RepVolume,
    Tonnage,
}

impl WeightingMode {
    pub const ALL: [Self; 3] = [Self::SetCount, Self::RepVolume, Self::Tonnage];

    pub fn wire(self) -> &'static str {
        match self {
            Self::SetCount => "set_count",
            Self::RepVolume => "rep_volume",
            Self::Tonnage => "tonnage",
        }
    }

    pub fn label(self) -> &'static str {
        match self {
            Self::SetCount => "Sets",
            Self::RepVolume => "Reps",
            Self::Tonnage => "Tonnage",
        }
    }

    pub fn from_wire(wire: &str) -> Option<Self> {
        Self::ALL.into_iter().find(|m| m.wire() == wire)
    }
}

#[derive(Debug, Clone, Copy)]
pub struct MuscleActivation {
    pub primary_weight: f64,
    pub secondary_weight: f64,
}

impl Default for MuscleActivation {
    fn default() -> Self {
        Self {
            primary_weight: 1.0,
            secondary_weight: 0.5,
        }
    }
}

impl MuscleActivation {
    /// A muscle listed as both primary and secondary keeps the primary weight.
    pub fn for_exercise(&self, exercise: &Exercise) -> Scores {
        let mut raw = Scores::new();
        for muscle in &exercise.primary_muscles {
            let slot = raw.entry(muscle.clone()).or_insert(0.0);
            *slot = slot.max(self.primary_weight);
        }
        for muscle in exercise.secondary_muscles() {
            let slot = raw.entry(muscle.clone()).or_insert(0.0);
            *slot = slot.max(self.secondary_weight);
        }
        normalize(raw)
    }

    pub fn for_session(
        &self,
        session: &Session,
        catalogue: &Catalogue,
        mode: WeightingMode,
    ) -> Scores {
        let mut raw = Scores::new();
        self.accumulate(&mut raw, session, catalogue, mode);
        normalize(raw)
    }

    /// Raw volume is summed across sessions and normalized once, rather than
    /// averaging maps that were each already normalized.
    pub fn for_sessions(
        &self,
        sessions: &[Session],
        catalogue: &Catalogue,
        mode: WeightingMode,
    ) -> Scores {
        let mut raw = Scores::new();
        for session in sessions {
            self.accumulate(&mut raw, session, catalogue, mode);
        }
        normalize(raw)
    }

    fn accumulate(
        &self,
        raw: &mut Scores,
        session: &Session,
        catalogue: &Catalogue,
        mode: WeightingMode,
    ) {
        for block in &session.blocks {
            match block {
                Block::Strength(strength) => {
                    if let Some(exercise) = catalogue.resolve(&strength.exercise) {
                        self.add(raw, exercise, strength_volume(&strength.sets, mode));
                    }
                }
                Block::Metcon(metcon) => {
                    for entry in &metcon.exercises {
                        if let Some(exercise) = catalogue.resolve(&entry.name) {
                            self.add(raw, exercise, metcon_volume(metcon, entry, mode));
                        }
                    }
                }
                Block::Cardio(_) | Block::Cooldown(_) => {}
            }
        }
    }

    fn add(&self, raw: &mut Scores, exercise: &Exercise, volume: f64) {
        if volume <= 0.0 {
            return;
        }
        for muscle in &exercise.primary_muscles {
            *raw.entry(muscle.clone()).or_insert(0.0) += volume * self.primary_weight;
        }
        for muscle in exercise.secondary_muscles() {
            *raw.entry(muscle.clone()).or_insert(0.0) += volume * self.secondary_weight;
        }
    }
}

fn strength_volume(sets: &[WorkSet], mode: WeightingMode) -> f64 {
    match mode {
        WeightingMode::SetCount => sets.len() as f64,
        WeightingMode::RepVolume => sets
            .iter()
            .map(|s| s.recorded_reps().unwrap_or(0) as f64)
            .sum(),
        WeightingMode::Tonnage => sets.iter().map(WorkSet::tonnage_kg).sum(),
    }
}

fn metcon_volume(block: &MetconBlock, entry: &MetconExercise, mode: WeightingMode) -> f64 {
    let scheme = entry.scheme_within(block.scheme.as_deref());
    let rounds = block.rounds.as_ref().map_or(scheme.len(), Vec::len);
    let scheme_reps: i64 = scheme.iter().sum();
    match mode {
        WeightingMode::SetCount => {
            let slots = if scheme.is_empty() { 1 } else { scheme.len() };
            rounds.max(slots) as f64
        }
        WeightingMode::RepVolume => scheme_reps as f64,
        WeightingMode::Tonnage => scheme_reps as f64 * entry.weight_kg.unwrap_or(0.0),
    }
}

fn normalize(raw: Scores) -> Scores {
    let peak = raw.values().copied().fold(f64::NEG_INFINITY, f64::max);
    if raw.is_empty() || peak <= 0.0 {
        return raw;
    }
    raw.into_iter().map(|(k, v)| (k, v / peak)).collect()
}
