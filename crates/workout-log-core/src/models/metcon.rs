use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct MetconBlock {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub format: Option<MetconFormat>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub scheme: Option<Vec<i64>>,
    #[serde(default)]
    pub exercises: Vec<MetconExercise>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rounds: Option<Vec<MetconRound>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub start_time: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub end_time: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub notes: Option<String>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum MetconFormat {
    ForTime,
    Amrap,
    Emom,
    Intervals,
    Ladder,
    Chipper,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct MetconExercise {
    pub name: String,
    /// The prescription as written on the whiteboard — "24kg+24kg", "60 cm",
    /// "bodyweight". Free text because a single number cannot hold a pair of
    /// bells or a box height; `weight_kg` stays the machine-readable load that
    /// tonnage reads.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub load: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub weight_kg: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub reps_override: Option<Vec<i64>>,
}

impl MetconExercise {
    /// What the WOD table prints in parentheses after the name.
    pub fn prescription(&self) -> Option<String> {
        if self.load.is_some() {
            return self.load.clone();
        }
        let kg = self.weight_kg?;
        Some(if kg.fract() == 0.0 {
            format!("{} kg", kg as i64)
        } else {
            format!("{kg} kg")
        })
    }

    /// The reps this movement actually does per round: its own override when it
    /// does not follow the workout's scheme.
    pub fn scheme_within<'a>(&'a self, block_scheme: Option<&'a [i64]>) -> &'a [i64] {
        self.reps_override
            .as_deref()
            .or(block_scheme)
            .unwrap_or(&[])
    }
}

/// Splits are stored cumulative (as on paper); per-round splits are derived at
/// read time, never stored.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct MetconRound {
    pub round: i64,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub reps: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub split_cumulative_sec: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub heart_rate: Option<i64>,
}
