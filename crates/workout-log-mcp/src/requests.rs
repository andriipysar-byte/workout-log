//! Tool input schemas. The enums mirror the core's rather than deriving
//! `JsonSchema` there, which would pull a protocol concern into the domain
//! (ADR-004).

use schemars::JsonSchema;
use serde::Deserialize;
use workout_log_core::analytics::muscle_activation::WeightingMode;
use workout_log_core::models::exercise::{ExerciseCategory, Modality, MovementPattern};
use workout_log_core::models::session::Kind;

/// training, deload or retest — deloads and retests are the anchors progress is
/// measured between (P10).
#[derive(Debug, Clone, Copy, Deserialize, JsonSchema)]
#[serde(rename_all = "snake_case")]
pub enum KindArg {
    Training,
    Deload,
    Retest,
}

impl From<KindArg> for Kind {
    fn from(value: KindArg) -> Self {
        match value {
            KindArg::Training => Self::Training,
            KindArg::Deload => Self::Deload,
            KindArg::Retest => Self::Retest,
        }
    }
}

/// Movement pattern — the grouping that makes variant rotation read as progress (P8).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, JsonSchema)]
#[serde(rename_all = "snake_case")]
pub enum PatternArg {
    Squat,
    Hinge,
    Press,
    Pull,
    Olympic,
    Carry,
    Core,
    Grip,
}

