//! Expands a cycle definition onto the calendar and into session stubs.
//!
//! Planning-only fields — `role`, `sets_reps`, `weekday`, `title`, `week` —
//! are consumed here and never reach a session file.

use chrono::{Datelike, Days, NaiveDate};

use super::cycle_day::CycleDay;
use crate::models::block::{Block, CardioBlock, CooldownBlock, StrengthBlock};
use crate::models::cycle::{BlockTemplate, Cycle, CycleSession};
use crate::models::metcon::MetconBlock;
use crate::models::session::{Kind, Session};
use crate::models::work_set::WorkSet;

#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
#[error("{0}")]
pub struct CycleGeneratorError(pub String);

type Result<T> = std::result::Result<T, CycleGeneratorError>;

pub const WEEKDAY_ABBREVIATIONS: [&str; 7] = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];

fn abbreviation(date: NaiveDate) -> &'static str {
    WEEKDAY_ABBREVIATIONS[date.weekday().num_days_from_monday() as usize]
}

/// Walks the calendar from `start` inclusive, collecting dates whose weekday is
/// in `training_days`, until `count` are found.
pub fn training_dates(
    start: NaiveDate,
    training_days: &[String],
    count: usize,
) -> Result<Vec<NaiveDate>> {
    let wanted: Vec<&str> = WEEKDAY_ABBREVIATIONS
        .into_iter()
        .filter(|abbrev| training_days.iter().any(|d| d == abbrev))
        .collect();
    if wanted.is_empty() {
        if count > 0 {
            return Err(CycleGeneratorError(format!(
                "no recognised training days in {} (expected any of {})",
                training_days.join(", "),
                WEEKDAY_ABBREVIATIONS.join(", ")
            )));
        }
        return Ok(Vec::new());
    }
    let mut dates = Vec::with_capacity(count);
    let mut cursor = start;
    while dates.len() < count {
        if wanted.contains(&abbreviation(cursor)) {
            dates.push(cursor);
        }
        // Calendar-day stepping, so a DST boundary cannot shift the walk.
        cursor = cursor
            .checked_add_days(Days::new(1))
            .ok_or_else(|| CycleGeneratorError("ran off the end of the calendar".to_string()))?;
    }
    Ok(dates)
}

pub fn generate(cycle: &Cycle) -> Result<Vec<Session>> {
    let start = NaiveDate::parse_from_str(&cycle.start_date, "%Y-%m-%d")
        .map_err(|_| CycleGeneratorError(format!("\"{}\" is not an ISO date", cycle.start_date)))?;
    let dates = training_dates(start, &cycle.training_days, cycle.sessions.len())?;
    cycle
        .sessions
        .iter()
        .zip(dates)
        .map(|(template, date)| session_from_template(template, date, true))
        .collect()
}

pub fn session_from_template(
    template: &CycleSession,
    when: NaiveDate,
    enforce_weekday: bool,
) -> Result<Session> {
    if enforce_weekday && let Some(expected) = &template.weekday {
        let actual = abbreviation(when);
        if actual != expected {
            return Err(CycleGeneratorError(format!(
                "{}: calendar says {actual} but template says {expected}",
                template.cycle_day
            )));
        }
    }
    let blocks = template
        .blocks
        .iter()
        .map(block_from_template)
        .collect::<Result<Vec<_>>>()?;
    Ok(Session {
        date: iso_date(when),
        cycle_day: template.cycle_day.clone(),
        start_time: None,
        kind: Kind::Training,
        bodyweight_kg: None,
        notes: non_empty(template.session_notes.as_deref()),
        blocks,
    })
}

pub fn block_from_template(template: &BlockTemplate) -> Result<Block> {
    let notes = non_empty(template.notes.as_deref());
    Ok(match template.block_type.as_str() {
        "cardio" => Block::Cardio(CardioBlock {
            machine: template.machine.clone().unwrap_or_default(),
            duration_min: template.duration_min,
            distance_m: None,
            end_time: None,
        }),
        "strength" => {
            let exercise = non_empty(template.exercise.as_deref()).ok_or_else(|| {
                CycleGeneratorError("a strength block in this cycle has no exercise yet".into())
            })?;
            Block::Strength(StrengthBlock {
                exercise,
                sets: template
                    .sets_reps()
                    .iter()
                    .map(|&reps| WorkSet {
                        reps: Some(reps),
                        ..WorkSet::default()
                    })
                    .collect(),
                start_time: None,
                end_time: None,
                notes,
            })
        }
        "metcon" => Block::Metcon(MetconBlock {
            format: template.format,
            scheme: template.scheme.clone(),
            exercises: template.exercises().to_vec(),
            rounds: None,
            start_time: None,
            end_time: None,
            notes,
        }),
        "cooldown" => Block::Cooldown(CooldownBlock::default()),
        other => {
            return Err(CycleGeneratorError(format!(
                "unknown block type: \"{other}\""
            )));
        }
    })
}

/// A planned session as the muscle map should see it: blocks that cannot be
/// built yet are skipped, and a named-but-setless lift still scores one slot.
pub fn preview(template: &CycleSession, on: Option<NaiveDate>) -> Session {
    let when = on.unwrap_or_else(|| NaiveDate::from_ymd_opt(2000, 1, 1).expect("valid date"));
    let blocks = template
        .blocks
        .iter()
        .filter_map(|b| block_from_template(b).ok())
        .map(|block| match block {
            Block::Strength(mut strength) if strength.sets.is_empty() => {
                strength.sets.push(WorkSet::default());
                Block::Strength(strength)
            }
            other => other,
        })
        .collect();
    Session {
        date: iso_date(when),
        cycle_day: template.cycle_day.clone(),
        start_time: None,
        kind: Kind::Training,
        bodyweight_kg: None,
        notes: None,
        blocks,
    }
}

pub fn iso_date(when: NaiveDate) -> String {
    format!("{:04}-{:02}-{:02}", when.year(), when.month(), when.day())
}

pub fn cycle_day_of(template: &CycleSession) -> Option<CycleDay> {
    CycleDay::try_parse(&template.cycle_day)
}

fn non_empty(text: Option<&str>) -> Option<String> {
    text.filter(|t| !t.is_empty()).map(str::to_string)
}
