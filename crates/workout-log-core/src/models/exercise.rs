use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};

/// `pattern` groups variants of one movement so conjugate rotation reads as
/// progress rather than scatter (P8).
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Exercise {
    /// Canonical, Ukrainian.
    pub name: String,
    /// `Some(vec![])` when the file carried an empty list, `None` when it had no
    /// such key — the catalogue writes `"secondary_muscles": []` for some
    /// entries, and omitting it on save would quietly delete it.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub aliases: Option<Vec<String>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pattern: Option<MovementPattern>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub modality: Option<Modality>,
    #[serde(default)]
    pub category: ExerciseCategory,
    #[serde(default)]
    pub primary_muscles: Vec<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub secondary_muscles: Option<Vec<String>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub notes: Option<String>,
    /// Keys this model has no field for, kept so a read-modify-write of the
    /// hand-maintained catalogue cannot silently delete them.
    #[serde(flatten)]
    pub extras: Map<String, Value>,
}

impl Exercise {
    /// The P3 bar-speed rule applies to explosive movements — both power and speed work.
    pub fn is_explosive(&self) -> bool {
        matches!(
            self.category,
            ExerciseCategory::Power | ExerciseCategory::Speed
        )
    }

    pub fn aliases(&self) -> &[String] {
        self.aliases.as_deref().unwrap_or(&[])
    }

    pub fn secondary_muscles(&self) -> &[String] {
        self.secondary_muscles.as_deref().unwrap_or(&[])
    }
}

/// Force–velocity / training emphasis of a movement.
/// - `power`: olympic lifts, force under heavy load (drives the P3 bar-speed rule)
/// - `speed`: light ballistic work (also drives P3)
/// - `strength`: max-force / loaded compound work
/// - `longevity`: durability work (grip, core, delts, calves, mobility, conditioning)
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ExerciseCategory {
    #[default]
    Strength,
    Power,
    Speed,
    Longevity,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum MovementPattern {
    Squat,
    Hinge,
    Press,
    Pull,
    Olympic,
    Carry,
    Core,
    Grip,
}

impl MovementPattern {
    /// Declaration order, which is the order reports list patterns in.
    pub const ALL: [Self; 8] = [
        Self::Squat,
        Self::Hinge,
        Self::Press,
        Self::Pull,
        Self::Olympic,
        Self::Carry,
        Self::Core,
        Self::Grip,
    ];
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Modality {
    Barbell,
    Dumbbell,
    Kettlebell,
    Bodyweight,
    Machine,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Catalogue {
    /// The file's own top-level `$comment`, kept so writing the catalogue back
    /// does not strip its documentation.
    #[serde(rename = "$comment", skip_serializing_if = "Option::is_none")]
    pub comment: Option<String>,
    pub exercises: Vec<Exercise>,
}

impl Catalogue {
    /// The catalogue with `exercise` appended, or replacing the entry of the
    /// same canonical name.
    pub fn with_exercise(&self, exercise: Exercise) -> Self {
        let mut next = self.exercises.clone();
        match next
            .iter()
            .position(|e| e.name.to_lowercase() == exercise.name.to_lowercase())
        {
            Some(index) => next[index] = exercise,
            None => next.push(exercise),
        }
        Self {
            comment: self.comment.clone(),
            exercises: next,
        }
    }

    /// Every muscle token the catalogue uses, sorted — the vocabulary a new
    /// exercise should pick from.
    pub fn known_muscles(&self) -> Vec<String> {
        let mut all: Vec<String> = self
            .exercises
            .iter()
            .flat_map(|e| e.primary_muscles.iter().chain(e.secondary_muscles()))
            .cloned()
            .collect();
        all.sort();
        all.dedup();
        all
    }

    /// Matches canonical names and aliases (case-insensitive); None when unknown
    /// (caller warns, never blocks).
    pub fn resolve(&self, typed: &str) -> Option<&Exercise> {
        let key = typed.trim().to_lowercase();
        self.exercises.iter().find(|e| {
            e.name.to_lowercase() == key || e.aliases().iter().any(|a| a.to_lowercase() == key)
        })
    }
}
