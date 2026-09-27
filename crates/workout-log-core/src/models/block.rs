use serde::{Deserialize, Serialize};

use super::metcon::MetconBlock;
use super::work_set::WorkSet;

/// Flat tagged union: `type` is a sibling of the payload, not a wrapper around
/// it, which is exactly serde's internally-tagged representation.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum Block {
    Cardio(CardioBlock),
    Strength(StrengthBlock),
    Metcon(MetconBlock),
    Cooldown(CooldownBlock),
}

impl Block {
    pub fn type_name(&self) -> &'static str {
        match self {
            Self::Cardio(_) => "cardio",
            Self::Strength(_) => "strength",
            Self::Metcon(_) => "metcon",
            Self::Cooldown(_) => "cooldown",
        }
    }

    pub fn end_time(&self) -> Option<&str> {
        match self {
            Self::Cardio(b) => b.end_time.as_deref(),
            Self::Strength(b) => b.end_time.as_deref(),
            Self::Metcon(b) => b.end_time.as_deref(),
            Self::Cooldown(b) => b.end_time.as_deref(),
        }
    }

    pub fn start_time(&self) -> Option<&str> {
        match self {
            Self::Cardio(_) => None,
            Self::Strength(b) => b.start_time.as_deref(),
            Self::Metcon(b) => b.start_time.as_deref(),
            Self::Cooldown(_) => None,
        }
    }

    pub fn notes(&self) -> Option<&str> {
        match self {
            Self::Cardio(_) => None,
            Self::Strength(b) => b.notes.as_deref(),
            Self::Metcon(b) => b.notes.as_deref(),
            Self::Cooldown(b) => b.notes.as_deref(),
        }
    }
}

/// `end_time` (on every block) is the bracket timestamp from the paper log — the
/// source of block duration and session density.
#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct CardioBlock {
    pub machine: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub duration_min: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub distance_m: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub end_time: Option<String>,
}

#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct StrengthBlock {
    pub exercise: String,
    #[serde(default)]
    pub sets: Vec<WorkSet>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub start_time: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub end_time: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub notes: Option<String>,
}

#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct CooldownBlock {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub end_time: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub notes: Option<String>,
}
