use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};

use super::metcon::{MetconExercise, MetconFormat};

/// A reusable cycle definition: an ordered list of session templates with no
/// weights. `role` and `sets_reps` are planning-only helpers consumed by the
/// cycle generator and never written to a session file.
///
/// Every level keeps an `extras` map of the keys it does not model. The planner
/// writes `cycles.json` back out, so anything dropped on read would be deleted
/// from the user's file on the next save — `skipped` and `role` are exactly
/// that, and nothing in the app would have noticed.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct CycleCatalogue {
    #[serde(rename = "$comment", skip_serializing_if = "Option::is_none")]
    pub comment: Option<String>,
    pub cycles: Vec<Cycle>,
}

impl CycleCatalogue {
    pub fn by_id(&self, id: &str) -> Option<&Cycle> {
        self.cycles.iter().find(|c| c.id == id)
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Cycle {
    pub id: String,
    /// Falls back to `id` when the file omits it, but is always written back.
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub training_days: Vec<String>,
    pub start_date: String,
    pub sessions: Vec<CycleSession>,
    #[serde(flatten)]
    pub extras: Map<String, Value>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct CycleSession {
    pub cycle_day: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub week: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub weekday: Option<String>,
    #[serde(rename = "type", skip_serializing_if = "Option::is_none")]
    pub session_type: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub title: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub session_notes: Option<String>,
    pub blocks: Vec<BlockTemplate>,
    #[serde(flatten)]
    pub extras: Map<String, Value>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct BlockTemplate {
    #[serde(rename = "type")]
    pub block_type: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub role: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub machine: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub duration_min: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub exercise: Option<String>,
    /// `Some(vec![])` when the file wrote an empty list by hand; see
    /// [`super::exercise::Exercise::aliases`].
    #[serde(skip_serializing_if = "Option::is_none")]
    pub sets_reps: Option<Vec<i64>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub notes: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub format: Option<MetconFormat>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub scheme: Option<Vec<i64>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub exercises: Option<Vec<MetconExercise>>,
    #[serde(flatten)]
    pub extras: Map<String, Value>,
}

impl BlockTemplate {
    pub fn sets_reps(&self) -> &[i64] {
        self.sets_reps.as_deref().unwrap_or(&[])
    }

    pub fn exercises(&self) -> &[MetconExercise] {
        self.exercises.as_deref().unwrap_or(&[])
    }
}
