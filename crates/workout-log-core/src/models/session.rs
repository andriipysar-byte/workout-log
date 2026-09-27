use serde::{Deserialize, Serialize};

use super::block::Block;

/// Files on disk are the source of truth (ADR-001); this is a lossless projection.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Session {
    pub date: String,
    pub cycle_day: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub start_time: Option<String>,
    #[serde(default)]
    pub kind: Kind,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub bodyweight_kg: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub notes: Option<String>,
    #[serde(default)]
    pub blocks: Vec<Block>,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Kind {
    #[default]
    Training,
    Deload,
    Retest,
}

impl Kind {
    pub const ALL: [Self; 3] = [Self::Training, Self::Deload, Self::Retest];
}
