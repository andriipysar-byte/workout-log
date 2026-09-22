use serde::{Deserialize, Serialize};

/// All fields optional so a weightless / bodyweight / timed set round-trips
/// without inventing values.
#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct WorkSet {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub weight_kg: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub reps: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub duration_sec: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cluster: Option<Vec<i64>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub total_reps: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rir: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rep_band: Option<RepBand>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub bar_speed: Option<BarSpeed>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub is_backoff: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub planned_reps: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub notes: Option<String>,
}

impl WorkSet {
    /// The reps this set actually recorded, however they were written down: a
    /// plain count, a cluster chain, or the total a cluster was summed to. None
    /// when the set measures something else (a hold) or nothing yet (a plan).
    pub fn recorded_reps(&self) -> Option<i64> {
        self.reps
            .or(self.total_reps)
            .or_else(|| self.cluster.as_ref().map(|c| c.iter().sum()))
    }

    /// The band this set belongs to (P6): what the log says when it says, else
    /// what its reps imply.
    pub fn band(&self) -> Option<RepBand> {
        self.rep_band
            .or_else(|| RepBand::for_reps(self.recorded_reps()))
    }

    pub fn tonnage_kg(&self) -> f64 {
        self.recorded_reps().unwrap_or(0) as f64 * self.weight_kg.unwrap_or(0.0)
    }
}

/// Declaration order is the band order: heavy < base < volume.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum RepBand {
    Heavy,
    Base,
    Volume,
}

impl RepBand {
    /// The band a rep count falls in (P6). Only a recorded set has a band, so a
    /// set with no reps at all — a hold, or a slot not filled in yet — has none.
    pub fn for_reps(reps: Option<i64>) -> Option<Self> {
        match reps {
            Some(r) if r > 0 && r <= 3 => Some(Self::Heavy),
            Some(r) if r <= 6 => Some(Self::Base),
            Some(_) => Some(Self::Volume),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum BarSpeed {
    Fast,
    Ok,
    Slow,
    Grind,
}
