use std::collections::BTreeMap;

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum MuscleGroup {
    Chest,
    Back,
    Shoulders,
    Arms,
    Legs,
    Core,
}

impl MuscleGroup {
    /// Declaration order, which is also the tie-break order for [`dominant`].
    pub const ALL: [Self; 6] = [
        Self::Chest,
        Self::Back,
        Self::Shoulders,
        Self::Arms,
        Self::Legs,
        Self::Core,
    ];

    pub fn hex(self) -> &'static str {
        match self {
            Self::Chest => "#d1495b",
            Self::Back => "#00798c",
            Self::Shoulders => "#edae49",
            Self::Arms => "#8e5ea2",
            Self::Legs => "#30638e",
            Self::Core => "#58a65c",
        }
    }

    pub fn name(self) -> &'static str {
        match self {
            Self::Chest => "chest",
            Self::Back => "back",
            Self::Shoulders => "shoulders",
            Self::Arms => "arms",
            Self::Legs => "legs",
            Self::Core => "core",
        }
    }

    pub fn label(self) -> &'static str {
        match self {
            Self::Chest => "Chest",
            Self::Back => "Back",
            Self::Shoulders => "Shoulders",
            Self::Arms => "Arms",
            Self::Legs => "Legs",
            Self::Core => "Core",
        }
    }

    pub fn of(muscle: &str) -> Option<Self> {
        Some(match muscle {
            "chest" => Self::Chest,
            "lats" | "rhomboids" | "traps" | "spinal_erectors" => Self::Back,
            "front_delts" | "side_delts" | "rear_delts" => Self::Shoulders,
            "biceps" | "triceps" | "forearms" => Self::Arms,
            "quads" | "hamstrings" | "glutes" | "calves" | "adductors" | "hip_flexors" => {
                Self::Legs
            }
            "abs" | "obliques" => Self::Core,
            _ => return None,
        })
    }

    /// Ties go to the earlier group in declaration order; None when no muscle maps.
    pub fn dominant(scores: &BTreeMap<String, f64>) -> Option<Self> {
        let mut totals: BTreeMap<Self, f64> = BTreeMap::new();
        for (muscle, score) in scores {
            if let Some(group) = Self::of(muscle) {
                *totals.entry(group).or_insert(0.0) += score;
            }
        }
        let mut best: Option<(Self, f64)> = None;
        for group in Self::ALL {
            if let Some(&total) = totals.get(&group)
                && best.is_none_or(|(_, high)| total > high)
            {
                best = Some((group, total));
            }
        }
        best.map(|(group, _)| group)
    }
}
