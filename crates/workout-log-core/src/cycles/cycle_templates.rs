use super::cycle_day::{CycleDay, DayKind};
use crate::models::cycle::{BlockTemplate, CycleSession};

fn block(block_type: &str, role: &str) -> BlockTemplate {
    BlockTemplate {
        block_type: block_type.into(),
        role: Some(role.into()),
        machine: None,
        duration_min: None,
        exercise: None,
        sets_reps: None,
        notes: None,
        format: None,
        scheme: None,
        exercises: None,
        extras: Default::default(),
    }
}

fn cardio(role: &str, minutes: f64) -> BlockTemplate {
    BlockTemplate {
        machine: Some(String::new()),
        duration_min: Some(minutes),
        ..block("cardio", role)
    }
}

fn strength(role: &str, exercise: Option<&str>, sets_reps: Option<Vec<i64>>) -> BlockTemplate {
    BlockTemplate {
        exercise: exercise.map(str::to_string),
        sets_reps,
        ..block("strength", role)
    }
}

pub fn blocks_for(kind: DayKind) -> Vec<BlockTemplate> {
    match kind {
        DayKind::Conditioning => vec![
            cardio("warmup", 10.0),
            strength("explosive", None, None),
            block("metcon", "metcon"),
            strength("accessory", None, Some(vec![6, 6, 6, 6])),
            strength("grip", None, Some(vec![8, 8, 8, 8])),
            block("cooldown", "cooldown"),
        ],
        DayKind::Heavy => vec![
            cardio("warmup", 15.0),
            strength("warmup", Some("гіперекстензія"), Some(vec![12, 12])),
            strength("main", None, None),
            strength("accessory", None, Some(vec![6, 6, 6, 6, 6])),
            strength("accessory", None, Some(vec![6, 6, 6, 6, 6])),
            strength("grip", None, Some(vec![8, 8, 8, 8])),
            block("cooldown", "cooldown"),
        ],
    }
}

pub fn session(day: CycleDay, week: Option<i64>, weekday: Option<String>) -> CycleSession {
    CycleSession {
        cycle_day: day.code(),
        week,
        weekday,
        session_type: Some(
            match day.kind {
                DayKind::Conditioning => "metcon",
                DayKind::Heavy => "heavy",
            }
            .into(),
        ),
        title: None,
        session_notes: None,
        blocks: blocks_for(day.kind),
        extras: Default::default(),
    }
}

/// The next code in the A1/A2 progression after everything already planned.
pub fn next_day(existing_codes: &[String]) -> CycleDay {
    let mut days: Vec<CycleDay> = existing_codes
        .iter()
        .filter_map(|c| CycleDay::try_parse(c))
        .collect();
    days.sort_by_key(|d| d.code());
    days.last().map_or(
        CycleDay {
            letter: 'A',
            kind: DayKind::Conditioning,
        },
        |last| last.next(),
    )
}