impl From<PatternArg> for MovementPattern {
    fn from(value: PatternArg) -> Self {
        match value {
            PatternArg::Squat => Self::Squat,
            PatternArg::Hinge => Self::Hinge,
            PatternArg::Press => Self::Press,
            PatternArg::Pull => Self::Pull,
            PatternArg::Olympic => Self::Olympic,
            PatternArg::Carry => Self::Carry,
            PatternArg::Core => Self::Core,
            PatternArg::Grip => Self::Grip,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, JsonSchema)]
#[serde(rename_all = "snake_case")]
pub enum ModalityArg {
    Barbell,
    Dumbbell,
    Kettlebell,
    Bodyweight,
    Machine,
}

impl From<ModalityArg> for Modality {
    fn from(value: ModalityArg) -> Self {
        match value {
            ModalityArg::Barbell => Self::Barbell,
            ModalityArg::Dumbbell => Self::Dumbbell,
            ModalityArg::Kettlebell => Self::Kettlebell,
            ModalityArg::Bodyweight => Self::Bodyweight,
            ModalityArg::Machine => Self::Machine,
        }
    }
}

/// Force-velocity emphasis; power and speed drive the P3 bar-speed rule.
#[derive(Debug, Clone, Copy, Deserialize, JsonSchema)]
#[serde(rename_all = "snake_case")]
pub enum CategoryArg {
    Strength,
    Power,
    Speed,
    Longevity,
}

impl From<CategoryArg> for ExerciseCategory {
    fn from(value: CategoryArg) -> Self {
        match value {
            CategoryArg::Strength => Self::Strength,
            CategoryArg::Power => Self::Power,
            CategoryArg::Speed => Self::Speed,
            CategoryArg::Longevity => Self::Longevity,
        }
    }
}

/// How muscle work is weighted: set_count (default), rep_volume or tonnage.
#[derive(Debug, Clone, Copy, Deserialize, JsonSchema)]
#[serde(rename_all = "snake_case")]
pub enum WeightingArg {
    SetCount,
    RepVolume,
    Tonnage,
}

impl From<WeightingArg> for WeightingMode {
    fn from(value: WeightingArg) -> Self {
        match value {
            WeightingArg::SetCount => Self::SetCount,
            WeightingArg::RepVolume => Self::RepVolume,
            WeightingArg::Tonnage => Self::Tonnage,
        }
    }
}

#[derive(Debug, Clone, Default, Deserialize, JsonSchema)]
pub struct ListSessions {
    /// Earliest date, inclusive (YYYY-MM-DD)
    pub from: Option<String>,
    /// Latest date, inclusive (YYYY-MM-DD)
    pub to: Option<String>,
    /// A1…F2
    pub cycle_day: Option<String>,
    pub kind: Option<KindArg>,
    /// Only sessions containing this exercise; aliases resolve
    pub exercise: Option<String>,
    /// Default 50
    pub limit: Option<i64>,
}

#[derive(Debug, Clone, Default, Deserialize, JsonSchema)]
pub struct Selector {
    /// File name, e.g. 2026-08-10_F1.json
    pub id: Option<String>,
    /// The session date, when it names one file (YYYY-MM-DD)
    pub date: Option<String>,
    /// A1…F2, to disambiguate a date
    pub cycle_day: Option<String>,
}

#[derive(Debug, Clone, Deserialize, JsonSchema)]
pub struct CreateSession {
    /// The day it was trained (YYYY-MM-DD)
    pub date: String,
    /// A1…F2
    pub cycle_day: String,
    /// Cycle id to take the template from; inferred when only one cycle defines that day
    pub from_cycle: Option<String>,
    pub kind: Option<KindArg>,
    /// HH:MM
    pub start_time: Option<String>,
    pub bodyweight_kg: Option<f64>,
    pub notes: Option<String>,
    /// Replace an existing file
    pub overwrite: Option<bool>,
}

#[derive(Debug, Clone, Deserialize, JsonSchema)]
pub struct WriteSession {
    /// A whole session document, as read_session returns it
    pub session: serde_json::Map<String, serde_json::Value>,
    /// The file being edited, when the header may move it
    pub id: Option<String>,
    /// Allow writing a file that does not exist yet
    pub overwrite: Option<bool>,
}

#[derive(Debug, Clone, Copy, Deserialize, JsonSchema)]
#[serde(rename_all = "snake_case")]
pub enum LogMode {
    Replace,
    Append,
}

#[derive(Debug, Clone, Deserialize, JsonSchema)]
pub struct LogSets {
    /// File name, e.g. 2026-08-10_F1.json
    pub id: Option<String>,
    /// The session date, when it names one file (YYYY-MM-DD)
    pub date: Option<String>,
    /// A1…F2, to disambiguate a date
    pub cycle_day: Option<String>,
    /// The line as written on paper
    pub notation: String,
    /// Which slot; aliases resolve
    pub exercise: Option<String>,
    /// Which slot, by position
    pub block_index: Option<i64>,
    /// replace (default) or append
    pub mode: Option<LogMode>,
}

#[derive(Debug, Clone, Deserialize, JsonSchema)]
pub struct DeleteSession {
    /// File name, e.g. 2026-08-10_F1.json
    pub id: Option<String>,
    /// The session date, when it names one file (YYYY-MM-DD)
    pub date: Option<String>,
    /// A1…F2, to disambiguate a date
    pub cycle_day: Option<String>,
    /// Must be true
    pub confirm: bool,
}

#[derive(Debug, Clone, Deserialize, JsonSchema)]
pub struct ParseNotation {
    pub notation: String,
}

#[derive(Debug, Clone, Default, Deserialize, JsonSchema)]
pub struct ListExercises {
    /// Substring of a name or alias
    pub query: Option<String>,
    pub pattern: Option<PatternArg>,
    pub modality: Option<ModalityArg>,
    /// A muscle token, e.g. quads
    pub muscle: Option<String>,
    /// Default 40
    pub limit: Option<i64>,
}

#[derive(Debug, Clone, Deserialize, JsonSchema)]
pub struct AddExercise {
    /// Canonical name, Ukrainian
    pub name: String,
    pub aliases: Option<Vec<String>>,
    pub pattern: Option<PatternArg>,
    pub modality: Option<ModalityArg>,
    pub category: Option<CategoryArg>,
    pub primary_muscles: Vec<String>,
    pub secondary_muscles: Option<Vec<String>>,
    pub notes: Option<String>,
    pub overwrite: Option<bool>,
}

#[derive(Debug, Clone, Default, Deserialize, JsonSchema)]
pub struct ListCycles {
    /// Full definition of one cycle
    pub id: Option<String>,
}

#[derive(Debug, Clone, Deserialize, JsonSchema)]
pub struct GenerateCycle {
    pub cycle_id: String,
    /// Overrides the cycle start date (YYYY-MM-DD)
    pub start_date: Option<String>,
    /// Overwrite existing files
    pub force: Option<bool>,
    pub dry_run: Option<bool>,
}

#[derive(Debug, Clone, Default, Deserialize, JsonSchema)]
pub struct AnalyzeSession {
    /// File name, e.g. 2026-08-10_F1.json
    pub id: Option<String>,
    /// The session date, when it names one file (YYYY-MM-DD)
    pub date: Option<String>,
    /// A1…F2, to disambiguate a date
    pub cycle_day: Option<String>,
    pub mode: Option<WeightingArg>,
}

#[derive(Debug, Clone, Default, Deserialize, JsonSchema)]
pub struct AnalyzeTraining {
    /// Earliest date, inclusive (YYYY-MM-DD)
    pub from: Option<String>,
    /// Latest date, inclusive (YYYY-MM-DD)
    pub to: Option<String>,
    /// Default 12
    pub cycle_length_days: Option<i64>,
    /// Share of 7+ rep slots that raises the P5 alert; default 0.3
    pub volume_share_threshold: Option<f64>,
    /// Per-session load series; default true
    pub include_load: Option<bool>,
}

#[derive(Debug, Clone, Deserialize, JsonSchema)]
pub struct ExerciseProgressArgs {
    /// Canonical name or alias
    pub exercise: String,
    /// Compare variants of one pattern (P8)
    pub by_pattern: Option<bool>,
    /// Earliest date, inclusive (YYYY-MM-DD)
    pub from: Option<String>,
    /// Latest date, inclusive (YYYY-MM-DD)
    pub to: Option<String>,
}

#[derive(Debug, Clone, Default, Deserialize, JsonSchema)]
pub struct MuscleActivationArgs {
    /// File name, e.g. 2026-08-10_F1.json
    pub id: Option<String>,
    /// The session date, when it names one file (YYYY-MM-DD)
    pub date: Option<String>,
    /// A1…F2, to disambiguate a date
    pub cycle_day: Option<String>,
    /// Range mode: earliest date
    pub from: Option<String>,
    /// Range mode: latest date
    pub to: Option<String>,
    pub mode: Option<WeightingArg>,
}

#[derive(Debug, Clone, Default, Deserialize, JsonSchema)]
pub struct ValidateArchive {
    /// Earliest date, inclusive (YYYY-MM-DD)
    pub from: Option<String>,
    /// Latest date, inclusive (YYYY-MM-DD)
    pub to: Option<String>,
}
